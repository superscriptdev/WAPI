/**
 * WAPI - Event Types and Structures
 * Version 1.0.0
 *
 * Defines the event types, per-event structs, and the 128-byte event
 * union for all platform events: input, lifecycle, I/O completions,
 * device changes, drag-and-drop, and more.
 *
 * Event delivery is through the wapi_io_t vtable (ctx->io->poll /
 * ctx->io->wait). There is no separate "wapi_event" import namespace.
 * All events -- whether initiated by the module (I/O completions) or
 * pushed by the host (input, lifecycle) -- arrive through the same
 * poll/wait interface on the I/O vtable.
 *
 * @see wapi_io_t in wapi_context.h
 */

#ifndef WAPI_EVENT_H
#define WAPI_EVENT_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Event Types
 * ============================================================
 * Event type ranges follow SDL3 conventions for familiarity.
 */

typedef enum wapi_event_type_t {
    WAPI_EVENT_NONE = 0,

    /* Application lifecycle (0x100-0x1FF) */
    WAPI_EVENT_QUIT             = 0x100,
    WAPI_EVENT_TERMINATING      = 0x101,
    WAPI_EVENT_LOW_MEMORY       = 0x102,
    WAPI_EVENT_WILL_ENTER_BG    = 0x103,
    WAPI_EVENT_DID_ENTER_BG     = 0x104,
    WAPI_EVENT_WILL_ENTER_FG    = 0x105,
    WAPI_EVENT_DID_ENTER_FG     = 0x106,

    /* Surface events (0x200-0x20F) -- see wapi_surface.h */
    WAPI_EVENT_SURFACE_RESIZED      = 0x0200,
    WAPI_EVENT_SURFACE_DPI_CHANGED  = 0x020A,

    /* Window events (0x210-0x2FF) -- see wapi_window.h */
    WAPI_EVENT_WINDOW_CLOSE         = 0x0210,
    WAPI_EVENT_WINDOW_FOCUS_GAINED  = 0x0211,
    WAPI_EVENT_WINDOW_FOCUS_LOST    = 0x0212,
    WAPI_EVENT_WINDOW_SHOWN         = 0x0213,
    WAPI_EVENT_WINDOW_HIDDEN        = 0x0214,
    WAPI_EVENT_WINDOW_MINIMIZED     = 0x0215,
    WAPI_EVENT_WINDOW_MAXIMIZED     = 0x0216,
    WAPI_EVENT_WINDOW_RESTORED      = 0x0217,
    WAPI_EVENT_WINDOW_MOVED         = 0x0218,

    /* Keyboard events (0x300-0x3FF) */
    WAPI_EVENT_KEY_DOWN         = 0x300,
    WAPI_EVENT_KEY_UP           = 0x301,
    WAPI_EVENT_TEXT_INPUT        = 0x302,
    WAPI_EVENT_TEXT_EDITING      = 0x303,

    /* Mouse events (0x400-0x4FF) */
    WAPI_EVENT_MOUSE_MOTION     = 0x400,
    WAPI_EVENT_MOUSE_BUTTON_DOWN = 0x401,
    WAPI_EVENT_MOUSE_BUTTON_UP  = 0x402,
    WAPI_EVENT_MOUSE_WHEEL      = 0x403,

    /* Input device lifecycle events (0x500-0x5FF) */
    WAPI_EVENT_DEVICE_ADDED     = 0x500,
    WAPI_EVENT_DEVICE_REMOVED   = 0x501,

    /* Touch events (0x700-0x7FF) */
    WAPI_EVENT_TOUCH_DOWN       = 0x700,
    WAPI_EVENT_TOUCH_UP         = 0x701,
    WAPI_EVENT_TOUCH_MOTION     = 0x702,
    WAPI_EVENT_TOUCH_CANCELED   = 0x703,

    /* Gamepad events (0x650-0x6FF) */
    WAPI_EVENT_GAMEPAD_AXIS           = 0x652,
    WAPI_EVENT_GAMEPAD_BUTTON_DOWN    = 0x653,
    WAPI_EVENT_GAMEPAD_BUTTON_UP      = 0x654,
    WAPI_EVENT_GAMEPAD_SENSOR         = 0x655,
    WAPI_EVENT_GAMEPAD_TOUCHPAD_DOWN  = 0x656,
    WAPI_EVENT_GAMEPAD_TOUCHPAD_UP    = 0x657,
    WAPI_EVENT_GAMEPAD_TOUCHPAD_MOTION = 0x658,
    WAPI_EVENT_GAMEPAD_REMAPPED       = 0x659,

    /* Touch gesture events (0x750-0x7FF) */
    WAPI_EVENT_GESTURE          = 0x750,

    /* Pen/stylus events (0x800-0x8FF) */
    WAPI_EVENT_PEN_DOWN            = 0x800,
    WAPI_EVENT_PEN_UP              = 0x801,
    WAPI_EVENT_PEN_MOTION          = 0x802,
    WAPI_EVENT_PEN_PROXIMITY_IN    = 0x803,
    WAPI_EVENT_PEN_PROXIMITY_OUT   = 0x804,
    WAPI_EVENT_PEN_BUTTON_DOWN     = 0x805,
    WAPI_EVENT_PEN_BUTTON_UP       = 0x806,

    /* Drop events (0x1000-0x10FF) */
    WAPI_EVENT_DROP_FILE        = 0x1000,
    WAPI_EVENT_DROP_TEXT        = 0x1001,
    WAPI_EVENT_DROP_BEGIN       = 0x1002,
    WAPI_EVENT_DROP_COMPLETE    = 0x1003,

    /* Display events (0x1800-0x18FF) */
    WAPI_EVENT_DISPLAY_ADDED    = 0x1800,
    WAPI_EVENT_DISPLAY_REMOVED  = 0x1801,
    WAPI_EVENT_DISPLAY_CHANGED  = 0x1802,

    /* Theme events (0x1810-0x181F) */
    WAPI_EVENT_THEME_CHANGED    = 0x1810,

    /* System tray events (0x1820-0x182F) */
    WAPI_EVENT_TRAY_CLICK       = 0x1820,
    WAPI_EVENT_TRAY_MENU        = 0x1821,

    /* Global hotkey events (0x1830-0x183F) */
    WAPI_EVENT_HOTKEY           = 0x1830,

    /* File watcher events (0x1840-0x184F) */
    WAPI_EVENT_FILE_CHANGED     = 0x1840,

    /* Menu events (0x1850-0x185F) */
    WAPI_EVENT_MENU_SELECT      = 0x1850,

    /* Content tree events (0x1900-0x190F) */
    WAPI_EVENT_CONTENT_ACTIVATE     = 0x1900,  /* User activated a node (Enter/click) */
    WAPI_EVENT_CONTENT_FOCUS        = 0x1901,  /* Keyboard focus moved to a node (Tab) */
    WAPI_EVENT_CONTENT_VALUE_CHANGE = 0x1902,  /* User changed a node's value */

    /* I/O completion events (0x2000-0x20FF) — all async ops */
    WAPI_EVENT_IO_COMPLETION    = 0x2000,

    /* User events (0x8000-0xFFFF) */
    WAPI_EVENT_USER             = 0x8000,

    WAPI_EVENT_FORCE32          = 0x7FFFFFFF
} wapi_event_type_t;

