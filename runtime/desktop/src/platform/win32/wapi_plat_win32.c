/**
 * WAPI Desktop Runtime - Win32 Platform Backend
 *
 * Implements wapi_plat.h using native Win32 only: no SDL, no
 * cross-platform framework. Uses user32 / gdi32 / ole32 / imm32 /
 * dwmapi / winmm / shell32.
 *
 * Threading: WndProc runs on the thread that owns the window
 * (the thread that called wapi_plat_init -> the main thread).
 * That thread has exclusive access to the event ring, so no
 * locks. wapi_plat_wake() sets a kernel event that
 * MsgWaitForMultipleObjectsEx watches alongside input queue.
 */

#include "wapi_plat.h"

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

#include <windows.h>
#include <windowsx.h>
#include <shellscalingapi.h>
#include <imm.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "imm32")
#pragma comment(lib, "dwmapi")
#pragma comment(lib, "shell32")
#pragma comment(lib, "winmm")

/* ============================================================
 * Storage
 * ============================================================ */

#define WAPI_WIN32_MAX_WINDOWS 16
#define WAPI_WIN32_EV_RING     256

/* Preedit side-store sized for typical CJK candidate strings. UTF-16 max
 * is 256 wchars (covers 5–8 clauses) which fits in 1 KiB UTF-8 worst case. */
#define WAPI_WIN32_PREEDIT_BYTES   1024
#define WAPI_WIN32_PREEDIT_SEGS    32

typedef struct preedit_seg_t {
    uint32_t start;   /* byte offset into preedit text */
    uint32_t length;  /* byte length */
    uint32_t flags;   /* WAPI_IME_SEG_* (matches wapi_input.h) */
} preedit_seg_t;

struct wapi_plat_window_t {
    HWND         hwnd;
    uint32_t     id;             /* stable, nonzero */
    uint32_t     flags;
    bool         wants_relative;
    bool         tracking_leave;
    bool         text_input_on;
    HIMC         saved_imc;      /* when text input is off */
    int32_t      last_width;
    int32_t      last_height;
    POINT        relative_anchor; /* client-center when relative mouse active */
    HCURSOR      cursor;         /* currently-set cursor */
    HCURSOR      custom_cursor;  /* owned by us, freed on destroy / replace */
    bool         cursor_hidden;

    /* IME preedit side-store */
    bool          ime_composing;
    bool          ime_had_result;    /* latched on GCS_RESULTSTR; clears CANCEL on EndComposition */
    char          preedit[WAPI_WIN32_PREEDIT_BYTES];
    uint32_t      preedit_len;       /* UTF-8 byte length */
    uint32_t      preedit_cursor;    /* caret byte offset */
    preedit_seg_t preedit_segs[WAPI_WIN32_PREEDIT_SEGS];
    uint32_t      preedit_seg_count;
    POINT         ime_candidate_pt;  /* client-space anchor */
    bool          ime_candidate_set;
};

typedef struct plat_state_t {
    /* Window pool (index 0 unused so id=0 means "none") */
    struct wapi_plat_window_t windows[WAPI_WIN32_MAX_WINDOWS];
    uint32_t                  next_window_id;

    /* Event ring */
    wapi_plat_event_t ev_ring[WAPI_WIN32_EV_RING];
    int               ev_head;
    int               ev_tail;
    int               ev_count;

    /* Wake */
    HANDLE  wake_event;

    /* Clock */
    LARGE_INTEGER qpc_freq;

    /* Keyboard state (indexed by USB HID scancode, 0..255) */
    uint8_t  key_state[256];
    uint16_t mod_state;

    /* Mouse (last known client-space position on the active window) */
    float    mouse_x, mouse_y;
    uint32_t mouse_buttons;

    /* Win32 window class */
    ATOM   wclass_atom;
    HINSTANCE hinst;

    /* Stashed system cursors (lazy) */
    HCURSOR cur_arrow;
    HCURSOR cur_hand;
    HCURSOR cur_ibeam;
    HCURSOR cur_cross;
    HCURSOR cur_sizeall;
    HCURSOR cur_sizens;
    HCURSOR cur_sizewe;
    HCURSOR cur_sizenwse;
    HCURSOR cur_sizenesw;
    HCURSOR cur_no;
    HCURSOR cur_wait;

    /* Text input state: the HWND whose WndProc should forward WM_CHAR. */
    HWND    text_input_hwnd;

    bool    initialized;
} plat_state_t;

static plat_state_t g;

/* ============================================================
 * Internal helpers
 * ============================================================ */

static uint64_t now_ns(void) {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    /* Convert c.QuadPart ticks at g.qpc_freq Hz to nanoseconds.
     * Avoid overflow by splitting into seconds + remainder. */
    uint64_t f = (uint64_t)g.qpc_freq.QuadPart;
    if (f == 0) return 0;
    uint64_t sec = (uint64_t)c.QuadPart / f;
    uint64_t rem = (uint64_t)c.QuadPart % f;
    return sec * 1000000000ULL + (rem * 1000000000ULL) / f;
}

static void ev_push(const wapi_plat_event_t* ev) {
    if (g.ev_count >= WAPI_WIN32_EV_RING) return; /* drop — oldest kept */
    g.ev_ring[g.ev_tail] = *ev;
    g.ev_tail = (g.ev_tail + 1) % WAPI_WIN32_EV_RING;
    g.ev_count++;
}

/* Exposed to sibling TUs in platform/win32 (gamepad, audio) for
 * event injection and timestamping. */
void wapi_plat_win32_push_event(const wapi_plat_event_t* ev) { ev_push(ev); }
uint64_t wapi_plat_win32_now_ns(void)                        { return now_ns(); }

/* Forward decl — implemented in wapi_plat_win32_gamepad.c. */
void wapi_plat_win32_gamepad_poll(void);

static bool ev_pop(wapi_plat_event_t* out) {
    if (g.ev_count == 0) return false;
    *out = g.ev_ring[g.ev_head];
    g.ev_head = (g.ev_head + 1) % WAPI_WIN32_EV_RING;
    g.ev_count--;
    return true;
}

static struct wapi_plat_window_t* win_from_hwnd(HWND hwnd) {
    for (int i = 1; i < WAPI_WIN32_MAX_WINDOWS; i++) {
        if (g.windows[i].hwnd == hwnd) return &g.windows[i];
    }
    return NULL;
}

static uint32_t id_from_hwnd(HWND hwnd) {
    struct wapi_plat_window_t* w = win_from_hwnd(hwnd);
    return w ? w->id : 0;
}

static struct wapi_plat_window_t* win_alloc(void) {
    for (int i = 1; i < WAPI_WIN32_MAX_WINDOWS; i++) {
        if (g.windows[i].hwnd == NULL && g.windows[i].id == 0) {
            memset(&g.windows[i], 0, sizeof(g.windows[i]));
            g.windows[i].id = ++g.next_window_id;
            if (g.windows[i].id == 0) g.windows[i].id = ++g.next_window_id;
            return &g.windows[i];
        }
    }
    return NULL;
}

static void win_free(struct wapi_plat_window_t* w) {
    memset(w, 0, sizeof(*w));
}

/* ============================================================
 * Scancode translation: Win32 PS/2 (set 1) -> USB HID usage
 * ============================================================
 * Win32 LPARAM bits 16..23 give the PS/2 scancode (set 1).
 * Bit 24 indicates the E0-extended prefix. Combined key index
 * is `(extended << 7) | scancode_low` (lower 7 bits of scan,
 * since top bit signals key release in PS/2, not relevant here).
 *
 * Output is HID Usage Page 0x07 (Keyboard/Keypad).
 * 0 = unmapped.
 */
