/**
 * WAPI - Geolocation
 * Version 1.0.0
 *
 * Maps to: Web Geolocation API, CoreLocation (iOS/macOS),
 *          Android LocationManager
 *
 * Import module: "wapi_geo"
 */

#ifndef WAPI_GEOLOCATION_H
#define WAPI_GEOLOCATION_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Geolocation position.
 *
 * Layout (48 bytes, align 8):
 *   Offset  0: float64 latitude    Decimal degrees
 *   Offset  8: float64 longitude   Decimal degrees
 *   Offset 16: float64 altitude    Meters (NaN if unavailable)
 *   Offset 24: float64 accuracy    Meters (horizontal)
 *   Offset 32: float64 alt_accuracy Meters (NaN if unavailable)
 *   Offset 40: float64 heading     Degrees from north (NaN if unavailable)
 */
typedef struct wapi_geo_position_t {
    double  latitude;
    double  longitude;
    double  altitude;
    double  accuracy;
    double  altitude_accuracy;
    double  heading;
} wapi_geo_position_t;

/* Accuracy hint */
#define WAPI_GEO_HIGHACCURACY  0x0001
#define WAPI_GEO_LOWPOWER      0x0002

/**
 * Submit a one-shot position request. Completion's payload[0..47]
 * inlines the 48-byte wapi_geo_position_t (WAPI_IO_CQE_F_INLINE set),
 * or result carries a negative error (WAPI_ERR_ACCES / WAPI_ERR_TIMEDOUT).
 */
static inline wapi_result_t wapi_geo_get_position(
    const wapi_io_t* io, wapi_flags_t flags, uint32_t timeout_ms,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_GEO_POSITION_GET;
    op.flags     = flags;
    op.offset    = (uint64_t)timeout_ms;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/**
 * Submit a watch-position request. The host emits one completion
 * per reading (flags include WAPI_IO_CQE_F_MORE until the watch is
 * cleared). *out_watch receives the watch handle for clear_watch.
 */
static inline wapi_result_t wapi_geo_watch_position(
    const wapi_io_t* io, wapi_flags_t flags,
    wapi_handle_t* out_watch, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_GEO_POSITION_WATCH;
    op.flags      = flags;
    op.result_ptr = (uint64_t)(uintptr_t)out_watch;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/**
 * Stop watching position changes. Bounded-local (cancels an existing
 * subscription); uses io->cancel keyed on the watch's user_data.
 */
static inline wapi_result_t wapi_geo_clear_watch(
    const wapi_io_t* io, uint64_t watch_user_data)
{
    return io->cancel(io->impl, watch_user_data);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_GEOLOCATION_H */
