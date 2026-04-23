/**
 * WAPI - Input Devices
 * Version 1.0.0
 *
 * State queries and per-kind surfaces for mouse, keyboard, touch,
 * pen, gamepad, and HID. Endpoints are acquired through the role
 * system (WAPI_ROLE_MOUSE / KEYBOARD / TOUCH / PEN / GAMEPAD / HID /
 * POINTER); this header owns the HID prefs, endpoint_info, and the
 * use surfaces.
 *
 * Device connect/disconnect arrives as WAPI_EVENT_DEVICE_ADDED /
 * WAPI_EVENT_DEVICE_REMOVED.
 *
 * Import module: "wapi_input"
 */

#ifndef WAPI_INPUT_H
#define WAPI_INPUT_H

#include "wapi.h"
#include "wapi_seat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Pointer Source Types
 * ============================================================
 * Identifies which class of input device originated a pointer event.
 */

typedef enum wapi_pointer_type_t {
    WAPI_POINTER_MOUSE  = 0,
    WAPI_POINTER_TOUCH  = 1,
    WAPI_POINTER_PEN    = 2,
    WAPI_POINTER_FORCE32 = 0x7FFFFFFF
} wapi_pointer_type_t;

/* ============================================================
 * Input Device Lifecycle
 * ============================================================
 * Endpoints come from the role system. These functions query and
 * close granted handles; they work on any input-kind handle.
 *
 * MOUSE and POINTER roles each have a system-aggregate endpoint
 * that is always present; request with the role kind and no
 * target_uid to receive it.
 */

/** Close any input endpoint handle (mouse/keyboard/touch/pen/gamepad/hid/pointer). */
WAPI_IMPORT(wapi_input, close)
wapi_result_t wapi_input_close(wapi_handle_t handle);

/** Role kind of an open endpoint. Returns -1 on invalid handle. */
WAPI_IMPORT(wapi_input, role_kind)
int32_t wapi_input_role_kind(wapi_handle_t handle);

/** 16-byte UID of an open endpoint. Stable across reconnects. */
WAPI_IMPORT(wapi_input, uid)
wapi_result_t wapi_input_uid(wapi_handle_t handle, uint8_t uid[16]);

/** UTF-8 display name of an open endpoint. May be redacted. */
WAPI_IMPORT(wapi_input, name)
wapi_result_t wapi_input_name(wapi_handle_t handle, char* buf,
                              wapi_size_t buf_len, wapi_size_t* name_len);

/** Owning seat for multi-seat systems. Returns WAPI_SEAT_DEFAULT otherwise. */
WAPI_IMPORT(wapi_input, seat)
wapi_seat_t wapi_input_seat(wapi_handle_t handle);

/* ============================================================
 *  ███╗   ███╗ ██████╗ ██╗   ██╗███████╗███████╗
 *  ████╗ ████║██╔═══██╗██║   ██║██╔════╝██╔════╝
 *  ██╔████╔██║██║   ██║██║   ██║███████╗█████╗
 *  ██║╚██╔╝██║██║   ██║██║   ██║╚════██║██╔══╝
 *  ██║ ╚═╝ ██║╚██████╔╝╚██████╔╝███████║███████╗
 *  ╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚══════╝╚══════╝
 * ============================================================ */

/* ---- Cursor Types ---- */

typedef enum wapi_cursor_type_t {
    WAPI_CURSOR_DEFAULT     = 0,
    WAPI_CURSOR_POINTER     = 1,   /* Hand/link pointer */
    WAPI_CURSOR_TEXT        = 2,   /* I-beam for text */
    WAPI_CURSOR_CROSSHAIR  = 3,
    WAPI_CURSOR_MOVE        = 4,
    WAPI_CURSOR_RESIZE_NS   = 5,
    WAPI_CURSOR_RESIZE_EW   = 6,
    WAPI_CURSOR_RESIZE_NWSE = 7,
    WAPI_CURSOR_RESIZE_NESW = 8,
    WAPI_CURSOR_NOTALLOWED  = 9,
    WAPI_CURSOR_WAIT        = 10,
    WAPI_CURSOR_GRAB        = 11,
    WAPI_CURSOR_GRABBING    = 12,
    WAPI_CURSOR_NONE        = 13,  /* Hidden cursor */
    WAPI_CURSOR_FORCE32     = 0x7FFFFFFF
} wapi_cursor_type_t;

/* ---- Mouse Info ----
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: uint32_t button_count
 *   Offset  4: uint8_t  has_wheel
 *   Offset  5: uint8_t  _reserved[11]
 */

typedef struct wapi_mouse_info_t {
    uint32_t    button_count;
    uint8_t     has_wheel;
    uint8_t     _reserved[11];
} wapi_mouse_info_t;

_Static_assert(sizeof(wapi_mouse_info_t) == 16,
               "wapi_mouse_info_t must be 16 bytes");
_Static_assert(_Alignof(wapi_mouse_info_t) == 4,
               "wapi_mouse_info_t must be 4-byte aligned");

/* ---- Mouse Functions ---- */