/* ============================================================
 * Scan Codes (Physical Keys)
 * ============================================================
 * Based on USB HID Usage Tables, matching W3C UIEvents code values.
 * Only the most common keys are listed; the full set is in the spec.
 */

typedef enum wapi_scancode_t {
    WAPI_SCANCODE_UNKNOWN = 0,

    /* Letters */
    WAPI_SCANCODE_A = 4,  WAPI_SCANCODE_B = 5,  WAPI_SCANCODE_C = 6,
    WAPI_SCANCODE_D = 7,  WAPI_SCANCODE_E = 8,  WAPI_SCANCODE_F = 9,
    WAPI_SCANCODE_G = 10, WAPI_SCANCODE_H = 11, WAPI_SCANCODE_I = 12,
    WAPI_SCANCODE_J = 13, WAPI_SCANCODE_K = 14, WAPI_SCANCODE_L = 15,
    WAPI_SCANCODE_M = 16, WAPI_SCANCODE_N = 17, WAPI_SCANCODE_O = 18,
    WAPI_SCANCODE_P = 19, WAPI_SCANCODE_Q = 20, WAPI_SCANCODE_R = 21,
    WAPI_SCANCODE_S = 22, WAPI_SCANCODE_T = 23, WAPI_SCANCODE_U = 24,
    WAPI_SCANCODE_V = 25, WAPI_SCANCODE_W = 26, WAPI_SCANCODE_X = 27,
    WAPI_SCANCODE_Y = 28, WAPI_SCANCODE_Z = 29,

    /* Numbers */
    WAPI_SCANCODE_1 = 30, WAPI_SCANCODE_2 = 31, WAPI_SCANCODE_3 = 32,
    WAPI_SCANCODE_4 = 33, WAPI_SCANCODE_5 = 34, WAPI_SCANCODE_6 = 35,
    WAPI_SCANCODE_7 = 36, WAPI_SCANCODE_8 = 37, WAPI_SCANCODE_9 = 38,
    WAPI_SCANCODE_0 = 39,

    /* Control keys */
    WAPI_SCANCODE_RETURN    = 40,
    WAPI_SCANCODE_ESCAPE    = 41,
    WAPI_SCANCODE_BACKSPACE = 42,
    WAPI_SCANCODE_TAB       = 43,
    WAPI_SCANCODE_SPACE     = 44,

    /* Modifiers */
    WAPI_SCANCODE_LCTRL  = 224, WAPI_SCANCODE_LSHIFT = 225,
    WAPI_SCANCODE_LALT   = 226, WAPI_SCANCODE_LGUI   = 227,
    WAPI_SCANCODE_RCTRL  = 228, WAPI_SCANCODE_RSHIFT = 229,
    WAPI_SCANCODE_RALT   = 230, WAPI_SCANCODE_RGUI   = 231,

    /* Function keys */
    WAPI_SCANCODE_F1  = 58,  WAPI_SCANCODE_F2  = 59,
    WAPI_SCANCODE_F3  = 60,  WAPI_SCANCODE_F4  = 61,
    WAPI_SCANCODE_F5  = 62,  WAPI_SCANCODE_F6  = 63,
    WAPI_SCANCODE_F7  = 64,  WAPI_SCANCODE_F8  = 65,
    WAPI_SCANCODE_F9  = 66,  WAPI_SCANCODE_F10 = 67,
    WAPI_SCANCODE_F11 = 68,  WAPI_SCANCODE_F12 = 69,

    /* Navigation */
    WAPI_SCANCODE_INSERT    = 73,
    WAPI_SCANCODE_HOME      = 74,
    WAPI_SCANCODE_PAGEUP    = 75,
    WAPI_SCANCODE_DELETE    = 76,
    WAPI_SCANCODE_END       = 77,
    WAPI_SCANCODE_PAGEDOWN  = 78,
    WAPI_SCANCODE_RIGHT     = 79,
    WAPI_SCANCODE_LEFT      = 80,
    WAPI_SCANCODE_DOWN      = 81,
    WAPI_SCANCODE_UP        = 82,

    WAPI_SCANCODE_FORCE32   = 0x7FFFFFFF
} wapi_scancode_t;

