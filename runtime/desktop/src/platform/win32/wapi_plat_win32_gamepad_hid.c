/**
 * WAPI Desktop Runtime - Win32 HID Gamepad Backend
 *
 * Covers every pad that does NOT enumerate through XInput: DualShock4,
 * DualSense, Switch Pro / JoyCons, and a long tail of generic HID pads
 * (flight sticks, fight pads, arcade sticks, third-party controllers).
 *
 * Shape
 * -----
 *   - Enumerates HID game controllers (usage page 0x01, usage 0x04/0x05)
 *     via SetupDi, filtering out XInput-owned devices by probing for
 *     the "IG_" substring in the device instance path.
 *   - Per-device driver dispatch:
 *       Sony DualSense  (0x054C / 0x0CE6, 0x0DF2)     — full decode
 *       Sony DualShock4 (0x054C / 0x05C4, 0x09CC, 0x0BA0) — full decode minus trigger-rumble
 *       Switch Pro      (0x057E / 0x2009)             — basic decode + LED
 *       everything else → generic HidP_* + SDL_GameControllerDB mapping
 *   - Reads are OVERLAPPED so the main poll can drain them without
 *     blocking. Each slot holds at most one pending read. Hot-plug is
 *     handled by rescanning when a read fails with DEVICE_NOT_CONNECTED
 *     and also on an explicit periodic tick.
 *
 * Emitted events match wapi_plat_win32_gamepad.c (same GPAD_ADDED /
 * REMOVED / AXIS / BUTTON_* types) so the translator in wapi_host_input.c
 * doesn't need to care whether a gamepad is XInput or HID.
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
#include "wapi_host.h"   /* wapi_gpdb_mapping_t, wapi_gamepaddb_resolve, wapi_gamepaddb_make_guid */

#include <windows.h>
#include <setupapi.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

#pragma comment(lib, "setupapi")

extern void     wapi_plat_win32_push_event(const wapi_plat_event_t* ev);
extern uint64_t wapi_plat_win32_now_ns(void);

/* ============================================================
 * WAPI-canonical button/axis enum shadows
 * ============================================================ */
#define BTN_A      0
#define BTN_B      1
#define BTN_X      2
#define BTN_Y      3
#define BTN_BACK   4
#define BTN_GUIDE  5
#define BTN_START  6
#define BTN_LS     7
#define BTN_RS     8
#define BTN_LSH    9
#define BTN_RSH   10
#define BTN_DU    11
#define BTN_DD    12
#define BTN_DL    13
#define BTN_DR    14

#define AX_LX     0
#define AX_LY     1
#define AX_RX     2
#define AX_RY     3
#define AX_LT     4
#define AX_RT     5

/* ============================================================
 * hid.dll function resolution (shared with wapi_plat_win32_hid.c
 * but that file keeps its table private; we resolve again here to
 * avoid a cross-file handshake for what is a handful of exports).
 * ============================================================ */

typedef struct _HIDD_ATTRIBUTES_T {
    ULONG  Size;
    USHORT VendorID;
    USHORT ProductID;
    USHORT VersionNumber;
} HIDD_ATTRIBUTES_T;

typedef struct _HIDP_CAPS_T {
    USHORT Usage;
    USHORT UsagePage;
    USHORT InputReportByteLength;
    USHORT OutputReportByteLength;
    USHORT FeatureReportByteLength;
    USHORT Reserved[17];
    USHORT NumberLinkCollectionNodes;
    USHORT NumberInputButtonCaps;
    USHORT NumberInputValueCaps;
    USHORT NumberInputDataIndices;
    USHORT NumberOutputButtonCaps;
    USHORT NumberOutputValueCaps;
    USHORT NumberOutputDataIndices;
    USHORT NumberFeatureButtonCaps;
    USHORT NumberFeatureValueCaps;
    USHORT NumberFeatureDataIndices;
} HIDP_CAPS_T;

typedef union _HIDP_BUTTON_OR_VALUE_RANGE_T {
    struct { USHORT UsageMin, UsageMax; USHORT StringMin, StringMax; USHORT DesignatorMin, DesignatorMax; USHORT DataIndexMin, DataIndexMax; } Range;
    struct { USHORT Usage, Reserved1;   USHORT StringIndex, Reserved2; USHORT DesignatorIndex, Reserved3; USHORT DataIndex, Reserved4; } NotRange;
} HIDP_BVR_T;

typedef struct _HIDP_VALUE_CAPS_T {
    USHORT UsagePage;
    UCHAR  ReportID;
    BOOLEAN IsAlias;
    USHORT BitField;
    USHORT LinkCollection;
    USHORT LinkUsage;
    USHORT LinkUsagePage;
    BOOLEAN IsRange;
    BOOLEAN IsStringRange;
    BOOLEAN IsDesignatorRange;
    BOOLEAN IsAbsolute;
    BOOLEAN HasNull;
    UCHAR  Reserved;
    USHORT BitSize;
    USHORT ReportCount;
    USHORT Reserved2[5];
    ULONG  UnitsExp;
    ULONG  Units;
    LONG   LogicalMin, LogicalMax;
    LONG   PhysicalMin, PhysicalMax;
    HIDP_BVR_T u;
} HIDP_VALUE_CAPS_T;

typedef struct _HIDP_BUTTON_CAPS_T {
    USHORT UsagePage;
    UCHAR  ReportID;
    BOOLEAN IsAlias;
    USHORT BitField;
    USHORT LinkCollection;
    USHORT LinkUsage;
    USHORT LinkUsagePage;
    BOOLEAN IsRange;
    BOOLEAN IsStringRange;
    BOOLEAN IsDesignatorRange;
    BOOLEAN IsAbsolute;
    BOOLEAN Reserved;
    USHORT ReportCount;
    USHORT Reserved2[4];
    ULONG  Reserved3[9];
    HIDP_BVR_T u;
} HIDP_BUTTON_CAPS_T;

#define HIDP_INPUT    0
#define HIDP_STATUS_SUCCESS 0x00110000

typedef LONG NTSTATUS_T;

typedef void    (__stdcall *pfn_HidD_GetHidGuid_t)(LPGUID);
typedef BOOLEAN (__stdcall *pfn_HidD_GetAttributes_t)(HANDLE, HIDD_ATTRIBUTES_T*);
typedef BOOLEAN (__stdcall *pfn_HidD_GetPreparsedData_t)(HANDLE, void**);
typedef BOOLEAN (__stdcall *pfn_HidD_FreePreparsedData_t)(void*);
typedef BOOLEAN (__stdcall *pfn_HidD_GetProductString_t)(HANDLE, PVOID, ULONG);
typedef BOOLEAN (__stdcall *pfn_HidD_SetOutputReport_t)(HANDLE, PVOID, ULONG);
typedef BOOLEAN (__stdcall *pfn_HidD_SetFeature_t)(HANDLE, PVOID, ULONG);

