/**
 * WAPI Desktop Runtime - Platform Abstraction
 *
 * Boundary between the cross-platform host modules (wapi_host_*.c)
 * and the per-platform backends (platform/win32, platform/cocoa,
 * platform/wayland). No SDL, no cross-platform framework:
 * each backend calls OS APIs directly.
 *
 * Data-first: all state returned by value or filled into caller
 * buffers. Opaque handles are pointers into per-backend pools.
 * Events are POD, fixed-size, memcpy-safe.
 */

#ifndef WAPI_PLAT_H
#define WAPI_PLAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Lifecycle
 * ============================================================ */

bool wapi_plat_init(void);
void wapi_plat_shutdown(void);

/* ============================================================
 * Clock
 * ============================================================ */

uint64_t wapi_plat_time_monotonic_ns(void);
uint64_t wapi_plat_time_realtime_ns(void);
uint64_t wapi_plat_perf_counter(void);
uint64_t wapi_plat_perf_frequency(void);
uint64_t wapi_plat_time_resolution_monotonic_ns(void);
void     wapi_plat_sleep_ns(uint64_t ns);
void     wapi_plat_yield(void);

/* ============================================================
 * Window
 *
 * Windows carry a stable uint32_t id (surface_id) that host
 * modules use to correlate events with WAPI surface handles.
 * The id is assigned at create time and does not reuse the
 * handle table; host modules map id -> handle in their own
 * table.
 * ============================================================ */

typedef struct wapi_plat_window_t wapi_plat_window_t; /* opaque */

/* Window creation flags (bitfield, ABI-matched to wapi_surface.h) */
#define WAPI_PLAT_WIN_RESIZABLE     0x0001
#define WAPI_PLAT_WIN_BORDERLESS    0x0002
#define WAPI_PLAT_WIN_FULLSCREEN    0x0004
#define WAPI_PLAT_WIN_HIDDEN        0x0008
#define WAPI_PLAT_WIN_HIGH_DPI      0x0010
#define WAPI_PLAT_WIN_ALWAYS_ON_TOP 0x0020
#define WAPI_PLAT_WIN_TRANSPARENT   0x0040

typedef struct wapi_plat_window_desc_t {
    const char* title;      /* utf8, NUL-terminated */
    int32_t     width;      /* logical pixels; <=0 -> 1280 */
    int32_t     height;     /* logical pixels; <=0 -> 720  */
    uint32_t    flags;
} wapi_plat_window_desc_t;

/* Returns NULL on failure. Window is shown unless HIDDEN is set. */
wapi_plat_window_t* wapi_plat_window_create(const wapi_plat_window_desc_t* desc);
void                wapi_plat_window_destroy(wapi_plat_window_t* w);
uint32_t            wapi_plat_window_id(wapi_plat_window_t* w);

void  wapi_plat_window_get_size_pixels (wapi_plat_window_t* w, int32_t* out_w, int32_t* out_h);
void  wapi_plat_window_get_size_logical(wapi_plat_window_t* w, int32_t* out_w, int32_t* out_h);
float wapi_plat_window_get_dpi_scale   (wapi_plat_window_t* w);
void  wapi_plat_window_set_size        (wapi_plat_window_t* w, int32_t width, int32_t height);
void  wapi_plat_window_set_title       (wapi_plat_window_t* w, const char* title, size_t len);
void  wapi_plat_window_set_fullscreen  (wapi_plat_window_t* w, bool fullscreen);
void  wapi_plat_window_set_visible     (wapi_plat_window_t* w, bool visible);
void  wapi_plat_window_minimize        (wapi_plat_window_t* w);
void  wapi_plat_window_maximize        (wapi_plat_window_t* w);
void  wapi_plat_window_restore         (wapi_plat_window_t* w);

