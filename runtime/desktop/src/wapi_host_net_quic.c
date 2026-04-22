/**
 * WAPI Desktop Runtime — msquic transport
 *
 * QUIC client via Microsoft's msquic library. msquic.dll is loaded
 * dynamically so the runtime binary does not depend on it at link
 * time — connect returns WAPI_ERR_NOTSUP when the library is absent.
 * This file mirrors the minimal SSPI-style vtable usage from the
 * msquic quickstart:
 *
 *   1. `MsQuicOpenVersion(2, &api)` loads the API jump table.
 *   2. A process-wide registration + client configuration are lazy-
 *      initialised on first connect.
 *   3. Each `wapi_host_net_quic_connect` opens a connection, starts
 *      a single bidirectional stream, and uses Win32 events to
 *      marshal the async callback world into blocking send/recv that
 *      fits WAPI's op-ctx contract.
 *
 * Scope: one bidi stream per connection — QUIC multiplexing is
 * unused here. An app that wants channels uses the existing
 * NETWORK_CHANNEL_OPEN opcode (still NOSYS), which will ride on
 * msquic.StreamOpen once implemented.
 */

#include "wapi_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/* --- Local copies of the msquic API surface we need. Keeping them
 *     inline lets us build without the msquic SDK headers; the ABI
 *     is stable across msquic v2.x. Layouts from the upstream
 *     `msquic.h` on github.com/microsoft/msquic. --- */

#define QUIC_VERSION_2 2

typedef struct QUIC_HANDLE* HQUIC;

typedef enum QUIC_STATUS {
    QUIC_STATUS_SUCCESS               = 0x00000000,
    QUIC_STATUS_PENDING               = 0x703E5,   /* QUIC_STATUS_PENDING = HRESULT 0x0703E5 */
    QUIC_STATUS_CONTINUE              = 0x704DE,
    QUIC_STATUS_OUT_OF_MEMORY         = 0x8007000E,
    QUIC_STATUS_INVALID_PARAMETER     = 0x80070057,
    QUIC_STATUS_INVALID_STATE         = 0x8007139F,
    QUIC_STATUS_NOT_SUPPORTED         = 0x80004001,
    QUIC_STATUS_NOT_FOUND             = 0x80070490,
    QUIC_STATUS_ABORTED               = 0x80004004,
    QUIC_STATUS_ADDRESS_IN_USE        = 0x80072740,
    QUIC_STATUS_CONNECTION_TIMEOUT    = 0x80410006,
    QUIC_STATUS_CONNECTION_IDLE       = 0x80410005,
    QUIC_STATUS_INTERNAL_ERROR        = 0x80004005,
    QUIC_STATUS_CONNECTION_REFUSED    = 0x800704C9,
    QUIC_STATUS_PROTOCOL_ERROR        = 0x80410001,
    QUIC_STATUS_VER_NEG_ERROR         = 0x80410002,
    QUIC_STATUS_USER_CANCELED         = 0x80410003,
    QUIC_STATUS_ALPN_NEG_FAILURE      = 0x80410004,
    QUIC_STATUS_STREAM_LIMIT_REACHED  = 0x80410007,
    QUIC_STATUS_ALPN_IN_USE           = 0x80410008,
    QUIC_STATUS_CLOSE_NOTIFY          = 0x80410009,
    QUIC_STATUS_BAD_CERTIFICATE       = 0x8041000A,
} QUIC_STATUS;

typedef enum QUIC_EXECUTION_PROFILE { QUIC_EXECUTION_PROFILE_LOW_LATENCY = 0 } QUIC_EXECUTION_PROFILE;

typedef struct QUIC_REGISTRATION_CONFIG {
    const char*            AppName;
    QUIC_EXECUTION_PROFILE ExecutionProfile;
} QUIC_REGISTRATION_CONFIG;

