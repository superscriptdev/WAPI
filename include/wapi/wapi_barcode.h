/**
 * WAPI - Barcode Detection Capability
 * Version 1.0.0
 *
 * Detect and decode barcodes from image data or camera feeds.
 *
 * Maps to: Barcode Detection API (Web), AVFoundation (iOS),
 *          ML Kit Barcode Scanning (Android), ZXing (Desktop)
 *
 * Import module: "wapi_barcode"
 *
 * Query availability with wapi_capability_supported("wapi.barcode", 11)
 */

#ifndef WAPI_BARCODE_H
#define WAPI_BARCODE_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Barcode Types
 * ============================================================ */

typedef enum wapi_barcode_format_t {
    WAPI_BARCODE_QR          = 0,
    WAPI_BARCODE_EAN_13      = 1,
    WAPI_BARCODE_EAN_8       = 2,
    WAPI_BARCODE_CODE_128    = 3,
    WAPI_BARCODE_CODE_39     = 4,
    WAPI_BARCODE_UPC_A       = 5,
    WAPI_BARCODE_UPC_E       = 6,
    WAPI_BARCODE_DATA_MATRIX = 7,
    WAPI_BARCODE_PDF417      = 8,
    WAPI_BARCODE_AZTEC       = 9,
    WAPI_BARCODE_FORCE32     = 0x7FFFFFFF
} wapi_barcode_format_t;

/**
 * Barcode detection result.
 *
 * Layout (32 bytes, align 8):
 *   Offset  0: uint32_t format     (wapi_barcode_format_t)
 *   Offset  4: uint32_t value_len
 *   Offset  8: uint64_t value_ptr  Linear memory address of decoded string
 *   Offset 16: float    x          (bounding box origin x)
 *   Offset 20: float    y          (bounding box origin y)
 *   Offset 24: float    w          (bounding box width)
 *   Offset 28: float    h          (bounding box height)
 */
typedef struct wapi_barcode_result_t {
    uint32_t format;      /* wapi_barcode_format_t */
    uint32_t value_len;
    uint64_t value_ptr;   /* Linear memory address of decoded string */
    float    x;
    float    y;
    float    w;
    float    h;
} wapi_barcode_result_t;

_Static_assert(sizeof(wapi_barcode_result_t) == 32,
               "wapi_barcode_result_t must be 32 bytes");

/* ============================================================
 * Barcode Functions
 * ============================================================ */

/**
 * Detect barcodes in image data.
 *
 * @param image_data   Pointer to RGBA pixel data.
 * @param width        Image width in pixels.
 * @param height       Image height in pixels.
 * @param results_buf  Buffer to receive barcode results.
 * @param max_results  Maximum number of results to return.
 * @return Number of barcodes detected on success, or negative error code.
 *
 * Wasm signature: (i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_barcode, detect)
wapi_result_t wapi_barcode_detect(const void* image_data, uint32_t width,
                                  uint32_t height,
                                  wapi_barcode_result_t* results_buf,
                                  uint32_t max_results);

/**
 * Detect barcodes from a camera handle.
 *
 * @param camera_handle  Camera handle obtained from wapi_camera.
 * @param results_buf    Buffer to receive barcode results.
 * @param max_results    Maximum number of results to return.
 * @return Number of barcodes detected on success, or negative error code.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_barcode, detect_from_camera)
wapi_result_t wapi_barcode_detect_from_camera(wapi_handle_t camera_handle,
                                              wapi_barcode_result_t* results_buf,
                                              uint32_t max_results);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_BARCODE_H */
