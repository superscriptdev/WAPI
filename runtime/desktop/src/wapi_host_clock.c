/**
 * WAPI Desktop Runtime - Clocks and Timers
 *
 * Delegates to wapi_plat.h (platform backend).
 */

#include "wapi_host.h"

static wasm_trap_t* host_clock_time_get(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t clock_id = WAPI_ARG_I32(0);
    uint32_t time_ptr = WAPI_ARG_U32(1);
    uint64_t ns;
    switch (clock_id) {
    case 0: ns = wapi_plat_time_monotonic_ns(); break;
    case 1: ns = wapi_plat_time_realtime_ns();  break;
    default:
        wapi_set_error("Unknown clock ID");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }
    wapi_wasm_write_u64(time_ptr, ns);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_clock_resolution(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t clock_id = WAPI_ARG_I32(0);
    uint32_t res_ptr = WAPI_ARG_U32(1);
    uint64_t r;
    switch (clock_id) {
    case 0: r = wapi_plat_time_resolution_monotonic_ns(); break;
    case 1: r = 1000000; break; /* 1ms typical wall clock */
    default:
        wapi_set_error("Unknown clock ID");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }
    wapi_wasm_write_u64(res_ptr, r);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_clock_perf_counter(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I64(wapi_plat_perf_counter());
    return NULL;
}

static wasm_trap_t* host_clock_perf_frequency(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I64(wapi_plat_perf_frequency());
    return NULL;
}

static wasm_trap_t* host_yield(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)results; (void)nresults;
    wapi_plat_yield();
    return NULL;
}

static wasm_trap_t* host_sleep(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)results; (void)nresults;
    wapi_plat_sleep_ns(WAPI_ARG_U64(0));
    return NULL;
}

void wapi_host_register_clock(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_clock", "time_get",   host_clock_time_get);
    WAPI_DEFINE_2_1(linker, "wapi_clock", "resolution", host_clock_resolution);
    wapi_linker_define(linker, "wapi_clock", "perf_counter", host_clock_perf_counter,
                       0, NULL, 1, (wasm_valkind_t[]){WASM_I64});
    wapi_linker_define(linker, "wapi_clock", "perf_frequency", host_clock_perf_frequency,
                       0, NULL, 1, (wasm_valkind_t[]){WASM_I64});
    WAPI_DEFINE_0_0(linker, "wapi_clock", "yield", host_yield);
    wapi_linker_define(linker, "wapi_clock", "sleep", host_sleep,
                       1, (wasm_valkind_t[]){WASM_I64}, 0, NULL);
}
