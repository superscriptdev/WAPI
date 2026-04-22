/**
 * WAPI Desktop Runtime - Surfaces (render targets)
 *
 * "wapi_surface" module:
 *   create, destroy, get_size, get_dpi_scale, request_size
 *
 * Window management (title, fullscreen, show/hide, min/max) lives
 * in wapi_host_window.c under the "wapi_window" module, per the
 * header split in wapi_surface.h / wapi_window.h.
 *
 * wapi_surface_desc_t layout (24B, align 8):
 *   +0   u64 nextInChain
 *   +8   i32 width
 *   +12  i32 height
 *   +16  u64 flags   (WAPI_SURFACE_FLAG_*)
 *
 * If nextInChain is non-zero it points at a wapi_window_desc_t
 * (sType == WAPI_STYPE_WINDOW_CONFIG = 0x0001). We read the title +
 * window_flags from there and hand the merged descriptor to
 * wapi_plat.
 */

#include "wapi_host.h"

#define WAPI_STYPE_WINDOW_CONFIG 0x0001u

/* Walk nextInChain looking for WAPI_STYPE_WINDOW_CONFIG.
 * On match, fills title_buf (NUL-terminated, up to 255 chars) and
 * writes *window_flags. Returns false if no window config chained. */
static bool read_window_config(uint64_t chain_addr,
                               char title_buf[256],
                               uint32_t* window_flags)
{
    while (chain_addr != 0) {
        /* wapi_chain_t: {u64 next @0, u32 sType @8, u32 _pad @12} */
        void* ch = wapi_wasm_ptr((uint32_t)chain_addr, 16);
        if (!ch) return false;
        uint64_t next;  memcpy(&next,  (uint8_t*)ch + 0, 8);
        uint32_t stype; memcpy(&stype, (uint8_t*)ch + 8, 4);

        if (stype == WAPI_STYPE_WINDOW_CONFIG) {
            /* wapi_window_desc_t (40B):
             *   +0  wapi_chain_t chain (16B)
             *   +16 wapi_stringview_t title {u64 data, u64 length}
             *   +32 u32 window_flags
             *   +36 u32 _pad */
            void* wd = wapi_wasm_ptr((uint32_t)chain_addr, 40);
            if (!wd) return false;

            uint64_t title_data, title_len;
            uint32_t wflags;
            memcpy(&title_data, (uint8_t*)wd + 16, 8);
            memcpy(&title_len,  (uint8_t*)wd + 24, 8);
            memcpy(&wflags,     (uint8_t*)wd + 32, 4);
            *window_flags = wflags;

            if (title_data != 0) {
                if (title_len == UINT64_MAX) {
                    /* null-terminated */
                    const char* t = (const char*)wapi_wasm_ptr((uint32_t)title_data, 1);
                    if (t) {
                        size_t l = strnlen(t, 255);
                        memcpy(title_buf, t, l);
                        title_buf[l] = '\0';
                    }
                } else {
                    uint32_t copy = (uint32_t)(title_len < 255 ? title_len : 255);
                    const char* t = (const char*)wapi_wasm_ptr((uint32_t)title_data, copy);
                    if (t) {
                        memcpy(title_buf, t, copy);
                        title_buf[copy] = '\0';
                    }
                }
            }
            return true;
        }
        chain_addr = next;
    }
    return false;
}

/* ============================================================
 * Imports
 * ============================================================ */

