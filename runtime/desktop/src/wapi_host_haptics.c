/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Haptics Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

static wasm_trap_t* cb_haptic_vibrate(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

static wasm_trap_t* cb_haptic_vibrate_cancel(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

static wasm_trap_t* cb_haptic_open(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

static wasm_trap_t* cb_haptic_close(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

static wasm_trap_t* cb_haptic_rumble(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

static wasm_trap_t* cb_haptic_get_features(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0);
    return NULL;
}

static wasm_trap_t* cb_haptic_effect_create(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

static wasm_trap_t* cb_haptic_effect_play(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

static wasm_trap_t* cb_haptic_effect_stop(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

static wasm_trap_t* cb_haptic_effect_destroy(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_haptics(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_haptic", "vibrate",              cb_haptic_vibrate);
    WAPI_DEFINE_0_1(linker, "wapi_haptic", "vibrate_cancel",       cb_haptic_vibrate_cancel);
    WAPI_DEFINE_2_1(linker, "wapi_haptic", "haptic_open",          cb_haptic_open);
    WAPI_DEFINE_1_1(linker, "wapi_haptic", "haptic_close",         cb_haptic_close);
    WAPI_DEFINE_3_1(linker, "wapi_haptic", "haptic_rumble",        cb_haptic_rumble);
    WAPI_DEFINE_1_1(linker, "wapi_haptic", "haptic_get_features",  cb_haptic_get_features);
    WAPI_DEFINE_4_1(linker, "wapi_haptic", "haptic_effect_create", cb_haptic_effect_create);
    WAPI_DEFINE_2_1(linker, "wapi_haptic", "haptic_effect_play",   cb_haptic_effect_play);
    WAPI_DEFINE_1_1(linker, "wapi_haptic", "haptic_effect_stop",   cb_haptic_effect_stop);
    WAPI_DEFINE_1_1(linker, "wapi_haptic", "haptic_effect_destroy",cb_haptic_effect_destroy);
}
