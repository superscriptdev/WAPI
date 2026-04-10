/**
 * WAPI - Clocks and Timers
 * Version 1.0.0
 *
 * Monotonic and wall clocks, plus timer scheduling.
 * Modeled on WASI Preview 1 clocks with simplification.
 *
 * Import module: "wapi_clock"
 */

#ifndef WAPI_CLOCK_H
#define WAPI_CLOCK_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Clock IDs
 * ============================================================ */

typedef enum wapi_clock_id_t {
    /**
     * Monotonic clock. Never decreases, not affected by system
     * time changes. Use for measuring intervals and timeouts.
     * Epoch is arbitrary (typically process start).
     */
    WAPI_CLOCK_MONOTONIC = 0,

    /**
     * Wall clock (realtime). Nanoseconds since Unix epoch
     * (1970-01-01T00:00:00Z). May be adjusted by the system.
     * Use for timestamps and display.
     */
    WAPI_CLOCK_REALTIME = 1,

    WAPI_CLOCK_FORCE32 = 0x7FFFFFFF
} wapi_clock_id_t;

/* ============================================================
 * Clock Functions
 * ============================================================ */

/**
 * Get the current time from a clock.
 *
 * @param clock_id  Which clock to query.
 * @param time      [out] Current time in nanoseconds.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_clock, time_get)
wapi_result_t wapi_clock_time_get(wapi_clock_id_t clock_id, wapi_timestamp_t* time);

/**
 * Get the resolution (smallest measurable interval) of a clock.
 *
 * @param clock_id    Which clock to query.
 * @param resolution  [out] Resolution in nanoseconds.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_clock, resolution)
wapi_result_t wapi_clock_resolution(wapi_clock_id_t clock_id, wapi_timestamp_t* resolution);

/* ============================================================
 * Performance Counter
 * ============================================================
 * High-resolution counter for frame timing and profiling.
 */

/**
 * Get the current value of a high-resolution performance counter.
 * The counter is monotonically increasing and has the highest
 * resolution available on the platform.
 *
 * Wasm signature: () -> i64
 */
WAPI_IMPORT(wapi_clock, perf_counter)
uint64_t wapi_clock_perf_counter(void);

/**
 * Get the frequency of the performance counter in Hz
 * (ticks per second).
 *
 * Wasm signature: () -> i64
 */
WAPI_IMPORT(wapi_clock, perf_frequency)
uint64_t wapi_clock_perf_frequency(void);

/* ============================================================
 * Yield / Sleep
 * ============================================================ */

/**
 * Yield the current execution slice. Hint to the host that the
 * module has no immediate work. Equivalent to sched_yield().
 *
 * Wasm signature: () -> void
 */
WAPI_IMPORT(wapi_clock, yield)
void wapi_yield(void);

/**
 * Sleep for a specified duration.
 * For non-blocking waits, submit a WAPI_IO_OP_TIMEOUT via the wapi_io_t vtable instead.
 *
 * @param duration_ns  Duration in nanoseconds.
 *
 * Wasm signature: (i64) -> void
 */
WAPI_IMPORT(wapi_clock, sleep)
void wapi_sleep(uint64_t duration_ns);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CLOCK_H */
