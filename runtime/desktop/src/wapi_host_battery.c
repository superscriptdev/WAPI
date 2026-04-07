/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Battery Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* get_info: (i32 out_ptr) -> i32 */
static wasm_trap_t* cb_battery_get_info(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_battery(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_battery", "get_info", cb_battery_get_info);
}
