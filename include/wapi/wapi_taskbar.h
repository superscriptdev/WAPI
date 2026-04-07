/**
 * WAPI - Taskbar / Dock Capability
 * Version 1.0.0
 *
 * Taskbar/dock progress indicators, badge counts, attention requests,
 * and overlay icons.
 *
 * Maps to: ITaskbarList3 (Windows), NSDockTile (macOS),
 *          LauncherApps / ShortcutBadger (Android)
 *
 * Import module: "wapi_taskbar"
 *
 * Query availability with wapi_capability_supported("wapi.taskbar", 11)
 */

#ifndef WAPI_TASKBAR_H
#define WAPI_TASKBAR_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Progress State
 * ============================================================ */

typedef enum wapi_taskbar_progress_t {
    WAPI_TASKBAR_PROGRESS_NONE          = 0,
    WAPI_TASKBAR_PROGRESS_INDETERMINATE = 1,
    WAPI_TASKBAR_PROGRESS_NORMAL        = 2,
    WAPI_TASKBAR_PROGRESS_ERROR         = 3,
    WAPI_TASKBAR_PROGRESS_PAUSED        = 4,
    WAPI_TASKBAR_PROGRESS_FORCE32       = 0x7FFFFFFF
} wapi_taskbar_progress_t;

/* ============================================================
 * Progress Functions
 * ============================================================ */

/**
 * Set progress bar on taskbar/dock icon.
 *
 * @param surface  Surface whose taskbar icon to update.
 * @param state    Progress state.
 * @param value    Progress value (0.0 to 1.0). Ignored for NONE/INDETERMINATE.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, f32) -> i32
 */
WAPI_IMPORT(wapi_taskbar, set_progress)
wapi_result_t wapi_taskbar_set_progress(wapi_handle_t surface,
                                     wapi_taskbar_progress_t state, float value);

/* ============================================================
 * Badge Count
 * ============================================================ */

/**
 * Set badge count on app icon (0 to clear).
 *
 * @param count  Badge number. 0 removes the badge.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_taskbar, set_badge)
wapi_result_t wapi_taskbar_set_badge(uint32_t count);

/* ============================================================
 * Attention Request
 * ============================================================ */

/**
 * Request user attention (flash taskbar / bounce dock icon).
 *
 * @param surface   Surface requesting attention.
 * @param critical  If true, attention persists until user interacts.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_taskbar, request_attention)
wapi_result_t wapi_taskbar_request_attention(wapi_handle_t surface,
                                          wapi_bool_t critical);

/* ============================================================
 * Overlay Icon
 * ============================================================ */

/**
 * Set overlay icon on taskbar icon (small icon badge, Windows-specific).
 *
 * @param surface      Surface whose taskbar icon to overlay.
 * @param icon_data    Icon image data (PNG).
 * @param icon_len     Icon data length.
 * @param description  Accessible description of the overlay.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_taskbar, set_overlay_icon)
wapi_result_t wapi_taskbar_set_overlay_icon(wapi_handle_t surface,
                                         const void* icon_data, wapi_size_t icon_len,
                                         wapi_string_view_t description);

/**
 * Clear overlay icon from taskbar icon.
 *
 * @param surface  Surface whose overlay to clear.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_taskbar, clear_overlay)
wapi_result_t wapi_taskbar_clear_overlay(wapi_handle_t surface);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_TASKBAR_H */
