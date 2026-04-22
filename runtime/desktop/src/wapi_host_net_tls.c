/**
 * WAPI Desktop Runtime — Schannel TLS transport
 *
 * Client-side TLS (and DTLS) over an already-connected Winsock
 * socket, via SSPI + Schannel. The session owns:
 *   - a credential handle (CredHandle; outbound, default system policy)
 *   - a security context handle (CtxtHandle; released on close)
 *   - SECPKG_ATTR_STREAM_SIZES for header/trailer/max-message sizing
 *   - an encrypted-side receive buffer that accumulates TCP bytes
 *     between DecryptMessage calls (Schannel needs whole records)
 *   - a plaintext spill buffer for DecryptMessage output that didn't
 *     fit the caller's recv buffer
 *
 * Handshake runs blocking. The caller ensures the socket is blocking
 * (non-blocking is restored after handshake). Once established,
 * send/recv each run one EncryptMessage / one-or-more-read +
 * DecryptMessage cycle per call — a send returns WAPI_ERR_IO on any
 * record failure, a recv returns 0 on TLS close_notify.
 */

#include "wapi_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif

#include <windows.h>
#include <sspi.h>
#include <schannel.h>

#pragma comment(lib, "secur32")

/* Schannel package name. Using the Unicode "Microsoft Unified
 * Security Protocol Provider" covers both TLS 1.0..1.3 depending on
 * the OS. */
static LPCWSTR SCHANNEL_PKG = UNISP_NAME_W;

/* Exposed to wapi_host_net.c to avoid a second interop header. */
int  wapi_host_net_tls_connect(wapi_socket_t sock, const char* sni,
                               size_t sni_len, bool dtls,
                               void** out_state);
int  wapi_host_net_tls_send   (void* state, wapi_socket_t sock,
                               const void* data, size_t len);
int  wapi_host_net_tls_recv   (void* state, wapi_socket_t sock,
                               void* buf, size_t buf_len);
void wapi_host_net_tls_close  (void* state, wapi_socket_t sock);

typedef struct wapi_tls_session_t {
    CredHandle               cred;
    CtxtHandle               ctx;
    bool                     cred_valid;
    bool                     ctx_valid;
    bool                     dtls;
    SecPkgContext_StreamSizes sizes;

    /* Encrypted-byte receive buffer. */
    uint8_t*                 rx_buf;
    uint32_t                 rx_cap;
    uint32_t                 rx_len;

    /* Plaintext spill from a previous decrypt that didn't fit. */
    uint8_t*                 plain;
    uint32_t                 plain_cap;
    uint32_t                 plain_off;
    uint32_t                 plain_len;
} wapi_tls_session_t;

static void tls_free(wapi_tls_session_t* s) {
    if (!s) return;
    if (s->ctx_valid)  DeleteSecurityContext(&s->ctx);
    if (s->cred_valid) FreeCredentialsHandle(&s->cred);
    free(s->rx_buf);
    free(s->plain);
    free(s);
}

/* Blocking send-all: Schannel handshake bytes must be delivered in
 * full before the next InitializeSecurityContext call. */
static bool send_all(wapi_socket_t sock, const void* data, int len) {
    const char* p = (const char*)data;
    while (len > 0) {
        int n = send(sock, p, len, 0);
        if (n <= 0) return false;
        p   += n;
        len -= n;
    }
    return true;
}