/* Cursors (enum matches wapi_surface.h cursor IDs, 0..13) */
#define WAPI_PLAT_CURSOR_DEFAULT     0
#define WAPI_PLAT_CURSOR_POINTER     1
#define WAPI_PLAT_CURSOR_TEXT        2
#define WAPI_PLAT_CURSOR_CROSSHAIR   3
#define WAPI_PLAT_CURSOR_MOVE        4
#define WAPI_PLAT_CURSOR_RESIZE_NS   5
#define WAPI_PLAT_CURSOR_RESIZE_EW   6
#define WAPI_PLAT_CURSOR_RESIZE_NWSE 7
#define WAPI_PLAT_CURSOR_RESIZE_NESW 8
#define WAPI_PLAT_CURSOR_NOT_ALLOWED 9
#define WAPI_PLAT_CURSOR_WAIT        10
#define WAPI_PLAT_CURSOR_GRAB        11
#define WAPI_PLAT_CURSOR_GRABBING    12
#define WAPI_PLAT_CURSOR_HIDDEN      13

void wapi_plat_window_set_cursor        (wapi_plat_window_t* w, uint32_t cursor_id);
void wapi_plat_window_set_relative_mouse(wapi_plat_window_t* w, bool enable);
void wapi_plat_window_start_text_input  (wapi_plat_window_t* w);
void wapi_plat_window_stop_text_input   (wapi_plat_window_t* w);

/* Teleport the mouse cursor to (x, y) in window-client logical coordinates.
 * Returns false if the window is not a valid backend-owned window. */
bool wapi_plat_window_warp_mouse        (wapi_plat_window_t* w, float x, float y);

/* Install a custom cursor on this window from a premultiplied RGBA8 image.
 * hot_x / hot_y are measured in image pixels. The cursor becomes effective
 * on the next WM_SETCURSOR and is replaced by wapi_plat_window_set_cursor.
 * Returns false if the image is invalid or creation failed. */
bool wapi_plat_window_set_cursor_image  (wapi_plat_window_t* w,
                                         const void* rgba, int32_t w_px, int32_t h_px,
                                         int32_t hot_x, int32_t hot_y);

/* ---- IME composition control / query ----
 *
 * The backend maintains a side-store of the active preedit for each
 * window. `wapi_plat_window_ime_get_preedit` fills `buf` with up to
 * `buf_len` bytes of UTF-8 preedit text and reports the full length,
 * caret byte offset, and segment count. Returns true when a live
 * preedit exists.
 *
 * `wapi_plat_window_ime_get_segment` reads one segment descriptor
 * (start byte, byte length, WAPI_IME_SEG_* flags) at `index`. Flags
 * use the same bitmap as wapi_input.h (RAW/CONVERTED/SELECTED/TARGET).
 *
 * `wapi_plat_window_ime_set_candidate_rect` anchors the platform IME
 * candidate window (relative to the client rect). `wapi_plat_window_ime_force_commit`
 * finalizes any in-flight composition (host will see an IME_COMMIT
 * path through the normal event stream). `_force_cancel` drops it
 * and emits an IME_CANCEL.
 */
bool wapi_plat_window_ime_get_preedit     (wapi_plat_window_t* w,
                                           char* buf, uint32_t buf_len,
                                           uint32_t* out_byte_len,
                                           uint32_t* out_cursor_bytes,
                                           uint32_t* out_segment_count);
bool wapi_plat_window_ime_get_segment     (wapi_plat_window_t* w, uint32_t index,
                                           uint32_t* out_start, uint32_t* out_length,
                                           uint32_t* out_flags);
void wapi_plat_window_ime_set_candidate_rect(wapi_plat_window_t* w,
                                             int32_t x, int32_t y,
                                             int32_t w_px, int32_t h_px);
void wapi_plat_window_ime_force_commit    (wapi_plat_window_t* w);
void wapi_plat_window_ime_force_cancel    (wapi_plat_window_t* w);

/* ============================================================
 * Native handle (for WebGPU surface creation)
 * ============================================================ */

#define WAPI_PLAT_NATIVE_NONE    0
#define WAPI_PLAT_NATIVE_WIN32   1 /* a=HWND      b=HINSTANCE  */
#define WAPI_PLAT_NATIVE_COCOA   2 /* a=CAMetalLayer*          */
#define WAPI_PLAT_NATIVE_WAYLAND 3 /* a=wl_display* b=wl_surface* */

typedef struct wapi_plat_native_handle_t {
    int   kind;
    void* a;
    void* b;
} wapi_plat_native_handle_t;

