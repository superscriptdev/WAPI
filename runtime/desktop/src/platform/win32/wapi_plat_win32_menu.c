/**
 * WAPI Desktop Runtime - Win32 Menu + Tray Backend
 *
 * Native HMENU for menubars / popups, Shell_NotifyIconW for tray.
 *
 * WM_COMMAND routing
 * ------------------
 * Each menu has a 16-bit "token" assigned by the host layer. Menu
 * items are registered with the Win32 item id = (token << 16) | slot,
 * where `slot` is an index into a per-menu item table keyed 1..N.
 * When WM_COMMAND arrives, we split the id and look up the slot to
 * recover the guest-supplied `item_id`. Menubars forward WM_COMMAND
 * through the owning window's WndProc; popup context menus receive
 * WM_COMMAND posted to the window that hosted TrackPopupMenu. Tray
 * items reuse the same encoding, but the token range is separate so
 * the dispatcher can disambiguate into menu-vs-tray events.
 *
 * Tokens
 * ------
 *   MENU:  1..0x3FFF      (menu popup + menubar)
 *   TRAY:  0x4000..0x7FFF (tray context menus; picked up via NIN_SELECT
 *                          + context-menu WM_COMMAND on the hidden
 *                          message-only window)
 *
 * Slots are 1-based; slot 0 is reserved so a zero Win32 id means
 * "no command" on Cancel/Escape.
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

#include <windows.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "shell32")

/* Forward decls from sibling TU. */
void      wapi_plat_win32_push_event(const wapi_plat_event_t* ev);
uint64_t  wapi_plat_win32_now_ns(void);
/* Exposed for window WndProc so native WM_COMMAND can route through
 * the menu dispatcher. Safe no-op when nothing matches. */
bool      wapi_plat_win32_menu_dispatch_command(uint32_t id);

/* Hidden message-only window class we create lazily for tray icons. */
static ATOM s_tray_msg_class;
static HINSTANCE s_hinst;

/* ============================================================
 * Menu storage
 * ============================================================ */

#define MENU_MAX          128
#define MENU_ITEMS_MAX    128

typedef struct menu_item_t {
    uint32_t guest_id;   /* id the guest registered */
    uint32_t flags;      /* WAPI_MENU_* */
} menu_item_t;

struct wapi_plat_menu_t {
    uint32_t   token;
    HMENU      hmenu;
    bool       popup;        /* TrackPopupMenu-compatible vs mbar */
    menu_item_t items[MENU_ITEMS_MAX];
    int        item_count;   /* 1-based slot is items[slot-1] */
};

/* Registry of live menus keyed by token for WM_COMMAND → guest_id. */
static struct wapi_plat_menu_t* s_menus[MENU_MAX];

/* ============================================================
 * Tray storage
 * ============================================================ */

#define TRAY_MAX        16
#define TRAY_TOKEN_BASE 0x4000

#define WM_WAPI_TRAY_NOTIFY  (WM_APP + 1)

struct wapi_plat_tray_t {
    uint32_t         token;           /* 0x4000..0x7FFF */
    UINT             uid;             /* NOTIFYICONDATA id   */
    HICON            icon;            /* owned; DestroyIcon on replace/destroy */
    HWND             msg_hwnd;        /* message-only window */
    wapi_plat_menu_t* menu;           /* context menu (not owned)   */
    bool             installed;
};

static struct wapi_plat_tray_t* s_trays[TRAY_MAX];

/* ============================================================
 * Helpers
 * ============================================================ */

static WCHAR* utf8_to_wide(const char* s, size_t len) {
    if (!s || len == 0) { WCHAR* w = (WCHAR*)malloc(sizeof(WCHAR)); if (w) w[0] = 0; return w; }
    int n = MultiByteToWideChar(CP_UTF8, 0, s, (int)len, NULL, 0);
    if (n <= 0) return NULL;
    WCHAR* w = (WCHAR*)malloc(sizeof(WCHAR) * (size_t)(n + 1));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, s, (int)len, w, n);
    w[n] = 0;
    return w;
}

static uint32_t pack_id(uint16_t token, uint16_t slot) {
    return ((uint32_t)token << 16) | (uint32_t)slot;
}

static void unpack_id(uint32_t id, uint16_t* out_token, uint16_t* out_slot) {
    *out_token = (uint16_t)(id >> 16);
    *out_slot  = (uint16_t)(id & 0xFFFF);
}

