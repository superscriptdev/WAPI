/**
 * WAPI Desktop Runtime - Win32 ROUTED transfer (system share sheet)
 *
 * Windows 10+ exposes a "share" shell verb that invokes the system
 * share picker — the same UI a user sees from Explorer's "Share"
 * context-menu entry, backed by DataTransferManager. Programmatic
 * invocation is `ShellExecuteExW(lpVerb=L"share", lpFile=<path>)`.
 *
 * The verb operates on files, so for text/image MIMEs we materialize
 * a temp file in %TEMP% with an appropriate extension and hand that
 * to the share sheet. For text/uri-list we parse the first referenced
 * file URI and share it directly.
 *
 * Temp files are left behind — the share target may read them
 * asynchronously after ShellExecuteEx returns. Windows' tempdir
 * cleanup disposes of them eventually.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "wapi_plat.h"

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdint.h>
#include <string.h>

#pragma comment(lib, "shell32")
#pragma comment(lib, "shlwapi")

/* HWND helper — the window backend owns this. */
static HWND hwnd_of_share(wapi_plat_window_t* w) {
    if (!w) return NULL;
    wapi_plat_native_handle_t n;
    if (!wapi_plat_window_get_native(w, &n)) return NULL;
    if (n.kind != WAPI_PLAT_NATIVE_WIN32) return NULL;
    return (HWND)n.a;
}

/* Write `data` to a fresh temp file with the given wide extension
 * (including the leading dot, e.g. L".txt"). Returns the path in
 * `out_path` or fails. */
static bool materialize_temp_file(const void* data, size_t len,
                                  const WCHAR* dot_ext,
                                  WCHAR* out_path, size_t out_cap)
{
    WCHAR tempdir[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, tempdir);
    if (n == 0 || n >= MAX_PATH) return false;

    /* GetTempFileNameW creates a unique name with .tmp extension; we
     * want a typed extension so Windows picks the right share handler.
     * Use a random suffix we compose ourselves. */
    LARGE_INTEGER qpc; QueryPerformanceCounter(&qpc);
    int written = _snwprintf_s(out_path, out_cap, _TRUNCATE,
                               L"%swapi-share-%llx%s",
                               tempdir, (unsigned long long)qpc.QuadPart, dot_ext);
    if (written <= 0) return false;

    HANDLE h = CreateFileW(out_path, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    BOOL ok = (len == 0) ? TRUE : WriteFile(h, data, (DWORD)len, &wrote, NULL);
    CloseHandle(h);
    return ok && (len == 0 || wrote == len);
}

/* Decode a percent-encoded file:// URI into a Win32 wide path.
 * Returns bytes written into out (including NUL). Handles only
 * "file:///C:/..." form — the one wapi_plat's uri-list emits. */
static bool file_uri_to_path(const char* uri, size_t len, WCHAR* out, size_t out_cap) {
    static const char PREFIX[] = "file:///";
    size_t p = sizeof(PREFIX) - 1;
    if (len < p || memcmp(uri, PREFIX, p) != 0) return false;

    /* Percent-decode into a UTF-8 scratch buffer. */
    char scratch[1024];
    size_t w = 0;
    for (size_t i = p; i < len && w < sizeof(scratch) - 1; i++) {
        char c = uri[i];
        if (c == '%' && i + 2 < len) {
            int hi = -1, lo = -1;
            char a = uri[i + 1], b = uri[i + 2];
            if      (a >= '0' && a <= '9') hi = a - '0';
            else if (a >= 'A' && a <= 'F') hi = a - 'A' + 10;
            else if (a >= 'a' && a <= 'f') hi = a - 'a' + 10;
            if      (b >= '0' && b <= '9') lo = b - '0';
            else if (b >= 'A' && b <= 'F') lo = b - 'A' + 10;
            else if (b >= 'a' && b <= 'f') lo = b - 'a' + 10;
            if (hi >= 0 && lo >= 0) {
                scratch[w++] = (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        scratch[w++] = (c == '/') ? '\\' : c;
    }
    scratch[w] = 0;

    int wn = MultiByteToWideChar(CP_UTF8, 0, scratch, (int)w,
                                 out, (int)out_cap - 1);
    if (wn <= 0) return false;
    out[wn] = 0;
    return true;
}

bool wapi_plat_share_data(wapi_plat_window_t* parent,
                          const char* mime, size_t mime_len,
                          const void* data, size_t data_len,
                          const char* title, size_t title_len)
{
    (void)title; (void)title_len; /* share verb doesn't take a title */
    if (!mime || mime_len == 0) return false;

    WCHAR path[MAX_PATH * 2];
    path[0] = 0;

    if (mime_len == 10 && memcmp(mime, "text/plain", 10) == 0) {
        if (!materialize_temp_file(data, data_len, L".txt",
                                   path, sizeof(path)/sizeof(WCHAR)))
            return false;
    } else if (mime_len == 9 && memcmp(mime, "image/bmp", 9) == 0) {
        if (!materialize_temp_file(data, data_len, L".bmp",
                                   path, sizeof(path)/sizeof(WCHAR)))
            return false;
    } else if (mime_len == 13 && memcmp(mime, "text/uri-list", 13) == 0) {
        /* Share the first file referenced by the URI list. A multi-
         * file share would need IShellItemArray, left for a future
         * upgrade. */
        const char* p = (const char*)data;
        size_t end = 0;
        while (end < data_len && p[end] != '\r' && p[end] != '\n') end++;
        if (end == 0) return false;
        if (!file_uri_to_path(p, end, path, sizeof(path)/sizeof(WCHAR)))
            return false;
    } else {
        return false;
    }

    SHELLEXECUTEINFOW sei; memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask  = SEE_MASK_DEFAULT;
    sei.hwnd   = hwnd_of_share(parent);
    sei.lpVerb = L"share";
    sei.lpFile = path;
    sei.nShow  = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) ? true : false;
}
