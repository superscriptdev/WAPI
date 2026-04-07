/**
 * WAPI - Haptics Capability
 * Version 1.0.0
 *
 * Device vibration patterns and advanced haptic effects.
 * Gamepad rumble is in wapi_input.h (Gamepad section).
 *
 * Maps to: Vibration API (Web),
 *          UIImpactFeedbackGenerator / CoreHaptics (iOS),
 *          Vibrator / VibratorManager (Android),
 *          SDL_Haptic (Desktop)
 *
 * Import module: "wapi_haptic"
 *
 * Query availability with wapi_capability_supported("wapi.haptics", 12)
 */

#ifndef WAPI_HAPTICS_H
#define WAPI_HAPTICS_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Haptic Effect Types
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
 * Haptic Feature Flags (returned by haptic_get_features)
 * ============================================================ */

#define WAPI_HAPTIC_FEAT_CONSTANT  0x0001
#define WAPI_HAPTIC_FEAT_SINE      0x0002
#define WAPI_HAPTIC_FEAT_TRIANGLE  0x0004
#define WAPI_HAPTIC_FEAT_SAWTOOTH  0x0008
#define WAPI_HAPTIC_FEAT_RAMP      0x0010
#define WAPI_HAPTIC_FEAT_RUMBLE    0x0020
#define WAPI_HAPTIC_FEAT_CUSTOM    0x0040

/* ============================================================
 * Simple Vibration (phone / browser)
 * ============================================================ */

/**
 * Vibrate with a pattern.
 *
 * The pattern is an array of uint32_t durations in milliseconds,
 * alternating between vibrate and pause intervals (starting with
 * vibrate). For a single vibration, pass a one-element array.
 *
 * @param pattern_ptr  Pointer to array of uint32_t durations (ms).
 * @param pattern_len  Number of elements in the pattern array.
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if not supported.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_haptic, vibrate)
wapi_result_t wapi_haptic_vibrate(const uint32_t* pattern_ptr,
                                  wapi_size_t pattern_len);

/**
 * Cancel any ongoing vibration.
 *
 * @return WAPI_OK on success.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_haptic, vibrate_cancel)
wapi_result_t wapi_haptic_vibrate_cancel(void);

/* ============================================================
 * Device-Level Haptics (SDL_Haptic backed)
 * ============================================================ */

/**
 * Open a haptic device by index.
 *
 * @param device_index  Device index (0-based).
 * @param out_handle    [out] Haptic device handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_haptic, haptic_open)
wapi_result_t wapi_haptic_open(uint32_t device_index,
                               wapi_handle_t* out_handle);

/**
 * Close a haptic device.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_haptic, haptic_close)
wapi_result_t wapi_haptic_close(wapi_handle_t handle);

/**
 * Simple rumble effect on a haptic device.
 *
 * @param handle       Haptic device handle.
 * @param strength     Strength [0.0, 1.0] (pointer to float).
 * @param duration_ms  Duration in milliseconds.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_haptic, haptic_rumble)
wapi_result_t wapi_haptic_rumble(wapi_handle_t handle,
                                 const float* strength,
                                 uint32_t duration_ms);

/**
 * Query supported haptic features for a device.
 *
 * @param handle  Haptic device handle.
 * @return Bitmask of WAPI_HAPTIC_FEAT_* flags, or 0 on error.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_haptic, haptic_get_features)
uint32_t wapi_haptic_get_features(wapi_handle_t handle);

/**
 * Create an advanced haptic effect (constant, periodic, ramp).
 *
 * @param handle       Haptic device handle.
 * @param desc_ptr     Pointer to effect descriptor.
 * @param desc_len     Size of descriptor in bytes.
 * @param out_effect   [out] Effect handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_haptic, haptic_effect_create)
wapi_result_t wapi_haptic_effect_create(wapi_handle_t handle,
                                        const void* desc_ptr,
                                        wapi_size_t desc_len,
                                        wapi_handle_t* out_effect);

/**
 * Play a previously created haptic effect.
 *
 * @param effect     Effect handle.
 * @param iterations Number of iterations (0 = infinite).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_haptic, haptic_effect_play)
wapi_result_t wapi_haptic_effect_play(wapi_handle_t effect,
                                      uint32_t iterations);

/**
 * Stop a running haptic effect.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_haptic, haptic_effect_stop)
wapi_result_t wapi_haptic_effect_stop(wapi_handle_t effect);

/**
 * Destroy a haptic effect.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_haptic, haptic_effect_destroy)
wapi_result_t wapi_haptic_effect_destroy(wapi_handle_t effect);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_HAPTICS_H */