/**
 * Get mouse-specific info.
 *
 * @param handle    Mouse device handle.
 * @param info_ptr  [out] Pointer to wapi_mouse_info_t.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if not a mouse.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, mouse_get_info)
wapi_result_t wapi_mouse_get_info(wapi_handle_t handle,
                                  wapi_mouse_info_t* info_ptr);

/**
 * Get the current mouse position relative to a surface.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, mouse_get_position)
wapi_result_t wapi_mouse_get_position(wapi_handle_t handle,
                                      wapi_handle_t surface,
                                      float* x, float* y);

/**
 * Get the current mouse button state.
 *
 * @param handle  Mouse device handle.
 * @return Button state mask (bit N = button N pressed).
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_input, mouse_get_button_state)
uint32_t wapi_mouse_get_button_state(wapi_handle_t handle);

/**
 * Set relative mouse mode (captures mouse, provides relative motion).
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, mouse_set_relative)
wapi_result_t wapi_mouse_set_relative(wapi_handle_t handle,
                                      wapi_handle_t surface,
                                      wapi_bool_t enabled);

/**
 * Warp (teleport) a mouse cursor to a position on a surface.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, mouse_warp)
wapi_result_t wapi_mouse_warp(wapi_handle_t handle,
                              wapi_handle_t surface,
                              float x, float y);

/**
 * Set the cursor style for a mouse.
 *
 * @param handle       Mouse device handle.
 * @param cursor_type  One of wapi_cursor_type_t.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, mouse_set_cursor)
wapi_result_t wapi_mouse_set_cursor(wapi_handle_t handle,
                                    wapi_cursor_type_t cursor_type);

/**
 * Set a custom cursor image for a mouse.
 *
 * @param handle      Mouse device handle.
 * @param image_data  Pointer to RGBA pixel data.
 * @param w           Width in pixels.
 * @param h           Height in pixels.
 * @param hot_x       Hotspot X.
 * @param hot_y       Hotspot Y.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, mouse_set_cursor_image)
wapi_result_t wapi_mouse_set_cursor_image(wapi_handle_t handle,
                                          const void* image_data,
                                          int32_t w, int32_t h,
                                          int32_t hot_x, int32_t hot_y);

/* ============================================================
 *  ██╗  ██╗███████╗██╗   ██╗██████╗  ██████╗  █████╗ ██████╗ ██████╗
 *  ██║ ██╔╝██╔════╝╚██╗ ██╔╝██╔══██╗██╔═══██╗██╔══██╗██╔══██╗██╔══██╗
 *  █████╔╝ █████╗   ╚████╔╝ ██████╔╝██║   ██║███████║██████╔╝██║  ██║
 *  ██╔═██╗ ██╔══╝    ╚██╔╝  ██╔══██╗██║   ██║██╔══██║██╔══██╗██║  ██║
 *  ██║  ██╗███████╗   ██║   ██████╔╝╚██████╔╝██║  ██║██║  ██║██████╔╝
 *  ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═════╝
 * ============================================================ */

/**
 * Check if a key is currently pressed on a keyboard.
 *
 * @param handle    Keyboard device handle.
 * @param scancode  Physical key scancode.
 * @return 1 if pressed, 0 if not.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, keyboard_key_pressed)
wapi_bool_t wapi_keyboard_key_pressed(wapi_handle_t handle,
                                      wapi_scancode_t scancode);

/**
 * Get the current modifier key state for a keyboard.
 *
 * @param handle  Keyboard device handle.
 * @return Modifier flags (WAPI_KMOD_*).
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_input, keyboard_get_modstate)
uint16_t wapi_keyboard_get_modstate(wapi_handle_t handle);

/* ---- Text Input ----
 * Text input is surface-scoped (not keyboard-scoped) because
 * IME composition targets a focused surface, not a specific
 * physical keyboard.
 */

/**
 * Start text input mode. Enables IME and generates
 * WAPI_EVENT_TEXTINPUT events.
 *
 * Wasm signature: (i32) -> void
 */
WAPI_IMPORT(wapi_input, start_textinput)
void wapi_input_start_textinput(wapi_handle_t surface);

/**
 * Stop text input mode.
 *
 * Wasm signature: (i32) -> void
 */
WAPI_IMPORT(wapi_input, stop_textinput)
void wapi_input_stop_textinput(wapi_handle_t surface);

/* ============================================================
 *  ████████╗ ██████╗ ██╗   ██╗ ██████╗██╗  ██╗
 *  ╚══██╔══╝██╔═══██╗██║   ██║██╔════╝██║  ██║
 *     ██║   ██║   ██║██║   ██║██║     ███████║
 *     ██║   ██║   ██║██║   ██║██║     ██╔══██║
 *     ██║   ╚██████╔╝╚██████╔╝╚██████╗██║  ██║
 *     ╚═╝    ╚═════╝  ╚═════╝  ╚═════╝╚═╝  ╚═╝
 * ============================================================ */

/* ---- Touch Device Type ---- */

typedef enum wapi_touch_device_type_t {
    WAPI_TOUCH_DEVICE_DIRECT   = 0,  /* Touchscreen */
    WAPI_TOUCH_DEVICE_INDIRECT = 1,  /* Trackpad */
    WAPI_TOUCH_DEVICE_FORCE32  = 0x7FFFFFFF
} wapi_touch_device_type_t;

/* ---- Touch Info ----
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: uint32_t type          wapi_touch_device_type_t
 *   Offset  4: uint32_t max_fingers   Max simultaneous touch points
 *   Offset  8: uint8_t  _reserved[8]
 */

typedef struct wapi_touch_info_t {
    uint32_t    type;
    uint32_t    max_fingers;
    uint8_t     _reserved[8];
} wapi_touch_info_t;

_Static_assert(sizeof(wapi_touch_info_t) == 16,
               "wapi_touch_info_t must be 16 bytes");
_Static_assert(_Alignof(wapi_touch_info_t) == 4,
               "wapi_touch_info_t must be 4-byte aligned");

/* ---- Finger State ----
 *
 * Raw device query — no surface context, so coordinates are
 * device-native normalized 0..1.  Touch *events* (which carry
 * a surface_id) use surface-pixel coordinates.
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: int32_t finger_index
 *   Offset  4: float   x             Normalized 0..1 (device-native)
 *   Offset  8: float   y             Normalized 0..1 (device-native)
 *   Offset 12: float   pressure      Normalized 0..1
 */

typedef struct wapi_finger_state_t {
    int32_t     finger_index;
    float       x;
    float       y;
    float       pressure;
} wapi_finger_state_t;

_Static_assert(sizeof(wapi_finger_state_t) == 16,
               "wapi_finger_state_t must be 16 bytes");
