/**
 * WAPI - Bluetooth Capability
 * Version 1.0.0
 *
 * Maps to: Web Bluetooth API, CoreBluetooth (iOS/macOS),
 *          Android Bluetooth LE
 *
 * Focuses on Bluetooth Low Energy (BLE) GATT profile.
 *
 * Import module: "wapi_bt"
 *
 * Query availability with wapi_capability_supported("wapi.bluetooth", 12)
 */

#ifndef WAPI_BLUETOOTH_H
#define WAPI_BLUETOOTH_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * BLE scan filter.
 *
 * Layout (32 bytes, align 8):
 *   Offset  0: wapi_string_view_t service_uuid  UUID string (NULL = any)
 *   Offset 16: wapi_string_view_t name_prefix   Device name prefix (NULL = any)
 */
typedef struct wapi_bt_filter_t {
    wapi_string_view_t service_uuid;
    wapi_string_view_t name_prefix;
} wapi_bt_filter_t;

/**
 * Request a BLE device (shows picker dialog).
 *
 * @see WAPI_IO_OP_BT_REQUEST_DEVICE
 * @param filters       Array of scan filters.
 * @param filter_count  Number of filters.
 * @param device        [out] Device handle.
 */
WAPI_IMPORT(wapi_bt, request_device)
wapi_result_t wapi_bt_request_device(const wapi_bt_filter_t* filters,
                                  uint32_t filter_count, wapi_handle_t* device);

/**
 * Connect to a BLE device's GATT server.
 *
 * @see WAPI_IO_OP_BT_CONNECT
 */
WAPI_IMPORT(wapi_bt, connect)
wapi_result_t wapi_bt_connect(wapi_handle_t device);

/**
 * Disconnect from a BLE device.
 */
WAPI_IMPORT(wapi_bt, disconnect)
wapi_result_t wapi_bt_disconnect(wapi_handle_t device);

/**
 * Get a GATT service by UUID.
 *
 * @param device       Device handle.
 * @param uuid         Service UUID string.
 * @param service      [out] Service handle.
 */
WAPI_IMPORT(wapi_bt, get_service)
wapi_result_t wapi_bt_get_service(wapi_handle_t device, wapi_string_view_t uuid,
                               wapi_handle_t* service);

/**
 * Get a GATT characteristic by UUID.
 *
 * @param service        Service handle.
 * @param uuid           Characteristic UUID string.
 * @param characteristic [out] Characteristic handle.
 */
WAPI_IMPORT(wapi_bt, get_characteristic)
wapi_result_t wapi_bt_get_characteristic(wapi_handle_t service, wapi_string_view_t uuid,
                                      wapi_handle_t* characteristic);

/**
 * Read a GATT characteristic value.
 *
 * @see WAPI_IO_OP_BT_READ_VALUE
 */
WAPI_IMPORT(wapi_bt, read_value)
wapi_result_t wapi_bt_read_value(wapi_handle_t characteristic, void* buf,
                              wapi_size_t buf_len, wapi_size_t* val_len);

/**
 * Write a GATT characteristic value.
 *
 * @see WAPI_IO_OP_BT_WRITE_VALUE
 */
WAPI_IMPORT(wapi_bt, write_value)
wapi_result_t wapi_bt_write_value(wapi_handle_t characteristic, const void* data,
                               wapi_size_t len);

/**
 * Subscribe to GATT characteristic notifications.
 *
 * @see WAPI_IO_OP_BT_START_NOTIFICATIONS
 */
WAPI_IMPORT(wapi_bt, start_notifications)
wapi_result_t wapi_bt_start_notifications(wapi_handle_t characteristic);

/**
 * Unsubscribe from notifications.
 */
WAPI_IMPORT(wapi_bt, stop_notifications)
wapi_result_t wapi_bt_stop_notifications(wapi_handle_t characteristic);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_BLUETOOTH_H */