static struct wapi_plat_menu_t* menu_from_token(uint16_t token) {
    if (token == 0 || token >= MENU_MAX) {
        /* Tray-range tokens are stored on tray->menu; look those up
         * via the tray registry. */
        if (token >= TRAY_TOKEN_BASE) {
            for (int i = 0; i < TRAY_MAX; i++) {
                if (s_trays[i] && s_trays[i]->menu &&
                    s_trays[i]->token == token) {
                    return s_trays[i]->menu;
                }
            }
        }
        return NULL;
    }
    return s_menus[token];
}

/* ============================================================
 * Menu API
 * ============================================================ */

wapi_plat_menu_t* wapi_plat_menu_create(uint32_t token) {
    if (token == 0) return NULL;
    /* Menu-range tokens index s_menus[]; tray-range tokens are looked
     * up via the tray registry in menu_from_token. */
    bool in_menu_range = (token < MENU_MAX);
    if (in_menu_range && s_menus[token]) return NULL;

    struct wapi_plat_menu_t* m = (struct wapi_plat_menu_t*)calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->token = token;
    m->hmenu = CreatePopupMenu(); /* works for bar too via AppendMenu */
    m->popup = true;
    if (!m->hmenu) { free(m); return NULL; }
    if (in_menu_range) s_menus[token] = m;
    return m;
}

void wapi_plat_menu_destroy(wapi_plat_menu_t* m) {
    if (!m) return;
    if (m->token < MENU_MAX && s_menus[m->token] == m) s_menus[m->token] = NULL;
    if (m->hmenu) DestroyMenu(m->hmenu);
    free(m);
}

bool wapi_plat_menu_add_item(wapi_plat_menu_t* m, uint32_t guest_id,
                             const char* utf8, size_t label_len, uint32_t flags)
{
    if (!m || m->item_count >= MENU_ITEMS_MAX) return false;

    uint16_t slot = (uint16_t)(m->item_count + 1);
    m->items[m->item_count].guest_id = guest_id;
    m->items[m->item_count].flags    = flags;
    m->item_count++;

    UINT mf = MF_STRING;
    if (flags & 0x1 /* WAPI_MENU_SEPARATOR */) mf = MF_SEPARATOR;
    if (flags & 0x2 /* WAPI_MENU_DISABLED  */) mf |= MF_GRAYED;
    if (flags & 0x4 /* WAPI_MENU_CHECKED   */) mf |= MF_CHECKED;

    if (mf & MF_SEPARATOR) {
        return AppendMenuW(m->hmenu, MF_SEPARATOR, 0, NULL) ? true : false;
    }

    WCHAR* w = utf8_to_wide(utf8, label_len);
    if (!w) return false;
    uint32_t id = pack_id((uint16_t)m->token, slot);
    BOOL ok = AppendMenuW(m->hmenu, mf, id, w);
    free(w);
    return ok ? true : false;
}

bool wapi_plat_menu_add_submenu(wapi_plat_menu_t* m,
                                const char* utf8, size_t label_len,
                                wapi_plat_menu_t* submenu)
{
    if (!m || !submenu) return false;
    WCHAR* w = utf8_to_wide(utf8, label_len);
    if (!w) return false;
    /* AppendMenu with MF_POPUP takes the sub-HMENU as uIDNewItem. */
    BOOL ok = AppendMenuW(m->hmenu, MF_STRING | MF_POPUP,
                          (UINT_PTR)submenu->hmenu, w);
    free(w);
    return ok ? true : false;
}

/* Resolve HWND from the opaque window; the window TU exposes
 * wapi_plat_window_get_native and we ask for WIN32 HWND. */
static HWND hwnd_of(wapi_plat_window_t* w) {
    wapi_plat_native_handle_t n;
    if (!wapi_plat_window_get_native(w, &n)) return NULL;
    if (n.kind != WAPI_PLAT_NATIVE_WIN32) return NULL;
    return (HWND)n.a;
}

bool wapi_plat_menu_show_context(wapi_plat_menu_t* m, wapi_plat_window_t* w,
                                 int32_t x, int32_t y)
{
    if (!m || !w) return false;
    HWND h = hwnd_of(w);
    if (!h) return false;
    POINT p = { x, y };
    ClientToScreen(h, &p);
    SetForegroundWindow(h); /* required for TrackPopupMenu to work right */
    /* TPM_RETURNCMD would swallow the click and hand us the id; we
     * instead let WM_COMMAND fire so the dispatch path is uniform. */
    BOOL ok = TrackPopupMenu(m->hmenu, TPM_LEFTALIGN | TPM_TOPALIGN,
                             p.x, p.y, 0, h, NULL);
    return ok ? true : false;
}

