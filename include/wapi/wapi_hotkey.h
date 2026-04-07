/**
 * WAPI - Global Hotkey Capability
 * Version 1.0.0
 *
 * Global keyboard shortcuts that work even when the window is not focused.
 *
 * Maps to: RegisterHotKey (Windows), CGEventTap (macOS),
 *          XGrabKey (Linux/X11)
 *
 * Import module: "wapi_hotkey"
 *
 * Query availability with wapi_capability_supported("wapi.hotkey", 10)
 */

#ifndef WAPI_HOTKEY_H
#define WAPI_HOTKEY_H

#include "wapi_types.h"
#include "wapi_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Hotkey Functions
 * ============================================================ */

/**
 * Register a global hotkey.
 *
 * mod_flags uses the WAPI_KMOD_* flags from wapi_event.h.
 * When the hotkey is triggered, a hotkey event is emitted with the
 * returned id.
 *
 * @param mod_flags  Modifier key bitmask (WAPI_KMOD_* flags).
 * @param scancode   Physical key scancode.
 * @param out_id     [out] Receives the hotkey registration id.
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if not supported,
 *         WAPI_ERR_BUSY if the hotkey is already registered.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_hotkey, hotkey_register)
wapi_result_t wapi_hotkey_register(uint32_t mod_flags,
                                   uint32_t scancode,
                                   uint32_t* out_id);

/**
 * Unregister a previously registered global hotkey.
 *
 * @param id  Hotkey registration id returned by hotkey_register.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid id.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_hotkey, hotkey_unregister)
wapi_result_t wapi_hotkey_unregister(uint32_t id);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_HOTKEY_H */
