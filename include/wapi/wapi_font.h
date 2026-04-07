/**
 * WAPI - Font System Queries
 * Version 1.0.0
 *
 * Query the host's font system: enumerate families, inspect
 * weight/style ranges, check script support, and resolve
 * fallback chains.
 *
 * This is a read-only discovery API. The module asks what fonts
 * are available; actual text shaping and rendering happen through
 * wapi_content or custom GPU rendering.
 *
 * Import module: "wapi_font"
 */

#ifndef WAPI_FONT_H
#define WAPI_FONT_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Font Style Flags
 * ============================================================ */

#define WAPI_FONT_HAS_NORMAL  0x0001
#define WAPI_FONT_HAS_ITALIC  0x0002
#define WAPI_FONT_HAS_OBLIQUE 0x0004

/* ============================================================
 * Font Family Info
 * ============================================================
 * Describes a font family's capabilities.
 *
 * Layout (24 bytes, align 4):
 *   Offset  0: ptr      family         Font family name (UTF-8)
 *   Offset  4: uint32_t family_len     Byte length of name
 *   Offset  8: uint32_t weight_min     Minimum weight (e.g., 100)
 *   Offset 12: uint32_t weight_max     Maximum weight (e.g., 900)
 *   Offset 16: uint32_t style_flags    Bitmask of WAPI_FONT_HAS_*
 *   Offset 20: int32_t  is_variable    Variable font? (wapi_bool_t)
 */

typedef struct wapi_font_info_t {
    const char* family;       /* Font family name */
    wapi_size_t   family_len;
    uint32_t    weight_min;   /* Minimum weight (e.g., 100) */
    uint32_t    weight_max;   /* Maximum weight (e.g., 900) */
    uint32_t    style_flags;  /* Bitmask: WAPI_FONT_HAS_NORMAL | WAPI_FONT_HAS_ITALIC | WAPI_FONT_HAS_OBLIQUE */
    wapi_bool_t   is_variable;  /* Variable font? */
} wapi_font_info_t;

/* ============================================================
 * Font Enumeration
 * ============================================================ */

/**
 * Get the number of available font families.
 *
 * @return Number of font families, or negative on error.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_font, family_count)
int32_t wapi_font_family_count(void);

/**
 * Get information about a font family by index.
 *
 * @param index  Family index (0 .. family_count-1).
 * @param info   [out] Font family information.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_font, family_info)
wapi_result_t wapi_font_family_info(uint32_t index, wapi_font_info_t* info);

/* ============================================================
 * Script and Feature Queries
 * ============================================================ */

/**
 * Check whether the host's font system supports a given script.
 * Uses ISO 15924 four-letter script tags (e.g., "Latn", "Arab",
 * "Hans", "Deva").
 *
 * @param script_tag  ISO 15924 script tag (UTF-8).
 * @param tag_len     Length of the script tag in bytes.
 * @return 1 if supported, 0 if not.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_font, supports_script)
wapi_bool_t wapi_font_supports_script(const char* script_tag, wapi_size_t tag_len);

/**
 * Check whether a font family supports an OpenType feature.
 * The feature tag is a 4-byte tag packed as a uint32 in
 * big-endian order (e.g., 'kern' = 0x6B65726E).
 *
 * @param family        Font family name (UTF-8).
 * @param family_len    Family name length in bytes.
 * @param opentype_tag  OpenType feature tag (4-byte packed uint32).
 * @return 1 if supported, 0 if not.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_font, has_feature)
wapi_bool_t wapi_font_has_feature(const char* family, wapi_size_t family_len,
                               uint32_t opentype_tag);

/* ============================================================
 * Fallback Chain
 * ============================================================ */

/**
 * Get the number of fallback fonts for a given family.
 *
 * @param family      Font family name (UTF-8).
 * @param family_len  Family name length in bytes.
 * @return Number of fallback fonts, or negative on error.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_font, fallback_count)
int32_t wapi_font_fallback_count(const char* family, wapi_size_t family_len);

/**
 * Get the name of a fallback font by index.
 *
 * @param family      Font family name (UTF-8).
 * @param family_len  Family name length in bytes.
 * @param index       Fallback index (0 .. fallback_count-1).
 * @param buf         Buffer for fallback font name (UTF-8).
 * @param buf_len     Buffer capacity.
 * @param name_len    [out] Actual name length.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_font, fallback_get)
wapi_result_t wapi_font_fallback_get(const char* family, wapi_size_t family_len,
                                  uint32_t index, char* buf,
                                  wapi_size_t buf_len, wapi_size_t* name_len);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_FONT_H */
