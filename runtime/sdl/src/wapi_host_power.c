/**
 * WAPI SDL Runtime - Power
 *
 * Battery/source info via SDL_GetPowerInfo. Wake-lock, idle, saver, and
 * thermal queries are not exposed by SDL3 and return WAPI_ERR_NOTSUP.
 */

#include "wapi_host.h"
#include <math.h>

static int32_t host_power_get_info(wasm_exec_env_t env, uint32_t info_ptr) {
    (void)env;
    int seconds = -1, percent = -1;
    SDL_PowerState state = SDL_GetPowerInfo(&seconds, &percent);

    uint32_t src;
    switch (state) {
        case SDL_POWERSTATE_ON_BATTERY: src = 1; break;  /* BATTERY */
        case SDL_POWERSTATE_NO_BATTERY: src = 2; break;  /* AC */
        case SDL_POWERSTATE_CHARGING:   src = 3; break;  /* CHARGING */
        case SDL_POWERSTATE_CHARGED:    src = 4; break;  /* CHARGED */
        default:                         src = 0; break;
    }
    float level = percent >= 0 ? (percent / 100.0f) : NAN;
    float secs  = seconds >= 0 ? (float)seconds : INFINITY;

    uint8_t buf[16] = {0};
    memcpy(buf + 0, &src,   4);
    memcpy(buf + 4, &level, 4);
    memcpy(buf + 8, &secs,  4);
    wapi_wasm_write_bytes(info_ptr, buf, 16);
    return WAPI_OK;
}

static int32_t host_power_wake_acquire(wasm_exec_env_t env,
                                       int32_t type, uint32_t lock_out) {
    (void)env; (void)type;
    wapi_wasm_write_i32(lock_out, 0);
    return WAPI_ERR_NOTSUP;
}

static int32_t host_power_wake_release(wasm_exec_env_t env, int32_t lock) {
    (void)env; (void)lock; return WAPI_ERR_NOTSUP;
}

static int32_t host_power_idle_start(wasm_exec_env_t env, int32_t threshold_ms) {
    (void)env; (void)threshold_ms; return WAPI_ERR_NOTSUP;
}
static int32_t host_power_idle_stop(wasm_exec_env_t env) {
    (void)env; return WAPI_ERR_NOTSUP;
}
static int32_t host_power_idle_get(wasm_exec_env_t env, uint32_t out_state) {
    (void)env; (void)out_state; return WAPI_ERR_NOTSUP;
}
static int32_t host_power_saver_get(wasm_exec_env_t env, uint32_t out_state) {
    (void)env; (void)out_state; return WAPI_ERR_NOTSUP;
}
static int32_t host_power_thermal_get(wasm_exec_env_t env, uint32_t out_state) {
    (void)env; (void)out_state; return WAPI_ERR_NOTSUP;
}

static NativeSymbol g_symbols[] = {
    { "get_info",     (void*)host_power_get_info,     "(i)i",  NULL },
    { "wake_acquire", (void*)host_power_wake_acquire, "(ii)i", NULL },
    { "wake_release", (void*)host_power_wake_release, "(i)i",  NULL },
    { "idle_start",   (void*)host_power_idle_start,   "(i)i",  NULL },
    { "idle_stop",    (void*)host_power_idle_stop,    "()i",   NULL },
    { "idle_get",     (void*)host_power_idle_get,     "(i)i",  NULL },
    { "saver_get",    (void*)host_power_saver_get,    "(i)i",  NULL },
    { "thermal_get",  (void*)host_power_thermal_get,  "(i)i",  NULL },
};

wapi_cap_registration_t wapi_host_power_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_power",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
