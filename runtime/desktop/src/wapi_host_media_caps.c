/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Media Caps Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* query_decode: (i32 config_ptr, i32 out_ptr) -> i32 */
static wasm_trap_t* cb_mediacaps_query_decode(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* query_encode: (i32 config_ptr, i32 out_ptr) -> i32 */
static wasm_trap_t* cb_mediacaps_query_encode(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_media_caps(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_mediacaps", "query_decode", cb_mediacaps_query_decode);
    WAPI_DEFINE_2_1(linker, "wapi_mediacaps", "query_encode", cb_mediacaps_query_encode);
}
