/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Register Callbacks (all return WAPI_ERR_NOTSUP unless noted)
 * ============================================================ */

/* url_scheme: (i32, i32) -> i32 */
static wasm_trap_t* cb_reg_url_scheme(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* unregister_url_scheme: (i32, i32) -> i32 */
static wasm_trap_t* cb_reg_unregister_url_scheme(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* file_type: (i32) -> i32 */
static wasm_trap_t* cb_reg_file_type(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* unregister_file_type: (i32, i32) -> i32 */
static wasm_trap_t* cb_reg_unregister_file_type(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* preview_provider: (i32, i32) -> i32 */
static wasm_trap_t* cb_reg_preview_provider(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* is_default_for_scheme: (i32, i32) -> i32 (returns 0) */
static wasm_trap_t* cb_reg_is_default_for_scheme(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0);
    return NULL;
}

/* is_default_for_type: (i32, i32) -> i32 (returns 0) */
static wasm_trap_t* cb_reg_is_default_for_type(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_register(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_register", "url_scheme",              cb_reg_url_scheme);
    WAPI_DEFINE_2_1(linker, "wapi_register", "unregister_url_scheme",   cb_reg_unregister_url_scheme);
    WAPI_DEFINE_1_1(linker, "wapi_register", "file_type",               cb_reg_file_type);
    WAPI_DEFINE_2_1(linker, "wapi_register", "unregister_file_type",    cb_reg_unregister_file_type);
    WAPI_DEFINE_2_1(linker, "wapi_register", "preview_provider",        cb_reg_preview_provider);
    WAPI_DEFINE_2_1(linker, "wapi_register", "is_default_for_scheme",   cb_reg_is_default_for_scheme);
    WAPI_DEFINE_2_1(linker, "wapi_register", "is_default_for_type",     cb_reg_is_default_for_type);
}