bool wapi_plat_window_get_native(wapi_plat_window_t* w,
                                 wapi_plat_native_handle_t* out);

/* ============================================================
 * Displays
 * ============================================================ */

int  wapi_plat_display_count(void);
bool wapi_plat_display_info(int index,
                            int32_t* out_w, int32_t* out_h,
                            int32_t* out_hz);

/* ============================================================
 * Events
 *
 * Fixed-size POD. Event payload is a tagged union.
 * Each variant is <= 48 bytes so the whole struct is 64 bytes
 * and 16-byte aligned: cache-friendly, memcpy-safe.
 * ============================================================ */

enum {
    /* Lifecycle */
    WAPI_PLAT_EV_QUIT              = 1,

    /* Window (surface) */
    WAPI_PLAT_EV_WINDOW_CLOSE      = 10,
    WAPI_PLAT_EV_WINDOW_RESIZE     = 11,
    WAPI_PLAT_EV_WINDOW_FOCUS      = 12,
    WAPI_PLAT_EV_WINDOW_BLUR       = 13,
    WAPI_PLAT_EV_WINDOW_SHOW       = 14,
    WAPI_PLAT_EV_WINDOW_HIDE       = 15,
    WAPI_PLAT_EV_WINDOW_MIN        = 16,
    WAPI_PLAT_EV_WINDOW_MAX        = 17,
    WAPI_PLAT_EV_WINDOW_RESTORE    = 18,
    WAPI_PLAT_EV_WINDOW_MOVE       = 19,

    /* Keyboard */
    WAPI_PLAT_EV_KEY_DOWN          = 30,
    WAPI_PLAT_EV_KEY_UP            = 31,
    WAPI_PLAT_EV_TEXT_INPUT        = 32,

    /* IME composition (preedit lifecycle; commits flow as TEXT_INPUT) */
    WAPI_PLAT_EV_IME_START         = 33,
    WAPI_PLAT_EV_IME_UPDATE        = 34,
    WAPI_PLAT_EV_IME_CANCEL        = 35,

    /* Mouse */
    WAPI_PLAT_EV_MOUSE_MOTION      = 40,
    WAPI_PLAT_EV_MOUSE_DOWN        = 41,
    WAPI_PLAT_EV_MOUSE_UP          = 42,
    WAPI_PLAT_EV_MOUSE_WHEEL       = 43,

    /* Touch */
    WAPI_PLAT_EV_FINGER_DOWN       = 50,
    WAPI_PLAT_EV_FINGER_UP         = 51,
    WAPI_PLAT_EV_FINGER_MOTION     = 52,

    /* Pen / stylus */
    WAPI_PLAT_EV_PEN_DOWN          = 60,
    WAPI_PLAT_EV_PEN_UP            = 61,
    WAPI_PLAT_EV_PEN_MOTION        = 62,

    /* Gamepad */
    WAPI_PLAT_EV_GPAD_ADDED        = 70,
    WAPI_PLAT_EV_GPAD_REMOVED      = 71,
    WAPI_PLAT_EV_GPAD_AXIS         = 72,
    WAPI_PLAT_EV_GPAD_BUTTON_DOWN  = 73,
    WAPI_PLAT_EV_GPAD_BUTTON_UP    = 74,

    /* Hotkey */
    WAPI_PLAT_EV_HOTKEY            = 80,

    /* Menu / tray */
    WAPI_PLAT_EV_MENU_SELECT       = 90,
    WAPI_PLAT_EV_TRAY_CLICK        = 91,
    WAPI_PLAT_EV_TRAY_MENU         = 92,

    /* File drop (external drag-and-drop) */
    WAPI_PLAT_EV_DROP_FILES        = 100,
};

