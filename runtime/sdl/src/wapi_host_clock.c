/**
 * WAPI SDL Runtime - Clocks and Timers
 *
 * Implements: wapi_clock.time_get, wapi_clock.resolution,
 *             wapi_clock.perf_counter, wapi_clock.perf_frequency,
 *             wapi_clock.yield, wapi_clock.sleep
 *
 * Uses SDL3 for high-resolution timing.
 */

#include "wapi_host.h"
#include <time.h>

static int32_t host_time_get(wasm_exec_env_t env,
                             int32_t clock_id, uint32_t time_ptr) {
    (void)env;
    uint64_t ns;
    switch (clock_id) {
    case 0: /* WAPI_CLOCK_MONOTONIC */
        ns = SDL_GetTicksNS();
        break;
    case 1: { /* WAPI_CLOCK_REALTIME */
        SDL_Time sdl_time;
        if (SDL_GetCurrentTime(&sdl_time)) {
            ns = (uint64_t)sdl_time;
        } else {
            ns = (uint64_t)time(NULL) * 1000000000ULL;
        }
        break;
    }
    default:
        wapi_set_error("Unknown clock ID");
        return WAPI_ERR_INVAL;
    }
    wapi_wasm_write_u64(time_ptr, ns);
    return WAPI_OK;
}

static int32_t host_resolution(wasm_exec_env_t env,
                               int32_t clock_id, uint32_t res_ptr) {
    (void)env;
    uint64_t resolution_ns;
    switch (clock_id) {
    case 0: {
        uint64_t freq = SDL_GetPerformanceFrequency();
        resolution_ns = freq > 0 ? (1000000000ULL / freq) : 1000000;
        if (resolution_ns == 0) resolution_ns = 1;
        break;
    }
    case 1:
        resolution_ns = 1000000;
        break;
    default:
        return WAPI_ERR_INVAL;
    }
    wapi_wasm_write_u64(res_ptr, resolution_ns);
    return WAPI_OK;
}

static int64_t host_perf_counter(wasm_exec_env_t env) {
    (void)env;
    return (int64_t)SDL_GetPerformanceCounter();
}

static int64_t host_perf_frequency(wasm_exec_env_t env) {
    (void)env;
    return (int64_t)SDL_GetPerformanceFrequency();
}

static void host_yield(wasm_exec_env_t env) {
    (void)env;
    SDL_Delay(0);
}

static void host_sleep(wasm_exec_env_t env, int64_t duration_ns) {
    (void)env;
    SDL_DelayNS((uint64_t)duration_ns);
}

static NativeSymbol g_symbols[] = {
    { "time_get",        (void*)host_time_get,        "(ii)i", NULL },
    { "resolution",      (void*)host_resolution,      "(ii)i", NULL },
    { "perf_counter",    (void*)host_perf_counter,    "()I",   NULL },
    { "perf_frequency",  (void*)host_perf_frequency,  "()I",   NULL },
    { "yield",           (void*)host_yield,           "()",    NULL },
    { "sleep",           (void*)host_sleep,           "(I)",   NULL },
};

wapi_cap_registration_t wapi_host_clock_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_clock",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
