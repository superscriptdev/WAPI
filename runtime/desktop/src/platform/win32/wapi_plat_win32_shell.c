/**
 * WAPI Desktop Runtime - Win32 Taskbar + Balloon Notifications
 *
 * ITaskbarList3 for progress / overlay icons / attention-flash and
 * Shell_NotifyIconW balloons for notifications.
 *
 * The balloon path piggybacks on a shared hidden notify-icon created
 * on first use: one icon for the whole process, with balloon text
 * swapped per notification. On click we emit
 * WAPI_PLAT_EV_TRAY_CLICK carrying a synthetic token so guests can
 * distinguish notification clicks from tray-icon clicks if they wire
 * both.
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
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "wapi_plat.h"

#define COBJMACROS
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <objbase.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ole32")
#pragma comment(lib, "shell32")

/* Forward decls (live in wapi_plat_win32.c). */
void     wapi_plat_win32_push_event(const wapi_plat_event_t* ev);
uint64_t wapi_plat_win32_now_ns(void);

/* --- Local IIDs --- */
static const IID   WAPI_IID_ITaskbarList3  = { 0xEA1AFB91, 0x9E28, 0x4B86, { 0x90, 0xE9, 0x9E, 0x9F, 0x8A, 0x5E, 0xEF, 0xAF } };
static const CLSID WAPI_CLSID_TaskbarList  = { 0x56FDF344, 0xFD6D, 0x11D0, { 0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90 } };

/* TBPF_* progress states — not always in shobjidl.h with UNICODE flags. */
#define WAPI_TBPF_NOPROGRESS    0x0
#define WAPI_TBPF_INDETERMINATE 0x1
#define WAPI_TBPF_NORMAL        0x2
#define WAPI_TBPF_ERROR         0x4
#define WAPI_TBPF_PAUSED        0x8

static bool ensure_com(void) {
    static DWORD tls = TLS_OUT_OF_INDEXES;
    if (tls == TLS_OUT_OF_INDEXES) tls = TlsAlloc();
    if (TlsGetValue(tls)) return true;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE) hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr) || hr == S_FALSE) { TlsSetValue(tls, (void*)1); return true; }
    return false;
}

static ITaskbarList3* g_taskbar;
static bool g_taskbar_tried;

static ITaskbarList3* taskbar_ensure(void) {
    if (g_taskbar) return g_taskbar;
    if (g_taskbar_tried) return NULL;
    g_taskbar_tried = true;
    if (!ensure_com()) return NULL;
    HRESULT hr = CoCreateInstance(&WAPI_CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER,
                                  &WAPI_IID_ITaskbarList3, (void**)&g_taskbar);
    if (FAILED(hr) || !g_taskbar) { g_taskbar = NULL; return NULL; }
    if (FAILED(ITaskbarList3_HrInit(g_taskbar))) {
        ITaskbarList3_Release(g_taskbar);
        g_taskbar = NULL;
    }
    return g_taskbar;
}

static HWND hwnd_of_plat(wapi_plat_window_t* w) {
    wapi_plat_native_handle_t n;
    if (!wapi_plat_window_get_native(w, &n)) return NULL;
    if (n.kind != WAPI_PLAT_NATIVE_WIN32) return NULL;
    return (HWND)n.a;
}

/* ============================================================
 * Taskbar progress / overlay / attention
 * ============================================================ */

bool wapi_plat_taskbar_set_progress(wapi_plat_window_t* w, int state, float value) {
    ITaskbarList3* tb = taskbar_ensure();
    HWND hwnd = hwnd_of_plat(w);
    if (!tb || !hwnd) return false;

    int flag;
    switch (state) {
    case 0: flag = WAPI_TBPF_NOPROGRESS;    break;
    case 1: flag = WAPI_TBPF_INDETERMINATE; break;
    case 2: flag = WAPI_TBPF_NORMAL;        break;
    case 3: flag = WAPI_TBPF_ERROR;         break;
    case 4: flag = WAPI_TBPF_PAUSED;        break;
    default: flag = WAPI_TBPF_NOPROGRESS;   break;
    }
    HRESULT hr1 = ITaskbarList3_SetProgressState(tb, hwnd, flag);
    HRESULT hr2 = S_OK;
    if (state == 2 || state == 3 || state == 4) {
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;
        hr2 = ITaskbarList3_SetProgressValue(tb, hwnd, (ULONGLONG)(value * 1000.0f), 1000);
    }
    return SUCCEEDED(hr1) && SUCCEEDED(hr2);
}

