/**
 * WAPI Desktop Runtime — Compression (wapi_compression.h)
 *
 * Implements the sole compression entry-point, WAPI_IO_OP_COMPRESS_PROCESS,
 * as a synchronous op-ctx handler. Every submit processes one input
 * buffer into one output buffer in full and completes with:
 *   result    = bytes written, or a negative WAPI_ERR_* on failure
 *   flags     (request) = wapi_compress_algo_t
 *   flags2    (request) = wapi_compress_mode_t (compress / decompress)
 *
 * Backing: vendored miniz (src/third_party/miniz/) for gzip, deflate,
 * and raw deflate. zstd / lz4 complete with WAPI_ERR_NOTSUP since the
 * host does not link the relevant libraries.
 */

#include "wapi_host.h"
#include "third_party/miniz/miniz.h"

/* Mirrors of enums from wapi_compression.h (kept local to avoid
 * forcing a cross-module include into the shared host header). */
#define CA_GZIP         0
#define CA_DEFLATE      1
#define CA_DEFLATE_RAW  2
#define CA_ZSTD         3
#define CA_LZ4          4

#define CM_COMPRESS     0
#define CM_DECOMPRESS   1

static wapi_result_t miniz_err_to_wapi(int mz) {
    switch (mz) {
        case MZ_OK:          return WAPI_OK;
        case MZ_BUF_ERROR:   return WAPI_ERR_OVERFLOW; /* output too small */
        case MZ_DATA_ERROR:  return WAPI_ERR_INVAL;
        case MZ_MEM_ERROR:   return WAPI_ERR_NOMEM;
        case MZ_STREAM_END:  return WAPI_OK;
        default:             return WAPI_ERR_IO;
    }
}

/* One-shot compress/decompress through miniz. Returns negative WAPI_ERR_*
 * on failure, otherwise bytes produced. */
static int32_t run_deflate(const void* in, size_t in_len,
                           void* out, size_t out_cap,
                           int window_bits, int mode)
{
    mz_stream s;
    memset(&s, 0, sizeof(s));
    int rc;
    if (mode == CM_COMPRESS) {
        rc = mz_deflateInit2(&s,
                             MZ_DEFAULT_COMPRESSION,
                             MZ_DEFLATED,
                             window_bits,
                             /* mem_level */ 8,
                             MZ_DEFAULT_STRATEGY);
        if (rc != MZ_OK) return (int32_t)miniz_err_to_wapi(rc);

        s.next_in   = (const unsigned char*)in;
        s.avail_in  = (mz_uint32)in_len;
        s.next_out  = (unsigned char*)out;
        s.avail_out = (mz_uint32)out_cap;
        rc = mz_deflate(&s, MZ_FINISH);
        size_t written = s.total_out;
        mz_deflateEnd(&s);
        if (rc != MZ_STREAM_END) return (int32_t)miniz_err_to_wapi(rc);
        return (int32_t)written;
    } else {
        rc = mz_inflateInit2(&s, window_bits);
        if (rc != MZ_OK) return (int32_t)miniz_err_to_wapi(rc);

        s.next_in   = (const unsigned char*)in;
        s.avail_in  = (mz_uint32)in_len;
        s.next_out  = (unsigned char*)out;
        s.avail_out = (mz_uint32)out_cap;
        rc = mz_inflate(&s, MZ_FINISH);
        size_t written = s.total_out;
        mz_inflateEnd(&s);
        if (rc != MZ_STREAM_END) return (int32_t)miniz_err_to_wapi(rc);
        return (int32_t)written;
    }
}

/* miniz window-bits conventions:
 *   15            — zlib (deflate) wrapper
 *   -15           — raw deflate (no wrapper)
 *   15 + 16 = 31  — gzip wrapper (when compressing; inflate accepts 32 | 15 = auto detect)
 * For decompression we use the matching positive for zlib, negative for
 * raw, and 32 + 15 for gzip (miniz accepts this per zlib convention). */

void wapi_host_compress_process_op(op_ctx_t* c) {
    uint32_t algo = c->flags;
    uint32_t mode = c->flags2;

    if (algo == CA_ZSTD || algo == CA_LZ4) {
        c->result = WAPI_ERR_NOTSUP;
        c->cqe_flags |= 0x0008 /* WAPI_IO_CQE_F_NOSYS */;
        return;
    }
    if (algo != CA_GZIP && algo != CA_DEFLATE && algo != CA_DEFLATE_RAW) {
        c->result = WAPI_ERR_INVAL;
        return;
    }
    if (mode != CM_COMPRESS && mode != CM_DECOMPRESS) {
        c->result = WAPI_ERR_INVAL;
        return;
    }

    const void* in = (c->len == 0) ? NULL :
                     wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len);
    void*       out = (c->len2 == 0) ? NULL :
                      wapi_wasm_ptr((uint32_t)c->addr2, (uint32_t)c->len2);
    if ((c->len > 0 && !in) || (c->len2 > 0 && !out)) {
        c->result = WAPI_ERR_INVAL;
        return;
    }

    int window_bits;
    switch (algo) {
        case CA_GZIP:
            window_bits = (mode == CM_COMPRESS) ? (15 + 16) : (15 + 32);
            break;
        case CA_DEFLATE:
            window_bits = 15;
            break;
        case CA_DEFLATE_RAW:
            window_bits = -15;
            break;
        default:
            c->result = WAPI_ERR_INVAL;
            return;
    }

    c->result = run_deflate(in, (size_t)c->len, out, (size_t)c->len2,
                            window_bits, (int)mode);
}
