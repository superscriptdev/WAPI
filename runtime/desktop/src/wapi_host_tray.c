/**
 * WAPI Desktop Runtime — System Tray (wapi_tray.h)
 *
 * Wraps the platform tray + menu backend. The tray_set_menu call
 * takes a flat array of wapi_tray_menu_item_t and materializes a
 * platform menu bound to the tray's own token so WM_COMMAND routes
 * TRAY_MENU events rather than generic MENU_SELECT.
 */

#include "wapi_host.h"

#define WAPI_TRAY_TOKEN_MIN 0x4000
#define WAPI_TRAY_TOKEN_MAX 0x7FFF

static uint32_t s_tray_token_bitmap[((WAPI_TRAY_TOKEN_MAX - WAPI_TRAY_TOKEN_MIN + 1) + 31) / 32];

static uint32_t tray_token_alloc(void) {
    for (uint32_t i = 0; i <= WAPI_TRAY_TOKEN_MAX - WAPI_TRAY_TOKEN_MIN; i++) {
        uint32_t w = i >> 5, b = i & 31;
        if ((s_tray_token_bitmap[w] & (1u << b)) == 0) {
            s_tray_token_bitmap[w] |= (1u << b);
            return i + WAPI_TRAY_TOKEN_MIN;
        }
    }
    return 0;
}

static void tray_token_free(uint32_t token) {
    if (token < WAPI_TRAY_TOKEN_MIN || token > WAPI_TRAY_TOKEN_MAX) return;
    uint32_t idx = token - WAPI_TRAY_TOKEN_MIN;
    uint32_t w = idx >> 5, b = idx & 31;
    s_tray_token_bitmap[w] &= ~(1u << b);
}

/* Tray's internal menu is a platform menu without a matching MENU
 * handle in the WAPI table — the guest uses wapi_tray_set_menu with
 * a descriptor array rather than a menu handle, so the platform side
 * is the sole owner. Keep a pointer in the tray handle's data so
 * destroy can free it. */
static void tray_clear_internal_menu(wapi_handle_entry_t* he) {
    if (he->data.tray.menu_handle > 0) {
        /* menu_handle is a stashed platform-owned menu we created. */
        wapi_plat_menu_t* plat = (wapi_plat_menu_t*)(uintptr_t)he->data.tray.menu_handle;
        wapi_plat_tray_set_menu(he->data.tray.plat, NULL);
        wapi_plat_menu_destroy(plat);
        he->data.tray.menu_handle = 0;
    }
}

/* ===== Imports ===== */

