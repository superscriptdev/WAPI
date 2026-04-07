/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Module Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* load: (i32 import_ptr, i32 out_handle) -> i32 */
static wasm_trap_t* cb_module_load(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* init: (i32 module, i32 ctx_ptr) -> i32 */
static wasm_trap_t* cb_module_init(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_func: (i32 module, i32 name_ptr, i32 name_len) -> i32 (returns u32 func ptr) */
static wasm_trap_t* cb_module_get_func(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* NULL function pointer = not found */
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

/* release: (i32 module) -> i32 */
static wasm_trap_t* cb_module_release(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* lend: (i32 ptr, i32 len, i32 out_lease) -> i32 */
static wasm_trap_t* cb_module_lend(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* return_lease: (i32 lease) -> i32 */
static wasm_trap_t* cb_module_return_lease(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* is_lent: (i32 ptr, i32 len) -> i32 */
static wasm_trap_t* cb_module_is_lent(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* not lent */
    return NULL;
}

/* is_cached: (i32 name_ptr, i32 name_len, i32 major) -> i32 */
static wasm_trap_t* cb_module_is_cached(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* not cached */
    return NULL;
}

/* prefetch: (i32 import_ptr) -> i32 */
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
    WAPI_DEFINE_2_1(linker, "wapi_module", "load",          cb_module_load);
    WAPI_DEFINE_2_1(linker, "wapi_module", "init",          cb_module_init);
    WAPI_DEFINE_3_1(linker, "wapi_module", "get_func",      cb_module_get_func);
    WAPI_DEFINE_2_1(linker, "wapi_module", "get_desc",      cb_module_get_desc);
    WAPI_DEFINE_1_1(linker, "wapi_module", "release",       cb_module_release);
    WAPI_DEFINE_3_1(linker, "wapi_module", "lend",          cb_module_lend);
    WAPI_DEFINE_1_1(linker, "wapi_module", "return_lease",  cb_module_return_lease);
    WAPI_DEFINE_2_1(linker, "wapi_module", "is_lent",       cb_module_is_lent);
    WAPI_DEFINE_3_1(linker, "wapi_module", "is_cached",     cb_module_is_cached);
    WAPI_DEFINE_1_1(linker, "wapi_module", "prefetch",      cb_module_prefetch);
}
