/**
 * WAPI SDL Runtime - Audio (SDL3 SDL_Audio*)
 */

#include "wapi_host.h"

static SDL_AudioFormat wapi_audio_format_to_sdl(uint32_t fmt) {
    switch (fmt) {
    case 0x0008: return SDL_AUDIO_U8;
    case 0x8010: return SDL_AUDIO_S16LE;
    case 0x8020: return SDL_AUDIO_S32LE;
    case 0x8120: return SDL_AUDIO_F32LE;
    default:     return SDL_AUDIO_F32LE;
    }
}

/* wapi_audio_spec_t: u32 format, i32 channels, i32 freq (12 bytes). */
static bool read_audio_spec(uint32_t ptr, SDL_AudioSpec* out) {
    uint8_t* host = (uint8_t*)wapi_wasm_ptr(ptr, 12);
    if (!host) return false;
    uint32_t format;
    int32_t channels, freq;
    memcpy(&format,   host + 0, 4);
    memcpy(&channels, host + 4, 4);
    memcpy(&freq,     host + 8, 4);
    out->format = wapi_audio_format_to_sdl(format);
    out->channels = channels;
    out->freq = freq;
    return true;
}

static SDL_AudioDeviceID map_device_id(int32_t id) {
    if (id == -1) return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    if (id == -2) return SDL_AUDIO_DEVICE_DEFAULT_RECORDING;
    return (SDL_AudioDeviceID)id;
}

static int32_t host_open_device(wasm_exec_env_t env,
                                int32_t device_id, uint32_t spec_ptr,
                                uint32_t device_out_ptr) {
    (void)env;
    SDL_AudioSpec spec;
    SDL_AudioSpec* sp = NULL;
    if (spec_ptr != 0 && read_audio_spec(spec_ptr, &spec)) sp = &spec;
    SDL_AudioDeviceID opened = SDL_OpenAudioDevice(map_device_id(device_id), sp);
    if (opened == 0) { wapi_set_error(SDL_GetError()); return WAPI_ERR_UNKNOWN; }
    int32_t h = wapi_handle_alloc(WAPI_HTYPE_AUDIO_DEVICE);
    if (h == 0) { SDL_CloseAudioDevice(opened); return WAPI_ERR_NOMEM; }
    g_rt.handles[h].data.audio_device_id = opened;
    wapi_wasm_write_i32(device_out_ptr, h);
    return WAPI_OK;
}

static int32_t host_close_device(wasm_exec_env_t env, int32_t h) {
    (void)env;
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_DEVICE)) return WAPI_ERR_BADF;
    SDL_CloseAudioDevice(g_rt.handles[h].data.audio_device_id);
    wapi_handle_free(h);
    return WAPI_OK;
}
static int32_t host_resume_device(wasm_exec_env_t env, int32_t h) {
    (void)env;
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_DEVICE)) return WAPI_ERR_BADF;
    SDL_ResumeAudioDevice(g_rt.handles[h].data.audio_device_id);
    return WAPI_OK;
}
static int32_t host_pause_device(wasm_exec_env_t env, int32_t h) {
    (void)env;
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_DEVICE)) return WAPI_ERR_BADF;
    SDL_PauseAudioDevice(g_rt.handles[h].data.audio_device_id);
    return WAPI_OK;
}

static int32_t host_create_stream(wasm_exec_env_t env,
                                  uint32_t src_ptr, uint32_t dst_ptr,
                                  uint32_t out_ptr) {
    (void)env;
    SDL_AudioSpec src, dst;
    SDL_AudioSpec *sp = NULL, *dp = NULL;
    if (src_ptr && read_audio_spec(src_ptr, &src)) sp = &src;
    if (dst_ptr && read_audio_spec(dst_ptr, &dst)) dp = &dst;
    SDL_AudioStream* s = SDL_CreateAudioStream(sp, dp);
    if (!s) { wapi_set_error(SDL_GetError()); return WAPI_ERR_UNKNOWN; }
    int32_t h = wapi_handle_alloc(WAPI_HTYPE_AUDIO_STREAM);
    if (h == 0) { SDL_DestroyAudioStream(s); return WAPI_ERR_NOMEM; }
    g_rt.handles[h].data.audio_stream = s;
    wapi_wasm_write_i32(out_ptr, h);
    return WAPI_OK;
}

