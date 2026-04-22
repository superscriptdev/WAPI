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
#define MIME_IMAGE_BMP      "image/bmp"
#define MIME_IMAGE_BMP_LEN  9
#define MIME_URI_LIST       "text/uri-list"
#define MIME_URI_LIST_LEN   13

static int mime_is(const char* expected, size_t expected_len,
                   const char* data, uint64_t len) {
    return data && len == expected_len && memcmp(data, expected, expected_len) == 0;
}
#define mime_is_text_plain(d, l) mime_is(MIME_TEXT_PLAIN, MIME_TEXT_PLAIN_LEN, (d), (l))
#define mime_is_image_bmp(d, l)  mime_is(MIME_IMAGE_BMP,  MIME_IMAGE_BMP_LEN,  (d), (l))
#define mime_is_uri_list(d, l)   mime_is(MIME_URI_LIST,   MIME_URI_LIST_LEN,   (d), (l))

/* Enumerate clipboard formats currently on offer for LATENT mode.
 * Order is stable — format_name indexes into this list. */
typedef struct { const char* mime; size_t len; bool (*check)(void); } mime_entry_t;
static bool has_text (void) { return wapi_plat_clipboard_has_text(); }
static bool has_image(void) { return wapi_plat_clipboard_has_image(); }
static bool has_files(void) { return wapi_plat_clipboard_has_files(); }
static const mime_entry_t LATENT_MIMES[] = {
    { MIME_TEXT_PLAIN, MIME_TEXT_PLAIN_LEN, has_text },
    { MIME_IMAGE_BMP,  MIME_IMAGE_BMP_LEN,  has_image },
    { MIME_URI_LIST,   MIME_URI_LIST_LEN,   has_files },
};
#define LATENT_MIME_COUNT (sizeof(LATENT_MIMES) / sizeof(LATENT_MIMES[0]))

static int latent_format_index(uint32_t i, const mime_entry_t** out) {
    uint32_t seen = 0;
    for (size_t k = 0; k < LATENT_MIME_COUNT; k++) {
        if (!LATENT_MIMES[k].check()) continue;
        if (seen == i) { *out = &LATENT_MIMES[k]; return 0; }
        seen++;
    }
    return -1;
}

