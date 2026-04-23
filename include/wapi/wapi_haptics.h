/**
 * WAPI - Haptics
 * Version 1.0.0
 *
 * Haptic endpoints are acquired through the role system
 * (WAPI_ROLE_HAPTIC). Gamepad rumble is addressed via a HAPTIC role
 * on the gamepad's UID. Built-in phone/browser vibration is a HAPTIC
 * role with FOLLOW_DEFAULT and no target_uid.
 *
 * Import module: "wapi_haptic"
 */

#ifndef WAPI_HAPTICS_H
#define WAPI_HAPTICS_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Effect Types
 * ============================================================ */

typedef enum wapi_haptic_effect_t {
    WAPI_HAPTIC_CLICK        = 0,
    WAPI_HAPTIC_DOUBLE_CLICK = 1,
    WAPI_HAPTIC_TICK         = 2,
    WAPI_HAPTIC_HEAVY        = 3,
    WAPI_HAPTIC_MEDIUM       = 4,
    WAPI_HAPTIC_LIGHT        = 5,
    WAPI_HAPTIC_CUSTOM       = 6,
    WAPI_HAPTIC_FORCE32      = 0x7FFFFFFF
} wapi_haptic_effect_t;

/* ============================================================
 * Feature Flags
 * ============================================================ */

#define WAPI_HAPTIC_FEAT_CONSTANT  0x0001
#define WAPI_HAPTIC_FEAT_SINE      0x0002
#define WAPI_HAPTIC_FEAT_TRIANGLE  0x0004
#define WAPI_HAPTIC_FEAT_SAWTOOTH  0x0008
#define WAPI_HAPTIC_FEAT_RAMP      0x0010
#define WAPI_HAPTIC_FEAT_RUMBLE    0x0020
#define WAPI_HAPTIC_FEAT_CUSTOM    0x0040
#define WAPI_HAPTIC_FEAT_PATTERN   0x0080 /* accepts duration patterns */

/* ============================================================
 * Endpoint Metadata
 * ============================================================ */

/**
 * Metadata about a resolved haptic endpoint.
 *
 * Layout (24 bytes, align 8):
 *   Offset  0: uint32_t features   bitmask of WAPI_HAPTIC_FEAT_*
 *   Offset  4: uint32_t _pad
 *   Offset  8: uint8_t  uid[16]
 */
typedef struct wapi_haptic_endpoint_info_t {
    uint32_t features;
    uint32_t _pad;
    uint8_t  uid[16];
} wapi_haptic_endpoint_info_t;

_Static_assert(sizeof(wapi_haptic_endpoint_info_t) == 24, "wapi_haptic_endpoint_info_t must be 24 bytes");
_Static_assert(_Alignof(wapi_haptic_endpoint_info_t) == 4, "wapi_haptic_endpoint_info_t must be 4-byte aligned");

WAPI_IMPORT(wapi_haptic, endpoint_info)
wapi_result_t wapi_haptic_endpoint_info(wapi_handle_t handle,
                                        wapi_haptic_endpoint_info_t* out,
                                        char* name_buf, wapi_size_t name_buf_len,
                                        wapi_size_t* name_len);

/** Close a granted haptic endpoint. */
WAPI_IMPORT(wapi_haptic, close)
wapi_result_t wapi_haptic_close(wapi_handle_t handle);

/* ============================================================
 * Playback
 * ============================================================ */

/** Vibrate with a pattern (ms durations alternating vibrate/pause). */
WAPI_IMPORT(wapi_haptic, vibrate)
wapi_result_t wapi_haptic_vibrate(wapi_handle_t handle,
                                  const uint32_t* pattern_ptr,
                                  wapi_size_t pattern_len);

/** Cancel any ongoing playback on this endpoint. */
WAPI_IMPORT(wapi_haptic, cancel)
wapi_result_t wapi_haptic_cancel(wapi_handle_t handle);

/** Simple rumble. */
WAPI_IMPORT(wapi_haptic, rumble)
wapi_result_t wapi_haptic_rumble(wapi_handle_t handle,
                                 const float* strength,
                                 uint32_t duration_ms);

/** Create a parametric effect (constant, periodic, ramp). */
WAPI_IMPORT(wapi_haptic, effect_create)
wapi_result_t wapi_haptic_effect_create(wapi_handle_t handle,
                                        const void* desc_ptr,
                                        wapi_size_t desc_len,
                                        wapi_handle_t* out_effect);

WAPI_IMPORT(wapi_haptic, effect_play)
wapi_result_t wapi_haptic_effect_play(wapi_handle_t effect, uint32_t iterations);

WAPI_IMPORT(wapi_haptic, effect_stop)
wapi_result_t wapi_haptic_effect_stop(wapi_handle_t effect);

WAPI_IMPORT(wapi_haptic, effect_destroy)
wapi_result_t wapi_haptic_effect_destroy(wapi_handle_t effect);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_HAPTICS_H */