bool wapi_plat_taskbar_request_attention(wapi_plat_window_t* w, bool critical) {
    HWND hwnd = hwnd_of_plat(w);
    if (!hwnd) return false;
    FLASHWINFO fi = {0};
    fi.cbSize    = sizeof(fi);
    fi.hwnd      = hwnd;
    fi.dwFlags   = FLASHW_ALL | (critical ? FLASHW_TIMERNOFG : FLASHW_TIMER);
    fi.uCount    = critical ? (UINT)-1 : 3;
    fi.dwTimeout = 0;
    FlashWindowEx(&fi);
    return true;
}

/* Build an HICON from ICO bytes; PNG -> default on failure. */
static HICON icon_from_bytes(const void* bytes, size_t len) {
    if (bytes && len >= 22) {
        HICON h = CreateIconFromResourceEx((PBYTE)bytes, (DWORD)len, TRUE,
                                           0x00030000, 0, 0, LR_DEFAULTCOLOR);
        if (h) return h;
    }
    return NULL;
}

bool wapi_plat_taskbar_set_overlay(wapi_plat_window_t* w,
                                   const void* icon, size_t icon_len,
                                   const char* desc, size_t desc_len)
{
    ITaskbarList3* tb = taskbar_ensure();
    HWND hwnd = hwnd_of_plat(w);
    if (!tb || !hwnd) return false;

    HICON hi = icon_from_bytes(icon, icon_len);
    WCHAR wdesc[256];
    wdesc[0] = 0;
    if (desc && desc_len > 0) {
        int n = MultiByteToWideChar(CP_UTF8, 0, desc, (int)desc_len,
                                    wdesc, (int)(sizeof(wdesc)/sizeof(WCHAR)) - 1);
        if (n > 0) wdesc[n] = 0;
    }
    HRESULT hr = ITaskbarList3_SetOverlayIcon(tb, hwnd, hi, wdesc);
    /* SetOverlayIcon copies the icon; safe to release ours. */
    if (hi) DestroyIcon(hi);
    return SUCCEEDED(hr);
}

bool wapi_plat_taskbar_clear_overlay(wapi_plat_window_t* w) {
    ITaskbarList3* tb = taskbar_ensure();
    HWND hwnd = hwnd_of_plat(w);
    if (!tb || !hwnd) return false;
    HRESULT hr = ITaskbarList3_SetOverlayIcon(tb, hwnd, NULL, L"");
    return SUCCEEDED(hr);
}

/* ============================================================
 * Balloon notifications
 * ============================================================ */

#define WM_WAPI_NOTIFY  (WM_APP + 2)
#define NOTIFY_TOKEN    0x7FFF  /* dedicated tray-range token for notify-click events */

static ATOM     g_notify_class;
static HWND     g_notify_hwnd;
static UINT     g_notify_uid = 0x5FFF;
static bool     g_notify_installed;
/* Mapping from notification id -> guest handle id (= notification id).
 * Notifications close when the balloon expires or is clicked; guests
 * can also call wapi_notify_close(id). We keep it simple: one active
 * balloon at a time; subsequent shows replace. */
static uint32_t g_last_notify_id;

