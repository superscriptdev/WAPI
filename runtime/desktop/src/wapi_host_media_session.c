/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Media Session Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* set_metadata: (i32) -> i32 */
static wasm_trap_t* cb_media_set_metadata(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_playback_state: (i32) -> i32 */
static wasm_trap_t* cb_media_set_playback_state(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_position: (f32, f32) -> i32 */
static wasm_trap_t* cb_media_set_position(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_actions: (i32, i32) -> i32 */
static wasm_trap_t* cb_media_set_actions(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_media_session(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_media", "set_metadata",        cb_media_set_metadata);
    WAPI_DEFINE_1_1(linker, "wapi_media", "set_playback_state",  cb_media_set_playback_state);

    /* set_position: f32, f32 -> i32 */
    wapi_linker_define(linker, "wapi_media", "set_position", cb_media_set_position,
                     2, (wasm_valkind_t[]){WASM_F32, WASM_F32},
                     1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_2_1(linker, "wapi_media", "set_actions",         cb_media_set_actions);
}