typedef struct wapi_plat_ev_resize_t  { int32_t w, h; }                                        wapi_plat_ev_resize_t;
typedef struct wapi_plat_ev_move_t    { int32_t x, y; }                                        wapi_plat_ev_move_t;
typedef struct wapi_plat_ev_key_t     {
    uint32_t scancode;   /* USB HID */
    uint32_t keycode;    /* virtual key */
    uint16_t mod;
    uint8_t  down;
    uint8_t  repeat;
    uint8_t  _pad[4];
}                                                                                              wapi_plat_ev_key_t;
typedef struct wapi_plat_ev_text_t    { char text[32]; }                                       wapi_plat_ev_text_t;
typedef struct wapi_plat_ev_motion_t  {
    uint32_t mouse_id;
    uint32_t button_state;
    float    x, y;
    float    xrel, yrel;
    uint8_t  _pad[16];
}                                                                                              wapi_plat_ev_motion_t;
typedef struct wapi_plat_ev_button_t  {
    uint32_t mouse_id;
    uint8_t  button;
    uint8_t  down;
    uint8_t  clicks;
    uint8_t  _pad0;
    float    x, y;
    uint8_t  _pad[24];
}                                                                                              wapi_plat_ev_button_t;
typedef struct wapi_plat_ev_wheel_t   {
    uint32_t mouse_id;
    uint32_t _pad0;
    float    x, y;
    uint8_t  _pad[32];
}                                                                                              wapi_plat_ev_wheel_t;
typedef struct wapi_plat_ev_touch_t   {
    uint64_t touch_id;
    uint64_t finger_id;
    float    x, y;
    float    dx, dy;
    float    pressure;
    uint8_t  _pad[4];
}                                                                                              wapi_plat_ev_touch_t;
typedef struct wapi_plat_ev_pen_t     {
    uint32_t pen_id;
    uint8_t  tool_type;   /* 0=pen, 1=eraser */
    uint8_t  button;
    uint8_t  _pad0[2];
    float    x, y;
    float    pressure;
    float    tilt_x, tilt_y;
    float    twist;
    float    distance;
    uint8_t  _pad[8];
}                                                                                              wapi_plat_ev_pen_t;
typedef struct wapi_plat_ev_gpad_t    {
    uint32_t gamepad_id;
    uint8_t  axis;            /* for AXIS events */
    uint8_t  button;          /* for BUTTON events */
    uint8_t  _pad0[2];
    int16_t  axis_value;      /* for AXIS events */
    uint8_t  _pad[34];
}                                                                                              wapi_plat_ev_gpad_t;
typedef struct wapi_plat_ev_hotkey_t  {
    uint32_t hotkey_id;
    uint8_t  _pad[44];
}                                                                                              wapi_plat_ev_hotkey_t;
typedef struct wapi_plat_ev_menu_t    {
    uint32_t menu_token;    /* host-assigned id bound to a menu handle */
    uint32_t item_id;       /* guest-provided id for the item */
    uint8_t  _pad[40];
}                                                                                              wapi_plat_ev_menu_t;
typedef struct wapi_plat_ev_tray_t    {
    uint32_t tray_token;    /* host-assigned id bound to a tray handle */
    uint32_t item_id;       /* 0 for click, non-zero for menu item id */
    uint8_t  _pad[40];
}                                                                                              wapi_plat_ev_tray_t;
typedef struct wapi_plat_ev_drop_t    {
    float    x, y;          /* client-pixel drop point */
    uint32_t payload_bytes; /* queryable via wapi_plat_window_drop_payload */
    uint8_t  _pad[36];
}                                                                                              wapi_plat_ev_drop_t;

typedef struct wapi_plat_event_t {
    uint32_t type;           /* WAPI_PLAT_EV_* */
    uint32_t window_id;      /* 0 if not window-bound */
    uint64_t timestamp_ns;
    union {
        wapi_plat_ev_resize_t resize;
        wapi_plat_ev_move_t   move;
        wapi_plat_ev_key_t    key;
        wapi_plat_ev_text_t   text;
        wapi_plat_ev_motion_t motion;
        wapi_plat_ev_button_t button;
        wapi_plat_ev_wheel_t  wheel;
        wapi_plat_ev_touch_t  touch;
        wapi_plat_ev_pen_t    pen;
        wapi_plat_ev_gpad_t   gpad;
        wapi_plat_ev_hotkey_t hotkey;
        wapi_plat_ev_menu_t   menu;
        wapi_plat_ev_tray_t   tray;
        wapi_plat_ev_drop_t   drop;
        uint8_t raw[48];
    } u;
} wapi_plat_event_t;