typedef NTSTATUS_T (__stdcall *pfn_HidP_GetCaps_t)(void*, HIDP_CAPS_T*);
typedef NTSTATUS_T (__stdcall *pfn_HidP_GetButtonCaps_t)(int, HIDP_BUTTON_CAPS_T*, USHORT*, void*);
typedef NTSTATUS_T (__stdcall *pfn_HidP_GetValueCaps_t)(int, HIDP_VALUE_CAPS_T*, USHORT*, void*);
typedef NTSTATUS_T (__stdcall *pfn_HidP_GetUsages_t)(int, USHORT, USHORT, USHORT*, ULONG*, void*, CHAR*, ULONG);
typedef NTSTATUS_T (__stdcall *pfn_HidP_GetUsageValue_t)(int, USHORT, USHORT, USHORT, ULONG*, void*, CHAR*, ULONG);

static struct {
    bool     initialized;
    bool     available;
    HMODULE  dll;
    GUID     hid_guid;
    pfn_HidD_GetHidGuid_t        HidD_GetHidGuid;
    pfn_HidD_GetAttributes_t     HidD_GetAttributes;
    pfn_HidD_GetPreparsedData_t  HidD_GetPreparsedData;
    pfn_HidD_FreePreparsedData_t HidD_FreePreparsedData;
    pfn_HidD_GetProductString_t  HidD_GetProductString;
    pfn_HidD_SetOutputReport_t   HidD_SetOutputReport;
    pfn_HidD_SetFeature_t        HidD_SetFeature;
    pfn_HidP_GetCaps_t           HidP_GetCaps;
    pfn_HidP_GetButtonCaps_t     HidP_GetButtonCaps;
    pfn_HidP_GetValueCaps_t      HidP_GetValueCaps;
    pfn_HidP_GetUsages_t         HidP_GetUsages;
    pfn_HidP_GetUsageValue_t     HidP_GetUsageValue;
} g_hid;

static bool hid_dll_init(void) {
    if (g_hid.initialized) return g_hid.available;
    g_hid.initialized = true;
    g_hid.dll = LoadLibraryW(L"hid.dll");
    if (!g_hid.dll) return false;
    #define R(n) g_hid.n = (pfn_##n##_t)GetProcAddress(g_hid.dll, #n)
    R(HidD_GetHidGuid); R(HidD_GetAttributes);
    R(HidD_GetPreparsedData); R(HidD_FreePreparsedData);
    R(HidD_GetProductString); R(HidD_SetOutputReport); R(HidD_SetFeature);
    R(HidP_GetCaps); R(HidP_GetButtonCaps); R(HidP_GetValueCaps);
    R(HidP_GetUsages); R(HidP_GetUsageValue);
    #undef R
    if (!g_hid.HidD_GetHidGuid || !g_hid.HidD_GetAttributes ||
        !g_hid.HidD_GetPreparsedData || !g_hid.HidP_GetCaps) return false;
    g_hid.HidD_GetHidGuid(&g_hid.hid_guid);
    g_hid.available = true;
    return true;
}

/* ============================================================
 * Per-slot gamepad record
 * ============================================================ */

#define HID_MAX_SLOTS  (WAPI_PLAT_GAMEPAD_SLOTS - 4)  /* 4 reserved for XInput */
#define HID_FIRST_SLOT 4
#define HID_REPORT_BUF 128

struct gp; typedef struct gp gp_t;
typedef struct gp_driver_t {
    uint32_t type;
    uint8_t  has_rumble, has_trigger_rumble, has_led, has_sensors, has_touchpad;
    bool (*init)   (gp_t*);                         /* optional post-open handshake */
    void (*parse)  (gp_t*, const uint8_t*, int);    /* consume one input report */
    void (*output) (gp_t*);                         /* flush LED/rumble state */
} gp_driver_t;

struct gp {
    bool            in_use;
    bool            connected;
    uint32_t        slot;
    HANDLE          h;
    WCHAR           path[260];
    uint16_t        vid, pid, version;
    char            name[128];
    const gp_driver_t* drv;

    /* preparsed HID data (generic driver only) */
    void*           preparsed;
    HIDP_CAPS_T     caps;
    wapi_gpdb_mapping_t mapping;
    bool            has_mapping;

    /* OVERLAPPED read slot */
    OVERLAPPED      ov;
    HANDLE          ov_event;
    uint8_t         read_buf[HID_REPORT_BUF];
    bool            read_pending;

    /* Last published state (for diffing) */
    uint16_t        last_buttons;    /* bit i = WAPI_GAMEPAD_BUTTON_i */
    int16_t         last_axes[6];

    /* Live queryable state */
    uint16_t        buttons;
    int16_t         axes[6];
    uint8_t         battery_percent; /* 0..100 or 255 */
    uint8_t         battery_state;

    /* Sensors (not zeroed when disabled — we just gate enabled flag) */
    bool            sensors_enabled[2];
    float           accel[3];
    float           gyro[3];

    /* Touchpad fingers (PS4/PS5 only) */
    struct { bool down; float x, y; } touch[2];

    /* Output pending state */
    uint16_t        out_rumble_lo, out_rumble_hi;
    uint16_t        out_trig_l,    out_trig_r;
    uint8_t         out_led_r, out_led_g, out_led_b;
    bool            output_dirty;
};

static gp_t  s_pads[HID_MAX_SLOTS];
static DWORD s_last_scan_ms = 0;

/* ---- helper: timestamped-now event push ---- */

static void push_conn(uint32_t slot, bool added) {
    wapi_plat_event_t ev; memset(&ev, 0, sizeof(ev));
    ev.type = added ? WAPI_PLAT_EV_GPAD_ADDED : WAPI_PLAT_EV_GPAD_REMOVED;
    ev.timestamp_ns = wapi_plat_win32_now_ns();
    ev.u.gpad.gamepad_id = slot;
    wapi_plat_win32_push_event(&ev);
}
static void push_btn(uint32_t slot, uint8_t btn, bool down) {
    wapi_plat_event_t ev; memset(&ev, 0, sizeof(ev));
    ev.type = down ? WAPI_PLAT_EV_GPAD_BUTTON_DOWN : WAPI_PLAT_EV_GPAD_BUTTON_UP;
    ev.timestamp_ns = wapi_plat_win32_now_ns();
    ev.u.gpad.gamepad_id = slot;
    ev.u.gpad.button = btn;
    wapi_plat_win32_push_event(&ev);
}
static void push_axis(uint32_t slot, uint8_t ax, int16_t v) {
    wapi_plat_event_t ev; memset(&ev, 0, sizeof(ev));
    ev.type = WAPI_PLAT_EV_GPAD_AXIS;
    ev.timestamp_ns = wapi_plat_win32_now_ns();
    ev.u.gpad.gamepad_id = slot;
    ev.u.gpad.axis = ax;
    ev.u.gpad.axis_value = v;
    wapi_plat_win32_push_event(&ev);
}

/* Diff `p->buttons / p->axes` against `p->last_*` and emit events. */
static void gp_publish(gp_t* p) {
    uint16_t changed = p->buttons ^ p->last_buttons;
    if (changed) {
        for (int i = 0; i < 15; i++) {
            if (changed & (1u << i)) {
                push_btn(p->slot, (uint8_t)i, (p->buttons & (1u << i)) != 0);
            }
        }
        p->last_buttons = p->buttons;
    }
    for (int i = 0; i < 6; i++) {
        if (p->axes[i] != p->last_axes[i]) {
            push_axis(p->slot, (uint8_t)i, p->axes[i]);
            p->last_axes[i] = p->axes[i];
        }
    }
}

