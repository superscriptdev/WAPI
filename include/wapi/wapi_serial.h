/**
 * WAPI - Serial Port
 * Version 1.0.0
 *
 * Serial port communication for hardware peripherals.
 *
 * Maps to: Web Serial API (Web), OS serial APIs (Desktop)
 *
 * Import module: "wapi_serial"
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
typedef struct wapi_serial_desc_t {
    uint32_t baud_rate;
    uint8_t  data_bits;
    uint8_t  stop_bits;
    uint8_t  parity;          /* wapi_serial_parity_t */
    uint8_t  flow_control;
    uint64_t _pad;
} wapi_serial_desc_t;

_Static_assert(sizeof(wapi_serial_desc_t) == 16,
               "wapi_serial_desc_t must be 16 bytes");
_Static_assert(_Alignof(wapi_serial_desc_t) == 8,
               "wapi_serial_desc_t must be 8-byte aligned");

/* ============================================================
 * Serial Operations (async, submitted via wapi_io_t)
 * ============================================================ */

/** Request access to a serial port (shows picker). */
static inline wapi_result_t wapi_serial_request_port(
    const wapi_io_t* io, wapi_handle_t* out_port, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_SERIAL_PORT_REQUEST;
    op.result_ptr = (uint64_t)(uintptr_t)out_port;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Open a serial port with the given descriptor. */
static inline wapi_result_t wapi_serial_open(
    const wapi_io_t* io, wapi_handle_t port,
    const wapi_serial_desc_t* desc, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_SERIAL_OPEN;
    op.fd        = port;
    op.addr      = (uint64_t)(uintptr_t)desc;
    op.len       = sizeof(*desc);
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Close a serial port. */
static inline wapi_result_t wapi_serial_close(
    const wapi_io_t* io, uint64_t open_user_data)
{
    return io->cancel(io->impl, open_user_data);
}

/** Write data to a serial port. Returns once queued. */
static inline wapi_result_t wapi_serial_write(
    const wapi_io_t* io, wapi_handle_t port,
    const void* data, wapi_size_t data_len, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_SERIAL_WRITE;
    op.fd        = port;
    op.addr      = (uint64_t)(uintptr_t)data;
    op.len       = data_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Read data from a serial port. */
static inline wapi_result_t wapi_serial_read(
    const wapi_io_t* io, wapi_handle_t port,
    void* buf, wapi_size_t buf_len, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_SERIAL_READ;
    op.fd        = port;
    op.addr      = (uint64_t)(uintptr_t)buf;
    op.len       = buf_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Set DTR / RTS signals. No corresponding IO opcode today — this is
 *  a bounded-local flag flip. Kept as a direct sync import because
 *  every platform's serial stack exposes it synchronously. */
WAPI_IMPORT(wapi_serial, set_signals)
wapi_result_t wapi_serial_set_signals(wapi_handle_t port, wapi_bool_t dtr,
                                      wapi_bool_t rts);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SERIAL_H */
