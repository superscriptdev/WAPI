/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * HID Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* request_device: (i32 filters_ptr, i32 filter_count, i32 out_ptr, i32 out_len) -> i32 */
static wasm_trap_t* cb_hid_request_device(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* open: (i32 device) -> i32 */
static wasm_trap_t* cb_hid_open(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* close: (i32 device) -> i32 */
static wasm_trap_t* cb_hid_close(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* send_report: (i32 device, i32 report_id, i32 buf, i32 len) -> i32 */
static wasm_trap_t* cb_hid_send_report(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* send_feature_report: (i32 device, i32 report_id, i32 buf, i32 len) -> i32 */
static wasm_trap_t* cb_hid_send_feature_report(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* receive_report: (i32 device, i32 report_id, i32 buf, i32 len) -> i32 */
static wasm_trap_t* cb_hid_receive_report(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_hid(wasmtime_linker_t* linker) {
    WAPI_DEFINE_4_1(linker, "wapi_hid", "request_device",       cb_hid_request_device);
    WAPI_DEFINE_1_1(linker, "wapi_hid", "open",                 cb_hid_open);
    WAPI_DEFINE_1_1(linker, "wapi_hid", "close",                cb_hid_close);
    WAPI_DEFINE_4_1(linker, "wapi_hid", "send_report",          cb_hid_send_report);
    WAPI_DEFINE_4_1(linker, "wapi_hid", "send_feature_report",  cb_hid_send_feature_report);
    WAPI_DEFINE_4_1(linker, "wapi_hid", "receive_report",       cb_hid_receive_report);
}
