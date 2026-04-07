/**
 * WAPI - XR (Extended Reality) Capability
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
 *
 * Query availability with wapi_capability_supported("wapi.xr", 5)
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
    WAPI_XR_SPACE_LOCAL_FLOOR = 1, /* Standing */
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
 *   Offset  0: float position[3]   (x, y, z in meters)
 *   Offset 12: float orientation[4] (quaternion: x, y, z, w)
 */
typedef struct wapi_xr_pose_t {
    float position[3];
    float orientation[4];
} wapi_xr_pose_t;

/**
 * XR view (one per eye for stereo, one for mono).
 *
 * Layout (92 bytes, align 4):
 *   Offset  0: wapi_xr_pose_t pose          (28 bytes)
 *   Offset 28: float projection[16]       (4x4 projection matrix)
 *   Offset 92: end
 */
typedef struct wapi_xr_view_t {
    wapi_xr_pose_t pose;
    float        projection[16];
} wapi_xr_view_t;

/**
 * XR frame state returned each frame.
 *
 * Layout (24 bytes, align 8):
 *   Offset  0: uint64_t predicted_display_time (nanoseconds)
 *   Offset  8: uint32_t view_count
 *   Offset 12: uint32_t _pad
 *   Offset 16: ptr      views (array of wapi_xr_view_t)
 *   Offset 20: uint32_t _reserved
 */
typedef struct wapi_xr_frame_state_t {
    uint64_t      predicted_display_time;
    uint32_t      view_count;
    uint32_t      _pad;
    wapi_xr_view_t* views;  /* Points into caller-provided buffer */
    uint32_t      _reserved;
} wapi_xr_frame_state_t;

/* ============================================================
 * Session Management
 * ============================================================ */

/**
 * Check if an XR session type is supported.
 */
WAPI_IMPORT(wapi_xr, is_supported)
wapi_bool_t wapi_xr_is_supported(wapi_xr_session_type_t type);

/**
 * Request and start an XR session.
 *
 * @see WAPI_IO_OP_XR_REQUEST_SESSION
 *
 * @param type     Session type.
 * @param session  [out] Session handle.
 */
WAPI_IMPORT(wapi_xr, request_session)
wapi_result_t wapi_xr_request_session(wapi_xr_session_type_t type,
                                   wapi_handle_t* session);

/**
 * End an XR session.
 */
WAPI_IMPORT(wapi_xr, end_session)
wapi_result_t wapi_xr_end_session(wapi_handle_t session);

/**
 * Create a reference space for spatial tracking.
 *
 * @param session  Session handle.
 * @param type     Reference space type.
 * @param space    [out] Space handle.
 */
WAPI_IMPORT(wapi_xr, create_ref_space)
wapi_result_t wapi_xr_create_ref_space(wapi_handle_t session, wapi_xr_ref_space_t type,
                                    wapi_handle_t* space);

/* ============================================================
 * Frame Loop
 * ============================================================ */

/**
 * Wait for the next XR frame. Blocks until the runtime is ready.
 *
 * @see WAPI_IO_OP_XR_WAIT_FRAME
 *
 * @param session  Session handle.
 * @param state    [out] Frame state with views and timing.
 * @param views    Buffer for view data (2 views for stereo).
 * @param max_views Buffer capacity in number of views.
 * @return WAPI_OK on success.
 */
WAPI_IMPORT(wapi_xr, wait_frame)
wapi_result_t wapi_xr_wait_frame(wapi_handle_t session, wapi_xr_frame_state_t* state,
                              wapi_xr_view_t* views, uint32_t max_views);

/**
 * Begin an XR frame (call after wait_frame).
 */
WAPI_IMPORT(wapi_xr, begin_frame)
wapi_result_t wapi_xr_begin_frame(wapi_handle_t session);

/**
 * End an XR frame and submit rendered layers.
 *
 * @param session     Session handle.
 * @param textures    Array of GPU texture handles (one per view).
 * @param tex_count   Number of textures.
 */
WAPI_IMPORT(wapi_xr, end_frame)
wapi_result_t wapi_xr_end_frame(wapi_handle_t session, const wapi_handle_t* textures,
                             uint32_t tex_count);

/* ============================================================
 * Input / Controllers
 * ============================================================ */

/**
 * Get the pose of a controller/hand.
 *
 * @param session  Session handle.
 * @param space    Reference space handle.
 * @param hand     Which hand/controller.
 * @param pose     [out] Current pose.
 */
WAPI_IMPORT(wapi_xr, get_controller_pose)
wapi_result_t wapi_xr_get_controller_pose(wapi_handle_t session, wapi_handle_t space,
                                       wapi_xr_hand_t hand, wapi_xr_pose_t* pose);

/**
 * Get controller button/trigger state.
 *
 * @param session    Session handle.
 * @param hand       Which hand.
 * @param buttons    [out] Button bitmask.
 * @param trigger    [out] Trigger value 0.0-1.0.
 * @param grip       [out] Grip value 0.0-1.0.
 * @param thumbstick_x [out] Thumbstick X axis -1.0 to 1.0.
 * @param thumbstick_y [out] Thumbstick Y axis -1.0 to 1.0.
 */
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

/* ============================================================
 * AR Features
 * ============================================================ */

/**
 * Perform a hit test (raycast into the real world).
 *
 * @param session  Session handle.
 * @param space    Reference space.
 * @param origin   Ray origin (3 floats).
 * @param direction Ray direction (3 floats).
 * @param pose     [out] Hit pose (where the ray hit a surface).
 * @return WAPI_OK if hit, WAPI_ERR_NOENT if no hit.
 */
WAPI_IMPORT(wapi_xr, hit_test)
wapi_result_t wapi_xr_hit_test(wapi_handle_t session, wapi_handle_t space,
                            const float origin[3], const float direction[3],
                            wapi_xr_pose_t* pose);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_XR_H */