static int32_t host_destroy_stream(wasm_exec_env_t env, int32_t h) {
    (void)env;
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) return WAPI_ERR_BADF;
    SDL_DestroyAudioStream(g_rt.handles[h].data.audio_stream);
    wapi_handle_free(h);
    return WAPI_OK;
}

static int32_t host_bind_stream(wasm_exec_env_t env, int32_t dev, int32_t str) {
    (void)env;
    if (!wapi_handle_valid(dev, WAPI_HTYPE_AUDIO_DEVICE)) return WAPI_ERR_BADF;
    if (!wapi_handle_valid(str, WAPI_HTYPE_AUDIO_STREAM)) return WAPI_ERR_BADF;
    return SDL_BindAudioStream(g_rt.handles[dev].data.audio_device_id,
                               g_rt.handles[str].data.audio_stream)
           ? WAPI_OK : WAPI_ERR_UNKNOWN;
}

static int32_t host_unbind_stream(wasm_exec_env_t env, int32_t str) {
    (void)env;
    if (!wapi_handle_valid(str, WAPI_HTYPE_AUDIO_STREAM)) return WAPI_ERR_BADF;
    SDL_UnbindAudioStream(g_rt.handles[str].data.audio_stream);
    return WAPI_OK;
}

static int32_t host_put_stream_data(wasm_exec_env_t env,
                                    int32_t str, uint32_t buf_ptr, uint32_t len) {
    (void)env;
    if (!wapi_handle_valid(str, WAPI_HTYPE_AUDIO_STREAM)) return WAPI_ERR_BADF;
    void* data = wapi_wasm_ptr(buf_ptr, len);
    if (!data && len > 0) return WAPI_ERR_INVAL;
    return SDL_PutAudioStreamData(g_rt.handles[str].data.audio_stream,
                                  data, (int)len) ? WAPI_OK : WAPI_ERR_IO;
}

static int32_t host_get_stream_data(wasm_exec_env_t env,
                                    int32_t str, uint32_t buf_ptr,
                                    uint32_t len, uint32_t bytes_ptr) {
    (void)env;
    if (!wapi_handle_valid(str, WAPI_HTYPE_AUDIO_STREAM)) return WAPI_ERR_BADF;
    void* data = wapi_wasm_ptr(buf_ptr, len);
    if (!data && len > 0) return WAPI_ERR_INVAL;
    int got = SDL_GetAudioStreamData(g_rt.handles[str].data.audio_stream,
                                     data, (int)len);
    if (got < 0) { wapi_wasm_write_u32(bytes_ptr, 0); return WAPI_ERR_IO; }
    wapi_wasm_write_u32(bytes_ptr, (uint32_t)got);
    return WAPI_OK;
}

static int32_t host_stream_available(wasm_exec_env_t env, int32_t str) {
    (void)env;
    if (!wapi_handle_valid(str, WAPI_HTYPE_AUDIO_STREAM)) return 0;
    return SDL_GetAudioStreamAvailable(g_rt.handles[str].data.audio_stream);
}
static int32_t host_stream_queued(wasm_exec_env_t env, int32_t str) {
    (void)env;
    if (!wapi_handle_valid(str, WAPI_HTYPE_AUDIO_STREAM)) return 0;
    return SDL_GetAudioStreamQueued(g_rt.handles[str].data.audio_stream);
}

