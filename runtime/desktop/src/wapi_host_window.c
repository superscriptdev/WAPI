/**
 * WAPI Desktop Runtime - Window Management
 *
 * "wapi_window" module — per wapi_window.h:
 *   set_title, get_size_logical, set_fullscreen, set_visible,
 *   minimize, maximize, restore.
 *
 * All take a WAPI surface handle. Cursor is a per-mouse-device
 * property and lives in wapi_host_input.c (mouse_set_cursor).
 *
 * Every function returns WAPI_ERR_NOTSUP on offscreen surfaces;
 * a surface without a chained wapi_window_desc_t is still backed
 * by wapi_plat_window_t* in the current runtime, so this is a
 * soft no-op rather than a hard error.
 */

#include "wapi_host.h"

/* Read wapi_stringview_t (16B: {u64 data, u64 length}) at sv_ptr
 * into a NUL-terminated buffer. Returns bytes written (not counting
 * NUL), or -1 on invalid pointer. */
static int32_t read_stringview(uint32_t sv_ptr, char* buf, size_t buf_cap) {
    if (buf_cap == 0) return -1;
    void* sv = wapi_wasm_ptr(sv_ptr, 16);
    if (!sv) return -1;

    uint64_t data, length;
    memcpy(&data,   (uint8_t*)sv + 0, 8);
    memcpy(&length, (uint8_t*)sv + 8, 8);

    if (data == 0) { buf[0] = '\0'; return 0; }

    size_t copy;
    if (length == UINT64_MAX) {
        const char* s = (const char*)wapi_wasm_ptr((uint32_t)data, 1);
        if (!s) return -1;
        copy = strnlen(s, buf_cap - 1);
        memcpy(buf, s, copy);
    } else {
        copy = (size_t)length < buf_cap - 1 ? (size_t)length : buf_cap - 1;
        const char* s = (const char*)wapi_wasm_ptr((uint32_t)data, (uint32_t)copy);
        if (!s && copy > 0) return -1;
        if (s) memcpy(buf, s, copy);
    }
    buf[copy] = '\0';
    return (int32_t)copy;
}

static wasm_trap_t* host_window_set_title(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h      = WAPI_ARG_I32(0);
    uint32_t sv_ptr = WAPI_ARG_U32(1);
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }

    char buf[256];
    int32_t len = read_stringview(sv_ptr, buf, sizeof(buf));
    if (len < 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    wapi_plat_window_set_title(g_rt.handles[h].data.window, buf, (uint32_t)len);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_window_get_size_logical(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    uint32_t w_ptr = WAPI_ARG_U32(1), h_ptr = WAPI_ARG_U32(2);
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    int32_t lw, lh;
    wapi_plat_window_get_size_logical(g_rt.handles[h].data.window, &lw, &lh);
    wapi_wasm_write_i32(w_ptr, lw);
    wapi_wasm_write_i32(h_ptr, lh);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_window_set_fullscreen(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h  = WAPI_ARG_I32(0);
    int32_t fs = WAPI_ARG_I32(1);
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_window_set_fullscreen(g_rt.handles[h].data.window, fs != 0);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_window_set_visible(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    int32_t v = WAPI_ARG_I32(1);
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_window_set_visible(g_rt.handles[h].data.window, v != 0);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

#define HOST_WINDOW_OP(NAME, PLAT_CALL) \
    static wasm_trap_t* host_window_##NAME(void* env, wasmtime_caller_t* caller, \
        const wasmtime_val_t* args, size_t nargs, \
        wasmtime_val_t* results, size_t nresults) { \
        (void)env; (void)caller; (void)nargs; (void)nresults; \
        int32_t h = WAPI_ARG_I32(0); \
        if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; } \
        PLAT_CALL(g_rt.handles[h].data.window); \
        WAPI_RET_I32(WAPI_OK); \
        return NULL; \
    }

HOST_WINDOW_OP(minimize, wapi_plat_window_minimize)
HOST_WINDOW_OP(maximize, wapi_plat_window_maximize)
HOST_WINDOW_OP(restore,  wapi_plat_window_restore)

void wapi_host_register_window(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_window", "set_title",        host_window_set_title);
    WAPI_DEFINE_3_1(linker, "wapi_window", "get_size_logical", host_window_get_size_logical);
    WAPI_DEFINE_2_1(linker, "wapi_window", "set_fullscreen",   host_window_set_fullscreen);
    WAPI_DEFINE_2_1(linker, "wapi_window", "set_visible",      host_window_set_visible);
    WAPI_DEFINE_1_1(linker, "wapi_window", "minimize",         host_window_minimize);
    WAPI_DEFINE_1_1(linker, "wapi_window", "maximize",         host_window_maximize);
    WAPI_DEFINE_1_1(linker, "wapi_window", "restore",          host_window_restore);
}