static wasm_trap_t* host_surface_create(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t desc_ptr        = WAPI_ARG_U32(0);
    uint32_t surface_out_ptr = WAPI_ARG_U32(1);

    void* desc = wapi_wasm_ptr(desc_ptr, 24);
    if (!desc) { wapi_set_error("Invalid descriptor"); WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    uint64_t next_in_chain, flags;
    int32_t  width, height;
    memcpy(&next_in_chain, (uint8_t*)desc +  0, 8);
    memcpy(&width,         (uint8_t*)desc +  8, 4);
    memcpy(&height,        (uint8_t*)desc + 12, 4);
    memcpy(&flags,         (uint8_t*)desc + 16, 8);

    char     title_buf[256] = "WAPI Application";
    uint32_t window_flags   = 0;
    (void)read_window_config(next_in_chain, title_buf, &window_flags);

    wapi_plat_window_desc_t d = {0};
    d.title  = title_buf;
    d.width  = (width  > 0) ? width  : 800;
    d.height = (height > 0) ? height : 600;
    /* surface flags live in the low bits, window flags are merged in;
     * wapi_plat flag fields match WAPI bits 1:1. */
    d.flags  = (uint32_t)(flags & 0xFFFFFFFFu) | window_flags;

    wapi_plat_window_t* w = wapi_plat_window_create(&d);
    if (!w) { wapi_set_error("Window creation failed"); WAPI_RET_I32(WAPI_ERR_UNKNOWN); return NULL; }
    /* Every surface accepts external file drops by default — keeps
     * the guest-side transfer contract symmetric across platforms.
     * Backends that can't honour this return false and drops stay
     * silent. */
    wapi_plat_window_register_drop_target(w);

    int32_t handle = wapi_handle_alloc(WAPI_HTYPE_SURFACE);
    if (handle == 0) {
        wapi_plat_window_destroy(w);
        wapi_set_error("Handle table full");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }
    g_rt.handles[handle].data.window = w;
    wapi_wasm_write_i32(surface_out_ptr, handle);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_surface_destroy(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_window_destroy(g_rt.handles[h].data.window);
    wapi_handle_free(h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_surface_get_size(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    uint32_t w_ptr = WAPI_ARG_U32(1), h_ptr = WAPI_ARG_U32(2);
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    int32_t pw, ph;
    wapi_plat_window_get_size_pixels(g_rt.handles[h].data.window, &pw, &ph);
    wapi_wasm_write_i32(w_ptr, pw);
    wapi_wasm_write_i32(h_ptr, ph);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_surface_get_dpi_scale(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    uint32_t scale_ptr = WAPI_ARG_U32(1);
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    float s = wapi_plat_window_get_dpi_scale(g_rt.handles[h].data.window);
    wapi_wasm_write_f32(scale_ptr, s);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* host_surface_request_size(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h  = WAPI_ARG_I32(0);
    int32_t w  = WAPI_ARG_I32(1);
    int32_t hi = WAPI_ARG_I32(2);
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_window_set_size(g_rt.handles[h].data.window, w, hi);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* Windows desktop windows have no hardware cutouts (no notch, no
 * rounded corners in a way the client rect has to dodge) and no
 * system UI inside the client area (taskbar/dock sit outside the
 * window). Safe rect = full client rect. Mobile / embedded backends
 * subtract status bar, IME, notch projection, etc. */
static wasm_trap_t* host_surface_get_safe_rect(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h     = WAPI_ARG_I32(0);
    uint32_t x_ptr = WAPI_ARG_U32(1);
    uint32_t y_ptr = WAPI_ARG_U32(2);
    uint32_t w_ptr = WAPI_ARG_U32(3);
    uint32_t h_ptr = WAPI_ARG_U32(4);
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    int32_t pw, ph;
    wapi_plat_window_get_size_pixels(g_rt.handles[h].data.window, &pw, &ph);
    if (x_ptr) wapi_wasm_write_i32(x_ptr, 0);
    if (y_ptr) wapi_wasm_write_i32(y_ptr, 0);
    if (w_ptr) wapi_wasm_write_i32(w_ptr, pw);
    if (h_ptr) wapi_wasm_write_i32(h_ptr, ph);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_surface(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_surface", "create",        host_surface_create);
    WAPI_DEFINE_1_1(linker, "wapi_surface", "destroy",       host_surface_destroy);
    WAPI_DEFINE_3_1(linker, "wapi_surface", "get_size",      host_surface_get_size);
    WAPI_DEFINE_2_1(linker, "wapi_surface", "get_dpi_scale", host_surface_get_dpi_scale);
    WAPI_DEFINE_3_1(linker, "wapi_surface", "request_size",  host_surface_request_size);
    WAPI_DEFINE_5_1(linker, "wapi_surface", "get_safe_rect", host_surface_get_safe_rect);
}
