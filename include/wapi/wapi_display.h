/**
 * WAPI - Display Information
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
 */

#ifndef WAPI_DISPLAY_H
#define WAPI_DISPLAY_H

#include "wapi.h"

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
_Static_assert(_Alignof(wapi_subpixel_t) == 1, "wapi_subpixel_t must be 1-byte aligned");

/* ============================================================
 * Display Info
 * ============================================================
 *
 * Layout (56 bytes, align 8):
 *   Offset  0: uint32_t display_id         Display identifier
 *   Offset  4: int32_t  x                  Display x in global coords
 *   Offset  8: int32_t  y                  Display y in global coords
 *   Offset 12: int32_t  width              Width in pixels
 *   Offset 16: int32_t  height             Height in pixels
 *   Offset 20: float    refresh_rate_hz    Refresh rate in Hz
 *   Offset 24: float    scale_factor       DPI scale (e.g. 2.0 for Retina)
 *   Offset 28: uint32_t _pad0              (alignment padding)
 *   Offset 32: wapi_stringview_t name      Display name (UTF-8, 16 bytes)
 *   Offset 48: uint8_t  is_primary         1 if primary display
 *   Offset 49: uint8_t  orientation        0=land, 1=port, 2=land-flip, 3=port-flip
 *   Offset 50: uint8_t  subpixel_count     Number of sub-pixels per pixel (0=unknown)
 *   Offset 51: uint8_t  _pad1
 *   Offset 52: uint16_t rotation_deg       Physical rotation: 0, 90, 180, 270
 *   Offset 54: uint8_t  _reserved[2]
 */

typedef struct wapi_display_info_t {
    uint32_t    display_id;
    int32_t     x;
    int32_t     y;
    int32_t     width;
    int32_t     height;
    float       refresh_rate_hz;
    float       scale_factor;
    uint32_t    _pad0;
    wapi_stringview_t name;
    uint8_t     is_primary;
    uint8_t     orientation;
    uint8_t     subpixel_count;  /* 0=unknown, typically 3-4 */
    uint8_t     _pad1;
    uint16_t    rotation_deg;    /* 0, 90, 180, 270 */
    uint8_t     _reserved[2];
} wapi_display_info_t;

_Static_assert(sizeof(wapi_display_info_t) == 56,
               "wapi_display_info_t must be 56 bytes");
_Static_assert(_Alignof(wapi_display_info_t) == 8,
               "wapi_display_info_t must be 8-byte aligned");

/* ============================================================
 * Display Functions
 * ============================================================ */

/**
 * Get the number of connected displays.
 *
 * @return Number of displays (>= 0), or negative on error.
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

/* ============================================================
 * Panel Type
 * ============================================================
 * Physical display technology. Returned by
 * wapi_display_get_panel_info() when the host has a matching entry
 * in its installed panel database;
 * WAPI_DISPLAY_PANEL_TYPE_UNKNOWN otherwise.
 */

