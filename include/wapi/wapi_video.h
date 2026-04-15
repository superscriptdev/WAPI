/**
 * WAPI - Video / Media Playback
 * Version 1.0.0
 *
 * Descriptor-based video playback with a state machine model.
 * Follows the same create/destroy handle pattern as wapi_audio.
 *
 * The module creates a video from a URL or memory buffer, controls
 * playback, and retrieves decoded frames as GPU textures. The host
 * handles codec selection, hardware decoding, and A/V sync.
 *
 * Import module: "wapi_video"
 */

#ifndef WAPI_VIDEO_H
#define WAPI_VIDEO_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Video Codec
 * ============================================================ */

typedef enum wapi_video_codec_t {
    WAPI_VIDEO_CODEC_H264    = 0,
    WAPI_VIDEO_CODEC_H265    = 1,
    WAPI_VIDEO_CODEC_VP9     = 2,
    WAPI_VIDEO_CODEC_AV1     = 3,
    WAPI_VIDEO_CODEC_FORCE32 = 0x7FFFFFFF
} wapi_video_codec_t;

/* ============================================================
 * Video State
 * ============================================================ */

typedef enum wapi_video_state_t {
    WAPI_VIDEO_STATE_IDLE    = 0,
    WAPI_VIDEO_STATE_LOADING = 1,
    WAPI_VIDEO_STATE_READY   = 2,
    WAPI_VIDEO_STATE_PLAYING = 3,
    WAPI_VIDEO_STATE_PAUSED  = 4,
    WAPI_VIDEO_STATE_ENDED   = 5,
    WAPI_VIDEO_STATE_ERROR   = 6,
    WAPI_VIDEO_STATE_FORCE32 = 0x7FFFFFFF
} wapi_video_state_t;

/* ============================================================
 * Video Source Type
 * ============================================================ */

typedef enum wapi_video_source_t {
    WAPI_VIDEO_SOURCE_URL     = 0,  /* Load from URL/path */
    WAPI_VIDEO_SOURCE_MEMORY  = 1,  /* Decode from memory buffer */
    WAPI_VIDEO_SOURCE_FORCE32 = 0x7FFFFFFF
} wapi_video_source_t;

/* ============================================================
 * Video Descriptor
 * ============================================================
 * Describes the video source for creation.
 *
 * Layout (32 bytes, align 8):
 *   Offset  0: uint64_t nextInChain  Linear memory address of chained struct, or 0
 *   Offset  8: uint32_t source       (wapi_video_source_t)
 *   Offset 12: uint32_t _pad0
 *   Offset 16: uint64_t data         Linear memory address of URL or video data
 *   Offset 24: uint32_t data_len     Byte length of data
 *   Offset 28: uint32_t codec_hint   (wapi_video_codec_t, hint for decoder)
 */

typedef struct wapi_video_desc_t {
    uint64_t               nextInChain;  /* Address of wapi_chain_t, or 0 */
    uint32_t               source;      /* wapi_video_source_t */
    uint32_t               _pad0;
    uint64_t               data;        /* Address of data (URL or buffer) */
    wapi_size_t              data_len;
    uint32_t               codec_hint;  /* wapi_video_codec_t */
} wapi_video_desc_t;

/* ============================================================
 * Video Info
 * ============================================================
 * Read-only information about a loaded video.
 *
 * Layout (20 bytes, align 4):
 *   Offset  0: int32_t  width        Frame width in pixels
 *   Offset  4: int32_t  height       Frame height in pixels
 *   Offset  8: float    duration     Total duration in seconds
 *   Offset 12: float    frame_rate   Frames per second
 *   Offset 16: uint32_t codec        (wapi_video_codec_t)
 */

typedef struct wapi_video_info_t {
    int32_t     width;
    int32_t     height;
    float       duration;    /* Total duration in seconds */
    float       frame_rate;  /* Frames per second */
    uint32_t    codec;       /* wapi_video_codec_t */
} wapi_video_info_t;

_Static_assert(sizeof(wapi_video_info_t) == 20, "wapi_video_info_t must be 20 bytes");
_Static_assert(_Alignof(wapi_video_info_t) == 4, "wapi_video_info_t must be 4-byte aligned");

/* ============================================================
 * Lifecycle
 * ============================================================ */