/* Drain up to `max` events into `out`. Returns count drained.
 * Non-blocking. */
int wapi_plat_poll_events(wapi_plat_event_t* out, int max);

/* Block until at least one platform event is pending, or the
 * timeout expires, or wapi_plat_wake() is called from another
 * thread/callback. timeout_ns < 0 means wait forever. */
void wapi_plat_wait_events(int64_t timeout_ns);

/* Inject a wake from any thread (used by I/O completions to
 * break a wait_events blocking call). Cheap if already awake. */
void wapi_plat_wake(void);

/* Direct state polls (match SDL's snapshot semantics) */
bool     wapi_plat_key_pressed(uint32_t scancode);
uint16_t wapi_plat_mod_state(void);
void     wapi_plat_mouse_state(float* out_x, float* out_y, uint32_t* out_buttons);

/* ---- Touch snapshot accessors ----
 *
 * Backend tracks active fingers in its event-pump loop; the host
 * exposes one aggregate touch device that reports device-native
 * normalized (0..1) positions. `finger_index` is a stable slot id
 * from the backend's tracking table (NOT the OS-native id, which
 * has no bound). Positions are in the same coordinate frame the
 * `WAPI_PLAT_EV_FINGER_*` events carried. */
/* ---- ROUTED transfer (system share) ----
 *
 * Hand `data` off to the OS share sheet. Returns true if the share
 * UI was invoked successfully (does NOT signal that the user picked
 * a target — the share sheet is fire-and-forget and completes
 * asynchronously on the platform side). `mime` matches the LATENT
 * MIME set: text/plain, image/bmp, text/uri-list.
 *
 * The `title` is a short label shown in the share picker when the
 * platform supports it. The parent window is used to position the
 * share sheet. */
bool wapi_plat_share_data(wapi_plat_window_t* parent,
                          const char* mime, size_t mime_len,
                          const void* data, size_t data_len,
                          const char* title, size_t title_len);

bool     wapi_plat_touch_available     (void);
uint32_t wapi_plat_touch_max_fingers   (void);
int      wapi_plat_touch_finger_count  (void);
/* Returns true if slot `index` (0..finger_count-1) is live and fills
 * out_finger_idx / out_x / out_y / out_pressure. Any out pointer may
 * be NULL. Positions are normalized 0..1 for the aggregate query
 * path (no surface context). */
bool     wapi_plat_touch_get_finger    (int index,
                                        int32_t* out_finger_idx,
                                        float* out_x, float* out_y,
                                        float* out_pressure);

/* ---- Pen snapshot accessors ----
 *
 * Backend tracks the currently-active pen (if any) from WM_POINTER* /
 * equivalent. Position is last known surface-pixel coordinates.
 * `capabilities_mask` is a bitmask of wapi_pen_axis_t values the
 * platform reports. */
bool     wapi_plat_pen_available       (void);
bool     wapi_plat_pen_get_info        (uint32_t* out_tool_type,
                                        uint32_t* out_capabilities_mask);
bool     wapi_plat_pen_get_axis        (int axis, float* out_value);
bool     wapi_plat_pen_get_position    (float* out_x, float* out_y);

/* ---- Raw HID ----
 *
 * Backend enumerates connected HID devices. Guests filter and
 * (implicitly) request a specific device via
 * `wapi_plat_hid_request_device`; once granted, the host allocates
 * a tracked handle and exposes send/receive/feature report I/O.
 *
 * Devices are identified by a stable 16-byte uid derived from
 * SetupDi device instance path so reconnect is detectable. */
typedef struct wapi_plat_hid_device_t wapi_plat_hid_device_t; /* opaque */

typedef struct wapi_plat_hid_info_t {
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t usage_page;
    uint16_t usage;
    uint16_t version;
    uint8_t  uid[16];
    char     name[128];
    uint32_t max_input_report;
    uint32_t max_output_report;
    uint32_t max_feature_report;
} wapi_plat_hid_info_t;

/* Enumerate connected HID devices matching optional filters (0 = any).
 * Returns the total count of matching devices; fills up to `max`
 * entries into `out`. Safe to call with out=NULL to probe count. */
