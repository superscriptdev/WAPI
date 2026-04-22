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

/* WM_POINTER / RegisterTouchWindow / pointer_* require Win8+ headers. */
#ifndef WM_POINTERDOWN
#define WM_POINTERDOWN           0x0246
#define WM_POINTERUP             0x0247
#define WM_POINTERUPDATE         0x0245
#define WM_POINTERENTER          0x0249
#define WM_POINTERLEAVE          0x024A
#define WM_POINTERCAPTURECHANGED 0x024C
#endif
#ifndef TOUCHEVENTF_MOVE
#define TOUCHEVENTF_MOVE     0x0001
#define TOUCHEVENTF_DOWN     0x0002
#define TOUCHEVENTF_UP       0x0004
#define TOUCHEVENTF_INRANGE  0x0008
#define TOUCHEVENTF_PRIMARY  0x0010
#define TOUCHEVENTF_NOCOALESCE 0x0020
#define TOUCHEVENTF_PEN      0x0040
#define TOUCHEVENTF_PALM     0x0080
typedef struct tagTOUCHINPUT {
    LONG      x;
    LONG      y;
    HANDLE    hSource;
    DWORD     dwID;
    DWORD     dwFlags;
    DWORD     dwMask;
    DWORD     dwTime;
    ULONG_PTR dwExtraInfo;
    DWORD     cxContact;
    DWORD     cyContact;
} TOUCHINPUT, *PTOUCHINPUT;
typedef TOUCHINPUT const * PCTOUCHINPUT;
#define TOUCH_COORD_TO_PIXEL(l) ((l) / 100)
#endif

/* Pointer / pen info — use our own struct layout and cast to the
 * SDK type at the call site. Mirrors <winuser.h>
 * (POINTER_INFO + POINTER_PEN_INFO) per Win10 SDK; we only read
 * what we care about. */
#define WAPI_PT_POINTER 1
#define WAPI_PT_TOUCH   2
#define WAPI_PT_PEN     3
#define WAPI_PT_MOUSE   4

#define WAPI_PEN_FLAG_BARREL    0x00000001
#define WAPI_PEN_FLAG_INVERTED  0x00000002
#define WAPI_PEN_FLAG_ERASER    0x00000004

#define WAPI_PEN_MASK_PRESSURE  0x00000001
#define WAPI_PEN_MASK_ROTATION  0x00000002
#define WAPI_PEN_MASK_TILT_X    0x00000004
#define WAPI_PEN_MASK_TILT_Y    0x00000008

typedef struct wapi_pointer_info_win_t {
    UINT32       pointerType;
    UINT32       pointerId;
    UINT32       frameId;
    UINT32       pointerFlags;
    HANDLE       sourceDevice;
    HWND         hwndTarget;
    POINT        ptPixelLocation;
    POINT        ptHimetricLocation;
    POINT        ptPixelLocationRaw;
    POINT        ptHimetricLocationRaw;
    DWORD        dwTime;
    UINT32       historyCount;
    INT32        inputData;
    DWORD        dwKeyStates;
    UINT64       PerformanceCount;
    INT          ButtonChangeType;
} wapi_pointer_info_win_t;

typedef struct wapi_pointer_pen_info_win_t {
    wapi_pointer_info_win_t pointerInfo;
    UINT32  penFlags;
    UINT32  penMask;
    UINT32  pressure;      /* 0..1024 */
    UINT32  rotation;      /* 0..359 degrees */
    INT32   tiltX;         /* -90..90 */
    INT32   tiltY;         /* -90..90 */
} wapi_pointer_pen_info_win_t;

#ifndef GET_POINTERID_WPARAM
#define GET_POINTERID_WPARAM(w) ((UINT32)(LOWORD(w)))
#endif

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

    /* Drop-target side-store. The last WM_DROPFILES payload is kept
     * on the window until the host drains it via
     * wapi_plat_window_drop_payload. Overwritten on each drop. */
    bool          drop_enabled;
    char*         drop_payload;      /* heap-allocated URI list; LF-separated */
    size_t        drop_payload_len;
    float         drop_point_x;
    float         drop_point_y;
};

#define WAPI_WIN32_MAX_FINGERS 10
#define WAPI_WIN32_MAX_HOTKEYS 64

typedef struct touch_finger_t {
    bool     active;
    uint32_t os_id;          /* TOUCHINPUT.dwID / POINTER pointerId  */
    int32_t  finger_idx;     /* stable slot index 0..MAX_FINGERS-1   */
    float    norm_x, norm_y; /* screen-normalized 0..1 for raw query */
    float    pressure;
} touch_finger_t;

