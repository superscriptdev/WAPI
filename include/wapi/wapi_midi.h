/**
 * WAPI - MIDI Capability
 * Version 1.0.0
 *
 * Maps to: Web MIDI API, CoreMIDI (macOS/iOS),
 *          Android MIDI, ALSA MIDI (Linux)
 *
 * Import module: "wapi_midi"
 *
 * Query availability with wapi_capability_supported("wapi.midi", 7)
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

/**
 * Request MIDI access (may show permission prompt).
 *
 * @param sysex  If true, request SysEx message access.
 * @return WAPI_OK if granted.
 */
WAPI_IMPORT(wapi_midi, request_access)
wapi_result_t wapi_midi_request_access(wapi_bool_t sysex);

/**
 * Get the number of available MIDI ports.
 *
 * @param type  WAPI_MIDI_INPUT or WAPI_MIDI_OUTPUT.
 * @return Number of ports.
 */
WAPI_IMPORT(wapi_midi, port_count)
int32_t wapi_midi_port_count(wapi_midi_port_type_t type);

/**
 * Get MIDI port name.
 */
WAPI_IMPORT(wapi_midi, port_name)
wapi_result_t wapi_midi_port_name(wapi_midi_port_type_t type, int32_t index,
                               char* buf, wapi_size_t buf_len, wapi_size_t* name_len);

/**
 * Open a MIDI port.
 *
 * @param type   Input or output.
 * @param index  Port index.
 * @param port   [out] Port handle.
 */
WAPI_IMPORT(wapi_midi, open_port)
wapi_result_t wapi_midi_open_port(wapi_midi_port_type_t type, int32_t index,
                               wapi_handle_t* port);

/**
 * Close a MIDI port.
 */
WAPI_IMPORT(wapi_midi, close_port)
wapi_result_t wapi_midi_close_port(wapi_handle_t port);

/**
 * Send MIDI data to an output port.
 *
 * @param port  Output port handle.
 * @param data  MIDI message bytes.
 * @param len   Message length.
 */
WAPI_IMPORT(wapi_midi, send)
wapi_result_t wapi_midi_send(wapi_handle_t port, const uint8_t* data, wapi_size_t len);

/**
 * Receive MIDI data from an input port.
 *
 * @param port       Input port handle.
 * @param buf        Buffer for MIDI message.
 * @param buf_len    Buffer capacity.
 * @param msg_len    [out] Actual message length.
 * @param timestamp  [out] Message timestamp (nanoseconds).
 * @return WAPI_OK on success, WAPI_ERR_AGAIN if no message.
 */
WAPI_IMPORT(wapi_midi, recv)
wapi_result_t wapi_midi_recv(wapi_handle_t port, uint8_t* buf, wapi_size_t buf_len,
                          wapi_size_t* msg_len, wapi_timestamp_t* timestamp);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_MIDI_H */
