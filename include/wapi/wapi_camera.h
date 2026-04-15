/**
 * WAPI - Camera Capability
 * Version 1.0.0
 *
 * Maps to: MediaDevices.getUserMedia (Web), AVCaptureSession (iOS),
 *          Camera2 API (Android)
 *
 * Provides access to camera video frames as GPU textures or
 * raw pixel data.
 *
 * Import module: "wapi_camera"
 *
 * Query availability with wapi_capability_supported("wapi.camera", 9)
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

/**
 * Get the number of available cameras.
 */
WAPI_IMPORT(wapi_camera, count)
int32_t wapi_camera_count(void);

/**
 * Open a camera (shows permission prompt if needed).
 *
 * @param desc    Camera descriptor.
 * @param camera  [out] Camera handle.
 */
WAPI_IMPORT(wapi_camera, open)
wapi_result_t wapi_camera_open(const wapi_camera_desc_t* desc,
                            wapi_handle_t* camera);

/**
 * Close a camera.
 */
WAPI_IMPORT(wapi_camera, close)
wapi_result_t wapi_camera_close(wapi_handle_t camera);

/**
 * Grab the latest frame as raw pixel data.
 *
 * @param camera  Camera handle.
 * @param frame   [out] Frame metadata.
 * @param buf     Buffer for pixel data.
 * @param buf_len Buffer capacity.
 * @param size    [out] Actual data size.
 * @return WAPI_OK, WAPI_ERR_AGAIN if no new frame.
 */
WAPI_IMPORT(wapi_camera, read_frame)
wapi_result_t wapi_camera_read_frame(wapi_handle_t camera, wapi_camera_frame_t* frame,
                                  void* buf, wapi_size_t buf_len, wapi_size_t* size);

/**
 * Get the latest frame as a GPU texture (zero-copy path).
 *
 * @param camera   Camera handle.
 * @param frame    [out] Frame metadata.
 * @param texture  [out] GPU texture handle (valid until next call).
 * @return WAPI_OK, WAPI_ERR_AGAIN if no new frame.
 */
WAPI_IMPORT(wapi_camera, read_frame_gpu)
wapi_result_t wapi_camera_read_frame_gpu(wapi_handle_t camera,
                                      wapi_camera_frame_t* frame,
                                      wapi_handle_t* texture);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CAMERA_H */
