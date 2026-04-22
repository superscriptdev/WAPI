/**
 * WAPI Desktop Runtime - Theme (wapi_theme.h)
 *
 * Reads from the Personalize registry key and DWM colorization.
 * Contrast / reduced-motion / font-scale via SystemParametersInfo.
 */

#include "wapi_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/* Declared in wapi_host_sysinfo.c. */
uint32_t wapi_host_win_dark_mode(void);
uint32_t wapi_host_win_accent_rgba(void);

static wasm_trap_t* h_is_dark(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(wapi_host_win_dark_mode() ? 1 : 0);
    return NULL;
}

static wasm_trap_t* h_get_accent(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t out_ptr = WAPI_ARG_U32(0);
    uint32_t rgba = wapi_host_win_accent_rgba();
    if (!wapi_wasm_write_u32(out_ptr, rgba)) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_contrast(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    /* High-contrast → more-contrast (1). Otherwise normal (0). */
    HIGHCONTRASTW hc; memset(&hc, 0, sizeof(hc)); hc.cbSize = sizeof(hc);
    int pref = 0;
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0)) {
        if (hc.dwFlags & HCF_HIGHCONTRASTON) pref = 1;
    }
    WAPI_RET_I32(pref);
    return NULL;
}

static wasm_trap_t* h_reduced_motion(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    BOOL disabled = FALSE;
    /* SPI_GETCLIENTAREAANIMATION returns TRUE when animations are
     * *enabled*; we invert for reduced-motion. */
    BOOL anim_on = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &anim_on, 0);
    disabled = !anim_on;
    WAPI_RET_I32(disabled ? 1 : 0);
    return NULL;
}

static wasm_trap_t* h_font_scale(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t out_ptr = WAPI_ARG_U32(0);
    /* Approximation: scale = system-DPI / 96. Matches what most apps
     * treat as "text size" on Windows. */
    HDC dc = GetDC(NULL);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    float scale = dpi > 0 ? (float)dpi / 96.0f : 1.0f;
    if (!wapi_wasm_write_f32(out_ptr, scale)) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

void wapi_host_register_theme(wasmtime_linker_t* linker) {
    WAPI_DEFINE_0_1(linker, "wapi_theme", "theme_is_dark",              h_is_dark);
    WAPI_DEFINE_1_1(linker, "wapi_theme", "theme_get_accent_color",     h_get_accent);
    WAPI_DEFINE_0_1(linker, "wapi_theme", "theme_get_contrast_preference", h_contrast);
    WAPI_DEFINE_0_1(linker, "wapi_theme", "theme_get_reduced_motion",   h_reduced_motion);
    WAPI_DEFINE_1_1(linker, "wapi_theme", "theme_get_font_scale",       h_font_scale);
}
