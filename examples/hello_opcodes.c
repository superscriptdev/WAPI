/**
 * hello_opcodes.c — end-to-end demo of the opcode-based capability ABI.
 *
 * Exercises:
 *   1. Universal capability request via WAPI_IO_OP_CAP_REQUEST.
 *   2. A core-namespace IO op (crypto hash).
 *   3. Vendor namespace registration via wapi_io_t::namespace_register.
 *   4. Submission of a vendor opcode whose handler is plugged in from
 *      the host side via WAPI.registerOpcodeHandler.
 *
 * Build (WASI SDK clang):
 *   clang --target=wasm32-wasi -O2 -nostartfiles                 \
 *     -Wl,--no-entry -Wl,--export=wapi_main -Wl,--export=wapi_frame \
 *     -Wl,--export=__indirect_function_table                     \
 *     -o hello_opcodes.wasm hello_opcodes.c
 */

#include "../include/wapi/wapi.h"
#include "../include/wapi/wapi_crypto.h"

#include <string.h>

/* ---------- helpers ---------- */

static const wapi_io_t*        g_io;
static const wapi_allocator_t* g_alloc;

static uint16_t g_demo_ns;                /* id minted for "dev.wapi.hello" */
#define METHOD_ECHO 0x0001                /* vendor method: echo 4 bytes inline */

static void log_msg(const char* s) {
    wapi_io_op_t op = {0};
    op.opcode = WAPI_IO_OP_LOG;
    op.flags  = WAPI_LOG_INFO;
    op.addr   = (uint64_t)(uintptr_t)s;
    op.len    = strlen(s);
    g_io->submit(g_io->impl, &op, 1);
}

/* Drain one completion event off the io vtable. Returns the event
 * type or 0 if none arrived. */
static uint32_t poll_event(wapi_event_t* ev) {
    int32_t n = g_io->poll(g_io->impl, ev);
    return n > 0 ? ev->type : 0;
}

/* ---------- main ---------- */

WAPI_EXPORT(wapi_main)
wapi_result_t wapi_main(void) {
    g_io    = wapi_io_get();
    g_alloc = wapi_allocator_get();
    if (!g_io || !g_alloc) return WAPI_ERR_UNKNOWN;

    log_msg("[hello_opcodes] booted");

    /* 1. Ask for every capability we might touch. The universal
     *    CAP_REQUEST op takes a capability name and completes with
     *    the current grant state inlined. */
    const char kCryptoName[]  = "wapi.crypto";
    const char kHelloNsName[] = "dev.wapi.hello";
    wapi_cap_state_t cap_state = 0;
    wapi_cap_request(g_io,
        (wapi_stringview_t){ .data = (uint64_t)(uintptr_t)kCryptoName,
                             .length = sizeof(kCryptoName) - 1 },
        &cap_state,
        /*user_data=*/ 1);

    /* 2. Submit a crypto hash op. 32 bytes of "A" → SHA-256 digest
     *    arrives inline in the completion payload. */
    static const char msg[32] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    wapi_crypto_hash(g_io, WAPI_HASH_SHA256, msg, sizeof(msg),
                     /*user_data=*/ 2);

    /* 3. Register a vendor namespace. The host mints an id the first
     *    time it sees this DNS-style name; subsequent calls with the
     *    same name return the same id for this vtable. */
    g_io->namespace_register(g_io->impl,
        (wapi_stringview_t){ .data = (uint64_t)(uintptr_t)kHelloNsName,
                             .length = sizeof(kHelloNsName) - 1 },
        &g_demo_ns);

    /* 4. Submit an op against the vendor namespace. The shim will
     *    dispatch through its registered handler table; if no JS
     *    handler is plugged in, we receive a NOSYS completion. */
    {
        wapi_io_op_t op = {0};
        op.opcode    = WAPI_NS(g_demo_ns, METHOD_ECHO);
        op.flags     = 0xCAFEBABE;
        op.user_data = 3;
        g_io->submit(g_io->impl, &op, 1);
    }

    return WAPI_OK;
}

/* Every frame, drain up to 8 completions and log what they carried. */
WAPI_EXPORT(wapi_frame)
wapi_result_t wapi_frame(uint64_t ts_ns) {
    (void)ts_ns;
    wapi_event_t ev;
    for (int i = 0; i < 8; i++) {
        uint32_t ty = poll_event(&ev);
        if (ty == 0) break;
        if (ty != WAPI_EVENT_IO_COMPLETION) continue;

        char buf[128];
        int n = 0;
        uint64_t ud = ev.io.user_data;
        int32_t  r  = ev.io.result;
        uint32_t f  = ev.io.flags;

        if (ud == 1) {
            n = 0;
            memcpy(buf, "[cap] state=", 12); n = 12;
            buf[n++] = '0' + ev.io.payload[0];
            buf[n++] = ' '; buf[n++] = 'r'; buf[n++] = '=';
            if (r < 0) { buf[n++] = '-'; r = -r; }
            buf[n++] = '0' + (r % 10);
            buf[n] = 0;
        } else if (ud == 2) {
            /* Digest inlined in payload[0..31] for SHA-256. */
            const char hex[] = "0123456789abcdef";
            memcpy(buf, "[hash] ", 7); n = 7;
            for (int j = 0; j < 16 && n < 120; j++) {
                buf[n++] = hex[(ev.io.payload[j] >> 4) & 0xF];
                buf[n++] = hex[ev.io.payload[j] & 0xF];
            }
            buf[n] = 0;
        } else if (ud == 3) {
            memcpy(buf, "[vendor] ", 9); n = 9;
            if (f & 0x0008 /* NOSYS */) {
                memcpy(buf + n, "no handler (NOSYS)", 18); n += 18;
            } else {
                memcpy(buf + n, "handled", 7); n += 7;
            }
            buf[n] = 0;
        } else {
            buf[0] = 0;
        }
        if (buf[0]) log_msg(buf);
    }
    return WAPI_OK;
}
