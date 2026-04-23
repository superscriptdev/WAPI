/**
 * WAPI Desktop Runtime - Win32 Gamepad Backend (XInput + HID)
 *
 * Slots 0..3 are XInput (Xbox 360 / One / Series). Slots 4..15 are
 * routed to the HID backend in `wapi_plat_win32_gamepad_hid.c` which
 * handles DualSense, DualShock4, Switch Pro, and the long tail of
 * SDL_GameControllerDB-mapped pads.
 *
 * Polled model: XInput has no event stream; we diff per-pump. The HID
 * backend has its own OVERLAPPED read pump but is drained from the
 * same poll tick so the event ring is single-threaded.
 */

#include "wapi_plat.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>
#include <string.h>

#pragma comment(lib, "xinput")

/* Canonical WAPI gamepad button indices (match wapi_input.h). */
#define WAPI_BTN_A              0
#define WAPI_BTN_B              1
#define WAPI_BTN_X              2
#define WAPI_BTN_Y              3
#define WAPI_BTN_BACK           4
#define WAPI_BTN_GUIDE          5
#define WAPI_BTN_START          6
#define WAPI_BTN_LSTICK         7
#define WAPI_BTN_RSTICK         8
#define WAPI_BTN_LSHOULDER      9
#define WAPI_BTN_RSHOULDER     10
#define WAPI_BTN_DPAD_UP       11
#define WAPI_BTN_DPAD_DOWN     12
#define WAPI_BTN_DPAD_LEFT     13
#define WAPI_BTN_DPAD_RIGHT    14

#define WAPI_AXIS_LEFTX         0
#define WAPI_AXIS_LEFTY         1
#define WAPI_AXIS_RIGHTX        2
#define WAPI_AXIS_RIGHTY        3
#define WAPI_AXIS_LTRIGGER      4
#define WAPI_AXIS_RTRIGGER      5

typedef struct pad_slot_t {
    bool     connected;
    DWORD    last_packet;
    uint16_t last_buttons;
    int16_t  last_axes[6];
} pad_slot_t;

static pad_slot_t s_slots[XUSER_MAX_COUNT];
static bool       s_xinput_inited;

/* Bridging symbols into wapi_plat_win32.c. */
extern void wapi_plat_win32_push_event(const wapi_plat_event_t* ev);
extern uint64_t wapi_plat_win32_now_ns(void);

/* HID backend. Implemented in wapi_plat_win32_gamepad_hid.c. */
extern void    wapi_plat_win32_hid_gamepad_poll(void);
extern void    wapi_plat_win32_hid_gamepad_shutdown(void);
extern bool    wapi_plat_win32_hid_gamepad_connected(uint32_t slot);
extern bool    wapi_plat_win32_hid_gamepad_button_pressed(uint32_t slot, uint8_t button);
extern int16_t wapi_plat_win32_hid_gamepad_axis(uint32_t slot, uint8_t axis);
extern bool    wapi_plat_win32_hid_gamepad_get_info(uint32_t slot, wapi_plat_gamepad_info_t* out);
extern bool    wapi_plat_win32_hid_gamepad_rumble(uint32_t slot, uint16_t lo, uint16_t hi, uint32_t dur);
extern bool    wapi_plat_win32_hid_gamepad_rumble_triggers(uint32_t slot, uint16_t l, uint16_t r, uint32_t dur);
extern bool    wapi_plat_win32_hid_gamepad_set_led(uint32_t slot, uint8_t r, uint8_t g, uint8_t b);
extern bool    wapi_plat_win32_hid_gamepad_enable_sensor(uint32_t slot, uint32_t sensor, bool en);
extern bool    wapi_plat_win32_hid_gamepad_get_sensor(uint32_t slot, uint32_t sensor, float out[3]);
extern bool    wapi_plat_win32_hid_gamepad_get_touchpad_finger(uint32_t slot, uint32_t tp,
                                                               uint32_t finger, bool* out_down,
                                                               float* out_x, float* out_y, float* out_pressure);
extern uint8_t wapi_plat_win32_hid_gamepad_battery_percent(uint32_t slot);

/* Map the 14 XInput button bits to our canonical index. */
static const uint8_t kXButtonToWapi[16] = {
    [0]  = WAPI_BTN_DPAD_UP,
    [1]  = WAPI_BTN_DPAD_DOWN,
    [2]  = WAPI_BTN_DPAD_LEFT,
    [3]  = WAPI_BTN_DPAD_RIGHT,
    [4]  = WAPI_BTN_START,
    [5]  = WAPI_BTN_BACK,
    [6]  = WAPI_BTN_LSTICK,
    [7]  = WAPI_BTN_RSTICK,
    [8]  = WAPI_BTN_LSHOULDER,
    [9]  = WAPI_BTN_RSHOULDER,
    [10] = 0xFF,
    [11] = 0xFF,
    [12] = WAPI_BTN_A,
    [13] = WAPI_BTN_B,
    [14] = WAPI_BTN_X,
    [15] = WAPI_BTN_Y,
};

