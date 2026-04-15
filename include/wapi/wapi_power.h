/**
 * WAPI - Power Management Capability
 * Version 1.0.0
 *
 * The device's power-management subsystem: battery / AC source,
 * wake locks, user idle detection, OS power-saver mode, and
 * thermal throttling hints. All six concerns live here because
 * they are different facets of the same OS subsystem and apps
 * typically react to them together (e.g. drop framerate when
 * saver turns on *or* thermals rise *or* battery goes critical).
 *
 * Maps to:
 *   source:  Battery Status API / IOPowerSources / BatteryManager / SYSTEM_POWER_STATUS
 *   wake:    Screen Wake Lock API / IOPMAssertionCreateWithName / PowerManager.WakeLock / SetThreadExecutionState
 *   idle:    Idle Detection API / NSWorkspace / UserManager / GetLastInputInfo
 *   saver:   (no web std) / NSProcessInfo.lowPowerModeEnabled / PowerManager.isPowerSaveMode / EnergySaverStatus
 *   thermal: (no web std) / NSProcessInfo.thermalState / PowerManager.getCurrentThermalStatus
 *
 * Not covered here:
 *   - Per-device battery (e.g. gamepad) -- see wapi_input.h
 *     (wapi_gamepad_battery_t), which is a property of a specific
 *     input device, not the host's power subsystem.
 *   - Discrete vs integrated GPU selection -- see wapi_gpu.h
 *     (WAPI_GPU_POWER_LOWPOWER / HIGHPERF), which is a creation-time
 *     adapter hint, not a runtime advisory.
 *   - Thread scheduling / big.LITTLE placement -- see wapi_thread.h
 *     (wapi_thread_qos_t), which is a per-thread QoS hint that the
 *     OS scheduler uses for core placement.
 *
 * Import module: "wapi_power"
 *
 * Query availability with wapi_capability_supported("wapi.power", 12)
 */

#ifndef WAPI_POWER_H
#define WAPI_POWER_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Event Types (0x1310-0x132F)
 * ============================================================ */

#define WAPI_EVENT_POWER_CHANGED          0x1310  /* source or battery level */
#define WAPI_EVENT_POWER_SAVER_CHANGED    0x1311  /* OS saver mode toggled */
#define WAPI_EVENT_POWER_THERMAL_CHANGED  0x1312  /* thermal state changed */
#define WAPI_EVENT_POWER_IDLE_CHANGED     0x1320  /* user idle transition */

/* ============================================================
 * Power Source
 * ============================================================
 * Host power source + live battery level. SDL3 CategoryPower analogue. */

typedef enum wapi_power_source_t {
    WAPI_POWER_SOURCE_UNKNOWN  = 0,
    WAPI_POWER_SOURCE_BATTERY  = 1,  /* On battery, discharging */
    WAPI_POWER_SOURCE_AC       = 2,  /* AC, no battery present */
    WAPI_POWER_SOURCE_CHARGING = 3,  /* AC, battery charging */
    WAPI_POWER_SOURCE_CHARGED  = 4,  /* AC, battery full */
    WAPI_POWER_SOURCE_FORCE32  = 0x7FFFFFFF
} wapi_power_source_t;

/* Layout (16 bytes, align 4):
 *   Offset  0: uint32_t source             (wapi_power_source_t)
 *   Offset  4: float    battery_level      0.0..1.0, NaN if unknown
 *   Offset  8: float    seconds_remaining  Inf if N/A
 *   Offset 12: uint32_t _pad
 */
typedef struct wapi_power_info_t {
    uint32_t    source;
    float       battery_level;
    float       seconds_remaining;
    uint32_t    _pad;
} wapi_power_info_t;

_Static_assert(sizeof(wapi_power_info_t) == 16,
               "wapi_power_info_t must be 16 bytes");
_Static_assert(_Alignof(wapi_power_info_t) == 4,
               "wapi_power_info_t must be 4-byte aligned");

/**
 * Get current power source and battery state.
 *
 * @param info  [out] Power info.
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if no power info available.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_power, get_info)
wapi_result_t wapi_power_get_info(wapi_power_info_t* info);

/* ============================================================
 * Wake Lock
 * ============================================================
 * Prevent screen dimming or device sleep. Acquire returns an
 * opaque handle; release it when the reason no longer applies.
 * Wake locks stack: if two parts of the app hold a SCREEN lock,
 * the screen stays on until both are released. */

