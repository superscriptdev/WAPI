/**
 * WAPI Desktop Runtime - Networking
 *
 * Implements all wapi_net.* imports.
 * TCP is fully implemented; QUIC and WebSocket return WAPI_ERR_NOTSUP.
 *
 * Platform notes:
 *   - Windows: uses winsock2.h (SOCKET type), included via wapi_host.h
 *   - Unix:    uses sys/socket.h (int fd),     included via wapi_host.h
 */

#include "wapi_host.h"
#include <errno.h>

#ifdef _WIN32
#define WAPI_CLOSESOCKET(s)  closesocket(s)
#define WAPI_LAST_NET_ERROR  WSAGetLastError()
#define WAPI_EWOULDBLOCK     WSAEWOULDBLOCK
#define WAPI_EINPROGRESS     WSAEWOULDBLOCK
#define WAPI_ECONNREFUSED    WSAECONNREFUSED
#define WAPI_ECONNRESET      WSAECONNRESET
#define WAPI_ECONNABORTED    WSAECONNABORTED
#define WAPI_ENETUNREACH     WSAENETUNREACH
#define WAPI_EHOSTUNREACH    WSAEHOSTUNREACH
#define WAPI_EADDRINUSE      WSAEADDRINUSE
#else
#include <errno.h>
#define WAPI_CLOSESOCKET(s)  close(s)
#define WAPI_LAST_NET_ERROR  errno
#define WAPI_EWOULDBLOCK     EWOULDBLOCK
#define WAPI_EINPROGRESS     EINPROGRESS
#define WAPI_ECONNREFUSED    ECONNREFUSED
#define WAPI_ECONNRESET      ECONNRESET
#define WAPI_ECONNABORTED    ECONNABORTED
#define WAPI_ENETUNREACH     ENETUNREACH
#define WAPI_EHOSTUNREACH    EHOSTUNREACH
#define WAPI_EADDRINUSE      EADDRINUSE
#endif

/* ============================================================
 * Helpers
 * ============================================================ */

static int32_t net_error_to_wapi(int err) {
#ifdef _WIN32
    switch (err) {
    case WSAEWOULDBLOCK:   return WAPI_ERR_AGAIN;
    case WSAECONNREFUSED:  return WAPI_ERR_CONNREFUSED;
    case WSAECONNRESET:    return WAPI_ERR_CONNRESET;
    case WSAECONNABORTED:  return WAPI_ERR_CONNABORTED;
    case WSAENETUNREACH:   return WAPI_ERR_NETUNREACH;
    case WSAEHOSTUNREACH:  return WAPI_ERR_HOSTUNREACH;
    case WSAEADDRINUSE:    return WAPI_ERR_ADDRINUSE;
    default:               return WAPI_ERR_IO;
    }
#else
    switch (err) {
    case EAGAIN:
#if EAGAIN != EWOULDBLOCK
    case EWOULDBLOCK:
#endif
                           return WAPI_ERR_AGAIN;
    case ECONNREFUSED:     return WAPI_ERR_CONNREFUSED;
    case ECONNRESET:       return WAPI_ERR_CONNRESET;
    case ECONNABORTED:     return WAPI_ERR_CONNABORTED;
    case ENETUNREACH:      return WAPI_ERR_NETUNREACH;
    case EHOSTUNREACH:     return WAPI_ERR_HOSTUNREACH;
    case EADDRINUSE:       return WAPI_ERR_ADDRINUSE;
    default:               return WAPI_ERR_IO;
    }
#endif
}

