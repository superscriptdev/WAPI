/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Compression Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* create: (i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_compress_create(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* destroy: (i32) -> i32 */
static wasm_trap_t* cb_compress_destroy(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* write: (i32, i32, i32) -> i32 */
static wasm_trap_t* cb_compress_write(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* read: (i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_compress_read(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* finish: (i32) -> i32 */
static wasm_trap_t* cb_compress_finish(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* oneshot: (i32, i32, i32, i32, i32, i32, i32) -> i32 */
static wasm_trap_t* cb_compress_oneshot(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_compression(wasmtime_linker_t* linker) {
    WAPI_DEFINE_4_1(linker, "wapi_compress", "create",   cb_compress_create);
    WAPI_DEFINE_1_1(linker, "wapi_compress", "destroy",  cb_compress_destroy);
    WAPI_DEFINE_3_1(linker, "wapi_compress", "write",    cb_compress_write);
    WAPI_DEFINE_4_1(linker, "wapi_compress", "read",     cb_compress_read);
    WAPI_DEFINE_1_1(linker, "wapi_compress", "finish",   cb_compress_finish);

    /* oneshot: 7 i32 params -> i32 */
    wapi_linker_define(linker, "wapi_compress", "oneshot", cb_compress_oneshot,
                     7, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I32, WASM_I32,
                                           WASM_I32, WASM_I32, WASM_I32},
                     1, (wasm_valkind_t[]){WASM_I32});
}