static const uint8_t kWinPS2ToHid[256] = {
    /* Non-extended (bit 24 clear) 0x00..0x7F */
    [0x01]=0x29, /* Escape */
    [0x02]=0x1E, /* 1 */ [0x03]=0x1F, [0x04]=0x20, [0x05]=0x21, [0x06]=0x22,
    [0x07]=0x23, [0x08]=0x24, [0x09]=0x25, [0x0A]=0x26, [0x0B]=0x27, /* 0 */
    [0x0C]=0x2D, /* - */ [0x0D]=0x2E, /* = */
    [0x0E]=0x2A, /* Backspace */
    [0x0F]=0x2B, /* Tab */
    [0x10]=0x14, /* Q */ [0x11]=0x1A, [0x12]=0x08, [0x13]=0x15, [0x14]=0x17,
    [0x15]=0x1C, [0x16]=0x18, [0x17]=0x0C, [0x18]=0x12, [0x19]=0x13, /* P */
    [0x1A]=0x2F, /* [ */ [0x1B]=0x30, /* ] */
    [0x1C]=0x28, /* Enter */
    [0x1D]=0xE0, /* LCtrl */
    [0x1E]=0x04, /* A */ [0x1F]=0x16, [0x20]=0x07, [0x21]=0x09, [0x22]=0x0A,
    [0x23]=0x0B, [0x24]=0x0D, [0x25]=0x0E, [0x26]=0x0F, [0x27]=0x33, /* ; */
    [0x28]=0x34, /* ' */ [0x29]=0x35, /* ` */
    [0x2A]=0xE1, /* LShift */
    [0x2B]=0x31, /* \ */
    [0x2C]=0x1D, /* Z */ [0x2D]=0x1B, [0x2E]=0x06, [0x2F]=0x19, [0x30]=0x05,
    [0x31]=0x11, [0x32]=0x10, [0x33]=0x36, /* , */ [0x34]=0x37, /* . */
    [0x35]=0x38, /* / */
    [0x36]=0xE5, /* RShift */
    [0x37]=0x55, /* KP * */
    [0x38]=0xE2, /* LAlt */
    [0x39]=0x2C, /* Space */
    [0x3A]=0x39, /* CapsLock */
    [0x3B]=0x3A, /* F1 */  [0x3C]=0x3B, [0x3D]=0x3C, [0x3E]=0x3D, [0x3F]=0x3E,
    [0x40]=0x3F, [0x41]=0x40, [0x42]=0x41, [0x43]=0x42, [0x44]=0x43, /* F10 */
    [0x45]=0x53, /* NumLock */
    [0x46]=0x47, /* ScrollLock */
    [0x47]=0x5F, /* KP7 */ [0x48]=0x60, [0x49]=0x61, [0x4A]=0x56, /* KP- */
    [0x4B]=0x5C, /* KP4 */ [0x4C]=0x5D, [0x4D]=0x5E, [0x4E]=0x57, /* KP+ */
    [0x4F]=0x59, /* KP1 */ [0x50]=0x5A, [0x51]=0x5B,
    [0x52]=0x62, /* KP0 */ [0x53]=0x63, /* KP. */
    [0x56]=0x64, /* ISO extra backslash */
    [0x57]=0x44, /* F11 */ [0x58]=0x45, /* F12 */

    /* Extended (bit 24 set) mapped into upper half of table.
     * Index = 0x80 | (win32_sc & 0x7F). */
    [0x80|0x1C]=0x58, /* KP Enter */
    [0x80|0x1D]=0xE4, /* RCtrl */
    [0x80|0x35]=0x54, /* KP / */
    [0x80|0x37]=0x46, /* PrintScr */
    [0x80|0x38]=0xE6, /* RAlt */
    [0x80|0x47]=0x4A, /* Home */
    [0x80|0x48]=0x52, /* Up */
    [0x80|0x49]=0x4B, /* PageUp */
    [0x80|0x4B]=0x50, /* Left */
    [0x80|0x4D]=0x4F, /* Right */
    [0x80|0x4F]=0x4D, /* End */
    [0x80|0x50]=0x51, /* Down */
    [0x80|0x51]=0x4E, /* PageDown */
    [0x80|0x52]=0x49, /* Insert */
    [0x80|0x53]=0x4C, /* Delete */
    [0x80|0x5B]=0xE3, /* LGui */
    [0x80|0x5C]=0xE7, /* RGui */
    [0x80|0x5D]=0x65, /* Application/Menu */
};

/* Pause key is 0xE1 0x1D 0x45 sequence; VK_PAUSE comes through as a
 * non-extended scancode 0x45 with no bit set -> conflict with NumLock.
 * We special-case it on VK_PAUSE in the wndproc. */

static uint32_t win32_lparam_to_hid(LPARAM lParam, WPARAM vk) {
    uint32_t scan = (uint32_t)((lParam >> 16) & 0xFF);
    uint32_t ext  = (lParam & (1 << 24)) ? 1u : 0u;

    if (vk == VK_PAUSE) return 0x48; /* HID: Pause */

    uint32_t idx = (ext << 7) | (scan & 0x7F);
    return kWinPS2ToHid[idx];
}

/* ============================================================
 * Modifier state recompute
 * ============================================================ */

#define WAPI_MOD_LSHIFT 0x0001
#define WAPI_MOD_RSHIFT 0x0002
#define WAPI_MOD_LCTRL  0x0040
#define WAPI_MOD_RCTRL  0x0080
#define WAPI_MOD_LALT   0x0100
#define WAPI_MOD_RALT   0x0200
#define WAPI_MOD_LGUI   0x0400
#define WAPI_MOD_RGUI   0x0800
#define WAPI_MOD_NUM    0x1000
#define WAPI_MOD_CAPS   0x2000

static uint16_t recompute_mods(void) {
    uint16_t m = 0;
    if (GetKeyState(VK_LSHIFT)   & 0x8000) m |= WAPI_MOD_LSHIFT;
    if (GetKeyState(VK_RSHIFT)   & 0x8000) m |= WAPI_MOD_RSHIFT;
    if (GetKeyState(VK_LCONTROL) & 0x8000) m |= WAPI_MOD_LCTRL;
    if (GetKeyState(VK_RCONTROL) & 0x8000) m |= WAPI_MOD_RCTRL;
    if (GetKeyState(VK_LMENU)    & 0x8000) m |= WAPI_MOD_LALT;
    if (GetKeyState(VK_RMENU)    & 0x8000) m |= WAPI_MOD_RALT;
    if (GetKeyState(VK_LWIN)     & 0x8000) m |= WAPI_MOD_LGUI;
    if (GetKeyState(VK_RWIN)     & 0x8000) m |= WAPI_MOD_RGUI;
    if (GetKeyState(VK_NUMLOCK)  & 0x0001) m |= WAPI_MOD_NUM;
    if (GetKeyState(VK_CAPITAL)  & 0x0001) m |= WAPI_MOD_CAPS;
    return m;
}

/* ============================================================
 * IME composition helpers
 * ============================================================
 *
 * Win32 IMM exposes the active composition through three GCS_* slots
 * read out of the HIMC during WM_IME_COMPOSITION:
 *
 *   GCS_COMPSTR    UTF-16 preedit text (still being composed)
 *   GCS_COMPATTR   one ATTR_* byte per preedit wchar
 *   GCS_CURSORPOS  caret position, in wchars
 *   GCS_RESULTSTR  finalized text (delivered when composition completes)
 *
 * ATTR_* bytes are translated into the per-segment WAPI_IME_SEG_*
 * bitmask (the same flags wapi_input.h documents). Adjacent wchars
 * with the same attribute coalesce into one segment.
 */

/* Win32 IMM ATTR_* → WAPI_IME_SEG_*. Mirrors wapi_input.h definitions
 * (RAW=1, CONVERTED=2, SELECTED=4, TARGET=8). */
#define WAPI_PLAT_IME_SEG_RAW        0x01u
#define WAPI_PLAT_IME_SEG_CONVERTED  0x02u
#define WAPI_PLAT_IME_SEG_SELECTED   0x04u
#define WAPI_PLAT_IME_SEG_TARGET     0x08u

static uint32_t ime_attr_to_seg_flags(BYTE attr) {
    switch (attr) {
    case ATTR_INPUT:                 return WAPI_PLAT_IME_SEG_RAW;
    case ATTR_TARGET_CONVERTED:      return WAPI_PLAT_IME_SEG_CONVERTED | WAPI_PLAT_IME_SEG_TARGET;
    case ATTR_CONVERTED:             return WAPI_PLAT_IME_SEG_CONVERTED;
    case ATTR_TARGET_NOTCONVERTED:   return WAPI_PLAT_IME_SEG_RAW | WAPI_PLAT_IME_SEG_TARGET;
    case ATTR_INPUT_ERROR:           return WAPI_PLAT_IME_SEG_RAW;
    case ATTR_FIXEDCONVERTED:        return WAPI_PLAT_IME_SEG_CONVERTED;
    default:                         return WAPI_PLAT_IME_SEG_RAW;
    }
}