static LRESULT CALLBACK notify_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_WAPI_NOTIFY) {
        UINT ev = LOWORD(lp);
        if (ev == NIN_BALLOONUSERCLICK || ev == WM_LBUTTONUP) {
            wapi_plat_event_t e; memset(&e, 0, sizeof(e));
            e.type         = WAPI_PLAT_EV_TRAY_CLICK;
            e.timestamp_ns = wapi_plat_win32_now_ns();
            e.u.tray.tray_token = NOTIFY_TOKEN;
            e.u.tray.item_id    = g_last_notify_id;
            wapi_plat_win32_push_event(&e);
        }
        (void)wp;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool notify_ensure_window(void) {
    if (g_notify_hwnd) return true;
    HINSTANCE hi = GetModuleHandleW(NULL);
    if (!g_notify_class) {
        WNDCLASSEXW wcx = {0};
        wcx.cbSize        = sizeof(wcx);
        wcx.lpfnWndProc   = notify_wndproc;
        wcx.hInstance     = hi;
        wcx.lpszClassName = L"WAPI_NotifyMsg";
        g_notify_class    = RegisterClassExW(&wcx);
        if (!g_notify_class) return false;
    }
    g_notify_hwnd = CreateWindowExW(0, L"WAPI_NotifyMsg", L"", 0, 0, 0, 0, 0,
                                    HWND_MESSAGE, NULL, hi, NULL);
    return g_notify_hwnd != NULL;
}

static bool notify_install_base(void) {
    if (g_notify_installed) return true;
    if (!notify_ensure_window()) return false;
    NOTIFYICONDATAW nid; memset(&nid, 0, sizeof(nid));
    nid.cbSize           = sizeof(nid);
    nid.hWnd             = g_notify_hwnd;
    nid.uID              = g_notify_uid;
    nid.uFlags           = NIF_ICON | NIF_MESSAGE;
    nid.uCallbackMessage = WM_WAPI_NOTIFY;
    nid.hIcon            = LoadIconW(NULL, IDI_APPLICATION);
    if (!Shell_NotifyIconW(NIM_ADD, &nid)) return false;
    g_notify_installed = true;
    return true;
}

/* Show a balloon with title/body. Returns a non-zero notification id
 * on success. urgency maps to NIIF_{INFO,WARNING,ERROR}; body is
 * clamped to the NOTIFYICONDATA szInfo cap (256 wchars including
 * NUL); title clamped to 64 wchars. */
uint32_t wapi_plat_notify_show(const char* title, size_t title_len,
                               const char* body,  size_t body_len,
                               int urgency)
{
    if (!notify_install_base()) return 0;
    NOTIFYICONDATAW nid; memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd   = g_notify_hwnd;
    nid.uID    = g_notify_uid;
    nid.uFlags = NIF_INFO;

    if (title && title_len > 0) {
        int n = MultiByteToWideChar(CP_UTF8, 0, title, (int)title_len,
                                    nid.szInfoTitle,
                                    (int)(sizeof(nid.szInfoTitle)/sizeof(WCHAR)) - 1);
        if (n > 0) nid.szInfoTitle[n] = 0;
    }
    if (body && body_len > 0) {
        int n = MultiByteToWideChar(CP_UTF8, 0, body, (int)body_len,
                                    nid.szInfo,
                                    (int)(sizeof(nid.szInfo)/sizeof(WCHAR)) - 1);
        if (n > 0) nid.szInfo[n] = 0;
    }
    switch (urgency) {
    case 0: nid.dwInfoFlags = NIIF_INFO;    break;
    case 1: nid.dwInfoFlags = NIIF_INFO;    break;
    case 2: nid.dwInfoFlags = NIIF_WARNING; break;
    default: nid.dwInfoFlags = NIIF_INFO;   break;
    }
    if (!Shell_NotifyIconW(NIM_MODIFY, &nid)) return 0;

    /* Mint a non-zero id; wraps past 1 if it ever overflows. */
    g_last_notify_id = g_last_notify_id + 1;
    if (g_last_notify_id == 0) g_last_notify_id = 1;
    return g_last_notify_id;
}

bool wapi_plat_notify_close(uint32_t id) {
    /* Single-balloon implementation — "close" clears the balloon
     * regardless of id, matching the Shell_NotifyIcon model. */
    (void)id;
    if (!g_notify_installed) return true;
    NOTIFYICONDATAW nid; memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd   = g_notify_hwnd;
    nid.uID    = g_notify_uid;
    nid.uFlags = NIF_INFO;
    /* Empty strings dismiss the current balloon. */
    return Shell_NotifyIconW(NIM_MODIFY, &nid) ? true : false;
}
