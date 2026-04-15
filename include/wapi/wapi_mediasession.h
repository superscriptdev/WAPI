/**
 * WAPI - Media Session
 * Version 1.0.0
 *
 * Expose now-playing metadata to OS media controls.
 * Maps to: Media Session API (Web), MPNowPlayingInfoCenter (iOS/macOS),
 *          MediaSession (Android), SMTC (Windows)
 *
 * Import module: "wapi_media"
 */

#ifndef WAPI_MEDIASESSION_H
#define WAPI_MEDIASESSION_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Media Actions
 * ============================================================ */

typedef enum wapi_media_action_t {
    WAPI_MEDIA_ACTION_PLAY           = 0,
    WAPI_MEDIA_ACTION_PAUSE          = 1,
    WAPI_MEDIA_ACTION_STOP           = 2,
    WAPI_MEDIA_ACTION_SEEK_BACKWARD  = 3,
    WAPI_MEDIA_ACTION_SEEK_FORWARD   = 4,
    WAPI_MEDIA_ACTION_PREVIOUSTRACK = 5,
    WAPI_MEDIA_ACTION_NEXTTRACK     = 6,
    WAPI_MEDIA_ACTION_FORCE32        = 0x7FFFFFFF
} wapi_media_action_t;

/* Event type for media action callbacks: WAPI_EVENT_MEDIA_ACTION = 0x1200 */
#define WAPI_EVENT_MEDIA_ACTION 0x1200

/* ============================================================
 * Media Metadata
 * ============================================================
 *
 * Layout (64 bytes, align 8):
 *   Offset  0: wapi_stringview_t title       (16 bytes)
 *   Offset 16: wapi_stringview_t artist      (16 bytes)
 *   Offset 32: wapi_stringview_t album       (16 bytes)
 *   Offset 48: uint64_t artwork_data          Linear memory address of PNG/JPEG data
 *   Offset 56: uint32_t artwork_len
 *   Offset 60: uint32_t _pad
 */

typedef struct wapi_media_metadata_t {
    wapi_stringview_t title;
    wapi_stringview_t artist;
    wapi_stringview_t album;
    uint64_t    artwork_data;   /* Linear memory address of PNG/JPEG image data */
    wapi_size_t artwork_len;
    uint32_t    _pad;
} wapi_media_metadata_t;

_Static_assert(sizeof(wapi_media_metadata_t) == 64,
               "wapi_media_metadata_t must be 64 bytes");
_Static_assert(_Alignof(wapi_media_metadata_t) == 8,
               "wapi_media_metadata_t must be 8-byte aligned");

/* ============================================================
 * Media Session Functions
 * ============================================================ */

/**
 * Set the now-playing metadata displayed by OS media controls.
 *
 * @param metadata  Media metadata (title, artist, album, artwork).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_media, set_metadata)
wapi_result_t wapi_media_set_metadata(const wapi_media_metadata_t* metadata);

/**
 * Set the current playback state.
 *
 * @param state  Playback state: 0 = none, 1 = paused, 2 = playing.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_media, set_playback_state)
wapi_result_t wapi_media_set_playback_state(uint32_t state);

/**
 * Set the current playback position and total duration.
 *
 * @param position_seconds  Current position in seconds.
 * @param duration_seconds  Total duration in seconds.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (f32, f32) -> i32
 */
WAPI_IMPORT(wapi_media, set_position)
wapi_result_t wapi_media_set_position(float position_seconds,
                                      float duration_seconds);

/**
 * Declare which media actions the application handles.
 * The host will deliver WAPI_EVENT_MEDIA_ACTION events for
 * registered actions when the user interacts with OS media controls.
 *
 * @param actions  Array of wapi_media_action_t values.
 * @param count    Number of actions in the array.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_media, set_actions)
wapi_result_t wapi_media_set_actions(const uint32_t* actions, wapi_size_t count);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_MEDIASESSION_H */
