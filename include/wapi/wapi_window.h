/**
 * WAPI - Window Management
 * Version 1.0.0
 *
 * OS window operations for on-screen surfaces. Chain a
 * wapi_window_desc_t onto a surface descriptor to create
 * a windowed surface with title, decorations, and resize behavior.
 *
 * All functions take a surface handle and return WAPI_ERR_NOTSUP
 * on offscreen surfaces.
 *
 * Import module: "wapi_window"
 */

#ifndef WAPI_WINDOW_H
#define WAPI_WINDOW_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Window Flags
 * ============================================================ */

#define WAPI_WINDOW_FLAG_RESIZABLE     0x0001  /* Window can be resized */
#define WAPI_WINDOW_FLAG_BORDERLESS    0x0002  /* No window decorations */
#define WAPI_WINDOW_FLAG_FULLSCREEN    0x0004  /* Start fullscreen */
#define WAPI_WINDOW_FLAG_HIDDEN        0x0008  /* Start hidden */
#define WAPI_WINDOW_FLAG_ALWAYSONTOP 0x0010  /* Stay above other windows */

/* ============================================================
 * Window Config (Chained Struct)
 * ============================================================
 * Chain onto wapi_surface_desc_t::nextInChain to create a
 * windowed surface.  sType = WAPI_STYPE_WINDOW_CONFIG.
 *
 * Layout (40 bytes, align 8):
 *   Offset  0: wapi_chain_t chain   (16 bytes)
 *   Offset 16: wapi_stringview_t    title   (16 bytes, UTF-8, 0 = untitled)
 *   Offset 32: uint32_t              window_flags  WAPI_WINDOW_FLAG_*
 *   Offset 36: uint32_t              _pad
 */

typedef struct wapi_window_desc_t {
    wapi_chain_t   chain;
    wapi_stringview_t      title;
    uint32_t                window_flags;
    uint32_t                _pad;
} wapi_window_desc_t;

/* Window events are defined in wapi.h as WAPI_EVENT_WINDOW_*
 * and delivered via the event queue on the surface handle. */

/* ============================================================
 * Window Functions
 * ============================================================ */

/**
 * Set the window title (ignored on non-desktop platforms).
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_window, set_title)
wapi_result_t wapi_window_set_title(wapi_handle_t surface,
                                    wapi_stringview_t title);

/**
 * Get the surface size in device-independent (logical) units.
 * Different from pixel size on high-DPI displays.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_window, get_size_logical)
wapi_result_t wapi_window_get_size_logical(wapi_handle_t surface,
                                           int32_t* width, int32_t* height);

/**
 * Set fullscreen mode.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_window, set_fullscreen)
wapi_result_t wapi_window_set_fullscreen(wapi_handle_t surface,
                                         wapi_bool_t fullscreen);

/**
 * Show or hide the window.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_window, set_visible)
wapi_result_t wapi_window_set_visible(wapi_handle_t surface,
                                      wapi_bool_t visible);

/**
 * Minimize the window (desktop only).
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_window, minimize)
wapi_result_t wapi_window_minimize(wapi_handle_t surface);

/**
 * Maximize the window (desktop only).
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_window, maximize)
wapi_result_t wapi_window_maximize(wapi_handle_t surface);

/**
 * Restore the window from minimized/maximized state.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_window, restore)
wapi_result_t wapi_window_restore(wapi_handle_t surface);

/* Cursor control is per-mouse device: see wapi_input.h (Mouse section) */

#ifdef __cplusplus
}
#endif

#endif /* WAPI_WINDOW_H */