/* ============================================================
 * Modifier Flags
 * ============================================================ */

#define WAPI_KMOD_NONE   0x0000
#define WAPI_KMOD_LSHIFT 0x0001
#define WAPI_KMOD_RSHIFT 0x0002
#define WAPI_KMOD_LCTRL  0x0040
#define WAPI_KMOD_RCTRL  0x0080
#define WAPI_KMOD_LALT   0x0100
#define WAPI_KMOD_RALT   0x0200
#define WAPI_KMOD_LGUI   0x0400
#define WAPI_KMOD_RGUI   0x0800
#define WAPI_KMOD_CAPS   0x2000
#define WAPI_KMOD_NUM    0x1000

#define WAPI_KMOD_SHIFT  (WAPI_KMOD_LSHIFT | WAPI_KMOD_RSHIFT)
#define WAPI_KMOD_CTRL   (WAPI_KMOD_LCTRL  | WAPI_KMOD_RCTRL)
#define WAPI_KMOD_ALT    (WAPI_KMOD_LALT   | WAPI_KMOD_RALT)
#define WAPI_KMOD_GUI    (WAPI_KMOD_LGUI   | WAPI_KMOD_RGUI)

/* Platform action modifier: Cmd on macOS, Ctrl everywhere else.
 * The host sets this bit on key events so modules can test a single
 * flag for copy/paste/save/undo without platform-specific logic. */
