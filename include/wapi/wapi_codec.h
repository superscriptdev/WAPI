/**
 * WAPI - Codec
 * Version 1.0.0
 *
 * Hardware-accelerated video/audio encode and decode.
 *
 * Follows the universal codec pipeline pattern:
 *   1. Create:    wapi_codec_create_video / wapi_codec_create_audio
 *   2. Feed:      submit WAPI_IO_OP_CODEC_DECODE or WAPI_IO_OP_CODEC_ENCODE
 *   3. Receive:   WAPI_EVENT_IO_COMPLETION arrives when output is ready
 *   4. Retrieve:  submit WAPI_IO_OP_CODEC_GET_OUTPUT to copy the data
 *   5. Drain:     submit WAPI_IO_OP_CODEC_FLUSH; keep retrieving until done
 *   6. Destroy:   wapi_codec_destroy
 *
 * Output is asynchronous -- hardware decoders pipeline multiple frames
 * and may reorder output (B-frames). Completions arrive as
 * WAPI_EVENT_IO_COMPLETION events with the submitted user_data.
 *
 * @see WAPI_IO_OP_CODEC_DECODE, WAPI_IO_OP_CODEC_ENCODE,
 *      WAPI_IO_OP_CODEC_GET_OUTPUT, WAPI_IO_OP_CODEC_FLUSH
 *
 * Platform mapping:
 *   Web:     WebCodecs (VideoEncoder/Decoder, AudioEncoder/Decoder)
 *   Android: MediaCodec
 *   iOS/Mac: VideoToolbox / AudioToolbox
 *   Windows: Media Foundation Transforms
 *   Linux:   VA-API, NVENC/NVDEC, or FFmpeg
 *
 * Import module: "wapi_codec"
 */

#ifndef WAPI_CODEC_H
#define WAPI_CODEC_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Codec Types
 * ============================================================ */

typedef enum wapi_codec_type_t {
    WAPI_CODEC_H264    = 0,
    WAPI_CODEC_H265    = 1,
    WAPI_CODEC_VP9     = 2,
    WAPI_CODEC_AV1     = 3,
    WAPI_CODEC_AAC     = 10,
    WAPI_CODEC_OPUS    = 11,
    WAPI_CODEC_VORBIS  = 12,
    WAPI_CODEC_FLAC    = 13,
    WAPI_CODEC_FORCE32 = 0x7FFFFFFF
} wapi_codec_type_t;

typedef enum wapi_codec_mode_t {
    WAPI_CODEC_DECODE       = 0,
    WAPI_CODEC_ENCODE       = 1,
    WAPI_CODEC_FORCE32_MODE = 0x7FFFFFFF
} wapi_codec_mode_t;

/* ============================================================
 * Video Codec Configuration
 * ============================================================
 *
 * Layout (32 bytes, align 4):
 *   Offset  0: uint32_t codec       (wapi_codec_type_t)
 *   Offset  4: uint32_t mode        (wapi_codec_mode_t)
 *   Offset  8: int32_t  width
 *   Offset 12: int32_t  height
 *   Offset 16: uint32_t bitrate     (bits per second, encode only)
 *   Offset 20: float    framerate   (fps, encode only)
 *   Offset 24: uint32_t profile     (codec-specific profile)
 *   Offset 28: uint32_t _reserved
 */

typedef struct wapi_video_codec_config_t {
    uint32_t codec;          /* wapi_codec_type_t */
    uint32_t mode;           /* wapi_codec_mode_t */
    int32_t  width;
    int32_t  height;
    uint32_t bitrate;        /* bits per second (encode only) */
    float    framerate;      /* fps (encode only) */
    uint32_t profile;        /* codec-specific profile */
    uint32_t _reserved;
} wapi_video_codec_config_t;

_Static_assert(sizeof(wapi_video_codec_config_t) == 32,
               "wapi_video_codec_config_t must be 32 bytes");

/* ============================================================
 * Audio Codec Configuration
 * ============================================================
 *
 * Layout (24 bytes, align 4):
 *   Offset  0: uint32_t codec
 *   Offset  4: uint32_t mode
 *   Offset  8: uint32_t sample_rate
 *   Offset 12: uint32_t channels
 *   Offset 16: uint32_t bitrate
 *   Offset 20: uint32_t _reserved
 */

typedef struct wapi_audio_codec_config_t {
    uint32_t codec;          /* wapi_codec_type_t */
    uint32_t mode;           /* wapi_codec_mode_t */
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bitrate;
    uint32_t _reserved;
} wapi_audio_codec_config_t;

_Static_assert(sizeof(wapi_audio_codec_config_t) == 24,
               "wapi_audio_codec_config_t must be 24 bytes");

