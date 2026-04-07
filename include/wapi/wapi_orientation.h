/**
 * WAPI - Screen Orientation Capability
 * Version 1.0.0
 *
 * Query and lock screen orientation.
 *
 * Maps to: Screen Orientation API (Web), UIInterfaceOrientation (iOS),
 *          ActivityInfo.screenOrientation (Android)
 *
 * Import module: "wapi_orient"
 *
 * Query availability with wapi_capability_supported("wapi.orientation", 15)
 */

#ifndef WAPI_ORIENTATION_H
#define WAPI_ORIENTATION_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Event Types
 * ============================================================ */

#define WAPI_EVENT_ORIENTATION_CHANGED 0x020B

/* ============================================================
 * Orientation Types
 * ============================================================ */

typedef enum wapi_orientation_t {
    WAPI_ORIENT_PORTRAIT_PRIMARY    = 0,
    WAPI_ORIENT_PORTRAIT_SECONDARY  = 1,  /* Upside down */
    WAPI_ORIENT_LANDSCAPE_PRIMARY   = 2,
    WAPI_ORIENT_LANDSCAPE_SECONDARY = 3,
    WAPI_ORIENT_FORCE32             = 0x7FFFFFFF
} wapi_orientation_t;

/* ============================================================
 * Orientation Lock Types
 * ============================================================ */

typedef enum wapi_orientation_lock_t {
    WAPI_ORIENT_LOCK_ANY        = 0,
    WAPI_ORIENT_LOCK_PORTRAIT   = 1,
    WAPI_ORIENT_LOCK_LANDSCAPE  = 2,
    WAPI_ORIENT_LOCK_NATURAL    = 3,
    WAPI_ORIENT_LOCK_FORCE32    = 0x7FFFFFFF
} wapi_orientation_lock_t;

/* ============================================================
 * Orientation Functions
 * ============================================================ */

/**
 * Get the current screen orientation.
 *
 * @param orientation  [out] Current orientation.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_orient, get)
wapi_result_t wapi_orient_get(wapi_orientation_t* orientation);

/**
 * Lock the screen to a specific orientation.
 *
 * @param lock_type  Orientation lock mode.
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if not supported.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_orient, lock)
wapi_result_t wapi_orient_lock(wapi_orientation_lock_t lock_type);

/**
 * Unlock the screen orientation (allow free rotation).
 *
 * @return WAPI_OK on success.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_orient, unlock)
wapi_result_t wapi_orient_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_ORIENTATION_H */