int wapi_host_net_tls_connect(wapi_socket_t sock, const char* sni,
                              size_t sni_len, bool dtls, void** out_state)
{
    if (!out_state) return -1;
    *out_state = NULL;

    wapi_tls_session_t* s = (wapi_tls_session_t*)calloc(1, sizeof(*s));
    if (!s) return -1;
    s->dtls = dtls;

    /* SCHANNEL_CRED is the broadly-available credential struct;
     * Schannel auto-negotiates TLS 1.2/1.3 based on OS support when
     * grbitEnabledProtocols is left 0. */
    SCHANNEL_CRED cred = {0};
    cred.dwVersion = SCHANNEL_CRED_VERSION;
    cred.dwFlags   = SCH_USE_STRONG_CRYPTO | SCH_CRED_AUTO_CRED_VALIDATION;

    SECURITY_STATUS st = AcquireCredentialsHandleW(
        NULL, (SEC_WCHAR*)SCHANNEL_PKG, SECPKG_CRED_OUTBOUND, NULL,
        &cred, NULL, NULL, &s->cred, NULL);
    if (st != SEC_E_OK) { tls_free(s); return -1; }
    s->cred_valid = true;

    /* Widen the SNI hostname. Schannel requires UTF-16. Pass NULL if
     * the caller didn't supply one — TLS still works but SNI is off. */
    WCHAR sni_w[256];
    WCHAR* sni_arg = NULL;
    if (sni && sni_len > 0) {
        int n = MultiByteToWideChar(CP_UTF8, 0, sni, (int)sni_len,
                                    sni_w, (int)(sizeof(sni_w)/sizeof(WCHAR)) - 1);
        if (n > 0) { sni_w[n] = 0; sni_arg = sni_w; }
    }

    /* Handshake loop. in_buf accumulates ciphertext from the server;
     * InitializeSecurityContext returns SEC_E_INCOMPLETE_MESSAGE when
     * we need more, SEC_I_CONTINUE_NEEDED when the handshake has
     * output for us to send, SEC_E_OK on completion. */
    uint8_t  in_buf[16384];
    uint32_t in_len = 0;
    bool     first  = true;

    DWORD req_flags = ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT |
                      ISC_REQ_SEQUENCE_DETECT | ISC_REQ_ALLOCATE_MEMORY |
                      (dtls ? ISC_REQ_DATAGRAM : ISC_REQ_STREAM);

    for (;;) {
        SecBuffer  in_bufs[2];
        SecBufferDesc in_desc;
        SecBuffer  out_bufs[3];
        SecBufferDesc out_desc;

        in_bufs[0].BufferType = SECBUFFER_TOKEN;
        in_bufs[0].pvBuffer   = in_buf;
        in_bufs[0].cbBuffer   = in_len;
        in_bufs[1].BufferType = SECBUFFER_EMPTY;
        in_bufs[1].pvBuffer   = NULL;
        in_bufs[1].cbBuffer   = 0;
        in_desc.ulVersion     = SECBUFFER_VERSION;
        in_desc.cBuffers      = 2;
        in_desc.pBuffers      = in_bufs;

        out_bufs[0].BufferType = SECBUFFER_TOKEN;
        out_bufs[0].pvBuffer   = NULL;
        out_bufs[0].cbBuffer   = 0;
        out_bufs[1].BufferType = SECBUFFER_ALERT;
        out_bufs[1].pvBuffer   = NULL;
        out_bufs[1].cbBuffer   = 0;
        out_bufs[2].BufferType = SECBUFFER_EMPTY;
        out_bufs[2].pvBuffer   = NULL;
        out_bufs[2].cbBuffer   = 0;
        out_desc.ulVersion     = SECBUFFER_VERSION;
        out_desc.cBuffers      = 3;
        out_desc.pBuffers      = out_bufs;

        DWORD out_flags = 0;
        st = InitializeSecurityContextW(
            &s->cred, first ? NULL : &s->ctx, sni_arg,
            req_flags, 0, 0,
            first ? NULL : &in_desc, 0,
            first ? &s->ctx : NULL, &out_desc, &out_flags, NULL);
        if (first) s->ctx_valid = true;

        /* Send any output token produced this round. */
        if (out_bufs[0].cbBuffer > 0 && out_bufs[0].pvBuffer) {
            if (!send_all(sock, out_bufs[0].pvBuffer, (int)out_bufs[0].cbBuffer)) {
                FreeContextBuffer(out_bufs[0].pvBuffer);
                tls_free(s); return -1;
            }
            FreeContextBuffer(out_bufs[0].pvBuffer);
        }

        if (st == SEC_E_INCOMPLETE_MESSAGE) {
            /* Read more ciphertext and loop. */
            int got = recv(sock, (char*)in_buf + in_len,
                           (int)(sizeof(in_buf) - in_len), 0);
            if (got <= 0) { tls_free(s); return -1; }
            in_len += (uint32_t)got;
            first = false;
            continue;
        }
        if (st == SEC_I_CONTINUE_NEEDED) {
            /* Consume any SECBUFFER_EXTRA and read more if room. */
            if (in_bufs[1].BufferType == SECBUFFER_EXTRA) {
                memmove(in_buf, in_buf + (in_len - in_bufs[1].cbBuffer),
                        in_bufs[1].cbBuffer);
                in_len = in_bufs[1].cbBuffer;
            } else {
                in_len = 0;
            }
            if (in_len == 0 || in_len < sizeof(in_buf)) {
                int got = recv(sock, (char*)in_buf + in_len,
                               (int)(sizeof(in_buf) - in_len), 0);
                if (got <= 0) { tls_free(s); return -1; }
                in_len += (uint32_t)got;
            }
            first = false;
            continue;
        }
        if (st == SEC_E_OK) {
            /* Handshake complete. Stash any leftover ciphertext. */
            uint32_t extra = 0;
            if (in_bufs[1].BufferType == SECBUFFER_EXTRA)
                extra = in_bufs[1].cbBuffer;

            if (QueryContextAttributesW(&s->ctx, SECPKG_ATTR_STREAM_SIZES,
                                        &s->sizes) != SEC_E_OK) {
                tls_free(s); return -1;
            }

            /* rx_buf sized to one max record, doubled to allow
             * coalescing a half-received follow-up. */
            s->rx_cap = s->sizes.cbHeader + s->sizes.cbMaximumMessage +
                        s->sizes.cbTrailer;
            s->rx_cap *= 2;
            s->rx_buf = (uint8_t*)malloc(s->rx_cap);
            s->plain_cap = s->sizes.cbMaximumMessage;
            s->plain     = (uint8_t*)malloc(s->plain_cap);
            if (!s->rx_buf || !s->plain) { tls_free(s); return -1; }
            if (extra > 0 && extra <= s->rx_cap) {
                memcpy(s->rx_buf, in_buf + (in_len - extra), extra);
                s->rx_len = extra;
            }
            *out_state = s;
            return 0;
        }
        /* Any other status = fatal. */
        tls_free(s); return -1;
    }
}

