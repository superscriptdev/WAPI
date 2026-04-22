/**
 * WAPI Desktop Runtime - Audio
 *
 * Thin wrapper over wapi_plat audio. Platform backend owns the
 * format conversion / device lifecycle / worker thread.
 */

#include "wapi_host.h"

static bool read_audio_spec(uint32_t ptr, wapi_plat_audio_spec_t* out) {
    /* wapi_audio_spec_t wasm32 layout:
     *   +0 u32 format, +4 i32 channels, +8 i32 freq */
    void* host = wapi_wasm_ptr(ptr, 12);
    if (!host) return false;
    uint32_t format; int32_t channels, freq;
    memcpy(&format,   (uint8_t*)host + 0, 4);
    memcpy(&channels, (uint8_t*)host + 4, 4);
    memcpy(&freq,     (uint8_t*)host + 8, 4);
    out->format   = format;
    out->channels = channels;
    out->freq     = freq;
    out->_pad     = 0;
    return true;
}

static int normalize_device_id(int32_t id) {
    if (id == -1) return WAPI_PLAT_AUDIO_DEFAULT_PLAYBACK;
    if (id == -2) return WAPI_PLAT_AUDIO_DEFAULT_RECORDING;
    return id;
}

/* ============================================================
 * Devices
 * ============================================================ */

