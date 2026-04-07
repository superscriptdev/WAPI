/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Sensor Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* available: (i32 type) -> i32 */
static wasm_trap_t* cb_sensor_available(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* not available */
    return NULL;
}

/* start: (i32 type, f32 freq_hz, i32 out_sensor) -> i32 */
static wasm_trap_t* cb_sensor_start(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* stop: (i32 sensor) -> i32 */
static wasm_trap_t* cb_sensor_stop(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* read_xyz: (i32 sensor, i32 reading_ptr) -> i32 */
static wasm_trap_t* cb_sensor_read_xyz(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* read_scalar: (i32 sensor, i32 reading_ptr) -> i32 */
static wasm_trap_t* cb_sensor_read_scalar(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_sensors(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_sensor", "available",    cb_sensor_available);

    /* start: (i32 type, f32 freq_hz, i32 out_sensor) -> i32 */
    wapi_linker_define(linker, "wapi_sensor", "start", cb_sensor_start,
                     3, (wasm_valkind_t[]){WASM_I32, WASM_F32, WASM_I32},
                     1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_1_1(linker, "wapi_sensor", "stop",         cb_sensor_stop);
    WAPI_DEFINE_2_1(linker, "wapi_sensor", "read_xyz",     cb_sensor_read_xyz);
    WAPI_DEFINE_2_1(linker, "wapi_sensor", "read_scalar",  cb_sensor_read_scalar);
}
