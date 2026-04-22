/**
 * WAPI Desktop Runtime — HTTP (wapi_http.h)
 *
 * One-shot fetch through WinHTTP. The op-ctx handler runs sync on
 * the dispatch thread; each submit sends the request, reads the
 * full body into the caller-provided buffer, and completes with
 * `result` = bytes_written and `cqe_flags` = HTTP status code.
 *
 * No cookie jar, no header API, no streaming — matches the shape
 * spec'd in wapi_http.h. A body > `len2` completes with
 * WAPI_ERR_OVERFLOW (buffer contents unspecified).
 */

#include "wapi_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp")

/* wapi_http_method_t */
#define M_GET    0
#define M_POST   1
#define M_PUT    2
#define M_DELETE 3
#define M_HEAD   4

static const WCHAR* method_verb(uint32_t m) {
    switch (m) {
    case M_GET:    return L"GET";
    case M_POST:   return L"POST";
    case M_PUT:    return L"PUT";
    case M_DELETE: return L"DELETE";
    case M_HEAD:   return L"HEAD";
    default:       return L"GET";
    }
}

void wapi_host_http_fetch_op(op_ctx_t* c) {
    /* url   = addr/len (UTF-8)
     * out   = addr2/len2 (response body buffer)
     * flags = method  */
    if (c->len == 0) { c->result = WAPI_ERR_INVAL; return; }
    const char* url = (const char*)wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len);
    void* out       = c->len2 ? wapi_wasm_ptr((uint32_t)c->addr2, (uint32_t)c->len2) : NULL;
    if (!url || (c->len2 > 0 && !out)) { c->result = WAPI_ERR_INVAL; return; }

    /* Widen the URL so we can feed it to WinHttpCrackUrl. */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, url, (int)c->len, NULL, 0);
    if (wlen <= 0) { c->result = WAPI_ERR_INVAL; return; }
    WCHAR* wurl = (WCHAR*)malloc(((size_t)wlen + 1) * sizeof(WCHAR));
    if (!wurl) { c->result = WAPI_ERR_NOMEM; return; }
    MultiByteToWideChar(CP_UTF8, 0, url, (int)c->len, wurl, wlen);
    wurl[wlen] = 0;

    HINTERNET hs = NULL, hc = NULL, hr = NULL;
    URL_COMPONENTSW u; memset(&u, 0, sizeof(u));
    u.dwStructSize      = sizeof(u);
    u.dwSchemeLength    = (DWORD)-1;
    u.dwHostNameLength  = (DWORD)-1;
    u.dwUrlPathLength   = (DWORD)-1;
    u.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wurl, (DWORD)wlen, 0, &u)) {
        c->result = WAPI_ERR_INVAL;
        goto cleanup;
    }

    hs = WinHttpOpen(L"WAPI/1.0",
                     WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hs) { c->result = WAPI_ERR_IO; goto cleanup; }

    /* WinHttpConnect needs a pure host (no port) — the URL_COMPONENTS
     * fields are not NUL-terminated, so we copy. */
    WCHAR host[256];
    DWORD hlen = u.dwHostNameLength < 255 ? u.dwHostNameLength : 255;
    memcpy(host, u.lpszHostName, hlen * sizeof(WCHAR));
    host[hlen] = 0;

    hc = WinHttpConnect(hs, host, u.nPort, 0);
    if (!hc) { c->result = WAPI_ERR_IO; goto cleanup; }

    /* Path + query together as the URL-Path of the request. */
    WCHAR path[2048];
    DWORD pathlen = 0;
    if (u.dwUrlPathLength + u.dwExtraInfoLength + 1 < 2048) {
        memcpy(path + pathlen, u.lpszUrlPath, u.dwUrlPathLength * sizeof(WCHAR));
        pathlen += u.dwUrlPathLength;
        if (u.dwExtraInfoLength) {
            memcpy(path + pathlen, u.lpszExtraInfo, u.dwExtraInfoLength * sizeof(WCHAR));
            pathlen += u.dwExtraInfoLength;
        }
        path[pathlen] = 0;
    } else {
        path[0] = L'/'; path[1] = 0;
    }

    DWORD req_flags = (u.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    hr = WinHttpOpenRequest(hc, method_verb(c->flags), path, NULL,
                            WINHTTP_NO_REFERER,
                            WINHTTP_DEFAULT_ACCEPT_TYPES, req_flags);
    if (!hr) { c->result = WAPI_ERR_IO; goto cleanup; }

    if (!WinHttpSendRequest(hr, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        c->result = WAPI_ERR_IO;
        goto cleanup;
    }
    if (!WinHttpReceiveResponse(hr, NULL)) {
        c->result = WAPI_ERR_IO;
        goto cleanup;
    }

    /* Pull the status code into the CQE flags field. */
    DWORD status = 0, sz = sizeof(status);
    WinHttpQueryHeaders(hr, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz,
                        WINHTTP_NO_HEADER_INDEX);
    c->cqe_flags = status;

    /* Drain body into the caller's buffer. */
    uint8_t*  dst      = (uint8_t*)out;
    uint32_t  cap      = (uint32_t)c->len2;
    uint32_t  written  = 0;
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hr, &avail)) { c->result = WAPI_ERR_IO; goto cleanup; }
        if (avail == 0) break;
        if (written + avail > cap) {
            /* Drain whatever remains to keep the socket sane, then
             * report overflow. */
            uint8_t scratch[4096];
            while (avail > 0) {
                DWORD n = avail > sizeof(scratch) ? (DWORD)sizeof(scratch) : avail;
                DWORD got = 0;
                if (!WinHttpReadData(hr, scratch, n, &got) || got == 0) break;
                avail -= got;
            }
            c->result = WAPI_ERR_OVERFLOW;
            goto cleanup;
        }
        DWORD got = 0;
        if (!WinHttpReadData(hr, dst + written, avail, &got)) {
            c->result = WAPI_ERR_IO; goto cleanup;
        }
        if (got == 0) break;
        written += got;
    }
    c->result = (int32_t)written;

cleanup:
    if (hr) WinHttpCloseHandle(hr);
    if (hc) WinHttpCloseHandle(hc);
    if (hs) WinHttpCloseHandle(hs);
    free(wurl);
}
