/**
 * WAPI Desktop Runtime — Taskbar / Dock (wapi_taskbar.h)
 *
 * Thin host wrapper over the platform ITaskbarList3 integration:
 *   - set_progress (state + 0..1 value)
 *   - request_attention (flash taskbar)
 *   - set_overlay_icon / clear_overlay
 *   - set_badge (Windows has no per-app badge API; NOSYS)
 */

#include "wapi_host.h"

static wasm_trap_t* h_set_progress(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t s   = WAPI_ARG_I32(0);
    int32_t st  = WAPI_ARG_I32(1);
    float   val = WAPI_ARG_F32(2);
    if (!wapi_handle_valid(s, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    bool ok = wapi_plat_taskbar_set_progress(g_rt.handles[s].data.window, st, val);
    WAPI_RET_I32(ok ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

static wasm_trap_t* h_set_badge(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    /* Windows: no direct per-app badge API in classic Win32; it's
     * typically emulated as an overlay icon. Guests that want a badge
     * should render text into an icon and call set_overlay_icon. */
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

static wasm_trap_t* h_request_attention(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t s    = WAPI_ARG_I32(0);
    int32_t crit = WAPI_ARG_I32(1);
    if (!wapi_handle_valid(s, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    bool ok = wapi_plat_taskbar_request_attention(g_rt.handles[s].data.window, crit != 0);
    WAPI_RET_I32(ok ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

/* set_overlay_icon (i32 surface, i32 icon_ptr, i64 icon_len, i32 desc_sv_ptr) -> i32 */
static wasm_trap_t* h_set_overlay_icon(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  s        = WAPI_ARG_I32(0);
    uint32_t icon_ptr = WAPI_ARG_U32(1);
    uint64_t icon_len = WAPI_ARG_U64(2);
    uint32_t sv_ptr   = WAPI_ARG_U32(3);
    if (!wapi_handle_valid(s, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }

    const void* icon = icon_len ? wapi_wasm_ptr(icon_ptr, (uint32_t)icon_len) : NULL;
    if (icon_len > 0 && !icon) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    uint64_t desc_data = 0, desc_len = 0;
    if (sv_ptr) {
        uint8_t* sv = (uint8_t*)wapi_wasm_ptr(sv_ptr, 16);
        if (!sv) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
        memcpy(&desc_data, sv + 0, 8);
        memcpy(&desc_len,  sv + 8, 8);
    }
    const char* desc = desc_len ? (const char*)wapi_wasm_ptr((uint32_t)desc_data, (uint32_t)desc_len) : NULL;
    if (desc_len > 0 && !desc) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    bool ok = wapi_plat_taskbar_set_overlay(g_rt.handles[s].data.window,
                                            icon, (size_t)icon_len,
                                            desc, (size_t)desc_len);
    WAPI_RET_I32(ok ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

static wasm_trap_t* h_clear_overlay(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t s = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(s, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    bool ok = wapi_plat_taskbar_clear_overlay(g_rt.handles[s].data.window);
    WAPI_RET_I32(ok ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

void wapi_host_register_taskbar(wasmtime_linker_t* linker) {
    /* set_progress: (i32 surface, i32 state, f32 value) -> i32 */
    wapi_linker_define(linker, "wapi_taskbar", "set_progress", h_set_progress,
        3, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_F32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, "wapi_taskbar", "set_badge",         h_set_badge);
    WAPI_DEFINE_2_1(linker, "wapi_taskbar", "request_attention", h_request_attention);
    /* set_overlay_icon: (i32 surface, i32 icon_ptr, i64 icon_len, i32 desc_sv) -> i32 */
    wapi_linker_define(linker, "wapi_taskbar", "set_overlay_icon", h_set_overlay_icon,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, "wapi_taskbar", "clear_overlay",     h_clear_overlay);
}
