/**
 * WAPI - Clipboard
 * Version 1.0.0
 *
 * System clipboard access for copy/paste operations.
 *
 * Formats are MIME-type strings ("text/plain", "image/png", etc.).
 * Well-known format constants are provided for convenience.
 * Applications may use arbitrary MIME types for custom formats.
 *
 * The clipboard holds multiple representations of the same content.
 * The writer provides several formats atomically via wapi_clipboard_set;
 * the reader enumerates available formats and picks the best one.
 *
 * Import module: "wapi_clipboard"
 */

#ifndef WAPI_CLIPBOARD_H
#define WAPI_CLIPBOARD_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Well-Known Format Strings
 * ============================================================ */

#define WAPI_CLIPBOARD_TEXT_PLAIN  "text/plain"
#define WAPI_CLIPBOARD_TEXT_HTML   "text/html"
#define WAPI_CLIPBOARD_TEXT_URI    "text/uri-list"
#define WAPI_CLIPBOARD_IMAGE_PNG   "image/png"
#define WAPI_CLIPBOARD_IMAGE_JPEG  "image/jpeg"
#define WAPI_CLIPBOARD_IMAGE_SVG   "image/svg+xml"

/* ============================================================
 * Clipboard Item (one representation in a multi-format set)
 * ============================================================ */

typedef struct wapi_clipboard_item_t {
    wapi_string_view_t mime;    /* Offset  0: MIME type string (16 bytes) */
    uint64_t        data;       /* Offset 16: Linear memory address of data */
    wapi_size_t     data_len;   /* Offset 24: Data length in bytes */
    uint32_t        _pad;       /* Offset 28: (alignment padding) */
} wapi_clipboard_item_t;        /* 32 bytes, align 8 */

_Static_assert(sizeof(wapi_clipboard_item_t) == 32,
               "wapi_clipboard_item_t must be 32 bytes");

/* ============================================================
 * Enumerate Available Formats
 * ============================================================ */

/**
 * Get the number of formats currently on the clipboard.
 *
 * @return Number of available formats, or 0 if clipboard is empty.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_clipboard, format_count)
wapi_size_t wapi_clipboard_format_count(void);

/**
 * Get the MIME type string of a format by index.
 *
 * @param index     Format index (0 .. format_count-1).
 * @param buf       Buffer to receive MIME string (not NUL-terminated).
 * @param buf_len   Buffer capacity.
 * @param out_len   [out] Actual length of the MIME string.
 * @return WAPI_OK on success, WAPI_ERR_RANGE if index out of bounds.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_clipboard, format_name)
wapi_result_t wapi_clipboard_format_name(wapi_size_t index, char* buf,
                                         wapi_size_t buf_len,
                                         wapi_size_t* out_len);

/* ============================================================
 * Read
 * ============================================================ */

/**
 * Check if the clipboard contains data in a given MIME type.
 *
 * @param mime      MIME type string.
 * @return 1 if data is available, 0 if not.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_clipboard, has_format)
wapi_bool_t wapi_clipboard_has_format(wapi_string_view_t mime);

/**
 * Read data from the clipboard in a specific format.
 *
 * @param mime          MIME type string.
 * @param buf           Buffer to receive data.
 * @param buf_len       Buffer capacity.
 * @param bytes_written [out] Actual bytes written.
 * @return WAPI_OK on success, WAPI_ERR_NOENT if format not available.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_clipboard, read)
wapi_result_t wapi_clipboard_read(wapi_string_view_t mime,
                                  void* buf, wapi_size_t buf_len,
                                  wapi_size_t* bytes_written);

/* ============================================================
 * Write
 * ============================================================ */

/**
 * Set the clipboard contents, replacing everything.
 * Provide multiple representations of the same content so readers
 * can pick the richest format they understand.
 *
 * @param items  Array of clipboard items (format + data pairs).
 * @param count  Number of items.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_clipboard, set)
wapi_result_t wapi_clipboard_set(const wapi_clipboard_item_t* items,
                                 wapi_size_t count);

/**
 * Clear all clipboard contents.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_clipboard, clear)
wapi_result_t wapi_clipboard_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CLIPBOARD_H */
