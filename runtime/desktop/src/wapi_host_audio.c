/**
 * WAPI Desktop Runtime - Audio
 *
 * Endpoints are acquired through the role system (see wapi_host_io.c
 * role dispatcher). This file owns the audio stream lifecycle, the
 * endpoint control surface (close/pause/resume) on granted handles,
 * and the endpoint_info query.
 */

#include "wapi_host.h"

bool wapi_host_audio_read_spec(uint32_t ptr, wapi_plat_audio_spec_t* out) {
    /* wapi_audio_spec_t wasm32 layout: +0 u32 format, +4 i32 channels, +8 i32 freq */
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

/* ============================================================
 * Endpoint metadata + control (operate on granted handle)
 * ============================================================ */

/* endpoint_info: (i32 handle, i32 info_out, i32 name_buf, i64 name_buf_len, i32 name_len_out) -> i32
 *
 * Writes wapi_audio_endpoint_info_t (32 bytes, layout: native_spec(12)
 * + form(4) + uid[16]) into info_out. Copies up to name_buf_len bytes
 * of the endpoint's UTF-8 display label into name_buf; writes the
 * actual length into *name_len_out. */
static wasm_trap_t* host_audio_endpoint_info(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h         = WAPI_ARG_I32(0);
    uint32_t info_out  = WAPI_ARG_U32(1);
    uint32_t name_buf  = WAPI_ARG_U32(2);
    uint64_t name_cap  = WAPI_ARG_U64(3);
    uint32_t name_len_out = WAPI_ARG_U32(4);

    if (!wapi_handle_valid(h, WAPI_HTYPE_AUDIO_DEVICE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }

    wapi_plat_audio_device_t* d = g_rt.handles[h].data.audio_device;
    wapi_plat_audio_spec_t native = {0};
    uint32_t form = 0; /* WAPI_AUDIO_FORM_UNKNOWN */
    uint8_t  uid[16] = {0};
    char     name[128] = {0};
    size_t   name_len = 0;

    wapi_plat_audio_device_describe(d, &native, &form, uid, name, sizeof(name), &name_len);

    /* Write wapi_audio_endpoint_info_t (32 bytes). */
    if (info_out) {
        uint8_t info[32];
        memcpy(info +  0, &native.format,   4);
        memcpy(info +  4, &native.channels, 4);
        memcpy(info +  8, &native.freq,     4);
        memcpy(info + 12, &form,            4);
        memcpy(info + 16, uid,             16);
        wapi_wasm_write_bytes(info_out, info, 32);
    }
    if (name_buf && name_cap > 0 && name_len > 0) {
        uint32_t copy = (uint32_t)(name_len < name_cap ? name_len : name_cap);
        wapi_wasm_write_bytes(name_buf, name, copy);
    }
    if (name_len_out) wapi_wasm_write_u64(name_len_out, (uint64_t)name_len);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_audio_close(void* env, wasmtime_caller_t* caller,
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

HOST_AUDIO_DEV_CMD(resume, wapi_plat_audio_resume_device)
HOST_AUDIO_DEV_CMD(pause,  wapi_plat_audio_pause_device)

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
    if (src_ptr != 0 && wapi_host_audio_read_spec(src_ptr, &src)) sp = &src;
    if (dst_ptr != 0 && wapi_host_audio_read_spec(dst_ptr, &dst)) dp = &dst;

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

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_audio(wasmtime_linker_t* linker) {
    wapi_linker_define(linker, "wapi_audio", "endpoint_info", host_audio_endpoint_info,
        5, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, "wapi_audio", "close",           host_audio_close);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "resume",          host_audio_resume);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "pause",           host_audio_pause);
    WAPI_DEFINE_3_1(linker, "wapi_audio", "create_stream",   host_audio_create_stream);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "destroy_stream",  host_audio_destroy_stream);
    WAPI_DEFINE_2_1(linker, "wapi_audio", "bind_stream",     host_audio_bind_stream);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "unbind_stream",   host_audio_unbind_stream);
    wapi_linker_define(linker, "wapi_audio", "put_stream_data", host_audio_put_stream_data,
        3, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, "wapi_audio", "get_stream_data", host_audio_get_stream_data,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, "wapi_audio", "stream_available", host_audio_stream_available);
    WAPI_DEFINE_1_1(linker, "wapi_audio", "stream_queued",    host_audio_stream_queued);
}
