/**
 * WAPI - Surfaces (Render Targets)
 * Version 1.0.0
 *
 * A surface is a rectangular render target. What backs it depends
 * on the descriptor's chain:
 *   - With wapi_window_desc_t chained: OS window (desktop),
 *     canvas (browser), or display surface (mobile)
 *   - Without: offscreen buffer for texture generation, thumbnails,
 *     screenshot tests, or headless rendering
 *
 * Surfaces are render targets only. OS window management (title,
 * fullscreen, minimize, cursor) lives in wapi_window.h.
 *
 * Import module: "wapi_surface"
 */

#ifndef WAPI_SURFACE_H
#define WAPI_SURFACE_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Surface Flags
 * ============================================================ */

#define WAPI_SURFACE_FLAG_HIGH_DPI      0x0001  /* Enable high-DPI rendering */
#define WAPI_SURFACE_FLAG_TRANSPARENT   0x0002  /* Transparent background */

/* ============================================================
 * Surface Descriptor
 * ============================================================
 *
 * Layout (24 bytes, align 8):
 *   Offset  0: uint64_t     nextInChain  (address of chained struct, or 0)
 *   Offset  8: int32_t      width        Requested width (0 = host default)
 *   Offset 12: int32_t      height       Requested height (0 = host default)
 *   Offset 16: uint64_t     flags        WAPI_SURFACE_FLAG_* (wapi_flags_t)
 */

typedef struct wapi_surface_desc_t {
    uint64_t                nextInChain;  /* Address of wapi_chain_t, or 0 */
    int32_t                 width;
    int32_t                 height;
    wapi_flags_t            flags;
} wapi_surface_desc_t;

/* ============================================================
 * Surface Events
 * ============================================================
 * Delivered via wapi_event.h's event queue.
 * Window-specific events (close, focus, minimize, etc.) are
 * defined in wapi_window.h.
 */

typedef enum wapi_surface_event_type_t {
    WAPI_SURFACE_EVENT_RESIZED      = 0x0200,  /* Surface size changed */
    WAPI_SURFACE_EVENT_DPI_CHANGED  = 0x020A,  /* Display DPI changed */
    WAPI_SURFACE_EVENT_FORCE32      = 0x7FFFFFFF
} wapi_surface_event_type_t;

/* ============================================================
 * Surface Functions
 * ============================================================ */

/**
 * Create a new surface.
 *
 * For a windowed surface, chain a wapi_window_desc_t onto
 * desc->nextInChain.  Without it, the surface is an offscreen
 * render target.
 *
 * @param desc     Surface descriptor.
 * @param surface  [out] Surface handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_surface, create)
wapi_result_t wapi_surface_create(const wapi_surface_desc_t* desc,
                                  wapi_handle_t* surface);

/**
 * Destroy a surface and release its resources.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_surface, destroy)
wapi_result_t wapi_surface_destroy(wapi_handle_t surface);

/**
 * Get the current surface size in pixels.
 *
 * @param surface  Surface handle.
 * @param width    [out] Width in pixels.
 * @param height   [out] Height in pixels.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_surface, get_size)
wapi_result_t wapi_surface_get_size(wapi_handle_t surface,
                                    int32_t* width, int32_t* height);

/**
 * Get the DPI scale factor for a surface.
 * Offscreen surfaces always return 1.0.
 *
 * @param surface  Surface handle.
 * @param scale    [out] Scale factor (1.0 = standard, 2.0 = Retina).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_surface, get_dpi_scale)
wapi_result_t wapi_surface_get_dpi_scale(wapi_handle_t surface, float* scale);

/**
 * Request a surface size change.
 * The host may ignore this (e.g., in a browser).
 * A WAPI_SURFACE_EVENT_RESIZED event is delivered if the size changes.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_surface, request_size)
wapi_result_t wapi_surface_request_size(wapi_handle_t surface,
                                        int32_t width, int32_t height);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SURFACE_H */
