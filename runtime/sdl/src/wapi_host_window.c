/**
 * WAPI SDL Runtime - Window operations (SDL3 SDL_Window)
 *
 * The WAPI surface handle stores an SDL_Window*. wapi_surface creates
 * the window; wapi_window tweaks title / fullscreen / visibility / etc.
 */

#include "wapi_host.h"

#define REQUIRE_WINDOW(h, out_var) \
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) return WAPI_ERR_BADF; \
    SDL_Window* out_var = g_rt.handles[h].data.window;                   \
    if (!out_var) return WAPI_ERR_BADF

static int32_t host_set_title(wasm_exec_env_t env,
                              int32_t surface,
                              uint32_t title_ptr, uint32_t title_len) {
    (void)env;
    REQUIRE_WINDOW(surface, win);
    const char* src = (const char*)wapi_wasm_ptr(title_ptr, title_len);
    if (!src && title_len > 0) return WAPI_ERR_INVAL;
    char tmp[512];
    uint32_t n = title_len < 511 ? title_len : 511;
    if (n > 0) memcpy(tmp, src, n);
    tmp[n] = '\0';
    return SDL_SetWindowTitle(win, tmp) ? WAPI_OK : WAPI_ERR_IO;
}

static int32_t host_get_size_logical(wasm_exec_env_t env,
                                     int32_t surface,
                                     uint32_t w_ptr, uint32_t h_ptr) {
    (void)env;
    REQUIRE_WINDOW(surface, win);
    int w = 0, h = 0;
    if (!SDL_GetWindowSize(win, &w, &h)) return WAPI_ERR_IO;
    wapi_wasm_write_i32(w_ptr, w);
    wapi_wasm_write_i32(h_ptr, h);
    return WAPI_OK;
}

static int32_t host_set_fullscreen(wasm_exec_env_t env,
                                   int32_t surface, int32_t fullscreen) {
    (void)env;
    REQUIRE_WINDOW(surface, win);
    return SDL_SetWindowFullscreen(win, fullscreen != 0) ? WAPI_OK : WAPI_ERR_IO;
}

static int32_t host_set_visible(wasm_exec_env_t env,
                                int32_t surface, int32_t visible) {
    (void)env;
    REQUIRE_WINDOW(surface, win);
    bool ok = visible ? SDL_ShowWindow(win) : SDL_HideWindow(win);
    return ok ? WAPI_OK : WAPI_ERR_IO;
}

static int32_t host_minimize(wasm_exec_env_t env, int32_t surface) {
    (void)env;
    REQUIRE_WINDOW(surface, win);
    return SDL_MinimizeWindow(win) ? WAPI_OK : WAPI_ERR_IO;
}
static int32_t host_maximize(wasm_exec_env_t env, int32_t surface) {
    (void)env;
    REQUIRE_WINDOW(surface, win);
    return SDL_MaximizeWindow(win) ? WAPI_OK : WAPI_ERR_IO;
}
static int32_t host_restore(wasm_exec_env_t env, int32_t surface) {
    (void)env;
    REQUIRE_WINDOW(surface, win);
    return SDL_RestoreWindow(win) ? WAPI_OK : WAPI_ERR_IO;
}

static NativeSymbol g_symbols[] = {
    { "set_title",        (void*)host_set_title,        "(iii)i", NULL },
    { "get_size_logical", (void*)host_get_size_logical, "(iii)i", NULL },
    { "set_fullscreen",   (void*)host_set_fullscreen,   "(ii)i",  NULL },
    { "set_visible",      (void*)host_set_visible,      "(ii)i",  NULL },
    { "minimize",         (void*)host_minimize,         "(i)i",   NULL },
    { "maximize",         (void*)host_maximize,         "(i)i",   NULL },
    { "restore",          (void*)host_restore,          "(i)i",   NULL },
};

wapi_cap_registration_t wapi_host_window_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_window",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