/**
 * Create a video from a descriptor.
 *
 * @param desc   Video source descriptor.
 * @param video  [out] Video handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_video, create)
wapi_result_t wapi_video_create(const wapi_video_desc_t* desc, wapi_handle_t* video);

/**
 * Destroy a video and release all associated resources.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_video, destroy)
wapi_result_t wapi_video_destroy(wapi_handle_t video);

/**
 * Get information about a loaded video.
 *
 * @param video  Video handle.
 * @param info   [out] Video information.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_video, get_info)
wapi_result_t wapi_video_get_info(wapi_handle_t video, wapi_video_info_t* info);

/* ============================================================
 * Playback Control
 * ============================================================ */

/**
 * Start or resume video playback.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_video, play)
wapi_result_t wapi_video_play(wapi_handle_t video);

/**
 * Pause video playback.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_video, pause)
wapi_result_t wapi_video_pause(wapi_handle_t video);

/**
 * Seek to a position in the video.
 *
 * @param video         Video handle.
 * @param time_seconds  Target position in seconds.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, f32) -> i32
 */
WAPI_IMPORT(wapi_video, seek)
wapi_result_t wapi_video_seek(wapi_handle_t video, float time_seconds);

/* ============================================================
 * State and Position Queries
 * ============================================================ */

/**
 * Get the current playback state.
 *
 * @param video  Video handle.
 * @param state  [out] Current state.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_video, get_state)
wapi_result_t wapi_video_get_state(wapi_handle_t video, wapi_video_state_t* state);

/**
 * Get the current playback position.
 *
 * @param video         Video handle.
 * @param time_seconds  [out] Current position in seconds.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_video, get_position)
wapi_result_t wapi_video_get_position(wapi_handle_t video, float* time_seconds);

/* ============================================================
 * Frame Access
 * ============================================================ */

/**
 * Get the current decoded frame as a GPU texture.
 * The returned texture handle is valid until the next call to
 * this function or the next wapi_frame boundary.
 *
 * @param video        Video handle.
 * @param gpu_texture  [out] GPU texture handle for the current frame.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_video, get_frame_texture)
wapi_result_t wapi_video_get_frame_texture(wapi_handle_t video,
                                        wapi_handle_t* gpu_texture);

/**
 * Render the current video frame into a region of a GPU texture.
 *
 * @param video        Video handle.
 * @param gpu_texture  Destination GPU texture handle (from wapi_gpu).
 * @param x            X offset in the texture.
 * @param y            Y offset in the texture.
 * @param width        Render width.
 * @param height       Render height.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_video, blit)
wapi_result_t wapi_video_blit(wapi_handle_t video,
                                        wapi_handle_t gpu_texture,
                                        int32_t x, int32_t y,
                                        int32_t width, int32_t height);

/* ============================================================
 * Audio Integration
 * ============================================================ */

/**
 * Bind the video's audio track to an existing wapi_audio stream.
 * The host synchronizes audio and video playback.
 *
 * @param video         Video handle.
 * @param audio_stream  Audio stream handle (from wapi_audio).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_video, bind_audio)
wapi_result_t wapi_video_bind_audio(wapi_handle_t video, wapi_handle_t audio_stream);

/**
 * Set the video's audio volume.
 *
 * @param video   Video handle.
 * @param volume  Volume level (0.0 = silent, 1.0 = full).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, f32) -> i32
 */
WAPI_IMPORT(wapi_video, set_volume)
wapi_result_t wapi_video_set_volume(wapi_handle_t video, float volume);

/**
 * Mute or unmute the video's audio.
 *
 * @param video  Video handle.
 * @param muted  1 to mute, 0 to unmute.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_video, set_muted)
wapi_result_t wapi_video_set_muted(wapi_handle_t video, wapi_bool_t muted);

/* ============================================================
 * Playback Options
 * ============================================================ */

/**
 * Enable or disable looping playback.
 *
 * @param video  Video handle.
 * @param loop   1 to loop, 0 for single play.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_video, set_loop)
wapi_result_t wapi_video_set_loop(wapi_handle_t video, wapi_bool_t loop);

/**
 * Set the playback rate.
 *
 * @param video  Video handle.
 * @param rate   Playback rate (1.0 = normal, 2.0 = double speed).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, f32) -> i32
 */
WAPI_IMPORT(wapi_video, set_playback_rate)
wapi_result_t wapi_video_set_playback_rate(wapi_handle_t video, float rate);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_VIDEO_H */
