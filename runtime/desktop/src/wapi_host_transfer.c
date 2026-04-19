/**
 * WAPI Desktop Runtime - Transfer (clipboard/DnD/share unified)
 *
 * Direct imports for the wapi_transfer module. The IO ops
 * WAPI_IO_OP_TRANSFER_OFFER and WAPI_IO_OP_TRANSFER_READ are dispatched
 * from wapi_host_io.c, which calls into wapi_host_transfer_io_offer and
 * wapi_host_transfer_io_read declared here.
 *
 * Single-seat host. POINTED (drag) and ROUTED (share) return NOTSUP;
 * LATENT (clipboard) is backed by SDL3 text clipboard.
 */

#include "wapi_host.h"
#include <string.h>

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

/* revoke: (i32 seat, i32 mode) -> i32 */
static wasm_trap_t* host_transfer_revoke(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  seat = WAPI_ARG_I32(0);
    uint32_t mode = WAPI_ARG_U32(1);
    if (seat != 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    if (mode & WAPI_TRANSFER_LATENT) {
        if (!SDL_SetClipboardText("")) {
            WAPI_RET_I32(WAPI_ERR_IO);
            return NULL;
        }
    }
    /* POINTED/ROUTED: nothing active to revoke. */
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* format_count: (i32 seat, i32 mode) -> i64 */
static wasm_trap_t* host_transfer_format_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  seat = WAPI_ARG_I32(0);
    uint32_t mode = WAPI_ARG_U32(1);
    if (seat != 0) { WAPI_RET_I64(0); return NULL; }

    if (mode == WAPI_TRANSFER_LATENT && SDL_HasClipboardText()) {
        WAPI_RET_I64(1);
        return NULL;
    }
    WAPI_RET_I64(0);
    return NULL;
}

/* format_name: (i32 seat, i32 mode, i64 index, i32 buf, i64 buf_len, i32 out_len) -> i32 */
static wasm_trap_t* host_transfer_format_name(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  seat        = WAPI_ARG_I32(0);
    uint32_t mode        = WAPI_ARG_U32(1);
    uint64_t index       = WAPI_ARG_U64(2);
    uint32_t buf_ptr     = WAPI_ARG_U32(3);
    uint64_t buf_len     = WAPI_ARG_U64(4);
    uint32_t out_len_ptr = WAPI_ARG_U32(5);

    if (seat != 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    if (mode != WAPI_TRANSFER_LATENT || index != 0 || !SDL_HasClipboardText()) {
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

/* has_format: (i32 seat, i32 mode, i32 mime_data, i64 mime_len) -> i32 */
static wasm_trap_t* host_transfer_has_format(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  seat       = WAPI_ARG_I32(0);
    uint32_t mode       = WAPI_ARG_U32(1);
    uint32_t mime_ptr   = WAPI_ARG_U32(2);
    uint64_t mime_len   = WAPI_ARG_U64(3);

    if (seat != 0) { WAPI_RET_I32(0); return NULL; }
    if (mode != WAPI_TRANSFER_LATENT) { WAPI_RET_I32(0); return NULL; }

    const char* mime = (const char*)wapi_wasm_ptr(mime_ptr, (uint32_t)mime_len);
    if (!mime_is_text_plain(mime, mime_len)) { WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(SDL_HasClipboardText() ? 1 : 0);
    return NULL;
}

/* set_action: (i32 seat, i32 action) -> i32 */
static wasm_trap_t* host_transfer_set_action(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    /* No active drag on this host. */
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * IO op handlers (called from wapi_host_io.c)
 *
 * Both return a WAPI_OK/error code and write the completion result
 * directly. Caller is responsible for pushing the completion event.
 *
 * Offer-result encoding for LATENT clipboard write: returns WAPI_OK,
 * out_result = (LATENT << 8) | COPY = 0x101.
 * ============================================================ */

int32_t wapi_host_transfer_io_offer(int32_t seat, uint32_t mode,
                                    uint32_t offer_ptr, uint32_t offer_len,
                                    uint32_t* out_result)
{
    if (seat != 0) return WAPI_ERR_INVAL;
    if (offer_len < 48) return WAPI_ERR_INVAL;

    uint8_t* offer = (uint8_t*)wapi_wasm_ptr(offer_ptr, offer_len);
    if (!offer) return WAPI_ERR_INVAL;

    /* wapi_transfer_offer_t layout (48B):
     *   0: u64 items   (wasm ptr)
     *   8: u32 item_count
     *  12: u32 allowed_actions
     *  16: u64 title.data
     *  24: u64 title.length
     *  32: i32 preview
     *  36: u32 _reserved
     *  40: u64 _reserved2
     */
    uint64_t items_ptr;  uint32_t item_count;
    memcpy(&items_ptr,  offer + 0, 8);
    memcpy(&item_count, offer + 8, 4);

    if (mode & WAPI_TRANSFER_LATENT) {
        /* Find a text/plain item; copy its bytes; SetClipboardText. */
        uint8_t* items = (uint8_t*)wapi_wasm_ptr((uint32_t)items_ptr,
                                                  item_count * 32);
        if (!items) return WAPI_ERR_INVAL;

        for (uint32_t i = 0; i < item_count; i++) {
            uint8_t* it = items + i * 32;
            uint64_t mime_data, mime_len, data_addr, data_len;
            memcpy(&mime_data, it + 0,  8);
            memcpy(&mime_len,  it + 8,  8);
            memcpy(&data_addr, it + 16, 8);
            memcpy(&data_len,  it + 24, 8);

            const char* mime = (const char*)wapi_wasm_ptr((uint32_t)mime_data,
                                                           (uint32_t)mime_len);
            if (!mime_is_text_plain(mime, mime_len)) continue;

            const char* data = (const char*)wapi_wasm_ptr((uint32_t)data_addr,
                                                           (uint32_t)data_len);
            if (!data) return WAPI_ERR_INVAL;

            char* tmp = (char*)malloc((size_t)data_len + 1);
            if (!tmp) return WAPI_ERR_NOMEM;
            if (data_len > 0) memcpy(tmp, data, (size_t)data_len);
            tmp[data_len] = '\0';

            bool ok = SDL_SetClipboardText(tmp);
            free(tmp);
            if (!ok) return WAPI_ERR_IO;

            *out_result = (WAPI_TRANSFER_LATENT << 8) | 1u; /* COPY */
            return WAPI_OK;
        }
        return WAPI_ERR_NOTSUP; /* no compatible item */
    }

    /* POINTED / ROUTED not supported on this host. */
    return WAPI_ERR_NOTSUP;
}

int32_t wapi_host_transfer_io_read(int32_t seat, uint32_t mode,
                                   uint32_t mime_ptr, uint32_t mime_len,
                                   uint32_t buf_ptr, uint32_t buf_len,
                                   uint32_t* out_bytes_written)
{
    if (seat != 0) return WAPI_ERR_INVAL;
    if (mode != WAPI_TRANSFER_LATENT) return WAPI_ERR_NOTSUP;

    const char* mime = (const char*)wapi_wasm_ptr(mime_ptr, mime_len);
    if (!mime_is_text_plain(mime, mime_len)) return WAPI_ERR_NOTSUP;

    if (!SDL_HasClipboardText()) return WAPI_ERR_NOENT;

    char* text = SDL_GetClipboardText();
    if (!text) return WAPI_ERR_IO;

    size_t text_len = strlen(text);
    uint32_t copy = (uint32_t)text_len;
    if (copy > buf_len) copy = buf_len;
    if (copy > 0) {
        void* buf = wapi_wasm_ptr(buf_ptr, copy);
        if (!buf) { SDL_free(text); return WAPI_ERR_INVAL; }
        memcpy(buf, text, copy);
    }
    SDL_free(text);
    *out_bytes_written = copy;
    return WAPI_OK;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_transfer(wasmtime_linker_t* linker) {
    /* revoke: (i32, i32) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_transfer", "revoke", host_transfer_revoke);

    /* format_count: (i32, i32) -> i64 */
    wapi_linker_define(linker, "wapi_transfer", "format_count",
        host_transfer_format_count,
        2, (wasm_valkind_t[]){WASM_I32, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I64});

    /* format_name: (i32, i32, i64, i32, i64, i32) -> i32 */
    wapi_linker_define(linker, "wapi_transfer", "format_name",
        host_transfer_format_name,
        6, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64,
                              WASM_I32, WASM_I64, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    /* has_format: (i32, i32, i32, i64) -> i32 */
    wapi_linker_define(linker, "wapi_transfer", "has_format",
        host_transfer_has_format,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I32, WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});

    /* set_action: (i32, i32) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_transfer", "set_action", host_transfer_set_action);
}