static wasm_trap_t* h_tray_create(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t icon_ptr = WAPI_ARG_U32(0);
    uint64_t icon_len = WAPI_ARG_U64(1);
    uint32_t sv_ptr   = WAPI_ARG_U32(2); /* stringview tooltip */
    uint32_t out_ptr  = WAPI_ARG_U32(3);

    const void* icon = icon_len ? wapi_wasm_ptr(icon_ptr, (uint32_t)icon_len) : NULL;
    if (icon_len > 0 && !icon) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    uint64_t tip_data = 0, tip_len = 0;
    if (sv_ptr) {
        uint8_t* sv = (uint8_t*)wapi_wasm_ptr(sv_ptr, 16);
        if (!sv) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
        memcpy(&tip_data, sv + 0, 8);
        memcpy(&tip_len,  sv + 8, 8);
    }
    const char* tip = tip_len ? (const char*)wapi_wasm_ptr((uint32_t)tip_data, (uint32_t)tip_len) : NULL;
    if (tip_len > 0 && !tip) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    uint32_t token = tray_token_alloc();
    if (!token) { WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }

    wapi_plat_tray_t* t = wapi_plat_tray_create(token, icon, (size_t)icon_len,
                                                tip, (size_t)tip_len);
    if (!t) { tray_token_free(token); WAPI_RET_I32(WAPI_ERR_IO); return NULL; }

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_TRAY);
    if (h <= 0) {
        wapi_plat_tray_destroy(t);
        tray_token_free(token);
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }
    g_rt.handles[h].data.tray.plat        = t;
    g_rt.handles[h].data.tray.token       = token;
    g_rt.handles[h].data.tray.menu_handle = 0;
    wapi_wasm_write_i32(out_ptr, h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_tray_destroy(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_TRAY)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    tray_clear_internal_menu(&g_rt.handles[h]);
    wapi_plat_tray_destroy(g_rt.handles[h].data.tray.plat);
    tray_token_free(g_rt.handles[h].data.tray.token);
    wapi_handle_free(h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_tray_set_icon(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h        = WAPI_ARG_I32(0);
    uint32_t icon_ptr = WAPI_ARG_U32(1);
    uint64_t icon_len = WAPI_ARG_U64(2);
    if (!wapi_handle_valid(h, WAPI_HTYPE_TRAY)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    const void* icon = icon_len ? wapi_wasm_ptr(icon_ptr, (uint32_t)icon_len) : NULL;
    if (icon_len > 0 && !icon) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    bool ok = wapi_plat_tray_set_icon(g_rt.handles[h].data.tray.plat,
                                      icon, (size_t)icon_len);
    WAPI_RET_I32(ok ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

static wasm_trap_t* h_tray_set_tooltip(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h      = WAPI_ARG_I32(0);
    uint32_t sv_ptr = WAPI_ARG_U32(1);
    if (!wapi_handle_valid(h, WAPI_HTYPE_TRAY)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    uint8_t* sv = (uint8_t*)wapi_wasm_ptr(sv_ptr, 16);
    if (!sv) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    uint64_t data, len;
    memcpy(&data, sv + 0, 8);
    memcpy(&len,  sv + 8, 8);
    const char* tip = len ? (const char*)wapi_wasm_ptr((uint32_t)data, (uint32_t)len) : NULL;
    bool ok = wapi_plat_tray_set_tooltip(g_rt.handles[h].data.tray.plat,
                                         tip, (size_t)len);
    WAPI_RET_I32(ok ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

static wasm_trap_t* h_tray_set_menu(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h          = WAPI_ARG_I32(0);
    uint32_t items_ptr  = WAPI_ARG_U32(1);
    uint32_t item_count = WAPI_ARG_U32(2);
    if (!wapi_handle_valid(h, WAPI_HTYPE_TRAY)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_handle_entry_t* he = &g_rt.handles[h];

    /* Clear any prior menu before replacing. */
    tray_clear_internal_menu(he);

    if (item_count == 0 || items_ptr == 0) {
        WAPI_RET_I32(WAPI_OK); return NULL;
    }

    /* wapi_tray_menu_item_t (40B):
     *   +0  u32 id, +4  u32 label_len, +8  u64 label_ptr,
     *   +16 u32 icon_len, +20 u32 _pad, +24 u64 icon_ptr,
     *   +32 u32 flags, +36 u32 _pad */
    uint8_t* src = (uint8_t*)wapi_wasm_ptr(items_ptr, item_count * 40u);
    if (!src) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    /* Build the platform menu using the tray's token — WM_COMMAND
     * routes through the tray dispatcher because menu_from_token for
     * tray-range tokens walks the tray registry. */
    wapi_plat_menu_t* plat = wapi_plat_menu_create(he->data.tray.token);
    if (!plat) { WAPI_RET_I32(WAPI_ERR_IO); return NULL; }

    for (uint32_t i = 0; i < item_count; i++) {
        uint8_t* it = src + i * 40u;
        uint32_t id, label_len, flags;
        uint64_t label_ptr;
        memcpy(&id,        it + 0,  4);
        memcpy(&label_len, it + 4,  4);
        memcpy(&label_ptr, it + 8,  8);
        memcpy(&flags,     it + 32, 4);
        const char* label = NULL;
        if (label_len > 0 && !(flags & 0x1 /* separator */)) {
            label = (const char*)wapi_wasm_ptr((uint32_t)label_ptr, label_len);
            if (!label) { wapi_plat_menu_destroy(plat); WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
        }
        wapi_plat_menu_add_item(plat, id, label, label_len, flags);
    }

    wapi_plat_tray_set_menu(he->data.tray.plat, plat);
    /* Stash the platform pointer in menu_handle so destroy can free it. */
    he->data.tray.menu_handle = (int32_t)(intptr_t)plat;
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

void wapi_host_register_tray(wasmtime_linker_t* linker) {
    wapi_linker_define(linker, "wapi_tray", "tray_create", h_tray_create,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I64, WASM_I32, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, "wapi_tray", "tray_destroy", h_tray_destroy);
    wapi_linker_define(linker, "wapi_tray", "tray_set_icon", h_tray_set_icon,
        3, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_2_1(linker, "wapi_tray", "tray_set_tooltip", h_tray_set_tooltip);
    WAPI_DEFINE_3_1(linker, "wapi_tray", "tray_set_menu",    h_tray_set_menu);
}
