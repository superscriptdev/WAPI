/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * MIDI Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* request_access: (i32 sysex) -> i32 */
static wasm_trap_t* cb_midi_request_access(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* port_count: (i32 type) -> i32 */
static wasm_trap_t* cb_midi_port_count(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* no ports */
    return NULL;
}

/* port_name: (i32 type, i32 index, i32 buf, i32 buf_len, i32 name_len_ptr) -> i32 */
static wasm_trap_t* cb_midi_port_name(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* open_port: (i32 type, i32 index, i32 out_port) -> i32 */
static wasm_trap_t* cb_midi_open_port(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* close_port: (i32 port) -> i32 */
static wasm_trap_t* cb_midi_close_port(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* send: (i32 port, i32 data_ptr, i32 len) -> i32 */
static wasm_trap_t* cb_midi_send(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* recv: (i32 port, i32 buf, i32 buf_len, i32 msg_len_ptr, i32 timestamp_ptr) -> i32 */
static wasm_trap_t* cb_midi_recv(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_midi(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_midi", "request_access", cb_midi_request_access);
    WAPI_DEFINE_1_1(linker, "wapi_midi", "port_count",     cb_midi_port_count);
    WAPI_DEFINE_5_1(linker, "wapi_midi", "port_name",      cb_midi_port_name);
    WAPI_DEFINE_3_1(linker, "wapi_midi", "open_port",      cb_midi_open_port);
    WAPI_DEFINE_1_1(linker, "wapi_midi", "close_port",     cb_midi_close_port);
    WAPI_DEFINE_3_1(linker, "wapi_midi", "send",           cb_midi_send);
    WAPI_DEFINE_5_1(linker, "wapi_midi", "recv",           cb_midi_recv);
}