bool wapi_plat_menu_set_bar(wapi_plat_window_t* w, wapi_plat_menu_t* m) {
    HWND h = hwnd_of(w);
    if (!h) return false;
    HMENU cur = GetMenu(h);
    if (!m) {
        SetMenu(h, NULL);
        if (cur) DrawMenuBar(h);
        return true;
    }
    /* A popup HMENU works fine as a menubar; AppendMenu with MF_POPUP
     * children gives the usual "File | Edit | ..." bar. */
    SetMenu(h, m->hmenu);
    DrawMenuBar(h);
    m->popup = false;
    return true;
}

/* ============================================================
 * Tray — message-only window + Shell_NotifyIconW
 * ============================================================ */

static LRESULT CALLBACK tray_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_WAPI_TRAY_NOTIFY) {
        /* wp = tray uid, lp = notification message (mouse event) */
        UINT uid = (UINT)wp;
        UINT ev  = LOWORD(lp);
        /* Find tray by uid. */
        struct wapi_plat_tray_t* t = NULL;
        for (int i = 0; i < TRAY_MAX; i++) {
            if (s_trays[i] && s_trays[i]->uid == uid) { t = s_trays[i]; break; }
        }
        if (!t) return 0;
        if (ev == WM_LBUTTONUP || ev == NIN_SELECT) {
            wapi_plat_event_t e; memset(&e, 0, sizeof(e));
            e.type         = WAPI_PLAT_EV_TRAY_CLICK;
            e.timestamp_ns = wapi_plat_win32_now_ns();
            e.u.tray.tray_token = t->token;
            e.u.tray.item_id    = 0;
            wapi_plat_win32_push_event(&e);
        } else if (ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU) {
            /* Pop the context menu at the cursor. TrackPopupMenu posts
             * WM_COMMAND back to this same hwnd; tray_dispatch_command
             * turns it into WAPI_PLAT_EV_TRAY_MENU. */
            if (t->menu) {
                POINT p; GetCursorPos(&p);
                SetForegroundWindow(hwnd);
                TrackPopupMenu(t->menu->hmenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN,
                               p.x, p.y, 0, hwnd, NULL);
                PostMessageW(hwnd, WM_NULL, 0, 0); /* dismiss per MS kb */
            }
        }
        return 0;
    }
    if (msg == WM_COMMAND) {
        uint32_t id = (uint32_t)LOWORD(wp);
        if (wapi_plat_win32_menu_dispatch_command(id)) return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HWND tray_msg_window_ensure(void) {
    if (!s_hinst) s_hinst = GetModuleHandleW(NULL);
    if (!s_tray_msg_class) {
        WNDCLASSEXW wcx = {0};
        wcx.cbSize        = sizeof(wcx);
        wcx.lpfnWndProc   = tray_wndproc;
        wcx.hInstance     = s_hinst;
        wcx.lpszClassName = L"WAPI_TrayMsg";
        s_tray_msg_class  = RegisterClassExW(&wcx);
        if (!s_tray_msg_class) return NULL;
    }
    HWND h = CreateWindowExW(0, L"WAPI_TrayMsg", L"", 0, 0, 0, 0, 0,
                             HWND_MESSAGE, NULL, s_hinst, NULL);
    return h;
}

static HICON icon_from_png_or_ico(const void* bytes, size_t len) {
    /* CreateIconFromResourceEx handles ICO resource data directly;
     * PNG requires WIC. We accept either: try ICO first, then fall
     * back to the default app icon on failure (caller always gets a
     * valid HICON so Shell_NotifyIcon has something to draw). */
    if (bytes && len >= 22) {
        HICON h = CreateIconFromResourceEx((PBYTE)bytes, (DWORD)len, TRUE,
                                           0x00030000, 0, 0, LR_DEFAULTCOLOR);
        if (h) return h;
    }
    return LoadIconW(NULL, IDI_APPLICATION);
}

wapi_plat_tray_t* wapi_plat_tray_create(uint32_t token,
                                        const void* icon_bytes, size_t icon_len,
                                        const char* tooltip, size_t tooltip_len)
{
    if (token < TRAY_TOKEN_BASE) return NULL;
    int slot = -1;
    for (int i = 0; i < TRAY_MAX; i++) if (!s_trays[i]) { slot = i; break; }
    if (slot < 0) return NULL;

    struct wapi_plat_tray_t* t = (struct wapi_plat_tray_t*)calloc(1, sizeof(*t));
    if (!t) return NULL;
    t->token    = token;
    t->uid      = (UINT)token;
    t->msg_hwnd = tray_msg_window_ensure();
    if (!t->msg_hwnd) { free(t); return NULL; }
    t->icon = icon_from_png_or_ico(icon_bytes, icon_len);

    NOTIFYICONDATAW nid; memset(&nid, 0, sizeof(nid));
    nid.cbSize           = sizeof(nid);
    nid.hWnd             = t->msg_hwnd;
    nid.uID              = t->uid;
    nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_WAPI_TRAY_NOTIFY;
    nid.hIcon            = t->icon;
    if (tooltip && tooltip_len > 0) {
        int n = MultiByteToWideChar(CP_UTF8, 0, tooltip, (int)tooltip_len,
                                    nid.szTip, (int)(sizeof(nid.szTip)/sizeof(WCHAR)) - 1);
        if (n > 0) nid.szTip[n] = 0;
    }

    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        DestroyWindow(t->msg_hwnd);
        if (t->icon) DestroyIcon(t->icon);
        free(t);
        return NULL;
    }
    t->installed = true;
    s_trays[slot] = t;
    return t;
}