int wapi_host_net_tls_send(void* state, wapi_socket_t sock,
                           const void* data, size_t len)
{
    wapi_tls_session_t* s = (wapi_tls_session_t*)state;
    if (!s || !s->ctx_valid) return -1;

    const uint8_t* p = (const uint8_t*)data;
    size_t remaining = len;
    size_t total_sent = 0;

    /* Each TLS record fits at most cbMaximumMessage plaintext bytes.
     * Wrap a fresh record buffer per chunk; send header + data +
     * trailer together over TCP. */
    uint32_t max_plain = s->sizes.cbMaximumMessage;
    uint32_t rec_cap   = s->sizes.cbHeader + max_plain + s->sizes.cbTrailer;
    uint8_t* rec       = (uint8_t*)malloc(rec_cap);
    if (!rec) return -1;

    while (remaining > 0) {
        size_t chunk = remaining < max_plain ? remaining : max_plain;
        memcpy(rec + s->sizes.cbHeader, p, chunk);

        SecBuffer bufs[4];
        bufs[0].BufferType = SECBUFFER_STREAM_HEADER;
        bufs[0].pvBuffer   = rec;
        bufs[0].cbBuffer   = s->sizes.cbHeader;
        bufs[1].BufferType = SECBUFFER_DATA;
        bufs[1].pvBuffer   = rec + s->sizes.cbHeader;
        bufs[1].cbBuffer   = (ULONG)chunk;
        bufs[2].BufferType = SECBUFFER_STREAM_TRAILER;
        bufs[2].pvBuffer   = rec + s->sizes.cbHeader + chunk;
        bufs[2].cbBuffer   = s->sizes.cbTrailer;
        bufs[3].BufferType = SECBUFFER_EMPTY;
        bufs[3].pvBuffer   = NULL;
        bufs[3].cbBuffer   = 0;

        SecBufferDesc desc;
        desc.ulVersion = SECBUFFER_VERSION;
        desc.cBuffers  = 4;
        desc.pBuffers  = bufs;

        SECURITY_STATUS st = EncryptMessage(&s->ctx, 0, &desc, 0);
        if (st != SEC_E_OK) { free(rec); return -1; }

        /* After EncryptMessage the three buffers' cbBuffer values are
         * the actual on-wire sizes (header may shrink, data may be
         * padded). Send them in order. */
        size_t wire = (size_t)bufs[0].cbBuffer + bufs[1].cbBuffer + bufs[2].cbBuffer;
        if (!send_all(sock, rec, (int)wire)) { free(rec); return -1; }

        p           += chunk;
        remaining   -= chunk;
        total_sent  += chunk;
    }
    free(rec);
    return (int)total_sent;
}

