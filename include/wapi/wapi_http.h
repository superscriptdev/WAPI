/**
 * WAPI - HTTP
 * Version 1.0.0
 *
 * One-shot HTTP requests (GET / POST / PUT / DELETE / HEAD).
 *
 * Operations go through the async I/O object as WAPI_IO_OP_HTTP_FETCH.
 * Each submit performs a single request, writes the response body into
 * the caller-provided output buffer, and delivers a single
 * WAPI_EVENT_IO_COMPLETION with the number of bytes written and the
 * HTTP status code in the completion's flags field. There is no
 * streaming handle, no header API, no cookie jar, no per-chunk
 * write/read — that fits the browser's `fetch()` API naturally without
 * introducing a stateful sync contract that browsers cannot honour.
 *
 * Maps to:
 *   Web:     fetch() (CORS rules apply)
 *   Native:  libcurl, WinHTTP, NSURLSession
 *
 * Usage:
 *
 *   wapi_io_op_t op = {0};
 *   op.opcode    = WAPI_IO_OP_HTTP_FETCH;
 *   op.addr      = (uintptr_t)url_bytes;
 *   op.len       = url_len;
 *   op.addr2     = (uintptr_t)response_buffer;
 *   op.len2      = response_capacity;
 *   op.flags     = WAPI_HTTP_GET;
 *   op.user_data = my_correlation_token;
 *   io->submit(io->impl, &op, 1);
 *
 *   // Later, a WAPI_EVENT_IO_COMPLETION arrives where
 *   //   event.io.user_data == my_correlation_token
 *   //   event.io.result    == bytes_written (or negative error)
 *   //   event.io.flags     == http_status_code (e.g. 200, 404)
 *
 * If the response exceeds the caller-provided capacity the completion
 * reports a negative error and the buffer contents are unspecified.
 */

#ifndef WAPI_HTTP_H
#define WAPI_HTTP_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * HTTP Methods (flags field on WAPI_IO_OP_HTTP_FETCH)
 * ============================================================ */

typedef enum wapi_http_method_t {
    WAPI_HTTP_GET     = 0,
    WAPI_HTTP_POST    = 1,
    WAPI_HTTP_PUT     = 2,
    WAPI_HTTP_DELETE  = 3,
    WAPI_HTTP_HEAD    = 4,
    WAPI_HTTP_FORCE32 = 0x7FFFFFFF
} wapi_http_method_t;

/* ============================================================
 * Inline helper — fills a wapi_io_op_t for an HTTP_FETCH op and
 * submits it through the supplied IO vtable. The caller keeps
 * ownership of url_bytes and response_buffer; both must remain
 * valid until the matching WAPI_EVENT_IO_COMPLETION arrives.
 *
 * Returns the number of ops the backend accepted (0 or 1).
 * ============================================================ */

static inline int32_t wapi_http_fetch(const wapi_io_t*   io,
                                      const void*        url_bytes,
                                      uint32_t           url_len,
                                      void*              response_buffer,
                                      uint32_t           response_capacity,
                                      wapi_http_method_t method,
                                      uint64_t           user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_HTTP_FETCH;
    op.flags     = (uint32_t)method;
    op.addr      = (uint64_t)(uintptr_t)url_bytes;
    op.len       = (uint64_t)url_len;
    op.addr2     = (uint64_t)(uintptr_t)response_buffer;
    op.len2      = (uint64_t)response_capacity;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_HTTP_H */
