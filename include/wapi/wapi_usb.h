/**
 * WAPI - USB Capability
 * Version 1.0.0
 *
 * Maps to: WebUSB API, libusb, OS USB APIs
 *
 * Import module: "wapi_usb"
 *
 * Query availability with wapi_capability_supported("wapi.usb", 6)
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

/**
 * Request access to a USB device (shows permission prompt).
 *
 * @see WAPI_IO_OP_USB_DEVICE_REQUEST
 * @param filters      Array of filters (NULL = any device).
 * @param filter_count Number of filters.
 * @param device       [out] USB device handle.
 */
WAPI_IMPORT(wapi_usb, request_device)
wapi_result_t wapi_usb_request_device(const wapi_usb_filter_t* filters,
                                   uint32_t filter_count, wapi_handle_t* device);

/**
 * Open a USB device for I/O.
 *
 * @see WAPI_IO_OP_USB_OPEN
 */
WAPI_IMPORT(wapi_usb, open)
wapi_result_t wapi_usb_open(wapi_handle_t device);

/**
 * Close a USB device.
 */
WAPI_IMPORT(wapi_usb, close)
wapi_result_t wapi_usb_close(wapi_handle_t device);

/**
 * Claim a USB interface.
 *
 * @see WAPI_IO_OP_USB_INTERFACE_CLAIM
 */
WAPI_IMPORT(wapi_usb, claim_interface)
wapi_result_t wapi_usb_claim_interface(wapi_handle_t device, uint8_t interface_num);

/**
 * Release a USB interface.
 */
WAPI_IMPORT(wapi_usb, release_interface)
wapi_result_t wapi_usb_release_interface(wapi_handle_t device, uint8_t interface_num);

/**
 * Bulk/interrupt transfer in.
 *
 * @see WAPI_IO_OP_USB_TRANSFER_IN
 */
WAPI_IMPORT(wapi_usb, transfer_in)
wapi_result_t wapi_usb_transfer_in(wapi_handle_t device, uint8_t endpoint,
                                void* buf, wapi_size_t len, wapi_size_t* transferred);

/**
 * Bulk/interrupt transfer out.
 *
 * @see WAPI_IO_OP_USB_TRANSFER_OUT
 */
WAPI_IMPORT(wapi_usb, transfer_out)
wapi_result_t wapi_usb_transfer_out(wapi_handle_t device, uint8_t endpoint,
                                 const void* buf, wapi_size_t len,
                                 wapi_size_t* transferred);

/**
 * Control transfer.
 *
 * @see WAPI_IO_OP_USB_CONTROL_TRANSFER
 */
WAPI_IMPORT(wapi_usb, control_transfer)
wapi_result_t wapi_usb_control_transfer(wapi_handle_t device,
                                     uint8_t request_type, uint8_t request,
                                     uint16_t value, uint16_t index,
                                     void* buf, wapi_size_t len,
                                     wapi_size_t* transferred);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_USB_H */