/* Read (text+attrs+cursor) from the HIMC and refresh the per-window
 * preedit side-store. Returns true on success. */
static bool ime_refresh_preedit(struct wapi_plat_window_t* w, HIMC himc) {
    if (!w || !himc) return false;

    LONG bytes16 = ImmGetCompositionStringW(himc, GCS_COMPSTR, NULL, 0);
    if (bytes16 < 0) bytes16 = 0;
    int wchars = (int)(bytes16 / (LONG)sizeof(WCHAR));
    if (wchars <= 0) {
        w->preedit_len = 0;
        w->preedit_cursor = 0;
        w->preedit_seg_count = 0;
        w->preedit[0] = 0;
        return true;
    }
    if (wchars > 512) wchars = 512;

    WCHAR wbuf[512];
    LONG read16 = ImmGetCompositionStringW(himc, GCS_COMPSTR,
                                           wbuf, (DWORD)(wchars * (int)sizeof(WCHAR)));
    if (read16 < 0) read16 = 0;
    int rwchars = (int)(read16 / (LONG)sizeof(WCHAR));
    if (rwchars > wchars) rwchars = wchars;

    /* Per-wchar attribute table */
    BYTE attrs[512];
    memset(attrs, ATTR_INPUT, sizeof(attrs));
    LONG nattr = ImmGetCompositionStringW(himc, GCS_COMPATTR,
                                          attrs, (DWORD)rwchars);
    if (nattr < 0) nattr = 0;
    if (nattr > rwchars) nattr = rwchars;

    /* Caret position in wchars */
    LONG cursor_w = ImmGetCompositionStringW(himc, GCS_CURSORPOS, NULL, 0);
    if (cursor_w < 0) cursor_w = 0;
    if (cursor_w > rwchars) cursor_w = rwchars;

    /* UTF-16 → UTF-8 */
    int u8_total = WideCharToMultiByte(CP_UTF8, 0, wbuf, rwchars, NULL, 0, NULL, NULL);
    if (u8_total < 0) u8_total = 0;
    if ((uint32_t)u8_total >= sizeof(w->preedit)) u8_total = (int)sizeof(w->preedit) - 1;
    int u8_written = WideCharToMultiByte(CP_UTF8, 0, wbuf, rwchars,
                                         w->preedit, u8_total, NULL, NULL);
    if (u8_written < 0) u8_written = 0;
    w->preedit[u8_written] = 0;
    w->preedit_len = (uint32_t)u8_written;

    /* Per-wchar UTF-8 byte advance (for caret + segment bounds) */
    uint32_t byte_offset_at_wchar[513];
    byte_offset_at_wchar[0] = 0;
    {
        uint32_t off = 0;
        for (int i = 0; i < rwchars; i++) {
            int n = WideCharToMultiByte(CP_UTF8, 0, &wbuf[i], 1, NULL, 0, NULL, NULL);
            if (n < 0) n = 0;
            off += (uint32_t)n;
            if (off > w->preedit_len) off = w->preedit_len;
            byte_offset_at_wchar[i + 1] = off;
        }
    }
    if (cursor_w >= 0 && cursor_w <= rwchars) {
        w->preedit_cursor = byte_offset_at_wchar[cursor_w];
    } else {
        w->preedit_cursor = w->preedit_len;
    }

    /* Coalesce attribute runs into segments */
    w->preedit_seg_count = 0;
    if (rwchars > 0) {
        BYTE cur_attr = attrs[0];
        int  run_start = 0;
        for (int i = 1; i <= rwchars; i++) {
            BYTE next = (i < rwchars) ? attrs[i] : (BYTE)~cur_attr; /* force flush at end */
            if (next != cur_attr) {
                if (w->preedit_seg_count < WAPI_WIN32_PREEDIT_SEGS) {
                    preedit_seg_t* seg = &w->preedit_segs[w->preedit_seg_count++];
                    seg->start  = byte_offset_at_wchar[run_start];
                    seg->length = byte_offset_at_wchar[i] - seg->start;
                    seg->flags  = ime_attr_to_seg_flags(cur_attr);
                }
                cur_attr  = (i < rwchars) ? attrs[i] : 0;
                run_start = i;
            }
        }
    }
    return true;
}

/* Drain GCS_RESULTSTR and queue it as a TEXT_INPUT event (the host's
 * existing IME side-store path then assigns the IME_COMMIT sequence).
 * Long commits are split across consecutive TEXT_INPUT events because
 * wapi_plat_ev_text_t has a 32-byte payload. */
static void ime_emit_result_string(struct wapi_plat_window_t* w, HIMC himc) {
    LONG bytes16 = ImmGetCompositionStringW(himc, GCS_RESULTSTR, NULL, 0);
    if (bytes16 <= 0) return;
    int wchars = (int)(bytes16 / (LONG)sizeof(WCHAR));
    if (wchars <= 0) return;
    if (wchars > 512) wchars = 512;

    WCHAR wbuf[512];
    LONG read16 = ImmGetCompositionStringW(himc, GCS_RESULTSTR,
                                           wbuf, (DWORD)(wchars * (int)sizeof(WCHAR)));
    if (read16 <= 0) return;
    int rwchars = (int)(read16 / (LONG)sizeof(WCHAR));
    if (rwchars > wchars) rwchars = wchars;

    char u8[2048];
    int u8_len = WideCharToMultiByte(CP_UTF8, 0, wbuf, rwchars,
                                     u8, (int)sizeof(u8), NULL, NULL);
    if (u8_len <= 0) return;

    /* Split into 31-byte chunks aligned on UTF-8 boundaries (back off
     * over continuation bytes 10xxxxxx so multi-byte chars stay intact). */
    int off = 0;
    while (off < u8_len) {
        int chunk = u8_len - off;
        if (chunk > 31) {
            chunk = 31;
            while (chunk > 0 && (u8[off + chunk] & 0xC0) == 0x80) chunk--;
            if (chunk <= 0) chunk = 31;
        }
        wapi_plat_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = WAPI_PLAT_EV_TEXT_INPUT;
        ev.window_id = w->id;
        ev.timestamp_ns = now_ns();
        memcpy(ev.u.text.text, u8 + off, (size_t)chunk);
        ev_push(&ev);
        off += chunk;
    }
}

/* ============================================================
 * WndProc
 * ============================================================ */

static void push_window_event(uint32_t type, HWND hwnd) {
    wapi_plat_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.window_id = id_from_hwnd(hwnd);
    ev.timestamp_ns = now_ns();
    ev_push(&ev);
}

static void emit_pointer_state_from_lparam(HWND hwnd, LPARAM lParam) {
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);
    g.mouse_x = (float)x;
    g.mouse_y = (float)y;
    (void)hwnd;
}

