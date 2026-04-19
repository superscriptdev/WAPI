/**
 * WAPI - Barcode Detection
 * Version 1.0.0
 *
 * Detect and decode barcodes from image data or camera feeds.
 *
 * Maps to: Barcode Detection API (Web), AVFoundation (iOS),
 *          ML Kit Barcode Scanning (Android), ZXing (Desktop)
 *
 * Import module: "wapi_barcode"
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
    WAPI_BARCODE_EAN13      = 1,
    WAPI_BARCODE_EAN8       = 2,
    WAPI_BARCODE_CODE128    = 3,
    WAPI_BARCODE_CODE39     = 4,
    WAPI_BARCODE_UPCA       = 5,
    WAPI_BARCODE_UPCE       = 6,
    WAPI_BARCODE_DATAMATRIX = 7,
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
_Static_assert(_Alignof(wapi_barcode_result_t) == 8,
               "wapi_barcode_result_t must be 8-byte aligned");

/* ============================================================
 * Barcode Operations (async, submitted via wapi_io_t)
 * ============================================================
 * Detected strings are allocated by the host in the module's linear
 * memory (via the module's allocator vtable); each wapi_barcode_result_t
 * entry's value_ptr points to a UTF-8 buffer the caller frees after
 * use.
 */

/**
 * Submit an image for barcode detection. Completion carries
 * result = number of barcodes written to results_buf, or a
 * negative error code.
 */
static inline wapi_result_t wapi_barcode_detect_image(
    const wapi_io_t* io,
    const void* image_data, uint32_t width, uint32_t height,
    wapi_barcode_result_t* results_buf, uint32_t max_results,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_BARCODE_DETECT_IMAGE;
    op.addr      = (uint64_t)(uintptr_t)image_data;
    op.len       = (uint64_t)width * height * 4;
    op.flags     = width;
    op.flags2    = height;
    op.addr2     = (uint64_t)(uintptr_t)results_buf;
    op.len2      = (uint64_t)max_results * sizeof(wapi_barcode_result_t);
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/**
 * Submit a camera-feed frame for barcode detection.
 */
static inline wapi_result_t wapi_barcode_detect_from_camera(
    const wapi_io_t* io,
    wapi_handle_t camera_handle,
    wapi_barcode_result_t* results_buf, uint32_t max_results,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_BARCODE_DETECT_CAMERA;
    op.fd        = camera_handle;
    op.addr      = (uint64_t)(uintptr_t)results_buf;
    op.len       = (uint64_t)max_results * sizeof(wapi_barcode_result_t);
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_BARCODE_H */
