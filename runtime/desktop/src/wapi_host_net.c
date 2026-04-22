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

/* Forward declaration — probe is defined after resolve_transport in
 * the async-IO section. The registrar below only needs the symbol. */
static wasm_trap_t* host_net_qualities_supported(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults);

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

    /* Sync transport-capability probe — the sole direct import on
     * module "wapi_network". Everything else is async via the I/O
     * bridge. */
    WAPI_DEFINE_1_1(linker, "wapi_network", "qualities_supported",
                    host_net_qualities_supported);
}

/* ============================================================
 * Async I/O op handlers (WAPI_IO_OP_CONNECT / SEND / RECV / CLOSE /
 * NETWORK_RESOLVE)
 * ============================================================
 * Dispatched from wapi_host_io.c. Each runs blocking on the
 * dispatch thread and completes synchronously — the async shape
 * matches the spec but the platform impl is sync for now; a
 * future upgrade can route through IOCP / epoll for true async.
 */

/* ---- Transport resolution ----
 *
 * wapi_network.h models qualities (reliability, ordering, framing,
 * encryption, multiplexing, low-latency, broadcast) as a bitmask and
 * lets the platform pick a concrete transport. The table below is
 * the exhaustive mapping — every quality combination resolves to
 * exactly one enum value, and unsupported combinations get their own
 * explicit branch so callers see a precise error (NOTSUP for missing
 * platform support, INVAL for self-contradictory combinations). */

/* Defined in wapi_host_net_tls.c. */
int  wapi_host_net_tls_connect(wapi_socket_t sock, const char* sni,
                               size_t sni_len, bool dtls, void** out_state);
int  wapi_host_net_tls_send   (void* state, wapi_socket_t sock,
                               const void* data, size_t len);
int  wapi_host_net_tls_recv   (void* state, wapi_socket_t sock,
                               void* buf, size_t buf_len);
void wapi_host_net_tls_close  (void* state, wapi_socket_t sock);

/* Defined in wapi_host_net_quic.c. */
int  wapi_host_net_quic_connect(const char* host, size_t host_len,
                                uint16_t port, void** out_state);
int  wapi_host_net_quic_send   (void* state, const void* data, size_t len);
int  wapi_host_net_quic_recv   (void* state, void* buf, size_t cap);
void wapi_host_net_quic_close  (void* state);
bool wapi_host_net_quic_available(void);

typedef enum resolved_transport_t {
    T_TCP           = 1,  /* reliable + ordered (no encryption)        */
    T_TCP_TLS       = 2,  /* reliable + ordered + encrypted             */
    T_UDP           = 3,  /* framed + low_latency (no reliability)      */
    T_UDP_DTLS      = 4,  /* framed + low_latency + encrypted           */
    T_QUIC          = 5,  /* reliable + ordered + framed + multiplexed  */
    T_MULTICAST     = 6,  /* reliable + ordered + framed + broadcast    */
    T_UNSPEC        = 0,
} resolved_transport_t;

#define Q_RELIABLE    0x01
#define Q_ORDERED     0x02
#define Q_FRAMED      0x04
#define Q_ENCRYPTED   0x08
#define Q_MULTIPLEXED 0x10
#define Q_LOW_LATENCY 0x20
#define Q_BROADCAST   0x40

static resolved_transport_t resolve_transport(uint32_t q) {
    bool reliable   = (q & Q_RELIABLE)    != 0;
    bool ordered    = (q & Q_ORDERED)     != 0;
    bool framed     = (q & Q_FRAMED)      != 0;
    bool encrypted  = (q & Q_ENCRYPTED)   != 0;
    bool mux        = (q & Q_MULTIPLEXED) != 0;
    bool lowlat     = (q & Q_LOW_LATENCY) != 0;
    bool broadcast  = (q & Q_BROADCAST)   != 0;

    /* Reliable multicast is its own family. */
    if (broadcast) {
        if (reliable && ordered && framed) return T_MULTICAST;
        /* Bare broadcast without reliability = UDP multicast socket,
         * but the caller should set MESSAGE_FRAMED + LOW_LATENCY for
         * that. Disallow ambiguous combos. */
        return T_UNSPEC;
    }

    /* Multiplexed streams map to QUIC only. */
    if (mux) {
        if (reliable && ordered && framed) return T_QUIC;
        return T_UNSPEC;
    }

    /* Byte-stream transports: TCP or TCP+TLS. */
    if (reliable && ordered && !framed) {
        return encrypted ? T_TCP_TLS : T_TCP;
    }

    /* Datagram transports: UDP or DTLS. */
    if (framed && !reliable) {
        (void)lowlat; /* low_latency is a hint, not a discriminator */
        return encrypted ? T_UDP_DTLS : T_UDP;
    }

    /* Any other combination (e.g. ordered without reliable, framed +
     * reliable without mux, encrypted without reliable) is either
     * nonsensical or not yet mapped to a concrete transport. */
    return T_UNSPEC;
}

