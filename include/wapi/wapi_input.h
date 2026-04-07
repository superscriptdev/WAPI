/**
 * WAPI - Input Devices
 * Version 1.0.0
 *
 * Unified input device enumeration, lifecycle, and state queries
 * for all device types: mouse, keyboard, touch, pen, gamepad, and HID.
 *
 * All devices share a common lifecycle:
 *   1. Enumerate: wapi_device_count(type)
 *   2. Open:      wapi_device_open(type, index, &handle)
 *   3. Query:     wapi_device_get_uid / get_name / get_type
 *   4. Use:       Device-specific state/action functions (below)
 *   5. Close:     wapi_device_close(handle)
 *
 * HID devices additionally support permission-gated acquisition
 * via wapi_hid_request_device(). Once granted, they appear in
 * wapi_device_count(WAPI_DEVICE_HID) and can be re-opened on
 * reconnect without re-prompting.
 *
 * Device connect/disconnect is reported via WAPI_EVENT_DEVICE_ADDED
 * and WAPI_EVENT_DEVICE_REMOVED (see wapi_event.h).
 *
 * Import module: "wapi_input"
 */

#ifndef WAPI_INPUT_H
#define WAPI_INPUT_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Device Types
 * ============================================================ */

typedef enum wapi_device_type_t {
    WAPI_DEVICE_MOUSE    = 0,
    WAPI_DEVICE_KEYBOARD = 1,
    WAPI_DEVICE_TOUCH    = 2,
    WAPI_DEVICE_PEN      = 3,
    WAPI_DEVICE_GAMEPAD  = 4,
    WAPI_DEVICE_HID      = 5,
    WAPI_DEVICE_FORCE32  = 0x7FFFFFFF
} wapi_device_type_t;

/* ============================================================
 * Unified Device Lifecycle
 * ============================================================
 * These functions work on any device type. Handles from different
 * device types are interchangeable for these common operations.
 *
 * Mouse index 0 is always the system aggregate pointer (combined
 * state of all mice). It is always present and never generates
 * DEVICE_REMOVED events.
 */

/**
 * Get the number of connected devices of a given type.
 *
 * @param type  Device type to count.
 * @return Number of connected devices (>= 0).
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_input, device_count)
int32_t wapi_device_count(wapi_device_type_t type);

/**
 * Open a device by type and index.
 *
 * @param type        Device type.
 * @param index       Device index (0-based).
 * @param out_handle  [out] Device handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, device_open)
wapi_result_t wapi_device_open(wapi_device_type_t type, int32_t index,
                               wapi_handle_t* out_handle);

/**
 * Close a device handle. Works on any device type.
 *
 * @param handle  Device handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_input, device_close)
wapi_result_t wapi_device_close(wapi_handle_t handle);

/**
 * Get the type of an open device.
 *
 * @param handle  Device handle.
 * @return Device type, or -1 on invalid handle.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_input, device_get_type)
wapi_device_type_t wapi_device_get_type(wapi_handle_t handle);

/**
 * Get the stable UID of an open device. The UID persists across
 * sessions for the same physical device, enabling reconnect tracking.
 *
 * @param handle  Device handle.
 * @param uid     [out] 16-byte UID buffer.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, device_get_uid)
wapi_result_t wapi_device_get_uid(wapi_handle_t handle, uint8_t uid[16]);

/**
 * Get the human-readable name of an open device.
 *
 * @param handle       Device handle.
 * @param buf          Buffer to write name into (UTF-8).
 * @param buf_len      Size of buffer in bytes.
 * @param name_len_ptr [out] Actual name length.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, device_get_name)
wapi_result_t wapi_device_get_name(wapi_handle_t handle, char* buf,
                                   wapi_size_t buf_len,
                                   wapi_size_t* name_len_ptr);

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
    WAPI_CURSOR_NOT_ALLOWED = 9,
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
WAPI_IMPORT(wapi_input, keyboard_get_mod_state)
uint16_t wapi_keyboard_get_mod_state(wapi_handle_t handle);

/* ---- Text Input ----
 * Text input is surface-scoped (not keyboard-scoped) because
 * IME composition targets a focused surface, not a specific
 * physical keyboard.
 */

/**
 * Start text input mode. Enables IME and generates
 * WAPI_EVENT_TEXT_INPUT events.
 *
 * Wasm signature: (i32) -> void
 */
WAPI_IMPORT(wapi_input, start_text_input)
void wapi_input_start_text_input(wapi_handle_t surface);

/**
 * Stop text input mode.
 *
 * Wasm signature: (i32) -> void
 */
WAPI_IMPORT(wapi_input, stop_text_input)
void wapi_input_stop_text_input(wapi_handle_t surface);

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

/* ---- Finger State ----
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: int32_t finger_index
 *   Offset  4: float   x             Normalized 0..1
 *   Offset  8: float   y             Normalized 0..1
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

/* ---- Gesture Type ---- */

