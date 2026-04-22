/**
 * WAPI - Networking
 * Version 2.0.0
 *
 * Quality-based network abstraction. The caller never names a transport
 * protocol — they describe what the channel must guarantee, and the
 * platform picks the most efficient transport that satisfies those
 * qualities. If no transport on the current platform can satisfy the
 * requested qualities, the op completes with WAPI_ERR_NOTSUP.
 *
 * Why qualities, not protocols:
 *
 *   This is a platform abstraction layer, not a protocol query layer.
 *   Naming TCP/UDP/QUIC/WebSocket in the API forces every caller to
 *   know the transport landscape of every host it will ever run on.
 *   That couples application code to deployment topology and rules
 *   out hosts that lack a particular protocol (a browser caller can
 *   never speak raw UDP; an embedded host may have only UDP). By
 *   describing qualities, the same caller code runs unchanged on
 *   every host that can satisfy the request, and fails cleanly on
 *   those that can't.
 *
 * Qualities (bitfield, packed into wapi_io_op_t::flags):
 *
 *   WAPI_NET_RELIABLE        delivery is guaranteed (retransmit on loss)
 *   WAPI_NET_ORDERED         frames are delivered in send order
 *   WAPI_NET_MESSAGE_FRAMED  send/recv preserve message boundaries
 *                            (cleared = byte stream, no framing)
 *   WAPI_NET_ENCRYPTED       transport-level confidentiality + integrity
 *                            (TLS/DTLS/QUIC crypto — required, not best-effort)
 *   WAPI_NET_MULTIPLEXED     endpoint can host multiple independent
 *                            channels (open with channel_open)
 *   WAPI_NET_LOW_LATENCY     hint: prefer latency over throughput
 *   WAPI_NET_BROADCAST       one-to-many send (multicast/broadcast)
 *
 * The platform may satisfy qualities with any transport it likes:
 *
 *   reliable+ordered+encrypted          -> TCP+TLS, QUIC stream, ...
 *   reliable+ordered+framed+encrypted   -> WebSocket/wss, QUIC stream w/ length-prefix, ...
 *   framed+low_latency (no reliability) -> UDP, QUIC datagram, DTLS, ...
 *   reliable+ordered+framed+broadcast   -> reliable multicast, gossip overlay, ...
 *
 * All operations go through the async I/O object as wapi_io_op_t
 * submissions. There are no direct imports — that keeps the surface
 * area small and lets the platform refuse capabilities it can't
 * provide rather than crashing on a missing import.
 *
 * Operations:
 *
 *   WAPI_IO_OP_CONNECT                addr=address_str, flags=qualities
 *                                     -> completion.result = conn handle
 *   WAPI_IO_OP_NETWORK_LISTEN         addr=bind_addr, flags=qualities,
 *                                     flags2=(port<<16)|backlog
 *                                     -> completion.result = listener handle
 *   WAPI_IO_OP_ACCEPT                 fd=listener
 *                                     -> completion.result = conn handle
 *   WAPI_IO_OP_NETWORK_CHANNEL_OPEN   fd=conn (must be MULTIPLEXED),
 *                                     flags=channel qualities
 *                                     -> completion.result = channel handle
 *   WAPI_IO_OP_NETWORK_CHANNEL_ACCEPT fd=conn
 *                                     -> completion.result = channel handle
 *   WAPI_IO_OP_SEND                   fd=conn|channel, addr=data
 *                                     -> completion.result = bytes sent
 *   WAPI_IO_OP_RECV                   fd=conn|channel, addr=buffer
 *                                     -> completion.result = bytes received
 *   WAPI_IO_OP_CLOSE                  fd=any handle
 *   WAPI_IO_OP_NETWORK_RESOLVE        addr=hostname, addr2=addrs_buffer
 *                                     -> completion.result = address count
 *
 * Address strings are "host:port" or "[v6]:port". Schemes ("https://",
 * "wss://", "udp://") are accepted as hints but are not authoritative —
 * qualities decide the transport. A datagram-framed channel does not
 * need WAPI_NET_RELIABLE; a byte-stream channel does not need
 * WAPI_NET_MESSAGE_FRAMED. Mutually contradictory combinations
 * (e.g. broadcast + encrypted on hosts without reliable multicast TLS)
 * fail with WAPI_ERR_NOTSUP at submit time.
 */

#ifndef WAPI_NETWORK_H
#define WAPI_NETWORK_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Quality flags — passed in wapi_io_op_t::flags for connect /
 * listen / channel_open. Combine with bitwise OR.
 * ============================================================ */

#define WAPI_NET_RELIABLE        (1u << 0)
#define WAPI_NET_ORDERED         (1u << 1)
#define WAPI_NET_MESSAGE_FRAMED  (1u << 2)
#define WAPI_NET_ENCRYPTED       (1u << 3)
#define WAPI_NET_MULTIPLEXED     (1u << 4)
#define WAPI_NET_LOW_LATENCY     (1u << 5)
#define WAPI_NET_BROADCAST       (1u << 6)