int wapi_plat_hid_enumerate(uint16_t vendor_filter,
                            uint16_t product_filter,
                            uint16_t usage_page_filter,
                            wapi_plat_hid_info_t* out,
                            int max);

/* Open an enumerated device by its uid (from wapi_plat_hid_info_t).
 * Returns NULL on failure (device gone, access denied). */
wapi_plat_hid_device_t* wapi_plat_hid_open(const uint8_t uid[16]);
void                    wapi_plat_hid_close(wapi_plat_hid_device_t* d);
bool                    wapi_plat_hid_get_info(wapi_plat_hid_device_t* d,
                                               wapi_plat_hid_info_t* out);

/* Blocking read of an input report (report_id prefix if numbered
 * reports are used). Returns bytes written, or <0 on error. */
int  wapi_plat_hid_read_report(wapi_plat_hid_device_t* d,
                               void* buf, int buf_len, int timeout_ms);
/* Write an output report. `data` must be prefixed with the report_id
 * byte (0 for non-numbered). Returns bytes written, or <0 on error. */
int  wapi_plat_hid_write_report(wapi_plat_hid_device_t* d,
                                const void* data, int len);
int  wapi_plat_hid_send_feature(wapi_plat_hid_device_t* d,
                                const void* data, int len);
int  wapi_plat_hid_get_feature (wapi_plat_hid_device_t* d,
                                void* buf, int buf_len);

/* ---- Native menus ----
 *
 * Backend stores an HMENU (Win32) / NSMenu (Cocoa) / Wayland
 * equivalent per handle. `token` is a short opaque id the host
 * assigns when binding a menu; the backend uses it as the WM_COMMAND
 * tag prefix so click dispatch knows which menu an item came from.
 *
 * Menu items share a 32-bit id space per menu; [16, 0xBFFF] is the
 * safe RegisterHotKey-free range on Win32. Submenus are added as
 * popups whose items carry the parent token so WM_COMMAND routing
 * still resolves. Flags mirror WAPI_MENU_* (separator / disabled /
 * checked / submenu). */
typedef struct wapi_plat_menu_t wapi_plat_menu_t; /* opaque */

wapi_plat_menu_t* wapi_plat_menu_create(uint32_t token);
void              wapi_plat_menu_destroy(wapi_plat_menu_t* m);
bool              wapi_plat_menu_add_item(wapi_plat_menu_t* m, uint32_t id,
                                          const char* utf8, size_t label_len,
                                          uint32_t flags);
bool              wapi_plat_menu_add_submenu(wapi_plat_menu_t* m,
                                             const char* utf8, size_t label_len,
                                             wapi_plat_menu_t* submenu);

/* Show context menu at screen-space pixel coordinates (converted by
 * caller from surface coords). Blocks until a choice is made / menu
 * is dismissed; selections arrive as WAPI_PLAT_EV_MENU_SELECT. */
bool              wapi_plat_menu_show_context(wapi_plat_menu_t* m,
                                              wapi_plat_window_t* w,
                                              int32_t x, int32_t y);

/* Attach a menu as the window's menubar (Win32 SetMenu; NSApp main
 * menu on macOS). Pass NULL to remove. */
bool              wapi_plat_menu_set_bar(wapi_plat_window_t* w,
                                         wapi_plat_menu_t* m);

/* ---- Taskbar / dock ----
 *
 * Thin pass-through to platform taskbar integration. `state` uses
 * the WAPI_TASKBAR_PROGRESS_* enum (0=none,1=indeterminate,2=normal,
 * 3=error,4=paused). `value` is 0..1 and ignored for none/indeterm. */
bool wapi_plat_taskbar_set_progress     (wapi_plat_window_t* w, int state, float value);
bool wapi_plat_taskbar_request_attention(wapi_plat_window_t* w, bool critical);
bool wapi_plat_taskbar_set_overlay      (wapi_plat_window_t* w,
                                         const void* icon, size_t icon_len,
                                         const char* desc, size_t desc_len);
bool wapi_plat_taskbar_clear_overlay    (wapi_plat_window_t* w);