static void push_gpad_axis(uint32_t gid, uint8_t axis, int16_t value) {
    wapi_plat_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = WAPI_PLAT_EV_GPAD_AXIS;
    ev.timestamp_ns = wapi_plat_win32_now_ns();
    ev.u.gpad.gamepad_id = gid;
    ev.u.gpad.axis = axis;
    ev.u.gpad.axis_value = value;
    wapi_plat_win32_push_event(&ev);
}

static void push_gpad_button(uint32_t gid, uint8_t btn, bool down) {
    wapi_plat_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = down ? WAPI_PLAT_EV_GPAD_BUTTON_DOWN : WAPI_PLAT_EV_GPAD_BUTTON_UP;
    ev.timestamp_ns = wapi_plat_win32_now_ns();
    ev.u.gpad.gamepad_id = gid;
    ev.u.gpad.button = btn;
    wapi_plat_win32_push_event(&ev);
}

static void push_gpad_conn(uint32_t gid, bool added) {
    wapi_plat_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = added ? WAPI_PLAT_EV_GPAD_ADDED : WAPI_PLAT_EV_GPAD_REMOVED;
    ev.timestamp_ns = wapi_plat_win32_now_ns();
    ev.u.gpad.gamepad_id = gid;
    wapi_plat_win32_push_event(&ev);
}

/* ============================================================
 * XInput / HID routing
 * ============================================================
 * Slot range 0..3 belongs to XInput, 4..15 to HID. */
static inline bool is_xinput_slot(uint32_t slot) { return slot < XUSER_MAX_COUNT; }
static inline bool is_hid_slot(uint32_t slot)    { return slot >= XUSER_MAX_COUNT && slot < WAPI_PLAT_GAMEPAD_SLOTS; }

/* ============================================================
 * Snapshot accessors
 * ============================================================ */

bool wapi_plat_gamepad_connected(uint32_t slot) {
    if (is_xinput_slot(slot)) return s_slots[slot].connected;
    if (is_hid_slot(slot))    return wapi_plat_win32_hid_gamepad_connected(slot);
    return false;
}

bool wapi_plat_gamepad_button_pressed(uint32_t slot, uint8_t button) {
    if (is_xinput_slot(slot)) {
        if (!s_slots[slot].connected) return false;
        for (int bit = 0; bit < 16; bit++) {
            if (kXButtonToWapi[bit] == button) {
                return (s_slots[slot].last_buttons & (1u << bit)) != 0;
            }
        }
        return false;
    }
    if (is_hid_slot(slot)) return wapi_plat_win32_hid_gamepad_button_pressed(slot, button);
    return false;
}

int16_t wapi_plat_gamepad_axis_value(uint32_t slot, uint8_t axis) {
    if (is_xinput_slot(slot)) {
        if (!s_slots[slot].connected || axis >= 6) return 0;
        return s_slots[slot].last_axes[axis];
    }
    if (is_hid_slot(slot)) return wapi_plat_win32_hid_gamepad_axis(slot, axis);
    return 0;
}

bool wapi_plat_gamepad_get_info(uint32_t slot, wapi_plat_gamepad_info_t* out) {
    if (!out) return false;
    if (is_xinput_slot(slot)) {
        if (!s_slots[slot].connected) return false;
        memset(out, 0, sizeof(*out));
        /* Probe the subtype via XInputGetCapabilities so we return a
         * meaningful type (Xbox 360 vs One vs generic). */
        XINPUT_CAPABILITIES cap = {0};
        out->type = 2 /* WAPI_GAMEPAD_TYPE_XBOX360 */;
        if (XInputGetCapabilities(slot, 0, &cap) == ERROR_SUCCESS) {
            /* XINPUT_DEVSUBTYPE_GAMEPAD=1; anything exotic reuses _STANDARD. */
            if (cap.SubType == 1 /* gamepad */) out->type = 2;
            else                                 out->type = 1 /* STANDARD */;
        }
        out->has_rumble = 1;
        out->has_trigger_rumble = 0;   /* no stock Win32 API; XInputSetStateEx is undocumented */
        out->has_led = 0;
        out->has_sensors = 0;
        out->has_touchpad = 0;
        uint8_t bp = wapi_plat_gamepad_battery_percent(slot);
        out->battery_percent = bp;
        if (bp == 255)      out->battery_state = 0 /* UNKNOWN */;
        else if (bp == 100) out->battery_state = 4 /* WIRED */;
        else                out->battery_state = 1 /* DISCHARGING */;
        return true;
    }
    if (is_hid_slot(slot)) return wapi_plat_win32_hid_gamepad_get_info(slot, out);
    return false;
}

bool wapi_plat_gamepad_rumble(uint32_t slot, uint16_t low_freq, uint16_t high_freq,
                              uint32_t duration_ms) {
    (void)duration_ms;
    if (is_xinput_slot(slot)) {
        if (!s_slots[slot].connected) return false;
        XINPUT_VIBRATION v;
        v.wLeftMotorSpeed  = low_freq;
        v.wRightMotorSpeed = high_freq;
        return XInputSetState(slot, &v) == ERROR_SUCCESS;
    }
    if (is_hid_slot(slot))
        return wapi_plat_win32_hid_gamepad_rumble(slot, low_freq, high_freq, duration_ms);
    return false;
}

