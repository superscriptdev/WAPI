/**
 * WAPI SDL Runtime - Transfer (clipboard/DnD/share unified)
 *
 * Single-seat host. POINTED (drag) start_drag and ROUTED (share) return
 * NOTSUP. LATENT (clipboard) is backed by SDL3 text clipboard.
 *
 * IO ops WAPI_IO_OP_TRANSFER_OFFER/READ are dispatched from
 * wapi_host_io.c, which calls into wapi_host_transfer_io_offer /
 * wapi_host_transfer_io_read defined here.
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

static int32_t host_transfer_revoke(wasm_exec_env_t env,
                                    int32_t seat, uint32_t mode) {
    (void)env;
    if (seat != 0) return WAPI_ERR_INVAL;
    if (mode & WAPI_TRANSFER_LATENT) {
        if (!SDL_SetClipboardText("")) return WAPI_ERR_IO;
    }
    return WAPI_OK;
}

static int64_t host_transfer_format_count(wasm_exec_env_t env,
                                          int32_t seat, uint32_t mode) {
    (void)env;
    if (seat != 0) return 0;
    if (mode == WAPI_TRANSFER_LATENT && SDL_HasClipboardText()) return 1;
    return 0;
}

static int32_t host_transfer_format_name(wasm_exec_env_t env,
                                         int32_t seat, uint32_t mode,
                                         uint64_t index, uint32_t buf_ptr,
                                         uint64_t buf_len, uint32_t out_len_ptr) {
    (void)env;
    if (seat != 0) return WAPI_ERR_INVAL;
    if (mode != WAPI_TRANSFER_LATENT || index != 0 || !SDL_HasClipboardText())
        return WAPI_ERR_RANGE;

    uint64_t copy = MIME_TEXT_PLAIN_LEN;
    if (copy > buf_len) copy = buf_len;
    if (copy > 0) {
        void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)copy);
        if (!buf) return WAPI_ERR_INVAL;
        memcpy(buf, MIME_TEXT_PLAIN, (size_t)copy);
    }
    wapi_wasm_write_u64(out_len_ptr, MIME_TEXT_PLAIN_LEN);
    return WAPI_OK;
}

static int32_t host_transfer_has_format(wasm_exec_env_t env,
                                        int32_t seat, uint32_t mode,
                                        uint32_t mime_ptr, uint64_t mime_len) {
    (void)env;
    if (seat != 0) return 0;
    if (mode != WAPI_TRANSFER_LATENT) return 0;
    const char* mime = (const char*)wapi_wasm_ptr(mime_ptr, (uint32_t)mime_len);
    if (!mime_is_text_plain(mime, mime_len)) return 0;
    return SDL_HasClipboardText() ? 1 : 0;
}

static int32_t host_transfer_set_action(wasm_exec_env_t env,
                                        int32_t seat, int32_t action) {
    (void)env; (void)seat; (void)action;
    return WAPI_ERR_NOTSUP;
}

/* ============================================================
 * IO op handlers (called from wapi_host_io.c)
 * ============================================================ */

int32_t wapi_host_transfer_io_offer(int32_t seat, uint32_t mode,
                                    uint32_t offer_ptr, uint32_t offer_len,
                                    uint32_t* out_result)
{
    if (seat != 0) return WAPI_ERR_INVAL;
    if (offer_len < 48) return WAPI_ERR_INVAL;

    uint8_t* offer = (uint8_t*)wapi_wasm_ptr(offer_ptr, offer_len);
    if (!offer) return WAPI_ERR_INVAL;

    uint64_t items_ptr;  uint32_t item_count;
    memcpy(&items_ptr,  offer + 0, 8);
    memcpy(&item_count, offer + 8, 4);

    if (mode & WAPI_TRANSFER_LATENT) {
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

            *out_result = (WAPI_TRANSFER_LATENT << 8) | 1u;
            return WAPI_OK;
        }
        return WAPI_ERR_NOTSUP;
    }

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
 * WAMR signature codes:
 *   i = i32, I = i64, * = pointer (i32 in wasm32), $ = i32 string ptr
 * ============================================================ */

static NativeSymbol g_symbols[] = {
    /* (seat, mode) -> i32 */
    { "revoke",       (void*)host_transfer_revoke,       "(ii)i",        NULL },
    /* (seat, mode) -> i64 */
    { "format_count", (void*)host_transfer_format_count, "(ii)I",        NULL },
    /* (seat, mode, index, buf, buf_len, out_len) -> i32 */
    { "format_name",  (void*)host_transfer_format_name,  "(iiIiIi)i",    NULL },
    /* (seat, mode, mime_ptr, mime_len) -> i32 */
    { "has_format",   (void*)host_transfer_has_format,   "(iiiI)i",      NULL },
    /* (seat, action) -> i32 */
    { "set_action",   (void*)host_transfer_set_action,   "(ii)i",        NULL },
};

wapi_cap_registration_t wapi_host_transfer_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_transfer",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
