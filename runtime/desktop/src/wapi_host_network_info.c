/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Network Info Callbacks
 * ============================================================ */

/* get_info: (i32 out_ptr) -> i32 */
static wasm_trap_t* cb_netinfo_get_info(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* is_online: () -> i32   (returns 1 -- desktop is likely online) */
static wasm_trap_t* cb_netinfo_is_online(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(1);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_network_info(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_netinfo", "get_info",  cb_netinfo_get_info);
    WAPI_DEFINE_0_1(linker, "wapi_netinfo", "is_online", cb_netinfo_is_online);
}
