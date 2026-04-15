/**
 * WAPI - Geolocation Capability
 * Version 1.0.0
 *
 * Maps to: Web Geolocation API, CoreLocation (iOS/macOS),
 *          Android LocationManager
 *
 * Import module: "wapi_geo"
 *
 * Query availability with wapi_capability_supported("wapi.geolocation", 16)
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
 * Request the current position (one-shot).
 *
 * @param flags     Accuracy flags.
 * @param timeout_ms  Maximum wait time in milliseconds.
 * @param position  [out] Position result.
 * @return WAPI_OK on success, WAPI_ERR_TIMEDOUT, WAPI_ERR_ACCES (denied).
 */
WAPI_IMPORT(wapi_geo, get_position)
wapi_result_t wapi_geo_get_position(wapi_flags_t flags, uint32_t timeout_ms,
                                 wapi_geo_position_t* position);

/**
 * Start watching position changes.
 * Positions are delivered as WAPI_IO_OP completions on the I/O queue.
 *
 * @param flags   Accuracy flags.
 * @param watch   [out] Watch handle (use wapi_geo_clear_watch to stop).
 * @return WAPI_OK on success.
 */
WAPI_IMPORT(wapi_geo, watch_position)
wapi_result_t wapi_geo_watch_position(wapi_flags_t flags, wapi_handle_t* watch);

/**
 * Stop watching position changes.
 */
WAPI_IMPORT(wapi_geo, clear_watch)
wapi_result_t wapi_geo_clear_watch(wapi_handle_t watch);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_GEOLOCATION_H */