typedef enum wapi_power_wake_t {
    WAPI_POWER_WAKE_SCREEN  = 0,  /* Keep display on */
    WAPI_POWER_WAKE_SYSTEM  = 1,  /* Keep CPU awake, display may dim */
    WAPI_POWER_WAKE_FORCE32 = 0x7FFFFFFF
} wapi_power_wake_t;

/**
 * Acquire a wake lock.
 *
 * @param type  Wake lock type.
 * @param lock  [out] Wake lock handle.
 * @return WAPI_OK on success, WAPI_ERR_ACCES if not permitted.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_power, wake_acquire)
wapi_result_t wapi_power_wake_acquire(wapi_power_wake_t type,
                                      wapi_handle_t* lock);

/**
 * Release a wake lock.
 *
 * @param lock  Wake lock handle.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_power, wake_release)
wapi_result_t wapi_power_wake_release(wapi_handle_t lock);

/* ============================================================
 * User Idle Detection
 * ============================================================
 * Observe when the user stops interacting or when the screen
 * locks. Call start() once with the idle threshold; the host
 * then delivers WAPI_EVENT_POWER_IDLE_CHANGED events on every
 * transition. */

typedef enum wapi_power_idle_state_t {
    WAPI_POWER_IDLE_ACTIVE  = 0,  /* User is actively interacting */
    WAPI_POWER_IDLE_IDLE    = 1,  /* User has been idle beyond threshold */
    WAPI_POWER_IDLE_LOCKED  = 2,  /* Screen is locked */
    WAPI_POWER_IDLE_FORCE32 = 0x7FFFFFFF
} wapi_power_idle_state_t;

/**
 * Start monitoring for idle state changes.
 *
 * @param threshold_ms  Idle time threshold before ACTIVE -> IDLE.
 * @return WAPI_OK on success, WAPI_ERR_ACCES if not permitted.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_power, idle_start)
wapi_result_t wapi_power_idle_start(uint32_t threshold_ms);

/**
 * Stop monitoring for idle state changes.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_power, idle_stop)
wapi_result_t wapi_power_idle_stop(void);

/**
 * Get the current idle state.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_power, idle_get)
wapi_result_t wapi_power_idle_get(wapi_power_idle_state_t* state);

/* ============================================================
 * OS Power-Saver Mode
 * ============================================================
 * Advisory signal: the OS or user has asked the app to back off.
 * On iOS this is Low Power Mode (triggers at 20% by default), on
 * Android PowerManager.isPowerSaveMode, on Windows EnergySaverStatus,
 * on macOS NSProcessInfo.lowPowerModeEnabled. Apps should respond
 * by reducing background work, capping framerate, and skipping
 * non-essential animations. */

typedef enum wapi_power_saver_t {
    WAPI_POWER_SAVER_OFF      = 0,  /* Normal operation */
    WAPI_POWER_SAVER_ON       = 1,  /* OS low-power mode active */
    WAPI_POWER_SAVER_CRITICAL = 2,  /* Battery critical, aggressive throttle */
    WAPI_POWER_SAVER_FORCE32  = 0x7FFFFFFF
} wapi_power_saver_t;

/**
 * Get the current OS power-saver state.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_power, saver_get)
wapi_result_t wapi_power_saver_get(wapi_power_saver_t* state);

/* ============================================================
 * Thermal State
 * ============================================================
 * Advisory signal mirroring NSProcessInfo.thermalState and
 * PowerManager.getCurrentThermalStatus. As temperature climbs
 * the OS will begin throttling; well-behaved apps voluntarily
 * reduce workload before they get throttled involuntarily. */

typedef enum wapi_power_thermal_t {
    WAPI_POWER_THERMAL_NOMINAL  = 0,
    WAPI_POWER_THERMAL_FAIR     = 1,
    WAPI_POWER_THERMAL_SERIOUS  = 2,  /* Reduce workload */
    WAPI_POWER_THERMAL_CRITICAL = 3,  /* System may throttle/shutdown */
    WAPI_POWER_THERMAL_FORCE32  = 0x7FFFFFFF
} wapi_power_thermal_t;

/**
 * Get the current thermal state.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_power, thermal_get)
wapi_result_t wapi_power_thermal_get(wapi_power_thermal_t* state);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_POWER_H */