typedef struct hotkey_slot_t {
    bool     active;
    uint32_t id;             /* guest-visible id   */
    int      atom_id;        /* RegisterHotKey id (1..0xBFFF)       */
    uint32_t mod_mask;       /* WAPI_KMOD_* for diag                 */
    uint32_t scancode;       /* HID scancode for diag                */
} hotkey_slot_t;

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

    /* Touch tracking — active fingers across all windows */
    touch_finger_t  fingers[WAPI_WIN32_MAX_FINGERS];
    bool            touch_available;   /* SM_DIGITIZER query */
    uint32_t        touch_max_fingers; /* SM_MAXIMUMTOUCHES  */

    /* Pen tracking — last known state */
    bool     pen_active;
    bool     pen_in_range;
    uint32_t pen_tool;            /* 0=pen, 1=eraser */
    float    pen_x, pen_y;        /* surface-pixel   */
    float    pen_pressure;
    float    pen_tilt_x, pen_tilt_y;
    float    pen_twist;
    float    pen_distance;
    uint32_t pen_capabilities;    /* bitmask of (1 << wapi_pen_axis_t) */

    /* Hotkeys */
    hotkey_slot_t hotkeys[WAPI_WIN32_MAX_HOTKEYS];
    int           hotkeys_next_atom; /* next RegisterHotKey id to try */

    /* Dynamically-resolved Win8+ entry points */
    BOOL (WINAPI *fn_RegisterTouchWindow)(HWND, ULONG);
    BOOL (WINAPI *fn_UnregisterTouchWindow)(HWND);
    BOOL (WINAPI *fn_GetTouchInputInfo)(HANDLE, UINT, PTOUCHINPUT, int);
    BOOL (WINAPI *fn_CloseTouchInputHandle)(HANDLE);
    BOOL (WINAPI *fn_EnableMouseInPointer)(BOOL);
    BOOL (WINAPI *fn_GetPointerType)(UINT32, UINT32*);
    BOOL (WINAPI *fn_GetPointerPenInfo)(UINT32, wapi_pointer_pen_info_win_t*);
    BOOL (WINAPI *fn_GetPointerInfo)(UINT32, wapi_pointer_info_win_t*);

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

/* Exposed to sibling TUs in platform/win32 (gamepad, audio, menu/tray)
 * for event injection and timestamping. */
void wapi_plat_win32_push_event(const wapi_plat_event_t* ev) { ev_push(ev); }
uint64_t wapi_plat_win32_now_ns(void)                        { return now_ns(); }

/* Forward decl — defined alongside the clipboard section below. The
 * WM_DROPFILES handler in wndproc needs it. */
static size_t hdrop_to_uri_list(HDROP hdrop, char* out, size_t out_cap);

/* Implemented in wapi_plat_win32_menu.c. Returns true if the command
 * id was a registered menu/tray item (in which case an event has
 * been queued); false lets DefWindowProc handle it. */
bool wapi_plat_win32_menu_dispatch_command(uint32_t id);

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

/* HID Usage Page 0x07 scancode -> Win32 VK_. 0 = unmapped.
 * Used by wapi_plat_hotkey_register to convert guest-supplied
 * HID scancodes into RegisterHotKey's VK parameter. */
static const uint16_t kHidToVk[256] = {
    [0x04]='A', [0x05]='B', [0x06]='C', [0x07]='D', [0x08]='E',
    [0x09]='F', [0x0A]='G', [0x0B]='H', [0x0C]='I', [0x0D]='J',
    [0x0E]='K', [0x0F]='L', [0x10]='M', [0x11]='N', [0x12]='O',
    [0x13]='P', [0x14]='Q', [0x15]='R', [0x16]='S', [0x17]='T',
    [0x18]='U', [0x19]='V', [0x1A]='W', [0x1B]='X', [0x1C]='Y',
    [0x1D]='Z',
    [0x1E]='1', [0x1F]='2', [0x20]='3', [0x21]='4', [0x22]='5',
    [0x23]='6', [0x24]='7', [0x25]='8', [0x26]='9', [0x27]='0',
    [0x28]=VK_RETURN, [0x29]=VK_ESCAPE, [0x2A]=VK_BACK,
    [0x2B]=VK_TAB,    [0x2C]=VK_SPACE,  [0x2D]=VK_OEM_MINUS,
    [0x2E]=VK_OEM_PLUS, [0x2F]=VK_OEM_4, [0x30]=VK_OEM_6,
    [0x31]=VK_OEM_5,    [0x33]=VK_OEM_1, [0x34]=VK_OEM_7,
    [0x35]=VK_OEM_3,    [0x36]=VK_OEM_COMMA,
    [0x37]=VK_OEM_PERIOD, [0x38]=VK_OEM_2,
    [0x39]=VK_CAPITAL,
    [0x3A]=VK_F1,  [0x3B]=VK_F2,  [0x3C]=VK_F3,  [0x3D]=VK_F4,
    [0x3E]=VK_F5,  [0x3F]=VK_F6,  [0x40]=VK_F7,  [0x41]=VK_F8,
    [0x42]=VK_F9,  [0x43]=VK_F10, [0x44]=VK_F11, [0x45]=VK_F12,
    [0x46]=VK_SNAPSHOT, [0x47]=VK_SCROLL, [0x48]=VK_PAUSE,
    [0x49]=VK_INSERT,   [0x4A]=VK_HOME,   [0x4B]=VK_PRIOR,
    [0x4C]=VK_DELETE,   [0x4D]=VK_END,    [0x4E]=VK_NEXT,
    [0x4F]=VK_RIGHT,    [0x50]=VK_LEFT,   [0x51]=VK_DOWN,
    [0x52]=VK_UP,
    [0x53]=VK_NUMLOCK,
    [0x54]=VK_DIVIDE, [0x55]=VK_MULTIPLY, [0x56]=VK_SUBTRACT,
    [0x57]=VK_ADD,    [0x58]=VK_RETURN,
    [0x59]=VK_NUMPAD1,[0x5A]=VK_NUMPAD2,  [0x5B]=VK_NUMPAD3,
    [0x5C]=VK_NUMPAD4,[0x5D]=VK_NUMPAD5,  [0x5E]=VK_NUMPAD6,
    [0x5F]=VK_NUMPAD7,[0x60]=VK_NUMPAD8,  [0x61]=VK_NUMPAD9,
    [0x62]=VK_NUMPAD0,[0x63]=VK_DECIMAL,
    [0xE0]=VK_LCONTROL,[0xE1]=VK_LSHIFT,[0xE2]=VK_LMENU,[0xE3]=VK_LWIN,
    [0xE4]=VK_RCONTROL,[0xE5]=VK_RSHIFT,[0xE6]=VK_RMENU,[0xE7]=VK_RWIN,
};