_Static_assert(_Alignof(wapi_finger_state_t) == 4,
               "wapi_finger_state_t must be 4-byte aligned");

/* ---- Gesture Type ---- */

typedef enum wapi_gesture_type_t {
    WAPI_GESTURE_PINCH       = 0,
    WAPI_GESTURE_ROTATE      = 1,
    WAPI_GESTURE_SWIPE_LEFT  = 2,
    WAPI_GESTURE_SWIPE_RIGHT = 3,
    WAPI_GESTURE_SWIPE_UP    = 4,
    WAPI_GESTURE_SWIPE_DOWN  = 5,
    WAPI_GESTURE_LONGPRESS   = 6,
    WAPI_GESTURE_TAP         = 7,
    WAPI_GESTURE_DOUBLETAP   = 8,
    WAPI_GESTURE_FORCE32     = 0x7FFFFFFF
} wapi_gesture_type_t;

/* ---- Touch Functions ---- */

/**
 * Get touch-specific info.
 *
 * @param handle    Touch device handle.
 * @param info_ptr  [out] Pointer to wapi_touch_info_t.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if not a touch device.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, touch_get_info)
wapi_result_t wapi_touch_get_info(wapi_handle_t handle,
                                  wapi_touch_info_t* info_ptr);

/**
 * Get the number of active fingers on a touch device.
 *
 * @param handle  Touch device handle.
 * @return Number of active fingers (>= 0).
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_input, touch_finger_count)
int32_t wapi_touch_finger_count(wapi_handle_t handle);

/**
 * Get the state of a specific finger on a touch device.
 *
 * @param handle        Touch device handle.
 * @param finger_index  Finger index (0-based, up to finger_count - 1).
 * @param state_ptr     [out] Pointer to wapi_finger_state_t.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, touch_get_finger)
wapi_result_t wapi_touch_get_finger(wapi_handle_t handle,
                                    int32_t finger_index,
                                    wapi_finger_state_t* state_ptr);

/* ============================================================
 *  ██████╗ ███████╗███╗   ██╗
 *  ██╔══██╗██╔════╝████╗  ██║
 *  ██████╔╝█████╗  ██╔██╗ ██║
 *  ██╔═══╝ ██╔══╝  ██║╚██╗██║
 *  ██║     ███████╗██║ ╚████║
 *  ╚═╝     ╚══════╝╚═╝  ╚═══╝
 * ============================================================ */

/* ---- Pen Tool Type ---- */

typedef enum wapi_pen_tool_t {
    WAPI_PEN_TOOL_PEN      = 0,
    WAPI_PEN_TOOL_ERASER   = 1,
    WAPI_PEN_TOOL_BRUSH    = 2,
    WAPI_PEN_TOOL_AIRBRUSH = 3,
    WAPI_PEN_TOOL_PENCIL   = 4,
    WAPI_PEN_TOOL_FORCE32  = 0x7FFFFFFF
} wapi_pen_tool_t;

/* ---- Pen Axis ---- */

typedef enum wapi_pen_axis_t {
    WAPI_PEN_AXIS_PRESSURE              = 0,
    WAPI_PEN_AXIS_TILT_X                = 1,
    WAPI_PEN_AXIS_TILT_Y                = 2,
    WAPI_PEN_AXIS_ROTATION              = 3,
    WAPI_PEN_AXIS_DISTANCE              = 4,
    WAPI_PEN_AXIS_SLIDER                = 5,
    WAPI_PEN_AXIS_TANGENTIALPRESSURE    = 6,
    WAPI_PEN_AXIS_FORCE32               = 0x7FFFFFFF
} wapi_pen_axis_t;

/* ---- Pen Info ----
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: uint32_t tool_type          wapi_pen_tool_t
 *   Offset  4: uint32_t capabilities_mask  Bitmask of wapi_pen_axis_t
 *   Offset  8: uint8_t  _reserved[8]
 */

typedef struct wapi_pen_info_t {
    uint32_t    tool_type;
    uint32_t    capabilities_mask;
    uint8_t     _reserved[8];
} wapi_pen_info_t;

_Static_assert(sizeof(wapi_pen_info_t) == 16,
               "wapi_pen_info_t must be 16 bytes");
_Static_assert(_Alignof(wapi_pen_info_t) == 4,
               "wapi_pen_info_t must be 4-byte aligned");

/* ---- Pen Functions ---- */

/**
 * Get pen-specific info.
 *
 * @param handle    Pen device handle.
 * @param info_ptr  [out] Pointer to wapi_pen_info_t.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if not a pen device.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, pen_get_info)
wapi_result_t wapi_pen_get_info(wapi_handle_t handle,
                                wapi_pen_info_t* info_ptr);

/**
 * Get the current value of a pen axis.
 *
 * @param handle     Pen device handle.
 * @param axis       Axis to query (wapi_pen_axis_t).
 * @param value_ptr  [out] Pointer to float.
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if axis not available.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, pen_get_axis)
wapi_result_t wapi_pen_get_axis(wapi_handle_t handle,
                                wapi_pen_axis_t axis,
                                float* value_ptr);

/**
 * Get the current position of a pen.
 *
 * @param handle  Pen device handle.
 * @param x_ptr   [out] X coordinate.
 * @param y_ptr   [out] Y coordinate.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, pen_get_position)
wapi_result_t wapi_pen_get_position(wapi_handle_t handle,
                                    float* x_ptr, float* y_ptr);

/* ============================================================
 *   ██████╗  █████╗ ███╗   ███╗███████╗██████╗  █████╗ ██████╗
 *  ██╔════╝ ██╔══██╗████╗ ████║██╔════╝██╔══██╗██╔══██╗██╔══██╗
 *  ██║  ███╗███████║██╔████╔██║█████╗  ██████╔╝███████║██║  ██║
 *  ██║   ██║██╔══██║██║╚██╔╝██║██╔══╝  ██╔═══╝ ██╔══██║██║  ██║
 *  ╚██████╔╝██║  ██║██║ ╚═╝ ██║███████╗██║     ██║  ██║██████╔╝
 *   ╚═════╝ ╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝╚═╝     ╚═╝  ╚═╝╚═════╝
 * ============================================================ */

