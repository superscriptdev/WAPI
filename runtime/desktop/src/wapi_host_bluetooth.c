/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Bluetooth Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* request_device: (i32 filters_ptr, i32 filter_count, i32 out_device) -> i32 */
static wasm_trap_t* cb_bt_request_device(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* connect: (i32 device) -> i32 */
static wasm_trap_t* cb_bt_connect(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* disconnect: (i32 device) -> i32 */
static wasm_trap_t* cb_bt_disconnect(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_service: (i32 device, i32 uuid_ptr, i32 uuid_len, i32 out_service) -> i32 */
static wasm_trap_t* cb_bt_get_service(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_characteristic: (i32 service, i32 uuid_ptr, i32 uuid_len, i32 out_char) -> i32 */
static wasm_trap_t* cb_bt_get_characteristic(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* read_value: (i32 characteristic, i32 buf, i32 buf_len, i32 val_len_ptr) -> i32 */
static wasm_trap_t* cb_bt_read_value(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* write_value: (i32 characteristic, i32 data_ptr, i32 len) -> i32 */
static wasm_trap_t* cb_bt_write_value(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* start_notifications: (i32 characteristic) -> i32 */
static wasm_trap_t* cb_bt_start_notifications(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* stop_notifications: (i32 characteristic) -> i32 */
static wasm_trap_t* cb_bt_stop_notifications(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_bluetooth(wasmtime_linker_t* linker) {
    WAPI_DEFINE_3_1(linker, "wapi_bt", "request_device",      cb_bt_request_device);
    WAPI_DEFINE_1_1(linker, "wapi_bt", "connect",             cb_bt_connect);
    WAPI_DEFINE_1_1(linker, "wapi_bt", "disconnect",          cb_bt_disconnect);
    WAPI_DEFINE_4_1(linker, "wapi_bt", "get_service",         cb_bt_get_service);
    WAPI_DEFINE_4_1(linker, "wapi_bt", "get_characteristic",  cb_bt_get_characteristic);
    WAPI_DEFINE_4_1(linker, "wapi_bt", "read_value",          cb_bt_read_value);
    WAPI_DEFINE_3_1(linker, "wapi_bt", "write_value",         cb_bt_write_value);
    WAPI_DEFINE_1_1(linker, "wapi_bt", "start_notifications", cb_bt_start_notifications);
    WAPI_DEFINE_1_1(linker, "wapi_bt", "stop_notifications",  cb_bt_stop_notifications);
}