#define WAPI_KMOD_ACTION 0x4000

/* ============================================================
 * Mouse Buttons
 * ============================================================ */

#define WAPI_MOUSE_BUTTON_LEFT   1
#define WAPI_MOUSE_BUTTON_MIDDLE 2
#define WAPI_MOUSE_BUTTON_RIGHT  3
#define WAPI_MOUSE_BUTTON_X1     4
#define WAPI_MOUSE_BUTTON_X2     5

/* ============================================================
 * Gamepad Buttons and Axes (matching SDL3 GamepadButton/Axis)
 * ============================================================ */

typedef enum wapi_gamepad_button_t {
    WAPI_GAMEPAD_BUTTON_SOUTH  = 0,  /* A / Cross */
    WAPI_GAMEPAD_BUTTON_EAST   = 1,  /* B / Circle */
    WAPI_GAMEPAD_BUTTON_WEST   = 2,  /* X / Square */
    WAPI_GAMEPAD_BUTTON_NORTH  = 3,  /* Y / Triangle */
    WAPI_GAMEPAD_BUTTON_BACK   = 4,
    WAPI_GAMEPAD_BUTTON_GUIDE  = 5,
    WAPI_GAMEPAD_BUTTON_START  = 6,
    WAPI_GAMEPAD_BUTTON_LSTICK = 7,
    WAPI_GAMEPAD_BUTTON_RSTICK = 8,
    WAPI_GAMEPAD_BUTTON_LSHOULDER = 9,
    WAPI_GAMEPAD_BUTTON_RSHOULDER = 10,
    WAPI_GAMEPAD_BUTTON_DPAD_UP    = 11,
    WAPI_GAMEPAD_BUTTON_DPAD_DOWN  = 12,
    WAPI_GAMEPAD_BUTTON_DPAD_LEFT  = 13,
    WAPI_GAMEPAD_BUTTON_DPAD_RIGHT = 14,
    WAPI_GAMEPAD_BUTTON_FORCE32    = 0x7FFFFFFF
} wapi_gamepad_button_t;

typedef enum wapi_gamepad_axis_t {
    WAPI_GAMEPAD_AXIS_LEFTX        = 0,
    WAPI_GAMEPAD_AXIS_LEFTY        = 1,
    WAPI_GAMEPAD_AXIS_RIGHTX       = 2,
    WAPI_GAMEPAD_AXIS_RIGHTY       = 3,
    WAPI_GAMEPAD_AXIS_LEFT_TRIGGER = 4,
    WAPI_GAMEPAD_AXIS_RIGHT_TRIGGER = 5,
    WAPI_GAMEPAD_AXIS_FORCE32      = 0x7FFFFFFF
} wapi_gamepad_axis_t;

/* ============================================================
 * Event Structures
 * ============================================================
 * All event structs share a common header.
 * The event union is padded to 128 bytes (matching SDL3).
 */

/** Common event header (12 bytes) */
typedef struct wapi_event_common_t {
    uint32_t    type;        /* wapi_event_type_t */
    uint32_t    surface_id;  /* Surface handle this event belongs to */
    uint64_t    timestamp;   /* Nanoseconds (monotonic clock) */
} wapi_event_common_t;

/** Keyboard event */
typedef struct wapi_keyboard_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    keyboard_handle; /* Keyboard device handle */
    uint32_t    scancode;    /* wapi_scancode_t (physical key) */
    uint32_t    keycode;     /* Virtual key code (layout-dependent) */
    uint16_t    mod;         /* Modifier flags (WAPI_KMOD_*) */
    uint8_t     down;        /* 1 = pressed, 0 = released */
    uint8_t     repeat;      /* 1 = key repeat */
} wapi_keyboard_event_t;

/** Text input event */
typedef struct wapi_text_input_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    char        text[32];    /* UTF-8 text input (null-terminated) */
} wapi_text_input_event_t;

