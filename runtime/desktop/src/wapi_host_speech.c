/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Speech Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* speak: (i32 utterance_ptr, i32 out_id) -> i32 */
static wasm_trap_t* cb_speech_speak(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* cancel: (i32 id) -> i32 */
static wasm_trap_t* cb_speech_cancel(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* cancel_all: () -> i32 */
static wasm_trap_t* cb_speech_cancel_all(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* is_speaking: () -> i32 */
static wasm_trap_t* cb_speech_is_speaking(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* not speaking */
    return NULL;
}

/* recognize_start: (i32 lang_ptr, i32 lang_len, i32 continuous, i32 out_session) -> i32 */
static wasm_trap_t* cb_speech_recognize_start(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* recognize_stop: (i32 session) -> i32 */
static wasm_trap_t* cb_speech_recognize_stop(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* recognize_result: (i32 session, i32 buf, i32 buf_len, i32 text_len_ptr, i32 confidence_ptr) -> i32 */
static wasm_trap_t* cb_speech_recognize_result(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_speech(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_speech", "speak",            cb_speech_speak);
    WAPI_DEFINE_1_1(linker, "wapi_speech", "cancel",           cb_speech_cancel);
    WAPI_DEFINE_0_1(linker, "wapi_speech", "cancel_all",       cb_speech_cancel_all);
    WAPI_DEFINE_0_1(linker, "wapi_speech", "is_speaking",      cb_speech_is_speaking);
    WAPI_DEFINE_4_1(linker, "wapi_speech", "recognize_start",  cb_speech_recognize_start);
    WAPI_DEFINE_1_1(linker, "wapi_speech", "recognize_stop",   cb_speech_recognize_stop);
    WAPI_DEFINE_5_1(linker, "wapi_speech", "recognize_result", cb_speech_recognize_result);
}