/* ---- Gamepad Type ---- */

typedef enum wapi_gamepad_type_t {
    WAPI_GAMEPAD_TYPE_UNKNOWN             = 0,
    WAPI_GAMEPAD_TYPE_STANDARD            = 1,
    WAPI_GAMEPAD_TYPE_XBOX360            = 2,
    WAPI_GAMEPAD_TYPE_XBOXONE            = 3,
    WAPI_GAMEPAD_TYPE_PS3                 = 4,
    WAPI_GAMEPAD_TYPE_PS4                 = 5,
    WAPI_GAMEPAD_TYPE_PS5                 = 6,
    WAPI_GAMEPAD_TYPE_SWITCHPRO          = 7,
    WAPI_GAMEPAD_TYPE_SWITCHJOYCON_LEFT  = 8,
    WAPI_GAMEPAD_TYPE_SWITCHJOYCON_RIGHT = 9,
    WAPI_GAMEPAD_TYPE_SWITCHJOYCON_PAIR  = 10,
    WAPI_GAMEPAD_TYPE_FORCE32             = 0x7FFFFFFF
} wapi_gamepad_type_t;

/* ---- Gamepad Battery State ---- */

typedef enum wapi_gamepad_battery_t {
    WAPI_GAMEPAD_BATTERY_UNKNOWN     = 0,
    WAPI_GAMEPAD_BATTERY_DISCHARGING = 1,  /* Running on internal battery */
    WAPI_GAMEPAD_BATTERY_CHARGING    = 2,
    WAPI_GAMEPAD_BATTERY_CHARGED     = 3,
    WAPI_GAMEPAD_BATTERY_WIRED       = 4,  /* No battery, wired power only */
    WAPI_GAMEPAD_BATTERY_FORCE32     = 0x7FFFFFFF
} wapi_gamepad_battery_t;

/* ---- Gamepad Sensor Type ---- */

typedef enum wapi_gamepad_sensor_t {
    WAPI_GAMEPAD_SENSOR_ACCEL   = 0,
    WAPI_GAMEPAD_SENSOR_GYRO    = 1,
    WAPI_GAMEPAD_SENSOR_FORCE32 = 0x7FFFFFFF
} wapi_gamepad_sensor_t;

/* ---- Gamepad Info ----
 *
 * Layout (48 bytes, align 4):
 *   Offset  0: uint32_t type
 *   Offset  4: uint16_t vendor_id
 *   Offset  6: uint16_t product_id
 *   Offset  8: uint8_t  has_rumble
 *   Offset  9: uint8_t  has_trigger_rumble
 *   Offset 10: uint8_t  has_led
 *   Offset 11: uint8_t  has_sensors
 *   Offset 12: uint8_t  has_touchpad
 *   Offset 13: uint8_t  battery_percent   0-100
 *   Offset 14: uint8_t  battery           wapi_gamepad_battery_t
 *   Offset 15: uint8_t  _pad[1]
 *   Offset 16: uint8_t  _reserved[32]
 */

typedef struct wapi_gamepad_info_t {
    uint32_t    type;
    uint16_t    vendor_id;
    uint16_t    product_id;
    uint8_t     has_rumble;
    uint8_t     has_trigger_rumble;
    uint8_t     has_led;
    uint8_t     has_sensors;
    uint8_t     has_touchpad;
    uint8_t     battery_percent;
    uint8_t     battery;
    uint8_t     _pad[1];
    uint8_t     _reserved[32];
} wapi_gamepad_info_t;

_Static_assert(sizeof(wapi_gamepad_info_t) == 48,
               "wapi_gamepad_info_t must be 48 bytes");
_Static_assert(_Alignof(wapi_gamepad_info_t) == 4,
               "wapi_gamepad_info_t must be 4-byte aligned");

/* ---- Touchpad Finger State ----
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: uint8_t down          1 if touching
 *   Offset  1: uint8_t _pad[3]
 *   Offset  4: float   x             Normalized 0..1
 *   Offset  8: float   y             Normalized 0..1
 *   Offset 12: float   pressure      Normalized 0..1
 */

typedef struct wapi_touchpad_finger_t {
    uint8_t     down;
    uint8_t     _pad[3];
    float       x;
    float       y;
    float       pressure;
} wapi_touchpad_finger_t;

_Static_assert(sizeof(wapi_touchpad_finger_t) == 16,
               "wapi_touchpad_finger_t must be 16 bytes");
_Static_assert(_Alignof(wapi_touchpad_finger_t) == 4,
               "wapi_touchpad_finger_t must be 4-byte aligned");

/* wapi_gamepad_button_t / wapi_gamepad_axis_t enums are declared in
 * wapi.h (alongside the event structs that reference them). */

/* ---- Gamepad Functions ---- */

