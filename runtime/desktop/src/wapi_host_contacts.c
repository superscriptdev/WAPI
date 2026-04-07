/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Contacts Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* pick: (i32 properties, i32 count, i32 out_buf, i32 out_len) -> i32 */
static wasm_trap_t* cb_contacts_pick(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* is_available: () -> i32 (returns 0) */
static wasm_trap_t* cb_contacts_is_available(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_contacts(wasmtime_linker_t* linker) {
    WAPI_DEFINE_4_1(linker, "wapi_contacts", "pick",          cb_contacts_pick);
    WAPI_DEFINE_0_1(linker, "wapi_contacts", "is_available",  cb_contacts_is_available);
}
