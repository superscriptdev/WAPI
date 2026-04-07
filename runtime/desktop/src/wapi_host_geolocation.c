/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Geolocation Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* get_position: (i32 flags, i32 timeout_ms, i32 position_ptr) -> i32 */
static wasm_trap_t* cb_geo_get_position(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* watch_position: (i32 flags, i32 out_watch) -> i32 */
static wasm_trap_t* cb_geo_watch_position(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* clear_watch: (i32 watch) -> i32 */
static wasm_trap_t* cb_geo_clear_watch(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_geolocation(wasmtime_linker_t* linker) {
    WAPI_DEFINE_3_1(linker, "wapi_geo", "get_position",   cb_geo_get_position);
    WAPI_DEFINE_2_1(linker, "wapi_geo", "watch_position",  cb_geo_watch_position);
    WAPI_DEFINE_1_1(linker, "wapi_geo", "clear_watch",     cb_geo_clear_watch);
}
