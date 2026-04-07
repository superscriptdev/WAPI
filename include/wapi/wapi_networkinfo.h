/**
 * WAPI - Network Information Capability
 * Version 1.0.0
 *
 * Query network connection type, effective bandwidth, and online status.
 *
 * Maps to: Network Information API (Web), NWPathMonitor (iOS/macOS),
 *          ConnectivityManager (Android), NetworkInformation (Windows)
 *
 * Import module: "wapi_netinfo"
 *
 * Query availability with wapi_capability_supported("wapi.networkinfo", 16)
 */

#ifndef WAPI_NETWORKINFO_H
#define WAPI_NETWORKINFO_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Event Types
 * ============================================================ */

#define WAPI_EVENT_NETWORK_CHANGED 0x1300

/* ============================================================
 * Connection Type
 * ============================================================ */

typedef enum wapi_connection_type_t {
    WAPI_CONNECTION_UNKNOWN      = 0,
    WAPI_CONNECTION_ETHERNET     = 1,
    WAPI_CONNECTION_WIFI         = 2,
    WAPI_CONNECTION_CELLULAR_2G  = 3,
    WAPI_CONNECTION_CELLULAR_3G  = 4,
    WAPI_CONNECTION_CELLULAR_4G  = 5,
    WAPI_CONNECTION_CELLULAR_5G  = 6,
    WAPI_CONNECTION_BLUETOOTH    = 7,
    WAPI_CONNECTION_NONE         = 8,
    WAPI_CONNECTION_FORCE32      = 0x7FFFFFFF
} wapi_connection_type_t;

/* ============================================================
 * Network Info
 * ============================================================
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: uint32_t type           (wapi_connection_type_t)
 *   Offset  4: float    downlink_mbps  Effective downlink in Mbps
 *   Offset  8: uint32_t rtt_ms         Round-trip time in milliseconds
 *   Offset 12: uint32_t save_data      Non-zero if user prefers reduced data
 */

typedef struct wapi_net_info_t {
    uint32_t    type;           /* wapi_connection_type_t */
    float       downlink_mbps;
    uint32_t    rtt_ms;
    uint32_t    save_data;
} wapi_net_info_t;

_Static_assert(sizeof(wapi_net_info_t) == 16,
               "wapi_net_info_t must be 16 bytes");

/* ============================================================
 * Network Info Functions
 * ============================================================ */

/**
 * Get current network connection information.
 *
 * @param info  [out] Network information.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_netinfo, get_info)
wapi_result_t wapi_netinfo_get_info(wapi_net_info_t* info);

/**
 * Check if the device currently has network connectivity.
 *
 * @return Non-zero if online, zero if offline.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_netinfo, is_online)
wapi_bool_t wapi_netinfo_is_online(void);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_NETWORKINFO_H */
