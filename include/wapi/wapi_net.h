/**
 * WAPI - Networking
 * Version 1.0.0
 *
 * QUIC/WebTransport-shaped networking. Connection-oriented with
 * built-in multiplexed streams, matching the direction of both
 * the web platform (WebTransport) and modern native networking.
 *
 * For high-throughput I/O, use wapi_io.h with WAPI_IO_OP_SEND/RECV.
 *
 * Import module: "wapi_net"
 */

#ifndef WAPI_NET_H
#define WAPI_NET_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Connection Types
 * ============================================================ */

typedef enum wapi_net_transport_t {
    WAPI_NET_TRANSPORT_QUIC          = 0,  /* QUIC / WebTransport */
    WAPI_NET_TRANSPORT_TCP           = 1,  /* TCP (fallback) */
    WAPI_NET_TRANSPORT_WEBSOCKET     = 2,  /* WebSocket (browser compat) */
    WAPI_NET_TRANSPORT_FORCE32       = 0x7FFFFFFF
} wapi_net_transport_t;

typedef enum wapi_net_stream_type_t {
    WAPI_NET_STREAM_BIDI    = 0,  /* Bidirectional stream */
    WAPI_NET_STREAM_UNI     = 1,  /* Unidirectional stream */
    WAPI_NET_STREAM_FORCE32 = 0x7FFFFFFF
} wapi_net_stream_type_t;

/* ============================================================
 * Connection Descriptor
 * ============================================================
 *
 * Chain a wapi_net_tls_config_t to enable TLS (always-on for QUIC).
 *
 * Layout (16 bytes on wasm32, align 4):
 *   Offset  0: ptr      nextInChain
 *   Offset  4: wapi_string_view_t url  URL string
 *   Offset 12: uint32_t transport    wapi_net_transport_t
 */

typedef struct wapi_net_connect_desc_t {
    wapi_chained_struct_t*  nextInChain;
    wapi_string_view_t      url;
    uint32_t                transport;   /* wapi_net_transport_t */
} wapi_net_connect_desc_t;

/* ============================================================
 * TLS Configuration (Chained Struct)
 * ============================================================
 * Chain onto wapi_net_connect_desc_t::nextInChain to enable TLS.
 * Without it, the connection is plaintext (except QUIC, which
 * is always TLS). sType = WAPI_STYPE_NET_TLS_CONFIG.
 *
 * Layout (8 bytes on wasm32, align 4):
 *   Offset  0: wapi_chained_struct_t chain
 */

typedef struct wapi_net_tls_config_t {
    wapi_chained_struct_t   chain;
    /* Future: cert_data, cert_len, alpn, client_cert, etc. */
} wapi_net_tls_config_t;

/* ============================================================
 * Listen Descriptor
 * ============================================================
 *
 * Layout (20 bytes, align 4):
 *   Offset  0: wapi_string_view_t addr  Bind address string
 *   Offset  8: uint32_t port         Port number
 *   Offset 12: uint32_t transport    wapi_net_transport_t
 *   Offset 16: uint32_t backlog      Connection backlog size
 */

typedef struct wapi_net_listen_desc_t {
    wapi_string_view_t    addr;
    uint32_t              port;
    uint32_t              transport;   /* wapi_net_transport_t */
    uint32_t              backlog;
} wapi_net_listen_desc_t;

/* ============================================================
 * Connection Functions
 * ============================================================ */

/**
 * Open a connection to a remote host.
 *
 * @see WAPI_IO_OP_CONNECT
 * @param desc  Connection descriptor.
 * @param conn  [out] Connection handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_net, connect)
wapi_result_t wapi_net_connect(const wapi_net_connect_desc_t* desc, wapi_handle_t* conn);

/**
 * Start listening for incoming connections.
 *
 * @see WAPI_IO_OP_NET_LISTEN
 * @param desc      Listen descriptor.
 * @param listener  [out] Listener handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_net, listen)
wapi_result_t wapi_net_listen(const wapi_net_listen_desc_t* desc, wapi_handle_t* listener);

/**
 * Accept an incoming connection on a listener.
 *
 * @see WAPI_IO_OP_ACCEPT
 * @param listener  Listener handle.
 * @param conn      [out] New connection handle.
 * @return WAPI_OK on success, WAPI_ERR_AGAIN if no pending connections.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_net, accept)
wapi_result_t wapi_net_accept(wapi_handle_t listener, wapi_handle_t* conn);

/**
 * Close a connection, listener, or stream.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_net, close)
wapi_result_t wapi_net_close(wapi_handle_t handle);

/* ============================================================
 * QUIC Stream Functions
 * ============================================================
 * QUIC connections support multiplexed streams. For TCP/WebSocket,
 * the connection handle itself acts as the single stream.
 */