/**
 * Get gamepad-specific info.
 *
 * @param handle    Gamepad device handle.
 * @param info_ptr  [out] Pointer to wapi_gamepad_info_t.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if not a gamepad.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, gamepad_get_info)
wapi_result_t wapi_gamepad_get_info(wapi_handle_t handle,
                                    wapi_gamepad_info_t* info_ptr);

/**
 * Check if a gamepad button is currently pressed.
 *
 * @param handle  Gamepad device handle.
 * @param button  Button identifier (wapi_gamepad_button_t).
 * @return 1 if pressed, 0 if not.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, gamepad_get_button)
wapi_bool_t wapi_gamepad_get_button(wapi_handle_t handle,
                                    wapi_gamepad_button_t button);

/**
 * Get the current value of a gamepad axis.
 *
 * @param handle     Gamepad device handle.
 * @param axis       Axis identifier (wapi_gamepad_axis_t).
 * @param value_ptr  [out] Pointer to int16_t (-32768 to 32767).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, gamepad_get_axis)
wapi_result_t wapi_gamepad_get_axis(wapi_handle_t handle,
                                    wapi_gamepad_axis_t axis,
                                    int16_t* value_ptr);

/**
 * Trigger rumble on the main motors.
 *
 * @param handle       Gamepad device handle.
 * @param low_freq     Low-frequency motor intensity (0-65535).
 * @param high_freq    High-frequency motor intensity (0-65535).
 * @param duration_ms  Duration in milliseconds.
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if not supported.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, gamepad_rumble)
wapi_result_t wapi_gamepad_rumble(wapi_handle_t handle,
                                  int32_t low_freq,
                                  int32_t high_freq,
                                  int32_t duration_ms);

/**
 * Trigger rumble on the adaptive triggers (left and right).
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, gamepad_rumble_triggers)
wapi_result_t wapi_gamepad_rumble_triggers(wapi_handle_t handle,
                                           int32_t left,
                                           int32_t right,
                                           int32_t duration_ms);

/**
 * Set the gamepad LED color.
 *
 * @param handle  Gamepad device handle.
 * @param r       Red component (0-255).
 * @param g       Green component (0-255).
 * @param b       Blue component (0-255).
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if not supported.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, gamepad_set_led)
wapi_result_t wapi_gamepad_set_led(wapi_handle_t handle,
                                   int32_t r, int32_t g, int32_t b);

/**
 * Enable or disable a gamepad sensor.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, gamepad_enable_sensor)
wapi_result_t wapi_gamepad_enable_sensor(wapi_handle_t handle,
                                         wapi_gamepad_sensor_t sensor_type,
                                         wapi_bool_t enabled);

/**
 * Read the latest sensor data from a gamepad.
 *
 * @param handle       Gamepad device handle.
 * @param sensor_type  Sensor type (wapi_gamepad_sensor_t).
 * @param data_ptr     [out] Pointer to 3 floats (x, y, z).
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if sensor not available.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, gamepad_get_sensor_data)
wapi_result_t wapi_gamepad_get_sensor_data(wapi_handle_t handle,
                                           wapi_gamepad_sensor_t sensor_type,
                                           float* data_ptr);

/**
 * Get the state of a finger on a gamepad touchpad.
 *
 * @param handle     Gamepad device handle.
 * @param touchpad   Touchpad index (usually 0).
 * @param finger     Finger index.
 * @param state_ptr  [out] Pointer to wapi_touchpad_finger_t.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, gamepad_get_touchpad_finger)
wapi_result_t wapi_gamepad_get_touchpad_finger(wapi_handle_t handle,
                                               int32_t touchpad,
                                               int32_t finger,
                                               wapi_touchpad_finger_t* state_ptr);

/**
 * Get the battery state for a gamepad.
 *
 * @param handle       Gamepad device handle.
 * @param percent_ptr  [out] Battery percentage (0-100).
 * @return Battery state (wapi_gamepad_battery_t) on success, negative on error.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, gamepad_get_battery)
int32_t wapi_gamepad_get_battery(wapi_handle_t handle,
                                 uint8_t* percent_ptr);

/* ============================================================
 *  ██████╗  ██████╗ ██╗███╗   ██╗████████╗███████╗██████╗
 *  ██╔══██╗██╔═══██╗██║████╗  ██║╚══██╔══╝██╔════╝██╔══██╗
 *  ██████╔╝██║   ██║██║██╔██╗ ██║   ██║   █████╗  ██████╔╝
 *  ██╔═══╝ ██║   ██║██║██║╚██╗██║   ██║   ██╔══╝  ██╔══██╗
 *  ██║     ╚██████╔╝██║██║ ╚████║   ██║   ███████╗██║  ██║
 *  ╚═╝      ╚═════╝ ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚══════╝╚═╝  ╚═╝
 * ============================================================
 * Unified pointer abstraction. A physical mouse is simultaneously
 * a Mouse device and a Pointer device; a touchscreen is both a
 * Touch device and a Pointer device; a stylus is both a Pen
 * device and a Pointer device.
 *
 * Pointer index 0 is the system aggregate pointer. It tracks
 * the most recent position from any source. Multi-touch fingers
 * and pen contacts are distinguished by pointer_id in events
 * (same pattern as touch events carrying finger_index).
 *
 * Pointer events use surface-pixel coordinates for all sources.
 */

/* ---- Pointer Info ----
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: uint8_t  has_pressure
 *   Offset  1: uint8_t  has_tilt
 *   Offset  2: uint8_t  has_twist
 *   Offset  3: uint8_t  has_width_height
 *   Offset  4: uint8_t  _reserved[12]
 */

typedef struct wapi_pointer_info_t {
    uint8_t     has_pressure;
    uint8_t     has_tilt;
    uint8_t     has_twist;
    uint8_t     has_width_height;
    uint8_t     _reserved[12];
} wapi_pointer_info_t;

_Static_assert(sizeof(wapi_pointer_info_t) == 16,
               "wapi_pointer_info_t must be 16 bytes");
_Static_assert(_Alignof(wapi_pointer_info_t) == 1,
               "wapi_pointer_info_t must be 1-byte aligned");

/* ---- Pointer Functions ---- */