/* ---- Notifications (balloon) ----
 *
 * Show a balloon notification. Returns a non-zero id on success that
 * the guest can later pass to wapi_plat_notify_close. urgency mirrors
 * wapi_notify_urgency_t (0=low,1=normal,2=high). */
uint32_t wapi_plat_notify_show (const char* title, size_t title_len,
                                const char* body,  size_t body_len,
                                int urgency);
bool     wapi_plat_notify_close(uint32_t id);

/* ---- System tray ----
 *
 * Backend manages a hidden message-only window that receives tray
 * notifications. `token` identifies the tray icon in events. The
 * icon and tooltip live in the backend; the caller owns the image
 * bytes only for the duration of the call. */
typedef struct wapi_plat_tray_t wapi_plat_tray_t; /* opaque */

wapi_plat_tray_t* wapi_plat_tray_create(uint32_t token,
                                        const void* icon_bytes, size_t icon_len,
                                        const char* tooltip, size_t tooltip_len);
void              wapi_plat_tray_destroy(wapi_plat_tray_t* t);
bool              wapi_plat_tray_set_icon(wapi_plat_tray_t* t,
                                          const void* icon_bytes, size_t icon_len);
bool              wapi_plat_tray_set_tooltip(wapi_plat_tray_t* t,
                                             const char* tooltip, size_t len);
/* Install a context menu for right-click. NULL clears it. */
bool              wapi_plat_tray_set_menu(wapi_plat_tray_t* t,
                                          wapi_plat_menu_t* menu);

/* ---- Hotkeys ----
 *
 * Register a process-global keyboard hotkey. `hid_scancode` is the
 * HID Usage Page 0x07 scancode (wapi_scancode_t). `mod_mask` is the
 * WAPI_KMOD_* bitmask. On fire, backend emits a
 * WAPI_PLAT_EV_HOTKEY event carrying `id`. Returns false if the
 * platform cannot bind (unknown scancode, id clash, system
 * reservation). */
bool     wapi_plat_hotkey_register     (uint32_t id, uint32_t mod_mask,
                                        uint32_t hid_scancode);
void     wapi_plat_hotkey_unregister   (uint32_t id);

/* Gamepad snapshot accessors — backend tracks per-slot state in its
 * event-pump loop. `slot` is 0..N-1 (XInput slots 0..3 on Win32). */
bool     wapi_plat_gamepad_connected       (uint32_t slot);
bool     wapi_plat_gamepad_button_pressed  (uint32_t slot, uint8_t button);
int16_t  wapi_plat_gamepad_axis_value      (uint32_t slot, uint8_t axis);
bool     wapi_plat_gamepad_rumble          (uint32_t slot,
                                            uint16_t low_freq, uint16_t high_freq,
                                            uint32_t duration_ms);
/* Returns 0..100, or 255 if unknown. */
uint8_t  wapi_plat_gamepad_battery_percent (uint32_t slot);

/* ============================================================
 * Clipboard (text/plain only for now)
 * ============================================================ */

bool   wapi_plat_clipboard_has_text(void);
bool   wapi_plat_clipboard_set_text(const char* utf8, size_t len);

/* Fills `out` with up to out_len bytes (no NUL). Returns the
 * total length of the clipboard text in bytes regardless of
 * truncation (so callers can size correctly). Returns 0 if
 * empty. */
size_t wapi_plat_clipboard_get_text(char* out, size_t out_len);

/* Image clipboard — BMP file bytes. Backed by CF_DIB on Win32 with
 * the BITMAPFILEHEADER prepended when reading, stripped when
 * writing. The guest-facing MIME is image/bmp. */
bool   wapi_plat_clipboard_has_image(void);
bool   wapi_plat_clipboard_set_image(const void* bmp_file, size_t len);
/* Fills `out` with up to out_len bytes. Returns the true total BMP
 * size regardless of truncation (so callers can size correctly).
 * Returns 0 when no image is on the clipboard. */
size_t wapi_plat_clipboard_get_image(void* out, size_t out_len);

/* File clipboard — a text/uri-list of file:// URIs separated by CRLF
 * (one URI per dropped/copied file). Backed by CF_HDROP on Win32. */
