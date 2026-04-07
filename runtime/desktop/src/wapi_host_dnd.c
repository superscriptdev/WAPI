/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Drag and Drop Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* start_drag: (i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_dnd_start_drag(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_drop_effect: (i32) -> i32 */
static wasm_trap_t* cb_dnd_set_drop_effect(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_drop_data: (i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_dnd_get_drop_data(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_dnd(wasmtime_linker_t* linker) {
    WAPI_DEFINE_4_1(linker, "wapi_dnd", "start_drag",       cb_dnd_start_drag);
    WAPI_DEFINE_1_1(linker, "wapi_dnd", "set_drop_effect",  cb_dnd_set_drop_effect);
    WAPI_DEFINE_4_1(linker, "wapi_dnd", "get_drop_data",    cb_dnd_get_drop_data);
}
