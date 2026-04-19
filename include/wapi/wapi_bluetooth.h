/**
 * WAPI - Bluetooth
 * Version 1.0.0
 *
 * Maps to: Web Bluetooth API, CoreBluetooth (iOS/macOS),
 *          Android Bluetooth LE
 *
 * Focuses on Bluetooth Low Energy (BLE) GATT profile.
 *
 * Import module: "wapi_bt"
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
 *   Offset  0: wapi_stringview_t service_uuid  UUID string (NULL = any)
 *   Offset 16: wapi_stringview_t name_prefix   Device name prefix (NULL = any)
 */
typedef struct wapi_bt_filter_t {
    wapi_stringview_t service_uuid;
    wapi_stringview_t name_prefix;
} wapi_bt_filter_t;

/* ============================================================
 * Bluetooth Operations (async, submitted via wapi_io_t)
 * ============================================================ */

/** Request a BLE device (shows system picker). */
static inline wapi_result_t wapi_bt_request_device(
    const wapi_io_t* io,
    const wapi_bt_filter_t* filters, uint32_t filter_count,
    wapi_handle_t* out_device, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_BT_DEVICE_REQUEST;
    op.addr       = (uint64_t)(uintptr_t)filters;
    op.len        = (uint64_t)filter_count * sizeof(wapi_bt_filter_t);
    op.flags2     = filter_count;
    op.result_ptr = (uint64_t)(uintptr_t)out_device;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Connect to a BLE device's GATT server. */
static inline wapi_result_t wapi_bt_connect(
    const wapi_io_t* io, wapi_handle_t device, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_BT_CONNECT;
    op.fd        = device;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Get a GATT service by UUID. */
static inline wapi_result_t wapi_bt_get_service(
    const wapi_io_t* io, wapi_handle_t device,
    wapi_stringview_t uuid, wapi_handle_t* out_service,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_BT_SERVICE_GET;
    op.fd         = device;
    op.addr       = uuid.data;
    op.len        = uuid.length;
    op.result_ptr = (uint64_t)(uintptr_t)out_service;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Get a GATT characteristic by UUID. */
static inline wapi_result_t wapi_bt_get_characteristic(
    const wapi_io_t* io, wapi_handle_t service,
    wapi_stringview_t uuid, wapi_handle_t* out_char,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_BT_CHARACTERISTIC_GET;
    op.fd         = service;
    op.addr       = uuid.data;
    op.len        = uuid.length;
    op.result_ptr = (uint64_t)(uintptr_t)out_char;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Read a GATT characteristic value. */
static inline wapi_result_t wapi_bt_read_value(
    const wapi_io_t* io, wapi_handle_t characteristic,
    void* buf, wapi_size_t buf_len, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_BT_VALUE_READ;
    op.fd        = characteristic;
    op.addr      = (uint64_t)(uintptr_t)buf;
    op.len       = buf_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Write a GATT characteristic value. */
static inline wapi_result_t wapi_bt_write_value(
    const wapi_io_t* io, wapi_handle_t characteristic,
    const void* data, wapi_size_t len, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_BT_VALUE_WRITE;
    op.fd        = characteristic;
    op.addr      = (uint64_t)(uintptr_t)data;
    op.len       = len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Subscribe to GATT characteristic notifications. Host emits a
 *  WAPI_EVENT_IO_COMPLETION per notification (WAPI_IO_CQE_F_MORE set). */
static inline wapi_result_t wapi_bt_start_notifications(
    const wapi_io_t* io, wapi_handle_t characteristic, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_BT_NOTIFICATIONS_START;
    op.fd        = characteristic;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Unsubscribe — cancels the notification subscription by user_data. */
static inline wapi_result_t wapi_bt_stop_notifications(
    const wapi_io_t* io, uint64_t subscription_user_data)
{
    return io->cancel(io->impl, subscription_user_data);
}

/** Disconnect — cancels the device's ongoing connection by user_data. */
static inline wapi_result_t wapi_bt_disconnect(
    const wapi_io_t* io, uint64_t connect_user_data)
{
    return io->cancel(io->impl, connect_user_data);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_BLUETOOTH_H */
