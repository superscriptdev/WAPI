/**
 * WAPI SDL Runtime - Haptics (SDL3 SDL_Haptic)
 *
 * Maps wapi_haptic opcodes to SDL_Haptic* APIs. Gamepad rumble uses
 * SDL_RumbleGamepad; device-level haptics use SDL_Haptic*.
 * Module name is "wapi_haptic" (singular) per the ABI.
 */

#include "wapi_host.h"

static int32_t host_vibrate(wasm_exec_env_t env,
                            int32_t duration_ms, int32_t intensity_pct) {
    (void)env; (void)duration_ms; (void)intensity_pct;
    /* Global device vibrate is not a first-class SDL3 concept. */
    return WAPI_ERR_NOTSUP;
}

static int32_t host_vibrate_cancel(wasm_exec_env_t env) {
    (void)env;
    return WAPI_ERR_NOTSUP;
}

static int32_t host_haptic_open(wasm_exec_env_t env,
                                int32_t device_id, uint32_t out_handle) {
    (void)env;
    SDL_Haptic* h = SDL_OpenHaptic((SDL_HapticID)device_id);
    if (!h) {
        wapi_wasm_write_i32(out_handle, 0);
        return WAPI_ERR_IO;
    }
    int32_t handle = wapi_handle_alloc(WAPI_HTYPE_HAPTIC);
    if (handle == 0) { SDL_CloseHaptic(h); return WAPI_ERR_NOMEM; }
    g_rt.handles[handle].data.haptic = h;
    wapi_wasm_write_i32(out_handle, handle);
    return WAPI_OK;
}

static int32_t host_haptic_close(wasm_exec_env_t env, int32_t handle) {
    (void)env;
    if (!wapi_handle_valid(handle, WAPI_HTYPE_HAPTIC)) return WAPI_ERR_BADF;
    SDL_CloseHaptic(g_rt.handles[handle].data.haptic);
    wapi_handle_free(handle);
    return WAPI_OK;
}

static int32_t host_haptic_rumble(wasm_exec_env_t env,
                                  int32_t handle, uint32_t intensity_bits,
                                  uint32_t duration_ms) {
    (void)env;
    if (!wapi_handle_valid(handle, WAPI_HTYPE_HAPTIC)) return WAPI_ERR_BADF;
    float intensity;
    memcpy(&intensity, &intensity_bits, 4);
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;
    SDL_Haptic* h = g_rt.handles[handle].data.haptic;
    if (!SDL_InitHapticRumble(h)) return WAPI_ERR_IO;
    return SDL_PlayHapticRumble(h, intensity, duration_ms)
        ? WAPI_OK : WAPI_ERR_IO;
}

static int32_t host_haptic_get_features(wasm_exec_env_t env,
                                        int32_t handle) {
    (void)env;
    if (!wapi_handle_valid(handle, WAPI_HTYPE_HAPTIC)) return 0;
    return (int32_t)SDL_GetHapticFeatures(g_rt.handles[handle].data.haptic);
}

static int32_t host_haptic_effect_create(wasm_exec_env_t env,
                                         int32_t handle, uint32_t type,
                                         uint32_t params_ptr, uint32_t params_len) {
    (void)env; (void)handle; (void)type; (void)params_ptr; (void)params_len;
    return WAPI_ERR_NOTSUP;
}
static int32_t host_haptic_effect_play(wasm_exec_env_t env,
                                       int32_t handle, int32_t effect_id) {
    (void)env; (void)handle; (void)effect_id; return WAPI_ERR_NOTSUP;
}
static int32_t host_haptic_effect_stop(wasm_exec_env_t env, int32_t effect_id) {
    (void)env; (void)effect_id; return WAPI_ERR_NOTSUP;
}
static int32_t host_haptic_effect_destroy(wasm_exec_env_t env, int32_t effect_id) {
    (void)env; (void)effect_id; return WAPI_ERR_NOTSUP;
}

static NativeSymbol g_symbols[] = {
    { "vibrate",               (void*)host_vibrate,               "(ii)i",   NULL },
    { "vibrate_cancel",        (void*)host_vibrate_cancel,        "()i",     NULL },
    { "haptic_open",           (void*)host_haptic_open,           "(ii)i",   NULL },
    { "haptic_close",          (void*)host_haptic_close,          "(i)i",    NULL },
    { "haptic_rumble",         (void*)host_haptic_rumble,         "(iii)i",  NULL },
    { "haptic_get_features",   (void*)host_haptic_get_features,   "(i)i",    NULL },
    { "haptic_effect_create",  (void*)host_haptic_effect_create,  "(iiii)i", NULL },
    { "haptic_effect_play",    (void*)host_haptic_effect_play,    "(ii)i",   NULL },
    { "haptic_effect_stop",    (void*)host_haptic_effect_stop,    "(i)i",    NULL },
    { "haptic_effect_destroy", (void*)host_haptic_effect_destroy, "(i)i",    NULL },
};

wapi_cap_registration_t wapi_host_haptics_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_haptic",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