bool wapi_plat_gamepad_rumble_triggers(uint32_t slot, uint16_t left, uint16_t right,
                                       uint32_t duration_ms) {
    if (is_hid_slot(slot))
        return wapi_plat_win32_hid_gamepad_rumble_triggers(slot, left, right, duration_ms);
    /* XInput stock API has no trigger rumble; DirectInput force-feedback
     * would be the fallback but is DirectInput-only and overlaps badly
     * with XInput on Series pads. Deferred. */
    return false;
}

bool wapi_plat_gamepad_set_led(uint32_t slot, uint8_t r, uint8_t g, uint8_t b) {
    if (is_hid_slot(slot)) return wapi_plat_win32_hid_gamepad_set_led(slot, r, g, b);
    return false;
}

bool wapi_plat_gamepad_enable_sensor(uint32_t slot, uint32_t sensor, bool enabled) {
    if (is_hid_slot(slot)) return wapi_plat_win32_hid_gamepad_enable_sensor(slot, sensor, enabled);
    return false;
}

bool wapi_plat_gamepad_get_sensor(uint32_t slot, uint32_t sensor, float out_xyz[3]) {
    if (is_hid_slot(slot)) return wapi_plat_win32_hid_gamepad_get_sensor(slot, sensor, out_xyz);
    return false;
}

bool wapi_plat_gamepad_get_touchpad_finger(uint32_t slot, uint32_t touchpad, uint32_t finger,
                                           bool* out_down, float* out_x, float* out_y,
                                           float* out_pressure) {
    if (is_hid_slot(slot))
        return wapi_plat_win32_hid_gamepad_get_touchpad_finger(slot, touchpad, finger,
                                                               out_down, out_x, out_y, out_pressure);
    return false;
}

uint8_t wapi_plat_gamepad_battery_percent(uint32_t slot) {
    if (is_xinput_slot(slot)) {
        if (!s_slots[slot].connected) return 255;
        XINPUT_BATTERY_INFORMATION bi;
        if (XInputGetBatteryInformation(slot, BATTERY_DEVTYPE_GAMEPAD, &bi) != ERROR_SUCCESS)
            return 255;
        if (bi.BatteryType == BATTERY_TYPE_WIRED) return 100;
        if (bi.BatteryType == BATTERY_TYPE_DISCONNECTED ||
            bi.BatteryType == BATTERY_TYPE_UNKNOWN) return 255;
        switch (bi.BatteryLevel) {
            case BATTERY_LEVEL_EMPTY:  return 5;
            case BATTERY_LEVEL_LOW:    return 25;
            case BATTERY_LEVEL_MEDIUM: return 55;
            case BATTERY_LEVEL_FULL:   return 95;
        }
        return 255;
    }
    if (is_hid_slot(slot)) return wapi_plat_win32_hid_gamepad_battery_percent(slot);
    return 255;
}

/* Poll both XInput and the HID backend each pump. */
void wapi_plat_win32_gamepad_poll(void) {
    if (!s_xinput_inited) {
        memset(s_slots, 0, sizeof(s_slots));
        s_xinput_inited = true;
    }

    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE st;
        DWORD rc = XInputGetState(i, &st);
        pad_slot_t* slot = &s_slots[i];

        if (rc != ERROR_SUCCESS) {
            if (slot->connected) {
                push_gpad_conn(i, false);
                memset(slot, 0, sizeof(*slot));
            }
            continue;
        }

        if (!slot->connected) {
            push_gpad_conn(i, true);
            slot->connected = true;
            slot->last_packet = 0xFFFFFFFFu;
        }

        if (st.dwPacketNumber == slot->last_packet) continue;
        slot->last_packet = st.dwPacketNumber;

        uint16_t b = st.Gamepad.wButtons;
        uint16_t changed = b ^ slot->last_buttons;
        if (changed) {
            for (int bit = 0; bit < 16; bit++) {
                uint16_t mask = (uint16_t)(1u << bit);
                if (!(changed & mask)) continue;
                uint8_t idx = kXButtonToWapi[bit];
                if (idx == 0xFF) continue;
                push_gpad_button(i, idx, (b & mask) != 0);
            }
            slot->last_buttons = b;
        }

        int16_t new_axes[6] = {
            st.Gamepad.sThumbLX,
            (int16_t)(-st.Gamepad.sThumbLY - 1),
            st.Gamepad.sThumbRX,
            (int16_t)(-st.Gamepad.sThumbRY - 1),
            (int16_t)((int)st.Gamepad.bLeftTrigger  * 32767 / 255),
            (int16_t)((int)st.Gamepad.bRightTrigger * 32767 / 255),
        };

        for (int a = 0; a < 6; a++) {
            if (new_axes[a] != slot->last_axes[a]) {
                push_gpad_axis(i, (uint8_t)a, new_axes[a]);
                slot->last_axes[a] = new_axes[a];
            }
        }
    }

    /* Drain the HID backend (reads completed + rescan). */
    wapi_plat_win32_hid_gamepad_poll();
}
