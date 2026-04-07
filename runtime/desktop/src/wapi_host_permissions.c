/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Permissions Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* query: (i32, i32, i32) -> i32 */
static wasm_trap_t* cb_perm_query(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* request: (i32, i32, i32) -> i32 */
static wasm_trap_t* cb_perm_request(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_permissions(wasmtime_linker_t* linker) {
    WAPI_DEFINE_3_1(linker, "wapi_perm", "query",   cb_perm_query);
    WAPI_DEFINE_3_1(linker, "wapi_perm", "request", cb_perm_request);
}