/* ============================================================
 * Encoded Chunk Descriptor
 * ============================================================
 *
 * Layout (32 bytes on wasm32, align 8):
 *   Offset  0: ptr         data         (pointer to encoded data)
 *   Offset  4: uint32_t    data_len
 *   Offset  8: uint64_t    timestamp_us (presentation timestamp, microseconds)
 *   Offset 16: wapi_flags_t flags       (0x1 = keyframe)
 *   Offset 24: uint32_t    _reserved
 *   Offset 28: (4 bytes padding)
 */

typedef struct wapi_codec_chunk_t {
    const void* data;
    wapi_size_t data_len;
    uint64_t    timestamp_us;  /* presentation timestamp in microseconds */
    wapi_flags_t flags;        /* 0x1 = keyframe */
    uint32_t    _reserved;
} wapi_codec_chunk_t;

#ifdef __wasm__
_Static_assert(sizeof(wapi_codec_chunk_t) == 32,
               "wapi_codec_chunk_t must be 32 bytes on wasm32");
#endif

/* Chunk flags */
#define WAPI_CODEC_FLAG_KEYFRAME 0x1

/* ============================================================
 * Codec Lifecycle
 * ============================================================ */

/**
 * Create a video encoder or decoder.
 *
 * @param config  Video codec configuration.
 * @param codec   [out] Codec handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_codec, create_video)
wapi_result_t wapi_codec_create_video(const wapi_video_codec_config_t* config,
                                      wapi_handle_t* codec);

/**
 * Create an audio encoder or decoder.
 *
 * @param config  Audio codec configuration.
 * @param codec   [out] Codec handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_codec, create_audio)
wapi_result_t wapi_codec_create_audio(const wapi_audio_codec_config_t* config,
                                      wapi_handle_t* codec);

/**
 * Destroy a codec instance and release associated resources.
 *
 * @param codec  Codec handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_codec, destroy)
wapi_result_t wapi_codec_destroy(wapi_handle_t codec);

/**
 * Check if a codec type and mode are supported by the host.
 *
 * @param codec_type  Codec type (wapi_codec_type_t).
 * @param mode        Decode or encode (wapi_codec_mode_t).
 * @return WAPI_OK if supported, WAPI_ERR_NOTSUP otherwise.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_codec, is_supported)
wapi_result_t wapi_codec_is_supported(uint32_t codec_type, uint32_t mode);

/* ============================================================
 * Decode / Encode (input side)
 * ============================================================ */

/**
 * Submit an encoded chunk for decoding.
 * Output arrives asynchronously as WAPI_EVENT_IO_COMPLETION.
 *
 * @param codec  Codec handle (configured for decode).
 * @param chunk  Encoded chunk descriptor.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_codec, decode)
wapi_result_t wapi_codec_decode(wapi_handle_t codec,
                                const wapi_codec_chunk_t* chunk);

/**
 * Submit raw frame data for encoding.
 * Output arrives asynchronously as WAPI_EVENT_IO_COMPLETION.
 *
 * @param codec         Codec handle (configured for encode).
 * @param data          Raw frame data (pixels or PCM samples).
 * @param data_len      Data length in bytes.
 * @param timestamp_us  Presentation timestamp in microseconds.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i64) -> i32
 */
WAPI_IMPORT(wapi_codec, encode)
wapi_result_t wapi_codec_encode(wapi_handle_t codec, const void* data,
                                wapi_size_t data_len, uint64_t timestamp_us);

/* ============================================================
 * Output Retrieval (called after WAPI_EVENT_CODEC_OUTPUT)
 * ============================================================ */

/**
 * Retrieve decoded or encoded output data.
 *
 * Call this after receiving a WAPI_EVENT_IO_COMPLETION event.
 * For a decoder, this returns decoded raw frame data.
 * For an encoder, this returns an encoded chunk.
 *
 * @param codec         Codec handle.
 * @param buf           Buffer to receive output data.
 * @param buf_len       Buffer capacity.
 * @param out_len       [out] Actual bytes written.
 * @param timestamp_us  [out] Presentation timestamp in microseconds.
 * @return WAPI_OK on success, WAPI_ERR_AGAIN if no output ready.
 *
 * Wasm signature: (i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_codec, get_output)
wapi_result_t wapi_codec_get_output(wapi_handle_t codec, void* buf,
                                    wapi_size_t buf_len, wapi_size_t* out_len,
                                    uint64_t* timestamp_us);

/**
 * Flush the codec, signaling end of input stream.
 * The host continues posting WAPI_EVENT_IO_COMPLETION events for any
 * remaining buffered output. When all output is drained, the host
 * posts a final completion with result == 0.
 *
 * @param codec  Codec handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_codec, flush)
wapi_result_t wapi_codec_flush(wapi_handle_t codec);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CODEC_H */
