/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Wake Lock Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* acquire: (i32, i32) -> i32 */
static wasm_trap_t* cb_wake_acquire(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* release: (i32) -> i32 */
static wasm_trap_t* cb_wake_release(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_wake_lock(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_wake", "acquire", cb_wake_acquire);
    WAPI_DEFINE_1_1(linker, "wapi_wake", "release", cb_wake_release);
}
