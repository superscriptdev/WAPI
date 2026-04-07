/**
 * WAPI - Wake Lock Capability
 * Version 1.0.0
 *
 * Prevent screen dimming or device sleep while the application
 * needs to remain active.
 *
 * Maps to: Screen Wake Lock API (Web), IOPMAssertionCreateWithName (macOS),
 *          PowerManager.WakeLock (Android), SetThreadExecutionState (Windows)
 *
 * Import module: "wapi_wake"
 *
 * Query availability with wapi_capability_supported("wapi.wake_lock", 13)
 */

#ifndef WAPI_WAKE_LOCK_H
#define WAPI_WAKE_LOCK_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Wake Lock Types
 * ============================================================ */

typedef enum wapi_wake_type_t {
    WAPI_WAKE_SCREEN  = 0,  /* Keep screen on */
    WAPI_WAKE_SYSTEM  = 1,  /* Keep system awake (screen may dim) */
    WAPI_WAKE_FORCE32 = 0x7FFFFFFF
} wapi_wake_type_t;

/* ============================================================
 * Wake Lock Functions
 * ============================================================ */

/**
 * Acquire a wake lock.
 *
 * @param type  Wake lock type (screen or system).
 * @param lock  [out] Wake lock handle.
 * @return WAPI_OK on success, WAPI_ERR_ACCES if not permitted.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_wake, acquire)
wapi_result_t wapi_wake_acquire(wapi_wake_type_t type, wapi_handle_t* lock);

/**
 * Release a wake lock.
 *
 * @param lock  Wake lock handle obtained from wapi_wake_acquire.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_wake, release)
wapi_result_t wapi_wake_release(wapi_handle_t lock);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_WAKE_LOCK_H */