bool   wapi_plat_clipboard_has_files(void);
/* Fills `out` with up to out_len bytes. Returns the true total byte
 * length of the URI list regardless of truncation. Returns 0 when
 * no files are on the clipboard. */
size_t wapi_plat_clipboard_get_files(char* out, size_t out_len);

/* Opt a window into WM_DROPFILES so dragging external files onto
 * the surface fires a drop event carrying a text/uri-list payload.
 * No-op on backends that don't support file drops. */
bool   wapi_plat_window_register_drop_target(wapi_plat_window_t* w);

/* Blocking read of the last-delivered drop payload. The backend
 * stashes the URI list between WM_DROPFILES and the guest's next
 * io.poll; hosts call this from the drop event translator. Returns
 * the URI-list length in bytes, or 0 if no drop is pending. */
size_t wapi_plat_window_drop_payload(wapi_plat_window_t* w,
                                     char* out, size_t out_len,
                                     float* out_x, float* out_y);

/* ============================================================
 * Audio (WASAPI / CoreAudio / PipeWire)
 *
 * Deliberately modelled on SDL3's stream+device split because
 * it's the proven shape and maps cleanly onto WAPI's ABI. The
 * backend handles format conversion / resampling internally
 * where the platform supports it; otherwise the stream does a
 * simple linear-interp resample + sample-format convert.
 * ============================================================ */

enum {
    WAPI_PLAT_AUDIO_U8  = 0x0008,
    WAPI_PLAT_AUDIO_S16 = 0x8010,
    WAPI_PLAT_AUDIO_S32 = 0x8020,
    WAPI_PLAT_AUDIO_F32 = 0x8120,
};

typedef struct wapi_plat_audio_spec_t {
    uint32_t format;
    int32_t  channels;
    int32_t  freq;
    int32_t  _pad;
} wapi_plat_audio_spec_t;

typedef struct wapi_plat_audio_device_t wapi_plat_audio_device_t;
typedef struct wapi_plat_audio_stream_t wapi_plat_audio_stream_t;

#define WAPI_PLAT_AUDIO_DEFAULT_PLAYBACK   (-1)
#define WAPI_PLAT_AUDIO_DEFAULT_RECORDING  (-2)

/* Device enumeration. index < 0 selects defaults. */
int  wapi_plat_audio_playback_device_count(void);
int  wapi_plat_audio_recording_device_count(void);
size_t wapi_plat_audio_device_name(int device_id, char* out, size_t out_len);

wapi_plat_audio_device_t* wapi_plat_audio_open_device (int device_id,
                                                       const wapi_plat_audio_spec_t* spec);
void                      wapi_plat_audio_close_device(wapi_plat_audio_device_t* d);
void                      wapi_plat_audio_pause_device(wapi_plat_audio_device_t* d);
void                      wapi_plat_audio_resume_device(wapi_plat_audio_device_t* d);

wapi_plat_audio_stream_t* wapi_plat_audio_stream_create(const wapi_plat_audio_spec_t* src,
                                                        const wapi_plat_audio_spec_t* dst);
void                      wapi_plat_audio_stream_destroy(wapi_plat_audio_stream_t* s);

bool wapi_plat_audio_stream_bind  (wapi_plat_audio_device_t* d, wapi_plat_audio_stream_t* s);
void wapi_plat_audio_stream_unbind(wapi_plat_audio_stream_t* s);

/* Convenience: open a device + stream in one call. Returns false on failure. */
bool wapi_plat_audio_open_device_stream(int device_id,
                                        const wapi_plat_audio_spec_t* spec,
                                        wapi_plat_audio_device_t** out_dev,
                                        wapi_plat_audio_stream_t** out_stream);

bool wapi_plat_audio_stream_put (wapi_plat_audio_stream_t* s, const void* data, int len);
int  wapi_plat_audio_stream_get (wapi_plat_audio_stream_t* s, void* data, int len);
int  wapi_plat_audio_stream_available(wapi_plat_audio_stream_t* s);
int  wapi_plat_audio_stream_queued   (wapi_plat_audio_stream_t* s);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_PLAT_H */
