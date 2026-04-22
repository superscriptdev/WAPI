/**
 * WAPI Desktop Runtime — Native Menus (wapi_menu.h)
 *
 * Thin host wrapper over the platform menu backend. Each menu handle
 * holds a `token` in the menu range (1..0x3FFF) that WM_COMMAND uses
 * to route item clicks to the right menu; tokens are recycled as
 * handles are freed.
 */

#include "wapi_host.h"

/* Forward decls from wapi_plat_win32_menu.c (other backends provide
 * equivalents). Declared here instead of wapi_plat.h so the platform
 * layer stays focused on window/input — menu is a host-facing service. */

#define WAPI_MENU_TOKEN_MIN 1
#define WAPI_MENU_TOKEN_MAX 0x3FFF

/* A simple low-water bitmap for token allocation. Recycles on destroy. */
static uint32_t s_menu_token_bitmap[(WAPI_MENU_TOKEN_MAX + 31) / 32];

static uint32_t menu_token_alloc(void) {
    for (uint32_t i = WAPI_MENU_TOKEN_MIN; i <= WAPI_MENU_TOKEN_MAX; i++) {
        uint32_t w = i >> 5, b = i & 31;
        if ((s_menu_token_bitmap[w] & (1u << b)) == 0) {
            s_menu_token_bitmap[w] |= (1u << b);
            return i;
        }
    }
    return 0;
}

static void menu_token_free(uint32_t token) {
    if (token < WAPI_MENU_TOKEN_MIN || token > WAPI_MENU_TOKEN_MAX) return;
    uint32_t w = token >> 5, b = token & 31;
    s_menu_token_bitmap[w] &= ~(1u << b);
}

/* Public for tray host. */
uint32_t wapi_host_menu_token_for_handle(int32_t menu_handle) {
    if (!wapi_handle_valid(menu_handle, WAPI_HTYPE_MENU)) return 0;
    return g_rt.handles[menu_handle].data.menu.token;
}
struct wapi_plat_menu_t* wapi_host_menu_plat_for_handle(int32_t menu_handle) {
    if (!wapi_handle_valid(menu_handle, WAPI_HTYPE_MENU)) return NULL;
    return g_rt.handles[menu_handle].data.menu.plat;
}

/* ===== Imports ===== */

static wasm_trap_t* h_menu_create(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t out_ptr = WAPI_ARG_U32(0);

    uint32_t token = menu_token_alloc();
    if (!token) { WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }
    wapi_plat_menu_t* m = wapi_plat_menu_create(token);
    if (!m) { menu_token_free(token); WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_MENU);
    if (h <= 0) {
        wapi_plat_menu_destroy(m);
        menu_token_free(token);
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }
    g_rt.handles[h].data.menu.plat  = m;
    g_rt.handles[h].data.menu.token = token;
    wapi_wasm_write_i32(out_ptr, h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_menu_destroy(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_MENU)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_menu_destroy(g_rt.handles[h].data.menu.plat);
    menu_token_free(g_rt.handles[h].data.menu.token);
    wapi_handle_free(h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_menu_add_item(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h        = WAPI_ARG_I32(0);
    uint32_t item_ptr = WAPI_ARG_U32(1);
    if (!wapi_handle_valid(h, WAPI_HTYPE_MENU)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }

    /* wapi_menu_item_t (48B):
     *   +0  u32 id, +4  u32 label_len, +8  u64 label_ptr,
     *   +16 u32 shortcut_len, +20 u32 icon_len,
     *   +24 u64 shortcut_ptr, +32 u64 icon_ptr,
     *   +40 u32 flags, +44 u32 _pad */
    uint8_t* src = (uint8_t*)wapi_wasm_ptr(item_ptr, 48);
    if (!src) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    uint32_t id, label_len, flags;
    uint64_t label_ptr;
    memcpy(&id,        src + 0,  4);
    memcpy(&label_len, src + 4,  4);
    memcpy(&label_ptr, src + 8,  8);
    memcpy(&flags,     src + 40, 4);

    const char* label = NULL;
    if (label_len > 0 && !(flags & 0x1 /* separator */)) {
        label = (const char*)wapi_wasm_ptr((uint32_t)label_ptr, label_len);
        if (!label) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    }
    bool ok = wapi_plat_menu_add_item(g_rt.handles[h].data.menu.plat,
                                      id, label, label_len, flags);
    WAPI_RET_I32(ok ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

static wasm_trap_t* h_menu_add_submenu(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h      = WAPI_ARG_I32(0);
    uint32_t sv_ptr = WAPI_ARG_U32(1); /* stringview label */
    int32_t  sub    = WAPI_ARG_I32(2);
    if (!wapi_handle_valid(h,   WAPI_HTYPE_MENU)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    if (!wapi_handle_valid(sub, WAPI_HTYPE_MENU)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    /* stringview (16B): u64 data, u64 length */
    uint8_t* sv = (uint8_t*)wapi_wasm_ptr(sv_ptr, 16);
    if (!sv) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    uint64_t data, len;
    memcpy(&data, sv + 0, 8);
    memcpy(&len,  sv + 8, 8);
    const char* label = len ? (const char*)wapi_wasm_ptr((uint32_t)data, (uint32_t)len) : NULL;
    bool ok = wapi_plat_menu_add_submenu(g_rt.handles[h].data.menu.plat,
                                         label, (size_t)len,
                                         g_rt.handles[sub].data.menu.plat);
    WAPI_RET_I32(ok ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

static wasm_trap_t* h_menu_show_context(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t m_h = WAPI_ARG_I32(0);
    int32_t s_h = WAPI_ARG_I32(1);
    int32_t x   = WAPI_ARG_I32(2);
    int32_t y   = WAPI_ARG_I32(3);
    if (!wapi_handle_valid(m_h, WAPI_HTYPE_MENU))    { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    if (!wapi_handle_valid(s_h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    bool ok = wapi_plat_menu_show_context(g_rt.handles[m_h].data.menu.plat,
                                          g_rt.handles[s_h].data.window, x, y);
    WAPI_RET_I32(ok ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

static wasm_trap_t* h_menu_set_bar(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t s_h = WAPI_ARG_I32(0);
    int32_t m_h = WAPI_ARG_I32(1);
    if (!wapi_handle_valid(s_h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_menu_t* m = NULL;
    if (m_h != 0) {
        if (!wapi_handle_valid(m_h, WAPI_HTYPE_MENU)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
        m = g_rt.handles[m_h].data.menu.plat;
    }
    bool ok = wapi_plat_menu_set_bar(g_rt.handles[s_h].data.window, m);
    WAPI_RET_I32(ok ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

void wapi_host_register_menu(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_menu", "menu_create",       h_menu_create);
    WAPI_DEFINE_2_1(linker, "wapi_menu", "menu_add_item",     h_menu_add_item);
    WAPI_DEFINE_3_1(linker, "wapi_menu", "menu_add_submenu",  h_menu_add_submenu);
    WAPI_DEFINE_4_1(linker, "wapi_menu", "menu_show_context", h_menu_show_context);
    WAPI_DEFINE_2_1(linker, "wapi_menu", "menu_set_bar",      h_menu_set_bar);
    WAPI_DEFINE_1_1(linker, "wapi_menu", "menu_destroy",      h_menu_destroy);
}
