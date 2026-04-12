/**
 * WAPI - Serial Port Capability
 * Version 1.0.0
 *
 * Serial port communication for hardware peripherals.
 *
 * Maps to: Web Serial API (Web), OS serial APIs (Desktop)
 *
 * Import module: "wapi_serial"
 *
 * Query availability with wapi_capability_supported("wapi.serial", 10)
 */

#ifndef WAPI_SERIAL_H
#define WAPI_SERIAL_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Serial Types
 * ============================================================ */

typedef enum wapi_serial_parity_t {
    WAPI_SERIAL_PARITY_NONE    = 0,
    WAPI_SERIAL_PARITY_EVEN    = 1,
    WAPI_SERIAL_PARITY_ODD     = 2,
    WAPI_SERIAL_PARITY_FORCE32 = 0x7FFFFFFF
} wapi_serial_parity_t;

/**
 * Serial port configuration.
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: uint32_t baud_rate
 *   Offset  4: uint8_t  data_bits
 *   Offset  5: uint8_t  stop_bits
 *   Offset  6: uint8_t  parity         (wapi_serial_parity_t)
 *   Offset  7: uint8_t  flow_control
 *   Offset  8: uint64_t _pad
 */
typedef struct wapi_serial_config_t {
    uint32_t baud_rate;
    uint8_t  data_bits;
    uint8_t  stop_bits;
    uint8_t  parity;          /* wapi_serial_parity_t */
    uint8_t  flow_control;
    uint64_t _pad;
} wapi_serial_config_t;

_Static_assert(sizeof(wapi_serial_config_t) == 16,
               "wapi_serial_config_t must be 16 bytes");
_Static_assert(_Alignof(wapi_serial_config_t) == 8,
               "wapi_serial_config_t must be 8-byte aligned");

/* ============================================================
 * Serial Functions
 * ============================================================ */

/**
 * Request access to a serial port (shows permission prompt).
 *
 * @see WAPI_IO_OP_SERIAL_REQUEST_PORT
 * @param port  [out] Serial port handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_serial, request_port)
wapi_result_t wapi_serial_request_port(wapi_handle_t* port);

/**
 * Open a serial port with the given configuration.
 *
 * @see WAPI_IO_OP_SERIAL_OPEN
 * @param port    Serial port handle.
 * @param config  Port configuration (baud rate, data bits, etc.).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_serial, open)
wapi_result_t wapi_serial_open(wapi_handle_t port,
                               const wapi_serial_config_t* config);

/**
 * Close a serial port.
 *
 * @param port  Serial port handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_serial, close)
wapi_result_t wapi_serial_close(wapi_handle_t port);

/**
 * Write data to the serial port.
 *
 * @param port      Serial port handle.
 * @param data      Pointer to data to write.
 * @param data_len  Length of the data.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_serial, write)
wapi_result_t wapi_serial_write(wapi_handle_t port, const void* data,
                                wapi_size_t data_len);

/**
 * Read data from the serial port.
 *
 * @param port        Serial port handle.
 * @param buf         Buffer to receive data.
 * @param buf_len     Size of the buffer.
 * @param bytes_read  [out] Actual number of bytes read.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_serial, read)
wapi_result_t wapi_serial_read(wapi_handle_t port, void* buf,
                               wapi_size_t buf_len, wapi_size_t* bytes_read);

/**
 * Set serial control signals.
 *
 * @param port  Serial port handle.
 * @param dtr   Data Terminal Ready signal (0 or 1).
 * @param rts   Request To Send signal (0 or 1).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_serial, set_signals)
wapi_result_t wapi_serial_set_signals(wapi_handle_t port, wapi_bool_t dtr,
                                      wapi_bool_t rts);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SERIAL_H */
