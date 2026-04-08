/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Module Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* load: (i32 hash_ptr, i32 url_ptr, i32 url_len, i32 out_handle) -> i32 */
static wasm_trap_t* cb_module_load(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_func: (i32 module, i32 name_ptr, i32 name_len, i32 out_func) -> i32 */
static wasm_trap_t* cb_module_get_func(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_desc: (i32 module, i32 desc_ptr) -> i32 */
static wasm_trap_t* cb_module_get_desc(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_hash: (i32 module, i32 hash_ptr) -> i32 */
static wasm_trap_t* cb_module_get_hash(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* release: (i32 module) -> i32 */
static wasm_trap_t* cb_module_release(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* call: (i32 module, i32 func, i32 args_ptr, i32 nargs, i32 results_ptr, i32 nresults) -> i32 */
static wasm_trap_t* cb_module_call(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* map: (i32 module, i32 src, i32 len, i32 flags, i32 out_child_ptr) -> i32 */
static wasm_trap_t* cb_module_map(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* unmap: (i32 module, i32 child_ptr) -> i32 */
static wasm_trap_t* cb_module_unmap(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* alloc_create: (i32 module, i32 out_alloc_handle) -> i32 */
static wasm_trap_t* cb_module_alloc_create(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* alloc_get: (i32 alloc_handle, i32 index, i32 out_ptr, i32 out_len) -> i32 */
static wasm_trap_t* cb_module_alloc_get(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* alloc_destroy: (i32 alloc_handle) -> i32 */
static wasm_trap_t* cb_module_alloc_destroy(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_io_policy: (i32 module, i32 policy_flags) -> i32 */
static wasm_trap_t* cb_module_set_io_policy(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* is_cached: (i32 hash_ptr) -> i32 */
static wasm_trap_t* cb_module_is_cached(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* not cached */
    return NULL;
}

/* prefetch: (i32 hash_ptr, i32 url_ptr, i32 url_len) -> i32 */
static wasm_trap_t* cb_module_prefetch(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_module(wasmtime_linker_t* linker) {
    WAPI_DEFINE_4_1(linker, "wapi_module", "load",           cb_module_load);
    WAPI_DEFINE_4_1(linker, "wapi_module", "get_func",       cb_module_get_func);
    WAPI_DEFINE_2_1(linker, "wapi_module", "get_desc",       cb_module_get_desc);
    WAPI_DEFINE_2_1(linker, "wapi_module", "get_hash",       cb_module_get_hash);
    WAPI_DEFINE_1_1(linker, "wapi_module", "release",        cb_module_release);
    WAPI_DEFINE_6_1(linker, "wapi_module", "call",           cb_module_call);
    WAPI_DEFINE_5_1(linker, "wapi_module", "map",            cb_module_map);
    WAPI_DEFINE_2_1(linker, "wapi_module", "unmap",          cb_module_unmap);
    WAPI_DEFINE_2_1(linker, "wapi_module", "alloc_create",   cb_module_alloc_create);
    WAPI_DEFINE_4_1(linker, "wapi_module", "alloc_get",      cb_module_alloc_get);
    WAPI_DEFINE_1_1(linker, "wapi_module", "alloc_destroy",  cb_module_alloc_destroy);
    WAPI_DEFINE_2_1(linker, "wapi_module", "set_io_policy",  cb_module_set_io_policy);
    WAPI_DEFINE_1_1(linker, "wapi_module", "is_cached",      cb_module_is_cached);
    WAPI_DEFINE_3_1(linker, "wapi_module", "prefetch",       cb_module_prefetch);
}