int wapi_host_net_tls_recv(void* state, wapi_socket_t sock,
                           void* buf, size_t buf_len)
{
    wapi_tls_session_t* s = (wapi_tls_session_t*)state;
    if (!s || !s->ctx_valid || buf_len == 0) return -1;

    /* Drain any plaintext spill from a prior recv first. */
    if (s->plain_off < s->plain_len) {
        uint32_t avail = s->plain_len - s->plain_off;
        uint32_t copy  = avail < buf_len ? avail : (uint32_t)buf_len;
        memcpy(buf, s->plain + s->plain_off, copy);
        s->plain_off += copy;
        return (int)copy;
    }

    /* Pull ciphertext + decrypt until we can hand plaintext back. */
    for (;;) {
        /* If we already have bytes, try decrypting them first — the
         * server may have sent multiple records in one TCP write. */
        if (s->rx_len > 0) {
            SecBuffer bufs[4];
            bufs[0].BufferType = SECBUFFER_DATA;
            bufs[0].pvBuffer   = s->rx_buf;
            bufs[0].cbBuffer   = s->rx_len;
            bufs[1].BufferType = SECBUFFER_EMPTY;
            bufs[1].pvBuffer   = NULL;
            bufs[1].cbBuffer   = 0;
            bufs[2].BufferType = SECBUFFER_EMPTY;
            bufs[2].pvBuffer   = NULL;
            bufs[2].cbBuffer   = 0;
            bufs[3].BufferType = SECBUFFER_EMPTY;
            bufs[3].pvBuffer   = NULL;
            bufs[3].cbBuffer   = 0;

            SecBufferDesc desc;
            desc.ulVersion = SECBUFFER_VERSION;
            desc.cBuffers  = 4;
            desc.pBuffers  = bufs;

            SECURITY_STATUS st = DecryptMessage(&s->ctx, &desc, 0, NULL);

            if (st == SEC_E_OK || st == SEC_I_RENEGOTIATE ||
                st == SEC_I_CONTEXT_EXPIRED)
            {
                SecBuffer* data_buf  = NULL;
                SecBuffer* extra_buf = NULL;
                for (int i = 0; i < 4; i++) {
                    if (bufs[i].BufferType == SECBUFFER_DATA)  data_buf  = &bufs[i];
                    if (bufs[i].BufferType == SECBUFFER_EXTRA) extra_buf = &bufs[i];
                }

                uint32_t plain_len = data_buf ? data_buf->cbBuffer : 0;
                uint32_t consumed  = s->rx_len - (extra_buf ? extra_buf->cbBuffer : 0);

                if (plain_len > 0) {
                    /* Copy what fits; spill the rest. */
                    uint32_t copy = plain_len < buf_len ? plain_len : (uint32_t)buf_len;
                    memcpy(buf, data_buf->pvBuffer, copy);
                    if (plain_len > copy) {
                        uint32_t rem = plain_len - copy;
                        if (rem <= s->plain_cap) {
                            memcpy(s->plain, (uint8_t*)data_buf->pvBuffer + copy, rem);
                            s->plain_off = 0;
                            s->plain_len = rem;
                        }
                    }

                    /* Slide EXTRA ciphertext to the start of rx_buf. */
                    if (extra_buf && extra_buf->cbBuffer > 0) {
                        memmove(s->rx_buf,
                                s->rx_buf + consumed,
                                extra_buf->cbBuffer);
                        s->rx_len = extra_buf->cbBuffer;
                    } else {
                        s->rx_len = 0;
                    }
                    if (st == SEC_I_CONTEXT_EXPIRED && copy == 0) return 0;
                    return (int)copy;
                }

                /* Empty plaintext (e.g. a record with zero payload).
                 * Slide EXTRA and loop. */
                if (extra_buf && extra_buf->cbBuffer > 0) {
                    memmove(s->rx_buf, s->rx_buf + consumed, extra_buf->cbBuffer);
                    s->rx_len = extra_buf->cbBuffer;
                } else {
                    s->rx_len = 0;
                }
                if (st == SEC_I_CONTEXT_EXPIRED) return 0; /* TLS close_notify */
                /* Renegotiate: not handled; treat as fatal. */
                if (st == SEC_I_RENEGOTIATE) return -1;
                continue;
            }

            if (st == SEC_E_INCOMPLETE_MESSAGE) {
                /* Fall through to read more bytes. */
            } else {
                /* Other status = fatal. */
                return -1;
            }
        }

        /* Read more ciphertext. */
        if (s->rx_len >= s->rx_cap) return -1;
        int got = recv(sock, (char*)s->rx_buf + s->rx_len,
                       (int)(s->rx_cap - s->rx_len), 0);
        if (got == 0) return 0;   /* peer closed */
        if (got < 0)  return -1;
        s->rx_len += (uint32_t)got;
    }
}

