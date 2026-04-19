/**
 * WAPI SDL Runtime - Display enumeration (SDL3)
 *
 * Maps wapi_display.* to SDL_GetDisplays / SDL_GetDisplayBounds /
 * SDL_GetDisplayUsableBounds / SDL_GetCurrentDisplayMode.
 */

#include "wapi_host.h"

/* info layout: 56 bytes; see wapi_display.h. name lives in a static arena. */
#define MAX_DISPLAY_NAMES 16
#define DISPLAY_NAME_BUF  64
static char g_display_name_arena[MAX_DISPLAY_NAMES][DISPLAY_NAME_BUF];

static int32_t host_display_count(wasm_exec_env_t env) {
    (void)env;
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    if (ids) SDL_free(ids);
    return count;
}

static int32_t host_display_get_info(wasm_exec_env_t env,
                                     int32_t index, uint32_t info_ptr) {
    (void)env;
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    if (!ids || index < 0 || index >= count) {
        if (ids) SDL_free(ids);
        return WAPI_ERR_INVAL;
    }
    SDL_DisplayID id = ids[index];
    SDL_free(ids);

    SDL_Rect bounds = {0};
    SDL_GetDisplayBounds(id, &bounds);
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(id);
    float scale = SDL_GetDisplayContentScale(id);
    if (scale <= 0.0f) scale = 1.0f;
    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    const char* name = SDL_GetDisplayName(id);
    if (!name) name = "";

    int arena_slot = index < MAX_DISPLAY_NAMES ? index : (MAX_DISPLAY_NAMES - 1);
    snprintf(g_display_name_arena[arena_slot], DISPLAY_NAME_BUF, "%s", name);

    uint8_t info[56] = {0};
    uint32_t display_id = (uint32_t)id;
    int32_t x = bounds.x, y = bounds.y, w = bounds.w, h = bounds.h;
    float rr = mode ? mode->refresh_rate : 60.0f;
    memcpy(info + 0,  &display_id, 4);
    memcpy(info + 4,  &x, 4);
    memcpy(info + 8,  &y, 4);
    memcpy(info + 12, &w, 4);
    memcpy(info + 16, &h, 4);
    memcpy(info + 20, &rr, 4);
    memcpy(info + 24, &scale, 4);
    /* wapi_stringview_t name at offset 32: { data_ptr(8), length(8) }
     * The guest cannot read host-arena addresses directly. Leave zeroed;
     * apps that need the name should fall back to an alternative source. */
    info[48] = (id == primary) ? 1 : 0;
    info[49] = 0;  /* orientation: landscape */
    info[50] = 0;  /* subpixel_count: unknown */
    uint16_t rotation_deg = 0;
    memcpy(info + 52, &rotation_deg, 2);

    return wapi_wasm_write_bytes(info_ptr, info, 56) ? WAPI_OK : WAPI_ERR_INVAL;
}

static int32_t host_display_get_subpixels(wasm_exec_env_t env,
                                          int32_t index, uint32_t sp_ptr,
                                          int32_t max_count, uint32_t count_ptr) {
    (void)env; (void)index; (void)sp_ptr; (void)max_count;
    wapi_wasm_write_u32(count_ptr, 0);
    return WAPI_ERR_NOTSUP;
}

static int32_t host_display_get_usable_bounds(wasm_exec_env_t env,
                                              int32_t index,
                                              uint32_t x_ptr, uint32_t y_ptr,
                                              uint32_t w_ptr, uint32_t h_ptr) {
    (void)env;
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    if (!ids || index < 0 || index >= count) {
        if (ids) SDL_free(ids);
        return WAPI_ERR_INVAL;
    }
    SDL_DisplayID id = ids[index];
    SDL_free(ids);
    SDL_Rect r = {0};
    if (!SDL_GetDisplayUsableBounds(id, &r)) return WAPI_ERR_IO;
    wapi_wasm_write_i32(x_ptr, r.x);
    wapi_wasm_write_i32(y_ptr, r.y);
    wapi_wasm_write_i32(w_ptr, r.w);
    wapi_wasm_write_i32(h_ptr, r.h);
    return WAPI_OK;
}

static NativeSymbol g_symbols[] = {
    { "display_count",             (void*)host_display_count,             "()i",     NULL },
    { "display_get_info",          (void*)host_display_get_info,          "(ii)i",   NULL },
    { "display_get_subpixels",     (void*)host_display_get_subpixels,     "(iiii)i", NULL },
    { "display_get_usable_bounds", (void*)host_display_get_usable_bounds, "(iiiii)i", NULL },
};

wapi_cap_registration_t wapi_host_display_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_display",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