/* wapi_net_qualities_supported — pure probe; no sockets or DLL
 * registration. Forward-declared above the registrar. */
static wasm_trap_t* host_net_qualities_supported(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t q = WAPI_ARG_U32(0);
    resolved_transport_t t = resolve_transport(q);
    int32_t ret;
    switch (t) {
    case T_TCP:
    case T_UDP:
    case T_TCP_TLS:
    case T_UDP_DTLS:    ret = 1; break;
    case T_QUIC:        ret = wapi_host_net_quic_available() ? 1 : 0; break;
    case T_MULTICAST:   ret = 0; break; /* PGM deprecated on Win */
    case T_UNSPEC:
    default:            ret = WAPI_ERR_INVAL; break;
    }
    WAPI_RET_I32(ret);
    return NULL;
}

/* CONNECT: addr=address_str (utf-8), flags=qualities bitmask.
 *   result = conn handle on success; negative WAPI_ERR_* on failure. */
void wapi_host_net_connect_op(op_ctx_t* c) {
    if (c->len == 0) { c->result = WAPI_ERR_INVAL; return; }
    const char* url = (const char*)wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len);
    if (!url) { c->result = WAPI_ERR_INVAL; return; }

    resolved_transport_t t = resolve_transport(c->flags);

    char host[256], port[16];
    if (!parse_host_port(url, (uint32_t)c->len, host, sizeof(host), port, sizeof(port))) {
        c->result = WAPI_ERR_INVAL; return;
    }

    /* QUIC has no Winsock socket — msquic owns the UDP socket
     * internally. Handle it before the BSD-style branch. */
    if (t == T_QUIC) {
        uint16_t p = (uint16_t)atoi(port);
        void* quic_state = NULL;
        int rc = wapi_host_net_quic_connect(host, strlen(host), p, &quic_state);
        if (rc != 0) {
            c->result = (rc == -2) ? WAPI_ERR_NOTSUP : WAPI_ERR_IO;
            return;
        }
        int32_t h = wapi_handle_alloc(WAPI_HTYPE_NET_CONN);
        if (h == 0) {
            wapi_host_net_quic_close(quic_state);
            c->result = WAPI_ERR_NOMEM; return;
        }
        g_rt.handles[h].data.net_conn.sock        = WAPI_INVALID_SOCKET;
        g_rt.handles[h].data.net_conn.transport   = (uint32_t)t;
        g_rt.handles[h].data.net_conn.connected   = true;
        g_rt.handles[h].data.net_conn.nonblocking = false;
        g_rt.handles[h].data.net_conn.quic_state  = quic_state;
        c->result = h;
        c->inline_payload = true;
        memcpy(c->payload, &h, 4);
        if (c->result_ptr) wapi_wasm_write_i32((uint32_t)c->result_ptr, h);
        return;
    }

    int sock_type, ip_proto;
    switch (t) {
    case T_TCP:
    case T_TCP_TLS:
        sock_type = SOCK_STREAM; ip_proto = IPPROTO_TCP; break;
    case T_UDP:
    case T_UDP_DTLS:
        sock_type = SOCK_DGRAM;  ip_proto = IPPROTO_UDP; break;
    case T_MULTICAST: c->result = WAPI_ERR_NOTSUP; return; /* PGM deprecated on Win */
    default:          c->result = WAPI_ERR_INVAL;  return;
    }

    struct addrinfo hints; memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = sock_type;
    hints.ai_protocol = ip_proto;

    struct addrinfo* ai = NULL;
    if (getaddrinfo(host, port, &hints, &ai) != 0 || !ai) {
        c->result = WAPI_ERR_IO; return;
    }
    wapi_socket_t sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sock == WAPI_INVALID_SOCKET) {
        freeaddrinfo(ai); c->result = WAPI_ERR_IO; return;
    }
    /* `connect` on a UDP socket sets the peer so subsequent send/recv
     * uses that address without sendto/recvfrom. For TCP it performs
     * the three-way handshake. */
    if (connect(sock, ai->ai_addr, (int)ai->ai_addrlen) != 0) {
        int err = WAPI_LAST_NET_ERROR;
        freeaddrinfo(ai); WAPI_CLOSESOCKET(sock);
        c->result = net_error_to_wapi(err); return;
    }
    freeaddrinfo(ai);

    /* TLS/DTLS handshake runs on the still-blocking socket. Only flip
     * to non-blocking after the handshake so send_all / recv inside
     * Schannel can loop without juggling EWOULDBLOCK. */
    void* tls_state = NULL;
    if (t == T_TCP_TLS || t == T_UDP_DTLS) {
        if (wapi_host_net_tls_connect(sock, host, strlen(host),
                                      (t == T_UDP_DTLS), &tls_state) != 0) {
            WAPI_CLOSESOCKET(sock);
            c->result = WAPI_ERR_IO; return;
        }
    }
    set_nonblocking(sock);

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_NET_CONN);
    if (h == 0) {
        if (tls_state) wapi_host_net_tls_close(tls_state, sock);
        WAPI_CLOSESOCKET(sock);
        c->result = WAPI_ERR_NOMEM; return;
    }
    g_rt.handles[h].data.net_conn.sock        = sock;
    g_rt.handles[h].data.net_conn.transport   = (uint32_t)t;
    g_rt.handles[h].data.net_conn.connected   = true;
    g_rt.handles[h].data.net_conn.nonblocking = true;
    g_rt.handles[h].data.net_conn.tls_state   = tls_state;
    c->result = h;
    c->inline_payload = true;
    memcpy(c->payload, &h, 4);
    if (c->result_ptr) wapi_wasm_write_i32((uint32_t)c->result_ptr, h);
}

