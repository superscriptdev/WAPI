/**
 * WAPI - Permissions Capability
 * Version 1.0.0
 *
 * Query and request permissions for capabilities at runtime.
 *
 * Maps to: Web Permissions API, NSPrivacy* (macOS/iOS),
 *          Android Runtime Permissions, Windows App Capabilities
 *
 * Import module: "wapi_perm"
 *
 * Query availability with wapi_capability_supported("wapi.permissions", 14)
 */

#ifndef WAPI_PERMISSIONS_H
#define WAPI_PERMISSIONS_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Permission State
 * ============================================================ */

typedef enum wapi_permission_state_t {
    WAPI_PERMISSION_PROMPT   = 0,  /* Not yet decided */
    WAPI_PERMISSION_GRANTED  = 1,
    WAPI_PERMISSION_DENIED   = 2,
    WAPI_PERMISSION_FORCE32  = 0x7FFFFFFF
} wapi_permission_state_t;

/* ============================================================
 * Permission Functions
 * ============================================================ */

/**
 * Query permission status for a capability.
 *
 * @param capability  Capability name (e.g., "wapi.camera", "wapi.geolocation").
 * @param state       [out] Current permission state.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_perm, query)
wapi_result_t wapi_perm_query(wapi_string_view_t capability,
                           wapi_permission_state_t* state);

/**
 * Request permission for a capability (may show OS dialog).
 *
 * @see WAPI_IO_OP_PERM_REQUEST
 *
 * @param capability  Capability name (e.g., "wapi.camera", "wapi.geolocation").
 * @param state       [out] Resulting permission state after user decision.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_perm, request)
wapi_result_t wapi_perm_request(wapi_string_view_t capability,
                             wapi_permission_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_PERMISSIONS_H */
