/**
 * WAPI - Sensors Capability
 * Version 1.0.0
 *
 * Maps to: Web Sensor API (Accelerometer, Gyroscope, AmbientLight),
 *          CoreMotion (iOS), Android SensorManager
 *
 * Import module: "wapi_sensor"
 *
 * Query availability with wapi_capability_supported("wapi.sensors", 10)
 */

#ifndef WAPI_SENSORS_H
#define WAPI_SENSORS_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum wapi_sensor_type_t {
    WAPI_SENSOR_ACCELEROMETER    = 0,
    WAPI_SENSOR_GYROSCOPE        = 1,
    WAPI_SENSOR_MAGNETOMETER     = 2,
    WAPI_SENSOR_AMBIENT_LIGHT    = 3,
    WAPI_SENSOR_PROXIMITY        = 4,
    WAPI_SENSOR_GRAVITY          = 5,
    WAPI_SENSOR_LINEAR_ACCEL     = 6,
    WAPI_SENSOR_FORCE32          = 0x7FFFFFFF
} wapi_sensor_type_t;

/**
 * 3-axis sensor reading.
 *
 * Layout (32 bytes, align 8):
 *   Offset  0: float64 x
 *   Offset  8: float64 y
 *   Offset 16: float64 z
 *   Offset 24: uint64_t timestamp (nanoseconds)
 */
typedef struct wapi_sensor_xyz_t {
    double      x;
    double      y;
    double      z;
    uint64_t    timestamp;
} wapi_sensor_xyz_t;

/**
 * Scalar sensor reading (ambient light, proximity).
 *
 * Layout (16 bytes, align 8):
 *   Offset 0: float64  value
 *   Offset 8: uint64_t timestamp
 */
typedef struct wapi_sensor_scalar_t {
    double      value;
    uint64_t    timestamp;
} wapi_sensor_scalar_t;

/**
 * Check if a sensor type is available.
 */
WAPI_IMPORT(wapi_sensor, available)
wapi_bool_t wapi_sensor_available(wapi_sensor_type_t type);

/**
 * Start a sensor with the given sampling frequency.
 *
 * @param type      Sensor type.
 * @param freq_hz   Desired sampling rate in Hz.
 * @param sensor    [out] Sensor handle.
 * @return WAPI_OK on success.
 */
WAPI_IMPORT(wapi_sensor, start)
wapi_result_t wapi_sensor_start(wapi_sensor_type_t type, float freq_hz,
                             wapi_handle_t* sensor);

/**
 * Stop and release a sensor.
 */
WAPI_IMPORT(wapi_sensor, stop)
wapi_result_t wapi_sensor_stop(wapi_handle_t sensor);

/**
 * Read the latest 3-axis sensor value (accelerometer, gyroscope, etc.).
 */
WAPI_IMPORT(wapi_sensor, read_xyz)
wapi_result_t wapi_sensor_read_xyz(wapi_handle_t sensor, wapi_sensor_xyz_t* reading);

/**
 * Read the latest scalar sensor value (ambient light, proximity).
 */
WAPI_IMPORT(wapi_sensor, read_scalar)
wapi_result_t wapi_sensor_read_scalar(wapi_handle_t sensor, wapi_sensor_scalar_t* reading);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SENSORS_H */
