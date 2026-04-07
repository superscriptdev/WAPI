/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Screen Capture Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* request: (i32 options, i32 out_capture) -> i32 */
static wasm_trap_t* cb_capture_request(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_frame: (i32 capture, i32 buf, i32 len, i32 out_size) -> i32 */
static wasm_trap_t* cb_capture_get_frame(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* stop: (i32 capture) -> i32 */
static wasm_trap_t* cb_capture_stop(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_screen_capture(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_capture", "request",    cb_capture_request);
    WAPI_DEFINE_4_1(linker, "wapi_capture", "get_frame",  cb_capture_get_frame);
    WAPI_DEFINE_1_1(linker, "wapi_capture", "stop",       cb_capture_stop);
}
