/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Authentication Callbacks
 * ============================================================ */

/* create_credential: (i32, i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_authn_create_credential(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_assertion: (i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_authn_get_assertion(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* is_available: () -> i32   (returns 0 = not available) */
static wasm_trap_t* cb_authn_is_available(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_authn(wasmtime_linker_t* linker) {
    WAPI_DEFINE_5_1(linker, "wapi_authn", "create_credential", cb_authn_create_credential);
    WAPI_DEFINE_4_1(linker, "wapi_authn", "get_assertion",     cb_authn_get_assertion);
    WAPI_DEFINE_0_1(linker, "wapi_authn", "is_available",      cb_authn_is_available);
}
