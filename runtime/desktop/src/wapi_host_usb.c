/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * USB Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* request_device: (i32 filters_ptr, i32 filter_count, i32 out_device) -> i32 */
static wasm_trap_t* cb_usb_request_device(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* open: (i32 device) -> i32 */
static wasm_trap_t* cb_usb_open(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* close: (i32 device) -> i32 */
static wasm_trap_t* cb_usb_close(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* claim_interface: (i32 device, i32 interface_num) -> i32 */
static wasm_trap_t* cb_usb_claim_interface(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* release_interface: (i32 device, i32 interface_num) -> i32 */
static wasm_trap_t* cb_usb_release_interface(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* transfer_in: (i32 device, i32 endpoint, i32 buf, i32 len, i32 transferred_ptr) -> i32 */
static wasm_trap_t* cb_usb_transfer_in(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* transfer_out: (i32 device, i32 endpoint, i32 buf, i32 len, i32 transferred_ptr) -> i32 */
static wasm_trap_t* cb_usb_transfer_out(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* control_transfer: (i32 device, i32 request_type, i32 request,
                      i32 value, i32 index, i32 buf, i32 len, i32 transferred_ptr) -> i32 */
static wasm_trap_t* cb_usb_control_transfer(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_usb(wasmtime_linker_t* linker) {
    WAPI_DEFINE_3_1(linker, "wapi_usb", "request_device",    cb_usb_request_device);
    WAPI_DEFINE_1_1(linker, "wapi_usb", "open",              cb_usb_open);
    WAPI_DEFINE_1_1(linker, "wapi_usb", "close",             cb_usb_close);
    WAPI_DEFINE_2_1(linker, "wapi_usb", "claim_interface",   cb_usb_claim_interface);
    WAPI_DEFINE_2_1(linker, "wapi_usb", "release_interface", cb_usb_release_interface);
    WAPI_DEFINE_5_1(linker, "wapi_usb", "transfer_in",       cb_usb_transfer_in);
    WAPI_DEFINE_5_1(linker, "wapi_usb", "transfer_out",      cb_usb_transfer_out);

    /* control_transfer: 8 i32 params -> i32 */
    wapi_linker_define(linker, "wapi_usb", "control_transfer", cb_usb_control_transfer,
                     8, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I32,
                                           WASM_I32,WASM_I32,WASM_I32,WASM_I32},
                     1, (wasm_valkind_t[]){WASM_I32});
}
