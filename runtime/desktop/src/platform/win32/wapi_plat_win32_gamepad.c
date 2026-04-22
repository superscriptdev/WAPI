/**
 * WAPI Desktop Runtime - Win32 Gamepad Backend (XInput)
 *
 * Phase 2: XInput only. Covers Xbox 360 / One / Series controllers
 * and most third-party pads that ship XInput drivers. Generic HID +
 * community mapping DB for DualShock/DualSense/Switch/flight sticks
 * is deferred — see NEXT_STEPS.md.
 *
 * Polled model: XInput has no event stream. We call XInputGetState
 * for all 4 slots each pump, diff against the previous snapshot,
 * and synthesize button/axis events.
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

/* Exposed from wapi_plat_win32.c. We push events into the shared ring
 * via the same path — so we need access. The simplest stable contract:
 * expose a push function from the core win32 backend. For now,
 * reimplement a minimal push that uses the ring via an exported
 * helper. We add `wapi_plat_win32_push_event` in wapi_plat_win32.c. */
extern void wapi_plat_win32_push_event(const wapi_plat_event_t* ev);
extern uint64_t wapi_plat_win32_now_ns(void);

/* Map the 14 XInput button bits to our canonical index. */
static const uint8_t kXButtonToWapi[16] = {
    [0]  = WAPI_BTN_DPAD_UP,    /* XINPUT_GAMEPAD_DPAD_UP         = 0x0001 bit 0 */
    [1]  = WAPI_BTN_DPAD_DOWN,  /* 0x0002 bit 1 */
    [2]  = WAPI_BTN_DPAD_LEFT,  /* 0x0004 bit 2 */
    [3]  = WAPI_BTN_DPAD_RIGHT, /* 0x0008 bit 3 */
    [4]  = WAPI_BTN_START,      /* 0x0010 */
    [5]  = WAPI_BTN_BACK,       /* 0x0020 */
    [6]  = WAPI_BTN_LSTICK,     /* 0x0040 */
    [7]  = WAPI_BTN_RSTICK,     /* 0x0080 */
    [8]  = WAPI_BTN_LSHOULDER,  /* 0x0100 */
    [9]  = WAPI_BTN_RSHOULDER,  /* 0x0200 */
    [10] = 0xFF,                /* reserved */
    [11] = 0xFF,                /* reserved */
    [12] = WAPI_BTN_A,          /* 0x1000 */
    [13] = WAPI_BTN_B,          /* 0x2000 */
    [14] = WAPI_BTN_X,          /* 0x4000 */
    [15] = WAPI_BTN_Y,          /* 0x8000 */
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
 * Snapshot accessors (read-only queries against s_slots)
 * ============================================================ */

bool wapi_plat_gamepad_connected(uint32_t slot) {
    if (slot >= XUSER_MAX_COUNT) return false;
    return s_slots[slot].connected;
}

bool wapi_plat_gamepad_button_pressed(uint32_t slot, uint8_t button) {
    if (slot >= XUSER_MAX_COUNT) return false;
    if (!s_slots[slot].connected) return false;
    /* Reverse-lookup: find the XInput bit that maps to this WAPI index. */
    for (int bit = 0; bit < 16; bit++) {
        if (kXButtonToWapi[bit] == button) {
            return (s_slots[slot].last_buttons & (1u << bit)) != 0;
        }
    }
    return false;
}

int16_t wapi_plat_gamepad_axis_value(uint32_t slot, uint8_t axis) {
    if (slot >= XUSER_MAX_COUNT) return 0;
    if (!s_slots[slot].connected) return 0;
    if (axis >= 6) return 0;
    return s_slots[slot].last_axes[axis];
}

bool wapi_plat_gamepad_rumble(uint32_t slot, uint16_t low_freq, uint16_t high_freq,
                              uint32_t duration_ms) {
    (void)duration_ms; /* XInput's XInputSetState has no duration; caller re-issues 0 to stop */
    if (slot >= XUSER_MAX_COUNT) return false;
    if (!s_slots[slot].connected) return false;
    XINPUT_VIBRATION v;
    v.wLeftMotorSpeed  = low_freq;
    v.wRightMotorSpeed = high_freq;
    return XInputSetState(slot, &v) == ERROR_SUCCESS;
}

uint8_t wapi_plat_gamepad_battery_percent(uint32_t slot) {
    if (slot >= XUSER_MAX_COUNT) return 255;
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

/* Called each pump; synthesizes gamepad events from polled state. */
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
            slot->last_packet = 0xFFFFFFFFu; /* force initial diff */
        }

        if (st.dwPacketNumber == slot->last_packet) continue;
        slot->last_packet = st.dwPacketNumber;

        /* Buttons */
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

        /* Axes — XInput sticks are int16 already (-32768..32767);
         * Y axes are inverted vs. our convention (XInput up = positive,
         * we want up = negative) — flip sign. */
        int16_t new_axes[6] = {
            st.Gamepad.sThumbLX,
            (int16_t)(-st.Gamepad.sThumbLY - 1),  /* invert, clamp -32768 */
            st.Gamepad.sThumbRX,
            (int16_t)(-st.Gamepad.sThumbRY - 1),
            /* Triggers are 0..255; scale to 0..32767 */
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
}