typedef enum wapi_gesture_type_t {
    WAPI_GESTURE_PINCH       = 0,
    WAPI_GESTURE_ROTATE      = 1,
    WAPI_GESTURE_SWIPE_LEFT  = 2,
    WAPI_GESTURE_SWIPE_RIGHT = 3,
    WAPI_GESTURE_SWIPE_UP    = 4,
    WAPI_GESTURE_SWIPE_DOWN  = 5,
    WAPI_GESTURE_LONG_PRESS  = 6,
    WAPI_GESTURE_TAP         = 7,
    WAPI_GESTURE_DOUBLE_TAP  = 8,
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
    WAPI_PEN_AXIS_XTILT                 = 1,
    WAPI_PEN_AXIS_YTILT                 = 2,
    WAPI_PEN_AXIS_ROTATION              = 3,
    WAPI_PEN_AXIS_DISTANCE              = 4,
    WAPI_PEN_AXIS_SLIDER                = 5,
    WAPI_PEN_AXIS_TANGENTIAL_PRESSURE   = 6,
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
    WAPI_GAMEPAD_TYPE_XBOX_360            = 2,
    WAPI_GAMEPAD_TYPE_XBOX_ONE            = 3,
    WAPI_GAMEPAD_TYPE_PS3                 = 4,
    WAPI_GAMEPAD_TYPE_PS4                 = 5,
    WAPI_GAMEPAD_TYPE_PS5                 = 6,
    WAPI_GAMEPAD_TYPE_SWITCH_PRO          = 7,
    WAPI_GAMEPAD_TYPE_SWITCH_JOYCON_LEFT  = 8,
    WAPI_GAMEPAD_TYPE_SWITCH_JOYCON_RIGHT = 9,
    WAPI_GAMEPAD_TYPE_SWITCH_JOYCON_PAIR  = 10,
    WAPI_GAMEPAD_TYPE_FORCE32             = 0x7FFFFFFF
} wapi_gamepad_type_t;

/* ---- Power State ---- */

typedef enum wapi_power_state_t {
    WAPI_POWER_STATE_UNKNOWN    = 0,
    WAPI_POWER_STATE_ON_BATTERY = 1,
    WAPI_POWER_STATE_CHARGING   = 2,
    WAPI_POWER_STATE_CHARGED    = 3,
    WAPI_POWER_STATE_WIRED      = 4,
    WAPI_POWER_STATE_FORCE32    = 0x7FFFFFFF
} wapi_power_state_t;

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
 *   Offset 14: uint8_t  power_state       wapi_power_state_t
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
    uint8_t     power_state;
    uint8_t     _pad[1];
    uint8_t     _reserved[32];
} wapi_gamepad_info_t;

_Static_assert(sizeof(wapi_gamepad_info_t) == 48,
               "wapi_gamepad_info_t must be 48 bytes");

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
 * Get the power/battery info for a gamepad.
 *
 * @param handle       Gamepad device handle.
 * @param percent_ptr  [out] Battery percentage (0-100).
 * @return Power state (wapi_power_state_t) on success, negative on error.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, gamepad_get_power_info)
int32_t wapi_gamepad_get_power_info(wapi_handle_t handle,
                                    uint8_t* percent_ptr);

/* ============================================================
 *  ██╗  ██╗██╗██████╗
 *  ██║  ██║██║██╔══██╗
 *  ███████║██║██║  ██║
 *  ██╔══██║██║██║  ██║
 *  ██║  ██║██║██████╔╝
 *  ╚═╝  ╚═╝╚═╝╚═════╝
 * ============================================================
 * Raw HID device access for custom peripherals. HID devices
 * additionally support permission-gated acquisition via
 * wapi_hid_request_device(). Once granted, they appear in
 * wapi_device_count(WAPI_DEVICE_HID) for reconnect.
 */

/* ---- HID Info ----
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: uint16_t vendor_id
 *   Offset  2: uint16_t product_id
 *   Offset  4: uint16_t usage_page
 *   Offset  6: uint16_t usage
 *   Offset  8: uint8_t  _reserved[8]
 */

typedef struct wapi_hid_info_t {
    uint16_t    vendor_id;
    uint16_t    product_id;
    uint16_t    usage_page;
    uint16_t    usage;
    uint8_t     _reserved[8];
} wapi_hid_info_t;

_Static_assert(sizeof(wapi_hid_info_t) == 16,
               "wapi_hid_info_t must be 16 bytes");

/* ---- HID Functions ---- */

/**
 * Request access to a HID device (shows permission prompt).
 * On success, the device can subsequently be found via
 * wapi_device_count(WAPI_DEVICE_HID) and opened normally.
 *
 * @param vendor_id   USB vendor ID filter (0 = any).
 * @param product_id  USB product ID filter (0 = any).
 * @param usage_page  HID usage page filter (0 = any).
 * @param out_handle  [out] HID device handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, hid_request_device)
wapi_result_t wapi_hid_request_device(uint16_t vendor_id,
                                      uint16_t product_id,
                                      uint16_t usage_page,
                                      wapi_handle_t* out_handle);

/**
 * Get HID-specific info.
 *
 * @param handle    HID device handle.
 * @param info_ptr  [out] Pointer to wapi_hid_info_t.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if not a HID device.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_input, hid_get_info)
wapi_result_t wapi_hid_get_info(wapi_handle_t handle,
                                wapi_hid_info_t* info_ptr);

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

#ifdef __cplusplus
}
#endif

#endif /* WAPI_INPUT_H */