/* ============================================================
 * Sony DualShock4 / DualSense shared helpers
 * ============================================================ */

/* Decode the 4-bit "dpad" nibble (0=N, 1=NE ... 7=NW, 8=neutral) into
 * four independent D-pad button bits (up/right/down/left). */
static uint16_t dpad_bits(uint8_t v) {
    v &= 0x0F;
    static const uint8_t k[9] = {
        (1<<0),                  (1<<0)|(1<<1),       (1<<1),
        (1<<1)|(1<<2),           (1<<2),              (1<<2)|(1<<3),
        (1<<3),                  (1<<3)|(1<<0),       0,
    };
    uint8_t m = (v <= 8) ? k[v] : 0;
    uint16_t out = 0;
    if (m & 1) out |= (1u << BTN_DU);
    if (m & 2) out |= (1u << BTN_DR);
    if (m & 4) out |= (1u << BTN_DD);
    if (m & 8) out |= (1u << BTN_DL);
    return out;
}

/* 8-bit unsigned stick axis [0..255] → signed int16 centered at 0.
 * DS4/DS5 report 128 as resting; invert Y so up is negative. */
static int16_t stick_u8_to_s16(uint8_t v, bool invert) {
    int s = (int)v - 128;
    if (invert) s = -s - 1;
    s *= 257;                    /* expand to -32768..32767 */
    if (s >  32767) s =  32767;
    if (s < -32768) s = -32768;
    return (int16_t)s;
}
static int16_t trigger_u8_to_s16(uint8_t v) {
    /* Trigger 0..255 → 0..32767. */
    return (int16_t)((int)v * 32767 / 255);
}

/* ============================================================
 * DualShock4 decoder (PS4 pad)
 * ============================================================ */
/* USB input report: report id 0x01, then 63 bytes.
 * Bytes (offset from start of data, after stripping report id):
 *   [ 0] LX    [ 1] LY    [ 2] RX    [ 3] RY
 *   [ 4] buttons1 (dpad + face)
 *   [ 5] buttons2 (shoulders, triggers-button, share/options, L3/R3)
 *   [ 6] buttons3 (PS, touchpad-click, counter bits)
 *   [ 7] L2      [ 8] R2
 *   [ 9..10] timestamp
 *   [11] battery
 *   [12..17] gyro (x,y,z) s16 LE
 *   [18..23] accel (x,y,z) s16 LE
 *   [32] touchpad packet counter
 *   [33..35] finger 1 (bit7 of [33]=NOT touching; bits[6..0]=counter;
 *                      [34..36] packed 12-bit x,y)
 *   [37..39] finger 2
 *
 * BT report 0x11 prepends 2 extra bytes; we skip the BT path in v1.
 */
static void ds4_parse(gp_t* p, const uint8_t* r, int len) {
    if (len < 12) return;
    int off = 0;
    if (r[0] == 0x01) { off = 1; len -= 1; r += 1; }
    /* Require enough bytes for the core block. */
    if (len < 10) return;

    p->axes[AX_LX] = stick_u8_to_s16(r[0], false);
    p->axes[AX_LY] = stick_u8_to_s16(r[1], true);
    p->axes[AX_RX] = stick_u8_to_s16(r[2], false);
    p->axes[AX_RY] = stick_u8_to_s16(r[3], true);
    p->axes[AX_LT] = trigger_u8_to_s16(r[7]);
    p->axes[AX_RT] = trigger_u8_to_s16(r[8]);

    uint8_t b1 = r[4], b2 = r[5], b3 = r[6];
    uint16_t btn = 0;
    btn |= dpad_bits(b1 & 0x0F);
    if (b1 & 0x10) btn |= (1u << BTN_X);      /* Square -> West */
    if (b1 & 0x20) btn |= (1u << BTN_A);      /* Cross  -> South */
    if (b1 & 0x40) btn |= (1u << BTN_B);      /* Circle -> East */
    if (b1 & 0x80) btn |= (1u << BTN_Y);      /* Triangle -> North */
    if (b2 & 0x01) btn |= (1u << BTN_LSH);    /* L1 */
    if (b2 & 0x02) btn |= (1u << BTN_RSH);    /* R1 */
    /* b2 & 0x04 = L2 button (digital) — covered by trigger axis */
    /* b2 & 0x08 = R2 button (digital) — covered by trigger axis */
    if (b2 & 0x10) btn |= (1u << BTN_BACK);   /* Share -> Back */
    if (b2 & 0x20) btn |= (1u << BTN_START);  /* Options -> Start */
    if (b2 & 0x40) btn |= (1u << BTN_LS);     /* L3 */
    if (b2 & 0x80) btn |= (1u << BTN_RS);     /* R3 */
    if (b3 & 0x01) btn |= (1u << BTN_GUIDE);  /* PS */
    /* b3 & 0x02 = Touchpad click (not part of the canonical 15) */
    p->buttons = btn;

    /* Battery byte: bit 4 = plugged-in; bits 0..3 = level 0..10. */
    if (len >= 12) {
        uint8_t be = r[11];
        uint8_t pct = (uint8_t)((be & 0x0F) * 10);
        if (pct > 100) pct = 100;
        p->battery_percent = pct;
        p->battery_state = (be & 0x10) ? 2 /* CHARGING */ : 1 /* DISCHARGING */;
    }

    /* Sensors */
    if (len >= 24 && p->sensors_enabled[0 /*ACCEL*/]) {
        int16_t ax = (int16_t)(r[18] | (r[19] << 8));
        int16_t ay = (int16_t)(r[20] | (r[21] << 8));
        int16_t az = (int16_t)(r[22] | (r[23] << 8));
        /* DS4 accel raw counts ~ 8192/g. Convert to m/s^2. */
        const float k = 9.80665f / 8192.0f;
        p->accel[0] = ax * k; p->accel[1] = ay * k; p->accel[2] = az * k;
    }
    if (len >= 18 && p->sensors_enabled[1 /*GYRO*/]) {
        int16_t gx = (int16_t)(r[12] | (r[13] << 8));
        int16_t gy = (int16_t)(r[14] | (r[15] << 8));
        int16_t gz = (int16_t)(r[16] | (r[17] << 8));
        /* DS4 gyro raw ~ 1024/(deg/s). Convert to rad/s. */
        const float k = 3.14159265f / (180.0f * 1024.0f);
        p->gyro[0] = gx * k; p->gyro[1] = gy * k; p->gyro[2] = gz * k;
    }

    /* Touchpad: 2 fingers */
    if (len >= 40) {
        for (int i = 0; i < 2; i++) {
            const uint8_t* f = r + 33 + i*4;
            bool down = (f[0] & 0x80) == 0;
            uint16_t x = (uint16_t)(f[1] | ((f[2] & 0x0F) << 8));
            uint16_t y = (uint16_t)(((f[2] & 0xF0) >> 4) | (f[3] << 4));
            p->touch[i].down = down;
            if (down) {
                p->touch[i].x = x / 1919.0f;   /* DS4 touchpad is 1920x942 */
                p->touch[i].y = y /  942.0f;
            }
        }
    }
}

