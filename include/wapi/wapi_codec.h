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
 * Maps to:
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
 * Codec Operations
 *
 * create/decode/encode/get_output submit through the IO vtable.
 * destroy, is_supported, flush are bounded-local (act on an
 * already-owned handle or a local capability probe). query_decode /
 * query_encode probe the host asynchronously — they go through IO.
 * ============================================================ */

/** Create a video codec. Config is sync on every platform WebCodecs
 *  is modelled after; we go through IO so the shim can prepare the
 *  VideoDecoder/Encoder on the correct thread. */
static inline wapi_result_t wapi_codec_create_video(
    const wapi_io_t* io, const wapi_video_codec_desc_t* desc,
    wapi_handle_t* out_codec, uint64_t user_data)
{
    /* Reuse the existing VIDEO_CREATE opcode for codec's video creator too. */
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_VIDEO_CREATE;
    op.flags      = 1; /* flag: this is a codec create, not a playback element */
    op.addr       = (uint64_t)(uintptr_t)desc;
    op.len        = sizeof(*desc);
    op.result_ptr = (uint64_t)(uintptr_t)out_codec;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Create an audio codec. */
static inline wapi_result_t wapi_codec_create_audio(
    const wapi_io_t* io, const wapi_audio_codec_desc_t* desc,
    wapi_handle_t* out_codec, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_VIDEO_CREATE; /* shared opcode; flags=2 → audio codec */
    op.flags      = 2;
    op.addr       = (uint64_t)(uintptr_t)desc;
    op.len        = sizeof(*desc);
    op.result_ptr = (uint64_t)(uintptr_t)out_codec;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Bounded-local: destroy codec. */
WAPI_IMPORT(wapi_codec, destroy)
wapi_result_t wapi_codec_destroy(wapi_handle_t codec);

/** Bounded-local: cached capability probe. */
WAPI_IMPORT(wapi_codec, is_supported)
wapi_result_t wapi_codec_is_supported(uint32_t codec_type, uint32_t mode);

/** Submit an encoded chunk to a decoder. */
static inline wapi_result_t wapi_codec_decode(
    const wapi_io_t* io, wapi_handle_t codec,
    const wapi_codec_chunk_t* chunk, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CODEC_DECODE;
    op.fd        = codec;
    op.offset    = chunk->timestamp_us;
    op.addr      = chunk->data;
    op.len       = chunk->data_len;
    op.flags     = chunk->flags;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Submit raw frame data to an encoder. */
static inline wapi_result_t wapi_codec_encode(
    const wapi_io_t* io, wapi_handle_t codec,
    const void* data, wapi_size_t data_len, uint64_t timestamp_us,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CODEC_ENCODE;
    op.fd        = codec;
    op.offset    = timestamp_us;
    op.addr      = (uint64_t)(uintptr_t)data;
    op.len       = data_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Pull the next available codec output. Timestamp arrives in the
 *  completion payload bytes 0..7 as u64 (WAPI_IO_CQE_F_INLINE). */
static inline wapi_result_t wapi_codec_get_output(
    const wapi_io_t* io, wapi_handle_t codec,
    void* buf, wapi_size_t buf_len, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CODEC_OUTPUT_GET;
    op.fd        = codec;
    op.addr      = (uint64_t)(uintptr_t)buf;
    op.len       = buf_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Bounded-local: tell the codec to drain. Subsequent OUTPUT_GET pulls
 *  any buffered frames out. */
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

/** Decode-capability probe. Async because Media Capabilities API
 *  (web) is Promise-based. The 12-byte result arrives inline in the
 *  completion payload (WAPI_IO_CQE_F_INLINE). */
static inline wapi_result_t wapi_codec_query_decode(
    const wapi_io_t* io, const wapi_codec_caps_query_t* query,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = 0x104; /* WAPI_IO_OP_CODEC_QUERY_DECODE — reserved in the codec range */
    op.addr      = (uint64_t)(uintptr_t)query;
    op.len       = sizeof(*query);
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Encode-capability probe. */
static inline wapi_result_t wapi_codec_query_encode(
    const wapi_io_t* io, const wapi_codec_caps_query_t* query,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = 0x105; /* WAPI_IO_OP_CODEC_QUERY_ENCODE — reserved in the codec range */
    op.addr      = (uint64_t)(uintptr_t)query;
    op.len       = sizeof(*query);
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CODEC_H */
