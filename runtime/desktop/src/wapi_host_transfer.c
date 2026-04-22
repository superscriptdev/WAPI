/**
 * WAPI Desktop Runtime - Transfer (clipboard/DnD/share unified)
 *
 * LATENT (clipboard) backed by wapi_plat clipboard (text/plain only).
 * POINTED (drag) and ROUTED (share) return NOTSUP for now — see
 * NEXT_STEPS.md.
 */

#include "wapi_host.h"

#define WAPI_TRANSFER_LATENT   0x01u
#define WAPI_TRANSFER_POINTED  0x02u
#define WAPI_TRANSFER_ROUTED   0x04u

#define MIME_TEXT_PLAIN     "text/plain"
#define MIME_TEXT_PLAIN_LEN 10

static int mime_is_text_plain(const char* data, uint64_t len) {
    return data && len == MIME_TEXT_PLAIN_LEN
        && memcmp(data, MIME_TEXT_PLAIN, MIME_TEXT_PLAIN_LEN) == 0;
}

/* ============================================================
 * Direct imports
 * ============================================================ */

static wasm_trap_t* host_transfer_revoke(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t seat = WAPI_ARG_I32(0);
    uint32_t mode = WAPI_ARG_U32(1);
    if (seat != 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    if (mode & WAPI_TRANSFER_LATENT) {
        if (!wapi_plat_clipboard_set_text("", 0)) { WAPI_RET_I32(WAPI_ERR_IO); return NULL; }
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_transfer_format_count(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  seat = WAPI_ARG_I32(0);
    uint32_t mode = WAPI_ARG_U32(1);
    if (seat != 0) { WAPI_RET_I64(0); return NULL; }
    if (mode == WAPI_TRANSFER_LATENT && wapi_plat_clipboard_has_text()) { WAPI_RET_I64(1); return NULL; }
    WAPI_RET_I64(0);
    return NULL;
}

static wasm_trap_t* host_transfer_format_name(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  seat        = WAPI_ARG_I32(0);
    uint32_t mode        = WAPI_ARG_U32(1);
    uint64_t index       = WAPI_ARG_U64(2);
    uint32_t buf_ptr     = WAPI_ARG_U32(3);
    uint64_t buf_len     = WAPI_ARG_U64(4);
    uint32_t out_len_ptr = WAPI_ARG_U32(5);

    if (seat != 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    if (mode != WAPI_TRANSFER_LATENT || index != 0 || !wapi_plat_clipboard_has_text()) {
        WAPI_RET_I32(WAPI_ERR_RANGE);
        return NULL;
    }

    uint64_t copy = MIME_TEXT_PLAIN_LEN;
    if (copy > buf_len) copy = buf_len;
    if (copy > 0) {
        void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)copy);
        if (!buf) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
        memcpy(buf, MIME_TEXT_PLAIN, (size_t)copy);
    }
    wapi_wasm_write_u64(out_len_ptr, MIME_TEXT_PLAIN_LEN);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_transfer_has_format(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  seat     = WAPI_ARG_I32(0);
    uint32_t mode     = WAPI_ARG_U32(1);
    uint32_t mime_ptr = WAPI_ARG_U32(2);
    uint64_t mime_len = WAPI_ARG_U64(3);

    if (seat != 0 || mode != WAPI_TRANSFER_LATENT) { WAPI_RET_I32(0); return NULL; }
    const char* mime = (const char*)wapi_wasm_ptr(mime_ptr, (uint32_t)mime_len);
    if (!mime_is_text_plain(mime, mime_len)) { WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(wapi_plat_clipboard_has_text() ? 1 : 0);
    return NULL;
}

static wasm_trap_t* host_transfer_set_action(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * IO op handlers
 * ============================================================
 * Dispatched from wapi_host_io.c when the IO bridge sees
 * WAPI_IO_OP_TRANSFER_OFFER (0x310) / WAPI_IO_OP_TRANSFER_READ
 * (0x311). op_ctx_t layout is shared via wapi_host.h. */

void wapi_host_transfer_offer_op(op_ctx_t* c) {
    int32_t  seat = c->fd;
    uint32_t mode = c->flags;

    if (seat != 0) { c->result = WAPI_ERR_INVAL; return; }
    if (c->len < 48) { c->result = WAPI_ERR_INVAL; return; }

    uint8_t* offer = (uint8_t*)wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len);
    if (!offer) { c->result = WAPI_ERR_INVAL; return; }

    uint64_t items_ptr; uint32_t item_count;
    memcpy(&items_ptr,  offer + 0, 8);
    memcpy(&item_count, offer + 8, 4);

    if (!(mode & WAPI_TRANSFER_LATENT)) { c->result = WAPI_ERR_NOTSUP; return; }

    uint8_t* items = (uint8_t*)wapi_wasm_ptr((uint32_t)items_ptr, item_count * 32);
    if (!items) { c->result = WAPI_ERR_INVAL; return; }

    for (uint32_t i = 0; i < item_count; i++) {
        uint8_t* it = items + i * 32;
        uint64_t mime_data, mime_len, data_addr, data_len;
        memcpy(&mime_data, it +  0, 8);
        memcpy(&mime_len,  it +  8, 8);
        memcpy(&data_addr, it + 16, 8);
        memcpy(&data_len,  it + 24, 8);

        const char* mime = (const char*)wapi_wasm_ptr((uint32_t)mime_data, (uint32_t)mime_len);
        if (!mime_is_text_plain(mime, mime_len)) continue;

        const char* data = (const char*)wapi_wasm_ptr((uint32_t)data_addr, (uint32_t)data_len);
        if (!data && data_len > 0) { c->result = WAPI_ERR_INVAL; return; }

        if (!wapi_plat_clipboard_set_text(data ? data : "", (size_t)data_len)) {
            c->result = WAPI_ERR_IO;
            return;
        }

        /* Pack (mode << 8) | action where action=1 means COPY */
        c->result = (int32_t)((WAPI_TRANSFER_LATENT << 8) | 1u);
        return;
    }
    c->result = WAPI_ERR_NOTSUP;
}

void wapi_host_transfer_read_op(op_ctx_t* c) {
    int32_t  seat = c->fd;
    uint32_t mode = c->flags;

    if (seat != 0) { c->result = WAPI_ERR_INVAL; return; }
    if (mode != WAPI_TRANSFER_LATENT) { c->result = WAPI_ERR_NOTSUP; return; }

    const char* mime = (const char*)wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len);
    if (!mime_is_text_plain(mime, c->len)) { c->result = WAPI_ERR_NOTSUP; return; }
    if (!wapi_plat_clipboard_has_text())  { c->result = WAPI_ERR_NOENT;  return; }

    size_t total = wapi_plat_clipboard_get_text(NULL, 0);
    if (total == 0) { c->result = WAPI_ERR_NOENT; return; }

    uint32_t buf_len = (uint32_t)c->len2;
    uint32_t copy    = (uint32_t)(total < buf_len ? total : buf_len);
    if (copy > 0) {
        void* buf = wapi_wasm_ptr((uint32_t)c->addr2, copy);
        if (!buf) { c->result = WAPI_ERR_INVAL; return; }
        wapi_plat_clipboard_get_text((char*)buf, copy);
    }
    c->result = (int32_t)copy;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_transfer(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_transfer", "revoke", host_transfer_revoke);

    wapi_linker_define(linker, "wapi_transfer", "format_count", host_transfer_format_count,
        2, (wasm_valkind_t[]){WASM_I32, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I64});

    wapi_linker_define(linker, "wapi_transfer", "format_name", host_transfer_format_name,
        6, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64, WASM_I32, WASM_I64, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    wapi_linker_define(linker, "wapi_transfer", "has_format", host_transfer_has_format,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I32, WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_2_1(linker, "wapi_transfer", "set_action", host_transfer_set_action);
}
