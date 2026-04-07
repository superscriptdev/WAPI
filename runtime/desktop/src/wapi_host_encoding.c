/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Encoding Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* convert: (i32, i32, i32, i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_encode_convert(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* query_size: (i32, i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_encode_query_size(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* detect: (i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_encode_detect(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_encoding(wasmtime_linker_t* linker) {
    wapi_linker_define(linker, "wapi_encode", "convert", cb_encode_convert,
        7, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I32,WASM_I32,WASM_I32,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_5_1(linker, "wapi_encode", "query_size", cb_encode_query_size);
    WAPI_DEFINE_4_1(linker, "wapi_encode", "detect",     cb_encode_detect);
}