/** Mouse motion event */
typedef struct wapi_mouse_motion_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    mouse_handle; /* Mouse device handle */
    uint32_t    button_state;/* Button state mask */
    float       x;           /* Position relative to surface */
    float       y;
    float       xrel;        /* Relative motion */
    float       yrel;
} wapi_mouse_motion_event_t;

/** Mouse button event */
typedef struct wapi_mouse_button_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    mouse_id;
    uint8_t     button;      /* WAPI_MOUSE_BUTTON_* */
    uint8_t     down;        /* 1 = pressed */
    uint8_t     clicks;      /* 1 = single, 2 = double, etc. */
    uint8_t     _pad;
    float       x;
    float       y;
} wapi_mouse_button_event_t;

/** Mouse wheel event */
typedef struct wapi_mouse_wheel_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    mouse_handle;
    uint32_t    _pad;
    float       x;           /* Horizontal scroll (positive = right) */
    float       y;           /* Vertical scroll (positive = away from user) */
} wapi_mouse_wheel_event_t;

/** Touch event */
typedef struct wapi_touch_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    int32_t     touch_handle; /* Touch device handle */
    int32_t     finger_index; /* 0-based finger index */
    float       x;            /* Normalized 0..1 */
    float       y;
    float       dx;           /* Normalized -1..1 */
    float       dy;
    float       pressure;     /* Normalized 0..1 */
} wapi_touch_event_t;

/** Gamepad axis event */
typedef struct wapi_gamepad_axis_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    gamepad_handle; /* Gamepad device handle */
    uint8_t     axis;        /* wapi_gamepad_axis_t */
    uint8_t     _pad[3];
    int16_t     value;       /* -32768 to 32767 */
    uint16_t    _pad2;
} wapi_gamepad_axis_event_t;

/** Gamepad button event */
typedef struct wapi_gamepad_button_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    gamepad_handle;
    uint8_t     button;      /* wapi_gamepad_button_t */
    uint8_t     down;
    uint8_t     _pad[2];
} wapi_gamepad_button_event_t;

/** Device lifecycle event (DEVICE_ADDED / DEVICE_REMOVED) */
typedef struct wapi_device_event_t {
    uint32_t    type;
    uint32_t    surface_id;     /* 0 for device events */
    uint64_t    timestamp;
    uint32_t    device_type;    /* wapi_device_type_t (defined in wapi_input.h) */
    int32_t     device_handle;  /* Handle if open, WAPI_HANDLE_INVALID otherwise */
    uint8_t     uid[16];        /* Stable device identity */
} wapi_device_event_t;

/** Surface/window event (resize, move, focus, etc.) */
typedef struct wapi_surface_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    int32_t     data1;       /* Width for resize, x for move */
    int32_t     data2;       /* Height for resize, y for move */
} wapi_surface_event_t;

/** Drop event */
typedef struct wapi_drop_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    const char* data;        /* File path or text (valid until next poll) */
    wapi_size_t   data_len;
} wapi_drop_event_t;

/** Pen/stylus event */
typedef struct wapi_pen_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    pen_handle;
    uint8_t     tool_type;   /* wapi_pen_tool_t */
    uint8_t     button;      /* Barrel button index (0 = none) */
    uint8_t     _pad[2];
    float       x;
    float       y;
    float       pressure;    /* 0.0 to 1.0 */
    float       tilt_x;     /* -90 to 90 degrees */
    float       tilt_y;     /* -90 to 90 degrees */
    float       twist;      /* 0 to 360 degrees */
    float       distance;   /* Distance from surface (0.0 to 1.0) */
} wapi_pen_event_t;

/** Gamepad sensor event */
typedef struct wapi_gamepad_sensor_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    gamepad_handle;
    uint32_t    sensor;      /* wapi_gamepad_sensor_t */
    float       data[3];     /* x, y, z */
} wapi_gamepad_sensor_event_t;

/** Gamepad touchpad event */
typedef struct wapi_gamepad_touchpad_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    gamepad_handle;
    uint8_t     touchpad;
    uint8_t     finger;
    uint8_t     _pad[2];
    float       x;
    float       y;
    float       pressure;
} wapi_gamepad_touchpad_event_t;