static LRESULT CALLBACK wapi_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    struct wapi_plat_window_t* w = win_from_hwnd(hwnd);

    switch (msg) {
    case WM_CLOSE:
        push_window_event(WAPI_PLAT_EV_WINDOW_CLOSE, hwnd);
        return 0;

    case WM_DESTROY:
        /* Window destruction handled by wapi_plat_window_destroy */
        return 0;

    case WM_SIZE: {
        int w_ = LOWORD(lp);
        int h_ = HIWORD(lp);
        if (w) { w->last_width = w_; w->last_height = h_; }
        wapi_plat_event_t ev;
        memset(&ev, 0, sizeof(ev));
        if (wp == SIZE_MINIMIZED)      ev.type = WAPI_PLAT_EV_WINDOW_MIN;
        else if (wp == SIZE_MAXIMIZED) ev.type = WAPI_PLAT_EV_WINDOW_MAX;
        else if (wp == SIZE_RESTORED)  ev.type = WAPI_PLAT_EV_WINDOW_RESTORE;
        else                           ev.type = WAPI_PLAT_EV_WINDOW_RESIZE;
        ev.window_id = id_from_hwnd(hwnd);
        ev.timestamp_ns = now_ns();
        ev.u.resize.w = w_;
        ev.u.resize.h = h_;
        ev_push(&ev);
        /* Always emit a RESIZE for render code too */
        if (ev.type != WAPI_PLAT_EV_WINDOW_RESIZE) {
            ev.type = WAPI_PLAT_EV_WINDOW_RESIZE;
            ev_push(&ev);
        }
        return 0;
    }

    case WM_MOVE: {
        wapi_plat_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = WAPI_PLAT_EV_WINDOW_MOVE;
        ev.window_id = id_from_hwnd(hwnd);
        ev.timestamp_ns = now_ns();
        ev.u.move.x = GET_X_LPARAM(lp);
        ev.u.move.y = GET_Y_LPARAM(lp);
        ev_push(&ev);
        return 0;
    }

    case WM_SETFOCUS:
        push_window_event(WAPI_PLAT_EV_WINDOW_FOCUS, hwnd);
        g.mod_state = recompute_mods();
        return 0;

    case WM_KILLFOCUS:
        push_window_event(WAPI_PLAT_EV_WINDOW_BLUR, hwnd);
        /* Clear held keys so we don't get stuck-down keys */
        memset(g.key_state, 0, sizeof(g.key_state));
        g.mouse_buttons = 0;
        return 0;

    case WM_SHOWWINDOW:
        push_window_event(wp ? WAPI_PLAT_EV_WINDOW_SHOW : WAPI_PLAT_EV_WINDOW_HIDE, hwnd);
        return 0;

    case WM_SETCURSOR:
        if (w && LOWORD(lp) == HTCLIENT) {
            if (w->cursor_hidden) {
                SetCursor(NULL);
            } else if (w->cursor) {
                SetCursor(w->cursor);
            } else {
                SetCursor(g.cur_arrow);
            }
            return TRUE;
        }
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        bool down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        bool repeat = down && (lp & (1 << 30));
        uint32_t hid = win32_lparam_to_hid(lp, wp);
        g.mod_state = recompute_mods();

        if (hid > 0 && hid < 256) {
            g.key_state[hid] = down ? 1 : 0;
        }

        wapi_plat_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = down ? WAPI_PLAT_EV_KEY_DOWN : WAPI_PLAT_EV_KEY_UP;
        ev.window_id = id_from_hwnd(hwnd);
        ev.timestamp_ns = now_ns();
        ev.u.key.scancode = hid;
        ev.u.key.keycode  = (uint32_t)wp;
        ev.u.key.mod      = g.mod_state;
        ev.u.key.down     = down ? 1 : 0;
        ev.u.key.repeat   = repeat ? 1 : 0;
        ev_push(&ev);

        /* Don't swallow WM_SYSKEYDOWN for Alt+F4 etc.; fall through */
        if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) break;
        return 0;
    }

    case WM_CHAR: {
        /* Accumulate UTF-16 surrogate pairs into UTF-8 in the text buf.
         * Single BMP char: emit immediately.  High surrogate: stash.
         * Low surrogate after high: combine + emit. */
        static WCHAR pending_high = 0;
        WCHAR ch = (WCHAR)wp;
        WCHAR utf16[2]; int utf16_len = 1;

        if (ch >= 0xD800 && ch <= 0xDBFF) {
            pending_high = ch;
            return 0;
        } else if (ch >= 0xDC00 && ch <= 0xDFFF) {
            if (pending_high) {
                utf16[0] = pending_high;
                utf16[1] = ch;
                utf16_len = 2;
                pending_high = 0;
            } else {
                return 0; /* stray low surrogate */
            }
        } else {
            utf16[0] = ch;
            pending_high = 0;
        }

        /* Control chars (Ctrl-A..Ctrl-Z come through as WM_CHAR 0x01..0x1A) —
         * filter, except tab / newline / backspace which may be useful. */
        if (utf16_len == 1 && utf16[0] < 0x20 &&
            utf16[0] != '\t' && utf16[0] != '\n' && utf16[0] != '\b') {
            return 0;
        }

        char utf8[32];
        int n = WideCharToMultiByte(CP_UTF8, 0, utf16, utf16_len,
                                    utf8, (int)sizeof(utf8) - 1, NULL, NULL);
        if (n <= 0) return 0;
        utf8[n] = '\0';

        wapi_plat_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = WAPI_PLAT_EV_TEXT_INPUT;
        ev.window_id = id_from_hwnd(hwnd);
        ev.timestamp_ns = now_ns();
        memcpy(ev.u.text.text, utf8, (size_t)n);
        ev_push(&ev);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!w) break;
        if (!w->tracking_leave) {
            TRACKMOUSEEVENT tme = {sizeof(tme)};
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            w->tracking_leave = true;
        }

        int x = GET_X_LPARAM(lp);
        int y = GET_Y_LPARAM(lp);
        float xrel = (float)x - g.mouse_x;
        float yrel = (float)y - g.mouse_y;
        g.mouse_x = (float)x;
        g.mouse_y = (float)y;

        wapi_plat_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = WAPI_PLAT_EV_MOUSE_MOTION;
        ev.window_id = w->id;
        ev.timestamp_ns = now_ns();
        ev.u.motion.mouse_id = 0;
        ev.u.motion.button_state = g.mouse_buttons;
        ev.u.motion.x = g.mouse_x;
        ev.u.motion.y = g.mouse_y;
        ev.u.motion.xrel = xrel;
        ev.u.motion.yrel = yrel;
        ev_push(&ev);

        /* Relative-mouse recenter */
        if (w->wants_relative) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            POINT c = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
            if (x != c.x || y != c.y) {
                ClientToScreen(hwnd, &c);
                SetCursorPos(c.x, c.y);
                /* Don't double-count the synthetic motion */
                g.mouse_x = (float)((rc.right - rc.left) / 2);
                g.mouse_y = (float)((rc.bottom - rc.top) / 2);
            }
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        if (w) w->tracking_leave = false;
        return 0;

    case WM_LBUTTONDOWN: case WM_LBUTTONUP:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: {
        bool down = (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN ||
                     msg == WM_MBUTTONDOWN || msg == WM_XBUTTONDOWN);
        uint8_t btn = 0;
        switch (msg) {
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: btn = 1; break; /* left */
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: btn = 2; break; /* middle */
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: btn = 3; break; /* right */
        case WM_XBUTTONDOWN: case WM_XBUTTONUP:
            btn = (GET_XBUTTON_WPARAM(wp) == XBUTTON1) ? 4 : 5; break;
        }

        if (down) {
            g.mouse_buttons |= (1u << btn);
            SetCapture(hwnd);
        } else {
            g.mouse_buttons &= ~(1u << btn);
            if (!g.mouse_buttons) ReleaseCapture();
        }

        int x = GET_X_LPARAM(lp);
        int y = GET_Y_LPARAM(lp);
        g.mouse_x = (float)x; g.mouse_y = (float)y;

        wapi_plat_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = down ? WAPI_PLAT_EV_MOUSE_DOWN : WAPI_PLAT_EV_MOUSE_UP;
        ev.window_id = id_from_hwnd(hwnd);
        ev.timestamp_ns = now_ns();
        ev.u.button.mouse_id = 0;
        ev.u.button.button = btn;
        ev.u.button.down = down ? 1 : 0;
        ev.u.button.clicks = 1; /* WM_LBUTTONDBLCLK would be 2 but is disabled */
        ev.u.button.x = g.mouse_x;
        ev.u.button.y = g.mouse_y;
        ev_push(&ev);

        /* XButton messages want TRUE return value */
        if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP) return TRUE;
        return 0;
    }

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL: {
        float delta = (float)GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA;
        wapi_plat_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = WAPI_PLAT_EV_MOUSE_WHEEL;
        ev.window_id = id_from_hwnd(hwnd);
        ev.timestamp_ns = now_ns();
        ev.u.wheel.mouse_id = 0;
        if (msg == WM_MOUSEWHEEL) { ev.u.wheel.x = 0;     ev.u.wheel.y = delta; }
        else                      { ev.u.wheel.x = delta; ev.u.wheel.y = 0;     }
        ev_push(&ev);
        return 0;
    }

    case WM_DPICHANGED: {
        /* Resize the window to the suggested rect */
        RECT* r = (RECT*)lp;
        SetWindowPos(hwnd, NULL, r->left, r->top,
                     r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_IME_STARTCOMPOSITION: {
        if (!w) break;
        w->ime_composing = true;
        w->ime_had_result = false;
        w->preedit_len = 0;
        w->preedit_cursor = 0;
        w->preedit_seg_count = 0;
        w->preedit[0] = 0;

        if (w->ime_candidate_set) {
            HIMC himc = ImmGetContext(hwnd);
            if (himc) {
                CANDIDATEFORM cf = {0};
                cf.dwIndex = 0;
                cf.dwStyle = CFS_EXCLUDE;
                cf.ptCurrentPos = w->ime_candidate_pt;
                ImmSetCandidateWindow(himc, &cf);
                ImmReleaseContext(hwnd, himc);
            }
        }
        push_window_event(WAPI_PLAT_EV_IME_START, hwnd);
        return 0;
    }

    case WM_IME_COMPOSITION: {
        if (!w) break;
        HIMC himc = ImmGetContext(hwnd);
        if (himc) {
            if (lp & GCS_RESULTSTR) {
                ime_emit_result_string(w, himc);
                w->ime_had_result = true;
            }
            if (lp & (GCS_COMPSTR | GCS_COMPATTR | GCS_CURSORPOS)) {
                ime_refresh_preedit(w, himc);
                push_window_event(WAPI_PLAT_EV_IME_UPDATE, hwnd);
            }
            ImmReleaseContext(hwnd, himc);
        }
        /* Suppress DefWindowProc so it does not regenerate WM_IME_CHAR /
         * WM_CHAR for GCS_RESULTSTR — we already emitted TEXT_INPUT. */
        return 0;
    }

    case WM_IME_ENDCOMPOSITION: {
        if (!w) break;
        bool was_composing = w->ime_composing;
        bool had_text      = w->preedit_len > 0;
        bool was_committed = w->ime_had_result;
        w->ime_composing = false;
        w->ime_had_result = false;
        w->preedit_len = 0;
        w->preedit_cursor = 0;
        w->preedit_seg_count = 0;
        w->preedit[0] = 0;
        /* CANCEL only when EndComposition arrives without a preceding
         * GCS_RESULTSTR but with nonempty preedit — this is the Escape /
         * focus-steal / CPS_CANCEL path. A normal commit already produced
         * TEXT_INPUT (→ IME_COMMIT) from the GCS_RESULTSTR handler. */
        if (was_composing && had_text && !was_committed) {
            push_window_event(WAPI_PLAT_EV_IME_CANCEL, hwnd);
        }
        return 0;
    }

    case WM_IME_SETCONTEXT:
        /* Hide the system candidate window when we own placement (we
         * already positioned it via ImmSetCandidateWindow). Mask off
         * ISC_SHOWUICANDIDATEWINDOW so the IME does not double-draw. */
        if (w && w->ime_candidate_set) {
            lp &= ~(LPARAM)(ISC_SHOWUICANDIDATEWINDOW |
                            (ISC_SHOWUICANDIDATEWINDOW << 1) |
                            (ISC_SHOWUICANDIDATEWINDOW << 2) |
                            (ISC_SHOWUICANDIDATEWINDOW << 3));
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ============================================================
 * Init / Shutdown
 * ============================================================ */

bool wapi_plat_init(void) {
    if (g.initialized) return true;

    /* Per-monitor DPI awareness V2 (Win10 1703+). Best-effort.
     * If that fails, fall back to per-monitor V1, then system DPI. */
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        typedef BOOL (WINAPI *fn_SetProcessDpiAwarenessContext)(void*);
        fn_SetProcessDpiAwarenessContext f =
            (fn_SetProcessDpiAwarenessContext)GetProcAddress(user32,
                "SetProcessDpiAwarenessContext");
        if (f) {
            #define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((void*)-4)
            f(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        } else {
            HMODULE shcore = LoadLibraryW(L"shcore.dll");
            if (shcore) {
                typedef HRESULT (WINAPI *fn_SetProcessDpiAwareness)(int);
                fn_SetProcessDpiAwareness g2 =
                    (fn_SetProcessDpiAwareness)GetProcAddress(shcore,
                        "SetProcessDpiAwareness");
                if (g2) g2(2 /* PROCESS_PER_MONITOR_DPI_AWARE */);
            } else {
                SetProcessDPIAware();
            }
        }
    }

    g.hinst = GetModuleHandleW(NULL);

    /* Register window class */
    WNDCLASSEXW wcx = {0};
    wcx.cbSize = sizeof(wcx);
    wcx.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcx.lpfnWndProc = wapi_wndproc;
    wcx.hInstance = g.hinst;
    wcx.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wcx.hCursor = NULL; /* we handle WM_SETCURSOR */
    wcx.hbrBackground = NULL;
    wcx.lpszClassName = L"WAPI_Window";
    g.wclass_atom = RegisterClassExW(&wcx);
    if (!g.wclass_atom) return false;

    /* Cache system cursors */
    g.cur_arrow     = LoadCursorW(NULL, IDC_ARROW);
    g.cur_hand      = LoadCursorW(NULL, IDC_HAND);
    g.cur_ibeam     = LoadCursorW(NULL, IDC_IBEAM);
    g.cur_cross     = LoadCursorW(NULL, IDC_CROSS);
    g.cur_sizeall   = LoadCursorW(NULL, IDC_SIZEALL);
    g.cur_sizens    = LoadCursorW(NULL, IDC_SIZENS);
    g.cur_sizewe    = LoadCursorW(NULL, IDC_SIZEWE);
    g.cur_sizenwse  = LoadCursorW(NULL, IDC_SIZENWSE);
    g.cur_sizenesw  = LoadCursorW(NULL, IDC_SIZENESW);
    g.cur_no        = LoadCursorW(NULL, IDC_NO);
    g.cur_wait      = LoadCursorW(NULL, IDC_WAIT);

    /* Clock */
    QueryPerformanceFrequency(&g.qpc_freq);

    /* Increase timer resolution to 1ms (for Sleep accuracy and
     * MsgWaitForMultipleObjectsEx timeout granularity). */
    timeBeginPeriod(1);

    /* Wake event */
    g.wake_event = CreateEventW(NULL, FALSE /* auto-reset */, FALSE, NULL);

    g.next_window_id = 0;
    g.initialized = true;
    return true;
}

void wapi_plat_shutdown(void) {
    if (!g.initialized) return;

    /* Destroy all windows */
    for (int i = 1; i < WAPI_WIN32_MAX_WINDOWS; i++) {
        if (g.windows[i].hwnd) {
            DestroyWindow(g.windows[i].hwnd);
            win_free(&g.windows[i]);
        }
    }

    if (g.wake_event) CloseHandle(g.wake_event);
    if (g.wclass_atom) UnregisterClassW(L"WAPI_Window", g.hinst);
    timeEndPeriod(1);

    memset(&g, 0, sizeof(g));
}

/* ============================================================
 * Clock
 * ============================================================ */

uint64_t wapi_plat_time_monotonic_ns(void) { return now_ns(); }

uint64_t wapi_plat_time_realtime_ns(void) {
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    uint64_t ticks = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    /* FILETIME epoch = 1601-01-01; convert to ns since 1970 */
    const uint64_t EPOCH_DIFF_100NS = 116444736000000000ULL;
    if (ticks <= EPOCH_DIFF_100NS) return 0;
    return (ticks - EPOCH_DIFF_100NS) * 100ULL;
}

uint64_t wapi_plat_perf_counter(void) {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (uint64_t)c.QuadPart;
}

uint64_t wapi_plat_perf_frequency(void) {
    return (uint64_t)g.qpc_freq.QuadPart;
}

uint64_t wapi_plat_time_resolution_monotonic_ns(void) {
    uint64_t f = (uint64_t)g.qpc_freq.QuadPart;
    if (f == 0) return 1;
    uint64_t r = 1000000000ULL / f;
    return r ? r : 1;
}

void wapi_plat_sleep_ns(uint64_t ns) {
    /* Sleep is millisecond-resolution. For sub-ms, spin the remainder
     * on QPC to avoid waking too early. */
    uint64_t ms = ns / 1000000ULL;
    if (ms > 0) Sleep((DWORD)(ms > 0xFFFFFFFFu ? 0xFFFFFFFFu : ms));

    uint64_t rem = ns - ms * 1000000ULL;
    if (rem == 0) return;
    uint64_t start = now_ns();
    while (now_ns() - start < rem) { YieldProcessor(); }
}

void wapi_plat_yield(void) { SwitchToThread(); }

/* ============================================================
 * Events
 * ============================================================ */

static void drain_messages(void) {
    /* Poll gamepads once per pump — XInput has no event stream. */
    wapi_plat_win32_gamepad_poll();

    MSG m;
    while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
        if (m.message == WM_QUIT) {
            wapi_plat_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = WAPI_PLAT_EV_QUIT;
            ev.timestamp_ns = now_ns();
            ev_push(&ev);
            continue;
        }
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

int wapi_plat_poll_events(wapi_plat_event_t* out, int max) {
    drain_messages();
    int n = 0;
    while (n < max && ev_pop(&out[n])) n++;
    return n;
}

void wapi_plat_wait_events(int64_t timeout_ns) {
    /* If events are already pending in the ring, don't block */
    if (g.ev_count > 0) return;

    DWORD timeout = INFINITE;
    if (timeout_ns >= 0) {
        uint64_t ms = (uint64_t)timeout_ns / 1000000ULL;
        if (ms > 0xFFFFFFFEu) ms = 0xFFFFFFFEu;
        timeout = (DWORD)ms;
    }

    HANDLE handles[1] = { g.wake_event };
    MsgWaitForMultipleObjectsEx(1, handles, timeout,
                                QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    drain_messages();
}

void wapi_plat_wake(void) {
    if (g.wake_event) SetEvent(g.wake_event);
}

/* ============================================================
 * State polls
 * ============================================================ */

bool wapi_plat_key_pressed(uint32_t scancode) {
    if (scancode >= 256) return false;
    return g.key_state[scancode] != 0;
}

uint16_t wapi_plat_mod_state(void) {
    return g.mod_state;
}

void wapi_plat_mouse_state(float* out_x, float* out_y, uint32_t* out_buttons) {
    if (out_x) *out_x = g.mouse_x;
    if (out_y) *out_y = g.mouse_y;
    if (out_buttons) *out_buttons = g.mouse_buttons;
}

/* ============================================================
 * Displays
 * ============================================================ */

typedef struct { int count; } monitor_count_ctx;
static BOOL CALLBACK monitor_count_cb(HMONITOR h, HDC dc, LPRECT r, LPARAM lp) {
    (void)h; (void)dc; (void)r;
    monitor_count_ctx* c = (monitor_count_ctx*)lp;
    c->count++;
    return TRUE;
}

int wapi_plat_display_count(void) {
    monitor_count_ctx c = {0};
    EnumDisplayMonitors(NULL, NULL, monitor_count_cb, (LPARAM)&c);
    return c.count;
}

typedef struct {
    int target;
    int current;
    int32_t w, h, hz;
    bool found;
} monitor_info_ctx;

static BOOL CALLBACK monitor_info_cb(HMONITOR h, HDC dc, LPRECT r, LPARAM lp) {
    (void)dc; (void)r;
    monitor_info_ctx* c = (monitor_info_ctx*)lp;
    if (c->current == c->target) {
        MONITORINFOEXW mi;
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(h, (MONITORINFO*)&mi)) {
            c->w = mi.rcMonitor.right  - mi.rcMonitor.left;
            c->h = mi.rcMonitor.bottom - mi.rcMonitor.top;
            DEVMODEW dm = {0};
            dm.dmSize = sizeof(dm);
            if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
                c->hz = (int32_t)dm.dmDisplayFrequency;
            } else {
                c->hz = 60;
            }
            c->found = true;
        }
        return FALSE;
    }
    c->current++;
    return TRUE;
}

bool wapi_plat_display_info(int index, int32_t* out_w, int32_t* out_h, int32_t* out_hz) {
    monitor_info_ctx c = {0};
    c.target = index;
    EnumDisplayMonitors(NULL, NULL, monitor_info_cb, (LPARAM)&c);
    if (!c.found) return false;
    if (out_w)  *out_w  = c.w;
    if (out_h)  *out_h  = c.h;
    if (out_hz) *out_hz = c.hz;
    return true;
}

/* ============================================================
 * Clipboard (CF_UNICODETEXT)
 * ============================================================ */

bool wapi_plat_clipboard_has_text(void) {
    return IsClipboardFormatAvailable(CF_UNICODETEXT) ? true : false;
}

bool wapi_plat_clipboard_set_text(const char* utf8, size_t len) {
    if (!OpenClipboard(NULL)) return false;
    EmptyClipboard();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, (int)len, NULL, 0);
    if (wlen < 0) { CloseClipboard(); return false; }

    HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, (size_t)(wlen + 1) * sizeof(WCHAR));
    if (!hmem) { CloseClipboard(); return false; }

    WCHAR* dst = (WCHAR*)GlobalLock(hmem);
    if (!dst) { GlobalFree(hmem); CloseClipboard(); return false; }
    if (wlen > 0) MultiByteToWideChar(CP_UTF8, 0, utf8, (int)len, dst, wlen);
    dst[wlen] = 0;
    GlobalUnlock(hmem);

    if (!SetClipboardData(CF_UNICODETEXT, hmem)) {
        GlobalFree(hmem);
        CloseClipboard();
        return false;
    }
    /* Ownership transferred to clipboard */
    CloseClipboard();
    return true;
}

size_t wapi_plat_clipboard_get_text(char* out, size_t out_len) {
    if (!OpenClipboard(NULL)) return 0;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (!h) { CloseClipboard(); return 0; }
    WCHAR* wsrc = (WCHAR*)GlobalLock(h);
    if (!wsrc) { CloseClipboard(); return 0; }

    int u8len = WideCharToMultiByte(CP_UTF8, 0, wsrc, -1, NULL, 0, NULL, NULL);
    size_t nbytes = u8len > 0 ? (size_t)(u8len - 1) : 0; /* exclude terminator */

    if (out && out_len > 0 && nbytes > 0) {
        size_t copy = nbytes < out_len ? nbytes : out_len;
        WideCharToMultiByte(CP_UTF8, 0, wsrc, -1, out, (int)copy, NULL, NULL);
    }

    GlobalUnlock(h);
    CloseClipboard();
    return nbytes;
}

/* ============================================================
 * Window operations
 * ============================================================ */

wapi_plat_window_t* wapi_plat_window_create(const wapi_plat_window_desc_t* desc) {
    if (!desc) return NULL;
    if (!g.initialized && !wapi_plat_init()) return NULL;

    struct wapi_plat_window_t* w = win_alloc();
    if (!w) return NULL;
    w->flags = desc->flags;

    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX);
    DWORD exstyle = WS_EX_APPWINDOW;

    if (desc->flags & WAPI_PLAT_WIN_RESIZABLE) style |= WS_MAXIMIZEBOX | WS_THICKFRAME;
    else                                        style &= ~(WS_MAXIMIZEBOX | WS_THICKFRAME);

    if (desc->flags & WAPI_PLAT_WIN_BORDERLESS) {
        style = WS_POPUP;
        exstyle = WS_EX_APPWINDOW;
    }
    if (desc->flags & WAPI_PLAT_WIN_ALWAYS_ON_TOP) exstyle |= WS_EX_TOPMOST;
    if (desc->flags & WAPI_PLAT_WIN_TRANSPARENT)   exstyle |= WS_EX_LAYERED;

    int width  = desc->width  > 0 ? desc->width  : 1280;
    int height = desc->height > 0 ? desc->height : 720;

    /* Convert client size to window size (approximate: uses system DPI;
     * we recomp with per-monitor DPI after creation if needed). */
    RECT rc = { 0, 0, width, height };
    AdjustWindowRectEx(&rc, style, FALSE, exstyle);
    int ww = rc.right - rc.left;
    int wh = rc.bottom - rc.top;

    WCHAR wtitle[256];
    wtitle[0] = 0;
    if (desc->title) {
        MultiByteToWideChar(CP_UTF8, 0, desc->title, -1,
                            wtitle, (int)(sizeof(wtitle) / sizeof(wtitle[0])));
    }

    HWND hwnd = CreateWindowExW(exstyle, L"WAPI_Window", wtitle, style,
                                CW_USEDEFAULT, CW_USEDEFAULT, ww, wh,
                                NULL, NULL, g.hinst, NULL);
    if (!hwnd) {
        win_free(w);
        return NULL;
    }
    w->hwnd = hwnd;
    w->last_width  = width;
    w->last_height = height;

    if (desc->flags & WAPI_PLAT_WIN_TRANSPARENT) {
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    }

    /* Text input disabled by default: detach the IMC */
    w->saved_imc = ImmAssociateContext(hwnd, NULL);
    w->text_input_on = false;

    if (!(desc->flags & WAPI_PLAT_WIN_HIDDEN)) {
        int nCmdShow = (desc->flags & WAPI_PLAT_WIN_FULLSCREEN) ? SW_SHOWMAXIMIZED : SW_SHOW;
        ShowWindow(hwnd, nCmdShow);
        if (desc->flags & WAPI_PLAT_WIN_FULLSCREEN) {
            wapi_plat_window_set_fullscreen(w, true);
        }
    }

    return w;
}

