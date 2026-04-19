/**
 * WAPI - Screen Capture
 * Version 1.0.0
 *
 * Capture screen, window, or tab contents.
 *
 * Maps to: Screen Capture API / getDisplayMedia (Web),
 *          CGDisplayStream (macOS), DXGI Desktop Duplication (Windows)
 *
 * Import module: "wapi_capture"
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
 * Screen Capture Operations (request is async, frames are polled)
 * ============================================================ */

/** Submit a screen-capture request. Shows the system picker. */
static inline wapi_result_t wapi_capture_request(
    const wapi_io_t* io, wapi_capture_source_t source_type,
    wapi_handle_t* out_capture, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_CAPTURE_REQUEST;
    op.flags      = (uint32_t)source_type;
    op.result_ptr = (uint64_t)(uintptr_t)out_capture;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Bounded-local: read the latest cached frame. */
WAPI_IMPORT(wapi_capture, get_frame)
wapi_result_t wapi_capture_get_frame(wapi_handle_t capture, void* buf,
                                     wapi_size_t buf_len,
                                     void* frame_info_ptr);

/** Bounded-local: stop the session (releases the media track). */
WAPI_IMPORT(wapi_capture, stop)
wapi_result_t wapi_capture_stop(wapi_handle_t capture);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SCREENCAPTURE_H */
