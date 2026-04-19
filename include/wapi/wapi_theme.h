/**
 * WAPI - Theme / Appearance
 * Version 1.0.0
 *
 * Query the host system's visual preferences: dark mode, accent color,
 * contrast preference, reduced-motion setting, and font scale.
 *
 * Maps to: prefers-color-scheme / prefers-contrast / prefers-reduced-motion (Web),
 *          UITraitCollection / NSAppearance (iOS/macOS),
 *          UiModeManager / Configuration (Android),
 *          SystemParametersInfo / DwmGetColorizationColor (Windows),
 *          GSettings / portal (Linux)
 *
 * Import module: "wapi_theme"
 */

#ifndef WAPI_THEME_H
#define WAPI_THEME_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Theme Functions
 * ============================================================ */

/**
 * Check if the system is in dark mode.
 *
 * @return 1 for dark mode, 0 for light mode.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_theme, theme_is_dark)
wapi_bool_t wapi_theme_is_dark(void);

/**
 * Get the system accent color.
 *
 * @param rgba_ptr  [out] Pointer to uint32_t RGBA (0xRRGGBBAA).
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if not available.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_theme, theme_get_accent_color)
wapi_result_t wapi_theme_get_accent_color(uint32_t* rgba_ptr);

/**
 * Get the user's contrast preference.
 *
 * @return 0 = normal, 1 = more contrast, 2 = less contrast.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_theme, theme_get_contrast_preference)
int32_t wapi_theme_get_contrast_preference(void);

/**
 * Check if the user prefers reduced motion.
 *
 * @return 1 if reduced motion is preferred, 0 otherwise.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_theme, theme_get_reduced_motion)
wapi_bool_t wapi_theme_get_reduced_motion(void);

/**
 * Get the system font scale factor.
 *
 * A value of 1.0 indicates the default system font size. Values greater
 * than 1.0 indicate larger text preferences.
 *
 * @param scale_ptr  [out] Pointer to float scale factor.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_theme, theme_get_font_scale)
wapi_result_t wapi_theme_get_font_scale(float* scale_ptr);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_THEME_H */
