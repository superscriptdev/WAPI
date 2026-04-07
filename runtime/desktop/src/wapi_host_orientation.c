/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Orientation Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* get: (i32) -> i32 */
static wasm_trap_t* cb_orient_get(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* lock: (i32) -> i32 */
static wasm_trap_t* cb_orient_lock(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* unlock: () -> i32 */
static wasm_trap_t* cb_orient_unlock(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_orientation(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_orient", "get",    cb_orient_get);
    WAPI_DEFINE_1_1(linker, "wapi_orient", "lock",   cb_orient_lock);
    WAPI_DEFINE_0_1(linker, "wapi_orient", "unlock", cb_orient_unlock);
}
