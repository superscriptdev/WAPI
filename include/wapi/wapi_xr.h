/**
 * WAPI - XR (Extended Reality)
 * Version 1.0.0
 *
 * Maps to: WebXR Device API, OpenXR, ARKit, ARCore
 *
 * Provides access to XR headsets, controllers, spatial tracking,
 * and immersive rendering. Covers the full spectrum of extended
 * reality: VR (immersive), AR (passthrough/overlay), and inline
 * (non-immersive) modes under a single wapi.xr capability.
 *
 * Import module: "wapi_xr"
 */

#ifndef WAPI_XR_H
#define WAPI_XR_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Session Types
 * ============================================================ */

typedef enum wapi_xr_session_type_t {
    WAPI_XR_IMMERSIVE_VR = 0,  /* Fully immersive VR */
    WAPI_XR_IMMERSIVE_AR = 1,  /* AR with passthrough */
    WAPI_XR_INLINE       = 2,  /* Non-immersive (phone AR) */
    WAPI_XR_FORCE32      = 0x7FFFFFFF
} wapi_xr_session_type_t;

typedef enum wapi_xr_ref_space_t {
    WAPI_XR_SPACE_LOCAL      = 0,  /* Seated / stationary */
    WAPI_XR_SPACE_LOCALFLOOR = 1, /* Standing */
    WAPI_XR_SPACE_BOUNDED    = 2,  /* Room-scale with boundary */
    WAPI_XR_SPACE_UNBOUNDED  = 3,  /* World-scale (AR) */
    WAPI_XR_SPACE_VIEWER     = 4,  /* Head-locked */
    WAPI_XR_SPACE_FORCE32    = 0x7FFFFFFF
} wapi_xr_ref_space_t;

typedef enum wapi_xr_hand_t {
    WAPI_XR_HAND_LEFT   = 0,
    WAPI_XR_HAND_RIGHT  = 1,
    WAPI_XR_HAND_NONE   = 2,
    WAPI_XR_HAND_FORCE32 = 0x7FFFFFFF
} wapi_xr_hand_t;

/* ============================================================
 * Spatial Types
 * ============================================================ */

/**
 * 3D pose (position + orientation).
 *
 * Layout (28 bytes, align 4):
 *   Offset  0: float position[3]      (x, y, z in meters)
 *   Offset 12: float orientation[4]   (quaternion: x, y, z, w)
 */
typedef struct wapi_xr_pose_t {
    float position[3];
    float orientation[4];
} wapi_xr_pose_t;

_Static_assert(offsetof(wapi_xr_pose_t, position)    ==  0, "");
_Static_assert(offsetof(wapi_xr_pose_t, orientation) == 12, "");
_Static_assert(sizeof(wapi_xr_pose_t)   == 28, "wapi_xr_pose_t must be 28 bytes");
_Static_assert(_Alignof(wapi_xr_pose_t) ==  4, "wapi_xr_pose_t must be 4-byte aligned");

/**
 * XR view (one per eye for stereo, one for mono).
 *
 * Layout (92 bytes, align 4):
 *   Offset  0: wapi_xr_pose_t pose       (28 bytes)
 *   Offset 28: float projection[16]      (4x4 column-major projection matrix)
 */
typedef struct wapi_xr_view_t {
    wapi_xr_pose_t pose;
    float          projection[16];
} wapi_xr_view_t;

_Static_assert(offsetof(wapi_xr_view_t, pose)       ==  0, "");
_Static_assert(offsetof(wapi_xr_view_t, projection) == 28, "");
_Static_assert(sizeof(wapi_xr_view_t)   == 92, "wapi_xr_view_t must be 92 bytes");
_Static_assert(_Alignof(wapi_xr_view_t) ==  4, "wapi_xr_view_t must be 4-byte aligned");

/**
 * XR frame state returned each frame.
 *
 * Layout (32 bytes, align 8):
 *   Offset  0: uint64_t predicted_display_time  (nanoseconds)
 *   Offset  8: uint32_t view_count
 *   Offset 12: uint32_t _pad0
 *   Offset 16: uint64_t views                   (linear-memory address of
 *                                                wapi_xr_view_t[view_count];
 *                                                points into caller buffer)
 *   Offset 24: uint32_t _reserved
 *   Offset 28: uint32_t _pad1
 */
typedef struct wapi_xr_frame_state_t {
    uint64_t predicted_display_time;
    uint32_t view_count;
    uint32_t _pad0;
    uint64_t views;
    uint32_t _reserved;
    uint32_t _pad1;
} wapi_xr_frame_state_t;