/* Output report 0x05 / 0x11 — rumble + LED. */
static void ds4_output(gp_t* p) {
    uint8_t buf[32];
    memset(buf, 0, sizeof(buf));
    buf[0]  = 0x05;                              /* report id */
    buf[1]  = 0xF7;                              /* flags: enable rumble+LED+flashes */
    buf[3]  = 0x00;
    buf[4]  = (uint8_t)(p->out_rumble_hi >> 8);  /* small motor (high freq) */
    buf[5]  = (uint8_t)(p->out_rumble_lo >> 8);  /* large motor (low freq)  */
    buf[6]  = p->out_led_r;
    buf[7]  = p->out_led_g;
    buf[8]  = p->out_led_b;
    DWORD wr = 0;
    WriteFile(p->h, buf, 32, &wr, NULL);
}

static const gp_driver_t DRV_DS4 = {
    .type = 5 /* PS4 */, .has_rumble = 1, .has_trigger_rumble = 0,
    .has_led = 1, .has_sensors = 1, .has_touchpad = 1,
    .parse = ds4_parse, .output = ds4_output,
};

/* ============================================================
 * DualSense decoder (PS5 pad)
 * ============================================================ */
/* USB input report: report id 0x01, then 63 bytes. Layout differs
 * from DS4 but uses the same 8-bit sticks + 8-bit triggers prefix.
 * Offsets after stripping report id:
 *   [ 0] LX  [ 1] LY  [ 2] RX  [ 3] RY
 *   [ 4] L2  [ 5] R2
 *   [ 6] seq
 *   [ 7] buttons1  (face + dpad)
 *   [ 8] buttons2  (shoulders + triggers_btn + create + options + L3/R3)
 *   [ 9] buttons3  (PS, touchpad click, mute)
 *   [10..11] trigger status counters
 *   [15..20] gyro
 *   [21..26] accel
 *   [32]  touchpad timestamp
 *   [33..39] finger 1 / finger 2 (same packing as DS4)
 *   [52]  battery (level bottom 4 bits, bit 4 = charging)
 */
static void ds5_parse(gp_t* p, const uint8_t* r, int len) {
    if (len < 12) return;
    int off = 0;
    if (r[0] == 0x01) { off = 1; len -= 1; r += 1; }
    if (len < 10) return;

    p->axes[AX_LX] = stick_u8_to_s16(r[0], false);
    p->axes[AX_LY] = stick_u8_to_s16(r[1], true);
    p->axes[AX_RX] = stick_u8_to_s16(r[2], false);
    p->axes[AX_RY] = stick_u8_to_s16(r[3], true);
    p->axes[AX_LT] = trigger_u8_to_s16(r[4]);
    p->axes[AX_RT] = trigger_u8_to_s16(r[5]);

    uint8_t b1 = r[7], b2 = r[8], b3 = r[9];
    uint16_t btn = 0;
    btn |= dpad_bits(b1 & 0x0F);
    if (b1 & 0x10) btn |= (1u << BTN_X);
    if (b1 & 0x20) btn |= (1u << BTN_A);
    if (b1 & 0x40) btn |= (1u << BTN_B);
    if (b1 & 0x80) btn |= (1u << BTN_Y);
    if (b2 & 0x01) btn |= (1u << BTN_LSH);
    if (b2 & 0x02) btn |= (1u << BTN_RSH);
    if (b2 & 0x10) btn |= (1u << BTN_BACK);   /* Create */
    if (b2 & 0x20) btn |= (1u << BTN_START);  /* Options */
    if (b2 & 0x40) btn |= (1u << BTN_LS);
    if (b2 & 0x80) btn |= (1u << BTN_RS);
    if (b3 & 0x01) btn |= (1u << BTN_GUIDE);  /* PS */
    p->buttons = btn;

    if (len >= 27 && p->sensors_enabled[1 /*GYRO*/]) {
        int16_t gx = (int16_t)(r[15] | (r[16] << 8));
        int16_t gy = (int16_t)(r[17] | (r[18] << 8));
        int16_t gz = (int16_t)(r[19] | (r[20] << 8));
        const float k = 3.14159265f / (180.0f * 1024.0f);
        p->gyro[0] = gx * k; p->gyro[1] = gy * k; p->gyro[2] = gz * k;
    }
    if (len >= 27 && p->sensors_enabled[0 /*ACCEL*/]) {
        int16_t ax = (int16_t)(r[21] | (r[22] << 8));
        int16_t ay = (int16_t)(r[23] | (r[24] << 8));
        int16_t az = (int16_t)(r[25] | (r[26] << 8));
        const float k = 9.80665f / 8192.0f;
        p->accel[0] = ax * k; p->accel[1] = ay * k; p->accel[2] = az * k;
    }

    if (len >= 40) {
        for (int i = 0; i < 2; i++) {
            const uint8_t* f = r + 33 + i*4;
            bool down = (f[0] & 0x80) == 0;
            uint16_t x = (uint16_t)(f[1] | ((f[2] & 0x0F) << 8));
            uint16_t y = (uint16_t)(((f[2] & 0xF0) >> 4) | (f[3] << 4));
            p->touch[i].down = down;
            if (down) {
                p->touch[i].x = x / 1919.0f;
                p->touch[i].y = y / 1079.0f;   /* DS5 touchpad is 1920x1080 */
            }
        }
    }

    if (len >= 53) {
        uint8_t be = r[52];
        uint8_t pct = (uint8_t)((be & 0x0F) * 10);
        if (pct > 100) pct = 100;
        p->battery_percent = pct;
        p->battery_state = (be & 0x10) ? 2 : 1;
    }
}

/* Output report 0x02 (USB). Minimal form: rumble + LED + trigger effects
 * disabled (leave adaptive triggers at factory default). */
static void ds5_output(gp_t* p) {
    uint8_t buf[48];
    memset(buf, 0, sizeof(buf));
    buf[ 0] = 0x02;
    buf[ 1] = 0x03;  /* enable rumble (bit 0) + trigger effects (bit 1) */
    buf[ 2] = 0x15;  /* enable LED + color + player LEDs */
    buf[ 3] = (uint8_t)(p->out_rumble_hi >> 8);
    buf[ 4] = (uint8_t)(p->out_rumble_lo >> 8);
    /* Trigger-rumble: use "vibration" mode 0x21 with single-amplitude. */
    buf[11] = (p->out_trig_r ? 0x21 : 0x05);
    buf[12] = (uint8_t)(p->out_trig_r >> 8);
    buf[22] = (p->out_trig_l ? 0x21 : 0x05);
    buf[23] = (uint8_t)(p->out_trig_l >> 8);
    /* LED RGB at offset 45..47 */
    buf[45] = p->out_led_r;
    buf[46] = p->out_led_g;
    buf[47] = p->out_led_b;
    DWORD wr = 0;
    WriteFile(p->h, buf, 48, &wr, NULL);
}

static const gp_driver_t DRV_DS5 = {
    .type = 6 /* PS5 */, .has_rumble = 1, .has_trigger_rumble = 1,
    .has_led = 1, .has_sensors = 1, .has_touchpad = 1,
    .parse = ds5_parse, .output = ds5_output,
};

