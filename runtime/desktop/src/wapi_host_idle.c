/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Idle Detection Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* start: (i32 threshold_ms) -> i32 */
static wasm_trap_t* cb_idle_start(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* stop: () -> i32 */
static wasm_trap_t* cb_idle_stop(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_state: (i32 out_ptr) -> i32 */
static wasm_trap_t* cb_idle_get_state(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_idle(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_idle", "start",     cb_idle_start);
    WAPI_DEFINE_0_1(linker, "wapi_idle", "stop",      cb_idle_stop);
    WAPI_DEFINE_1_1(linker, "wapi_idle", "get_state", cb_idle_get_state);
}