typedef enum wapi_display_panel_type_t {
    WAPI_DISPLAY_PANEL_TYPE_UNKNOWN      = 0,
    WAPI_DISPLAY_PANEL_TYPE_IPSLCD       = 1,
    WAPI_DISPLAY_PANEL_TYPE_VALCD        = 2,
    WAPI_DISPLAY_PANEL_TYPE_TNLCD        = 3,
    WAPI_DISPLAY_PANEL_TYPE_OLED         = 4,
    WAPI_DISPLAY_PANEL_TYPE_WOLED        = 5,
    WAPI_DISPLAY_PANEL_TYPE_QDOLED       = 6,
    WAPI_DISPLAY_PANEL_TYPE_MICROLED     = 7,
    WAPI_DISPLAY_PANEL_TYPE_STNLCD       = 8,
    WAPI_DISPLAY_PANEL_TYPE_PASSIVELCD   = 9,
    WAPI_DISPLAY_PANEL_TYPE_CRTSHADOW    = 10,
    WAPI_DISPLAY_PANEL_TYPE_CRTTRINITRON = 11,
    WAPI_DISPLAY_PANEL_TYPE_CRTSLOT      = 12,
    WAPI_DISPLAY_PANEL_TYPE_PLASMA       = 13,
    /* E-ink variants.
     *   EINKCARTA   — monochrome electrophoretic (Carta / Pearl).
     *                 No sub-pixel structure. Slow full refresh;
     *                 partial updates available. Animation-unsafe.
     *   EINKKALEIDO — Carta panel with an RGB color-filter overlay.
     *                 Has a stripe RGB sub-pixel grid. Slow; colors
     *                 are desaturated (~4k unique colors). Animation-
     *                 unsafe. Sub-pixel AA usable.
     *   EINKGALLERY — True-color electrophoretic (Gallery 3 / ACeP).
     *                 No sub-pixel grid; pigments generate color
     *                 directly. Multi-second full refresh; render
     *                 full pages at a time. Animation-unsafe. */
    WAPI_DISPLAY_PANEL_TYPE_EINKCARTA    = 14,
    WAPI_DISPLAY_PANEL_TYPE_EINKKALEIDO  = 15,
    WAPI_DISPLAY_PANEL_TYPE_EINKGALLERY  = 16,
    WAPI_DISPLAY_PANEL_TYPE_DLP          = 17,
    WAPI_DISPLAY_PANEL_TYPE_LCOS         = 18,
    WAPI_DISPLAY_PANEL_TYPE_PROJECTOR    = 19,
    WAPI_DISPLAY_PANEL_TYPE_FORCE32      = 0x7FFFFFFF
} wapi_display_panel_type_t;

/* ============================================================
 * Panel Update Class
 * ============================================================
 * Derived from panel technology: how quickly the panel refreshes
 * and how friendly it is to animation. Guests use this to decide
 * whether to animate at all (e-ink must not) or whether to aim
 * for high frame rates.
 *   FAST       LCD / OLED / MicroLED / plasma / CRT — animate freely
 *   SLOW       e-ink Carta / Kaleido — avoid animation; fade
 *              transitions between static states only
 *   VERY_SLOW  e-ink Gallery / ACeP — seconds per refresh, render
 *              whole pages at a time
 * Matches CSS's (update: fast | slow | none) media feature.
 */
typedef enum wapi_display_panel_update_t {
    WAPI_DISPLAY_PANEL_UPDATE_UNKNOWN   = 0,
    WAPI_DISPLAY_PANEL_UPDATE_FAST      = 1,
    WAPI_DISPLAY_PANEL_UPDATE_SLOW      = 2,
    WAPI_DISPLAY_PANEL_UPDATE_VERY_SLOW = 3,
    WAPI_DISPLAY_PANEL_UPDATE_FORCE32   = 0x7FFFFFFF
} wapi_display_panel_update_t;