static int32_t host_open_device_stream(wasm_exec_env_t env,
                                       int32_t device_id, uint32_t spec_ptr,
                                       uint32_t dev_out, uint32_t str_out) {
    (void)env;
    SDL_AudioSpec spec;
    SDL_AudioSpec* sp = NULL;
    if (spec_ptr && read_audio_spec(spec_ptr, &spec)) sp = &spec;
    SDL_AudioStream* s = SDL_OpenAudioDeviceStream(map_device_id(device_id),
                                                    sp, NULL, NULL);
    if (!s) { wapi_set_error(SDL_GetError()); return WAPI_ERR_UNKNOWN; }
    SDL_AudioDeviceID opened = SDL_GetAudioStreamDevice(s);
    int32_t dh = wapi_handle_alloc(WAPI_HTYPE_AUDIO_DEVICE);
    int32_t sh = wapi_handle_alloc(WAPI_HTYPE_AUDIO_STREAM);
    if (dh == 0 || sh == 0) {
        SDL_DestroyAudioStream(s);
        if (dh) wapi_handle_free(dh);
        if (sh) wapi_handle_free(sh);
        return WAPI_ERR_NOMEM;
    }
    g_rt.handles[dh].data.audio_device_id = opened;
    g_rt.handles[sh].data.audio_stream = s;
    wapi_wasm_write_i32(dev_out, dh);
    wapi_wasm_write_i32(str_out, sh);
    return WAPI_OK;
}

static int32_t host_playback_device_count(wasm_exec_env_t env) {
    (void)env;
    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&count);
    if (ids) SDL_free(ids);
    return count;
}
static int32_t host_recording_device_count(wasm_exec_env_t env) {
    (void)env;
    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioRecordingDevices(&count);
    if (ids) SDL_free(ids);
    return count;
}

static int32_t host_device_name(wasm_exec_env_t env,
                                int32_t device_handle, uint32_t buf_ptr,
                                uint32_t buf_len, uint32_t name_len_ptr) {
    (void)env;
    SDL_AudioDeviceID id;
    if (wapi_handle_valid(device_handle, WAPI_HTYPE_AUDIO_DEVICE)) {
        id = g_rt.handles[device_handle].data.audio_device_id;
    } else {
        id = (SDL_AudioDeviceID)device_handle;
    }
    const char* name = SDL_GetAudioDeviceName(id);
    if (!name) return WAPI_ERR_NOENT;
    uint32_t len = (uint32_t)strlen(name);
    wapi_wasm_write_u32(name_len_ptr, len);
    uint32_t copy = len < buf_len ? len : buf_len;
    if (copy > 0) wapi_wasm_write_bytes(buf_ptr, name, copy);
    return WAPI_OK;
}

static NativeSymbol g_symbols[] = {
    { "open_device",            (void*)host_open_device,            "(iii)i",  NULL },
    { "close_device",           (void*)host_close_device,           "(i)i",    NULL },
    { "resume_device",          (void*)host_resume_device,          "(i)i",    NULL },
    { "pause_device",           (void*)host_pause_device,           "(i)i",    NULL },
    { "create_stream",          (void*)host_create_stream,          "(iii)i",  NULL },
    { "destroy_stream",         (void*)host_destroy_stream,         "(i)i",    NULL },
    { "bind_stream",            (void*)host_bind_stream,            "(ii)i",   NULL },
    { "unbind_stream",          (void*)host_unbind_stream,          "(i)i",    NULL },
    { "put_stream_data",        (void*)host_put_stream_data,        "(iii)i",  NULL },
    { "get_stream_data",        (void*)host_get_stream_data,        "(iiii)i", NULL },
    { "stream_available",       (void*)host_stream_available,       "(i)i",    NULL },
    { "stream_queued",          (void*)host_stream_queued,          "(i)i",    NULL },
    { "open_device_stream",     (void*)host_open_device_stream,     "(iiii)i", NULL },
    { "playback_device_count",  (void*)host_playback_device_count,  "()i",     NULL },
    { "recording_device_count", (void*)host_recording_device_count, "()i",     NULL },
    { "device_name",            (void*)host_device_name,            "(iiii)i", NULL },
};

wapi_cap_registration_t wapi_host_audio_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_audio",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
