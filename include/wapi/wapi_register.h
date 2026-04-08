/**
 * WAPI - App Registration Capability
 * Version 1.0.0
 *
 * URL scheme handling, file type associations, and file preview providers.
 *
 * Maps to: CFBundleURLTypes / LSItemContentTypes (macOS/iOS),
 *          Intent Filters (Android), Registry file associations (Windows),
 *          Web App Manifest file_handlers / protocol_handlers
 *
 * Import module: "wapi_register"
 *
 * Query availability with wapi_capability_supported("wapi.register", 12)
 */

#ifndef WAPI_REGISTER_H
#define WAPI_REGISTER_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Event Types (0x1100-0x11FF)
 * ============================================================ */

#define WAPI_EVENT_URL_OPEN     0x1100  /* App opened via URL scheme */
#define WAPI_EVENT_FILE_OPEN    0x1101  /* App opened to handle a file */

/* ============================================================
 * Open Event
 * ============================================================
 * Delivered when the app is activated via a registered URL scheme
 * or file type association.
 *
 * Layout (40 bytes, align 8):
 *   Offset  0: uint32_t type
 *   Offset  4: uint32_t surface_id
 *   Offset  8: uint64_t timestamp              (8 bytes)
 *   Offset 16: wapi_string_view_t url          (16 bytes, URL or file path)
 *   Offset 32: uint32_t _reserved
 *   Offset 36: uint32_t _pad
 */
typedef struct wapi_open_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    wapi_string_view_t url;  /* URL or file path */
    uint32_t    _reserved;
    uint32_t    _pad;
} wapi_open_event_t;

/* ============================================================
 * File Type Descriptor
 * ============================================================
 *
 * Layout (64 bytes, align 8):
 *   Offset  0: wapi_string_view_t extension    (16 bytes)
 *   Offset 16: wapi_string_view_t mime_type    (16 bytes)
 *   Offset 32: wapi_string_view_t description  (16 bytes)
 *   Offset 48: wapi_string_view_t icon_path    (16 bytes)
 */
typedef struct wapi_filetype_desc_t {
    wapi_string_view_t extension;    /* e.g., ".xyz" */
    wapi_string_view_t mime_type;    /* e.g., "application/x-xyz" */
    wapi_string_view_t description;  /* e.g., "XYZ Document" */
    wapi_string_view_t icon_path;    /* Optional icon path (0 for none) */
} wapi_filetype_desc_t;

_Static_assert(sizeof(wapi_filetype_desc_t) == 64,
               "wapi_filetype_desc_t must be 64 bytes");

/* ============================================================
 * URL Scheme Registration
 * ============================================================ */

/**
 * Register a URL scheme handler (e.g., "myapp://").
 *
 * @param scheme      Scheme string (UTF-8, without "://").
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_register, url_scheme)
wapi_result_t wapi_register_url_scheme(wapi_string_view_t scheme);

/**
 * Unregister a previously registered URL scheme.
 *
 * @param scheme      Scheme string (UTF-8).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_register, unregister_url_scheme)
wapi_result_t wapi_register_unregister_url_scheme(wapi_string_view_t scheme);

/* ============================================================
 * File Type Registration
 * ============================================================ */

/**
 * Register a file type association.
 *
 * @param desc  File type descriptor.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_register, file_type)
wapi_result_t wapi_register_file_type(const wapi_filetype_desc_t* desc);

/**
 * Unregister a file type association.
 *
 * @param extension  File extension (e.g., ".xyz").
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_register, unregister_file_type)
wapi_result_t wapi_register_unregister_file_type(wapi_string_view_t extension);

/* ============================================================
 * File Preview Provider
 * ============================================================ */

/**
 * Register as a file preview provider (Finder QuickLook, Windows Preview Handler).
 *
 * @param extension  File extension to provide previews for.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_register, preview_provider)
wapi_result_t wapi_register_preview_provider(wapi_string_view_t extension);

/* ============================================================
 * Default Handler Queries
 * ============================================================ */

/**
 * Check if this app is the default handler for a URL scheme.
 *
 * @param scheme      Scheme string (UTF-8).
 * @return 1 if default, 0 if not.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_register, is_default_for_scheme)
wapi_bool_t wapi_register_is_default_for_scheme(wapi_string_view_t scheme);

/**
 * Check if this app is the default handler for a file type.
 *
 * @param extension  File extension (e.g., ".xyz").
 * @return 1 if default, 0 if not.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_register, is_default_for_type)
wapi_bool_t wapi_register_is_default_for_type(wapi_string_view_t extension);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_REGISTER_H */
