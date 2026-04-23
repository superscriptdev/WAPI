/**
 * WAPI - Audio
 * Version 1.0.0
 *
 * Endpoints are acquired through the role system (WAPI_ROLE_AUDIO_PLAYBACK
 * / WAPI_ROLE_AUDIO_RECORDING). This header owns the audio format/spec
 * types, the stream use surface, and endpoint metadata queries.
 *
 * Import module: "wapi_audio"
 */

#ifndef WAPI_AUDIO_H
#define WAPI_AUDIO_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Audio Format
 * ============================================================ */

typedef enum wapi_audio_format_t {
    WAPI_AUDIO_U8     = 0x0008,  /* Unsigned 8-bit */
    WAPI_AUDIO_S16    = 0x8010,  /* Signed 16-bit little-endian */
    WAPI_AUDIO_S32    = 0x8020,  /* Signed 32-bit little-endian */
    WAPI_AUDIO_F32    = 0x8120,  /* 32-bit float little-endian */
    WAPI_AUDIO_FORCE32 = 0x7FFFFFFF
} wapi_audio_format_t;

/* ============================================================
 * Audio Spec
 * ============================================================
 * Used both as role-request prefs and as stream source/dest format.
 *
 * Layout (12 bytes, align 4):
 *   Offset 0: uint32_t format    (wapi_audio_format_t)
 *   Offset 4: int32_t  channels  (1=mono, 2=stereo, ...)
 *   Offset 8: int32_t  freq      (sample rate in Hz)
 */
typedef struct wapi_audio_spec_t {
    uint32_t    format;
    int32_t     channels;
    int32_t     freq;
} wapi_audio_spec_t;

_Static_assert(sizeof(wapi_audio_spec_t) == 12, "wapi_audio_spec_t must be 12 bytes");
_Static_assert(_Alignof(wapi_audio_spec_t) == 4, "wapi_audio_spec_t must be 4-byte aligned");

/* ============================================================
 * Endpoint Metadata
 * ============================================================ */

typedef enum wapi_audio_form_t {
    WAPI_AUDIO_FORM_UNKNOWN    = 0,
    WAPI_AUDIO_FORM_SPEAKERS   = 1,  /* room, shared */
    WAPI_AUDIO_FORM_HEADPHONES = 2,  /* personal, 2ch */
    WAPI_AUDIO_FORM_HEADSET    = 3,  /* personal + paired recording */
    WAPI_AUDIO_FORM_LINEOUT    = 4,
    WAPI_AUDIO_FORM_BUILTIN    = 5,
    WAPI_AUDIO_FORM_FORCE32    = 0x7FFFFFFF
} wapi_audio_form_t;

/**
 * Metadata about a resolved audio endpoint.
 *
 * Layout (32 bytes, align 4):
 *   Offset  0: wapi_audio_spec_t native_spec (12 bytes)
 *   Offset 12: uint32_t          form         (wapi_audio_form_t)
 *   Offset 16: uint8_t           uid[16]      (stable per physical device)
 */
typedef struct wapi_audio_endpoint_info_t {
    wapi_audio_spec_t native_spec;
    uint32_t          form;
    uint8_t           uid[16];
} wapi_audio_endpoint_info_t;

_Static_assert(sizeof(wapi_audio_endpoint_info_t) == 32, "wapi_audio_endpoint_info_t must be 32 bytes");
_Static_assert(_Alignof(wapi_audio_endpoint_info_t) == 4, "wapi_audio_endpoint_info_t must be 4-byte aligned");

/**
 * Query metadata for a granted audio endpoint.
 * name_buf receives a UTF-8 display label; may be a generic redaction
 * if the host has not released the real label.
 *
 * Wasm signature: (i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_audio, endpoint_info)
wapi_result_t wapi_audio_endpoint_info(wapi_handle_t endpoint,
                                       wapi_audio_endpoint_info_t* out,
                                       char* name_buf, wapi_size_t name_buf_len,
                                       wapi_size_t* name_len);

/* ============================================================
 * Endpoint Control
 * ============================================================ */

/** Close a granted endpoint. Wasm: (i32) -> i32 */
WAPI_IMPORT(wapi_audio, close)
wapi_result_t wapi_audio_close(wapi_handle_t endpoint);

/** Unpause. Endpoints start paused after grant. Wasm: (i32) -> i32 */
WAPI_IMPORT(wapi_audio, resume)
wapi_result_t wapi_audio_resume(wapi_handle_t endpoint);

/** Pause. Wasm: (i32) -> i32 */
WAPI_IMPORT(wapi_audio, pause)
wapi_result_t wapi_audio_pause(wapi_handle_t endpoint);

/* ============================================================
 * Audio Stream
 * ============================================================
 * Converts between the module's format and the endpoint's format.
 * Playback: module pushes into the stream; host pulls for the endpoint.
 * Recording: host pushes from the endpoint; module pulls from the stream.
 */

/** Wasm: (i32, i32, i32) -> i32 */
WAPI_IMPORT(wapi_audio, create_stream)
wapi_result_t wapi_audio_create_stream(const wapi_audio_spec_t* src_spec,
                                       const wapi_audio_spec_t* dst_spec,
                                       wapi_handle_t* stream);

/** Wasm: (i32) -> i32 */
WAPI_IMPORT(wapi_audio, destroy_stream)
wapi_result_t wapi_audio_destroy_stream(wapi_handle_t stream);

/** Wasm: (i32, i32) -> i32 */
WAPI_IMPORT(wapi_audio, bind_stream)
wapi_result_t wapi_audio_bind_stream(wapi_handle_t endpoint, wapi_handle_t stream);

/** Wasm: (i32) -> i32 */
WAPI_IMPORT(wapi_audio, unbind_stream)
wapi_result_t wapi_audio_unbind_stream(wapi_handle_t stream);

/** Wasm: (i32, i32, i32) -> i32 */
WAPI_IMPORT(wapi_audio, put_stream_data)
wapi_result_t wapi_audio_put_stream_data(wapi_handle_t stream, const void* buf,
                                         wapi_size_t len);

/** Wasm: (i32, i32, i32, i32) -> i32 */
WAPI_IMPORT(wapi_audio, get_stream_data)
wapi_result_t wapi_audio_get_stream_data(wapi_handle_t stream, void* buf,
                                         wapi_size_t len, wapi_size_t* bytes_read);

/** Bytes currently available to read from the stream. Wasm: (i32) -> i32 */
WAPI_IMPORT(wapi_audio, stream_available)
int32_t wapi_audio_stream_available(wapi_handle_t stream);

/** Bytes currently queued in the stream's write buffer. Wasm: (i32) -> i32 */
WAPI_IMPORT(wapi_audio, stream_queued)
int32_t wapi_audio_stream_queued(wapi_handle_t stream);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_AUDIO_H */
