/* Stub -- capability grant callbacks not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Capability Grant Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* query: (i32, i32, i32) -> i32 */
static wasm_trap_t* cb_cap_query(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* request: (i32, i32, i32) -> i32 */
static wasm_trap_t* cb_cap_request(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

/* Per wapi.h (§PART 5 Vtables) and the verified ABI baseline:
 * capability queries/grants flow through the wapi_io_t vtable
 * (cap_supported / cap_version / cap_query on the vtable, plus
 * WAPI_IO_OP_CAP_REQUEST submitted via wapi_io_bridge.submit).
 * There is no "wapi_cap" import module. */
void wapi_host_register_permissions(wasmtime_linker_t* linker) {
    (void)linker; (void)cb_cap_query; (void)cb_cap_request;
}