/* ============================================================
 * Switch Pro Controller decoder (minimal)
 * ============================================================ */
/* The Switch Pro boots in "simple HID" mode on USB — report id 0x3F
 * that carries packed buttons + dpad nibble + analog sticks on [1..8].
 * Full state requires switching to "standard full" (0x30) via command
 * 0x03, but the simple-HID path covers buttons/sticks for games that
 * don't need gyro. We send command 0x03 on init and downgrade to 0x3F
 * parsing if the pad answers with 0x3F (BT without handshake). */
static bool switchpro_init(gp_t* p) {
    /* Output report 0x80 command 0x04 enables USB comm; 0x80 0x02
     * handshake; 0x01 0x03 sets input report to 0x30 (standard full).
     * These are best-effort — the pad falls back cleanly if rejected. */
    uint8_t a[2] = {0x80, 0x02};       WriteFile(p->h, a, sizeof(a), NULL, NULL);
    uint8_t b[2] = {0x80, 0x04};       WriteFile(p->h, b, sizeof(b), NULL, NULL);
    uint8_t c[16]; memset(c, 0, sizeof(c));
    c[0] = 0x01; c[10] = 0x03; c[11] = 0x30;
    WriteFile(p->h, c, sizeof(c), NULL, NULL);
    return true;
}

/* Report 0x3F "simple HID" parsing. Layout:
 *   [0] report id (0x3F)
 *   [1..2] buttons (bitfield)
 *   [3]    dpad (0..8)
 *   [4..5] LX (u16 LE, 0..65535)
 *   [6..7] LY
 *   [8..9] RX
 *  [10..11] RY
 */
static void switchpro_parse(gp_t* p, const uint8_t* r, int len) {
    if (len < 12) return;
    if (r[0] != 0x3F) {
        /* 0x30 standard full — extremely involved decode (calibration,
         * IMU packet, battery). For v1 we just decode the button bits
         * shared with the simple format so the pad is usable. */
        if (r[0] == 0x30 && len >= 14) {
            /* Buttons at offsets 3,4,5; sticks at 6..8 / 9..11 (12-bit packed). */
            uint8_t b0 = r[3], b1 = r[4], b2 = r[5];
            uint16_t btn = 0;
            if (b0 & 0x08) btn |= (1u << BTN_Y);       /* Y == North */
            if (b0 & 0x04) btn |= (1u << BTN_X);
            if (b0 & 0x02) btn |= (1u << BTN_B);
            if (b0 & 0x01) btn |= (1u << BTN_A);
            if (b0 & 0x40) btn |= (1u << BTN_RSH);
            if (b0 & 0x80) btn |= (1u << BTN_RS);      /* ZR */
            if (b1 & 0x01) btn |= (1u << BTN_BACK);    /* Minus */
            if (b1 & 0x02) btn |= (1u << BTN_START);   /* Plus */
            if (b1 & 0x04) btn |= (1u << BTN_RS);
            if (b1 & 0x08) btn |= (1u << BTN_LS);
            if (b1 & 0x10) btn |= (1u << BTN_GUIDE);   /* Home */
            if (b2 & 0x01) btn |= (1u << BTN_DD);
            if (b2 & 0x02) btn |= (1u << BTN_DU);
            if (b2 & 0x04) btn |= (1u << BTN_DR);
            if (b2 & 0x08) btn |= (1u << BTN_DL);
            if (b2 & 0x40) btn |= (1u << BTN_LSH);
            if (b2 & 0x80) btn |= (1u << BTN_LS);      /* ZL */
            p->buttons = btn;
            uint32_t lx = r[6] | ((r[7] & 0x0F) << 8);
            uint32_t ly = (r[7] >> 4) | (r[8] << 4);
            uint32_t rx = r[9] | ((r[10] & 0x0F) << 8);
            uint32_t ry = (r[10] >> 4) | (r[11] << 4);
            p->axes[AX_LX] = (int16_t)(((int)lx - 2048) * 16);
            p->axes[AX_LY] = (int16_t)(((int)(2048 - (int)ly)) * 16);
            p->axes[AX_RX] = (int16_t)(((int)rx - 2048) * 16);
            p->axes[AX_RY] = (int16_t)(((int)(2048 - (int)ry)) * 16);
        }
        return;
    }
    uint16_t bits = (uint16_t)(r[1] | (r[2] << 8));
    uint16_t btn = 0;
    if (bits & 0x0001) btn |= (1u << BTN_B);
    if (bits & 0x0002) btn |= (1u << BTN_A);
    if (bits & 0x0004) btn |= (1u << BTN_Y);
    if (bits & 0x0008) btn |= (1u << BTN_X);
    if (bits & 0x0010) btn |= (1u << BTN_LSH);
    if (bits & 0x0020) btn |= (1u << BTN_RSH);
    if (bits & 0x0040) btn |= (1u << BTN_LS);   /* ZL maps to LS trigger button */
    if (bits & 0x0080) btn |= (1u << BTN_RS);
    if (bits & 0x0100) btn |= (1u << BTN_BACK);
    if (bits & 0x0200) btn |= (1u << BTN_START);
    if (bits & 0x0400) btn |= (1u << BTN_LS);
    if (bits & 0x0800) btn |= (1u << BTN_RS);
    if (bits & 0x1000) btn |= (1u << BTN_GUIDE);
    btn |= dpad_bits(r[3]);
    p->buttons = btn;
    /* sticks are u16 LE; center at 32768 */
    int lx = (r[4] | (r[5] << 8)) - 32768;
    int ly = (r[6] | (r[7] << 8)) - 32768;
    int rx = (r[8] | (r[9] << 8)) - 32768;
    int ry = (r[10] | (r[11] << 8)) - 32768;
    if (lx < -32768) lx = -32768; if (lx > 32767) lx = 32767;
    if (ly < -32768) ly = -32768; if (ly > 32767) ly = 32767;
    if (rx < -32768) rx = -32768; if (rx > 32767) rx = 32767;
    if (ry < -32768) ry = -32768; if (ry > 32767) ry = 32767;
    p->axes[AX_LX] = (int16_t)lx;
    p->axes[AX_LY] = (int16_t)(-ly - 1);
    p->axes[AX_RX] = (int16_t)rx;
    p->axes[AX_RY] = (int16_t)(-ry - 1);
}

static const gp_driver_t DRV_SWITCHPRO = {
    .type = 7 /* SwitchPro */, .has_rumble = 0, .has_trigger_rumble = 0,
    .has_led = 1, .has_sensors = 0, .has_touchpad = 0,
    .init = switchpro_init, .parse = switchpro_parse, .output = NULL,
};

/* ============================================================
 * Generic HID driver (HidP_* + SDL_GameControllerDB mapping)
 * ============================================================ */

