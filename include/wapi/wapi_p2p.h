/**
 * WAPI - Peer-to-Peer Data Channels Capability
 * Version 1.0.0
 *
 * Peer-to-peer data channels for direct communication between modules.
 *
 * Maps to: WebRTC RTCPeerConnection/RTCDataChannel (Web),
 *          native WebRTC libraries (Desktop/Mobile)
 *
 * Import module: "wapi_p2p"
 *
 * Query availability with wapi_capability_supported("wapi.p2p", 7)
 */

#ifndef WAPI_P2P_H
#define WAPI_P2P_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * P2P Events
 * ============================================================ */

/** @deprecated Use WAPI_EVENT_IO_COMPLETION with the unified I/O queue instead. */
#define WAPI_EVENT_P2P_STATE_CHANGED  0x1400
/** @deprecated Use WAPI_EVENT_IO_COMPLETION with the unified I/O queue instead. */
#define WAPI_EVENT_P2P_DATA           0x1401

/* ============================================================
 * P2P Types
 * ============================================================ */

typedef enum wapi_p2p_state_t {
    WAPI_P2P_STATE_NEW          = 0,
    WAPI_P2P_STATE_CONNECTING   = 1,
    WAPI_P2P_STATE_CONNECTED    = 2,
    WAPI_P2P_STATE_DISCONNECTED = 3,
    WAPI_P2P_STATE_FAILED       = 4,
    WAPI_P2P_STATE_CLOSED       = 5,
    WAPI_P2P_STATE_FORCE32      = 0x7FFFFFFF
} wapi_p2p_state_t;

/**
 * P2P connection configuration.
 *
 * Layout (32 bytes, align 4):
 *   Offset  0: wapi_string_view_t stun_server
 *   Offset  8: wapi_string_view_t turn_server
 *   Offset 16: wapi_string_view_t turn_user
 *   Offset 24: wapi_string_view_t turn_pass
 */
typedef struct wapi_p2p_config_t {
    wapi_string_view_t stun_server;
    wapi_string_view_t turn_server;
    wapi_string_view_t turn_user;
    wapi_string_view_t turn_pass;
} wapi_p2p_config_t;

#ifdef __wasm__
_Static_assert(sizeof(wapi_p2p_config_t) == 32,
               "wapi_p2p_config_t must be 32 bytes on wasm32");
#endif

/* ============================================================
 * P2P Functions
 * ============================================================ */

/**
 * Create a new P2P connection.
 *
 * @see WAPI_IO_OP_P2P_CREATE
 *
 * @param config  Connection configuration (STUN/TURN servers).
 * @param conn    [out] P2P connection handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_p2p, create)
wapi_result_t wapi_p2p_create(const wapi_p2p_config_t* config,
                              wapi_handle_t* conn);

/**
 * Create an SDP offer for the connection.
 *
 * @see WAPI_IO_OP_P2P_CREATE_OFFER
 *
 * @param conn     P2P connection handle.
 * @param sdp_buf  [out] Buffer to receive the SDP offer string.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_p2p, create_offer)
wapi_result_t wapi_p2p_create_offer(wapi_handle_t conn, void* sdp_buf);

/**
 * Create an SDP answer for the connection.
 *
 * @see WAPI_IO_OP_P2P_CREATE_ANSWER
 *
 * @param conn     P2P connection handle.
 * @param sdp_buf  [out] Buffer to receive the SDP answer string.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_p2p, create_answer)
wapi_result_t wapi_p2p_create_answer(wapi_handle_t conn, void* sdp_buf);

/**
 * Set the remote SDP description.
 *
 * @param conn     P2P connection handle.
 * @param sdp      Remote SDP string.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_p2p, set_remote_desc)
wapi_result_t wapi_p2p_set_remote_desc(wapi_handle_t conn, wapi_string_view_t sdp);

/**
 * Add an ICE candidate to the connection.
 *
 * @see WAPI_IO_OP_P2P_ADD_ICE_CANDIDATE
 *
 * @param conn           P2P connection handle.
 * @param candidate      ICE candidate string.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_p2p, add_ice_candidate)
wapi_result_t wapi_p2p_add_ice_candidate(wapi_handle_t conn,
                                         wapi_string_view_t candidate);

/**
 * Send data over the P2P data channel.
 *
 * @see WAPI_IO_OP_P2P_SEND
 *
 * @param conn      P2P connection handle.
 * @param data      Pointer to data to send.
 * @param data_len  Length of the data.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_p2p, send)
wapi_result_t wapi_p2p_send(wapi_handle_t conn, const void* data,
                            wapi_size_t data_len);

/**
 * Close a P2P connection.
 *
 * @param conn  P2P connection handle.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_p2p, close)
wapi_result_t wapi_p2p_close(wapi_handle_t conn);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_P2P_H */
