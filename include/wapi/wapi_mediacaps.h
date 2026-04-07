/**
 * WAPI - Media Capabilities
 * Version 1.0.0
 *
 * Query codec and format support before creating encoders/decoders.
 * Maps to: Media Capabilities API (Web), MediaCodecList (Android)
 *
 * Import module: "wapi_mediacaps"
 */

#ifndef WAPI_MEDIACAPS_H
#define WAPI_MEDIACAPS_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Media Capabilities Query
 * ============================================================
 *
 * Layout (28 bytes, align 4):
 *   Offset  0: uint32_t codec        (wapi_codec_type_t)
 *   Offset  4: int32_t  width        (video: resolution width, 0 for audio)
 *   Offset  8: int32_t  height       (video: resolution height)
 *   Offset 12: uint32_t bitrate
 *   Offset 16: float    framerate
 *   Offset 20: uint32_t sample_rate  (audio: sample rate)
 *   Offset 24: uint32_t channels     (audio: channel count)
 */

typedef struct wapi_media_caps_query_t {
    uint32_t codec;          /* wapi_codec_type_t */
    int32_t  width;          /* Video: resolution width (0 for audio) */
    int32_t  height;         /* Video: resolution height */
    uint32_t bitrate;
    float    framerate;
    uint32_t sample_rate;    /* Audio: sample rate */
    uint32_t channels;       /* Audio: channel count */
} wapi_media_caps_query_t;

_Static_assert(sizeof(wapi_media_caps_query_t) == 28,
               "wapi_media_caps_query_t must be 28 bytes");

/* ============================================================
 * Media Capabilities Result
 * ============================================================
 *
 * Layout (12 bytes, align 4):
 *   Offset 0: int32_t supported
 *   Offset 4: int32_t smooth           (can decode smoothly)
 *   Offset 8: int32_t power_efficient  (hardware-accelerated)
 */

typedef struct wapi_media_caps_result_t {
    wapi_bool_t supported;
    wapi_bool_t smooth;           /* Can decode smoothly at given resolution */
    wapi_bool_t power_efficient;  /* Hardware-accelerated */
} wapi_media_caps_result_t;

_Static_assert(sizeof(wapi_media_caps_result_t) == 12,
               "wapi_media_caps_result_t must be 12 bytes");

/* ============================================================
 * Query Functions
 * ============================================================ */

/**
 * Query whether a codec configuration is supported for decoding.
 *
 * @param query   Codec parameters to test.
 * @param result  [out] Capability result.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_mediacaps, query_decode)
wapi_result_t wapi_mediacaps_query_decode(const wapi_media_caps_query_t* query,
                                          wapi_media_caps_result_t* result);

/**
 * Query whether a codec configuration is supported for encoding.
 *
 * @param query   Codec parameters to test.
 * @param result  [out] Capability result.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_mediacaps, query_encode)
wapi_result_t wapi_mediacaps_query_encode(const wapi_media_caps_query_t* query,
                                          wapi_media_caps_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_MEDIACAPS_H */