static bool set_nonblocking(wapi_socket_t sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

/* Parse "host:port" from a URL string. Writes null-terminated host and port. */
static bool parse_host_port(const char* url, uint32_t url_len,
                            char* host_out, size_t host_cap,
                            char* port_out, size_t port_cap) {
    /* Skip any "tcp://" prefix if present */
    const char* start = url;
    uint32_t len = url_len;
    if (len > 6 && memcmp(start, "tcp://", 6) == 0) {
        start += 6;
        len -= 6;
    }

    /* Handle IPv6 bracket notation: [::1]:port */
    if (len > 0 && start[0] == '[') {
        const char* close = memchr(start, ']', len);
        if (!close) return false;
        size_t hlen = (size_t)(close - start - 1);
        if (hlen >= host_cap) return false;
        memcpy(host_out, start + 1, hlen);
        host_out[hlen] = '\0';

        const char* after = close + 1;
        size_t remaining = len - (size_t)(after - start);
        if (remaining > 0 && after[0] == ':') {
            after++;
            remaining--;
            if (remaining == 0 || remaining >= port_cap) return false;
            memcpy(port_out, after, remaining);
            port_out[remaining] = '\0';
        } else {
            return false; /* port required */
        }
        return true;
    }

    /* Find last colon for host:port */
    const char* colon = NULL;
    for (uint32_t i = 0; i < len; i++) {
        if (start[i] == ':') colon = &start[i];
    }
    if (!colon) return false;

    size_t hlen = (size_t)(colon - start);
    size_t plen = len - hlen - 1;
    if (hlen == 0 || plen == 0) return false;
    if (hlen >= host_cap || plen >= port_cap) return false;

    memcpy(host_out, start, hlen);
    host_out[hlen] = '\0';
    memcpy(port_out, colon + 1, plen);
    port_out[plen] = '\0';
    return true;
}

/* ============================================================
 * net.connect
 * ============================================================
 * Wasm signature: (i32 desc_ptr, i32 conn_ptr) -> i32
 *
 * Reads wapi_net_connect_desc_t (24 bytes) from wasm memory.
 * Wasm layout:
 *   +0:  u32  url_ptr
 *   +4:  u32  url_len
 *   +8:  u32  transport
 *   +12: u32  flags
 *   +16: u32  _reserved[2]
 */
static wasm_trap_t* host_net_connect(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t desc_ptr = WAPI_ARG_U32(0);
    uint32_t conn_ptr = WAPI_ARG_U32(1);

    /* Read descriptor from wasm memory */
    void* desc_host = wapi_wasm_ptr(desc_ptr, 24);
    if (!desc_host) {
        wapi_set_error("connect: invalid desc pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    uint32_t url_gptr, url_len, transport, flags;
    memcpy(&url_gptr,   (uint8_t*)desc_host + 0, 4);
    memcpy(&url_len,    (uint8_t*)desc_host + 4, 4);
    memcpy(&transport,  (uint8_t*)desc_host + 8, 4);
    memcpy(&flags,      (uint8_t*)desc_host + 12, 4);

    /* Only TCP is supported for now */
    if (transport != 1 /* WAPI_NET_TRANSPORT_TCP */) {
        wapi_set_error("connect: only TCP transport is currently supported");
        WAPI_RET_I32(WAPI_ERR_NOTSUP);
        return NULL;
    }

    const char* url = wapi_wasm_read_string(url_gptr, url_len);
    if (!url) {
        wapi_set_error("connect: invalid URL pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    /* Parse host:port from URL */
    char host[256], port[16];
    if (!parse_host_port(url, url_len, host, sizeof(host), port, sizeof(port))) {
        wapi_set_error("connect: failed to parse host:port from URL");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    /* Resolve address */
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* ai = NULL;
    int gai_err = getaddrinfo(host, port, &hints, &ai);
    if (gai_err != 0 || !ai) {
        wapi_set_error("connect: getaddrinfo failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    /* Create socket and connect */
    wapi_socket_t sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sock == WAPI_INVALID_SOCKET) {
        freeaddrinfo(ai);
        wapi_set_error("connect: socket() failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    if (connect(sock, ai->ai_addr, (int)ai->ai_addrlen) != 0) {
        int err = WAPI_LAST_NET_ERROR;
        /* On non-blocking sockets EINPROGRESS is expected, but we connect
           blocking first, then switch to non-blocking. */
        freeaddrinfo(ai);
        WAPI_CLOSESOCKET(sock);
        wapi_set_error("connect: connect() failed");
        WAPI_RET_I32(net_error_to_wapi(err));
        return NULL;
    }
    freeaddrinfo(ai);

    /* Set non-blocking after successful connect */
    set_nonblocking(sock);

    /* Disable Nagle if requested */
    if (flags & 0x0001 /* WAPI_NET_FLAG_NODELAY */) {
        int yes = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof(yes));
    }

    /* Allocate handle */
    int32_t h = wapi_handle_alloc(WAPI_HTYPE_NET_CONN);
    if (h == 0) {
        WAPI_CLOSESOCKET(sock);
        wapi_set_error("connect: out of handles");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    g_rt.handles[h].data.net_conn.sock = sock;
    g_rt.handles[h].data.net_conn.transport = transport;
    g_rt.handles[h].data.net_conn.connected = true;
    g_rt.handles[h].data.net_conn.nonblocking = true;

    /* Write handle to guest */
    if (!wapi_wasm_write_i32(conn_ptr, h)) {
        wapi_handle_free(h);
        WAPI_CLOSESOCKET(sock);
        wapi_set_error("connect: invalid conn_ptr");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * net.listen
 * ============================================================
 * Wasm signature: (i32 desc_ptr, i32 listener_ptr) -> i32
 *
 * Wasm layout of wapi_net_listen_desc_t (20 bytes):
 *   +0:  u32  addr_ptr
 *   +4:  u32  addr_len
 *   +8:  u32  port
 *   +12: u32  transport
 *   +16: u32  backlog
 */
static wasm_trap_t* host_net_listen(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t desc_ptr     = WAPI_ARG_U32(0);
    uint32_t listener_ptr = WAPI_ARG_U32(1);

    void* desc_host = wapi_wasm_ptr(desc_ptr, 20);
    if (!desc_host) {
        wapi_set_error("listen: invalid desc pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    uint32_t addr_gptr, addr_len, port, transport, backlog;
    memcpy(&addr_gptr,  (uint8_t*)desc_host + 0,  4);
    memcpy(&addr_len,   (uint8_t*)desc_host + 4,  4);
    memcpy(&port,       (uint8_t*)desc_host + 8,  4);
    memcpy(&transport,  (uint8_t*)desc_host + 12, 4);
    memcpy(&backlog,    (uint8_t*)desc_host + 16, 4);

    if (transport != 1 /* WAPI_NET_TRANSPORT_TCP */) {
        wapi_set_error("listen: only TCP transport is currently supported");
        WAPI_RET_I32(WAPI_ERR_NOTSUP);
        return NULL;
    }

    if (backlog == 0) backlog = 128;

    /* Read bind address string */
    const char* addr_str = NULL;
    char addr_buf[256];
    if (addr_len > 0) {
        addr_str = wapi_wasm_read_string(addr_gptr, addr_len);
        if (!addr_str) {
            wapi_set_error("listen: invalid addr pointer");
            WAPI_RET_I32(WAPI_ERR_INVAL);
            return NULL;
        }
        if (addr_len >= sizeof(addr_buf)) addr_len = sizeof(addr_buf) - 1;
        memcpy(addr_buf, addr_str, addr_len);
        addr_buf[addr_len] = '\0';
        addr_str = addr_buf;
    }

    /* Resolve bind address */
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;      /* Prefer IPv6 dual-stack */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", port);

    struct addrinfo* ai = NULL;
    int gai_err = getaddrinfo(addr_str, port_str, &hints, &ai);
    if (gai_err != 0 || !ai) {
        /* Fall back to IPv4 */
        hints.ai_family = AF_INET;
        gai_err = getaddrinfo(addr_str, port_str, &hints, &ai);
        if (gai_err != 0 || !ai) {
            wapi_set_error("listen: getaddrinfo failed");
            WAPI_RET_I32(WAPI_ERR_IO);
            return NULL;
        }
    }

    wapi_socket_t sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sock == WAPI_INVALID_SOCKET) {
        freeaddrinfo(ai);
        wapi_set_error("listen: socket() failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    /* Allow address reuse */
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    /* On IPv6 sockets, allow dual-stack */
    if (ai->ai_family == AF_INET6) {
        int no = 0;
        setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&no, sizeof(no));
    }

    if (bind(sock, ai->ai_addr, (int)ai->ai_addrlen) != 0) {
        int err = WAPI_LAST_NET_ERROR;
        freeaddrinfo(ai);
        WAPI_CLOSESOCKET(sock);
        wapi_set_error("listen: bind() failed");
        WAPI_RET_I32(net_error_to_wapi(err));
        return NULL;
    }
    freeaddrinfo(ai);

    if (listen(sock, (int)backlog) != 0) {
        int err = WAPI_LAST_NET_ERROR;
        WAPI_CLOSESOCKET(sock);
        wapi_set_error("listen: listen() failed");
        WAPI_RET_I32(net_error_to_wapi(err));
        return NULL;
    }

    /* Set non-blocking so accept() can return WAPI_ERR_AGAIN */
    set_nonblocking(sock);

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_NET_LISTENER);
    if (h == 0) {
        WAPI_CLOSESOCKET(sock);
        wapi_set_error("listen: out of handles");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    g_rt.handles[h].data.net_conn.sock = sock;
    g_rt.handles[h].data.net_conn.transport = transport;
    g_rt.handles[h].data.net_conn.connected = false;
    g_rt.handles[h].data.net_conn.nonblocking = true;

    if (!wapi_wasm_write_i32(listener_ptr, h)) {
        wapi_handle_free(h);
        WAPI_CLOSESOCKET(sock);
        wapi_set_error("listen: invalid listener_ptr");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * net.accept
 * ============================================================
 * Wasm signature: (i32 listener, i32 conn_ptr) -> i32
 */
static wasm_trap_t* host_net_accept(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  listener_h = WAPI_ARG_I32(0);
    uint32_t conn_ptr   = WAPI_ARG_U32(1);

    if (!wapi_handle_valid(listener_h, WAPI_HTYPE_NET_LISTENER)) {
        wapi_set_error("accept: invalid listener handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    wapi_socket_t listen_sock = g_rt.handles[listener_h].data.net_conn.sock;
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    wapi_socket_t client = accept(listen_sock, (struct sockaddr*)&addr, &addr_len);
    if (client == WAPI_INVALID_SOCKET) {
        int err = WAPI_LAST_NET_ERROR;
        if (err == WAPI_EWOULDBLOCK) {
            WAPI_RET_I32(WAPI_ERR_AGAIN);
        } else {
            wapi_set_error("accept: accept() failed");
            WAPI_RET_I32(net_error_to_wapi(err));
        }
        return NULL;
    }

    set_nonblocking(client);

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_NET_CONN);
    if (h == 0) {
        WAPI_CLOSESOCKET(client);
        wapi_set_error("accept: out of handles");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    g_rt.handles[h].data.net_conn.sock = client;
    g_rt.handles[h].data.net_conn.transport = g_rt.handles[listener_h].data.net_conn.transport;
    g_rt.handles[h].data.net_conn.connected = true;
    g_rt.handles[h].data.net_conn.nonblocking = true;

    if (!wapi_wasm_write_i32(conn_ptr, h)) {
        wapi_handle_free(h);
        WAPI_CLOSESOCKET(client);
        wapi_set_error("accept: invalid conn_ptr");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * net.close
 * ============================================================
 * Wasm signature: (i32 handle) -> i32
 */
static wasm_trap_t* host_net_close(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);

    if (!wapi_handle_valid(h, WAPI_HTYPE_NET_CONN) &&
        !wapi_handle_valid(h, WAPI_HTYPE_NET_LISTENER)) {
        wapi_set_error("net_close: invalid handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    wapi_socket_t sock = g_rt.handles[h].data.net_conn.sock;
    if (sock != WAPI_INVALID_SOCKET) {
        WAPI_CLOSESOCKET(sock);
    }
    wapi_handle_free(h);

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * net.stream_open  (QUIC only -- stub)
 * ============================================================
 * Wasm signature: (i32 conn, i32 type, i32 stream_ptr) -> i32
 */
static wasm_trap_t* host_net_stream_open(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * net.stream_accept  (QUIC only -- stub)
 * ============================================================
 * Wasm signature: (i32 conn, i32 stream_ptr) -> i32
 */
static wasm_trap_t* host_net_stream_accept(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * net.send
 * ============================================================
 * Wasm signature: (i32 handle, i32 buf, i32 len, i32 sent_ptr) -> i32
 */
static wasm_trap_t* host_net_send(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  h        = WAPI_ARG_I32(0);
    uint32_t buf_ptr  = WAPI_ARG_U32(1);
    uint32_t len      = WAPI_ARG_U32(2);
    uint32_t sent_ptr = WAPI_ARG_U32(3);

    if (!wapi_handle_valid(h, WAPI_HTYPE_NET_CONN)) {
        wapi_set_error("send: invalid handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    const void* buf = wapi_wasm_ptr(buf_ptr, len);
    if (!buf && len > 0) {
        wapi_set_error("send: invalid buffer pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    wapi_socket_t sock = g_rt.handles[h].data.net_conn.sock;
    int sent = send(sock, (const char*)buf, (int)len, 0);
    if (sent < 0) {
        int err = WAPI_LAST_NET_ERROR;
        if (err == WAPI_EWOULDBLOCK) {
            wapi_wasm_write_u32(sent_ptr, 0);
            WAPI_RET_I32(WAPI_ERR_AGAIN);
        } else {
            wapi_set_error("send: send() failed");
            WAPI_RET_I32(net_error_to_wapi(err));
        }
        return NULL;
    }

    wapi_wasm_write_u32(sent_ptr, (uint32_t)sent);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * net.recv
 * ============================================================
 * Wasm signature: (i32 handle, i32 buf, i32 len, i32 recv_ptr) -> i32
 */
static wasm_trap_t* host_net_recv(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  h        = WAPI_ARG_I32(0);
    uint32_t buf_ptr  = WAPI_ARG_U32(1);
    uint32_t len      = WAPI_ARG_U32(2);
    uint32_t recv_ptr = WAPI_ARG_U32(3);

    if (!wapi_handle_valid(h, WAPI_HTYPE_NET_CONN)) {
        wapi_set_error("recv: invalid handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    void* buf = wapi_wasm_ptr(buf_ptr, len);
    if (!buf && len > 0) {
        wapi_set_error("recv: invalid buffer pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    wapi_socket_t sock = g_rt.handles[h].data.net_conn.sock;
    int received = recv(sock, (char*)buf, (int)len, 0);
    if (received < 0) {
        int err = WAPI_LAST_NET_ERROR;
        if (err == WAPI_EWOULDBLOCK) {
            wapi_wasm_write_u32(recv_ptr, 0);
            WAPI_RET_I32(WAPI_ERR_AGAIN);
        } else {
            wapi_set_error("recv: recv() failed");
            WAPI_RET_I32(net_error_to_wapi(err));
        }
        return NULL;
    }
    if (received == 0) {
        /* Peer closed the connection */
        wapi_wasm_write_u32(recv_ptr, 0);
        WAPI_RET_I32(WAPI_ERR_CONNRESET);
        return NULL;
    }

    wapi_wasm_write_u32(recv_ptr, (uint32_t)received);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * net.send_datagram  (QUIC only -- stub)
 * ============================================================
 * Wasm signature: (i32 conn, i32 buf, i32 len) -> i32
 */
static wasm_trap_t* host_net_send_datagram(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * net.recv_datagram  (QUIC only -- stub)
 * ============================================================
 * Wasm signature: (i32 conn, i32 buf, i32 len, i32 recv_len_ptr) -> i32
 */
static wasm_trap_t* host_net_recv_datagram(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * net.resolve
 * ============================================================
 * Wasm signature: (i32 host, i32 host_len, i32 addrs_buf, i32 buf_len, i32 count_ptr) -> i32
 *
 * Each result entry is 20 bytes:
 *   +0:  u32 family (4 = AF_INET, 6 = AF_INET6)
 *   +4:  u8[16] address (IPv4 uses first 4 bytes, IPv6 uses all 16)
 */
static wasm_trap_t* host_net_resolve(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t host_ptr  = WAPI_ARG_U32(0);
    uint32_t host_len  = WAPI_ARG_U32(1);
    uint32_t addrs_ptr = WAPI_ARG_U32(2);
    uint32_t buf_len   = WAPI_ARG_U32(3);
    uint32_t count_ptr = WAPI_ARG_U32(4);

    const char* hostname = wapi_wasm_read_string(host_ptr, host_len);
    if (!hostname) {
        wapi_set_error("resolve: invalid host pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    /* Null-terminate the hostname */
    char host_buf[256];
    if (host_len >= sizeof(host_buf)) {
        wapi_set_error("resolve: hostname too long");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }
    memcpy(host_buf, hostname, host_len);
    host_buf[host_len] = '\0';

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* ai = NULL;
    int gai_err = getaddrinfo(host_buf, NULL, &hints, &ai);
    if (gai_err != 0 || !ai) {
        wapi_set_error("resolve: getaddrinfo failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    /* Write results: each entry is 20 bytes (4 family + 16 addr) */
    uint32_t entry_size = 20;
    uint32_t max_entries = buf_len / entry_size;
    uint32_t count = 0;

    void* out_buf = wapi_wasm_ptr(addrs_ptr, buf_len);
    if (!out_buf && buf_len > 0) {
        freeaddrinfo(ai);
        wapi_set_error("resolve: invalid addrs_buf pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    for (struct addrinfo* cur = ai; cur && count < max_entries; cur = cur->ai_next) {
        uint8_t entry[20];
        memset(entry, 0, sizeof(entry));

        if (cur->ai_family == AF_INET) {
            struct sockaddr_in* sin = (struct sockaddr_in*)cur->ai_addr;
            uint32_t fam = 4;
            memcpy(entry + 0, &fam, 4);
            memcpy(entry + 4, &sin->sin_addr, 4);
        } else if (cur->ai_family == AF_INET6) {
            struct sockaddr_in6* sin6 = (struct sockaddr_in6*)cur->ai_addr;
            uint32_t fam = 6;
            memcpy(entry + 0, &fam, 4);
            memcpy(entry + 4, &sin6->sin6_addr, 16);
        } else {
            continue; /* skip unknown address families */
        }

        memcpy((uint8_t*)out_buf + count * entry_size, entry, entry_size);
        count++;
    }

    freeaddrinfo(ai);

    wapi_wasm_write_u32(count_ptr, count);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_net(wasmtime_linker_t* linker) {
    /* Initialize winsock on Windows */
#ifdef _WIN32
    if (!g_rt.net_initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            fprintf(stderr, "wapi_net: WSAStartup failed\n");
        }
        g_rt.net_initialized = true;
    }
#else
    g_rt.net_initialized = true;
#endif

    /* Per wapi_network.h: "All operations go through the async I/O
     * object as wapi_io_op_t submissions. There are no direct imports."
     * The host_net_* functions below are retained as scaffolding for
     * the op_ctx_t handlers that wapi_host_io.c will call for
     * WAPI_IO_OP_CONNECT / ACCEPT / SEND / RECV / NETWORK_LISTEN /
     * NETWORK_CHANNEL_OPEN / NETWORK_CHANNEL_ACCEPT / NETWORK_RESOLVE.
     * See NEXT_STEPS.md. */
    (void)host_net_connect; (void)host_net_listen; (void)host_net_accept;
    (void)host_net_close;   (void)host_net_stream_open; (void)host_net_stream_accept;
    (void)host_net_send;    (void)host_net_recv;
    (void)host_net_send_datagram; (void)host_net_recv_datagram;
    (void)host_net_resolve;
}
