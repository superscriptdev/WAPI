/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * XR Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* is_supported: (i32 type) -> i32 */
static wasm_trap_t* cb_xr_is_supported(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* not supported */
    return NULL;
}

/* request_session: (i32 type, i32 out_session) -> i32 */
static wasm_trap_t* cb_xr_request_session(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* end_session: (i32 session) -> i32 */
static wasm_trap_t* cb_xr_end_session(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* create_ref_space: (i32 session, i32 type, i32 out_space) -> i32 */
static wasm_trap_t* cb_xr_create_ref_space(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* wait_frame: (i32 session, i32 state_ptr, i32 views_ptr, i32 max_views) -> i32 */
static wasm_trap_t* cb_xr_wait_frame(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* begin_frame: (i32 session) -> i32 */
static wasm_trap_t* cb_xr_begin_frame(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* end_frame: (i32 session, i32 textures_ptr, i32 tex_count) -> i32 */
static wasm_trap_t* cb_xr_end_frame(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_controller_pose: (i32 session, i32 space, i32 hand, i32 pose_ptr) -> i32 */
static wasm_trap_t* cb_xr_get_controller_pose(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_controller_state: (i32 session, i32 hand, i32 buttons_ptr,
                          i32 trigger_ptr, i32 grip_ptr,
                          i32 thumbstick_x_ptr, i32 thumbstick_y_ptr) -> i32 */
static wasm_trap_t* cb_xr_get_controller_state(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* hit_test: (i32 session, i32 space, i32 origin_ptr, i32 direction_ptr, i32 pose_ptr) -> i32 */
static wasm_trap_t* cb_xr_hit_test(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_xr(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_xr", "is_supported",          cb_xr_is_supported);
    WAPI_DEFINE_2_1(linker, "wapi_xr", "request_session",       cb_xr_request_session);
    WAPI_DEFINE_1_1(linker, "wapi_xr", "end_session",           cb_xr_end_session);
    WAPI_DEFINE_3_1(linker, "wapi_xr", "create_ref_space",      cb_xr_create_ref_space);
    WAPI_DEFINE_4_1(linker, "wapi_xr", "wait_frame",            cb_xr_wait_frame);
    WAPI_DEFINE_1_1(linker, "wapi_xr", "begin_frame",           cb_xr_begin_frame);
    WAPI_DEFINE_3_1(linker, "wapi_xr", "end_frame",             cb_xr_end_frame);
    WAPI_DEFINE_4_1(linker, "wapi_xr", "get_controller_pose",   cb_xr_get_controller_pose);

    /* get_controller_state: 7 i32 params -> i32 */
    wapi_linker_define(linker, "wapi_xr", "get_controller_state", cb_xr_get_controller_state,
                     7, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I32,
                                           WASM_I32,WASM_I32,WASM_I32},
                     1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_5_1(linker, "wapi_xr", "hit_test",              cb_xr_hit_test);
}
