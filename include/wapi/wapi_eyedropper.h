/**
 * WAPI - Eyedropper Capability
 * Version 1.0.0
 *
 * Screen color picker.
 *
 * Maps to: EyeDropper API (Web), NSColorSampler (macOS),
 *          custom impl (Windows/Linux)
 *
 * Import module: "wapi_eyedrop"
 *
 * Query availability with wapi_capability_supported("wapi.eyedrop", 12)
 */

#ifndef WAPI_EYEDROPPER_H
#define WAPI_EYEDROPPER_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Eyedropper Functions
 * ============================================================ */

/**
 * Show the system color picker and return the selected color.
 *
 * Blocks until the user picks a color or cancels. The color is
 * written as a uint32_t in RGBA format (0xRRGGBBAA).
 *
 * @param rgba_ptr  [out] Pointer to receive the RGBA color value.
 * @return WAPI_OK on success, WAPI_ERR_CANCEL if user cancelled,
 *         WAPI_ERR_NOTSUP if not supported.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_eyedrop, eyedropper_pick)
wapi_result_t wapi_eyedropper_pick(uint32_t* rgba_ptr);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_EYEDROPPER_H */