static uint64_t latent_format_count(void) {
    uint64_t n = 0;
    for (size_t k = 0; k < LATENT_MIME_COUNT; k++) {
        if (LATENT_MIMES[k].check()) n++;
    }
    return n;
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
    if (mode != WAPI_TRANSFER_LATENT) { WAPI_RET_I64(0); return NULL; }
    WAPI_RET_I64(latent_format_count());
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

    if (seat != 0 || mode != WAPI_TRANSFER_LATENT) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    const mime_entry_t* e = NULL;
    if (latent_format_index((uint32_t)index, &e) != 0) {
        WAPI_RET_I32(WAPI_ERR_RANGE); return NULL;
    }

    uint64_t copy = e->len;
    if (copy > buf_len) copy = buf_len;
    if (copy > 0) {
        void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)copy);
        if (!buf) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
        memcpy(buf, e->mime, (size_t)copy);
    }
    if (out_len_ptr) wapi_wasm_write_u64(out_len_ptr, (uint64_t)e->len);
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
    if (mime_is_text_plain(mime, mime_len)) { WAPI_RET_I32(has_text()  ? 1 : 0); return NULL; }
    if (mime_is_image_bmp (mime, mime_len)) { WAPI_RET_I32(has_image() ? 1 : 0); return NULL; }
    if (mime_is_uri_list  (mime, mime_len)) { WAPI_RET_I32(has_files() ? 1 : 0); return NULL; }
    WAPI_RET_I32(0);
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

    if (!(mode & (WAPI_TRANSFER_LATENT | WAPI_TRANSFER_ROUTED))) {
        c->result = WAPI_ERR_NOTSUP;
        return;
    }

    uint8_t* items = (uint8_t*)wapi_wasm_ptr((uint32_t)items_ptr, item_count * 32);
    if (!items) { c->result = WAPI_ERR_INVAL; return; }

    /* Offer's title (at offset 16 inside wapi_transfer_offer_t) goes
     * through to the platform share sheet. */
    uint64_t title_data, title_len;
    memcpy(&title_data, offer + 16, 8);
    memcpy(&title_len,  offer + 24, 8);
    const char* title = title_len
        ? (const char*)wapi_wasm_ptr((uint32_t)title_data, (uint32_t)title_len)
        : NULL;

    if (mode & WAPI_TRANSFER_ROUTED) {
        /* ROUTED / system share. Find the first MIME the platform
         * share sheet can express and hand it off. */
        for (uint32_t i = 0; i < item_count; i++) {
            uint8_t* it = items + i * 32;
            uint64_t mime_data, mime_len, data_addr, data_len;
            memcpy(&mime_data, it +  0, 8);
            memcpy(&mime_len,  it +  8, 8);
            memcpy(&data_addr, it + 16, 8);
            memcpy(&data_len,  it + 24, 8);

            const char* mime = (const char*)wapi_wasm_ptr((uint32_t)mime_data, (uint32_t)mime_len);
            const void* data = data_len ? wapi_wasm_ptr((uint32_t)data_addr, (uint32_t)data_len) : NULL;
            if (data_len > 0 && !data) { c->result = WAPI_ERR_INVAL; return; }

            /* Route through any live surface so the share sheet can
             * parent correctly; NULL is acceptable (system places it). */
            wapi_plat_window_t* parent = NULL;
            for (int k = 1; k < WAPI_MAX_HANDLES; k++) {
                if (g_rt.handles[k].type == WAPI_HTYPE_SURFACE &&
                    g_rt.handles[k].data.window) {
                    parent = g_rt.handles[k].data.window;
                    break;
                }
            }

            if (wapi_plat_share_data(parent, mime, (size_t)mime_len,
                                     data, (size_t)data_len,
                                     title, (size_t)title_len)) {
                c->result = (int32_t)((WAPI_TRANSFER_ROUTED << 8) | 1u);
                return;
            }
        }
        /* Fall through: if ROUTED was the only requested mode, report
         * NOTSUP (no item matched). If LATENT was also requested, the
         * LATENT pass below gets a shot. */
        if (!(mode & WAPI_TRANSFER_LATENT)) {
            c->result = WAPI_ERR_NOTSUP;
            return;
        }
    }

    /* Scan items for the first MIME the host can express on the
     * clipboard. text/plain wins over image/bmp wins over
     * text/uri-list only because the guest usually passes a single
     * item; multi-item offers use the first match. */
    for (uint32_t i = 0; i < item_count; i++) {
        uint8_t* it = items + i * 32;
        uint64_t mime_data, mime_len, data_addr, data_len;
        memcpy(&mime_data, it +  0, 8);
        memcpy(&mime_len,  it +  8, 8);
        memcpy(&data_addr, it + 16, 8);
        memcpy(&data_len,  it + 24, 8);

        const char* mime = (const char*)wapi_wasm_ptr((uint32_t)mime_data, (uint32_t)mime_len);
        const void* data = data_len ? wapi_wasm_ptr((uint32_t)data_addr, (uint32_t)data_len) : NULL;
        if (data_len > 0 && !data) { c->result = WAPI_ERR_INVAL; return; }

        if (mime_is_text_plain(mime, mime_len)) {
            if (!wapi_plat_clipboard_set_text(data ? (const char*)data : "", (size_t)data_len)) {
                c->result = WAPI_ERR_IO; return;
            }
            c->result = (int32_t)((WAPI_TRANSFER_LATENT << 8) | 1u);
            return;
        }
        if (mime_is_image_bmp(mime, mime_len)) {
            if (!wapi_plat_clipboard_set_image(data, (size_t)data_len)) {
                c->result = WAPI_ERR_IO; return;
            }
            c->result = (int32_t)((WAPI_TRANSFER_LATENT << 8) | 1u);
            return;
        }
        /* text/uri-list write-back via CF_HDROP is deferred — it
         * needs a DROPFILES struct + path concat, and the use case
         * (copying a file list from one app to another) is rare. */
    }
    c->result = WAPI_ERR_NOTSUP;
}