typedef enum QUIC_CREDENTIAL_TYPE { QUIC_CREDENTIAL_TYPE_NONE = 0 } QUIC_CREDENTIAL_TYPE;
typedef enum QUIC_CREDENTIAL_FLAGS {
    QUIC_CREDENTIAL_FLAG_CLIENT                    = 0x00000001,
    QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION = 0x00000008,
} QUIC_CREDENTIAL_FLAGS;

typedef struct QUIC_CREDENTIAL_CONFIG {
    QUIC_CREDENTIAL_TYPE  Type;
    QUIC_CREDENTIAL_FLAGS Flags;
    void*                 Params; /* polymorphic; NULL for NONE+CLIENT */
    const char*           Principal;
    void*                 Reserved;
    void*                 AsyncHandler;
    uint32_t              AllowedCipherSuites;
} QUIC_CREDENTIAL_CONFIG;

typedef struct QUIC_BUFFER { uint32_t Length; uint8_t* Buffer; } QUIC_BUFFER;

typedef enum QUIC_SETTINGS_FLAGS_ { QUIC_SETTINGS_VERSION = 1 } QUIC_SETTINGS_FLAGS_;

typedef struct QUIC_SETTINGS {
    union { uint64_t IsSetFlags; uint8_t _raw[8]; } IsSet;
    /* Remaining fields unused; zero-init is fine and msquic fills
     * defaults when IsSet bits are clear. 512 bytes is safely larger
     * than the current struct. */
    uint8_t  _rest[512];
} QUIC_SETTINGS;

typedef enum QUIC_CONNECTION_SHUTDOWN_FLAGS {
    QUIC_CONNECTION_SHUTDOWN_FLAG_NONE = 0,
} QUIC_CONNECTION_SHUTDOWN_FLAGS;
typedef enum QUIC_STREAM_OPEN_FLAGS {
    QUIC_STREAM_OPEN_FLAG_NONE = 0,
} QUIC_STREAM_OPEN_FLAGS;
typedef enum QUIC_STREAM_START_FLAGS {
    QUIC_STREAM_START_FLAG_NONE = 0,
} QUIC_STREAM_START_FLAGS;
typedef enum QUIC_STREAM_SHUTDOWN_FLAGS {
    QUIC_STREAM_SHUTDOWN_FLAG_GRACEFUL = 0x0001,
} QUIC_STREAM_SHUTDOWN_FLAGS;
typedef enum QUIC_SEND_FLAGS {
    QUIC_SEND_FLAG_NONE = 0,
} QUIC_SEND_FLAGS;

/* Event callback bodies are opaque in this minimal binding — we
 * pass only our session pointer and watch a small set of event
 * types. */
typedef struct QUIC_CONNECTION_EVENT {
    uint32_t Type;
    uint8_t  _body[2040];
} QUIC_CONNECTION_EVENT;
typedef struct QUIC_STREAM_EVENT {
    uint32_t Type;
    uint8_t  _body[2040];
} QUIC_STREAM_EVENT;

/* Event Type values from msquic's public header. */
#define QUIC_CONNECTION_EVENT_CONNECTED                 0
#define QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE         2

#define QUIC_STREAM_EVENT_START_COMPLETE                0
#define QUIC_STREAM_EVENT_RECEIVE                       1
#define QUIC_STREAM_EVENT_SEND_COMPLETE                 2
#define QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN            3
#define QUIC_STREAM_EVENT_PEER_SEND_ABORTED             4
#define QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE             7

typedef QUIC_STATUS (__stdcall *QUIC_CONNECTION_CALLBACK)(HQUIC, void*, QUIC_CONNECTION_EVENT*);
typedef QUIC_STATUS (__stdcall *QUIC_STREAM_CALLBACK)    (HQUIC, void*, QUIC_STREAM_EVENT*);

