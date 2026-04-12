/**
 * WAPI - Drag and Drop Capability
 * Version 1.0.0
 *
 * Initiate and handle drag-and-drop operations.
 *
 * Maps to: HTML Drag and Drop API (Web), NSPasteboard/UIDragInteraction (macOS/iOS),
 *          OLE Drag and Drop (Windows), GDK DnD (Linux)
 *
 * Import module: "wapi_dnd"
 *
 * Query availability with wapi_capability_supported("wapi.dnd", 7)
 */

#ifndef WAPI_DND_H
#define WAPI_DND_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Drag and Drop Events
 * ============================================================ */

#define WAPI_EVENT_DND_ENTER  0x1600
#define WAPI_EVENT_DND_OVER   0x1601
#define WAPI_EVENT_DND_DROP   0x1602
#define WAPI_EVENT_DND_LEAVE  0x1603

/* ============================================================
 * Drag and Drop Types
 * ============================================================ */

typedef enum wapi_dnd_effect_t {
    WAPI_DND_NONE    = 0,
    WAPI_DND_COPY    = 1,
    WAPI_DND_MOVE    = 2,
    WAPI_DND_LINK    = 3,
    WAPI_DND_FORCE32 = 0x7FFFFFFF
} wapi_dnd_effect_t;

/**
 * Drag and drop data item.
 *
 * Layout (24 bytes, align 8):
 *   Offset  0: uint32_t type_len
 *   Offset  4: uint32_t data_len
 *   Offset  8: uint64_t type_ptr   Linear memory address of MIME type string
 *   Offset 16: uint64_t data_ptr   Linear memory address of item data
 */
typedef struct wapi_dnd_item_t {
    uint32_t type_len;
    uint32_t data_len;
    uint64_t type_ptr;
    uint64_t data_ptr;
} wapi_dnd_item_t;

_Static_assert(sizeof(wapi_dnd_item_t) == 24,
               "wapi_dnd_item_t must be 24 bytes");
_Static_assert(_Alignof(wapi_dnd_item_t) == 8,
               "wapi_dnd_item_t must be 8-byte aligned");

/* ============================================================
 * Drag and Drop Functions
 * ============================================================ */

/**
 * Initiate a drag operation.
 *
 * @param items            Pointer to array of drag items.
 * @param item_count       Number of items.
 * @param allowed_effects  Bitmask of allowed wapi_dnd_effect_t values.
 * @param icon_surface     Surface handle for drag icon (WAPI_HANDLE_INVALID for default).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dnd, start_drag)
wapi_result_t wapi_dnd_start_drag(const wapi_dnd_item_t* items,
                                  uint32_t item_count,
                                  uint32_t allowed_effects,
                                  wapi_handle_t icon_surface);

/**
 * Set the drop effect for the current drag-over operation.
 *
 * Call this in response to a WAPI_EVENT_DND_OVER event.
 *
 * @param effect  The desired drop effect.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_dnd, set_drop_effect)
wapi_result_t wapi_dnd_set_drop_effect(wapi_dnd_effect_t effect);

/**
 * Retrieve data from a dropped item.
 *
 * @param index          Item index in the drop payload.
 * @param buf            Buffer to receive the item data.
 * @param buf_len        Size of the buffer.
 * @param bytes_written  [out] Actual number of bytes written.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dnd, get_drop_data)
wapi_result_t wapi_dnd_get_drop_data(uint32_t index, void* buf,
                                     wapi_size_t buf_len,
                                     wapi_size_t* bytes_written);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_DND_H */
