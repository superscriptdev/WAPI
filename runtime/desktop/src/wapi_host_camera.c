/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Camera Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* count: () -> i32 */
static wasm_trap_t* cb_camera_count(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* no cameras */
    return NULL;
}

/* open: (i32 config_ptr, i32 out_camera) -> i32 */
static wasm_trap_t* cb_camera_open(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* close: (i32 camera) -> i32 */
static wasm_trap_t* cb_camera_close(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* read_frame: (i32 camera, i32 frame_ptr, i32 buf, i32 buf_len, i32 size_ptr) -> i32 */
static wasm_trap_t* cb_camera_read_frame(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* read_frame_gpu: (i32 camera, i32 frame_ptr, i32 out_texture) -> i32 */
static wasm_trap_t* cb_camera_read_frame_gpu(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_camera(wasmtime_linker_t* linker) {
    WAPI_DEFINE_0_1(linker, "wapi_camera", "count",          cb_camera_count);
    WAPI_DEFINE_2_1(linker, "wapi_camera", "open",           cb_camera_open);
    WAPI_DEFINE_1_1(linker, "wapi_camera", "close",          cb_camera_close);
    WAPI_DEFINE_5_1(linker, "wapi_camera", "read_frame",     cb_camera_read_frame);
    WAPI_DEFINE_3_1(linker, "wapi_camera", "read_frame_gpu", cb_camera_read_frame_gpu);
}
