/**
 * WAPI - USB
 * Version 1.0.0
 *
 * Maps to: WebUSB API, libusb, OS USB APIs
 *
 * Import module: "wapi_usb"
 */

#ifndef WAPI_USB_H
#define WAPI_USB_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB device info.
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: uint16_t vendor_id
 *   Offset  2: uint16_t product_id
 *   Offset  4: uint8_t  device_class
 *   Offset  5: uint8_t  device_subclass
 *   Offset  6: uint8_t  device_protocol
 *   Offset  7: uint8_t  _pad
 *   Offset  8: uint32_t serial_number_offset  (into name buffer)
 *   Offset 12: uint32_t serial_number_len
 */
typedef struct wapi_usb_device_info_t {
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t  device_class;
    uint8_t  device_subclass;
    uint8_t  device_protocol;
    uint8_t  _pad;
    uint32_t serial_number_offset;
    uint32_t serial_number_len;
} wapi_usb_device_info_t;

/**
 * USB device filter for requesting access.
 */
typedef struct wapi_usb_filter_t {
    uint16_t vendor_id;       /* 0 = any */
    uint16_t product_id;      /* 0 = any */
    uint8_t  device_class;    /* 0xFF = any */
    uint8_t  _pad[3];
} wapi_usb_filter_t;

/* ============================================================
 * USB Operations (async, submitted via wapi_io_t)
 *
 * close() and release_interface() are bounded-local (act on an
 * already-owned handle). The host exposes them as io->cancel targets
 * keyed on the open / claim user_data.
 * ============================================================ */

/** Request USB device access (shows picker). */
static inline wapi_result_t wapi_usb_request_device(
    const wapi_io_t* io,
    const wapi_usb_filter_t* filters, uint32_t filter_count,
    wapi_handle_t* out_device, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_USB_DEVICE_REQUEST;
    op.addr       = (uint64_t)(uintptr_t)filters;
    op.len        = (uint64_t)filter_count * sizeof(wapi_usb_filter_t);
    op.flags2     = filter_count;
    op.result_ptr = (uint64_t)(uintptr_t)out_device;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Open a USB device. */
static inline wapi_result_t wapi_usb_open(
    const wapi_io_t* io, wapi_handle_t device, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_USB_OPEN;
    op.fd        = device;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Close a USB device. */
static inline wapi_result_t wapi_usb_close(
    const wapi_io_t* io, uint64_t open_user_data)
{
    return io->cancel(io->impl, open_user_data);
}

/** Claim a USB interface. */
static inline wapi_result_t wapi_usb_claim_interface(
    const wapi_io_t* io, wapi_handle_t device, uint8_t interface_num,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_USB_INTERFACE_CLAIM;
    op.fd        = device;
    op.flags     = interface_num;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Release a claimed interface. */
static inline wapi_result_t wapi_usb_release_interface(
    const wapi_io_t* io, uint64_t claim_user_data)
{
    return io->cancel(io->impl, claim_user_data);
}

/** Bulk/interrupt transfer in. */
static inline wapi_result_t wapi_usb_transfer_in(
    const wapi_io_t* io, wapi_handle_t device, uint8_t endpoint,
    void* buf, wapi_size_t len, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_USB_TRANSFER_IN;
    op.fd        = device;
    op.flags     = endpoint;
    op.addr      = (uint64_t)(uintptr_t)buf;
    op.len       = len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Bulk/interrupt transfer out. */
static inline wapi_result_t wapi_usb_transfer_out(
    const wapi_io_t* io, wapi_handle_t device, uint8_t endpoint,
    const void* buf, wapi_size_t len, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_USB_TRANSFER_OUT;
    op.fd        = device;
    op.flags     = endpoint;
    op.addr      = (uint64_t)(uintptr_t)buf;
    op.len       = len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Control transfer. The request_type / request / value / index
 *  fields are packed into op.offset as:
 *      bits  0-7  request_type
 *      bits  8-15 request
 *      bits 16-31 value
 *      bits 32-47 index
 */
static inline wapi_result_t wapi_usb_control_transfer(
    const wapi_io_t* io, wapi_handle_t device,
    uint8_t request_type, uint8_t request,
    uint16_t value, uint16_t index,
    void* buf, wapi_size_t len, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_USB_CONTROL_TRANSFER;
    op.fd        = device;
    op.offset    = ((uint64_t)request_type        <<  0)
                 | ((uint64_t)request             <<  8)
                 | ((uint64_t)value               << 16)
                 | ((uint64_t)index               << 32);
    op.addr      = (uint64_t)(uintptr_t)buf;
    op.len       = len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_USB_H */