void wapi_plat_window_destroy(wapi_plat_window_t* w) {
    if (!w || !w->hwnd) return;
    if (w->text_input_on && w->saved_imc) {
        ImmAssociateContext(w->hwnd, w->saved_imc);
    }
    if (w->custom_cursor) {
        DestroyCursor(w->custom_cursor);
        w->custom_cursor = NULL;
    }
    DestroyWindow(w->hwnd);
    win_free(w);
}

uint32_t wapi_plat_window_id(wapi_plat_window_t* w) {
    return w ? w->id : 0;
}

void wapi_plat_window_get_size_pixels(wapi_plat_window_t* w, int32_t* out_w, int32_t* out_h) {
    if (!w || !w->hwnd) { if (out_w)*out_w=0; if(out_h)*out_h=0; return; }
    RECT rc;
    GetClientRect(w->hwnd, &rc);
    if (out_w) *out_w = rc.right - rc.left;
    if (out_h) *out_h = rc.bottom - rc.top;
}

void wapi_plat_window_get_size_logical(wapi_plat_window_t* w, int32_t* out_w, int32_t* out_h) {
    /* On Win32, client rect is already in physical pixels. Logical =
     * physical / scale. */
    int32_t pw, ph;
    wapi_plat_window_get_size_pixels(w, &pw, &ph);
    float s = wapi_plat_window_get_dpi_scale(w);
    if (s <= 0.0f) s = 1.0f;
    if (out_w) *out_w = (int32_t)((float)pw / s + 0.5f);
    if (out_h) *out_h = (int32_t)((float)ph / s + 0.5f);
}