/* SEND: fd=conn_handle, addr=data, len=len.
 *   result = bytes sent (>=0) or negative error. */
void wapi_host_net_send_op(op_ctx_t* c) {
    int32_t conn_h = c->fd;
    if (!wapi_handle_valid(conn_h, WAPI_HTYPE_NET_CONN)) {
        c->result = WAPI_ERR_BADF; return;
    }
    const void* data = c->len ? wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len) : NULL;
    if (c->len > 0 && !data) { c->result = WAPI_ERR_INVAL; return; }
    wapi_socket_t sock = g_rt.handles[conn_h].data.net_conn.sock;

    /* QUIC owns its own transport; no TCP socket. */
    void* quic_state = g_rt.handles[conn_h].data.net_conn.quic_state;
    if (quic_state) {
        int n = wapi_host_net_quic_send(quic_state, data, (size_t)c->len);
        c->result = (n < 0) ? WAPI_ERR_IO : n;
        return;
    }

    /* TLS/DTLS: encrypt path wraps send; it handles partial sends
     * internally so the caller contract is "all or nothing". */
    void* tls_state = g_rt.handles[conn_h].data.net_conn.tls_state;
    if (tls_state) {
        int n = wapi_host_net_tls_send(tls_state, sock, data, (size_t)c->len);
        c->result = (n < 0) ? WAPI_ERR_IO : n;
        return;
    }

    /* Non-blocking sockets: loop on EAGAIN so callers don't see
     * partial sends driven by socket buffer pressure. The op stays
     * a single completion, matching the spec shape. */
    const uint8_t* p = (const uint8_t*)data;
    size_t remaining = (size_t)c->len;
    size_t sent = 0;
    while (remaining > 0) {
        int n = send(sock, (const char*)p, (int)remaining, 0);
        if (n > 0) { p += n; remaining -= (size_t)n; sent += (size_t)n; continue; }
        int err = WAPI_LAST_NET_ERROR;
        if (err == WAPI_EWOULDBLOCK) {
            /* Park briefly; a future async rewrite uses IOCP/epoll. */
            Sleep(1);
            continue;
        }
        c->result = net_error_to_wapi(err);
        return;
    }
    c->result = (int32_t)sent;
}

/* RECV: fd=conn_handle, addr=buffer, len=cap.
 *   result = bytes received (>0), 0=EOF, negative=error. */