/**
 * Get pointer-specific info (capabilities).
 *
 * @param handle    Pointer device handle.
 * @param info_ptr  [out] Pointer to wapi_pointer_info_t.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if not a pointer device.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, pointer_get_info)
wapi_result_t wapi_pointer_get_info(wapi_handle_t handle,
                                    wapi_pointer_info_t* info_ptr);

/**
 * Get the current pointer position relative to a surface.
 * Returns the most recent position from any source (mouse, touch, pen).
 *
 * @param handle   Pointer device handle.
 * @param surface  Surface handle.
 * @param x        [out] X coordinate in surface pixels.
 * @param y        [out] Y coordinate in surface pixels.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, pointer_get_position)
wapi_result_t wapi_pointer_get_position(wapi_handle_t handle,
                                        wapi_handle_t surface,
                                        float* x, float* y);

/**
 * Get the current pointer button state.
 *
 * @param handle  Pointer device handle.
 * @return Button state mask (bit N = button N pressed).
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_input, pointer_get_buttons)
uint32_t wapi_pointer_get_buttons(wapi_handle_t handle);

/* ============================================================
 *  ██╗  ██╗██╗██████╗
 *  ██║  ██║██║██╔══██╗
 *  ███████║██║██║  ██║
 *  ██╔══██║██║██║  ██║
 *  ██║  ██║██║██████╔╝
 *  ╚═╝  ╚═╝╚═╝╚═════╝
 * ============================================================
 * Raw HID access for custom peripherals. HID endpoints are
 * acquired via ROLE_REQUEST(WAPI_ROLE_HID, prefs=wapi_hid_prefs_t)
 * — the prefs filter (vendor/product/usage) drives the runtime's
 * picker. Once granted, report I/O happens through this module.
 */

/**
 * HID role-request prefs (matches WebHID requestDevice filters).
 * 0 on any field means "any".
 *
 * Layout (8 bytes, align 2):
 *   Offset 0: uint16_t vendor_id
 *   Offset 2: uint16_t product_id
 *   Offset 4: uint16_t usage_page
 *   Offset 6: uint16_t usage
 */
typedef struct wapi_hid_prefs_t {
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t usage_page;
    uint16_t usage;
} wapi_hid_prefs_t;

_Static_assert(sizeof(wapi_hid_prefs_t) == 8, "wapi_hid_prefs_t must be 8 bytes");

typedef enum wapi_hid_transport_t {
    WAPI_HID_TRANSPORT_UNKNOWN = 0,
    WAPI_HID_TRANSPORT_USB     = 1,
    WAPI_HID_TRANSPORT_BT      = 2,
    WAPI_HID_TRANSPORT_BLE     = 3,
    WAPI_HID_TRANSPORT_I2C     = 4,
    WAPI_HID_TRANSPORT_FORCE32 = 0x7FFFFFFF
} wapi_hid_transport_t;

/**
 * Metadata about a resolved HID endpoint.
 *
 * Layout (32 bytes, align 8):
 *   Offset  0: uint16_t vendor_id
 *   Offset  2: uint16_t product_id
 *   Offset  4: uint16_t usage_page
 *   Offset  6: uint16_t usage
 *   Offset  8: uint32_t transport              wapi_hid_transport_t
 *   Offset 12: uint32_t report_descriptor_len
 *   Offset 16: uint8_t  uid[16]
 */
typedef struct wapi_hid_endpoint_info_t {
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t usage_page;
    uint16_t usage;
    uint32_t transport;
    uint32_t report_descriptor_len;
    uint8_t  uid[16];
} wapi_hid_endpoint_info_t;

_Static_assert(sizeof(wapi_hid_endpoint_info_t) == 32, "wapi_hid_endpoint_info_t must be 32 bytes");
_Static_assert(_Alignof(wapi_hid_endpoint_info_t) == 4, "wapi_hid_endpoint_info_t must be 4-byte aligned");

/** Query metadata for a granted HID endpoint. */
WAPI_IMPORT(wapi_input, hid_endpoint_info)
wapi_result_t wapi_hid_endpoint_info(wapi_handle_t handle,
                                     wapi_hid_endpoint_info_t* out,
                                     char* name_buf, wapi_size_t name_buf_len,
                                     wapi_size_t* name_len);

/** Fetch the device serial as UTF-8, privacy-gated.
 *  Returns WAPI_ERR_NOENT if the host withholds the serial. */
WAPI_IMPORT(wapi_input, hid_serial)
wapi_result_t wapi_hid_serial(wapi_handle_t handle, char* buf,
                              wapi_size_t buf_len, wapi_size_t* serial_len);

/** Fetch the raw HID report descriptor. Size in wapi_hid_endpoint_info_t. */
WAPI_IMPORT(wapi_input, hid_report_descriptor)
wapi_result_t wapi_hid_report_descriptor(wapi_handle_t handle, uint8_t* buf,
                                         wapi_size_t buf_len, wapi_size_t* desc_len);

/**
 * Send an output report to a HID device.
 *
 * @param handle     HID device handle.
 * @param report_id  Report ID.
 * @param data       Pointer to report data.
 * @param data_len   Length of the report data.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, hid_send_report)
wapi_result_t wapi_hid_send_report(wapi_handle_t handle, uint8_t report_id,
                                   const void* data, wapi_size_t data_len);

/**
 * Send a feature report to a HID device.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, hid_send_feature_report)
wapi_result_t wapi_hid_send_feature_report(wapi_handle_t handle,
                                           uint8_t report_id,
                                           const void* data,
                                           wapi_size_t data_len);

/**
 * Receive an input report from a HID device.
 *
 * @param handle      HID device handle.
 * @param buf         Buffer to receive report data.
 * @param buf_len     Size of the buffer.
 * @param bytes_read  [out] Actual number of bytes read.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, hid_receive_report)
wapi_result_t wapi_hid_receive_report(wapi_handle_t handle, void* buf,
                                      wapi_size_t buf_len,
                                      wapi_size_t* bytes_read);

/* ============================================================
 * IME / Text Input
 * ============================================================
 * Keyboard events carry physical key transitions; text is produced
 * separately by the platform IME (AltGr, compose, dead keys, CJK
 * composition, autocorrect, voice dictation, on-screen keyboards).
 *
 * Event flow:
 *
 *   1. The module calls wapi_input_ime_enable(surface, hint) on a
 *      text-editing surface. The host tells the platform IME which
 *      surface is the text target and what kind of content is
 *      expected (the hint tunes layouts, autocorrect, candidate
 *      filtering, password masking, etc.).
 *
 *   2. When the user starts composing the host emits
 *      WAPI_EVENT_IME_START, then WAPI_EVENT_IME_UPDATE any time
 *      the preedit text / caret / segment attributes change, and
 *      WAPI_EVENT_IME_COMMIT when the composition finalizes.
 *      WAPI_EVENT_IME_CANCEL fires if composition is abandoned.
 *
 *   3. Each event carries a host-assigned `sequence` id (see
 *      wapi_ime_event_t in wapi.h). The module reads the variable-
 *      length payload (UTF-8 text and attribute segments) by
 *      passing that sequence to the accessors below.
 *
 *   4. The sequence is valid from the time the event is delivered
 *      until the module's next wapi_io_poll call returns. Modules
 *      MUST drain what they care about inside the event handler;
 *      the host is permitted to recycle the payload storage as
 *      soon as poll re-enters.
 *
 *   5. When the editor loses focus or no longer wants text the
 *      module calls wapi_input_ime_disable(surface). Any active
 *      composition on that surface is finalized (commit) or
 *      dropped (cancel) by the platform, per its own conventions.
 *
 * Modules that only care about committed characters can ignore
 * START / UPDATE / CANCEL and just drain WAPI_EVENT_IME_COMMIT.
 *
 * Physical shortcuts (Ctrl+C, Cmd+S, etc.) belong on the keyboard
 * event stream. The IME stream is exclusively "text the user
 * intends to insert into a document". */

