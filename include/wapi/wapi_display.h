/**
 * WAPI - Display Information Capability
 * Version 1.0.0
 *
 * Enumerate connected displays and query their geometry, refresh rate,
 * scale factor, and usable bounds (excluding taskbar / dock).
 *
 * Maps to: Screen API / window.screen (Web),
 *          NSScreen (macOS), UIScreen (iOS),
 *          Display / DisplayManager (Android),
 *          EnumDisplayMonitors / GetMonitorInfo (Windows),
 *          XRandR / wl_output (Linux)
 *
 * Import module: "wapi_display"
 *
 * Query availability with wapi_capability_supported("wapi.display", 12)
 */

#ifndef WAPI_DISPLAY_H
#define WAPI_DISPLAY_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Sub-pixel Description
 * ============================================================
 * Real displays have varied sub-pixel arrangements:
 *   - Standard LCD:  3 horizontal RGB stripes
 *   - PenTile OLED:  RGBG diamond (2 greens per pixel)
 *   - LG WOLED:      WRGB (4 sub-pixels incl. white)
 *   - Delta/mosaic:  Non-rectangular layouts
 *
 * Each sub-pixel has a color and a position within the pixel cell.
 * Positions are normalized to 0-255 within the cell bounding box.
 * Query via wapi_display_get_subpixels().
 */

typedef enum wapi_subpixel_color_t {
    WAPI_SUBPIXEL_RED     = 0,
    WAPI_SUBPIXEL_GREEN   = 1,
    WAPI_SUBPIXEL_BLUE    = 2,
    WAPI_SUBPIXEL_WHITE   = 3,
    WAPI_SUBPIXEL_COLOR_FORCE32 = 0x7FFFFFFF
} wapi_subpixel_color_t;

/** Single sub-pixel within a pixel cell (4 bytes). */
typedef struct wapi_subpixel_t {
    uint8_t     color;  /* wapi_subpixel_color_t */
    uint8_t     x;      /* X position in cell, 0-255 */
    uint8_t     y;      /* Y position in cell, 0-255 */
    uint8_t     _pad;
} wapi_subpixel_t;

_Static_assert(sizeof(wapi_subpixel_t) == 4, "wapi_subpixel_t must be 4 bytes");

/* ============================================================
 * Display Info
 * ============================================================
 *
 * Layout (48 bytes, align 4):
 *   Offset  0: uint32_t display_id         Display identifier
 *   Offset  4: int32_t  x                  Display x in global coords
 *   Offset  8: int32_t  y                  Display y in global coords
 *   Offset 12: int32_t  width              Width in pixels
 *   Offset 16: int32_t  height             Height in pixels
 *   Offset 20: float    refresh_rate_hz    Refresh rate in Hz
 *   Offset 24: float    scale_factor       DPI scale (e.g. 2.0 for Retina)
 *   Offset 28: uint32_t name_ptr           Pointer to UTF-8 display name
 *   Offset 32: uint32_t name_len           Byte length of display name
 *   Offset 36: uint8_t  is_primary         1 if primary display
 *   Offset 37: uint8_t  orientation        0=land, 1=port, 2=land-flip, 3=port-flip
 *   Offset 38: uint8_t  subpixel_count     Number of sub-pixels per pixel (0=unknown)
 *   Offset 39: uint8_t  _pad
 *   Offset 40: uint16_t rotation_deg       Physical rotation: 0, 90, 180, 270
 *   Offset 42: uint8_t  _reserved[6]
 */

typedef struct wapi_display_info_t {
    uint32_t    display_id;
    int32_t     x;
    int32_t     y;
    int32_t     width;
    int32_t     height;
    float       refresh_rate_hz;
    float       scale_factor;
    uint32_t    name_ptr;
    uint32_t    name_len;
    uint8_t     is_primary;
    uint8_t     orientation;
    uint8_t     subpixel_count;  /* 0=unknown, typically 3-4 */
    uint8_t     _pad;
    uint16_t    rotation_deg;    /* 0, 90, 180, 270 */
    uint8_t     _reserved[6];
} wapi_display_info_t;

_Static_assert(sizeof(wapi_display_info_t) == 48,
               "wapi_display_info_t must be 48 bytes");

/* ============================================================
 * Display Functions
 * ============================================================ */

/**
 * Get the number of connected displays.
 *
 * @return Number of displays (>= 1).
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_display, display_count)
int32_t wapi_display_count(void);

/**
 * Get information about a display.
 *
 * @param index     Display index (0-based).
 * @param info_ptr  [out] Pointer to wapi_display_info_t.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if index is out of range.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_display, display_get_info)
wapi_result_t wapi_display_get_info(int32_t index,
                                    wapi_display_info_t* info_ptr);

/**
 * Get the sub-pixel layout of a display.
 *
 * Each entry describes one sub-pixel: its color (R/G/B/W) and position
 * within the pixel cell (0-255 normalized). Returns actual count via
 * count_ptr. Check subpixel_count in wapi_display_info_t first.
 *
 * Examples:
 *   Standard LCD:   [{R,0,128}, {G,85,128}, {B,170,128}]
 *   PenTile RGBG:   [{R,0,0}, {G,255,0}, {B,0,255}, {G,255,255}]
 *   LG WOLED WRGB:  [{W,0,128}, {R,64,128}, {G,128,128}, {B,192,128}]
 *
 * @param index          Display index (0-based).
 * @param subpixels_ptr  [out] Array of wapi_subpixel_t to fill.
 * @param max_count      Max entries to write.
 * @param count_ptr      [out] Actual number of sub-pixels written.
 * @return WAPI_OK, WAPI_ERR_INVAL, or WAPI_ERR_NOTSUP if unknown.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_display, display_get_subpixels)
wapi_result_t wapi_display_get_subpixels(int32_t index,
                                         wapi_subpixel_t* subpixels_ptr,
                                         int32_t max_count,
                                         int32_t* count_ptr);

/**
 * Get the usable bounds of a display (excluding taskbar, dock, etc.).
 *
 * @param index  Display index (0-based).
 * @param x_ptr  [out] Pointer to int32_t x origin.
 * @param y_ptr  [out] Pointer to int32_t y origin.
 * @param w_ptr  [out] Pointer to int32_t usable width.
 * @param h_ptr  [out] Pointer to int32_t usable height.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if index is out of range.
 *
 * Wasm signature: (i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_display, display_get_usable_bounds)
wapi_result_t wapi_display_get_usable_bounds(int32_t index,
                                             int32_t* x_ptr,
                                             int32_t* y_ptr,
                                             int32_t* w_ptr,
                                             int32_t* h_ptr);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_DISPLAY_H */