_Static_assert(offsetof(wapi_xr_frame_state_t, predicted_display_time) ==  0, "");
_Static_assert(offsetof(wapi_xr_frame_state_t, view_count)             ==  8, "");
_Static_assert(offsetof(wapi_xr_frame_state_t, _pad0)                  == 12, "");
_Static_assert(offsetof(wapi_xr_frame_state_t, views)                  == 16, "");
_Static_assert(offsetof(wapi_xr_frame_state_t, _reserved)              == 24, "");
_Static_assert(offsetof(wapi_xr_frame_state_t, _pad1)                  == 28, "");
_Static_assert(sizeof(wapi_xr_frame_state_t)   == 32, "wapi_xr_frame_state_t must be 32 bytes");
_Static_assert(_Alignof(wapi_xr_frame_state_t) ==  8, "wapi_xr_frame_state_t must be 8-byte aligned");

/* ============================================================
 * Session Management
 * ============================================================ */

/* ============================================================
 * XR Operations
 *
 * Session acquisition, ref-space creation and hit-test are async and
 * routed through the IO vtable. Begin/end/wait frame, and the
 * per-frame controller pose/state readers, remain bounded-local on
 * an already-opened session and stay as direct sync imports.
 * ============================================================ */

/** Probe whether a specific session type is supported on this device.
 *  Finer-grained than wapi_cap_supported("wapi.xr"): a device may expose
 *  the XR capability yet only support a subset of session types (e.g.
 *  INLINE but not IMMERSIVE_VR). Completion: result = 1/0. */
static inline wapi_result_t wapi_xr_is_supported(
    const wapi_io_t* io, wapi_xr_session_type_t type, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = 0x203; /* XR_IS_SUPPORTED — reserved method id in the XR range */
    op.flags     = (uint32_t)type;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Submit an XR session request. */
static inline wapi_result_t wapi_xr_request_session(
    const wapi_io_t* io, wapi_xr_session_type_t type,
    wapi_handle_t* out_session, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_XR_SESSION_REQUEST;
    op.flags      = (uint32_t)type;
    op.result_ptr = (uint64_t)(uintptr_t)out_session;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** End an XR session (bounded-local; closes the session). */
WAPI_IMPORT(wapi_xr, end_session)
wapi_result_t wapi_xr_end_session(wapi_handle_t session);

/** Submit a reference-space request. */
static inline wapi_result_t wapi_xr_create_ref_space(
    const wapi_io_t* io, wapi_handle_t session, wapi_xr_ref_space_t type,
    wapi_handle_t* out_space, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = 0x204; /* XR_REF_SPACE_CREATE — reserved in the XR range */
    op.fd         = session;
    op.flags      = (uint32_t)type;
    op.result_ptr = (uint64_t)(uintptr_t)out_space;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/* ============================================================
 * Per-frame state readers (bounded-local, direct sync imports)
 * ============================================================ */

WAPI_IMPORT(wapi_xr, wait_frame)
wapi_result_t wapi_xr_wait_frame(wapi_handle_t session, wapi_xr_frame_state_t* state,
                              wapi_xr_view_t* views, uint32_t max_views);

WAPI_IMPORT(wapi_xr, begin_frame)
wapi_result_t wapi_xr_begin_frame(wapi_handle_t session);

WAPI_IMPORT(wapi_xr, end_frame)
wapi_result_t wapi_xr_end_frame(wapi_handle_t session, const wapi_handle_t* textures,
                             uint32_t tex_count);

WAPI_IMPORT(wapi_xr, get_controller_pose)
wapi_result_t wapi_xr_get_controller_pose(wapi_handle_t session, wapi_handle_t space,
                                       wapi_xr_hand_t hand, wapi_xr_pose_t* pose);

WAPI_IMPORT(wapi_xr, get_controller_state)
wapi_result_t wapi_xr_get_controller_state(wapi_handle_t session, wapi_xr_hand_t hand,
                                        uint32_t* buttons, float* trigger,
                                        float* grip, float* thumbstick_x,
                                        float* thumbstick_y);

/* XR controller buttons */
#define WAPI_XR_BUTTON_TRIGGER  0x01
#define WAPI_XR_BUTTON_GRIP     0x02
#define WAPI_XR_BUTTON_MENU     0x04
#define WAPI_XR_BUTTON_A        0x08
#define WAPI_XR_BUTTON_B        0x10
#define WAPI_XR_BUTTON_THUMBSTICK 0x20

/** Submit a hit-test ray. Result pose inlines in completion payload. */
static inline wapi_result_t wapi_xr_hit_test(
    const wapi_io_t* io, wapi_handle_t session, wapi_handle_t space,
    const float origin[3], const float direction[3],
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_XR_HIT_TEST;
    op.fd        = session;
    op.flags     = (uint32_t)space;
    op.addr      = (uint64_t)(uintptr_t)origin;
    op.len       = 12;
    op.addr2     = (uint64_t)(uintptr_t)direction;
    op.len2      = 12;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_XR_H */
