/**
 * WAPI Desktop Runtime - Clocks and Timers
 *
 * Implements: wapi_clock.time_get, wapi_clock.resolution,
 *             wapi_clock.perf_counter, wapi_clock.perf_frequency,
 *             wapi_clock.yield, wapi_clock.sleep
 *
 * Uses SDL3 for high-resolution timing.
 */

#include "wapi_host.h"

/* ============================================================
 * Clock Functions
 * ============================================================ */

/* time_get: (i32 clock_id, i32 time_ptr) -> i32 */
static wasm_trap_t* host_clock_time_get(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t clock_id = WAPI_ARG_I32(0);
    uint32_t time_ptr = WAPI_ARG_U32(1);

    uint64_t ns;

    switch (clock_id) {
    case 0: /* WAPI_CLOCK_MONOTONIC */
        ns = SDL_GetTicksNS();
        break;
    case 1: { /* WAPI_CLOCK_REALTIME */
        /* SDL_GetRealtimeClockNS (SDL3 provides this) */
        /* Fallback: get ticks + estimate. SDL3 has SDL_GetCurrentTime. */
        SDL_Time sdl_time;
        if (SDL_GetCurrentTime(&sdl_time)) {
            ns = (uint64_t)sdl_time;
        } else {
            /* Fallback using C time */
            ns = (uint64_t)time(NULL) * 1000000000ULL;
        }
        break;
    }
    default:
        wapi_set_error("Unknown clock ID");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    wapi_wasm_write_u64(time_ptr, ns);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* resolution: (i32 clock_id, i32 resolution_ptr) -> i32 */
static wasm_trap_t* host_clock_resolution(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t clock_id = WAPI_ARG_I32(0);
    uint32_t res_ptr = WAPI_ARG_U32(1);

    uint64_t resolution_ns;

    switch (clock_id) {
    case 0: /* WAPI_CLOCK_MONOTONIC */
        /* SDL performance counter gives us the resolution */
        {
            uint64_t freq = SDL_GetPerformanceFrequency();
            if (freq > 0) {
                resolution_ns = 1000000000ULL / freq;
                if (resolution_ns == 0) resolution_ns = 1;
            } else {
                resolution_ns = 1000000; /* 1ms fallback */
            }
        }
        break;
    case 1: /* WAPI_CLOCK_REALTIME */
        resolution_ns = 1000000; /* 1ms typical wall clock resolution */
        break;
    default:
        wapi_set_error("Unknown clock ID");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    wapi_wasm_write_u64(res_ptr, resolution_ns);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* perf_counter: () -> i64 */
static wasm_trap_t* host_clock_perf_counter(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    uint64_t counter = SDL_GetPerformanceCounter();
    results[0].kind = WASMTIME_I64;
    results[0].of.i64 = (int64_t)counter;
    return NULL;
}

/* perf_frequency: () -> i64 */
static wasm_trap_t* host_clock_perf_frequency(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    uint64_t freq = SDL_GetPerformanceFrequency();
    results[0].kind = WASMTIME_I64;
    results[0].of.i64 = (int64_t)freq;
    return NULL;
}

/* yield: () -> void */
static wasm_trap_t* host_yield(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    SDL_Delay(0);
    return NULL;
}

/* sleep: (i64 duration_ns) -> void */
static wasm_trap_t* host_sleep(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint64_t duration_ns = WAPI_ARG_U64(0);
    SDL_DelayNS(duration_ns);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_clock(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_clock", "time_get",   host_clock_time_get);
    WAPI_DEFINE_2_1(linker, "wapi_clock", "resolution",  host_clock_resolution);

    /* perf_counter: () -> i64 */
    wapi_linker_define(linker, "wapi_clock", "perf_counter", host_clock_perf_counter,
                     0, NULL, 1, (wasm_valkind_t[]){WASM_I64});

    /* perf_frequency: () -> i64 */
    wapi_linker_define(linker, "wapi_clock", "perf_frequency", host_clock_perf_frequency,
                     0, NULL, 1, (wasm_valkind_t[]){WASM_I64});

    /* yield: () -> void */
    WAPI_DEFINE_0_0(linker, "wapi_clock", "yield", host_yield);

    /* sleep: (i64) -> void */
    wapi_linker_define(linker, "wapi_clock", "sleep", host_sleep,
                     1, (wasm_valkind_t[]){WASM_I64}, 0, NULL);
}
