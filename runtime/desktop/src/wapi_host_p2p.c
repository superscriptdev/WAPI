/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * P2P / WebRTC Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* create: (i32 config_ptr, i32 out_handle_ptr) -> i32 */
static wasm_trap_t* cb_p2p_create(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* create_offer: (i32 handle, i32 out_ptr) -> i32 */
static wasm_trap_t* cb_p2p_create_offer(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* create_answer: (i32 handle, i32 out_ptr) -> i32 */
static wasm_trap_t* cb_p2p_create_answer(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_remote_desc: (i32 handle, i32 desc_ptr, i32 desc_len) -> i32 */
static wasm_trap_t* cb_p2p_set_remote_desc(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* add_ice_candidate: (i32 handle, i32 candidate_ptr, i32 candidate_len) -> i32 */
static wasm_trap_t* cb_p2p_add_ice_candidate(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* send: (i32 handle, i32 data_ptr, i32 data_len) -> i32 */
static wasm_trap_t* cb_p2p_send(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* close: (i32 handle) -> i32 */
static wasm_trap_t* cb_p2p_close(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_p2p(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_p2p", "create",            cb_p2p_create);
    WAPI_DEFINE_2_1(linker, "wapi_p2p", "create_offer",      cb_p2p_create_offer);
    WAPI_DEFINE_2_1(linker, "wapi_p2p", "create_answer",     cb_p2p_create_answer);
    WAPI_DEFINE_3_1(linker, "wapi_p2p", "set_remote_desc",   cb_p2p_set_remote_desc);
    WAPI_DEFINE_3_1(linker, "wapi_p2p", "add_ice_candidate", cb_p2p_add_ice_candidate);
    WAPI_DEFINE_3_1(linker, "wapi_p2p", "send",              cb_p2p_send);
    WAPI_DEFINE_1_1(linker, "wapi_p2p", "close",             cb_p2p_close);
}