/* ============================================================
 * Panel Info
 * ============================================================
 * Static, cacheable panel-identity data. Carries primary
 * measurements — physical size, measured luminance, CIE primaries,
 * refresh range — not derived labels. Marketing tiers (HDR400 /
 * HDR1000), VRR brands (G-Sync / FreeSync), and gamut shorthands
 * (sRGB / P3) are intentionally absent: they either lie (cheap
 * "HDR400" panels with 250-nit peaks claim HDR10) or are
 * reconstructable from the measurements a guest actually needs.
 * Reports what the panel is physically capable of — not what it
 * is currently doing.
 *
 * All fields 0 when unknown. The host fills what its device DB
 * knows; unfilled fields stay 0. Independent of
 * wapi_display_info_t (OS-reported geometry + current refresh).
 *
 * Layout (72 bytes, align 4):
 *   Offset  0: uint32_t type             wapi_display_panel_type_t
 *                                        (physical panel technology)
 *   Offset  4: uint32_t width_mm         Visible-area physical width
 *   Offset  8: uint32_t height_mm        Visible-area physical height
 *   Offset 12: uint32_t diagonal_mm      Convenience; equals
 *                                        sqrt(w² + h²) rounded to mm
 *                                        when both axes are known.
 *
 *   Luminance (all peak SDR / HDR in integer cd/m² = nits; min in
 *   milli-cd/m² to preserve OLED floor precision, 500 mcd = 0.5 cd):
 *   Offset 16: uint32_t peak_sdr_cd_m2   Measured SDR peak
 *   Offset 20: uint32_t peak_hdr_cd_m2   Measured HDR peak;
 *                                        0 = not HDR-capable
 *   Offset 24: uint32_t min_mcd_m2       Measured floor
 *
 *   White point:
 *   Offset 28: uint32_t white_point_k    Kelvin; d65=6500, d93=9300,
 *                                        d50=5000.
 *
 *   CIE xy primaries (u16 fixed-point, value / 65535 = 0.0..1.0):
 *   Offset 32: uint16_t primary_rx, primary_ry
 *   Offset 36: uint16_t primary_gx, primary_gy
 *   Offset 40: uint16_t primary_bx, primary_by
 *   Offset 44: uint16_t primary_wx, primary_wy
 *
 *   Gamut coverage percent (0..100; coverage of named standard
 *   gamut, so sRGB-native panel can be ~70% P3):
 *   Offset 48: uint8_t  coverage_srgb
 *   Offset 49: uint8_t  coverage_p3
 *   Offset 50: uint8_t  coverage_rec2020
 *   Offset 51: uint8_t  coverage_adobe_rgb
 *
 *   Refresh range (milliHertz for 23.976 / 47.952 / 59.94
 *   precision; 0 = fixed, min==max = fixed at that rate):
 *   Offset 52: uint32_t refresh_min_mhz
 *   Offset 56: uint32_t refresh_max_mhz
 *
 *   Update class + surface input:
 *   Offset 60: uint32_t update_class     wapi_display_panel_update_t
 *   Offset 64: uint32_t stylus_pressure_levels  0 = no stylus / unknown
 *   Offset 68: uint8_t  has_touch
 *   Offset 69: uint8_t  has_stylus
 *   Offset 70: uint8_t  stylus_has_tilt
 *   Offset 71: uint8_t  _pad
 */

typedef struct wapi_display_panel_info_t {
    uint32_t    type;
    uint32_t    width_mm;
    uint32_t    height_mm;
    uint32_t    diagonal_mm;
    uint32_t    peak_sdr_cd_m2;
    uint32_t    peak_hdr_cd_m2;
    uint32_t    min_mcd_m2;
    uint32_t    white_point_k;
    uint16_t    primary_rx, primary_ry;
    uint16_t    primary_gx, primary_gy;
    uint16_t    primary_bx, primary_by;
    uint16_t    primary_wx, primary_wy;
    uint8_t     coverage_srgb;
    uint8_t     coverage_p3;
    uint8_t     coverage_rec2020;
    uint8_t     coverage_adobe_rgb;
    uint32_t    refresh_min_mhz;
    uint32_t    refresh_max_mhz;
    uint32_t    update_class;
    uint32_t    stylus_pressure_levels;
    uint8_t     has_touch;
    uint8_t     has_stylus;
    uint8_t     stylus_has_tilt;
    uint8_t     _pad;
} wapi_display_panel_info_t;

_Static_assert(sizeof(wapi_display_panel_info_t) == 72,
               "wapi_display_panel_info_t must be 72 bytes");
_Static_assert(_Alignof(wapi_display_panel_info_t) == 4,
               "wapi_display_panel_info_t must be 4-byte aligned");