void wapi_host_net_tls_close(void* state, wapi_socket_t sock) {
    wapi_tls_session_t* s = (wapi_tls_session_t*)state;
    if (!s) return;
    /* Best-effort TLS close_notify. Ignore failures — we're tearing
     * down regardless. */
    if (s->ctx_valid) {
        DWORD type = SCHANNEL_SHUTDOWN;
        SecBuffer b;
        b.BufferType = SECBUFFER_TOKEN;
        b.pvBuffer   = &type;
        b.cbBuffer   = sizeof(type);
        SecBufferDesc d;
        d.ulVersion = SECBUFFER_VERSION;
        d.cBuffers  = 1;
        d.pBuffers  = &b;
        ApplyControlToken(&s->ctx, &d);

        SecBuffer out_b;
        out_b.BufferType = SECBUFFER_TOKEN;
        out_b.pvBuffer   = NULL;
        out_b.cbBuffer   = 0;
        SecBufferDesc out_d;
        out_d.ulVersion = SECBUFFER_VERSION;
        out_d.cBuffers  = 1;
        out_d.pBuffers  = &out_b;
        DWORD flags = 0;
        SECURITY_STATUS st = InitializeSecurityContextW(
            &s->cred, &s->ctx, NULL,
            ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT |
            ISC_REQ_SEQUENCE_DETECT | ISC_REQ_ALLOCATE_MEMORY |
            (s->dtls ? ISC_REQ_DATAGRAM : ISC_REQ_STREAM),
            0, 0, NULL, 0, NULL, &out_d, &flags, NULL);
        if (st == SEC_E_OK && out_b.pvBuffer && out_b.cbBuffer > 0) {
            (void)send_all(sock, out_b.pvBuffer, (int)out_b.cbBuffer);
            FreeContextBuffer(out_b.pvBuffer);
        }
    }
    tls_free(s);
}
