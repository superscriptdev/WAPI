/**
 * WAPI - Screen Capture Capability
 * Version 1.0.0
 *
 * Capture screen, window, or tab contents.
 *
 * Maps to: Screen Capture API / getDisplayMedia (Web),
 *          CGDisplayStream (macOS), DXGI Desktop Duplication (Windows)
 *
 * Import module: "wapi_capture"
 *
 * Query availability with wapi_capability_supported("wapi.screencapture", 18)
 */

#ifndef WAPI_SCREENCAPTURE_H
#define WAPI_SCREENCAPTURE_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Screen Capture Types
 * ============================================================ */

typedef enum wapi_capture_source_t {
    WAPI_CAPTURE_SCREEN  = 0,
    WAPI_CAPTURE_WINDOW  = 1,
    WAPI_CAPTURE_TAB     = 2,
    WAPI_CAPTURE_FORCE32 = 0x7FFFFFFF
} wapi_capture_source_t;

/* ============================================================
 * Screen Capture Functions
 * ============================================================ */

/**
 * Request screen capture permission and begin capture.
 *
 * @param source_type  Type of capture source (screen, window, or tab).
 * @param capture      [out] Capture session handle.
 * @return WAPI_OK on success, WAPI_ERR_ACCES if not permitted.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_capture, request)
wapi_result_t wapi_capture_request(wapi_capture_source_t source_type,
                                   wapi_handle_t* capture);

/**
 * Get the next captured frame.
 *
 * @param capture         Capture session handle.
 * @param buf             Buffer to receive frame pixel data.
 * @param buf_len         Size of the buffer.
 * @param frame_info_ptr  [out] Pointer to receive frame metadata.
 * @return WAPI_OK on success, WAPI_ERR_AGAIN if no frame available.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_capture, get_frame)
wapi_result_t wapi_capture_get_frame(wapi_handle_t capture, void* buf,
                                     wapi_size_t buf_len,
                                     void* frame_info_ptr);

/**
 * Stop a screen capture session.
 *
 * @param capture  Capture session handle.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_capture, stop)
wapi_result_t wapi_capture_stop(wapi_handle_t capture);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SCREENCAPTURE_H */
