/* Stub -- not yet implemented for desktop.
 *
 * Grant acquisition flows through the universal WAPI_IO_OP_CAP_REQUEST
 * path; no per-module perm imports live here. */

#include "wapi_host.h"

/* ============================================================
 * Notification Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* show: (i32 desc_ptr, i32 out_id) -> i32 */
static wasm_trap_t* cb_notify_show(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* close: (i32 id) -> i32 */
static wasm_trap_t* cb_notify_close(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_notifications(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_notify", "show",  cb_notify_show);
    WAPI_DEFINE_1_1(linker, "wapi_notify", "close", cb_notify_close);
}
