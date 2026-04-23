/**
 * WAPI - MIDI
 * Version 1.0.0
 *
 * MIDI endpoints are acquired through the role system
 * (WAPI_ROLE_MIDI_INPUT / WAPI_ROLE_MIDI_OUTPUT). This header
 * owns the sysex prefs, endpoint_info, and the send/recv surface.
 *
 * Import module: "wapi_midi"
 */

#ifndef WAPI_MIDI_H
#define WAPI_MIDI_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum wapi_midi_flags_t {
    WAPI_MIDI_SYSEX        = 1 << 0, /* request sysex access */
    WAPI_MIDI_FLAGS_FORCE32 = 0x7FFFFFFF
} wapi_midi_flags_t;

/**
 * MIDI role-request prefs.
 *
 * Layout (4 bytes, align 4):
 *   Offset 0: uint32_t flags  (wapi_midi_flags_t bitmask)
 */
typedef struct wapi_midi_prefs_t {
    uint32_t flags;
} wapi_midi_prefs_t;

_Static_assert(sizeof(wapi_midi_prefs_t) == 4, "wapi_midi_prefs_t must be 4 bytes");

/**
 * Metadata about a resolved MIDI endpoint.
 *
 * Layout (24 bytes, align 8):
 *   Offset  0: uint32_t manufacturer_id  (MMA SysEx ID, 0 if unknown)
 *   Offset  4: uint32_t flags            (echoes granted flags, e.g. SYSEX)
 *   Offset  8: uint8_t  uid[16]
 */
typedef struct wapi_midi_endpoint_info_t {
    uint32_t manufacturer_id;
    uint32_t flags;
    uint8_t  uid[16];
} wapi_midi_endpoint_info_t;

_Static_assert(sizeof(wapi_midi_endpoint_info_t) == 24, "wapi_midi_endpoint_info_t must be 24 bytes");
_Static_assert(_Alignof(wapi_midi_endpoint_info_t) == 4, "wapi_midi_endpoint_info_t must be 4-byte aligned");

/** Query metadata for a granted MIDI endpoint. */
WAPI_IMPORT(wapi_midi, endpoint_info)
wapi_result_t wapi_midi_endpoint_info(wapi_handle_t port,
                                      wapi_midi_endpoint_info_t* out,
                                      char* name_buf, wapi_size_t name_buf_len,
                                      wapi_size_t* name_len);

/** Close a granted MIDI endpoint. */
WAPI_IMPORT(wapi_midi, close)
wapi_result_t wapi_midi_close(wapi_handle_t port);

/** Enqueue bytes to an output endpoint. */
WAPI_IMPORT(wapi_midi, send)
wapi_result_t wapi_midi_send(wapi_handle_t port, const uint8_t* data, wapi_size_t len);

/** Drain messages from an input endpoint's rx queue. Returns WAPI_ERR_AGAIN if empty. */
WAPI_IMPORT(wapi_midi, recv)
wapi_result_t wapi_midi_recv(wapi_handle_t port, uint8_t* buf, wapi_size_t buf_len,
                             wapi_size_t* msg_len, wapi_timestamp_t* timestamp);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_MIDI_H */
