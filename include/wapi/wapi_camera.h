/**
 * WAPI - Camera
 * Version 1.0.0
 *
 * Camera endpoints are acquired through the role system
 * (WAPI_ROLE_CAMERA). This header owns the desc (role prefs),
 * frame metadata, endpoint_info, and the frame-read surface.
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
    WAPI_CAMERA_FACING_ANY     = 0, /* prefs only */
    WAPI_CAMERA_FACING_FRONT   = 1,
    WAPI_CAMERA_FACING_BACK    = 2,
    WAPI_CAMERA_FACING_EXTERNAL = 3,
    WAPI_CAMERA_FACING_FORCE32 = 0x7FFFFFFF
} wapi_camera_facing_t;

typedef enum wapi_pixel_format_t {
    WAPI_PIXEL_RGBA8   = 0,
    WAPI_PIXEL_BGRA8   = 1,
    WAPI_PIXEL_NV12    = 2,  /* YUV 4:2:0 */
    WAPI_PIXEL_I420    = 3,  /* YUV planar */
    WAPI_PIXEL_FORCE32 = 0x7FFFFFFF
} wapi_pixel_format_t;

/**
 * Camera role-request prefs.
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: uint32_t facing   wapi_camera_facing_t (ANY = don't care)
 *   Offset  4: int32_t  width    0 = default
 *   Offset  8: int32_t  height   0 = default
 *   Offset 12: int32_t  fps      0 = default
 */
typedef struct wapi_camera_desc_t {
    uint32_t facing;
    int32_t  width;
    int32_t  height;
    int32_t  fps;
} wapi_camera_desc_t;

_Static_assert(sizeof(wapi_camera_desc_t) == 16, "wapi_camera_desc_t must be 16 bytes");
_Static_assert(_Alignof(wapi_camera_desc_t) == 4, "wapi_camera_desc_t must be 4-byte aligned");

/**
 * Per-frame metadata.
 *
 * Layout (24 bytes, align 8):
 *   Offset  0: int32_t  width
 *   Offset  4: int32_t  height
 *   Offset  8: uint32_t format      wapi_pixel_format_t
 *   Offset 12: int32_t  stride      bytes per row
 *   Offset 16: uint64_t timestamp   nanoseconds
 */
typedef struct wapi_camera_frame_t {
    int32_t     width;
    int32_t     height;
    uint32_t    format;
    int32_t     stride;
    uint64_t    timestamp;
} wapi_camera_frame_t;

/**
 * Metadata about a resolved camera endpoint.
 *
 * Layout (40 bytes, align 8):
 *   Offset  0: int32_t  width
 *   Offset  4: int32_t  height
 *   Offset  8: int32_t  fps
 *   Offset 12: uint32_t facing      wapi_camera_facing_t (never ANY)
 *   Offset 16: uint32_t native_format wapi_pixel_format_t
 *   Offset 20: uint32_t _pad
 *   Offset 24: uint8_t  uid[16]
 */
typedef struct wapi_camera_endpoint_info_t {
    int32_t     width;
    int32_t     height;
    int32_t     fps;
    uint32_t    facing;
    uint32_t    native_format;
    uint32_t    _pad;
    uint8_t     uid[16];
} wapi_camera_endpoint_info_t;

_Static_assert(sizeof(wapi_camera_endpoint_info_t) == 40, "wapi_camera_endpoint_info_t must be 40 bytes");
_Static_assert(_Alignof(wapi_camera_endpoint_info_t) == 4, "wapi_camera_endpoint_info_t must be 4-byte aligned");

/** Query metadata for a granted camera endpoint. */
WAPI_IMPORT(wapi_camera, endpoint_info)
wapi_result_t wapi_camera_endpoint_info(wapi_handle_t camera,
                                        wapi_camera_endpoint_info_t* out,
                                        char* name_buf, wapi_size_t name_buf_len,
                                        wapi_size_t* name_len);

/** Close a granted camera endpoint. */
WAPI_IMPORT(wapi_camera, close)
wapi_result_t wapi_camera_close(wapi_handle_t camera);

/** Read the latest cached frame. Returns WAPI_ERR_AGAIN if no frame yet. */
WAPI_IMPORT(wapi_camera, read_frame)
wapi_result_t wapi_camera_read_frame(wapi_handle_t camera, wapi_camera_frame_t* frame,
                                     void* buf, wapi_size_t buf_len, wapi_size_t* size);

/** Zero-copy GPU texture view of the latest frame. */
WAPI_IMPORT(wapi_camera, read_frame_gpu)
wapi_result_t wapi_camera_read_frame_gpu(wapi_handle_t camera,
                                         wapi_camera_frame_t* frame,
                                         wapi_handle_t* texture);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CAMERA_H */