void wapi_plat_tray_destroy(wapi_plat_tray_t* t) {
    if (!t) return;
    if (t->installed) {
        NOTIFYICONDATAW nid; memset(&nid, 0, sizeof(nid));
        nid.cbSize = sizeof(nid);
        nid.hWnd   = t->msg_hwnd;
        nid.uID    = t->uid;
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }
    for (int i = 0; i < TRAY_MAX; i++) if (s_trays[i] == t) s_trays[i] = NULL;
    if (t->msg_hwnd) DestroyWindow(t->msg_hwnd);
    if (t->icon)     DestroyIcon(t->icon);
    free(t);
}

bool wapi_plat_tray_set_icon(wapi_plat_tray_t* t, const void* bytes, size_t len) {
    if (!t || !t->installed) return false;
    HICON nh = icon_from_png_or_ico(bytes, len);
    NOTIFYICONDATAW nid; memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd   = t->msg_hwnd;
    nid.uID    = t->uid;
    nid.uFlags = NIF_ICON;
    nid.hIcon  = nh;
    BOOL ok = Shell_NotifyIconW(NIM_MODIFY, &nid);
    if (ok) {
        if (t->icon) DestroyIcon(t->icon);
        t->icon = nh;
    } else {
        if (nh) DestroyIcon(nh);
    }
    return ok ? true : false;
}

bool wapi_plat_tray_set_tooltip(wapi_plat_tray_t* t, const char* tip, size_t len) {
    if (!t || !t->installed) return false;
    NOTIFYICONDATAW nid; memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd   = t->msg_hwnd;
    nid.uID    = t->uid;
    nid.uFlags = NIF_TIP;
    if (tip && len > 0) {
        int n = MultiByteToWideChar(CP_UTF8, 0, tip, (int)len,
                                    nid.szTip, (int)(sizeof(nid.szTip)/sizeof(WCHAR)) - 1);
        if (n > 0) nid.szTip[n] = 0;
    }
    return Shell_NotifyIconW(NIM_MODIFY, &nid) ? true : false;
}

bool wapi_plat_tray_set_menu(wapi_plat_tray_t* t, wapi_plat_menu_t* m) {
    if (!t) return false;
    t->menu = m;
    return true;
}

/* ============================================================
 * WM_COMMAND dispatch (called from window WndProc + tray WndProc)
 * ============================================================ */

bool wapi_plat_win32_menu_dispatch_command(uint32_t id) {
    if (id == 0) return false;
    uint16_t token, slot;
    unpack_id(id, &token, &slot);
    struct wapi_plat_menu_t* m = menu_from_token(token);
    if (!m || slot == 0 || slot > (uint16_t)m->item_count) return false;
    uint32_t guest_id = m->items[slot - 1].guest_id;

    bool is_tray = (token >= TRAY_TOKEN_BASE);
    wapi_plat_event_t e; memset(&e, 0, sizeof(e));
    e.type         = is_tray ? WAPI_PLAT_EV_TRAY_MENU : WAPI_PLAT_EV_MENU_SELECT;
    e.timestamp_ns = wapi_plat_win32_now_ns();
    if (is_tray) {
        /* Find owning tray for the token. */
        uint32_t tray_token = 0;
        for (int i = 0; i < TRAY_MAX; i++) {
            if (s_trays[i] && s_trays[i]->menu == m) { tray_token = s_trays[i]->token; break; }
        }
        e.u.tray.tray_token = tray_token;
        e.u.tray.item_id    = guest_id;
    } else {
        e.u.menu.menu_token = token;
        e.u.menu.item_id    = guest_id;
    }
    wapi_plat_win32_push_event(&e);
    return true;
}