/* Convenience presets — purely compositional, no hidden behaviour. */
#define WAPI_NET_QUALITIES_BYTE_STREAM \
    (WAPI_NET_RELIABLE | WAPI_NET_ORDERED | WAPI_NET_ENCRYPTED)

#define WAPI_NET_QUALITIES_RELIABLE_MESSAGES \
    (WAPI_NET_RELIABLE | WAPI_NET_ORDERED | WAPI_NET_MESSAGE_FRAMED | WAPI_NET_ENCRYPTED)

#define WAPI_NET_QUALITIES_DATAGRAM \
    (WAPI_NET_MESSAGE_FRAMED | WAPI_NET_LOW_LATENCY)

#define WAPI_NET_QUALITIES_MULTIPLEXED_STREAMS \
    (WAPI_NET_RELIABLE | WAPI_NET_ORDERED | WAPI_NET_ENCRYPTED | WAPI_NET_MULTIPLEXED)

/* ============================================================
 * Address Forms
 * ============================================================
 *
 * Address strings passed to CONNECT / LISTEN are one of:
 *
 *   "host:port"           Direct transport address. Any quality
 *                         combination the host can satisfy directly
 *                         (TCP, UDP, QUIC, WebSocket, ...).
 *   "[v6]:port"           IPv6 literal form of the above.
 *   "peer:<peer-id>"      Rendezvous-mediated peer. Caller must attach
 *                         a WAPI_STYPE_NET_SIGNALING chained struct to
 *                         the op (via op.addr2 pointing at the chain
 *                         head) naming the rendezvous service and ICE
 *                         servers. The platform performs NAT traversal
 *                         (ICE/STUN/TURN) and hands back a handle that
 *                         behaves like any other connection. Qualities
 *                         still decide framing / reliability; WebRTC
 *                         data channels, µTP, or a custom hole-punch
 *                         transport are all valid backends.
 *
 * Schemes ("https://", "wss://", "udp://") are accepted as hints but
 * are not authoritative — qualities decide the transport.
 */

/* ============================================================
 * ICE Server (NAT traversal)
 * ============================================================
 *
 * Value-type description of a STUN/TURN server. Arrays of these are
 * referenced from wapi_network_signaling_t by linear-memory address.
 *
 * Layout (48 bytes, align 8):
 *   Offset  0: wapi_stringview_t url         STUN/TURN URL ("stun:host:port", "turn:host:port", ...)
 *   Offset 16: wapi_stringview_t username    Empty for STUN
 *   Offset 32: wapi_stringview_t credential  Empty for STUN
 */
typedef struct wapi_net_ice_server_t {
    wapi_stringview_t url;
    wapi_stringview_t username;
    wapi_stringview_t credential;
} wapi_net_ice_server_t;

_Static_assert(offsetof(wapi_net_ice_server_t, url)        ==  0, "");
_Static_assert(offsetof(wapi_net_ice_server_t, username)   == 16, "");
_Static_assert(offsetof(wapi_net_ice_server_t, credential) == 32, "");
_Static_assert(sizeof(wapi_net_ice_server_t)   == 48, "wapi_net_ice_server_t must be 48 bytes");
_Static_assert(_Alignof(wapi_net_ice_server_t) ==  8, "wapi_net_ice_server_t must be 8-byte aligned");

/* ============================================================
 * Signaling Chained Struct
 * ============================================================
 *
 * Attach to CONNECT / LISTEN when the peer is named by id rather
 * than by a directly-routable host:port. The platform uses the
 * rendezvous service to exchange candidates and the ICE servers
 * to punch through NAT. This struct is what lets a quality-based
 * network API describe NAT traversal without naming WebRTC.
 *
 * Chain header (embedded first field) must have
 *   sType = WAPI_STYPE_NET_SIGNALING.
 *
 * Layout (64 bytes, align 8):
 *   Offset  0: wapi_chain_t chain          (16 bytes)
 *   Offset 16: wapi_stringview_t    rendezvous_url (16 bytes) Signaling endpoint ("https://host/path", "wss://...")
 *   Offset 32: wapi_stringview_t    local_peer_id  (16 bytes) How remote peers name us (empty = platform assigns)
 *   Offset 48: uint64_t              ice_servers    (8 bytes)  Linear memory address of wapi_net_ice_server_t[]
 *   Offset 56: uint32_t              ice_count      (4 bytes)
 *   Offset 60: uint32_t              _pad0          (4 bytes)
 */
typedef struct wapi_network_signaling_t {
    wapi_chain_t chain;
    wapi_stringview_t    rendezvous_url;
    wapi_stringview_t    local_peer_id;
    uint64_t              ice_servers;
    uint32_t              ice_count;
    uint32_t              _pad0;
} wapi_network_signaling_t;

