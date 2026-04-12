/**
 * WAPI - Peer-to-Peer Data Channels Capability
 * Version 1.0.0
 *
 * WebRTC-based peer-to-peer connections with data channels.
 * All operations are asynchronous via the unified I/O queue.
 * Send/receive on data channels uses the generic SEND/RECV ops.
 * Close connections and channels with the generic CLOSE op.
 *
 * Maps to: RTCPeerConnection/RTCDataChannel (Web),
 *          libwebrtc / native WebRTC libraries (Desktop/Mobile)
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

/* ---- Data channel flags (WAPI_IO_OP_P2P_CREATE_CHANNEL flags field) ---- */

#define WAPI_P2P_CHANNEL_UNORDERED   0x01  /* Default: ordered */
#define WAPI_P2P_CHANNEL_UNRELIABLE  0x02  /* Default: reliable */

/* ============================================================
 * ICE Server Configuration
 * ============================================================
 *
 * Layout (48 bytes, align 8):
 *   Offset  0: wapi_string_view_t url         (16 bytes) e.g. "stun:stun.l.google.com:19302"
 *   Offset 16: wapi_string_view_t username     (16 bytes) empty for STUN
 *   Offset 32: wapi_string_view_t credential   (16 bytes) empty for STUN
 */
typedef struct wapi_p2p_ice_server_t {
    wapi_string_view_t url;
    wapi_string_view_t username;
    wapi_string_view_t credential;
} wapi_p2p_ice_server_t;

_Static_assert(offsetof(wapi_p2p_ice_server_t, url)        ==  0, "");
_Static_assert(offsetof(wapi_p2p_ice_server_t, username)    == 16, "");
_Static_assert(offsetof(wapi_p2p_ice_server_t, credential)  == 32, "");
_Static_assert(sizeof(wapi_p2p_ice_server_t) == 48,
               "wapi_p2p_ice_server_t must be 48 bytes");
_Static_assert(_Alignof(wapi_p2p_ice_server_t) == 8,
               "wapi_p2p_ice_server_t must be 8-byte aligned");

/* ============================================================
 * P2P Connection Configuration
 * ============================================================
 *
 * Passed to WAPI_IO_OP_P2P_CREATE via addr/len.
 *
 * Layout (16 bytes, align 8):
 *   Offset  0: uint64_t servers       pointer to wapi_p2p_ice_server_t[]
 *   Offset  8: uint64_t server_count  number of ICE servers
 */
typedef struct wapi_p2p_config_t {
    uint64_t servers;       /* Linear memory address of wapi_p2p_ice_server_t[] */
    uint64_t server_count;
} wapi_p2p_config_t;

_Static_assert(offsetof(wapi_p2p_config_t, servers)      == 0, "");
_Static_assert(offsetof(wapi_p2p_config_t, server_count) == 8, "");
_Static_assert(sizeof(wapi_p2p_config_t) == 16,
               "wapi_p2p_config_t must be 16 bytes");
_Static_assert(_Alignof(wapi_p2p_config_t) == 8,
               "wapi_p2p_config_t must be 8-byte aligned");

/* ============================================================
 * I/O Operations
 * ============================================================
 *
 * All P2P operations go through the unified I/O queue.
 * See wapi_io_opcode_t in wapi.h for opcode definitions.
 *
 * Typical offerer flow:
 *   1. P2P_CREATE           → conn handle
 *   2. P2P_CREATE_CHANNEL   → channel handle
 *   3. P2P_CREATE_OFFER     → local SDP
 *   4. Send SDP to remote via signaling (application's responsibility)
 *   5. P2P_SET_REMOTE_DESC  ← remote SDP
 *   6. P2P_GATHER_ICE       → local candidates (send each via signaling)
 *   7. P2P_ADD_ICE_CANDIDATE← remote candidates
 *   8. SEND/RECV on channel handle
 *   9. CLOSE channel, CLOSE conn
 *
 * Typical answerer flow:
 *   1. P2P_CREATE           → conn handle
 *   2. P2P_SET_REMOTE_DESC  ← remote SDP (the offer)
 *   3. P2P_CREATE_ANSWER    → local SDP
 *   4. Send SDP to remote via signaling
 *   5. P2P_GATHER_ICE       → local candidates
 *   6. P2P_ADD_ICE_CANDIDATE← remote candidates
 *   7. P2P_ACCEPT_CHANNEL   → channel handle (from remote)
 *   8. SEND/RECV on channel handle
 *   9. CLOSE channel, CLOSE conn
 */

#ifdef __cplusplus
}
#endif

#endif /* WAPI_P2P_H */
