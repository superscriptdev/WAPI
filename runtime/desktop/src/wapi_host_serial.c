/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Serial Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* request_port: (i32 out_port) -> i32 */
static wasm_trap_t* cb_serial_request_port(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* open: (i32 port, i32 options) -> i32 */
static wasm_trap_t* cb_serial_open(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* close: (i32 port) -> i32 */
static wasm_trap_t* cb_serial_close(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* write: (i32 port, i32 buf, i32 len) -> i32 */
static wasm_trap_t* cb_serial_write(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* read: (i32 port, i32 buf, i32 len, i32 bytes_read_ptr) -> i32 */
static wasm_trap_t* cb_serial_read(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_signals: (i32 port, i32 signals, i32 mask) -> i32 */
static wasm_trap_t* cb_serial_set_signals(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_serial(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_serial", "request_port",  cb_serial_request_port);
    WAPI_DEFINE_2_1(linker, "wapi_serial", "open",          cb_serial_open);
    WAPI_DEFINE_1_1(linker, "wapi_serial", "close",         cb_serial_close);
    WAPI_DEFINE_3_1(linker, "wapi_serial", "write",         cb_serial_write);
    WAPI_DEFINE_4_1(linker, "wapi_serial", "read",          cb_serial_read);
    WAPI_DEFINE_3_1(linker, "wapi_serial", "set_signals",   cb_serial_set_signals);
}