_Static_assert(offsetof(wapi_network_signaling_t, chain)          ==  0, "");
_Static_assert(offsetof(wapi_network_signaling_t, rendezvous_url) == 16, "");
_Static_assert(offsetof(wapi_network_signaling_t, local_peer_id)  == 32, "");
_Static_assert(offsetof(wapi_network_signaling_t, ice_servers)    == 48, "");
_Static_assert(offsetof(wapi_network_signaling_t, ice_count)      == 56, "");
_Static_assert(offsetof(wapi_network_signaling_t, _pad0)          == 60, "");
_Static_assert(sizeof(wapi_network_signaling_t)   == 64, "wapi_network_signaling_t must be 64 bytes");
_Static_assert(_Alignof(wapi_network_signaling_t) ==  8, "wapi_network_signaling_t must be 8-byte aligned");

/* ============================================================
 * Inline helpers — fill a wapi_io_op_t and submit it through the
 * supplied IO vtable. The caller keeps ownership of the address
 * and data buffers; both must remain valid until the matching
 * WAPI_EVENT_IO_COMPLETION arrives.
 *
 * All helpers return the number of ops the backend accepted (0 or 1).
 * ============================================================ */

static inline int32_t wapi_network_connect(const wapi_io_t* io,
                                           const void*      address,
                                           uint32_t         address_len,
                                           uint32_t         qualities,
                                           uint64_t         user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CONNECT;
    op.flags     = qualities;
    op.addr      = (uint64_t)(uintptr_t)address;
    op.len       = (uint64_t)address_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

static inline int32_t wapi_network_listen(const wapi_io_t* io,
                                          const void*      bind_address,
                                          uint32_t         bind_address_len,
                                          uint16_t         port,
                                          uint16_t         backlog,
                                          uint32_t         qualities,
                                          uint64_t         user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_NETWORK_LISTEN;
    op.flags     = qualities;
    op.flags2    = ((uint32_t)port << 16) | (uint32_t)backlog;
    op.addr      = (uint64_t)(uintptr_t)bind_address;
    op.len       = (uint64_t)bind_address_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

static inline int32_t wapi_network_accept(const wapi_io_t* io,
                                          wapi_handle_t    listener,
                                          uint64_t         user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_ACCEPT;
    op.fd        = (int32_t)listener;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

static inline int32_t wapi_network_channel_open(const wapi_io_t* io,
                                                wapi_handle_t    conn,
                                                uint32_t         qualities,
                                                uint64_t         user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_NETWORK_CHANNEL_OPEN;
    op.fd        = (int32_t)conn;
    op.flags     = qualities;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

static inline int32_t wapi_network_channel_accept(const wapi_io_t* io,
                                                  wapi_handle_t    conn,
                                                  uint64_t         user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_NETWORK_CHANNEL_ACCEPT;
    op.fd        = (int32_t)conn;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

static inline int32_t wapi_network_send(const wapi_io_t* io,
                                        wapi_handle_t    handle,
                                        const void*      data,
                                        uint32_t         len,
                                        uint64_t         user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_SEND;
    op.fd        = (int32_t)handle;
    op.addr      = (uint64_t)(uintptr_t)data;
    op.len       = (uint64_t)len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

static inline int32_t wapi_network_recv(const wapi_io_t* io,
                                        wapi_handle_t    handle,
                                        void*            buffer,
                                        uint32_t         capacity,
                                        uint64_t         user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_RECV;
    op.fd        = (int32_t)handle;
    op.addr      = (uint64_t)(uintptr_t)buffer;
    op.len       = (uint64_t)capacity;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

static inline int32_t wapi_network_close(const wapi_io_t* io,
                                         wapi_handle_t    handle,
                                         uint64_t         user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CLOSE;
    op.fd        = (int32_t)handle;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

static inline int32_t wapi_network_resolve(const wapi_io_t* io,
                                           const void*      hostname,
                                           uint32_t         hostname_len,
                                           void*            addrs_buffer,
                                           uint32_t         addrs_capacity,
                                           uint64_t         user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_NETWORK_RESOLVE;
    op.addr      = (uint64_t)(uintptr_t)hostname;
    op.len       = (uint64_t)hostname_len;
    op.addr2     = (uint64_t)(uintptr_t)addrs_buffer;
    op.len2      = (uint64_t)addrs_capacity;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/* ============================================================
 * Capability Probe
 * ============================================================
 *
 * Ask the host whether a CONNECT request with a given qualities
 * bitmask will succeed. Pure query — no sockets opened, no backing
 * libraries loaded, no side effects. Use this at startup to pick
 * between transport options (e.g. "prefer QUIC, fall back to TLS")
 * instead of try-and-fail via WAPI_IO_OP_CONNECT.
 *
 * Qualities carry the same WAPI_NET_* bits the CONNECT opcode
 * accepts in its flags field.
 *
 * Returns 1 if the host can honour the combination, 0 if the
 * combination maps to a concrete transport the host does not
 * provide (e.g. QUIC on a machine without msquic), and
 * WAPI_ERR_INVAL if the combination is self-contradictory
 * (ordered without reliable, broadcast without framing, etc.) on
 * every platform.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_network, qualities_supported)
int32_t wapi_net_qualities_supported(uint32_t qualities);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_NETWORK_H */
