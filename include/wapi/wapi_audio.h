/**
 * WAPI - Audio
 * Version 1.0.0
 *
 * SDL3-shaped audio with a push/pull stream model.
 * Supports both playback and recording (capture).
 *
 * The module creates an audio stream, optionally with format
 * conversion, and pushes/pulls samples. The host handles device
 * management, mixing, and low-level audio I/O.
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
 * Describes an audio format: sample format, channel count, and
 * sample rate.
 *
 * Layout (12 bytes, align 4):
 *   Offset 0: uint32_t format    (wapi_audio_format_t)
 *   Offset 4: int32_t  channels  (1=mono, 2=stereo, etc.)
 *   Offset 8: int32_t  freq      (sample rate in Hz)
 */

typedef struct wapi_audio_spec_t {
    uint32_t    format;     /* wapi_audio_format_t */
    int32_t     channels;
    int32_t     freq;       /* Sample rate (e.g., 44100, 48000) */
} wapi_audio_spec_t;

_Static_assert(sizeof(wapi_audio_spec_t) == 12, "wapi_audio_spec_t must be 12 bytes");
_Static_assert(_Alignof(wapi_audio_spec_t) == 4, "wapi_audio_spec_t must be 4-byte aligned");

/* ============================================================
 * Default Device IDs
 * ============================================================ */

#define WAPI_AUDIO_DEFAULT_PLAYBACK  ((wapi_handle_t)-1)
#define WAPI_AUDIO_DEFAULT_RECORDING ((wapi_handle_t)-2)

/* ============================================================
 * Audio Device Functions
 * ============================================================ */

/**
 * Open an audio device for playback or recording.
 *
 * @param device_id  Device to open (WAPI_AUDIO_DEFAULT_PLAYBACK,
 *                   WAPI_AUDIO_DEFAULT_RECORDING, or a specific device).
 * @param spec       Desired audio format (NULL for device default).
 * @param device     [out] Opened device handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_audio, open_device)
wapi_result_t wapi_audio_open_device(wapi_handle_t device_id,
                                  const wapi_audio_spec_t* spec,
                                  wapi_handle_t* device);

/**
 * Close an audio device.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_audio, close_device)
wapi_result_t wapi_audio_close_device(wapi_handle_t device);

/**
 * Resume (unpause) an audio device. Devices start paused.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_audio, resume_device)
wapi_result_t wapi_audio_resume_device(wapi_handle_t device);

/**
 * Pause an audio device.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_audio, pause_device)
wapi_result_t wapi_audio_pause_device(wapi_handle_t device);

/* ============================================================
 * Audio Stream Functions
 * ============================================================
 * An audio stream handles format conversion between the module's
 * preferred format and the device's format. The module pushes
 * samples in its format; the stream converts and feeds the device.
 */

/**
 * Create an audio stream with optional format conversion.
 *
 * @param src_spec  Source format (what the module provides).
 * @param dst_spec  Destination format (what the device needs).
 *                  NULL = use device's native format.
 * @param stream    [out] Stream handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_audio, create_stream)
wapi_result_t wapi_audio_create_stream(const wapi_audio_spec_t* src_spec,
                                    const wapi_audio_spec_t* dst_spec,
                                    wapi_handle_t* stream);

/**
 * Destroy an audio stream.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_audio, destroy_stream)
wapi_result_t wapi_audio_destroy_stream(wapi_handle_t stream);

/**
 * Bind an audio stream to a device.
 * Playback: stream data flows to the device.
 * Recording: device data flows into the stream.
 *
 * @param device  Device handle.
 * @param stream  Stream handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_audio, bind_stream)
wapi_result_t wapi_audio_bind_stream(wapi_handle_t device, wapi_handle_t stream);

/**
 * Unbind an audio stream from its device.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_audio, unbind_stream)
wapi_result_t wapi_audio_unbind_stream(wapi_handle_t stream);

/**
 * Push audio data into a stream (for playback).
 * The data is in the stream's source format.
 *
 * @param stream  Stream handle.
 * @param buf     Audio sample data.
 * @param len     Data length in bytes.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_audio, put_stream_data)
wapi_result_t wapi_audio_put_stream_data(wapi_handle_t stream, const void* buf,
                                      wapi_size_t len);

/**
 * Pull audio data from a stream (for recording).
 *
 * @param stream     Stream handle.
 * @param buf        Buffer to receive audio data.
 * @param len        Buffer capacity in bytes.
 * @param bytes_read [out] Actual bytes read.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_audio, get_stream_data)
wapi_result_t wapi_audio_get_stream_data(wapi_handle_t stream, void* buf,
                                      wapi_size_t len, wapi_size_t* bytes_read);

/**
 * Query how many bytes of audio data are available in the stream.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_audio, stream_available)
int32_t wapi_audio_stream_available(wapi_handle_t stream);

/**
 * Query how many bytes the stream can still accept before it's full.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_audio, stream_queued)
int32_t wapi_audio_stream_queued(wapi_handle_t stream);

/* ============================================================
 * Convenience: Open Device + Create Stream + Bind
 * ============================================================ */

/**
 * Open a device and create a bound stream in one call.
 * Equivalent to open_device + create_stream + bind_stream.
 *
 * @param device_id  Device to open.
 * @param spec       Audio format the module will use.
 * @param device     [out] Device handle.
 * @param stream     [out] Stream handle (already bound to device).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_audio, open_device_stream)
wapi_result_t wapi_audio_open_device_stream(wapi_handle_t device_id,
                                         const wapi_audio_spec_t* spec,
                                         wapi_handle_t* device,
                                         wapi_handle_t* stream);

/* ============================================================
 * Device Enumeration
 * ============================================================ */

/**
 * Get the number of available playback devices.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_audio, playback_device_count)
int32_t wapi_audio_playback_device_count(void);

/**
 * Get the number of available recording devices.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_audio, recording_device_count)
int32_t wapi_audio_recording_device_count(void);

/**
 * Get the name of an audio device.
 *
 * @param device_id  Device index or handle.
 * @param buf        Buffer for device name (UTF-8).
 * @param buf_len    Buffer capacity.
 * @param name_len   [out] Actual name length.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_audio, device_name)
wapi_result_t wapi_audio_device_name(wapi_handle_t device_id, char* buf,
                                  wapi_size_t buf_len, wapi_size_t* name_len);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_AUDIO_H */