/* For a given encoded-source field, read its value out of this report. */
static int generic_resolve(gp_t* p, uint16_t enc, bool* is_button,
                           int16_t* out_axis, bool* out_pressed)
{
    const void* pp = p->preparsed;
    uint8_t* buf = p->read_buf;
    int buf_len = p->caps.InputReportByteLength;
    if (buf_len == 0) buf_len = (int)sizeof(p->read_buf);

    uint32_t kind = enc & 0xF000;
    uint16_t idx  = enc & 0x007F;
    bool inverted = (enc & 0x0800) != 0;

    if (kind == 0x1000) {
        /* Button idx (SDL 1-based? SDL uses 0-based button ids mapped to
         * HID button usage 1+N). Query HidP_GetUsages with usage page
         * 0x09 (Buttons) and see if usage (idx+1) is present. */
        USHORT usages[64] = {0};
        ULONG  count = 64;
        if (g_hid.HidP_GetUsages(HIDP_INPUT, 0x09, 0, usages, &count,
                                 (void*)pp, (CHAR*)buf, (ULONG)buf_len) != HIDP_STATUS_SUCCESS) {
            return 0;
        }
        bool pressed = false;
        for (ULONG i = 0; i < count; i++) {
            if (usages[i] == (USHORT)(idx + 1)) { pressed = true; break; }
        }
        *is_button = true;
        *out_pressed = pressed;
        return 1;
    }
    if (kind == 0x2000 || kind == 0x2800) {
        /* Axis — SDL numbers axes in the order the HID descriptor exposes
         * value usages on page 0x01 (Generic Desktop). We built a
         * value-index table during generic_attach into p->mapping->(reserved).
         * Simpler: enumerate value caps each call and consume them in
         * order; return the idx'th one. Cheap enough — descriptors have
         * ~8 value caps. */
        USHORT n = p->caps.NumberInputValueCaps;
        if (n == 0) return 0;
        HIDP_VALUE_CAPS_T* caps = (HIDP_VALUE_CAPS_T*)_alloca(sizeof(HIDP_VALUE_CAPS_T) * n);
        USHORT got = n;
        if (g_hid.HidP_GetValueCaps(HIDP_INPUT, caps, &got, (void*)pp) != HIDP_STATUS_SUCCESS) return 0;
        /* Flatten ranges into individual axes by iteration order. */
        uint16_t seen = 0;
        for (USHORT i = 0; i < got; i++) {
            HIDP_VALUE_CAPS_T* c = &caps[i];
            USHORT page = c->UsagePage;
            USHORT u_lo = c->IsRange ? c->u.Range.UsageMin : c->u.NotRange.Usage;
            USHORT u_hi = c->IsRange ? c->u.Range.UsageMax : c->u.NotRange.Usage;
            for (USHORT u = u_lo; u <= u_hi; u++) {
                /* Treat Hat (usage 0x39) specially — it's handled by the
                 * hat encoding below, not as an axis. */
                if (page == 0x01 && u == 0x39) continue;
                if (seen == idx) {
                    ULONG raw = 0;
                    if (g_hid.HidP_GetUsageValue(HIDP_INPUT, page, 0, u, &raw,
                                                 (void*)pp, (CHAR*)buf, (ULONG)buf_len) == HIDP_STATUS_SUCCESS) {
                        LONG lmin = c->LogicalMin;
                        LONG lmax = c->LogicalMax;
                        if (lmax <= lmin) { *is_button = false; *out_axis = 0; return 1; }
                        /* Interpret raw as signed if LogicalMin < 0. */
                        LONG v = (LONG)raw;
                        if (lmin < 0 && c->BitSize < 32) {
                            LONG mask  = (1L << c->BitSize) - 1;
                            LONG sign  = (1L << (c->BitSize - 1));
                            v = (LONG)(raw & mask);
                            if (v & sign) v -= (1L << c->BitSize);
                        }
                        long range = lmax - lmin;
                        long norm  = (long)(((long long)(v - lmin) * 65535) / range) - 32768;
                        if (inverted) norm = -norm - 1;
                        if (norm >  32767) norm =  32767;
                        if (norm < -32768) norm = -32768;
                        *is_button = false;
                        *out_axis = (int16_t)norm;
                        return 1;
                    }
                    return 0;
                }
                seen++;
            }
        }
        return 0;
    }
    if (kind == 0x4000) {
        /* Hat: encoded as hat_idx (high byte) + mask (low nibble) */
        int hat_idx = (enc >> 8) & 0x7F;
        int mask    = enc & 0x0F;
        USHORT n = p->caps.NumberInputValueCaps;
        if (n == 0) return 0;
        HIDP_VALUE_CAPS_T* caps = (HIDP_VALUE_CAPS_T*)_alloca(sizeof(HIDP_VALUE_CAPS_T) * n);
        USHORT got = n;
        if (g_hid.HidP_GetValueCaps(HIDP_INPUT, caps, &got, (void*)pp) != HIDP_STATUS_SUCCESS) return 0;
        int seen = 0;
        for (USHORT i = 0; i < got; i++) {
            HIDP_VALUE_CAPS_T* c = &caps[i];
            USHORT u_lo = c->IsRange ? c->u.Range.UsageMin : c->u.NotRange.Usage;
            USHORT u_hi = c->IsRange ? c->u.Range.UsageMax : c->u.NotRange.Usage;
            for (USHORT u = u_lo; u <= u_hi; u++) {
                if (c->UsagePage == 0x01 && u == 0x39) {
                    if (seen == hat_idx) {
                        ULONG raw = 0;
                        if (g_hid.HidP_GetUsageValue(HIDP_INPUT, 0x01, 0, 0x39, &raw,
                                                     (void*)pp, (CHAR*)buf, (ULONG)buf_len) != HIDP_STATUS_SUCCESS) {
                            return 0;
                        }
                        /* Standard HID hat: 0=N,1=NE..7=NW,8=neutral (or LogicalMin=0,LogicalMax=7 */
                        int h = (int)raw - c->LogicalMin;
                        static const uint8_t bits[9] = {1,1|2,2,2|4,4,4|8,8,8|1,0};
                        int on = (h >= 0 && h <= 8) ? bits[h] : 0;
                        *is_button = true;
                        *out_pressed = (on & mask) != 0;
                        return 1;
                    }
                    seen++;
                }
            }
        }
    }
    return 0;
}

static void generic_parse(gp_t* p, const uint8_t* r, int len) {
    (void)r; (void)len;
    if (!p->has_mapping || !p->preparsed) return;
    uint16_t btn = 0;
    for (int i = 0; i < 15; i++) {
        uint16_t enc = p->mapping.buttons[i];
        if (!enc) continue;
        bool is_button = false, pressed = false;
        int16_t ax = 0;
        if (generic_resolve(p, enc, &is_button, &ax, &pressed)) {
            if (is_button ? pressed : ax > 16384) btn |= (1u << i);
        }
    }
    for (int i = 0; i < 6; i++) {
        uint16_t enc = p->mapping.axes[i];
        if (!enc) continue;
        bool is_button = false, pressed = false;
        int16_t ax = 0;
        if (!generic_resolve(p, enc, &is_button, &ax, &pressed)) continue;
        if (i == AX_LT || i == AX_RT) {
            if (is_button) ax = pressed ? 32767 : 0;
            else if (ax < 0) ax = 0;
        } else {
            if (is_button) ax = pressed ? 32767 : 0;
        }
        p->axes[i] = ax;
    }
    p->buttons = btn;
}