/* WAPI_KMOD_* bits -> RegisterHotKey fsModifiers. Left/right variants
 * collapse to the side-agnostic MOD_*, matching Win32's behavior. */
static UINT wapi_mod_to_winmod(uint32_t wapi_mod) {
    UINT m = 0;
    if (wapi_mod & (0x0001 | 0x0002)) m |= MOD_SHIFT;
    if (wapi_mod & (0x0040 | 0x0080)) m |= MOD_CONTROL;
    if (wapi_mod & (0x0100 | 0x0200)) m |= MOD_ALT;
    if (wapi_mod & (0x0400 | 0x0800)) m |= MOD_WIN;
    return m;
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

    case WM_COMMAND: {
        /* HIWORD(wp)==0 = menu, ==1 = accelerator, else control
         * notification. We only care about menu clicks here. */
        if (HIWORD(wp) == 0) {
            uint32_t id = (uint32_t)LOWORD(wp);
            if (wapi_plat_win32_menu_dispatch_command(id)) return 0;
        }
        break;
    }

    case WM_DROPFILES: {
        if (!w) break;
        HDROP drop = (HDROP)wp;

        /* Drop point in client coords. */
        POINT pt;
        if (DragQueryPoint(drop, &pt) == 0) {
            /* Point was outside the client rect; still deliver, at 0,0. */
            pt.x = 0; pt.y = 0;
        }

        /* Build the text/uri-list payload. Reuse the clipboard
         * helper so CF_HDROP and WM_DROPFILES share one path. */
        size_t need = hdrop_to_uri_list(drop, NULL, 0);
        if (need > 0) {
            free(w->drop_payload);
            w->drop_payload = (char*)malloc(need);
            if (w->drop_payload) {
                hdrop_to_uri_list(drop, w->drop_payload, need);
                w->drop_payload_len = need;
            } else {
                w->drop_payload_len = 0;
            }
        }
        w->drop_point_x = (float)pt.x;
        w->drop_point_y = (float)pt.y;
        DragFinish(drop);

        wapi_plat_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = WAPI_PLAT_EV_DROP_FILES;
        ev.window_id = w->id;
        ev.timestamp_ns = now_ns();
        ev.u.drop.x = w->drop_point_x;
        ev.u.drop.y = w->drop_point_y;
        ev.u.drop.payload_bytes = (uint32_t)w->drop_payload_len;
        ev_push(&ev);
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

    case 0x0240 /* WM_TOUCH */: {
        if (!w || !g.fn_GetTouchInputInfo || !g.fn_CloseTouchInputHandle) break;
        UINT count = LOWORD(wp);
        if (count == 0) break;
        if (count > WAPI_WIN32_MAX_FINGERS) count = WAPI_WIN32_MAX_FINGERS;
        TOUCHINPUT inputs[WAPI_WIN32_MAX_FINGERS];
        if (!g.fn_GetTouchInputInfo((HANDLE)lp, count, inputs, sizeof(TOUCHINPUT))) break;

        RECT rc;
        GetClientRect(hwnd, &rc);
        int cw = rc.right - rc.left; if (cw <= 0) cw = 1;
        int ch = rc.bottom - rc.top; if (ch <= 0) ch = 1;

        /* Screen dimensions for device-normalized coords */
        int sw = GetSystemMetrics(SM_CXSCREEN); if (sw <= 0) sw = 1;
        int sh = GetSystemMetrics(SM_CYSCREEN); if (sh <= 0) sh = 1;

        for (UINT i = 0; i < count; i++) {
            const TOUCHINPUT* ti = &inputs[i];
            int px_screen_x = TOUCH_COORD_TO_PIXEL(ti->x);
            int px_screen_y = TOUCH_COORD_TO_PIXEL(ti->y);
            POINT pt = { px_screen_x, px_screen_y };
            ScreenToClient(hwnd, &pt);

            /* Find or allocate a finger slot keyed on OS dwID */
            int slot = -1;
            for (int k = 0; k < WAPI_WIN32_MAX_FINGERS; k++) {
                if (g.fingers[k].active && g.fingers[k].os_id == ti->dwID) { slot = k; break; }
            }
            if (slot < 0 && (ti->dwFlags & TOUCHEVENTF_DOWN)) {
                for (int k = 0; k < WAPI_WIN32_MAX_FINGERS; k++) {
                    if (!g.fingers[k].active) {
                        g.fingers[k].active = true;
                        g.fingers[k].os_id = ti->dwID;
                        g.fingers[k].finger_idx = k;
                        slot = k;
                        break;
                    }
                }
            }
            if (slot < 0) continue;

            float prev_x = g.fingers[slot].norm_x;
            float prev_y = g.fingers[slot].norm_y;
            float norm_x = (float)px_screen_x / (float)sw;
            float norm_y = (float)px_screen_y / (float)sh;
            g.fingers[slot].norm_x = norm_x;
            g.fingers[slot].norm_y = norm_y;
            g.fingers[slot].pressure = (ti->dwFlags & TOUCHEVENTF_UP) ? 0.0f : 1.0f;

            wapi_plat_event_t ev; memset(&ev, 0, sizeof(ev));
            if      (ti->dwFlags & TOUCHEVENTF_DOWN) ev.type = WAPI_PLAT_EV_FINGER_DOWN;
            else if (ti->dwFlags & TOUCHEVENTF_UP)   ev.type = WAPI_PLAT_EV_FINGER_UP;
            else                                     ev.type = WAPI_PLAT_EV_FINGER_MOTION;
            ev.window_id = w->id;
            ev.timestamp_ns = now_ns();
            ev.u.touch.touch_id  = 1;              /* aggregate touch device */
            ev.u.touch.finger_id = (uint64_t)g.fingers[slot].finger_idx;
            ev.u.touch.x = (float)pt.x;
            ev.u.touch.y = (float)pt.y;
            ev.u.touch.dx = (norm_x - prev_x) * (float)cw;
            ev.u.touch.dy = (norm_y - prev_y) * (float)ch;
            ev.u.touch.pressure = g.fingers[slot].pressure;
            ev_push(&ev);

            if (ti->dwFlags & TOUCHEVENTF_UP) {
                g.fingers[slot].active = false;
                g.fingers[slot].os_id = 0;
            }
        }
        g.fn_CloseTouchInputHandle((HANDLE)lp);
        return 0;
    }

    case WM_POINTERDOWN:
    case WM_POINTERUP:
    case WM_POINTERUPDATE: {
        if (!w || !g.fn_GetPointerType || !g.fn_GetPointerPenInfo) break;
        UINT32 pid = GET_POINTERID_WPARAM(wp);
        UINT32 ptype = 0;
        if (!g.fn_GetPointerType(pid, &ptype)) break;
        if (ptype != WAPI_PT_PEN) break; /* touch is covered by WM_TOUCH */

        wapi_pointer_pen_info_win_t pen; memset(&pen, 0, sizeof(pen));
        if (!g.fn_GetPointerPenInfo(pid, &pen)) break;

        POINT pt = pen.pointerInfo.ptPixelLocation;
        ScreenToClient(hwnd, &pt);

        bool eraser  = (pen.penFlags & WAPI_PEN_FLAG_ERASER) != 0 ||
                       (pen.penFlags & WAPI_PEN_FLAG_INVERTED) != 0;
        bool barrel  = (pen.penFlags & WAPI_PEN_FLAG_BARREL) != 0;
        float pressure = (pen.penMask & WAPI_PEN_MASK_PRESSURE)
                       ? (float)pen.pressure / 1024.0f : 0.5f;
        float tilt_x  = (pen.penMask & WAPI_PEN_MASK_TILT_X)
                       ? (float)pen.tiltX / 90.0f : 0.0f;
        float tilt_y  = (pen.penMask & WAPI_PEN_MASK_TILT_Y)
                       ? (float)pen.tiltY / 90.0f : 0.0f;
        float twist   = (pen.penMask & WAPI_PEN_MASK_ROTATION)
                       ? (float)pen.rotation * (6.2831853f / 360.0f) : 0.0f;

        uint32_t caps = 0;
        if (pen.penMask & WAPI_PEN_MASK_PRESSURE) caps |= (1u << 0); /* PRESSURE */
        if (pen.penMask & WAPI_PEN_MASK_TILT_X)   caps |= (1u << 1); /* TILT_X   */
        if (pen.penMask & WAPI_PEN_MASK_TILT_Y)   caps |= (1u << 2); /* TILT_Y   */
        if (pen.penMask & WAPI_PEN_MASK_ROTATION) caps |= (1u << 3); /* ROTATION */

        g.pen_active    = (msg != WM_POINTERUP);
        g.pen_in_range  = g.pen_active;
        g.pen_tool      = eraser ? 1u : 0u;
        g.pen_x         = (float)pt.x;
        g.pen_y         = (float)pt.y;
        g.pen_pressure  = pressure;
        g.pen_tilt_x    = tilt_x;
        g.pen_tilt_y    = tilt_y;
        g.pen_twist     = twist;
        g.pen_distance  = 0.0f;
        g.pen_capabilities = caps;

        wapi_plat_event_t ev; memset(&ev, 0, sizeof(ev));
        if      (msg == WM_POINTERDOWN) ev.type = WAPI_PLAT_EV_PEN_DOWN;
        else if (msg == WM_POINTERUP)   ev.type = WAPI_PLAT_EV_PEN_UP;
        else                            ev.type = WAPI_PLAT_EV_PEN_MOTION;
        ev.window_id = w->id;
        ev.timestamp_ns = now_ns();
        ev.u.pen.pen_id    = 1;
        ev.u.pen.tool_type = eraser ? 1 : 0;
        ev.u.pen.button    = barrel ? 1 : 0;
        ev.u.pen.x         = (float)pt.x;
        ev.u.pen.y         = (float)pt.y;
        ev.u.pen.pressure  = pressure;
        ev.u.pen.tilt_x    = tilt_x;
        ev.u.pen.tilt_y    = tilt_y;
        ev.u.pen.twist     = twist;
        ev.u.pen.distance  = 0.0f;
        ev_push(&ev);
        return 0;
    }
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

    /* Dynamically resolve Win8+ input APIs; fall back silently when
     * missing so core windowing keeps working on older / stripped
     * Windows images. */
    if (user32) {
        g.fn_RegisterTouchWindow   = (BOOL (WINAPI*)(HWND, ULONG))            GetProcAddress(user32, "RegisterTouchWindow");
        g.fn_UnregisterTouchWindow = (BOOL (WINAPI*)(HWND))                   GetProcAddress(user32, "UnregisterTouchWindow");
        g.fn_GetTouchInputInfo     = (BOOL (WINAPI*)(HANDLE, UINT, PTOUCHINPUT, int)) GetProcAddress(user32, "GetTouchInputInfo");
        g.fn_CloseTouchInputHandle = (BOOL (WINAPI*)(HANDLE))                 GetProcAddress(user32, "CloseTouchInputHandle");
        g.fn_EnableMouseInPointer  = (BOOL (WINAPI*)(BOOL))                   GetProcAddress(user32, "EnableMouseInPointer");
        g.fn_GetPointerType        = (BOOL (WINAPI*)(UINT32, UINT32*))        GetProcAddress(user32, "GetPointerType");
        g.fn_GetPointerPenInfo     = (BOOL (WINAPI*)(UINT32, wapi_pointer_pen_info_win_t*)) GetProcAddress(user32, "GetPointerPenInfo");
        g.fn_GetPointerInfo        = (BOOL (WINAPI*)(UINT32, wapi_pointer_info_win_t*)) GetProcAddress(user32, "GetPointerInfo");
    }

    /* Touch availability: SM_DIGITIZER bit 0 = NID_INTEGRATED_TOUCH, bit 1 = NID_EXTERNAL_TOUCH. */
    int digi = GetSystemMetrics(94 /* SM_DIGITIZER */);
    g.touch_available   = (g.fn_RegisterTouchWindow && (digi & 0x03)) ? true : false;
    int maxfingers = GetSystemMetrics(95 /* SM_MAXIMUMTOUCHES */);
    if (maxfingers < 0) maxfingers = 0;
    if (maxfingers > WAPI_WIN32_MAX_FINGERS) maxfingers = WAPI_WIN32_MAX_FINGERS;
    g.touch_max_fingers = (uint32_t)maxfingers;

    /* Route pen/touch through the pointer stack so WM_POINTER* arrives
     * for pens; touch keeps going through WM_TOUCH since we register
     * those windows explicitly. Best-effort: some sessions reject this. */
    if (g.fn_EnableMouseInPointer) {
        g.fn_EnableMouseInPointer(TRUE);
    }

    g.hotkeys_next_atom = 1;

    g.next_window_id = 0;
    g.initialized = true;
    return true;
}

void wapi_plat_shutdown(void) {
    if (!g.initialized) return;

    /* Release any live global hotkeys before the thread message pump goes away. */
    for (int i = 0; i < WAPI_WIN32_MAX_HOTKEYS; i++) {
        if (g.hotkeys[i].active) {
            UnregisterHotKey(NULL, g.hotkeys[i].atom_id);
            g.hotkeys[i].active = false;
        }
    }

    /* Destroy all windows */
    for (int i = 1; i < WAPI_WIN32_MAX_WINDOWS; i++) {
        if (g.windows[i].hwnd) {
            if (g.touch_available && g.fn_UnregisterTouchWindow) {
                g.fn_UnregisterTouchWindow(g.windows[i].hwnd);
            }
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
        if (m.message == WM_HOTKEY) {
            /* wParam is the RegisterHotKey atom id; map back to the
             * guest-visible id through our table so modules never see
             * raw Win32 ids. */
            int atom_id = (int)m.wParam;
            for (int i = 0; i < WAPI_WIN32_MAX_HOTKEYS; i++) {
                if (g.hotkeys[i].active && g.hotkeys[i].atom_id == atom_id) {
                    wapi_plat_event_t ev; memset(&ev, 0, sizeof(ev));
                    ev.type = WAPI_PLAT_EV_HOTKEY;
                    ev.window_id = 0;
                    ev.timestamp_ns = now_ns();
                    ev.u.hotkey.hotkey_id = g.hotkeys[i].id;
                    ev_push(&ev);
                    break;
                }
            }
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

/* Image clipboard (CF_DIB).
 * Windows stores device-independent bitmap *contents* (BITMAPINFOHEADER
 * + palette + pixels) without the 14-byte BITMAPFILEHEADER. We produce
 * a standalone BMP by prefixing a file header when reading, and strip
 * it when writing. */

bool wapi_plat_clipboard_has_image(void) {
    return IsClipboardFormatAvailable(CF_DIB) ? true : false;
}

size_t wapi_plat_clipboard_get_image(void* out, size_t out_len) {
    if (!OpenClipboard(NULL)) return 0;
    HANDLE h = GetClipboardData(CF_DIB);
    if (!h) { CloseClipboard(); return 0; }
    BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)GlobalLock(h);
    if (!bih) { CloseClipboard(); return 0; }
    size_t dib_size = GlobalSize(h);
    if (dib_size < sizeof(BITMAPINFOHEADER)) {
        GlobalUnlock(h); CloseClipboard(); return 0;
    }

    /* Compute palette + pixel offset within the DIB. */
    size_t hdr = bih->biSize;
    size_t pal = 0;
    if (bih->biBitCount <= 8) {
        DWORD n = bih->biClrUsed ? bih->biClrUsed : (1u << bih->biBitCount);
        pal = (size_t)n * sizeof(RGBQUAD);
    } else if (bih->biCompression == BI_BITFIELDS) {
        pal = 3 * sizeof(DWORD);
    }
    size_t total = 14 /* BITMAPFILEHEADER */ + dib_size;

    if (out && out_len > 0) {
        size_t copy = total < out_len ? total : out_len;
        uint8_t* dst = (uint8_t*)out;
        if (copy >= 14) {
            /* BITMAPFILEHEADER: 'BM', size (total), reserved, pixel offset */
            dst[0] = 'B'; dst[1] = 'M';
            uint32_t sz = (uint32_t)total;
            memcpy(dst + 2, &sz, 4);
            uint32_t zero = 0;
            memcpy(dst + 6, &zero, 4);
            uint32_t pix_off = 14 + (uint32_t)(hdr + pal);
            memcpy(dst + 10, &pix_off, 4);
            size_t body = copy - 14;
            if (body > dib_size) body = dib_size;
            memcpy(dst + 14, bih, body);
        } else {
            memset(dst, 0, copy);
        }
    }
    GlobalUnlock(h);
    CloseClipboard();
    return total;
}

bool wapi_plat_clipboard_set_image(const void* bmp_file, size_t len) {
    if (!bmp_file || len < 14 + sizeof(BITMAPINFOHEADER)) return false;
    const uint8_t* p = (const uint8_t*)bmp_file;
    if (p[0] != 'B' || p[1] != 'M') return false;
    size_t dib_size = len - 14;

    HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, dib_size);
    if (!hmem) return false;
    void* dst = GlobalLock(hmem);
    if (!dst) { GlobalFree(hmem); return false; }
    memcpy(dst, p + 14, dib_size);
    GlobalUnlock(hmem);

    if (!OpenClipboard(NULL)) { GlobalFree(hmem); return false; }
    EmptyClipboard();
    if (!SetClipboardData(CF_DIB, hmem)) {
        GlobalFree(hmem); CloseClipboard(); return false;
    }
    CloseClipboard();
    return true;
}

/* File clipboard (CF_HDROP).
 * Win32 stores UTF-16 paths; we emit them as a text/uri-list of
 * file:// URIs separated by CRLF, with percent-encoding applied to
 * spaces and other reserved bytes. Path-to-URI encoding follows
 * RFC 8089 for the "file" scheme. */

static const char HEXCHARS[] = "0123456789ABCDEF";

/* Percent-encode a UTF-8 path chunk into `out`, returning bytes
 * written. `out_cap` caps the write; returns the byte count that
 * WOULD have been written when `out`/`out_cap` are NULL/0. */
static size_t uri_encode(const char* utf8, size_t len, char* out, size_t out_cap) {
    size_t w = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)utf8[i];
        bool safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') ||
                    c == '-' || c == '_' || c == '.' || c == '~' ||
                    c == '/';
        if (safe) {
            if (out && w < out_cap) out[w] = (char)c;
            w++;
        } else {
            if (out && w + 2 < out_cap) {
                out[w + 0] = '%';
                out[w + 1] = HEXCHARS[(c >> 4) & 0xF];
                out[w + 2] = HEXCHARS[c & 0xF];
            }
            w += 3;
        }
    }
    return w;
}

bool wapi_plat_clipboard_has_files(void) {
    return IsClipboardFormatAvailable(CF_HDROP) ? true : false;
}

/* Build a text/uri-list from a CF_HDROP (or HDROP from WM_DROPFILES).
 * Shared between wapi_plat_clipboard_get_files and the drop path. */
static size_t hdrop_to_uri_list(HDROP hdrop, char* out, size_t out_cap) {
    UINT n = DragQueryFileW(hdrop, 0xFFFFFFFF, NULL, 0);
    size_t total = 0;
    for (UINT i = 0; i < n; i++) {
        UINT wlen = DragQueryFileW(hdrop, i, NULL, 0);
        if (wlen == 0) continue;
        WCHAR* wbuf = (WCHAR*)malloc((size_t)(wlen + 1) * sizeof(WCHAR));
        if (!wbuf) continue;
        DragQueryFileW(hdrop, i, wbuf, wlen + 1);

        /* Normalize backslashes to forward slashes for the URI. */
        for (UINT k = 0; k < wlen; k++) if (wbuf[k] == L'\\') wbuf[k] = L'/';

        int u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen, NULL, 0, NULL, NULL);
        if (u8len <= 0) { free(wbuf); continue; }
        char* u8 = (char*)malloc((size_t)u8len);
        if (!u8) { free(wbuf); continue; }
        WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen, u8, u8len, NULL, NULL);

        /* "file:///" + percent-encoded path. Absolute Win32 paths start
         * with a drive letter, so we prefix with three slashes to make
         * a canonical file URI. */
        const char* prefix = "file:///";
        size_t plen = 8;
        size_t enc_need = uri_encode(u8, (size_t)u8len, NULL, 0);

        if (out && total + plen + enc_need + 2 <= out_cap) {
            memcpy(out + total, prefix, plen);
            uri_encode(u8, (size_t)u8len, out + total + plen, out_cap - (total + plen));
            out[total + plen + enc_need + 0] = '\r';
            out[total + plen + enc_need + 1] = '\n';
        }
        total += plen + enc_need + 2;

        free(u8);
        free(wbuf);
    }
    return total;
}

