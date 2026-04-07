/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * NFC Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* scan_start: () -> i32 */
static wasm_trap_t* cb_nfc_scan_start(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* scan_stop: () -> i32 */
static wasm_trap_t* cb_nfc_scan_stop(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* write: (i32, i32, i32) -> i32 */
static wasm_trap_t* cb_nfc_write(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* make_read_only: (i32) -> i32 */
static wasm_trap_t* cb_nfc_make_read_only(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_nfc(wasmtime_linker_t* linker) {
    WAPI_DEFINE_0_1(linker, "wapi_nfc", "scan_start",     cb_nfc_scan_start);
    WAPI_DEFINE_0_1(linker, "wapi_nfc", "scan_stop",       cb_nfc_scan_stop);
    WAPI_DEFINE_3_1(linker, "wapi_nfc", "write",            cb_nfc_write);
    WAPI_DEFINE_1_1(linker, "wapi_nfc", "make_read_only",  cb_nfc_make_read_only);
}