/**
 * Open a new stream on a QUIC connection.
 *
 * @see WAPI_IO_OP_NET_STREAM_OPEN
 * @param conn    Connection handle.
 * @param type    Stream type (bidi or uni).
 * @param stream  [out] Stream handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_net, stream_open)
wapi_result_t wapi_net_stream_open(wapi_handle_t conn, wapi_net_stream_type_t type,
                                wapi_handle_t* stream);

/**
 * Accept an incoming stream on a QUIC connection.
 *
 * @see WAPI_IO_OP_NET_STREAM_ACCEPT
 * @param conn    Connection handle.
 * @param stream  [out] Stream handle.
 * @return WAPI_OK on success, WAPI_ERR_AGAIN if no pending streams.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_net, stream_accept)
wapi_result_t wapi_net_stream_accept(wapi_handle_t conn, wapi_handle_t* stream);

/* ============================================================
 * Data Transfer
 * ============================================================
 * These work on both streams (QUIC) and connections (TCP/WS).
 * For high-throughput, use wapi_io.h WAPI_IO_OP_SEND/RECV instead.
 */

/**
 * Send data on a connection or stream.
 *
 * @see WAPI_IO_OP_SEND
 * @param handle        Connection or stream handle.
 * @param buf           Data to send.
 * @param len           Data length.
 * @param bytes_sent    [out] Actual bytes sent.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_net, send)
wapi_result_t wapi_net_send(wapi_handle_t handle, const void* buf, wapi_size_t len,
                         wapi_size_t* bytes_sent);

/**
 * Receive data from a connection or stream.
 *
 * @see WAPI_IO_OP_RECV
 * @param handle        Connection or stream handle.
 * @param buf           Buffer to receive data.
 * @param len           Buffer capacity.
 * @param bytes_recv    [out] Actual bytes received.
 * @return WAPI_OK on success, WAPI_ERR_AGAIN if no data available.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_net, recv)
wapi_result_t wapi_net_recv(wapi_handle_t handle, void* buf, wapi_size_t len,
                         wapi_size_t* bytes_recv);

/**
 * Send a datagram on a QUIC connection (unreliable, unordered).
 *
 * @see WAPI_IO_OP_NET_SEND_DATAGRAM
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_net, send_datagram)
wapi_result_t wapi_net_send_datagram(wapi_handle_t conn, const void* buf, wapi_size_t len);

/**
 * Receive a datagram from a QUIC connection.
 *
 * @see WAPI_IO_OP_NET_RECV_DATAGRAM
 * @param conn       Connection handle.
 * @param buf        Buffer to receive datagram.
 * @param len        Buffer capacity.
 * @param recv_len   [out] Actual datagram size.
 * @return WAPI_OK on success, WAPI_ERR_AGAIN if no datagrams available.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_net, recv_datagram)
wapi_result_t wapi_net_recv_datagram(wapi_handle_t conn, void* buf, wapi_size_t len,
                                  wapi_size_t* recv_len);

/* ============================================================
 * DNS Resolution
 * ============================================================ */

/**
 * Resolve a hostname to addresses.
 * Results are written as a sequence of wapi_net_addr_t structs.
 *
 * @see WAPI_IO_OP_NET_RESOLVE
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_net, resolve)
wapi_result_t wapi_net_resolve(wapi_string_view_t host,
                            void* addrs_buf, wapi_size_t buf_len,
                            wapi_size_t* count);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_NET_H */