float wapi_plat_window_get_dpi_scale(wapi_plat_window_t* w) {
    if (!w || !w->hwnd) return 1.0f;
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        typedef UINT (WINAPI *fn_GetDpiForWindow)(HWND);
        fn_GetDpiForWindow f = (fn_GetDpiForWindow)
            GetProcAddress(user32, "GetDpiForWindow");
        if (f) {
            UINT dpi = f(w->hwnd);
            if (dpi > 0) return (float)dpi / 96.0f;
        }
    }
    HDC dc = GetDC(w->hwnd);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(w->hwnd, dc);
    return dpi > 0 ? (float)dpi / 96.0f : 1.0f;
}

void wapi_plat_window_set_size(wapi_plat_window_t* w, int32_t width, int32_t height) {
    if (!w || !w->hwnd) return;
    RECT rc = { 0, 0, width, height };
    DWORD style = (DWORD)GetWindowLongW(w->hwnd, GWL_STYLE);
    DWORD ex    = (DWORD)GetWindowLongW(w->hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&rc, style, FALSE, ex);
    SetWindowPos(w->hwnd, NULL, 0, 0,
                 rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void wapi_plat_window_set_title(wapi_plat_window_t* w, const char* title, size_t len) {
    if (!w || !w->hwnd || !title) return;
    WCHAR buf[512];
    int n = MultiByteToWideChar(CP_UTF8, 0, title, (int)len,
                                buf, (int)(sizeof(buf)/sizeof(buf[0])) - 1);
    if (n < 0) n = 0;
    buf[n] = 0;
    SetWindowTextW(w->hwnd, buf);
}

void wapi_plat_window_set_fullscreen(wapi_plat_window_t* w, bool fullscreen) {
    if (!w || !w->hwnd) return;
    static WINDOWPLACEMENT saved_placement;
    static bool saved_valid = false;
    static DWORD saved_style = 0;

    DWORD style = (DWORD)GetWindowLongW(w->hwnd, GWL_STYLE);
    if (fullscreen) {
        if (!saved_valid) {
            saved_placement.length = sizeof(saved_placement);
            GetWindowPlacement(w->hwnd, &saved_placement);
            saved_style = style;
            saved_valid = true;
        }
        HMONITOR mon = MonitorFromWindow(w->hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {sizeof(mi)};
        if (GetMonitorInfoW(mon, &mi)) {
            SetWindowLongW(w->hwnd, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
            SetWindowPos(w->hwnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        }
    } else {
        if (saved_valid) {
            SetWindowLongW(w->hwnd, GWL_STYLE, saved_style);
            SetWindowPlacement(w->hwnd, &saved_placement);
            SetWindowPos(w->hwnd, NULL, 0,0,0,0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            saved_valid = false;
        }
    }
}

void wapi_plat_window_set_visible (wapi_plat_window_t* w, bool v) { if (w&&w->hwnd) ShowWindow(w->hwnd, v?SW_SHOW:SW_HIDE); }
void wapi_plat_window_minimize    (wapi_plat_window_t* w)         { if (w&&w->hwnd) ShowWindow(w->hwnd, SW_MINIMIZE); }
void wapi_plat_window_maximize    (wapi_plat_window_t* w)         { if (w&&w->hwnd) ShowWindow(w->hwnd, SW_MAXIMIZE); }
void wapi_plat_window_restore     (wapi_plat_window_t* w)         { if (w&&w->hwnd) ShowWindow(w->hwnd, SW_RESTORE); }

void wapi_plat_window_set_cursor(wapi_plat_window_t* w, uint32_t cursor_id) {
    if (!w) return;
    w->cursor_hidden = false;
    /* Switching to a built-in releases any owned custom cursor. System
     * cursors are shared (LoadCursorW returns shared HCURSORs) so they
     * must NOT be passed through DestroyCursor. */
    if (w->custom_cursor) {
        DestroyCursor(w->custom_cursor);
        w->custom_cursor = NULL;
    }
    switch (cursor_id) {
    case WAPI_PLAT_CURSOR_DEFAULT:     w->cursor = g.cur_arrow;    break;
    case WAPI_PLAT_CURSOR_POINTER:
    case WAPI_PLAT_CURSOR_GRAB:
    case WAPI_PLAT_CURSOR_GRABBING:    w->cursor = g.cur_hand;     break;
    case WAPI_PLAT_CURSOR_TEXT:        w->cursor = g.cur_ibeam;    break;
    case WAPI_PLAT_CURSOR_CROSSHAIR:   w->cursor = g.cur_cross;    break;
    case WAPI_PLAT_CURSOR_MOVE:        w->cursor = g.cur_sizeall;  break;
    case WAPI_PLAT_CURSOR_RESIZE_NS:   w->cursor = g.cur_sizens;   break;
    case WAPI_PLAT_CURSOR_RESIZE_EW:   w->cursor = g.cur_sizewe;   break;
    case WAPI_PLAT_CURSOR_RESIZE_NWSE: w->cursor = g.cur_sizenwse; break;
    case WAPI_PLAT_CURSOR_RESIZE_NESW: w->cursor = g.cur_sizenesw; break;
    case WAPI_PLAT_CURSOR_NOT_ALLOWED: w->cursor = g.cur_no;       break;
    case WAPI_PLAT_CURSOR_WAIT:        w->cursor = g.cur_wait;     break;
    case WAPI_PLAT_CURSOR_HIDDEN:      w->cursor_hidden = true;    break;
    default:                           w->cursor = g.cur_arrow;    break;
    }
    /* Provoke WM_SETCURSOR so it takes effect immediately */
    POINT p; GetCursorPos(&p);
    SetCursorPos(p.x, p.y);
}

void wapi_plat_window_set_relative_mouse(wapi_plat_window_t* w, bool enable) {
    if (!w || !w->hwnd) return;
    w->wants_relative = enable;
    if (enable) {
        RECT rc; GetClientRect(w->hwnd, &rc);
        POINT c = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
        ClientToScreen(w->hwnd, &c);
        SetCursorPos(c.x, c.y);
        ShowCursor(FALSE);
        /* Clip cursor to window during relative mode */
        GetClientRect(w->hwnd, &rc);
        POINT tl = { rc.left, rc.top }, br = { rc.right, rc.bottom };
        ClientToScreen(w->hwnd, &tl); ClientToScreen(w->hwnd, &br);
        RECT clip = { tl.x, tl.y, br.x, br.y };
        ClipCursor(&clip);
    } else {
        ClipCursor(NULL);
        ShowCursor(TRUE);
    }
}

void wapi_plat_window_start_text_input(wapi_plat_window_t* w) {
    if (!w || !w->hwnd || w->text_input_on) return;
    if (w->saved_imc) ImmAssociateContext(w->hwnd, w->saved_imc);
    w->text_input_on = true;
}

void wapi_plat_window_stop_text_input(wapi_plat_window_t* w) {
    if (!w || !w->hwnd || !w->text_input_on) return;
    w->saved_imc = ImmAssociateContext(w->hwnd, NULL);
    w->text_input_on = false;
}

/* ============================================================
 * Native handle (for WebGPU surface)
 * ============================================================ */

bool wapi_plat_window_get_native(wapi_plat_window_t* w, wapi_plat_native_handle_t* out) {
    if (!w || !w->hwnd || !out) return false;
    out->kind = WAPI_PLAT_NATIVE_WIN32;
    out->a = (void*)w->hwnd;
    out->b = (void*)g.hinst;
    return true;
}

/* ============================================================
 * Mouse warp + custom cursor image
 * ============================================================ */

bool wapi_plat_window_warp_mouse(wapi_plat_window_t* w, float x, float y) {
    if (!w || !w->hwnd) return false;
    POINT p = { (LONG)(x + 0.5f), (LONG)(y + 0.5f) };
    if (!ClientToScreen(w->hwnd, &p)) return false;
    if (!SetCursorPos(p.x, p.y)) return false;
    /* Mirror the snapshot so subsequent state polls see the new pos
     * before any synthetic WM_MOUSEMOVE arrives. */
    g.mouse_x = x;
    g.mouse_y = y;
    return true;
}

bool wapi_plat_window_set_cursor_image(wapi_plat_window_t* w,
                                       const void* rgba, int32_t w_px, int32_t h_px,
                                       int32_t hot_x, int32_t hot_y)
{
    if (!w || !w->hwnd || !rgba || w_px <= 0 || h_px <= 0) return false;
    if (w_px > 256 || h_px > 256) return false; /* Win32 cursor cap */

    /* Build a 32-bit ARGB top-down DIB, and a 1-bit mask. The mask is
     * required even for alpha cursors (Win32 still consults it for hit
     * testing); we use an all-zero mask so every pixel is sampled from
     * the color bitmap. */
    BITMAPV5HEADER bi = {0};
    bi.bV5Size        = sizeof(bi);
    bi.bV5Width       = w_px;
    bi.bV5Height      = -h_px;          /* top-down */
    bi.bV5Planes      = 1;
    bi.bV5BitCount    = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask     = 0x00FF0000;
    bi.bV5GreenMask   = 0x0000FF00;
    bi.bV5BlueMask    = 0x000000FF;
    bi.bV5AlphaMask   = 0xFF000000;

    HDC hdc = GetDC(NULL);
    if (!hdc) return false;

    void* bits = NULL;
    HBITMAP color = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS,
                                     &bits, NULL, 0);
    ReleaseDC(NULL, hdc);
    if (!color || !bits) {
        if (color) DeleteObject(color);
        return false;
    }

    /* Convert RGBA → BGRA for Windows, premultiplying alpha so layered
     * cursor compositing draws correctly on transparent pixels. */
    const uint8_t* src = (const uint8_t*)rgba;
    uint8_t*       dst = (uint8_t*)bits;
    int            n   = w_px * h_px;
    for (int i = 0; i < n; i++) {
        uint8_t r = src[i*4 + 0];
        uint8_t g_ = src[i*4 + 1];
        uint8_t b = src[i*4 + 2];
        uint8_t a = src[i*4 + 3];
        dst[i*4 + 0] = (uint8_t)((b * a + 127) / 255);
        dst[i*4 + 1] = (uint8_t)((g_ * a + 127) / 255);
        dst[i*4 + 2] = (uint8_t)((r * a + 127) / 255);
        dst[i*4 + 3] = a;
    }

    /* Empty mask: rows are DWORD-aligned, height = h_px, 1 bpp. */
    int  mask_stride = ((w_px + 31) / 32) * 4;
    int  mask_bytes  = mask_stride * h_px;
    uint8_t* mask_buf = (uint8_t*)calloc(1, (size_t)mask_bytes);
    if (!mask_buf) {
        DeleteObject(color);
        return false;
    }
    HBITMAP mask = CreateBitmap(w_px, h_px, 1, 1, mask_buf);
    free(mask_buf);
    if (!mask) {
        DeleteObject(color);
        return false;
    }

    ICONINFO ii = {0};
    ii.fIcon    = FALSE;
    ii.xHotspot = (DWORD)(hot_x < 0 ? 0 : hot_x);
    ii.yHotspot = (DWORD)(hot_y < 0 ? 0 : hot_y);
    ii.hbmMask  = mask;
    ii.hbmColor = color;
    HCURSOR cur = (HCURSOR)CreateIconIndirect(&ii);
    DeleteObject(mask);
    DeleteObject(color);
    if (!cur) return false;

    if (w->custom_cursor) DestroyCursor(w->custom_cursor);
    w->custom_cursor = cur;
    w->cursor        = cur;
    w->cursor_hidden = false;

    POINT p; GetCursorPos(&p);
    SetCursorPos(p.x, p.y); /* provoke WM_SETCURSOR */
    return true;
}

/* ============================================================
 * IME composition accessors / control
 * ============================================================ */

bool wapi_plat_window_ime_get_preedit(wapi_plat_window_t* w,
                                      char* buf, uint32_t buf_len,
                                      uint32_t* out_byte_len,
                                      uint32_t* out_cursor_bytes,
                                      uint32_t* out_segment_count)
{
    if (!w) return false;
    if (out_byte_len)      *out_byte_len      = w->preedit_len;
    if (out_cursor_bytes)  *out_cursor_bytes  = w->preedit_cursor;
    if (out_segment_count) *out_segment_count = w->preedit_seg_count;
    if (buf && buf_len > 0) {
        uint32_t copy = w->preedit_len < buf_len ? w->preedit_len : buf_len;
        memcpy(buf, w->preedit, copy);
        if (copy < buf_len) buf[copy] = 0;
    }
    return w->ime_composing || w->preedit_len > 0;
}

bool wapi_plat_window_ime_get_segment(wapi_plat_window_t* w, uint32_t index,
                                      uint32_t* out_start, uint32_t* out_length,
                                      uint32_t* out_flags)
{
    if (!w || index >= w->preedit_seg_count) return false;
    const preedit_seg_t* s = &w->preedit_segs[index];
    if (out_start)  *out_start  = s->start;
    if (out_length) *out_length = s->length;
    if (out_flags)  *out_flags  = s->flags;
    return true;
}

void wapi_plat_window_ime_set_candidate_rect(wapi_plat_window_t* w,
                                             int32_t x, int32_t y,
                                             int32_t w_px, int32_t h_px)
{
    if (!w || !w->hwnd) return;
    /* Anchor at the bottom-left of the caret rect so the IME draws the
     * candidate list below the caret (Win32 convention). */
    w->ime_candidate_pt.x = x;
    w->ime_candidate_pt.y = y + (h_px > 0 ? h_px : 0);
    w->ime_candidate_set  = true;
    (void)w_px;

    if (w->ime_composing) {
        HIMC himc = ImmGetContext(w->hwnd);
        if (himc) {
            CANDIDATEFORM cf = {0};
            cf.dwIndex      = 0;
            cf.dwStyle      = CFS_EXCLUDE;
            cf.ptCurrentPos = w->ime_candidate_pt;
            ImmSetCandidateWindow(himc, &cf);
            ImmReleaseContext(w->hwnd, himc);
        }
    }
}

void wapi_plat_window_ime_force_commit(wapi_plat_window_t* w) {
    if (!w || !w->hwnd || !w->ime_composing) return;
    HIMC himc = ImmGetContext(w->hwnd);
    if (himc) {
        /* Ask the IME to finalize. The composition will deliver
         * GCS_RESULTSTR via WM_IME_COMPOSITION → our wndproc emits
         * the corresponding TEXT_INPUT, then WM_IME_ENDCOMPOSITION
         * clears state. */
        ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
        ImmReleaseContext(w->hwnd, himc);
    }
}

void wapi_plat_window_ime_force_cancel(wapi_plat_window_t* w) {
    if (!w || !w->hwnd || !w->ime_composing) return;
    HIMC himc = ImmGetContext(w->hwnd);
    if (himc) {
        ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
        ImmReleaseContext(w->hwnd, himc);
    }
}
