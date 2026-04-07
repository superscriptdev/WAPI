/**
 * WAPI - Battery Status Capability
 * Version 1.0.0
 *
 * Query battery level and charging state.
 *
 * Maps to: Battery Status API (Web), UIDevice.batteryLevel (iOS),
 *          BatteryManager (Android), SYSTEM_POWER_STATUS (Windows)
 *
 * Import module: "wapi_battery"
 *
 * Query availability with wapi_capability_supported("wapi.battery", 12)
 */

#ifndef WAPI_BATTERY_H
#define WAPI_BATTERY_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Event Types
 * ============================================================ */

#define WAPI_EVENT_BATTERY_CHANGED 0x1310

/* ============================================================
 * Battery Info
 * ============================================================
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: float    level              0.0 (empty) to 1.0 (full)
 *   Offset  4: uint32_t charging           Non-zero if charging
 *   Offset  8: float    charging_time_s    Seconds until full (Inf if N/A)
 *   Offset 12: float    discharging_time_s Seconds until empty (Inf if N/A)
 */

typedef struct wapi_battery_info_t {
    float       level;
    uint32_t    charging;
    float       charging_time_s;
    float       discharging_time_s;
} wapi_battery_info_t;

_Static_assert(sizeof(wapi_battery_info_t) == 16,
               "wapi_battery_info_t must be 16 bytes");

/* ============================================================
 * Battery Functions
 * ============================================================ */

/**
 * Get current battery status information.
 *
 * @param info  [out] Battery information.
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if no battery present.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_battery, get_info)
wapi_result_t wapi_battery_get_info(wapi_battery_info_t* info);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_BATTERY_H */