typedef struct QUIC_API_TABLE {
    /* Slots 0..27 cover the msquic v2 public surface. Only the ones
     * we call are typed accurately; the rest are void* placeholders
     * to keep the offset right. */
    void* SetContext;
    void* GetContext;
    void* SetCallbackHandler;
    void* SetParam;
    void* GetParam;
    QUIC_STATUS (__stdcall *RegistrationOpen)(const QUIC_REGISTRATION_CONFIG*, HQUIC*);
    void (__stdcall *RegistrationClose)(HQUIC);
    void* RegistrationShutdown;
    QUIC_STATUS (__stdcall *ConfigurationOpen)(HQUIC, const QUIC_BUFFER*, uint32_t,
                                               const QUIC_SETTINGS*, uint32_t, void*, HQUIC*);
    void (__stdcall *ConfigurationClose)(HQUIC);
    QUIC_STATUS (__stdcall *ConfigurationLoadCredential)(HQUIC, const QUIC_CREDENTIAL_CONFIG*);
    void* ListenerOpen;
    void* ListenerClose;
    void* ListenerStart;
    void* ListenerStop;
    QUIC_STATUS (__stdcall *ConnectionOpen)(HQUIC, QUIC_CONNECTION_CALLBACK, void*, HQUIC*);
    void (__stdcall *ConnectionClose)(HQUIC);
    void (__stdcall *ConnectionShutdown)(HQUIC, QUIC_CONNECTION_SHUTDOWN_FLAGS, uint64_t);
    QUIC_STATUS (__stdcall *ConnectionStart)(HQUIC, HQUIC, uint32_t /*family*/,
                                             const char* /*server_name*/, uint16_t /*port*/);
    void* ConnectionSetConfiguration;
    void* ConnectionSendResumptionTicket;
    QUIC_STATUS (__stdcall *StreamOpen)(HQUIC, QUIC_STREAM_OPEN_FLAGS, QUIC_STREAM_CALLBACK,
                                        void*, HQUIC*);
    void (__stdcall *StreamClose)(HQUIC);
    QUIC_STATUS (__stdcall *StreamStart)(HQUIC, QUIC_STREAM_START_FLAGS);
    QUIC_STATUS (__stdcall *StreamShutdown)(HQUIC, QUIC_STREAM_SHUTDOWN_FLAGS, uint64_t);
    QUIC_STATUS (__stdcall *StreamSend)(HQUIC, const QUIC_BUFFER*, uint32_t,
                                        QUIC_SEND_FLAGS, void*);
    void (__stdcall *StreamReceiveComplete)(HQUIC, uint64_t);
    QUIC_STATUS (__stdcall *StreamReceiveSetEnabled)(HQUIC, BOOLEAN);
    /* Remainder not used. */
} QUIC_API_TABLE;

typedef QUIC_STATUS (__stdcall *fn_MsQuicOpenVersion)(uint32_t, const QUIC_API_TABLE**);
typedef void        (__stdcall *fn_MsQuicClose)(const QUIC_API_TABLE*);

/* --- Process-wide state --- */

static struct {
    HMODULE                 dll;
    const QUIC_API_TABLE*   api;
    HQUIC                   registration;
    HQUIC                   configuration;
    bool                    tried;
} g_quic;

/* Pure runtime-availability check — does NOT load msquic.dll or
 * register anything. Used by the qualities-probe import so the
 * query stays side-effect-free per spec. */
bool wapi_host_net_quic_available(void) {
    HMODULE probe = LoadLibraryExW(L"msquic.dll", NULL,
                                   LOAD_LIBRARY_AS_DATAFILE);
    if (!probe) return false;
    FreeLibrary(probe);
    return true;
}