static wasm_trap_t* host_audio_open_device(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t id       = WAPI_ARG_I32(0);
    uint32_t spec_ptr = WAPI_ARG_U32(1);
    uint32_t dev_out  = WAPI_ARG_U32(2);

    wapi_plat_audio_spec_t spec = {0};
    wapi_plat_audio_spec_t* sp = NULL;
    if (spec_ptr != 0 && read_audio_spec(spec_ptr, &spec)) sp = &spec;

    wapi_plat_audio_device_t* d = wapi_plat_audio_open_device(normalize_device_id(id), sp);
    if (!d) { wapi_set_error("audio open failed"); WAPI_RET_I32(WAPI_ERR_UNKNOWN); return NULL; }

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_AUDIO_DEVICE);
    if (h == 0) { wapi_plat_audio_close_device(d); WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }
    g_rt.handles[h].data.audio_device = d;
    wapi_wasm_write_i32(dev_out, h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_audio_close_device(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_DEVICE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_audio_close_device(g_rt.handles[h].data.audio_device);
    wapi_handle_free(h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

#define HOST_AUDIO_DEV_CMD(NAME, CALL) \
    static wasm_trap_t* host_audio_##NAME(void* env, wasmtime_caller_t* caller, \
        const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults) { \
        (void)env; (void)caller; (void)nargs; (void)nresults; \
        int32_t h = WAPI_ARG_I32(0); \
        if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_DEVICE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; } \
        CALL(g_rt.handles[h].data.audio_device); \
        WAPI_RET_I32(WAPI_OK); \
        return NULL; \
    }

HOST_AUDIO_DEV_CMD(resume_device, wapi_plat_audio_resume_device)
HOST_AUDIO_DEV_CMD(pause_device,  wapi_plat_audio_pause_device)

/* ============================================================
 * Streams
 * ============================================================ */

static wasm_trap_t* host_audio_create_stream(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t src_ptr = WAPI_ARG_U32(0);
    uint32_t dst_ptr = WAPI_ARG_U32(1);
    uint32_t out     = WAPI_ARG_U32(2);

    wapi_plat_audio_spec_t src, dst;
    wapi_plat_audio_spec_t* sp = NULL;
    wapi_plat_audio_spec_t* dp = NULL;
    if (src_ptr != 0 && read_audio_spec(src_ptr, &src)) sp = &src;
    if (dst_ptr != 0 && read_audio_spec(dst_ptr, &dst)) dp = &dst;

    wapi_plat_audio_stream_t* s = wapi_plat_audio_stream_create(sp, dp);
    if (!s) { WAPI_RET_I32(WAPI_ERR_UNKNOWN); return NULL; }

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_AUDIO_STREAM);
    if (h == 0) { wapi_plat_audio_stream_destroy(s); WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }
    g_rt.handles[h].data.audio_stream = s;
    wapi_wasm_write_i32(out, h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_audio_destroy_stream(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_audio_stream_destroy(g_rt.handles[h].data.audio_stream);
    wapi_handle_free(h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_audio_bind_stream(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t dh = WAPI_ARG_I32(0);
    int32_t sh = WAPI_ARG_I32(1);
    if (!wapi_handle_valid(dh, WAPI_HTYPE_AUDIO_DEVICE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    if (!wapi_handle_valid(sh, WAPI_HTYPE_AUDIO_STREAM)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    if (!wapi_plat_audio_stream_bind(g_rt.handles[dh].data.audio_device,
                                     g_rt.handles[sh].data.audio_stream)) {
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_audio_unbind_stream(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_audio_stream_unbind(g_rt.handles[h].data.audio_stream);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* put_stream_data: (i32 stream, i32 buf, i64 len) -> i32 */
static wasm_trap_t* host_audio_put_stream_data(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h       = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint64_t len     = WAPI_ARG_U64(2);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    void* data = wapi_wasm_ptr(buf_ptr, (uint32_t)len);
    if (!data && len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    if (!wapi_plat_audio_stream_put(g_rt.handles[h].data.audio_stream, data, (int)len)) {
        WAPI_RET_I32(WAPI_ERR_IO); return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* get_stream_data: (i32 stream, i32 buf, i64 len, i32 bytes_read_out) -> i32 */
static wasm_trap_t* host_audio_get_stream_data(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h       = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint64_t len     = WAPI_ARG_U64(2);
    uint32_t out_ptr = WAPI_ARG_U32(3);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    void* data = wapi_wasm_ptr(buf_ptr, (uint32_t)len);
    if (!data && len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    int got = wapi_plat_audio_stream_get(g_rt.handles[h].data.audio_stream, data, (int)len);
    wapi_wasm_write_u64(out_ptr, (uint64_t)(got < 0 ? 0 : got));
    WAPI_RET_I32(got < 0 ? WAPI_ERR_IO : WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_audio_stream_available(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) { WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(wapi_plat_audio_stream_available(g_rt.handles[h].data.audio_stream));
    return NULL;
}

static wasm_trap_t* host_audio_stream_queued(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_STREAM)) { WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(wapi_plat_audio_stream_queued(g_rt.handles[h].data.audio_stream));
    return NULL;
}

static wasm_trap_t* host_audio_open_device_stream(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t id       = WAPI_ARG_I32(0);
    uint32_t spec_ptr = WAPI_ARG_U32(1);
    uint32_t dev_out  = WAPI_ARG_U32(2);
    uint32_t str_out  = WAPI_ARG_U32(3);

    wapi_plat_audio_spec_t spec = {0};
    wapi_plat_audio_spec_t* sp = NULL;
    if (spec_ptr != 0 && read_audio_spec(spec_ptr, &spec)) sp = &spec;

    wapi_plat_audio_device_t* d = NULL;
    wapi_plat_audio_stream_t* s = NULL;
    if (!wapi_plat_audio_open_device_stream(normalize_device_id(id), sp, &d, &s)) {
        WAPI_RET_I32(WAPI_ERR_UNKNOWN); return NULL;
    }

    int32_t dh = wapi_handle_alloc(WAPI_HTYPE_AUDIO_DEVICE);
    int32_t sh = wapi_handle_alloc(WAPI_HTYPE_AUDIO_STREAM);
    if (dh == 0 || sh == 0) {
        wapi_plat_audio_stream_destroy(s);
        wapi_plat_audio_close_device(d);
        if (dh) wapi_handle_free(dh);
        if (sh) wapi_handle_free(sh);
        WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL;
    }
    g_rt.handles[dh].data.audio_device = d;
    g_rt.handles[sh].data.audio_stream = s;
    wapi_wasm_write_i32(dev_out, dh);
    wapi_wasm_write_i32(str_out, sh);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Enumeration
 * ============================================================ */

static wasm_trap_t* host_audio_playback_device_count(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(wapi_plat_audio_playback_device_count());
    return NULL;
}

static wasm_trap_t* host_audio_recording_device_count(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(wapi_plat_audio_recording_device_count());
    return NULL;
}

static wasm_trap_t* host_audio_device_name(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    /* device_name: (i32 id, i32 buf, i64 buf_len, i32 name_len_out) -> i32 */
    int32_t  id      = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint64_t buf_len = WAPI_ARG_U64(2);
    uint32_t len_ptr = WAPI_ARG_U32(3);

    int resolved = id;
    if (wapi_handle_valid(id, WAPI_HTYPE_AUDIO_DEVICE)) {
        /* Backend takes device_id, not our handle — pass default as a
         * stable lookup. In practice the guest passes enumeration
         * indices or sentinels, not handles. */
        resolved = WAPI_PLAT_AUDIO_DEFAULT_PLAYBACK;
    }

    char tmp[256];
    size_t nbytes = wapi_plat_audio_device_name(resolved, tmp, sizeof(tmp));
    wapi_wasm_write_u64(len_ptr, (uint64_t)nbytes);
    if (nbytes > 0 && buf_len > 0) {
        uint32_t copy = (uint32_t)(nbytes < buf_len ? nbytes : buf_len);
        wapi_wasm_write_bytes(buf_ptr, tmp, copy);
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_audio(wasmtime_linker_t* linker) {
    WAPI_DEFINE_3_1(linker, "wapi_audio", "open_device",            host_audio_open_device);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "close_device",           host_audio_close_device);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "resume_device",          host_audio_resume_device);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "pause_device",           host_audio_pause_device);
    WAPI_DEFINE_3_1(linker, "wapi_audio", "create_stream",          host_audio_create_stream);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "destroy_stream",         host_audio_destroy_stream);
    WAPI_DEFINE_2_1(linker, "wapi_audio", "bind_stream",            host_audio_bind_stream);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "unbind_stream",          host_audio_unbind_stream);
    wapi_linker_define(linker, "wapi_audio", "put_stream_data", host_audio_put_stream_data,
        3, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, "wapi_audio", "get_stream_data", host_audio_get_stream_data,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, "wapi_audio", "stream_available",       host_audio_stream_available);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "stream_queued",          host_audio_stream_queued);
    WAPI_DEFINE_4_1(linker, "wapi_audio", "open_device_stream",     host_audio_open_device_stream);
    WAPI_DEFINE_0_1(linker, "wapi_audio", "playback_device_count",  host_audio_playback_device_count);
    WAPI_DEFINE_0_1(linker, "wapi_audio", "recording_device_count", host_audio_recording_device_count);
    wapi_linker_define(linker, "wapi_audio", "device_name", host_audio_device_name,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
}
