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
 *   4. Retrieve:  submit WAPI_IO_OP_CODEC_OUTPUT_GET to copy the data
 *   5. Drain:     submit WAPI_IO_OP_CODEC_FLUSH; keep retrieving until done
 *   6. Destroy:   wapi_codec_destroy
 *
 * Output is asynchronous -- hardware decoders pipeline multiple frames
 * and may reorder output (B-frames). Completions arrive as
 * WAPI_EVENT_IO_COMPLETION events with the submitted user_data.
 *
 * @see WAPI_IO_OP_CODEC_DECODE, WAPI_IO_OP_CODEC_ENCODE,
 *      WAPI_IO_OP_CODEC_OUTPUT_GET, WAPI_IO_OP_CODEC_FLUSH
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

typedef struct wapi_video_codec_desc_t {
    uint32_t codec;          /* wapi_codec_type_t */
    uint32_t mode;           /* wapi_codec_mode_t */
    int32_t  width;
    int32_t  height;
    uint32_t bitrate;        /* bits per second (encode only) */
    float    framerate;      /* fps (encode only) */
    uint32_t profile;        /* codec-specific profile */
    uint32_t _reserved;
} wapi_video_codec_desc_t;

_Static_assert(sizeof(wapi_video_codec_desc_t) == 32,
               "wapi_video_codec_desc_t must be 32 bytes");
_Static_assert(_Alignof(wapi_video_codec_desc_t) == 4,
               "wapi_video_codec_desc_t must be 4-byte aligned");

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

typedef struct wapi_audio_codec_desc_t {
    uint32_t codec;          /* wapi_codec_type_t */
    uint32_t mode;           /* wapi_codec_mode_t */
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bitrate;
    uint32_t _reserved;
} wapi_audio_codec_desc_t;

_Static_assert(sizeof(wapi_audio_codec_desc_t) == 24,
               "wapi_audio_codec_desc_t must be 24 bytes");
_Static_assert(_Alignof(wapi_audio_codec_desc_t) == 4,
               "wapi_audio_codec_desc_t must be 4-byte aligned");

/* ============================================================
 * Encoded Chunk Descriptor
 * ============================================================
 *
 * Layout (40 bytes, align 8):
 *   Offset  0: uint64_t    data         Linear memory address of encoded data
 *   Offset  8: uint32_t    data_len
 *   Offset 12: uint32_t    _pad0
 *   Offset 16: uint64_t    timestamp_us (presentation timestamp, microseconds)
 *   Offset 24: wapi_flags_t flags       (0x1 = keyframe)
 *   Offset 32: uint32_t    _reserved
 *   Offset 36: uint32_t    _pad1
 */

typedef struct wapi_codec_chunk_t {
    uint64_t    data;          /* Linear memory address of encoded data */
    wapi_size_t data_len;
    uint32_t    _pad0;
    uint64_t    timestamp_us;  /* presentation timestamp in microseconds */
    wapi_flags_t flags;        /* 0x1 = keyframe */
    uint32_t    _reserved;
    uint32_t    _pad1;
} wapi_codec_chunk_t;

_Static_assert(sizeof(wapi_codec_chunk_t) == 40,
               "wapi_codec_chunk_t must be 40 bytes");
_Static_assert(_Alignof(wapi_codec_chunk_t) == 8,
               "wapi_codec_chunk_t must be 8-byte aligned");

/* Chunk flags */
#define WAPI_CODEC_FLAG_KEYFRAME 0x1

/* ============================================================
 * Codec Lifecycle
 * ============================================================ */

/**
 * Create a video encoder or decoder.
 *
 * @param desc   Video codec descriptor.
 * @param codec  [out] Codec handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_codec, create_video)
wapi_result_t wapi_codec_create_video(const wapi_video_codec_desc_t* desc,
                                      wapi_handle_t* codec);

/**
 * Create an audio encoder or decoder.
 *
 * @param desc   Audio codec descriptor.
 * @param codec  [out] Codec handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_codec, create_audio)
wapi_result_t wapi_codec_create_audio(const wapi_audio_codec_desc_t* desc,
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

/* ============================================================
 * Capability Query
 * ============================================================
 *
 * Probe codec support before creating an encoder/decoder.
 * Maps to: Media Capabilities API (Web), MediaCodecList (Android).
 */

/* -------- Query descriptor (28 bytes, align 4) --------
 *   Offset  0: uint32_t codec        (wapi_codec_type_t)
 *   Offset  4: int32_t  width        (video: resolution width, 0 for audio)
 *   Offset  8: int32_t  height       (video: resolution height)
 *   Offset 12: uint32_t bitrate
 *   Offset 16: float    framerate
 *   Offset 20: uint32_t sample_rate  (audio: sample rate)
 *   Offset 24: uint32_t channels     (audio: channel count)
 */

typedef struct wapi_codec_caps_query_t {
    uint32_t codec;          /* wapi_codec_type_t */
    int32_t  width;          /* Video: resolution width (0 for audio) */
    int32_t  height;         /* Video: resolution height */
    uint32_t bitrate;
    float    framerate;
    uint32_t sample_rate;    /* Audio: sample rate */
    uint32_t channels;       /* Audio: channel count */
} wapi_codec_caps_query_t;

_Static_assert(sizeof(wapi_codec_caps_query_t) == 28,
               "wapi_codec_caps_query_t must be 28 bytes");
_Static_assert(_Alignof(wapi_codec_caps_query_t) == 4,
               "wapi_codec_caps_query_t must be 4-byte aligned");

/* -------- Query result (12 bytes, align 4) --------
 *   Offset 0: int32_t supported
 *   Offset 4: int32_t smooth           (can decode smoothly)
 *   Offset 8: int32_t power_efficient  (hardware-accelerated)
 */

typedef struct wapi_codec_caps_result_t {
    wapi_bool_t supported;
    wapi_bool_t smooth;           /* Can decode smoothly at given resolution */
    wapi_bool_t power_efficient;  /* Hardware-accelerated */
} wapi_codec_caps_result_t;

_Static_assert(sizeof(wapi_codec_caps_result_t) == 12,
               "wapi_codec_caps_result_t must be 12 bytes");
_Static_assert(_Alignof(wapi_codec_caps_result_t) == 4,
               "wapi_codec_caps_result_t must be 4-byte aligned");

/**
 * Query whether a codec configuration is supported for decoding.
 *
 * @param query   Codec parameters to test.
 * @param result  [out] Capability result.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_codec, query_decode)
wapi_result_t wapi_codec_query_decode(const wapi_codec_caps_query_t* query,
                                      wapi_codec_caps_result_t* result);

/**
 * Query whether a codec configuration is supported for encoding.
 *
 * @param query   Codec parameters to test.
 * @param result  [out] Capability result.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_codec, query_encode)
wapi_result_t wapi_codec_query_encode(const wapi_codec_caps_query_t* query,
                                      wapi_codec_caps_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CODEC_H */