static bool quic_ensure(void) {
    if (g_quic.api) return true;
    if (g_quic.tried) return false;
    g_quic.tried = true;

    g_quic.dll = LoadLibraryW(L"msquic.dll");
    if (!g_quic.dll) return false;

    fn_MsQuicOpenVersion MsQuicOpenVersion =
        (fn_MsQuicOpenVersion)GetProcAddress(g_quic.dll, "MsQuicOpenVersion");
    if (!MsQuicOpenVersion) return false;
    if (MsQuicOpenVersion(QUIC_VERSION_2, &g_quic.api) != QUIC_STATUS_SUCCESS) {
        g_quic.api = NULL; return false;
    }

    QUIC_REGISTRATION_CONFIG reg = { "WAPI", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
    if (g_quic.api->RegistrationOpen(&reg, &g_quic.registration) != QUIC_STATUS_SUCCESS) {
        return false;
    }

    /* ALPN: use "h3" — the most common client scenario. A richer
     * API can let the guest pick, but that means extending the op
     * descriptor. Today: one ALPN. */
    QUIC_BUFFER alpn = { 2, (uint8_t*)"h3" };
    QUIC_SETTINGS settings = {0};
    /* IdleTimeoutMs slot (bit 0 of IsSet). Setting to 0 = default. */
    if (g_quic.api->ConfigurationOpen(g_quic.registration, &alpn, 1,
                                      &settings, sizeof(settings),
                                      NULL, &g_quic.configuration) != QUIC_STATUS_SUCCESS) {
        return false;
    }

    QUIC_CREDENTIAL_CONFIG cc = {0};
    cc.Type  = QUIC_CREDENTIAL_TYPE_NONE;
    cc.Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
    if (g_quic.api->ConfigurationLoadCredential(g_quic.configuration, &cc) != QUIC_STATUS_SUCCESS) {
        return false;
    }
    return true;
}

/* --- Per-connection session --- */

#define QUIC_RX_RING  (128 * 1024)

typedef struct wapi_quic_session_t {
    HQUIC   conn;
    HQUIC   stream;
    HANDLE  connected_ev;
    HANDLE  stream_ready_ev;
    HANDLE  rx_ev;              /* pulsed when rx_ring has data */
    HANDLE  send_complete_ev;   /* pulsed when StreamSend completes */
    CRITICAL_SECTION lock;      /* guards rx_ring + peer_shutdown */
    uint8_t rx_ring[QUIC_RX_RING];
    uint32_t rx_head, rx_tail;  /* rx_tail == rx_head → empty */
    bool    peer_shutdown;
    bool    connected;
    bool    failed;
} wapi_quic_session_t;

static uint32_t rx_available(const wapi_quic_session_t* s) {
    return (s->rx_head - s->rx_tail) & (QUIC_RX_RING - 1);
}
static uint32_t rx_free_space(const wapi_quic_session_t* s) {
    return QUIC_RX_RING - 1 - rx_available(s);
}

static void rx_push(wapi_quic_session_t* s, const uint8_t* data, uint32_t len) {
    /* Callers hold s->lock. Drop bytes that would overflow — keep
     * latency bounded (same policy as the audio ring). */
    uint32_t space = rx_free_space(s);
    if (len > space) len = space;
    uint32_t first = QUIC_RX_RING - (s->rx_head & (QUIC_RX_RING - 1));
    if (first > len) first = len;
    memcpy(s->rx_ring + (s->rx_head & (QUIC_RX_RING - 1)), data, first);
    if (len > first) memcpy(s->rx_ring, data + first, len - first);
    s->rx_head += len;
}

static uint32_t rx_drain(wapi_quic_session_t* s, uint8_t* dst, uint32_t cap) {
    uint32_t avail = rx_available(s);
    if (avail > cap) avail = cap;
    uint32_t first = QUIC_RX_RING - (s->rx_tail & (QUIC_RX_RING - 1));
    if (first > avail) first = avail;
    memcpy(dst, s->rx_ring + (s->rx_tail & (QUIC_RX_RING - 1)), first);
    if (avail > first) memcpy(dst + first, s->rx_ring, avail - first);
    s->rx_tail += avail;
    return avail;
}

/* --- Callbacks --- */

static QUIC_STATUS __stdcall stream_cb(HQUIC stream, void* ctx, QUIC_STREAM_EVENT* ev) {
    (void)stream;
    wapi_quic_session_t* s = (wapi_quic_session_t*)ctx;
    switch (ev->Type) {
    case QUIC_STREAM_EVENT_START_COMPLETE:
        SetEvent(s->stream_ready_ev);
        break;
    case QUIC_STREAM_EVENT_RECEIVE: {
        /* Body of QUIC_STREAM_EVENT_RECEIVE (as of msquic v2):
         *   u64 AbsoluteOffset, u64 TotalBufferLength,
         *   QUIC_BUFFER* Buffers, u32 BufferCount, u32 Flags */
        uint64_t total_len;
        QUIC_BUFFER* bufs;
        uint32_t buf_count;
        memcpy(&total_len, ev->_body + 8, 8);
        memcpy(&bufs,      ev->_body + 16, sizeof(void*));
        memcpy(&buf_count, ev->_body + 16 + sizeof(void*), 4);
        EnterCriticalSection(&s->lock);
        for (uint32_t i = 0; i < buf_count; i++) {
            rx_push(s, bufs[i].Buffer, bufs[i].Length);
        }
        LeaveCriticalSection(&s->lock);
        SetEvent(s->rx_ev);
        (void)total_len;
        return QUIC_STATUS_SUCCESS; /* synchronous: msquic recycles the buffer */
    }
    case QUIC_STREAM_EVENT_SEND_COMPLETE:
        SetEvent(s->send_complete_ev);
        break;
    case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
    case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
        EnterCriticalSection(&s->lock);
        s->peer_shutdown = true;
        LeaveCriticalSection(&s->lock);
        SetEvent(s->rx_ev);
        break;
    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
        EnterCriticalSection(&s->lock);
        s->peer_shutdown = true;
        LeaveCriticalSection(&s->lock);
        SetEvent(s->rx_ev);
        break;
    }
    return QUIC_STATUS_SUCCESS;
}

static QUIC_STATUS __stdcall conn_cb(HQUIC conn, void* ctx, QUIC_CONNECTION_EVENT* ev) {
    (void)conn;
    wapi_quic_session_t* s = (wapi_quic_session_t*)ctx;
    switch (ev->Type) {
    case QUIC_CONNECTION_EVENT_CONNECTED:
        s->connected = true;
        SetEvent(s->connected_ev);
        break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        if (!s->connected) s->failed = true;
        SetEvent(s->connected_ev);
        SetEvent(s->rx_ev);
        break;
    }
    return QUIC_STATUS_SUCCESS;
}

/* --- Public interface (called from wapi_host_net.c) --- */

int wapi_host_net_quic_connect(const char* host, size_t host_len, uint16_t port,
                               void** out_state)
{
    if (!out_state) return -1;
    *out_state = NULL;
    if (!quic_ensure()) return -2; /* msquic.dll not installed */

    wapi_quic_session_t* s = (wapi_quic_session_t*)calloc(1, sizeof(*s));
    if (!s) return -1;
    InitializeCriticalSection(&s->lock);
    s->connected_ev     = CreateEventW(NULL, TRUE, FALSE, NULL);
    s->stream_ready_ev  = CreateEventW(NULL, TRUE, FALSE, NULL);
    s->rx_ev            = CreateEventW(NULL, FALSE, FALSE, NULL);
    s->send_complete_ev = CreateEventW(NULL, FALSE, FALSE, NULL);

    if (g_quic.api->ConnectionOpen(g_quic.registration, conn_cb, s, &s->conn) != QUIC_STATUS_SUCCESS) {
        goto fail;
    }

    /* Copy host into a NUL-terminated buffer for ConnectionStart. */
    char host_buf[256];
    size_t n = host_len < sizeof(host_buf) - 1 ? host_len : sizeof(host_buf) - 1;
    memcpy(host_buf, host, n); host_buf[n] = 0;

    if (g_quic.api->ConnectionStart(s->conn, g_quic.configuration,
                                    0 /* AF_UNSPEC */, host_buf, port) != QUIC_STATUS_SUCCESS) {
        goto fail;
    }

    /* Wait for CONNECTED or SHUTDOWN_COMPLETE. Hard 10s timeout to
     * avoid hanging a misconfigured client forever. */
    if (WaitForSingleObject(s->connected_ev, 10000) != WAIT_OBJECT_0 || s->failed) {
        goto fail;
    }

    if (g_quic.api->StreamOpen(s->conn, QUIC_STREAM_OPEN_FLAG_NONE,
                               stream_cb, s, &s->stream) != QUIC_STATUS_SUCCESS) {
        goto fail;
    }
    if (g_quic.api->StreamStart(s->stream, QUIC_STREAM_START_FLAG_NONE) != QUIC_STATUS_SUCCESS) {
        goto fail;
    }
    if (WaitForSingleObject(s->stream_ready_ev, 5000) != WAIT_OBJECT_0) {
        goto fail;
    }

    *out_state = s;
    return 0;

fail:
    if (s->stream) g_quic.api->StreamClose(s->stream);
    if (s->conn)   g_quic.api->ConnectionClose(s->conn);
    if (s->connected_ev)     CloseHandle(s->connected_ev);
    if (s->stream_ready_ev)  CloseHandle(s->stream_ready_ev);
    if (s->rx_ev)            CloseHandle(s->rx_ev);
    if (s->send_complete_ev) CloseHandle(s->send_complete_ev);
    DeleteCriticalSection(&s->lock);
    free(s);
    return -1;
}

int wapi_host_net_quic_send(void* state, const void* data, size_t len) {
    wapi_quic_session_t* s = (wapi_quic_session_t*)state;
    if (!s || !s->stream) return -1;
    QUIC_BUFFER buf = { (uint32_t)len, (uint8_t*)data };
    ResetEvent(s->send_complete_ev);
    if (g_quic.api->StreamSend(s->stream, &buf, 1,
                               QUIC_SEND_FLAG_NONE, NULL) != QUIC_STATUS_SUCCESS) {
        return -1;
    }
    if (WaitForSingleObject(s->send_complete_ev, INFINITE) != WAIT_OBJECT_0) {
        return -1;
    }
    return (int)len;
}

int wapi_host_net_quic_recv(void* state, void* buf, size_t cap) {
    wapi_quic_session_t* s = (wapi_quic_session_t*)state;
    if (!s || !s->stream) return -1;
    for (;;) {
        EnterCriticalSection(&s->lock);
        if (rx_available(s) > 0) {
            uint32_t got = rx_drain(s, (uint8_t*)buf, (uint32_t)cap);
            LeaveCriticalSection(&s->lock);
            return (int)got;
        }
        bool eof = s->peer_shutdown;
        LeaveCriticalSection(&s->lock);
        if (eof) return 0;
        WaitForSingleObject(s->rx_ev, INFINITE);
    }
}

void wapi_host_net_quic_close(void* state) {
    wapi_quic_session_t* s = (wapi_quic_session_t*)state;
    if (!s) return;
    if (s->stream) {
        g_quic.api->StreamShutdown(s->stream, QUIC_STREAM_SHUTDOWN_FLAG_GRACEFUL, 0);
        g_quic.api->StreamClose(s->stream);
    }
    if (s->conn) {
        g_quic.api->ConnectionShutdown(s->conn, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
        g_quic.api->ConnectionClose(s->conn);
    }
    CloseHandle(s->connected_ev);
    CloseHandle(s->stream_ready_ev);
    CloseHandle(s->rx_ev);
    CloseHandle(s->send_complete_ev);
    DeleteCriticalSection(&s->lock);
    free(s);
}