typedef enum wapi_ime_hint_t {
    WAPI_IME_HINT_DEFAULT   = 0,   /* Free-form text */
    WAPI_IME_HINT_TEXT      = 1,   /* Explicit plain text */
    WAPI_IME_HINT_PASSWORD  = 2,   /* Hide candidates + mask display */
    WAPI_IME_HINT_EMAIL     = 3,   /* Email address layout */
    WAPI_IME_HINT_URL       = 4,   /* URL layout */
    WAPI_IME_HINT_NUMBER    = 5,   /* Numeric keypad, digits only */
    WAPI_IME_HINT_SEARCH    = 6,   /* Search box (single line, no autocorrect) */
    WAPI_IME_HINT_FORCE32   = 0x7FFFFFFF
} wapi_ime_hint_t;

/* Segment attribute flags (bitmask) */
#define WAPI_IME_SEG_RAW        0x01  /* Raw unconverted input */
#define WAPI_IME_SEG_CONVERTED  0x02  /* Converted (committed-in-preedit) */
#define WAPI_IME_SEG_SELECTED   0x04  /* Currently selected for editing */
#define WAPI_IME_SEG_TARGET     0x08  /* Active target clause (IME focus) */

/** Preedit attribute segment (12 bytes, align 4).
 *
 * Layout:
 *   0:  start    u32   Byte offset into preedit text
 *   4:  length   u32   Byte length of this segment
 *   8:  flags    u32   WAPI_IME_SEG_* bitmask
 */
typedef struct wapi_ime_segment_t {
    uint32_t    start;
    uint32_t    length;
    uint32_t    flags;
} wapi_ime_segment_t;

/**
 * Enable IME for a surface with an input-type hint.
 *
 * @param surface_id  Surface handle that owns the text caret.
 * @param hint        Input-type hint for the platform IME.
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if no IME is available.
 */
WAPI_IMPORT(wapi_input, ime_enable)
wapi_result_t wapi_input_ime_enable(uint32_t surface_id, wapi_ime_hint_t hint);

/**
 * Disable IME for a surface. Any active composition on the surface
 * is finalized or discarded per the platform's conventions.
 */
WAPI_IMPORT(wapi_input, ime_disable)
wapi_result_t wapi_input_ime_disable(uint32_t surface_id);

/**
 * Tell the platform where to anchor the candidate window.
 *
 * Rect is in the same coordinate space as the surface. The host
 * translates into platform-native screen coordinates. No-op when
 * the surface has no active composition or the platform ignores
 * the hint.
 */
WAPI_IMPORT(wapi_input, ime_set_candidate_rect)
wapi_result_t wapi_input_ime_set_candidate_rect(uint32_t surface_id,
                                                float x, float y,
                                                float w, float h);

/**
 * Force-commit any active composition on the surface. The host
 * emits a WAPI_EVENT_IME_COMMIT carrying whatever preedit text
 * was outstanding.
 */
WAPI_IMPORT(wapi_input, ime_commit)
wapi_result_t wapi_input_ime_commit(uint32_t surface_id);

/**
 * Cancel any active composition on the surface, dropping preedit
 * text. The host emits a WAPI_EVENT_IME_CANCEL.
 */
WAPI_IMPORT(wapi_input, ime_cancel)
wapi_result_t wapi_input_ime_cancel(uint32_t surface_id);

/**
 * Read the UTF-8 text payload for an IME event.
 *
 * For IME_START / IME_UPDATE the text is the current preedit.
 * For IME_COMMIT the text is the finalized bytes to insert.
 * For IME_CANCEL the text length is zero.
 *
 * @param sequence    Sequence id from wapi_ime_event_t.sequence.
 * @param buf         Caller-owned buffer.
 * @param buf_len     Size of buf in bytes.
 * @param out_len     [out] Actual byte length, even if it exceeds
 *                    buf_len. Callers can size buf from
 *                    wapi_ime_event_t.text_len to avoid truncation.
 * @return WAPI_OK on success,
 *         WAPI_ERR_NOENT if the sequence is no longer valid,
 *         WAPI_ERR_RANGE if buf_len is smaller than the payload
 *                        (bytes up to buf_len are still written).
 */
WAPI_IMPORT(wapi_input, ime_read_text)
wapi_result_t wapi_input_ime_read_text(uint64_t sequence,
                                       char* buf, uint32_t buf_len,
                                       uint32_t* out_len);

