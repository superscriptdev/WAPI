/**
 * WAPI - Media Session
 * Version 1.0.0
 *
 * Expose now-playing metadata to OS media controls.
 * Maps to: Media Session API (Web), MPNowPlayingInfoCenter (iOS),
 *          MediaSession (Android), SMTC (Windows)
 *
 * Import module: "wapi_media"
 */

#ifndef WAPI_MEDIA_SESSION_H
#define WAPI_MEDIA_SESSION_H

#include "wapi_types.h"

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
    WAPI_MEDIA_ACTION_PREVIOUS_TRACK = 5,
    WAPI_MEDIA_ACTION_NEXT_TRACK     = 6,
    WAPI_MEDIA_ACTION_FORCE32        = 0x7FFFFFFF
} wapi_media_action_t;

/* Event type for media action callbacks: WAPI_EVENT_MEDIA_ACTION = 0x1200 */
#define WAPI_EVENT_MEDIA_ACTION 0x1200

/* ============================================================
 * Media Metadata
 * ============================================================
 *
 * Layout (32 bytes on wasm32, align 4):
 *   Offset  0: ptr      title
 *   Offset  4: uint32_t title_len
 *   Offset  8: ptr      artist
 *   Offset 12: uint32_t artist_len
 *   Offset 16: ptr      album
 *   Offset 20: uint32_t album_len
 *   Offset 24: ptr      artwork_data   (PNG/JPEG image data)
 *   Offset 28: uint32_t artwork_len
 */

typedef struct wapi_media_metadata_t {
    const char* title;
    wapi_size_t title_len;
    const char* artist;
    wapi_size_t artist_len;
    const char* album;
    wapi_size_t album_len;
    const void* artwork_data;   /* PNG/JPEG image data */
    wapi_size_t artwork_len;
} wapi_media_metadata_t;

#ifdef __wasm__
_Static_assert(sizeof(wapi_media_metadata_t) == 32,
               "wapi_media_metadata_t must be 32 bytes on wasm32");
#endif

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

#endif /* WAPI_MEDIA_SESSION_H */
