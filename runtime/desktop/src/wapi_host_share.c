/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Share Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* can_share: () -> i32 */
static wasm_trap_t* cb_share_can_share(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* false -- sharing not supported */
    return NULL;
}

/* share: (i32 data_ptr) -> i32 */
static wasm_trap_t* cb_share_share(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_share(wasmtime_linker_t* linker) {
    WAPI_DEFINE_0_1(linker, "wapi_share", "can_share", cb_share_can_share);
    WAPI_DEFINE_1_1(linker, "wapi_share", "share",     cb_share_share);
}