/**
 * Get installed-panel-database info for a display.
 *
 * @param index     Display index (0-based; same indexing as
 *                  wapi_display_get_info).
 * @param info_ptr  [out] Pointer to wapi_display_panel_info_t.
 * @return WAPI_OK when the host has a database entry for the
 *         attached panel,
 *         WAPI_ERR_NOENT when no entry matches,
 *         WAPI_ERR_INVAL if index is out of range.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_display, display_get_panel_info)
wapi_result_t wapi_display_get_panel_info(int32_t index,
                                          wapi_display_panel_info_t* info_ptr);

/* ============================================================
 * Physical Shape / Geometry
 * ============================================================
 * A display's addressable pixel grid is always rectangular
 * (wapi_display_info_t.width × .height). What the user actually
 * sees may be a strict subset: a watch's round panel, a phone's
 * rounded rect with a notch, a laptop with a camera cutout.
 *
 * The shape is expressed in two parts:
 *   1. An envelope — the outline of the visible region (FULL /
 *      ROUNDED_RECT / CIRCLE). Covers every physical panel that has
 *      ever shipped. Per-corner radii for ROUNDED_RECT.
 *   2. A list of cutouts — axis-aligned rectangles inside the
 *      envelope where pixels are physically absent (notch, camera
 *      hole, under-display sensor). Drawing there has no effect.
 *
 * All coordinates are in physical pixels, in the display's canonical
 * (unrotated) orientation. Callers that only want "where can I
 * safely place content" should skip this layer and call
 * wapi_surface_get_safe_rect, which folds envelope, cutouts, current
 * rotation, and system UI into a single usable rectangle. */

typedef enum wapi_display_envelope_t {
    WAPI_DISPLAY_ENVELOPE_FULL         = 0, /* rectangle — monitors, TVs, projectors */
    WAPI_DISPLAY_ENVELOPE_ROUNDED_RECT = 1, /* phones, modern laptops, Apple Watch */
    WAPI_DISPLAY_ENVELOPE_CIRCLE       = 2, /* inscribed in width×height; round watches */
    WAPI_DISPLAY_ENVELOPE_FORCE32      = 0x7FFFFFFF
} wapi_display_envelope_t;

/** Axis-aligned cutout rect in physical pixel coords (4 × int32). */
typedef struct wapi_display_cutout_t {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} wapi_display_cutout_t;

_Static_assert(sizeof(wapi_display_cutout_t) == 16,
               "wapi_display_cutout_t must be 16 bytes");

/* Layout (16 bytes, align 4):
 *   Offset  0: uint32_t envelope           wapi_display_envelope_t
 *   Offset  4: uint16_t corner_radius_px[4] TL, TR, BR, BL (pixels).
 *                                          Meaningful only when
 *                                          envelope == ROUNDED_RECT;
 *                                          zero otherwise.
 *   Offset 12: uint32_t cutout_count       Number of hardware cutout
 *                                          rects; fetch via
 *                                          wapi_display_get_cutouts. */
typedef struct wapi_display_geometry_t {
    uint32_t envelope;
    uint16_t corner_radius_px[4];
    uint32_t cutout_count;
} wapi_display_geometry_t;

_Static_assert(sizeof(wapi_display_geometry_t) == 16,
               "wapi_display_geometry_t must be 16 bytes");
_Static_assert(_Alignof(wapi_display_geometry_t) == 4,
               "wapi_display_geometry_t must be 4-byte aligned");

/**
 * Get the physical shape of a display.
 *
 * @param index    Display index (0-based).
 * @param geom_ptr [out] Pointer to wapi_display_geometry_t.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if index is out of range.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_display, display_get_geometry)
wapi_result_t wapi_display_get_geometry(int32_t index,
                                        wapi_display_geometry_t* geom_ptr);

/**
 * Get the hardware cutout rectangles for a display.
 *
 * Fills up to max_count wapi_display_cutout_t entries describing
 * regions where pixels are physically absent (notch, camera hole,
 * etc.). Returns the actual count via count_ptr. Rects are in
 * canonical (unrotated) physical pixel coordinates.
 *
 * @param index         Display index (0-based).
 * @param cutouts_ptr   [out] Array of wapi_display_cutout_t to fill.
 * @param max_count     Max entries to write.
 * @param count_ptr     [out] Actual number of cutouts written.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if index is out of range.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_display, display_get_cutouts)
wapi_result_t wapi_display_get_cutouts(int32_t index,
                                       wapi_display_cutout_t* cutouts_ptr,
                                       int32_t max_count,
                                       int32_t* count_ptr);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_DISPLAY_H */