static const gp_driver_t DRV_GENERIC = {
    .type = 1 /* STANDARD */, .has_rumble = 0, .has_trigger_rumble = 0,
    .has_led = 0, .has_sensors = 0, .has_touchpad = 0,
    .parse = generic_parse, .output = NULL,
};

/* Pick the best driver for this device and install mapping state. */
static const gp_driver_t* pick_driver(gp_t* p) {
    if (p->vid == 0x054C) {
        if (p->pid == 0x0CE6 || p->pid == 0x0DF2) return &DRV_DS5;
        if (p->pid == 0x05C4 || p->pid == 0x09CC || p->pid == 0x0BA0) return &DRV_DS4;
    }
    if (p->vid == 0x057E && (p->pid == 0x2009 || p->pid == 0x2006 || p->pid == 0x2007))
        return &DRV_SWITCHPRO;
    /* Generic: load mapping from SDL DB. If we don't have one, we still
     * return the generic driver — the pad shows up as CONNECTED with
     * empty buttons/axes, same behavior as SDL fallback. */
    uint8_t guid[16];
    wapi_gamepaddb_make_guid(0x03, p->vid, p->pid, p->version, guid);
    p->has_mapping = wapi_gamepaddb_resolve(guid, &p->mapping);
    return &DRV_GENERIC;
}

/* ============================================================
 * Enumeration + hot-plug
 * ============================================================ */

static gp_t* gp_find_by_path(const WCHAR* path) {
    for (int i = 0; i < HID_MAX_SLOTS; i++) {
        if (s_pads[i].in_use && _wcsicmp(s_pads[i].path, path) == 0) return &s_pads[i];
    }
    return NULL;
}

static gp_t* gp_alloc_slot(void) {
    for (int i = 0; i < HID_MAX_SLOTS; i++) {
        if (!s_pads[i].in_use) {
            memset(&s_pads[i], 0, sizeof(s_pads[i]));
            s_pads[i].in_use = true;
            s_pads[i].slot = (uint32_t)(HID_FIRST_SLOT + i);
            s_pads[i].battery_percent = 255;
            return &s_pads[i];
        }
    }
    return NULL;
}

static void gp_close(gp_t* p) {
    if (!p || !p->in_use) return;
    if (p->read_pending) {
        CancelIoEx(p->h, &p->ov);
        DWORD dummy = 0;
        GetOverlappedResult(p->h, &p->ov, &dummy, TRUE);
    }
    if (p->ov_event) CloseHandle(p->ov_event);
    if (p->h && p->h != INVALID_HANDLE_VALUE) CloseHandle(p->h);
    if (p->preparsed) g_hid.HidD_FreePreparsedData(p->preparsed);
    bool was_connected = p->connected;
    uint32_t slot = p->slot;
    memset(p, 0, sizeof(*p));
    if (was_connected) push_conn(slot, false);
}

/* Start an OVERLAPPED read if none pending. */
static void gp_arm_read(gp_t* p) {
    if (p->read_pending) return;
    ResetEvent(p->ov_event);
    memset(&p->ov, 0, sizeof(p->ov));
    p->ov.hEvent = p->ov_event;
    DWORD got = 0;
    BOOL ok = ReadFile(p->h, p->read_buf, p->caps.InputReportByteLength ?
                       p->caps.InputReportByteLength : (DWORD)sizeof(p->read_buf),
                       &got, &p->ov);
    if (ok) {
        /* Synchronous completion — dispatch immediately. */
        if (p->drv && p->drv->parse) p->drv->parse(p, p->read_buf, (int)got);
        gp_publish(p);
    } else if (GetLastError() == ERROR_IO_PENDING) {
        p->read_pending = true;
    } else if (GetLastError() == ERROR_DEVICE_NOT_CONNECTED ||
               GetLastError() == ERROR_OPERATION_ABORTED ||
               GetLastError() == ERROR_INVALID_HANDLE) {
        gp_close(p);
    }
}

/* Check for pending-read completion and drain. */
static void gp_drain_read(gp_t* p) {
    if (!p->read_pending) { gp_arm_read(p); return; }
    if (WaitForSingleObject(p->ov_event, 0) != WAIT_OBJECT_0) return;
    DWORD got = 0;
    if (!GetOverlappedResult(p->h, &p->ov, &got, FALSE)) {
        DWORD e = GetLastError();
        p->read_pending = false;
        if (e == ERROR_DEVICE_NOT_CONNECTED || e == ERROR_OPERATION_ABORTED ||
            e == ERROR_INVALID_HANDLE) {
            gp_close(p);
            return;
        }
    } else {
        p->read_pending = false;
        if (p->drv && p->drv->parse) p->drv->parse(p, p->read_buf, (int)got);
        gp_publish(p);
    }
    if (p->output_dirty && p->drv && p->drv->output) {
        p->drv->output(p);
        p->output_dirty = false;
    }
    gp_arm_read(p);
}

/* Try to open a specific device-interface path as a gamepad. */
static bool gp_try_open(const WCHAR* path) {
    if (!hid_dll_init()) return false;
    /* Skip XInput-owned devices (Microsoft embeds "IG_" in the path). */
    if (wcsstr(path, L"IG_") || wcsstr(path, L"ig_")) return false;
    if (gp_find_by_path(path)) return true;

    HANDLE h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;

    HIDD_ATTRIBUTES_T attr = { .Size = sizeof(attr) };
    void* pp = NULL;
    HIDP_CAPS_T caps = {0};
    if (!g_hid.HidD_GetAttributes(h, &attr) ||
        !g_hid.HidD_GetPreparsedData(h, &pp) ||
        g_hid.HidP_GetCaps(pp, &caps) != HIDP_STATUS_SUCCESS) {
        if (pp) g_hid.HidD_FreePreparsedData(pp);
        CloseHandle(h);
        return false;
    }

    /* Gamepad usage page (0x01) + usage (0x04 Joystick or 0x05 Gamepad). */
    if (caps.UsagePage != 0x01 || (caps.Usage != 0x04 && caps.Usage != 0x05)) {
        g_hid.HidD_FreePreparsedData(pp);
        CloseHandle(h);
        return false;
    }

    gp_t* p = gp_alloc_slot();
    if (!p) { g_hid.HidD_FreePreparsedData(pp); CloseHandle(h); return false; }

    wcsncpy(p->path, path, 259); p->path[259] = 0;
    p->h = h;
    p->vid = attr.VendorID; p->pid = attr.ProductID; p->version = attr.VersionNumber;
    p->preparsed = pp;
    p->caps = caps;
    WCHAR wname[128] = {0};
    if (g_hid.HidD_GetProductString && g_hid.HidD_GetProductString(h, wname, sizeof(wname))) {
        WideCharToMultiByte(CP_UTF8, 0, wname, -1, p->name, (int)sizeof(p->name) - 1, NULL, NULL);
    }
    if (!p->name[0]) snprintf(p->name, sizeof(p->name), "HID Gamepad %04X:%04X", p->vid, p->pid);

    p->drv = pick_driver(p);
    p->ov_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!p->ov_event) { gp_close(p); return false; }

    if (p->drv->init) p->drv->init(p);

    p->connected = true;
    push_conn(p->slot, true);
    gp_arm_read(p);
    return true;
}

