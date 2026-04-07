/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Biometric Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* available_types: () -> i32 */
static wasm_trap_t* cb_bio_available_types(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* WAPI_BIO_NONE */
    return NULL;
}

/* authenticate: (i32 type, i32 reason_ptr, i32 reason_len) -> i32 */
static wasm_trap_t* cb_bio_authenticate(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* can_authenticate: () -> i32 */
static wasm_trap_t* cb_bio_can_authenticate(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* false */
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_biometric(wasmtime_linker_t* linker) {
    WAPI_DEFINE_0_1(linker, "wapi_bio", "available_types",   cb_bio_available_types);
    WAPI_DEFINE_3_1(linker, "wapi_bio", "authenticate",      cb_bio_authenticate);
    WAPI_DEFINE_0_1(linker, "wapi_bio", "can_authenticate",  cb_bio_can_authenticate);
}