/**
 * Read a single attribute segment from an IME event payload.
 *
 * Segments are indexed 0..wapi_ime_event_t.segment_count-1 and
 * describe non-overlapping ranges of the preedit text.
 *
 * @param sequence    Sequence id from wapi_ime_event_t.sequence.
 * @param index       Segment index.
 * @param out         [out] Segment descriptor.
 * @return WAPI_OK on success,
 *         WAPI_ERR_NOENT if the sequence is no longer valid,
 *         WAPI_ERR_RANGE if index is out of bounds.
 */
WAPI_IMPORT(wapi_input, ime_read_segment)
wapi_result_t wapi_input_ime_read_segment(uint64_t sequence, uint32_t index,
                                          wapi_ime_segment_t* out);

/* --- wapi_ime_segment_t (12 bytes, align 4) --- */
_Static_assert(offsetof(wapi_ime_segment_t, start)  == 0, "");
_Static_assert(offsetof(wapi_ime_segment_t, length) == 4, "");
_Static_assert(offsetof(wapi_ime_segment_t, flags)  == 8, "");
_Static_assert(sizeof(wapi_ime_segment_t) == 12, "wapi_ime_segment_t must be 12 bytes");
_Static_assert(_Alignof(wapi_ime_segment_t) == 4, "wapi_ime_segment_t must be 4-byte aligned");

/* ============================================================
 * Global Hotkeys
 * ============================================================
 * Global input bindings that fire even when the window is not
 * focused. Device-agnostic: a binding can trigger off a keyboard
 * chord, a gamepad button, or an arbitrary HID usage. Because
 * hotkeys build on the same device taxonomy as the rest of this
 * module, they live here rather than in a separate capability.
 *
 * Hotkeys are registered with a binding descriptor. On trigger,
 * the host emits WAPI_EVENT_HOTKEY carrying the id returned by
 * register. Modules look up what the id means from their own
 * state — the event intentionally carries no input payload so the
 * same handler works for any source.
 *
 * Platform backing:
 *   Keyboard: RegisterHotKey (Windows), CGEventTap (macOS),
 *             XGrabKey (Linux/X11).
 *   Gamepad:  Polled while the device is visible to the app
 *             (typically works across focus boundaries).
 *   HID:      Monitor reports on a granted HID device.
 *
 * Not every device type supports focus-independent binding on
 * every platform. Register returns WAPI_ERR_NOTSUP in that case.
 */

/* ============================================================
 * Hotkey Binding Descriptor (16 bytes, align 4)
 * ============================================================
 * Wire-format, no embedded pointers. Fields are source-tagged:
 * only the ones relevant to `role_kind` are read by the host.
 *
 * Layout:
 *   0:  role_kind   u32   wapi_role_kind_t. Only KEYBOARD,
 *                         GAMEPAD, and HID are valid for hotkeys;
 *                         other values return WAPI_ERR_NOTSUP.
 *   4:  device_id   u32   Endpoint handle from a granted role; 0
 *                         = any device of this kind. Keyboard
 *                         bindings on most platforms collapse to
 *                         "any keyboard" regardless of this field
 *                         because RegisterHotKey / XGrabKey do not
 *                         disambiguate attached keyboards.
 *   8:  modifiers   u32   Keyboard: WAPI_KMOD_* bitmask.
 *                         Other kinds: 0.
 *  12:  code        u32   Kind-specific trigger code:
 *                           Keyboard: wapi_scancode_t
 *                           Gamepad:  wapi_gamepad_button_t
 *                           HID:      (usage_page << 16) | usage
 */
typedef struct wapi_hotkey_binding_t {
    uint32_t    role_kind;
    uint32_t    device_id;
    uint32_t    modifiers;
    uint32_t    code;
} wapi_hotkey_binding_t;

/* Convenience: pack a HID (usage_page, usage) pair into the code field */
#define WAPI_HOTKEY_HID_CODE(usage_page, usage) \
    (((uint32_t)(usage_page) << 16) | (uint32_t)(usage))

/**
 * Register a global input binding.
 *
 * When the binding fires the host emits WAPI_EVENT_HOTKEY with
 * the id returned in out_id. Modules map id back to meaning from
 * their own tables.
 *
 * @param binding  Binding descriptor. Must outlive the call only.
 * @param out_id   [out] Registration id.
 * @return WAPI_OK on success,
 *         WAPI_ERR_NOTSUP if the platform does not support a
 *                         focus-independent binding for this
 *                         device type,
 *         WAPI_ERR_BUSY   if an equivalent binding is already
 *                         registered on this platform,
 *         WAPI_ERR_BADF   if device_id is non-zero and refers to
 *                         a closed or unknown device.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, hotkey_register)
wapi_result_t wapi_input_hotkey_register(const wapi_hotkey_binding_t* binding,
                                         uint32_t* out_id);

/**
 * Unregister a previously registered binding.
 *
 * @param id  Registration id returned by wapi_input_hotkey_register.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid id.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_input, hotkey_unregister)
wapi_result_t wapi_input_hotkey_unregister(uint32_t id);

/* --- wapi_hotkey_binding_t (16 bytes, align 4) --- */
_Static_assert(offsetof(wapi_hotkey_binding_t, role_kind)   == 0,  "");
_Static_assert(offsetof(wapi_hotkey_binding_t, device_id)   == 4,  "");
_Static_assert(offsetof(wapi_hotkey_binding_t, modifiers)   == 8,  "");
_Static_assert(offsetof(wapi_hotkey_binding_t, code)        == 12, "");
_Static_assert(sizeof(wapi_hotkey_binding_t) == 16, "wapi_hotkey_binding_t must be 16 bytes");
_Static_assert(_Alignof(wapi_hotkey_binding_t) == 4, "wapi_hotkey_binding_t must be 4-byte aligned");

#ifdef __cplusplus
}
#endif

#endif /* WAPI_INPUT_H */
