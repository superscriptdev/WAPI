/**
 * WAPI - System Tray Capability
 * Version 1.0.0
 *
 * System tray / notification area / menu bar extras.
 *
 * Maps to: NSStatusItem (macOS), Shell_NotifyIcon (Windows),
 *          AppIndicator (Linux)
 *
 * Import module: "wapi_tray"
 *
 * Query availability with wapi_capability_supported("wapi.tray", 8)
 */

#ifndef WAPI_TRAY_H
#define WAPI_TRAY_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Tray Item Flags
 * ============================================================ */

#define WAPI_TRAY_SEPARATOR  0x1
#define WAPI_TRAY_DISABLED   0x2
#define WAPI_TRAY_CHECKED    0x4

/* ============================================================
 * Tray Types
 * ============================================================ */

/**
 * Tray menu item descriptor.
 *
 * Layout (40 bytes, align 8):
 *   Offset  0: uint32_t     id
 *   Offset  4: uint32_t     label_len
 *   Offset  8: uint64_t     label_ptr    Linear memory address of label string
 *   Offset 16: uint32_t     icon_len
 *   Offset 20: uint32_t     _pad
 *   Offset 24: uint64_t     icon_ptr     Linear memory address of icon data
 *   Offset 32: wapi_flags_t flags
 */
typedef struct wapi_tray_menu_item_t {
    uint32_t     id;
    uint32_t     label_len;
    uint64_t     label_ptr;
    uint32_t     icon_len;
    uint32_t     _pad;
    uint64_t     icon_ptr;
    wapi_flags_t flags;
} wapi_tray_menu_item_t;

_Static_assert(sizeof(wapi_tray_menu_item_t) == 40,
               "wapi_tray_menu_item_t must be 40 bytes");
_Static_assert(_Alignof(wapi_tray_menu_item_t) == 8,
               "wapi_tray_menu_item_t must be 8-byte aligned");

/* ============================================================
 * Tray Functions
 * ============================================================ */

/**
 * Create a system tray icon.
 *
 * @param icon_data    Pointer to icon image data.
 * @param icon_len     Size of icon data in bytes.
 * @param tooltip      Tooltip string.
 * @param out_handle   [out] Receives the tray handle.
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if not supported.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_tray, tray_create)
wapi_result_t wapi_tray_create(const void* icon_data, wapi_size_t icon_len,
                               wapi_stringview_t tooltip,
                               wapi_handle_t* out_handle);

/**
 * Destroy a system tray icon.
 *
 * @param handle  Tray handle.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_tray, tray_destroy)
wapi_result_t wapi_tray_destroy(wapi_handle_t handle);

/**
 * Update the tray icon image.
 *
 * @param handle     Tray handle.
 * @param icon_data  Pointer to new icon image data.
 * @param icon_len   Size of icon data in bytes.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_tray, tray_set_icon)
wapi_result_t wapi_tray_set_icon(wapi_handle_t handle,
                                 const void* icon_data,
                                 wapi_size_t icon_len);

/**
 * Update the tray tooltip text.
 *
 * @param handle    Tray handle.
 * @param text      Tooltip string.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_tray, tray_set_tooltip)
wapi_result_t wapi_tray_set_tooltip(wapi_handle_t handle,
                                    wapi_stringview_t text);

/**
 * Set the context menu for the tray icon.
 *
 * @param handle      Tray handle.
 * @param items_ptr   Pointer to array of wapi_tray_menu_item_t.
 * @param item_count  Number of items in the array.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_tray, tray_set_menu)
wapi_result_t wapi_tray_set_menu(wapi_handle_t handle,
                                 const wapi_tray_menu_item_t* items_ptr,
                                 uint32_t item_count);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_TRAY_H */
