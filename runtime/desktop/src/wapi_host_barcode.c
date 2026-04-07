/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Barcode Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* detect: (i32, i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_barcode_detect(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* detect_from_camera: (i32, i32, i32) -> i32 */
static wasm_trap_t* cb_barcode_detect_from_camera(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_barcode(wasmtime_linker_t* linker) {
    WAPI_DEFINE_5_1(linker, "wapi_barcode", "detect",              cb_barcode_detect);
    WAPI_DEFINE_3_1(linker, "wapi_barcode", "detect_from_camera",  cb_barcode_detect_from_camera);
}