void wapi_host_net_recv_op(op_ctx_t* c) {
    int32_t conn_h = c->fd;
    if (!wapi_handle_valid(conn_h, WAPI_HTYPE_NET_CONN)) {
        c->result = WAPI_ERR_BADF; return;
    }
    void* buf = c->len ? wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len) : NULL;
    if (c->len > 0 && !buf) { c->result = WAPI_ERR_INVAL; return; }
    wapi_socket_t sock = g_rt.handles[conn_h].data.net_conn.sock;

    void* quic_state = g_rt.handles[conn_h].data.net_conn.quic_state;
    if (quic_state) {
        int n = wapi_host_net_quic_recv(quic_state, buf, (size_t)c->len);
        c->result = (n < 0) ? WAPI_ERR_IO : n;
        return;
    }

    void* tls_state = g_rt.handles[conn_h].data.net_conn.tls_state;
    if (tls_state) {
        int n = wapi_host_net_tls_recv(tls_state, sock, buf, (size_t)c->len);
        c->result = (n < 0) ? WAPI_ERR_IO : n;
        return;
    }

    for (;;) {
        int n = recv(sock, (char*)buf, (int)c->len, 0);
        if (n >= 0) { c->result = n; return; }
        int err = WAPI_LAST_NET_ERROR;
        if (err == WAPI_EWOULDBLOCK) { Sleep(1); continue; }
        c->result = net_error_to_wapi(err);
        return;
    }
}

/* CLOSE: fd=any net handle. */
void wapi_host_net_close_op(op_ctx_t* c) {
    int32_t h = c->fd;
    if (!wapi_handle_valid_any(h)) { c->result = WAPI_ERR_BADF; return; }
    wapi_handle_type_t t = g_rt.handles[h].type;
    if (t == WAPI_HTYPE_NET_CONN) {
        wapi_socket_t sock = g_rt.handles[h].data.net_conn.sock;
        void* quic_state = g_rt.handles[h].data.net_conn.quic_state;
        if (quic_state) {
            wapi_host_net_quic_close(quic_state);
        } else {
            void* tls_state = g_rt.handles[h].data.net_conn.tls_state;
            if (tls_state) wapi_host_net_tls_close(tls_state, sock);
            WAPI_CLOSESOCKET(sock);
        }
    } else if (t == WAPI_HTYPE_NET_LISTENER) {
        WAPI_CLOSESOCKET(g_rt.handles[h].data.net_conn.sock);
    } else {
        c->result = WAPI_ERR_BADF; return;
    }
    wapi_handle_free(h);
    c->result = 0;
}

/* NETWORK_RESOLVE: addr=hostname, addr2=address buffer.
 *   Writes a NUL-separated list of "host:port"-style strings (here
 *   just "ip" — caller re-uses the port from their original request).
 *   result = count of addresses resolved. */
void wapi_host_net_resolve_op(op_ctx_t* c) {
    if (c->len == 0) { c->result = WAPI_ERR_INVAL; return; }
    const char* hostname = (const char*)wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len);
    if (!hostname) { c->result = WAPI_ERR_INVAL; return; }

    /* NUL-terminate locally so getaddrinfo can parse. */
    char host[256];
    size_t n = c->len < sizeof(host) - 1 ? (size_t)c->len : sizeof(host) - 1;
    memcpy(host, hostname, n);
    host[n] = 0;

    struct addrinfo hints; memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* ai = NULL;
    if (getaddrinfo(host, NULL, &hints, &ai) != 0 || !ai) {
        c->result = WAPI_ERR_NOENT; return;
    }

    uint8_t* out    = c->len2 ? (uint8_t*)wapi_wasm_ptr((uint32_t)c->addr2, (uint32_t)c->len2) : NULL;
    size_t   cap    = (size_t)c->len2;
    size_t   used   = 0;
    int32_t  count  = 0;
    for (struct addrinfo* p = ai; p; p = p->ai_next) {
        char ipstr[INET6_ADDRSTRLEN] = {0};
        if (p->ai_family == AF_INET) {
            struct sockaddr_in* sa = (struct sockaddr_in*)p->ai_addr;
            inet_ntop(AF_INET, &sa->sin_addr, ipstr, sizeof(ipstr));
        } else if (p->ai_family == AF_INET6) {
            struct sockaddr_in6* sa = (struct sockaddr_in6*)p->ai_addr;
            inet_ntop(AF_INET6, &sa->sin6_addr, ipstr, sizeof(ipstr));
        } else continue;

        size_t slen = strlen(ipstr);
        if (out && used + slen + 1 <= cap) {
            memcpy(out + used, ipstr, slen);
            out[used + slen] = 0;
        }
        used += slen + 1;
        count++;
    }
    freeaddrinfo(ai);

    if (c->result_ptr) wapi_wasm_write_u64((uint32_t)c->result_ptr, (uint64_t)used);
    c->result = count;
}
