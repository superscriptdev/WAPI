/**
 * WAPI Desktop Runtime - Surfaces / Windowing
 *
 * Implements all wapi_surface.* imports using SDL3 windowing.
 * Each WAPI surface handle maps to an SDL_Window*.
 */

#include "wapi_host.h"

/* ============================================================
 * Surface Create / Destroy
 * ============================================================ */

/* create: (i32 desc_ptr, i32 surface_out_ptr) -> i32 */
static wasm_trap_t* host_surface_create(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t desc_ptr = WAPI_ARG_U32(0);
    uint32_t surface_out_ptr = WAPI_ARG_U32(1);

    /* Read the descriptor from Wasm memory.
     * wapi_surface_desc_t layout (wasm32):
     *   +0: u32 nextInChain (ptr, ignored)
     *   +4: u32 title (ptr)
     *   +8: u32 title_len
     *  +12: i32 width
     *  +16: i32 height
     *  +20: u32 flags
     */
    void* desc_host = wapi_wasm_ptr(desc_ptr, 24);
    if (!desc_host) {
        wapi_set_error("Invalid descriptor pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    uint32_t title_ptr, title_len;
    int32_t width, height;
    uint32_t flags;

    memcpy(&title_ptr, (uint8_t*)desc_host + 4, 4);
    memcpy(&title_len, (uint8_t*)desc_host + 8, 4);
    memcpy(&width,     (uint8_t*)desc_host + 12, 4);
    memcpy(&height,    (uint8_t*)desc_host + 16, 4);
    memcpy(&flags,     (uint8_t*)desc_host + 20, 4);

    /* Get title string */
    char title_buf[256] = "WAPI Application";
    if (title_ptr != 0 && title_len > 0) {
        const char* title_str = wapi_wasm_read_string(title_ptr, title_len);
        if (title_str) {
            uint32_t copy = title_len < 255 ? title_len : 255;
            memcpy(title_buf, title_str, copy);
            title_buf[copy] = '\0';
        }
    } else if (title_ptr != 0 && title_len == UINT32_MAX) {
        /* WAPI_STRLEN: null-terminated */
        const char* title_str = wapi_wasm_read_string(title_ptr, 256);
        if (title_str) {
            size_t len = strnlen(title_str, 255);
            memcpy(title_buf, title_str, len);
            title_buf[len] = '\0';
        }
    }

    /* Default sizes */
    if (width <= 0) width = 1280;
    if (height <= 0) height = 720;

    /* Map WAPI flags to SDL flags */
    SDL_WindowFlags sdl_flags = 0;
    if (flags & 0x0001) sdl_flags |= SDL_WINDOW_RESIZABLE;       /* RESIZABLE */
    if (flags & 0x0002) sdl_flags |= SDL_WINDOW_BORDERLESS;      /* BORDERLESS */
    if (flags & 0x0004) sdl_flags |= SDL_WINDOW_FULLSCREEN;      /* FULLSCREEN */
    if (flags & 0x0008) sdl_flags |= SDL_WINDOW_HIDDEN;          /* HIDDEN */
    if (flags & 0x0010) sdl_flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY; /* HIGH_DPI */
    if (flags & 0x0020) sdl_flags |= SDL_WINDOW_ALWAYS_ON_TOP;   /* ALWAYS_ON_TOP */
    if (flags & 0x0040) sdl_flags |= SDL_WINDOW_TRANSPARENT;     /* TRANSPARENT */

    SDL_Window* window = SDL_CreateWindow(title_buf, width, height, sdl_flags);
    if (!window) {
        wapi_set_error(SDL_GetError());
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }

    int32_t handle = wapi_handle_alloc(WAPI_HTYPE_SURFACE);
    if (handle == 0) {
        SDL_DestroyWindow(window);
        wapi_set_error("Handle table full");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    g_rt.handles[handle].data.window = window;
    wapi_wasm_write_i32(surface_out_ptr, handle);

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* destroy: (i32 surface) -> i32 */
static wasm_trap_t* host_surface_destroy(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);

    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    SDL_DestroyWindow(g_rt.handles[h].data.window);
    wapi_handle_free(h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Surface Queries
 * ============================================================ */

/* get_size: (i32 surface, i32 w_ptr, i32 h_ptr) -> i32 */
static wasm_trap_t* host_surface_get_size(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    uint32_t w_ptr = WAPI_ARG_U32(1);
    uint32_t h_ptr = WAPI_ARG_U32(2);

    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    int w, hi;
    SDL_GetWindowSizeInPixels(g_rt.handles[h].data.window, &w, &hi);
    wapi_wasm_write_i32(w_ptr, (int32_t)w);
    wapi_wasm_write_i32(h_ptr, (int32_t)hi);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* get_size_logical: (i32 surface, i32 w_ptr, i32 h_ptr) -> i32 */
static wasm_trap_t* host_surface_get_size_logical(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    uint32_t w_ptr = WAPI_ARG_U32(1);
    uint32_t h_ptr = WAPI_ARG_U32(2);

    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    int w, hi;
    SDL_GetWindowSize(g_rt.handles[h].data.window, &w, &hi);
    wapi_wasm_write_i32(w_ptr, (int32_t)w);
    wapi_wasm_write_i32(h_ptr, (int32_t)hi);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* get_dpi_scale: (i32 surface, i32 scale_ptr) -> i32 */
static wasm_trap_t* host_surface_get_dpi_scale(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    uint32_t scale_ptr = WAPI_ARG_U32(1);

    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    float scale = SDL_GetWindowDisplayScale(g_rt.handles[h].data.window);
    wapi_wasm_write_f32(scale_ptr, scale);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* request_size: (i32 surface, i32 w, i32 h) -> i32 */
static wasm_trap_t* host_surface_request_size(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    int32_t w = WAPI_ARG_I32(1);
    int32_t hi = WAPI_ARG_I32(2);

    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    if (!SDL_SetWindowSize(g_rt.handles[h].data.window, w, hi)) {
        wapi_set_error(SDL_GetError());
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* set_title: (i32 surface, i32 title_ptr, i32 title_len) -> i32 */
static wasm_trap_t* host_surface_set_title(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    uint32_t title_ptr = WAPI_ARG_U32(1);
    uint32_t title_len = WAPI_ARG_U32(2);

    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    const char* title = wapi_wasm_read_string(title_ptr, title_len);
    if (!title) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    char buf[256];
    uint32_t copy = title_len < 255 ? title_len : 255;
    memcpy(buf, title, copy);
    buf[copy] = '\0';

    if (!SDL_SetWindowTitle(g_rt.handles[h].data.window, buf)) {
        wapi_set_error(SDL_GetError());
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* set_fullscreen: (i32 surface, i32 fullscreen) -> i32 */
static wasm_trap_t* host_surface_set_fullscreen(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    int32_t fullscreen = WAPI_ARG_I32(1);

    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    if (!SDL_SetWindowFullscreen(g_rt.handles[h].data.window, fullscreen ? true : false)) {
        wapi_set_error(SDL_GetError());
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* set_visible: (i32 surface, i32 visible) -> i32 */
static wasm_trap_t* host_surface_set_visible(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    int32_t visible = WAPI_ARG_I32(1);

    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    if (visible)
        SDL_ShowWindow(g_rt.handles[h].data.window);
    else
        SDL_HideWindow(g_rt.handles[h].data.window);

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* minimize: (i32 surface) -> i32 */
static wasm_trap_t* host_surface_minimize(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    SDL_MinimizeWindow(g_rt.handles[h].data.window);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* maximize: (i32 surface) -> i32 */
static wasm_trap_t* host_surface_maximize(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    SDL_MaximizeWindow(g_rt.handles[h].data.window);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* restore: (i32 surface) -> i32 */
static wasm_trap_t* host_surface_restore(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    SDL_RestoreWindow(g_rt.handles[h].data.window);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* set_cursor: (i32 surface, i32 cursor_type) -> i32 */
static wasm_trap_t* host_surface_set_cursor(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    int32_t cursor_type = WAPI_ARG_I32(1);

    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    /* Map WAPI cursor types to SDL system cursors */
    SDL_SystemCursor sdl_cursor;
    switch (cursor_type) {
    case 0:  sdl_cursor = SDL_SYSTEM_CURSOR_DEFAULT;    break; /* DEFAULT */
    case 1:  sdl_cursor = SDL_SYSTEM_CURSOR_POINTER;    break; /* POINTER */
    case 2:  sdl_cursor = SDL_SYSTEM_CURSOR_TEXT;       break; /* TEXT */
    case 3:  sdl_cursor = SDL_SYSTEM_CURSOR_CROSSHAIR;  break; /* CROSSHAIR */
    case 4:  sdl_cursor = SDL_SYSTEM_CURSOR_MOVE;       break; /* MOVE */
    case 5:  sdl_cursor = SDL_SYSTEM_CURSOR_NS_RESIZE;  break; /* RESIZE_NS */
    case 6:  sdl_cursor = SDL_SYSTEM_CURSOR_EW_RESIZE;  break; /* RESIZE_EW */
    case 7:  sdl_cursor = SDL_SYSTEM_CURSOR_NWSE_RESIZE; break; /* RESIZE_NWSE */
    case 8:  sdl_cursor = SDL_SYSTEM_CURSOR_NESW_RESIZE; break; /* RESIZE_NESW */
    case 9:  sdl_cursor = SDL_SYSTEM_CURSOR_NOT_ALLOWED; break; /* NOT_ALLOWED */
    case 10: sdl_cursor = SDL_SYSTEM_CURSOR_WAIT;       break; /* WAIT */
    case 11: sdl_cursor = SDL_SYSTEM_CURSOR_POINTER;    break; /* GRAB (approx) */
    case 12: sdl_cursor = SDL_SYSTEM_CURSOR_POINTER;    break; /* GRABBING (approx) */
    case 13:
        SDL_HideCursor();
        WAPI_RET_I32(WAPI_OK);
        return NULL;
    default:
        sdl_cursor = SDL_SYSTEM_CURSOR_DEFAULT;
        break;
    }

    SDL_Cursor* cursor = SDL_CreateSystemCursor(sdl_cursor);
    if (cursor) {
        SDL_SetCursor(cursor);
        SDL_ShowCursor();
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Display Functions
 * ============================================================ */

/* display_count: () -> i32 */
static wasm_trap_t* host_surface_display_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    SDL_free(displays);
    WAPI_RET_I32(count);
    return NULL;
}

/* display_info: (i32 index, i32 w_ptr, i32 h_ptr, i32 hz_ptr) -> i32 */
static wasm_trap_t* host_surface_display_info(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t index = WAPI_ARG_I32(0);
    uint32_t w_ptr = WAPI_ARG_U32(1);
    uint32_t h_ptr = WAPI_ARG_U32(2);
    uint32_t hz_ptr = WAPI_ARG_U32(3);

    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    if (!displays || index < 0 || index >= count) {
        SDL_free(displays);
        WAPI_RET_I32(WAPI_ERR_RANGE);
        return NULL;
    }

    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displays[index]);
    SDL_free(displays);

    if (!mode) {
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }

    wapi_wasm_write_i32(w_ptr, mode->w);
    wapi_wasm_write_i32(h_ptr, mode->h);
    wapi_wasm_write_i32(hz_ptr, (int32_t)mode->refresh_rate);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_surface(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_surface", "create",           host_surface_create);
    WAPI_DEFINE_1_1(linker, "wapi_surface", "destroy",          host_surface_destroy);
    WAPI_DEFINE_3_1(linker, "wapi_surface", "get_size",         host_surface_get_size);
    WAPI_DEFINE_3_1(linker, "wapi_surface", "get_size_logical", host_surface_get_size_logical);
    WAPI_DEFINE_2_1(linker, "wapi_surface", "get_dpi_scale",    host_surface_get_dpi_scale);
    WAPI_DEFINE_3_1(linker, "wapi_surface", "request_size",     host_surface_request_size);
    WAPI_DEFINE_3_1(linker, "wapi_surface", "set_title",        host_surface_set_title);
    WAPI_DEFINE_2_1(linker, "wapi_surface", "set_fullscreen",   host_surface_set_fullscreen);
    WAPI_DEFINE_2_1(linker, "wapi_surface", "set_visible",      host_surface_set_visible);
    WAPI_DEFINE_1_1(linker, "wapi_surface", "minimize",         host_surface_minimize);
    WAPI_DEFINE_1_1(linker, "wapi_surface", "maximize",         host_surface_maximize);
    WAPI_DEFINE_1_1(linker, "wapi_surface", "restore",          host_surface_restore);
    WAPI_DEFINE_2_1(linker, "wapi_surface", "set_cursor",       host_surface_set_cursor);
    WAPI_DEFINE_0_1(linker, "wapi_surface", "display_count",    host_surface_display_count);
    WAPI_DEFINE_4_1(linker, "wapi_surface", "display_info",     host_surface_display_info);
}
