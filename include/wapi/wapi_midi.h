/**
 * WAPI - MIDI
 * Version 1.0.0
 *
 * Maps to: Web MIDI API, CoreMIDI (macOS/iOS),
 *          Android MIDI, ALSA MIDI (Linux)
 *
 * Import module: "wapi_midi"
 */

#ifndef WAPI_MIDI_H
#define WAPI_MIDI_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum wapi_midi_port_type_t {
    WAPI_MIDI_INPUT   = 0,
    WAPI_MIDI_OUTPUT  = 1,
    WAPI_MIDI_FORCE32 = 0x7FFFFFFF
} wapi_midi_port_type_t;

/** Submit a MIDI access request. */
static inline wapi_result_t wapi_midi_request_access(
    const wapi_io_t* io, wapi_bool_t sysex, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_MIDI_ACCESS_REQUEST;
    op.flags     = sysex ? 1 : 0;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Bounded-local: the number of known ports of the given type. */
WAPI_IMPORT(wapi_midi, port_count)
int32_t wapi_midi_port_count(wapi_midi_port_type_t type);

/** Bounded-local: the UTF-8 name of a known port. */
WAPI_IMPORT(wapi_midi, port_name)
wapi_result_t wapi_midi_port_name(wapi_midi_port_type_t type, int32_t index,
                               char* buf, wapi_size_t buf_len, wapi_size_t* name_len);

/** Submit an open-port request. */
static inline wapi_result_t wapi_midi_open_port(
    const wapi_io_t* io, wapi_midi_port_type_t type, int32_t index,
    wapi_handle_t* out_port, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_MIDI_PORT_OPEN;
    op.flags      = (uint32_t)type;
    op.flags2     = (uint32_t)index;
    op.result_ptr = (uint64_t)(uintptr_t)out_port;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Bounded-local on an owned handle. */
WAPI_IMPORT(wapi_midi, close_port)
wapi_result_t wapi_midi_close_port(wapi_handle_t port);

/** Bounded-local: enqueue bytes to an open output port. */
WAPI_IMPORT(wapi_midi, send)
wapi_result_t wapi_midi_send(wapi_handle_t port, const uint8_t* data, wapi_size_t len);

/** Bounded-local: drain from the in-port's rx queue (may return AGAIN). */
WAPI_IMPORT(wapi_midi, recv)
wapi_result_t wapi_midi_recv(wapi_handle_t port, uint8_t* buf, wapi_size_t buf_len,
                          wapi_size_t* msg_len, wapi_timestamp_t* timestamp);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_MIDI_H */
