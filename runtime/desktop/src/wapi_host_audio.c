/**
 * WAPI Desktop Runtime - Audio
 *
 * Implements all wapi_audio.* imports using SDL3 audio.
 * Maps WAPI audio format enums to SDL3 audio format enums.
 */

#include "wapi_host.h"

/* ============================================================
 * Format Conversion
 * ============================================================ */

static SDL_AudioFormat wapi_audio_format_to_sdl(uint32_t wapi_fmt) {
    switch (wapi_fmt) {
    case 0x0008: return SDL_AUDIO_U8;      /* WAPI_AUDIO_U8 */
    case 0x8010: return SDL_AUDIO_S16LE;   /* WAPI_AUDIO_S16 */
    case 0x8020: return SDL_AUDIO_S32LE;   /* WAPI_AUDIO_S32 */
    case 0x8120: return SDL_AUDIO_F32LE;   /* WAPI_AUDIO_F32 */
    default:     return SDL_AUDIO_F32LE;
    }
}

/* Read a wapi_audio_spec_t (12 bytes) from wasm memory:
 *   +0: u32 format
 *   +4: i32 channels
 *   +8: i32 freq
 */
static bool read_audio_spec(uint32_t ptr, SDL_AudioSpec* out) {
    void* host = wapi_wasm_ptr(ptr, 12);
    if (!host) return false;

    uint32_t format;
    int32_t channels, freq;
    memcpy(&format,   (uint8_t*)host + 0, 4);
    memcpy(&channels, (uint8_t*)host + 4, 4);
    memcpy(&freq,     (uint8_t*)host + 8, 4);

    out->format = wapi_audio_format_to_sdl(format);
    out->channels = channels;
    out->freq = freq;
    return true;
}

/* ============================================================
 * Audio Device Functions
 * ============================================================ */

