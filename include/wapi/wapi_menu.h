/**
 * WAPI - Menu
 * Version 1.0.0
 *
 * Native context menus and window menu bars.
 *
 * Import module: "wapi_menu"
 */

#ifndef WAPI_MENU_H
#define WAPI_MENU_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Menu Item Flags
 * ============================================================ */

#define WAPI_MENU_SEPARATOR  0x1
#define WAPI_MENU_DISABLED   0x2
#define WAPI_MENU_CHECKED    0x4
#define WAPI_MENU_SUBMENU    0x8

/* ============================================================
 * Menu Types
 * ============================================================ */

/**
 * Menu item descriptor.
 *
 * Layout (48 bytes, align 8):
 *   Offset  0: uint32_t     id
 *   Offset  4: uint32_t     label_len
 *   Offset  8: uint64_t     label_ptr      Linear memory address of label string
 *   Offset 16: uint32_t     shortcut_len
 *   Offset 20: uint32_t     icon_len
 *   Offset 24: uint64_t     shortcut_ptr   Linear memory address of shortcut string
 *   Offset 32: uint64_t     icon_ptr       Linear memory address of icon data
 *   Offset 40: wapi_flags_t flags
 */
typedef struct wapi_menu_item_t {
    uint32_t     id;
    uint32_t     label_len;
    uint64_t     label_ptr;
    uint32_t     shortcut_len;
    uint32_t     icon_len;
    uint64_t     shortcut_ptr;
    uint64_t     icon_ptr;
    wapi_flags_t flags;
} wapi_menu_item_t;

_Static_assert(sizeof(wapi_menu_item_t) == 48,
               "wapi_menu_item_t must be 48 bytes");
_Static_assert(_Alignof(wapi_menu_item_t) == 8,
               "wapi_menu_item_t must be 8-byte aligned");

/* ============================================================
 * Menu Functions
 * ============================================================ */

/**
 * Create an empty menu.
 *
 * @param out_handle  [out] Receives the menu handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_menu, menu_create)
wapi_result_t wapi_menu_create(wapi_handle_t* out_handle);

/**
 * Add an item to a menu.
 *
 * @param menu      Menu handle.
 * @param item_ptr  Pointer to a wapi_menu_item_t descriptor.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_menu, menu_add_item)
wapi_result_t wapi_menu_add_item(wapi_handle_t menu,
                                 const wapi_menu_item_t* item_ptr);

/**
 * Add a submenu to a menu.
 *
 * @param menu       Parent menu handle.
 * @param label_ptr  Submenu label string.
 * @param submenu    Submenu handle.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_menu, menu_add_submenu)
wapi_result_t wapi_menu_add_submenu(wapi_handle_t menu,
                                    wapi_stringview_t label_ptr,
                                    wapi_handle_t submenu);

/**
 * Show a context menu at a position on a surface.
 *
 * @param menu     Menu handle.
 * @param surface  Surface handle where the menu should appear.
 * @param x        X coordinate in surface-local pixels.
 * @param y        Y coordinate in surface-local pixels.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_menu, menu_show_context)
wapi_result_t wapi_menu_show_context(wapi_handle_t menu,
                                     wapi_handle_t surface,
                                     int32_t x, int32_t y);

/**
 * Set a menu as the window menu bar for a surface.
 *
 * @param surface  Surface handle.
 * @param menu     Menu handle.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_menu, menu_set_bar)
wapi_result_t wapi_menu_set_bar(wapi_handle_t surface,
                                wapi_handle_t menu);

/**
 * Destroy a menu and free its resources.
 *
 * @param menu  Menu handle.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_menu, menu_destroy)
wapi_result_t wapi_menu_destroy(wapi_handle_t menu);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_MENU_H */
