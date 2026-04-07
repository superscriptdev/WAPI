/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Taskbar Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* set_progress: (i32, i32, f32) -> i32 */
static wasm_trap_t* cb_taskbar_set_progress(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_badge: (i32) -> i32 */
static wasm_trap_t* cb_taskbar_set_badge(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* request_attention: (i32, i32) -> i32 */
static wasm_trap_t* cb_taskbar_request_attention(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_overlay_icon: (i32, i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_taskbar_set_overlay_icon(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* clear_overlay: (i32) -> i32 */
static wasm_trap_t* cb_taskbar_clear_overlay(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_taskbar(wasmtime_linker_t* linker) {
    /* set_progress: i32, i32, f32 -> i32 */
    wapi_linker_define(linker, "wapi_taskbar", "set_progress", cb_taskbar_set_progress,
                     3, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_F32},
                     1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_1_1(linker, "wapi_taskbar", "set_badge",           cb_taskbar_set_badge);
    WAPI_DEFINE_2_1(linker, "wapi_taskbar", "request_attention",   cb_taskbar_request_attention);
    WAPI_DEFINE_5_1(linker, "wapi_taskbar", "set_overlay_icon",    cb_taskbar_set_overlay_icon);
    WAPI_DEFINE_1_1(linker, "wapi_taskbar", "clear_overlay",       cb_taskbar_clear_overlay);
}