/* open_device: (i32 device_id, i32 spec_ptr, i32 device_out_ptr) -> i32 */
static wasm_trap_t* host_audio_open_device(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t device_id = WAPI_ARG_I32(0);
    uint32_t spec_ptr = WAPI_ARG_U32(1);
    uint32_t device_out_ptr = WAPI_ARG_U32(2);

    SDL_AudioDeviceID sdl_dev_id;
    if (device_id == -1) { /* WAPI_AUDIO_DEFAULT_PLAYBACK */
        sdl_dev_id = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    } else if (device_id == -2) { /* WAPI_AUDIO_DEFAULT_RECORDING */
        sdl_dev_id = SDL_AUDIO_DEVICE_DEFAULT_RECORDING;
    } else {
        sdl_dev_id = (SDL_AudioDeviceID)device_id;
    }

    /* Read spec if provided */
    SDL_AudioSpec spec;
    SDL_AudioSpec* spec_p = NULL;
    if (spec_ptr != 0) {
        if (read_audio_spec(spec_ptr, &spec)) {
            spec_p = &spec;
        }
    }

    /* SDL3: Open the device. In SDL3, SDL_OpenAudioDevice takes the device ID
     * and returns a new logical device ID. */
    SDL_AudioDeviceID opened = SDL_OpenAudioDevice(sdl_dev_id, spec_p);
    if (opened == 0) {
        wapi_set_error(SDL_GetError());
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }

    int32_t handle = wapi_handle_alloc(WAPI_HTYPE_AUDIO_DEVICE);
    if (handle == 0) {
        SDL_CloseAudioDevice(opened);
        wapi_set_error("Handle table full");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    g_rt.handles[handle].data.audio_device_id = opened;
    wapi_wasm_write_i32(device_out_ptr, handle);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* close_device: (i32 device) -> i32 */
static wasm_trap_t* host_audio_close_device(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_DEVICE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }
    SDL_CloseAudioDevice(g_rt.handles[h].data.audio_device_id);
    wapi_handle_free(h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* resume_device: (i32 device) -> i32 */
static wasm_trap_t* host_audio_resume_device(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_DEVICE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }
    SDL_ResumeAudioDevice(g_rt.handles[h].data.audio_device_id);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* pause_device: (i32 device) -> i32 */
static wasm_trap_t* host_audio_pause_device(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_DEVICE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }
    SDL_PauseAudioDevice(g_rt.handles[h].data.audio_device_id);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Audio Stream Functions
 * ============================================================ */

/* create_stream: (i32 src_spec_ptr, i32 dst_spec_ptr, i32 stream_out_ptr) -> i32 */
static wasm_trap_t* host_audio_create_stream(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t src_spec_ptr = WAPI_ARG_U32(0);
    uint32_t dst_spec_ptr = WAPI_ARG_U32(1);
    uint32_t stream_out_ptr = WAPI_ARG_U32(2);

    SDL_AudioSpec src_spec, dst_spec;
    SDL_AudioSpec* src_p = NULL;
    SDL_AudioSpec* dst_p = NULL;

    if (src_spec_ptr != 0 && read_audio_spec(src_spec_ptr, &src_spec))
        src_p = &src_spec;
    if (dst_spec_ptr != 0 && read_audio_spec(dst_spec_ptr, &dst_spec))
        dst_p = &dst_spec;

    SDL_AudioStream* stream = SDL_CreateAudioStream(src_p, dst_p);
    if (!stream) {
        wapi_set_error(SDL_GetError());
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }

    int32_t handle = wapi_handle_alloc(WAPI_HTYPE_AUDIO_STREAM);
    if (handle == 0) {
        SDL_DestroyAudioStream(stream);
        wapi_set_error("Handle table full");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    g_rt.handles[handle].data.audio_stream = stream;
    wapi_wasm_write_i32(stream_out_ptr, handle);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* destroy_stream: (i32 stream) -> i32 */
static wasm_trap_t* host_audio_destroy_stream(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }
    SDL_DestroyAudioStream(g_rt.handles[h].data.audio_stream);
    wapi_handle_free(h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* bind_stream: (i32 device, i32 stream) -> i32 */
static wasm_trap_t* host_audio_bind_stream(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t dev_h = WAPI_ARG_I32(0);
    int32_t str_h = WAPI_ARG_I32(1);

    if (!wapi_handle_valid(dev_h, WAPI_HTYPE_AUDIO_DEVICE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }
    if (!wapi_handle_valid(str_h, WAPI_HTYPE_AUDIO_STREAM)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    SDL_AudioDeviceID dev = g_rt.handles[dev_h].data.audio_device_id;
    SDL_AudioStream* stream = g_rt.handles[str_h].data.audio_stream;

    if (!SDL_BindAudioStream(dev, stream)) {
        wapi_set_error(SDL_GetError());
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* unbind_stream: (i32 stream) -> i32 */
static wasm_trap_t* host_audio_unbind_stream(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }
    SDL_UnbindAudioStream(g_rt.handles[h].data.audio_stream);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* put_stream_data: (i32 stream, i32 buf_ptr, i32 len) -> i32 */
static wasm_trap_t* host_audio_put_stream_data(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t len = WAPI_ARG_U32(2);

    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    void* data = wapi_wasm_ptr(buf_ptr, len);
    if (!data && len > 0) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    if (!SDL_PutAudioStreamData(g_rt.handles[h].data.audio_stream, data, (int)len)) {
        wapi_set_error(SDL_GetError());
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* get_stream_data: (i32 stream, i32 buf_ptr, i32 len, i32 bytes_read_ptr) -> i32 */
static wasm_trap_t* host_audio_get_stream_data(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t len = WAPI_ARG_U32(2);
    uint32_t bytes_read_ptr = WAPI_ARG_U32(3);

    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    void* data = wapi_wasm_ptr(buf_ptr, len);
    if (!data && len > 0) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    int got = SDL_GetAudioStreamData(g_rt.handles[h].data.audio_stream, data, (int)len);
    if (got < 0) {
        wapi_set_error(SDL_GetError());
        wapi_wasm_write_u32(bytes_read_ptr, 0);
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    wapi_wasm_write_u32(bytes_read_ptr, (uint32_t)got);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* stream_available: (i32 stream) -> i32 */
static wasm_trap_t* host_audio_stream_available(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) {
        WAPI_RET_I32(0);
        return NULL;
    }
    int available = SDL_GetAudioStreamAvailable(g_rt.handles[h].data.audio_stream);
    WAPI_RET_I32(available);
    return NULL;
}

/* stream_queued: (i32 stream) -> i32 */
static wasm_trap_t* host_audio_stream_queued(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) {
        WAPI_RET_I32(0);
        return NULL;
    }
    int queued = SDL_GetAudioStreamQueued(g_rt.handles[h].data.audio_stream);
    WAPI_RET_I32(queued);
    return NULL;
}

/* open_device_stream: (i32 device_id, i32 spec_ptr, i32 dev_out, i32 stream_out) -> i32 */
static wasm_trap_t* host_audio_open_device_stream(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t device_id = WAPI_ARG_I32(0);
    uint32_t spec_ptr = WAPI_ARG_U32(1);
    uint32_t dev_out_ptr = WAPI_ARG_U32(2);
    uint32_t stream_out_ptr = WAPI_ARG_U32(3);

    SDL_AudioDeviceID sdl_dev_id;
    if (device_id == -1) {
        sdl_dev_id = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    } else if (device_id == -2) {
        sdl_dev_id = SDL_AUDIO_DEVICE_DEFAULT_RECORDING;
    } else {
        sdl_dev_id = (SDL_AudioDeviceID)device_id;
    }

    SDL_AudioSpec spec;
    SDL_AudioSpec* spec_p = NULL;
    if (spec_ptr != 0 && read_audio_spec(spec_ptr, &spec))
        spec_p = &spec;

    /* SDL3: SDL_OpenAudioDeviceStream creates device + stream in one call */
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(sdl_dev_id, spec_p, NULL, NULL);
    if (!stream) {
        wapi_set_error(SDL_GetError());
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }

    /* Get the device from the stream */
    /* In SDL3, the stream is bound to the device internally.
     * We need to track both. The device can be retrieved. */
    SDL_AudioDeviceID opened = SDL_GetAudioStreamDevice(stream);

    int32_t dev_handle = wapi_handle_alloc(WAPI_HTYPE_AUDIO_DEVICE);
    int32_t stream_handle = wapi_handle_alloc(WAPI_HTYPE_AUDIO_STREAM);
    if (dev_handle == 0 || stream_handle == 0) {
        SDL_DestroyAudioStream(stream);
        if (dev_handle) wapi_handle_free(dev_handle);
        if (stream_handle) wapi_handle_free(stream_handle);
        wapi_set_error("Handle table full");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    g_rt.handles[dev_handle].data.audio_device_id = opened;
    g_rt.handles[stream_handle].data.audio_stream = stream;

    wapi_wasm_write_i32(dev_out_ptr, dev_handle);
    wapi_wasm_write_i32(stream_out_ptr, stream_handle);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Device Enumeration
 * ============================================================ */

/* playback_device_count: () -> i32 */
static wasm_trap_t* host_audio_playback_device_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    int count = 0;
    SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
    SDL_free(devices);
    WAPI_RET_I32(count);
    return NULL;
}

/* recording_device_count: () -> i32 */
static wasm_trap_t* host_audio_recording_device_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    int count = 0;
    SDL_AudioDeviceID* devices = SDL_GetAudioRecordingDevices(&count);
    SDL_free(devices);
    WAPI_RET_I32(count);
    return NULL;
}

/* device_name: (i32 device_id, i32 buf, i32 buf_len, i32 name_len_ptr) -> i32 */
static wasm_trap_t* host_audio_device_name(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t device_handle = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t buf_len = WAPI_ARG_U32(2);
    uint32_t name_len_ptr = WAPI_ARG_U32(3);

    SDL_AudioDeviceID dev_id;
    if (wapi_handle_valid(device_handle, WAPI_HTYPE_AUDIO_DEVICE)) {
        dev_id = g_rt.handles[device_handle].data.audio_device_id;
    } else {
        dev_id = (SDL_AudioDeviceID)device_handle;
    }

    const char* name = SDL_GetAudioDeviceName(dev_id);
    if (!name) {
        wapi_set_error("Unknown audio device");
        WAPI_RET_I32(WAPI_ERR_NOENT);
        return NULL;
    }

    uint32_t len = (uint32_t)strlen(name);
    wapi_wasm_write_u32(name_len_ptr, len);

    uint32_t copy_len = len < buf_len ? len : buf_len;
    if (copy_len > 0) {
        wapi_wasm_write_bytes(buf_ptr, name, copy_len);
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_audio(wasmtime_linker_t* linker) {
    WAPI_DEFINE_3_1(linker, "wapi_audio", "open_device",           host_audio_open_device);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "close_device",          host_audio_close_device);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "resume_device",         host_audio_resume_device);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "pause_device",          host_audio_pause_device);
    WAPI_DEFINE_3_1(linker, "wapi_audio", "create_stream",         host_audio_create_stream);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "destroy_stream",        host_audio_destroy_stream);
    WAPI_DEFINE_2_1(linker, "wapi_audio", "bind_stream",           host_audio_bind_stream);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "unbind_stream",         host_audio_unbind_stream);
    WAPI_DEFINE_3_1(linker, "wapi_audio", "put_stream_data",       host_audio_put_stream_data);
    WAPI_DEFINE_4_1(linker, "wapi_audio", "get_stream_data",       host_audio_get_stream_data);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "stream_available",      host_audio_stream_available);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "stream_queued",         host_audio_stream_queued);
    WAPI_DEFINE_4_1(linker, "wapi_audio", "open_device_stream",    host_audio_open_device_stream);
    WAPI_DEFINE_0_1(linker, "wapi_audio", "playback_device_count", host_audio_playback_device_count);
    WAPI_DEFINE_0_1(linker, "wapi_audio", "recording_device_count",host_audio_recording_device_count);
    WAPI_DEFINE_4_1(linker, "wapi_audio", "device_name",           host_audio_device_name);
}