/** Gesture event */
typedef struct wapi_gesture_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    gesture_type; /* wapi_gesture_type_t */
    uint32_t    _pad;
    float       magnitude;   /* Scale for pinch, angle for rotate */
    float       x;
    float       y;
} wapi_gesture_event_t;

/** Display event */
typedef struct wapi_display_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    display_id;
    uint32_t    _pad;
} wapi_display_event_t;

/** Hotkey event */
typedef struct wapi_hotkey_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    hotkey_id;
    uint32_t    _pad;
} wapi_hotkey_event_t;

/** Tray event */
typedef struct wapi_tray_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    tray_handle;
    uint32_t    item_id;     /* Menu item ID for TRAY_MENU events */
} wapi_tray_event_t;

/** File watcher event */
typedef struct wapi_fwatch_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    watch_handle;
    uint32_t    change_type; /* wapi_fwatch_change_t */
    uint32_t    path_ptr;    /* Pointer to changed path in wasm memory */
    uint32_t    path_len;
} wapi_fwatch_event_t;

/** Menu event */
typedef struct wapi_menu_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    menu_handle;
    uint32_t    item_id;
} wapi_menu_event_t;

/** Content tree event (activate, focus, value change) */
typedef struct wapi_content_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    node_id;        /* App-assigned node ID */
    uint32_t    _reserved;
} wapi_content_event_t;

/** I/O completion event -- async operation finished */
typedef struct wapi_io_event_t {
    uint32_t    type;           /* WAPI_EVENT_IO_COMPLETION */
    uint32_t    surface_id;     /* Always 0 for I/O events */
    uint64_t    timestamp;
    int32_t     result;         /* Bytes transferred, new fd, or negative error */
    uint32_t    flags;          /* Completion flags (WAPI_IO_CQE_F_*) */
    uint64_t    user_data;      /* Echoed from wapi_io_op_t */
} wapi_io_event_t;

/* I/O completion flags */
#define WAPI_IO_CQE_F_MORE      0x0001  /* More completions coming for this op */
#define WAPI_IO_CQE_F_OVERFLOW  0x0002  /* Completion queue overflowed */


/* ============================================================
 * Event Union (128 bytes, padded)
 * ============================================================ */

typedef union wapi_event_t {
    uint32_t                          type;
    wapi_event_common_t               common;
    wapi_keyboard_event_t             key;
    wapi_text_input_event_t           text;
    wapi_mouse_motion_event_t         motion;
    wapi_mouse_button_event_t         button;
    wapi_mouse_wheel_event_t          wheel;
    wapi_touch_event_t                touch;
    wapi_gamepad_axis_event_t         gaxis;
    wapi_gamepad_button_event_t       gbutton;
    wapi_gamepad_sensor_event_t       gsensor;
    wapi_gamepad_touchpad_event_t     gtouchpad;
    wapi_device_event_t               device;
    wapi_surface_event_t              surface;
    wapi_drop_event_t                 drop;
    wapi_pen_event_t                  pen;
    wapi_gesture_event_t              gesture;
    wapi_display_event_t              display;
    wapi_hotkey_event_t               hotkey;
    wapi_tray_event_t                 tray;
    wapi_fwatch_event_t               fwatch;
    wapi_menu_event_t                 menu;
    wapi_content_event_t              content;
    wapi_io_event_t                   io;
    uint8_t                           _padding[128];
} wapi_event_t;

_Static_assert(sizeof(wapi_event_t) == 128, "wapi_event_t must be 128 bytes");

/* ============================================================
 * Event Delivery
 * ============================================================
 * Events are delivered through the wapi_io_t vtable provided in
 * wapi_context_t. There is no separate event import namespace.
 *
 *   ctx->io->poll(ctx->io->impl, &event)   -- non-blocking
 *   ctx->io->wait(ctx->io->impl, &event, timeout_ms)  -- blocking
 *   ctx->io->flush(ctx->io->impl, event_type)  -- discard pending
 *
 * @see wapi_io_t in wapi_context.h
 */

#ifdef __cplusplus
}
#endif

#endif /* WAPI_EVENT_H */