void wapi_host_transfer_read_op(op_ctx_t* c) {
    int32_t  seat = c->fd;
    uint32_t mode = c->flags;

    if (seat != 0) { c->result = WAPI_ERR_INVAL; return; }

    const char* mime = (const char*)wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len);
    uint32_t buf_len = (uint32_t)c->len2;
    void*    buf     = buf_len ? wapi_wasm_ptr((uint32_t)c->addr2, buf_len) : NULL;
    if (buf_len > 0 && !buf) { c->result = WAPI_ERR_INVAL; return; }

    if (mode == WAPI_TRANSFER_LATENT) {
        if (mime_is_text_plain(mime, c->len)) {
            if (!wapi_plat_clipboard_has_text()) { c->result = WAPI_ERR_NOENT; return; }
            size_t total = wapi_plat_clipboard_get_text(NULL, 0);
            if (total == 0) { c->result = WAPI_ERR_NOENT; return; }
            size_t copy = total < buf_len ? total : buf_len;
            if (copy > 0) wapi_plat_clipboard_get_text((char*)buf, copy);
            c->result = (int32_t)copy;
            return;
        }
        if (mime_is_image_bmp(mime, c->len)) {
            if (!wapi_plat_clipboard_has_image()) { c->result = WAPI_ERR_NOENT; return; }
            size_t total = wapi_plat_clipboard_get_image(NULL, 0);
            if (total == 0) { c->result = WAPI_ERR_NOENT; return; }
            size_t copy = total < buf_len ? total : buf_len;
            if (copy > 0) wapi_plat_clipboard_get_image(buf, copy);
            c->result = (int32_t)copy;
            return;
        }
        if (mime_is_uri_list(mime, c->len)) {
            if (!wapi_plat_clipboard_has_files()) { c->result = WAPI_ERR_NOENT; return; }
            size_t total = wapi_plat_clipboard_get_files(NULL, 0);
            if (total == 0) { c->result = WAPI_ERR_NOENT; return; }
            size_t copy = total < buf_len ? total : buf_len;
            if (copy > 0) wapi_plat_clipboard_get_files((char*)buf, copy);
            c->result = (int32_t)copy;
            return;
        }
        c->result = WAPI_ERR_NOTSUP;
        return;
    }

    if (mode == WAPI_TRANSFER_POINTED) {
        /* Only text/uri-list is supported for drops today — that's
         * all WM_DROPFILES delivers. Full OLE IDropTarget with drag-
         * enter/over/leave and richer MIME set is a later upgrade. */
        if (!mime_is_uri_list(mime, c->len)) { c->result = WAPI_ERR_NOTSUP; return; }
        /* Route through whichever surface has a pending drop. We scan
         * the handle table for a SURFACE with a non-zero drop payload;
         * first hit wins. A fuller design would thread seat_id →
         * surface mapping, but single-seat desktop only has one. */
        for (int i = 1; i < WAPI_MAX_HANDLES; i++) {
            if (g_rt.handles[i].type != WAPI_HTYPE_SURFACE) continue;
            wapi_plat_window_t* w = g_rt.handles[i].data.window;
            if (!w) continue;
            size_t total = wapi_plat_window_drop_payload(w, NULL, 0, NULL, NULL);
            if (total == 0) continue;
            size_t copy = total < buf_len ? total : buf_len;
            if (copy > 0) {
                wapi_plat_window_drop_payload(w, (char*)buf, copy, NULL, NULL);
            }
            c->result = (int32_t)copy;
            return;
        }
        c->result = WAPI_ERR_NOENT;
        return;
    }

    c->result = WAPI_ERR_NOTSUP;
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
