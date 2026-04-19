/**
 * WAPI SDL Runtime - Surfaces
 *
 * A WAPI surface on this runtime is always backed by an SDL_Window.
 * Pure offscreen surfaces (no wapi_window_desc_t chained) are not
 * supported — the application should render to an offscreen WebGPU
 * texture via wapi_gpu instead. Offscreen surface creation therefore
 * returns WAPI_ERR_NOTSUP.
 *
 * Descriptor layout recap (from wapi_surface.h / wapi_window.h):
 *   wapi_surface_desc_t (24 bytes, align 8):
 *     0 : uint64_t nextInChain
 *     8 : int32_t  width
 *    12 : int32_t  height
 *    16 : uint64_t flags
 *   wapi_window_desc_t (40 bytes, align 8):
 *     0 : wapi_chain_t chain     (16: next u64, sType u32, _pad u32)
 *    16 : wapi_stringview_t title (16: data u32 + pad, length u64)
 *    32 : uint32_t window_flags
 *    36 : uint32_t _pad
 */

#include "wapi_host.h"

#define WAPI_STYPE_WINDOW_CONFIG 0x0001u

/* Window flags */
#define WAPI_WINDOW_FLAG_RESIZABLE   0x0001u
#define WAPI_WINDOW_FLAG_BORDERLESS  0x0002u
#define WAPI_WINDOW_FLAG_FULLSCREEN  0x0004u
#define WAPI_WINDOW_FLAG_MAXIMIZED   0x0008u
#define WAPI_WINDOW_FLAG_MINIMIZED   0x0010u
#define WAPI_WINDOW_FLAG_ALWAYS_ON_TOP 0x0020u

/* Surface flags */
#define WAPI_SURFACE_FLAG_HIGH_DPI    0x0001u
#define WAPI_SURFACE_FLAG_TRANSPARENT 0x0002u

static bool read_window_desc_from_chain(uint64_t chain_ptr,
                                        char* title_out, size_t title_buf,
                                        uint32_t* flags_out) {
    title_out[0] = '\0';
    if (flags_out) *flags_out = 0;
    if (chain_ptr == 0) return false;
    const uint8_t* chain = (const uint8_t*)wapi_wasm_ptr((uint32_t)chain_ptr, 16);
    if (!chain) return false;
    uint32_t stype = 0;
    memcpy(&stype, chain + 8, 4);
    if (stype != WAPI_STYPE_WINDOW_CONFIG) return false;

    /* Full window desc is 40 bytes. */
    const uint8_t* desc = (const uint8_t*)wapi_wasm_ptr((uint32_t)chain_ptr, 40);
    if (!desc) return false;

    /* title @ offset 16: stringview { data ptr@16 (4 + 4 pad), length @24 (8) } */
    uint32_t title_ptr = 0;
    uint64_t title_len = 0;
    memcpy(&title_ptr, desc + 16, 4);
    memcpy(&title_len, desc + 24, 8);
    if (title_ptr && title_len > 0) {
        uint32_t n = (uint32_t)(title_len < (title_buf - 1) ? title_len : (title_buf - 1));
        const char* src = (const char*)wapi_wasm_ptr(title_ptr, n);
        if (src) { memcpy(title_out, src, n); title_out[n] = '\0'; }
    }
    if (flags_out) memcpy(flags_out, desc + 32, 4);
    return true;
}

/* ---- create ---- */

