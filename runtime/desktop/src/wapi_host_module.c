/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Module Lifecycle Callbacks (all return WAPI_ERR_NOTSUP)
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

/* ============================================================
 * Shared Memory Callbacks
 * ============================================================ */

/* shared_alloc: (i32 size, i32 align) -> i32 */
static wasm_trap_t* cb_module_shared_alloc(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* failure: offset 0 */
    return NULL;
}

/* shared_free: (i32 offset) -> i32 */
static wasm_trap_t* cb_module_shared_free(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* shared_realloc: (i32 offset, i32 new_size, i32 align) -> i32 */
static wasm_trap_t* cb_module_shared_realloc(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* failure: offset 0 */
    return NULL;
}

/* shared_usable_size: (i32 offset) -> i32 */
static wasm_trap_t* cb_module_shared_usable_size(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0);
    return NULL;
}

/* shared_read: (i32 src_offset, i32 dst_ptr, i32 len) -> i32 */
static wasm_trap_t* cb_module_shared_read(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* shared_write: (i32 dst_offset, i32 src_ptr, i32 len) -> i32 */
static wasm_trap_t* cb_module_shared_write(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Borrow System Callbacks
 * ============================================================ */

/* lend: (i32 module, i32 offset, i32 flags, i32 out_borrow) -> i32 */
static wasm_trap_t* cb_module_lend(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* reclaim: (i32 borrow) -> i32 */
static wasm_trap_t* cb_module_reclaim(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Explicit Copy Callback
 * ============================================================ */

/* copy_in: (i32 module, i32 src_ptr, i32 len, i32 out_child_ptr) -> i32 */
static wasm_trap_t* cb_module_copy_in(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * I/O Policy & Cache Callbacks
 * ============================================================ */

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
    /* Module lifecycle */
    WAPI_DEFINE_4_1(linker, "wapi_module", "load",              cb_module_load);
    WAPI_DEFINE_4_1(linker, "wapi_module", "get_func",          cb_module_get_func);
    WAPI_DEFINE_2_1(linker, "wapi_module", "get_desc",          cb_module_get_desc);
    WAPI_DEFINE_2_1(linker, "wapi_module", "get_hash",          cb_module_get_hash);
    WAPI_DEFINE_1_1(linker, "wapi_module", "release",           cb_module_release);
    WAPI_DEFINE_6_1(linker, "wapi_module", "call",              cb_module_call);

    /* Shared memory */
    WAPI_DEFINE_2_1(linker, "wapi_module", "shared_alloc",      cb_module_shared_alloc);
    WAPI_DEFINE_1_1(linker, "wapi_module", "shared_free",       cb_module_shared_free);
    WAPI_DEFINE_3_1(linker, "wapi_module", "shared_realloc",    cb_module_shared_realloc);
    WAPI_DEFINE_1_1(linker, "wapi_module", "shared_usable_size",cb_module_shared_usable_size);
    WAPI_DEFINE_3_1(linker, "wapi_module", "shared_read",       cb_module_shared_read);
    WAPI_DEFINE_3_1(linker, "wapi_module", "shared_write",      cb_module_shared_write);

    /* Borrow system */
    WAPI_DEFINE_4_1(linker, "wapi_module", "lend",              cb_module_lend);
    WAPI_DEFINE_1_1(linker, "wapi_module", "reclaim",           cb_module_reclaim);

    /* Explicit copy */
    WAPI_DEFINE_4_1(linker, "wapi_module", "copy_in",           cb_module_copy_in);

    /* I/O policy & cache */
    WAPI_DEFINE_2_1(linker, "wapi_module", "set_io_policy",     cb_module_set_io_policy);
    WAPI_DEFINE_1_1(linker, "wapi_module", "is_cached",         cb_module_is_cached);
    WAPI_DEFINE_3_1(linker, "wapi_module", "prefetch",          cb_module_prefetch);
}