static void gp_rescan(void) {
    if (!hid_dll_init()) return;
    HDEVINFO set = SetupDiGetClassDevsW(&g_hid.hid_guid, NULL, NULL,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return;
    SP_DEVICE_INTERFACE_DATA ifd = { .cbSize = sizeof(ifd) };
    /* Track which paths we saw this scan so we can close stale slots. */
    bool seen[HID_MAX_SLOTS] = {0};
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(set, NULL, &g_hid.hid_guid, i, &ifd); i++) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifd, NULL, 0, &need, NULL);
        if (need == 0) continue;
        SP_DEVICE_INTERFACE_DETAIL_DATA_W* det =
            (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)calloc(1, need);
        if (!det) continue;
        det->cbSize = sizeof(*det);
        if (SetupDiGetDeviceInterfaceDetailW(set, &ifd, det, need, NULL, NULL)) {
            gp_t* existing = gp_find_by_path(det->DevicePath);
            if (existing) {
                seen[existing->slot - HID_FIRST_SLOT] = true;
            } else {
                if (gp_try_open(det->DevicePath)) {
                    gp_t* np = gp_find_by_path(det->DevicePath);
                    if (np) seen[np->slot - HID_FIRST_SLOT] = true;
                }
            }
        }
        free(det);
    }
    SetupDiDestroyDeviceInfoList(set);
    /* Any pad that disappeared from enumeration AND is not mid-read gets closed. */
    for (int i = 0; i < HID_MAX_SLOTS; i++) {
        if (s_pads[i].in_use && !seen[i]) gp_close(&s_pads[i]);
    }
}

/* ============================================================
 * Public entry points (called from wapi_plat_win32_gamepad.c)
 * ============================================================ */

void wapi_plat_win32_hid_gamepad_poll(void) {
    DWORD now = GetTickCount();
    /* Rescan once per second OR on first call. */
    if (s_last_scan_ms == 0 || (DWORD)(now - s_last_scan_ms) >= 1000) {
        gp_rescan();
        s_last_scan_ms = now;
    }
    for (int i = 0; i < HID_MAX_SLOTS; i++) {
        if (s_pads[i].in_use) gp_drain_read(&s_pads[i]);
    }
}

void wapi_plat_win32_hid_gamepad_shutdown(void) {
    for (int i = 0; i < HID_MAX_SLOTS; i++) {
        if (s_pads[i].in_use) gp_close(&s_pads[i]);
    }
}

/* ---- Queries + output ---- */

static gp_t* pad_for(uint32_t slot) {
    if (slot < HID_FIRST_SLOT) return NULL;
    int i = (int)(slot - HID_FIRST_SLOT);
    if (i < 0 || i >= HID_MAX_SLOTS) return NULL;
    if (!s_pads[i].in_use || !s_pads[i].connected) return NULL;
    return &s_pads[i];
}

bool wapi_plat_win32_hid_gamepad_connected(uint32_t slot) {
    return pad_for(slot) != NULL;
}

bool wapi_plat_win32_hid_gamepad_button_pressed(uint32_t slot, uint8_t btn) {
    gp_t* p = pad_for(slot); if (!p) return false;
    if (btn >= 15) return false;
    return (p->buttons & (1u << btn)) != 0;
}

int16_t wapi_plat_win32_hid_gamepad_axis(uint32_t slot, uint8_t ax) {
    gp_t* p = pad_for(slot); if (!p) return 0;
    if (ax >= 6) return 0;
    return p->axes[ax];
}

bool wapi_plat_win32_hid_gamepad_get_info(uint32_t slot, wapi_plat_gamepad_info_t* out) {
    gp_t* p = pad_for(slot); if (!p || !out) return false;
    memset(out, 0, sizeof(*out));
    out->type               = p->drv->type;
    out->vendor_id          = p->vid;
    out->product_id         = p->pid;
    out->has_rumble         = p->drv->has_rumble;
    out->has_trigger_rumble = p->drv->has_trigger_rumble;
    out->has_led            = p->drv->has_led;
    out->has_sensors        = p->drv->has_sensors;
    out->has_touchpad       = p->drv->has_touchpad;
    out->battery_percent    = p->battery_percent;
    out->battery_state      = p->battery_state;
    return true;
}

bool wapi_plat_win32_hid_gamepad_rumble(uint32_t slot, uint16_t lo, uint16_t hi, uint32_t dur) {
    (void)dur;
    gp_t* p = pad_for(slot); if (!p || !p->drv->has_rumble) return false;
    p->out_rumble_lo = lo; p->out_rumble_hi = hi;
    p->output_dirty = true;
    if (p->drv->output) p->drv->output(p);
    p->output_dirty = false;
    return true;
}

bool wapi_plat_win32_hid_gamepad_rumble_triggers(uint32_t slot, uint16_t l, uint16_t r, uint32_t dur) {
    (void)dur;
    gp_t* p = pad_for(slot); if (!p || !p->drv->has_trigger_rumble) return false;
    p->out_trig_l = l; p->out_trig_r = r;
    if (p->drv->output) p->drv->output(p);
    return true;
}

bool wapi_plat_win32_hid_gamepad_set_led(uint32_t slot, uint8_t r, uint8_t g, uint8_t b) {
    gp_t* p = pad_for(slot); if (!p || !p->drv->has_led) return false;
    p->out_led_r = r; p->out_led_g = g; p->out_led_b = b;
    if (p->drv->output) p->drv->output(p);
    return true;
}

bool wapi_plat_win32_hid_gamepad_enable_sensor(uint32_t slot, uint32_t sensor, bool en) {
    gp_t* p = pad_for(slot); if (!p || !p->drv->has_sensors) return false;
    if (sensor > 1) return false;
    p->sensors_enabled[sensor] = en;
    return true;
}

bool wapi_plat_win32_hid_gamepad_get_sensor(uint32_t slot, uint32_t sensor, float out[3]) {
    gp_t* p = pad_for(slot); if (!p || !p->drv->has_sensors || !out) return false;
    if (sensor == 0) { memcpy(out, p->accel, sizeof(p->accel)); return true; }
    if (sensor == 1) { memcpy(out, p->gyro,  sizeof(p->gyro));  return true; }
    return false;
}

bool wapi_plat_win32_hid_gamepad_get_touchpad_finger(uint32_t slot, uint32_t tp,
                                                     uint32_t finger, bool* out_down,
                                                     float* out_x, float* out_y, float* out_pressure)
{
    (void)tp;
    gp_t* p = pad_for(slot); if (!p || !p->drv->has_touchpad) return false;
    if (finger >= 2) return false;
    if (out_down) *out_down = p->touch[finger].down;
    if (out_x) *out_x = p->touch[finger].x;
    if (out_y) *out_y = p->touch[finger].y;
    if (out_pressure) *out_pressure = p->touch[finger].down ? 1.0f : 0.0f;
    return true;
}

uint8_t wapi_plat_win32_hid_gamepad_battery_percent(uint32_t slot) {
    gp_t* p = pad_for(slot); if (!p) return 255;
    return p->battery_percent;
}