static int32_t host_surface_create(wasm_exec_env_t env,
                                   uint32_t desc_ptr, uint32_t out_handle) {
    (void)env;
    const uint8_t* desc = (const uint8_t*)wapi_wasm_ptr(desc_ptr, 24);
    if (!desc) {
        wapi_wasm_write_i32(out_handle, 0);
        return WAPI_ERR_INVAL;
    }
    uint64_t chain = 0;
    int32_t w = 0, h = 0;
    uint64_t flags = 0;
    memcpy(&chain, desc + 0, 8);
    memcpy(&w,     desc + 8, 4);
    memcpy(&h,     desc + 12, 4);
    memcpy(&flags, desc + 16, 8);

    char title[256];
    uint32_t win_flags = 0;
    bool has_window = read_window_desc_from_chain(chain, title, sizeof(title),
                                                  &win_flags);
    if (!has_window) {
        /* Offscreen surface not supported on this runtime. */
        wapi_wasm_write_i32(out_handle, 0);
        return WAPI_ERR_NOTSUP;
    }

    SDL_WindowFlags sdl_flags = 0;
    if (win_flags & WAPI_WINDOW_FLAG_RESIZABLE)   sdl_flags |= SDL_WINDOW_RESIZABLE;
    if (win_flags & WAPI_WINDOW_FLAG_BORDERLESS)  sdl_flags |= SDL_WINDOW_BORDERLESS;
    if (win_flags & WAPI_WINDOW_FLAG_FULLSCREEN)  sdl_flags |= SDL_WINDOW_FULLSCREEN;
    if (win_flags & WAPI_WINDOW_FLAG_MAXIMIZED)   sdl_flags |= SDL_WINDOW_MAXIMIZED;
    if (win_flags & WAPI_WINDOW_FLAG_MINIMIZED)   sdl_flags |= SDL_WINDOW_MINIMIZED;
    if (win_flags & WAPI_WINDOW_FLAG_ALWAYS_ON_TOP) sdl_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    if (flags & WAPI_SURFACE_FLAG_HIGH_DPI)       sdl_flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (flags & WAPI_SURFACE_FLAG_TRANSPARENT)    sdl_flags |= SDL_WINDOW_TRANSPARENT;

    if (w <= 0) w = 800;
    if (h <= 0) h = 600;

    SDL_Window* win = SDL_CreateWindow(title[0] ? title : "WAPI", w, h, sdl_flags);
    if (!win) {
        wapi_wasm_write_i32(out_handle, 0);
        wapi_set_error(SDL_GetError());
        return WAPI_ERR_IO;
    }
    int32_t handle = wapi_handle_alloc(WAPI_HTYPE_SURFACE);
    if (handle == 0) {
        SDL_DestroyWindow(win);
        wapi_wasm_write_i32(out_handle, 0);
        return WAPI_ERR_NOMEM;
    }
    g_rt.handles[handle].data.window = win;
    wapi_wasm_write_i32(out_handle, handle);
    return WAPI_OK;
}

static int32_t host_surface_destroy(wasm_exec_env_t env, int32_t surface) {
    (void)env;
    if (!wapi_handle_valid(surface, WAPI_HTYPE_SURFACE)) return WAPI_ERR_BADF;

    /* Clean up any GPU surface state tied to this handle. */
    for (int i = 0; i < g_rt.gpu_surface_count; ) {
        if (g_rt.gpu_surfaces[i].wapi_surface_handle == surface) {
            if (g_rt.gpu_surfaces[i].wgpu_surface) {
                wgpuSurfaceRelease(g_rt.gpu_surfaces[i].wgpu_surface);
            }
            g_rt.gpu_surfaces[i] = g_rt.gpu_surfaces[--g_rt.gpu_surface_count];
        } else {
            i++;
        }
    }

    SDL_Window* win = g_rt.handles[surface].data.window;
    if (win) SDL_DestroyWindow(win);
    wapi_handle_free(surface);
    return WAPI_OK;
}

static int32_t host_surface_get_size(wasm_exec_env_t env,
                                     int32_t surface,
                                     uint32_t w_ptr, uint32_t h_ptr) {
    (void)env;
    if (!wapi_handle_valid(surface, WAPI_HTYPE_SURFACE)) return WAPI_ERR_BADF;
    SDL_Window* win = g_rt.handles[surface].data.window;
    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(win, &w, &h)) return WAPI_ERR_IO;
    wapi_wasm_write_i32(w_ptr, w);
    wapi_wasm_write_i32(h_ptr, h);
    return WAPI_OK;
}

static int32_t host_surface_get_dpi_scale(wasm_exec_env_t env,
                                          int32_t surface, uint32_t scale_ptr) {
    (void)env;
    if (!wapi_handle_valid(surface, WAPI_HTYPE_SURFACE)) return WAPI_ERR_BADF;
    SDL_Window* win = g_rt.handles[surface].data.window;
    float scale = SDL_GetWindowDisplayScale(win);
    if (scale <= 0.0f) scale = 1.0f;
    wapi_wasm_write_f32(scale_ptr, scale);
    return WAPI_OK;
}

static int32_t host_surface_request_size(wasm_exec_env_t env,
                                         int32_t surface,
                                         int32_t width, int32_t height) {
    (void)env;
    if (!wapi_handle_valid(surface, WAPI_HTYPE_SURFACE)) return WAPI_ERR_BADF;
    SDL_Window* win = g_rt.handles[surface].data.window;
    if (width <= 0 || height <= 0) return WAPI_ERR_INVAL;
    return SDL_SetWindowSize(win, width, height) ? WAPI_OK : WAPI_ERR_IO;
}

static NativeSymbol g_symbols[] = {
    { "create",        (void*)host_surface_create,        "(ii)i",  NULL },
    { "destroy",       (void*)host_surface_destroy,       "(i)i",   NULL },
    { "get_size",      (void*)host_surface_get_size,      "(iii)i", NULL },
    { "get_dpi_scale", (void*)host_surface_get_dpi_scale, "(ii)i",  NULL },
    { "request_size",  (void*)host_surface_request_size,  "(iii)i", NULL },
};

wapi_cap_registration_t wapi_host_surface_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_surface",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
