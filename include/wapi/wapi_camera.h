/**
 * WAPI - Camera
 * Version 1.0.0
 *
 * Maps to: MediaDevices.getUserMedia (Web), AVCaptureSession (iOS),
 *          Camera2 API (Android)
 *
 * Provides access to camera video frames as GPU textures or
 * raw pixel data.
 *
 * Import module: "wapi_camera"
 */

#ifndef WAPI_CAMERA_H
#define WAPI_CAMERA_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum wapi_camera_facing_t {
    WAPI_CAMERA_FACING_FRONT   = 0,
    WAPI_CAMERA_FACING_BACK    = 1,
    WAPI_CAMERA_FACING_ANY     = 2,
    WAPI_CAMERA_FACING_FORCE32 = 0x7FFFFFFF
} wapi_camera_facing_t;

typedef enum wapi_pixel_format_t {
    WAPI_PIXEL_RGBA8   = 0,
    WAPI_PIXEL_BGRA8   = 1,
    WAPI_PIXEL_NV12    = 2,  /* YUV 4:2:0, common camera format */
    WAPI_PIXEL_I420    = 3,  /* YUV planar */
    WAPI_PIXEL_FORCE32 = 0x7FFFFFFF
} wapi_pixel_format_t;

/**
 * Camera configuration.
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: uint32_t facing
 *   Offset  4: int32_t  width     Requested width (0 = default)
 *   Offset  8: int32_t  height    Requested height (0 = default)
 *   Offset 12: int32_t  fps       Requested frame rate (0 = default)
 */
typedef struct wapi_camera_desc_t {
    uint32_t facing;
    int32_t  width;
    int32_t  height;
    int32_t  fps;
} wapi_camera_desc_t;

/**
 * Camera frame metadata.
 *
 * Layout (24 bytes, align 8):
 *   Offset  0: int32_t  width
 *   Offset  4: int32_t  height
 *   Offset  8: uint32_t format      (wapi_pixel_format_t)
 *   Offset 12: int32_t  stride      (bytes per row)
 *   Offset 16: uint64_t timestamp   (nanoseconds)
 */
typedef struct wapi_camera_frame_t {
    int32_t     width;
    int32_t     height;
    uint32_t    format;
    int32_t     stride;
    uint64_t    timestamp;
} wapi_camera_frame_t;

/** Bounded-local: number of cameras the host has advertised. */
WAPI_IMPORT(wapi_camera, count)
int32_t wapi_camera_count(void);

/** Bounded-local: close a previously opened camera handle. */
WAPI_IMPORT(wapi_camera, close)
wapi_result_t wapi_camera_close(wapi_handle_t camera);

/** Bounded-local: read the latest cached frame. Returns WAPI_ERR_AGAIN
 *  if no frame has been delivered yet. */
WAPI_IMPORT(wapi_camera, read_frame)
wapi_result_t wapi_camera_read_frame(wapi_handle_t camera, wapi_camera_frame_t* frame,
                                  void* buf, wapi_size_t buf_len, wapi_size_t* size);

/** Bounded-local: zero-copy GPU texture view of the latest frame. */
WAPI_IMPORT(wapi_camera, read_frame_gpu)
wapi_result_t wapi_camera_read_frame_gpu(wapi_handle_t camera,
                                      wapi_camera_frame_t* frame,
                                      wapi_handle_t* texture);

/* ============================================================
 * Camera Operations (async, submitted via wapi_io_t)
 * ============================================================ */

/** Submit a camera open. May prompt for permission. */
static inline wapi_result_t wapi_camera_open(
    const wapi_io_t* io, const wapi_camera_desc_t* desc,
    wapi_handle_t* out_camera, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_CAMERA_OPEN;
    op.addr       = (uint64_t)(uintptr_t)desc;
    op.len        = sizeof(*desc);
    op.result_ptr = (uint64_t)(uintptr_t)out_camera;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CAMERA_H */
