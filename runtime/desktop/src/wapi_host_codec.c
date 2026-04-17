/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Codec Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* create_video: (i32, i32) -> i32 */
static wasm_trap_t* cb_codec_create_video(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* create_audio: (i32, i32) -> i32 */
static wasm_trap_t* cb_codec_create_audio(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* destroy: (i32) -> i32 */
static wasm_trap_t* cb_codec_destroy(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* is_supported: (i32, i32) -> i32 */
static wasm_trap_t* cb_codec_is_supported(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* decode: (i32, i32) -> i32 */
static wasm_trap_t* cb_codec_decode(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* encode: (i32, i32, i32, i64) -> i32 */
static wasm_trap_t* cb_codec_encode(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_output: (i32, i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_codec_get_output(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* flush: (i32) -> i32 */
static wasm_trap_t* cb_codec_flush(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* query_decode: (i32, i32) -> i32 */
static wasm_trap_t* cb_codec_query_decode(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* query_encode: (i32, i32) -> i32 */
static wasm_trap_t* cb_codec_query_encode(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_codec(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_codec", "create_video",  cb_codec_create_video);
    WAPI_DEFINE_2_1(linker, "wapi_codec", "create_audio",  cb_codec_create_audio);
    WAPI_DEFINE_1_1(linker, "wapi_codec", "destroy",        cb_codec_destroy);
    WAPI_DEFINE_2_1(linker, "wapi_codec", "is_supported",   cb_codec_is_supported);
    WAPI_DEFINE_2_1(linker, "wapi_codec", "decode",          cb_codec_decode);

    /* encode: i32, i32, i32, i64 -> i32 */
    wapi_linker_define(linker, "wapi_codec", "encode", cb_codec_encode,
                     4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I32, WASM_I64},
                     1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_5_1(linker, "wapi_codec", "get_output",     cb_codec_get_output);
    WAPI_DEFINE_1_1(linker, "wapi_codec", "flush",           cb_codec_flush);
    WAPI_DEFINE_2_1(linker, "wapi_codec", "query_decode",    cb_codec_query_decode);
    WAPI_DEFINE_2_1(linker, "wapi_codec", "query_encode",    cb_codec_query_encode);
}
