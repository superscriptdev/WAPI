/**
 * WAPI - Idle Detection Capability
 * Version 1.0.0
 *
 * Detect when the user is idle or the screen is locked.
 *
 * Maps to: Idle Detection API (Web), NSWorkspace (macOS),
 *          PowerManager/UserManager (Android), GetLastInputInfo (Windows)
 *
 * Import module: "wapi_idle"
 *
 * Query availability with wapi_capability_supported("wapi.idle", 9)
 */

#ifndef WAPI_IDLE_H
#define WAPI_IDLE_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Event Types
 * ============================================================ */

#define WAPI_EVENT_IDLE_CHANGED 0x1320

/* ============================================================
 * Idle State
 * ============================================================ */

typedef enum wapi_idle_state_t {
    WAPI_IDLE_ACTIVE  = 0,  /* User is actively interacting */
    WAPI_IDLE_IDLE    = 1,  /* User has been idle beyond threshold */
    WAPI_IDLE_LOCKED  = 2,  /* Screen is locked */
    WAPI_IDLE_FORCE32 = 0x7FFFFFFF
} wapi_idle_state_t;

/* ============================================================
 * Idle Detection Functions
 * ============================================================ */

/**
 * Start monitoring for idle state changes.
 * The host will deliver WAPI_EVENT_IDLE_CHANGED events when the
 * user's idle state transitions.
 *
 * @param threshold_ms  Idle time threshold in milliseconds before
 *                      transitioning from ACTIVE to IDLE.
 * @return WAPI_OK on success, WAPI_ERR_ACCES if not permitted.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_idle, start)
wapi_result_t wapi_idle_start(uint32_t threshold_ms);

/**
 * Stop monitoring for idle state changes.
 *
 * @return WAPI_OK on success.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_idle, stop)
wapi_result_t wapi_idle_stop(void);

/**
 * Get the current idle state.
 *
 * @param state  [out] Current idle state.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_idle, get_state)
wapi_result_t wapi_idle_get_state(wapi_idle_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_IDLE_H */
