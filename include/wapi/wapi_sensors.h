/**
 * WAPI - Sensors
 * Version 1.0.0
 *
 * Sensor endpoints are acquired through the role system
 * (WAPI_ROLE_SENSOR) with wapi_sensor_prefs_t specifying type and
 * desired frequency.
 *
 * Import module: "wapi_sensor"
 */

#ifndef WAPI_SENSORS_H
#define WAPI_SENSORS_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum wapi_sensor_type_t {
    WAPI_SENSOR_ACCELEROMETER = 0,
    WAPI_SENSOR_GYROSCOPE     = 1,
    WAPI_SENSOR_MAGNETOMETER  = 2,
    WAPI_SENSOR_AMBIENTLIGHT  = 3,
    WAPI_SENSOR_PROXIMITY     = 4,
    WAPI_SENSOR_GRAVITY       = 5,
    WAPI_SENSOR_LINEARACCEL   = 6,
    WAPI_SENSOR_FORCE32       = 0x7FFFFFFF
} wapi_sensor_type_t;

/**
 * Sensor role-request prefs.
 *
 * Layout (8 bytes, align 4):
 *   Offset 0: uint32_t type           wapi_sensor_type_t
 *   Offset 4: uint32_t freq_hz_bits   f32 frequency as raw bits (0 = default)
 */
typedef struct wapi_sensor_prefs_t {
    uint32_t type;
    uint32_t freq_hz_bits;
} wapi_sensor_prefs_t;

_Static_assert(sizeof(wapi_sensor_prefs_t) == 8, "wapi_sensor_prefs_t must be 8 bytes");

/**
 * Metadata about a resolved sensor endpoint.
 *
 * Layout (24 bytes, align 8):
 *   Offset  0: uint32_t type               wapi_sensor_type_t
 *   Offset  4: uint32_t native_freq_bits   f32 native sample rate
 *   Offset  8: uint8_t  uid[16]
 */
typedef struct wapi_sensor_endpoint_info_t {
    uint32_t type;
    uint32_t native_freq_bits;
    uint8_t  uid[16];
} wapi_sensor_endpoint_info_t;

_Static_assert(sizeof(wapi_sensor_endpoint_info_t) == 24, "wapi_sensor_endpoint_info_t must be 24 bytes");
_Static_assert(_Alignof(wapi_sensor_endpoint_info_t) == 4, "wapi_sensor_endpoint_info_t must be 4-byte aligned");

/** Cheap check: does the host advertise a sensor of this type at all? */
WAPI_IMPORT(wapi_sensor, available)
wapi_bool_t wapi_sensor_available(wapi_sensor_type_t type);

WAPI_IMPORT(wapi_sensor, endpoint_info)
wapi_result_t wapi_sensor_endpoint_info(wapi_handle_t sensor,
                                        wapi_sensor_endpoint_info_t* out,
                                        char* name_buf, wapi_size_t name_buf_len,
                                        wapi_size_t* name_len);

/** Release a granted sensor endpoint. */
WAPI_IMPORT(wapi_sensor, close)
wapi_result_t wapi_sensor_close(wapi_handle_t sensor);

/** 3-axis reading. Layout: double x, y, z, uint64 timestamp (32B). */
typedef struct wapi_sensor_xyz_t {
    double   x;
    double   y;
    double   z;
    uint64_t timestamp;
} wapi_sensor_xyz_t;

/** Scalar reading. Layout: double value, uint64 timestamp (16B). */
typedef struct wapi_sensor_scalar_t {
    double   value;
    uint64_t timestamp;
} wapi_sensor_scalar_t;

WAPI_IMPORT(wapi_sensor, read_xyz)
wapi_result_t wapi_sensor_read_xyz(wapi_handle_t sensor, wapi_sensor_xyz_t* reading);

WAPI_IMPORT(wapi_sensor, read_scalar)
wapi_result_t wapi_sensor_read_scalar(wapi_handle_t sensor, wapi_sensor_scalar_t* reading);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SENSORS_H */