size_t wapi_plat_clipboard_get_files(char* out, size_t out_len) {
    if (!OpenClipboard(NULL)) return 0;
    HDROP h = (HDROP)GetClipboardData(CF_HDROP);
    size_t total = h ? hdrop_to_uri_list(h, out, out_len) : 0;
    CloseClipboard();
    return total;
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

    /* Opt this window into WM_TOUCH delivery when the platform supports
     * it. Fine-touch mode disables palm rejection so the backend can
     * report palm flags via TOUCHEVENTF_PALM if needed; keep the default
     * (coalesced) for now. */
    if (g.touch_available && g.fn_RegisterTouchWindow) {
        g.fn_RegisterTouchWindow(hwnd, 0);
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
    if (g.touch_available && g.fn_UnregisterTouchWindow) {
        g.fn_UnregisterTouchWindow(w->hwnd);
    }
    if (w->text_input_on && w->saved_imc) {
        ImmAssociateContext(w->hwnd, w->saved_imc);
    }
    if (w->custom_cursor) {
        DestroyCursor(w->custom_cursor);
        w->custom_cursor = NULL;
    }
    if (w->drop_payload) {
        free(w->drop_payload);
        w->drop_payload = NULL;
        w->drop_payload_len = 0;
    }
    DestroyWindow(w->hwnd);
    win_free(w);
}

/* ============================================================
 * Drag-and-drop (external file drops via WM_DROPFILES)
 * ============================================================ */

bool wapi_plat_window_register_drop_target(wapi_plat_window_t* w) {
    if (!w || !w->hwnd) return false;
    DragAcceptFiles(w->hwnd, TRUE);
    w->drop_enabled = true;
    return true;
}

size_t wapi_plat_window_drop_payload(wapi_plat_window_t* w,
                                     char* out, size_t out_len,
                                     float* out_x, float* out_y)
{
    if (!w) return 0;
    if (out_x) *out_x = w->drop_point_x;
    if (out_y) *out_y = w->drop_point_y;
    if (w->drop_payload_len == 0 || !w->drop_payload) return 0;
    if (out && out_len > 0) {
        size_t copy = w->drop_payload_len < out_len ? w->drop_payload_len : out_len;
        memcpy(out, w->drop_payload, copy);
    }
    return w->drop_payload_len;
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

/* ============================================================
 * Touch / pen / hotkey accessors
 * ============================================================ */

bool wapi_plat_touch_available(void) { return g.touch_available; }

uint32_t wapi_plat_touch_max_fingers(void) {
    return g.touch_max_fingers ? g.touch_max_fingers : (uint32_t)WAPI_WIN32_MAX_FINGERS;
}

int wapi_plat_touch_finger_count(void) {
    int n = 0;
    for (int i = 0; i < WAPI_WIN32_MAX_FINGERS; i++) if (g.fingers[i].active) n++;
    return n;
}

bool wapi_plat_touch_get_finger(int index, int32_t* out_finger_idx,
                                float* out_x, float* out_y, float* out_pressure)
{
    int seen = 0;
    for (int i = 0; i < WAPI_WIN32_MAX_FINGERS; i++) {
        if (!g.fingers[i].active) continue;
        if (seen == index) {
            if (out_finger_idx) *out_finger_idx = g.fingers[i].finger_idx;
            if (out_x)          *out_x          = g.fingers[i].norm_x;
            if (out_y)          *out_y          = g.fingers[i].norm_y;
            if (out_pressure)   *out_pressure   = g.fingers[i].pressure;
            return true;
        }
        seen++;
    }
    return false;
}

bool wapi_plat_pen_available(void) { return g.fn_GetPointerPenInfo != NULL; }

bool wapi_plat_pen_get_info(uint32_t* out_tool_type, uint32_t* out_caps_mask) {
    if (out_tool_type) *out_tool_type = g.pen_tool;
    if (out_caps_mask) *out_caps_mask = g.pen_capabilities;
    return wapi_plat_pen_available();
}

bool wapi_plat_pen_get_axis(int axis, float* out_value) {
    if (!out_value) return false;
    switch (axis) {
    case 0: *out_value = g.pen_pressure; return true;
    case 1: *out_value = g.pen_tilt_x;   return true;
    case 2: *out_value = g.pen_tilt_y;   return true;
    case 3: *out_value = g.pen_twist;    return true;
    case 4: *out_value = g.pen_distance; return true;
    default: return false;
    }
}

bool wapi_plat_pen_get_position(float* out_x, float* out_y) {
    if (out_x) *out_x = g.pen_x;
    if (out_y) *out_y = g.pen_y;
    return wapi_plat_pen_available();
}

bool wapi_plat_hotkey_register(uint32_t id, uint32_t mod_mask, uint32_t hid_scancode) {
    if (hid_scancode == 0 || hid_scancode >= 256) return false;
    UINT vk = kHidToVk[hid_scancode];
    if (vk == 0) return false;

    /* Reuse existing slot if id is already registered (re-register). */
    int slot = -1;
    for (int i = 0; i < WAPI_WIN32_MAX_HOTKEYS; i++) {
        if (g.hotkeys[i].active && g.hotkeys[i].id == id) { slot = i; break; }
    }
    if (slot < 0) {
        for (int i = 0; i < WAPI_WIN32_MAX_HOTKEYS; i++) {
            if (!g.hotkeys[i].active) { slot = i; break; }
        }
    }
    if (slot < 0) return false;

    /* Unregister any prior binding on this slot before rebinding. */
    if (g.hotkeys[slot].active) {
        UnregisterHotKey(NULL, g.hotkeys[slot].atom_id);
        g.hotkeys[slot].active = false;
    }

    /* Pick a fresh atom id (1..0xBFFF per RegisterHotKey contract). */
    int atom = g.hotkeys_next_atom++;
    if (g.hotkeys_next_atom > 0xBFFE) g.hotkeys_next_atom = 1;

    UINT fs = wapi_mod_to_winmod(mod_mask) | MOD_NOREPEAT;
    if (!RegisterHotKey(NULL, atom, fs, vk)) return false;

    g.hotkeys[slot].active   = true;
    g.hotkeys[slot].id       = id;
    g.hotkeys[slot].atom_id  = atom;
    g.hotkeys[slot].mod_mask = mod_mask;
    g.hotkeys[slot].scancode = hid_scancode;
    return true;
}

void wapi_plat_hotkey_unregister(uint32_t id) {
    for (int i = 0; i < WAPI_WIN32_MAX_HOTKEYS; i++) {
        if (g.hotkeys[i].active && g.hotkeys[i].id == id) {
            UnregisterHotKey(NULL, g.hotkeys[i].atom_id);
            memset(&g.hotkeys[i], 0, sizeof(g.hotkeys[i]));
            return;
        }
    }
}
