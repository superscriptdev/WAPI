/**
 * WAPI SDL Runtime - Sensors (SDL3 SDL_Sensor)
 */

#include "wapi_host.h"

static SDL_SensorType map_type(uint32_t wapi_type) {
    switch (wapi_type) {
        case 0: return SDL_SENSOR_ACCEL;
        case 1: return SDL_SENSOR_GYRO;
        default: return SDL_SENSOR_UNKNOWN;  /* compass / light / proximity not in SDL3 */
    }
}

static int32_t host_available(wasm_exec_env_t env, int32_t type) {
    (void)env;
    SDL_SensorType st = map_type((uint32_t)type);
    if (st == SDL_SENSOR_UNKNOWN) return 0;
    int count = 0;
    SDL_SensorID* ids = SDL_GetSensors(&count);
    int available = 0;
    if (ids) {
        for (int i = 0; i < count; i++) {
            if (SDL_GetSensorTypeForID(ids[i]) == st) { available = 1; break; }
        }
        SDL_free(ids);
    }
    return available;
}

static int32_t host_start(wasm_exec_env_t env,
                          int32_t type, uint32_t freq_hz_bits,
                          uint32_t out_handle) {
    (void)env; (void)freq_hz_bits;  /* SDL3 sensors don't expose freq control */
    SDL_SensorType st = map_type((uint32_t)type);
    if (st == SDL_SENSOR_UNKNOWN) return WAPI_ERR_NOTSUP;

    int count = 0;
    SDL_SensorID* ids = SDL_GetSensors(&count);
    SDL_Sensor* s = NULL;
    if (ids) {
        for (int i = 0; i < count; i++) {
            if (SDL_GetSensorTypeForID(ids[i]) == st) {
                s = SDL_OpenSensor(ids[i]);
                break;
            }
        }
        SDL_free(ids);
    }
    if (!s) {
        wapi_wasm_write_i32(out_handle, 0);
        return WAPI_ERR_NOENT;
    }
    int32_t h = wapi_handle_alloc(WAPI_HTYPE_SENSOR);
    if (h == 0) { SDL_CloseSensor(s); return WAPI_ERR_NOMEM; }
    g_rt.handles[h].data.sensor = s;
    wapi_wasm_write_i32(out_handle, h);
    return WAPI_OK;
}

static int32_t host_stop(wasm_exec_env_t env, int32_t handle) {
    (void)env;
    if (!wapi_handle_valid(handle, WAPI_HTYPE_SENSOR)) return WAPI_ERR_BADF;
    SDL_CloseSensor(g_rt.handles[handle].data.sensor);
    wapi_handle_free(handle);
    return WAPI_OK;
}

static int32_t host_read_xyz(wasm_exec_env_t env,
                             int32_t handle, uint32_t reading_ptr) {
    (void)env;
    if (!wapi_handle_valid(handle, WAPI_HTYPE_SENSOR)) return WAPI_ERR_BADF;
    SDL_Sensor* s = g_rt.handles[handle].data.sensor;
    float data[3] = {0};
    if (!SDL_GetSensorData(s, data, 3)) return WAPI_ERR_IO;

    uint8_t buf[32] = {0};
    double x = data[0], y = data[1], z = data[2];
    uint64_t ts = SDL_GetTicksNS();
    memcpy(buf + 0,  &x,  8);
    memcpy(buf + 8,  &y,  8);
    memcpy(buf + 16, &z,  8);
    memcpy(buf + 24, &ts, 8);
    return wapi_wasm_write_bytes(reading_ptr, buf, 32) ? WAPI_OK : WAPI_ERR_INVAL;
}

static int32_t host_read_scalar(wasm_exec_env_t env,
                                int32_t handle, uint32_t reading_ptr) {
    (void)env; (void)handle; (void)reading_ptr;
    /* SDL3 doesn't expose scalar sensors (light/proximity). */
    return WAPI_ERR_NOTSUP;
}

static NativeSymbol g_symbols[] = {
    { "available",   (void*)host_available,   "(i)i",  NULL },
    { "start",       (void*)host_start,       "(ifi)i", NULL },
    { "stop",        (void*)host_stop,        "(i)i",  NULL },
    { "read_xyz",    (void*)host_read_xyz,    "(ii)i", NULL },
    { "read_scalar", (void*)host_read_scalar, "(ii)i", NULL },
};

wapi_cap_registration_t wapi_host_sensors_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_sensor",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
