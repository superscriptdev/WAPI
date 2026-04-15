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

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Font Weight
 * ============================================================
 * CSS-style integer weight, 1..1000. Named stops are provided
 * for convenience; any integer in the range is valid input. For
 * variable-weight families (is_variable == true), hosts
 * interpolate between axis stops; for static families, hosts
 * snap to the nearest available face.
 */

typedef enum wapi_font_weight_t {
    WAPI_FONT_WEIGHT_THIN       = 100,
    WAPI_FONT_WEIGHT_LIGHT      = 300,
    WAPI_FONT_WEIGHT_NORMAL     = 400,
    WAPI_FONT_WEIGHT_MEDIUM     = 500,
    WAPI_FONT_WEIGHT_SEMIBOLD   = 600,
    WAPI_FONT_WEIGHT_BOLD       = 700,
    WAPI_FONT_WEIGHT_EXTRABOLD  = 800,
    WAPI_FONT_WEIGHT_BLACK      = 900,
    WAPI_FONT_WEIGHT_FORCE32    = 0x7FFFFFFF
} wapi_font_weight_t;

/* ============================================================
 * Font Style
 * ============================================================
 * A single style selection. For capability advertisement (which
 * styles a family *offers*) use a bitmask where bit N corresponds
 * to the enum value N -- see WAPI_FONT_STYLE_BIT() and the
 * `supported_styles` field of wapi_font_info_t.
 */

typedef enum wapi_font_style_t {
    WAPI_FONT_STYLE_NORMAL  = 0,
    WAPI_FONT_STYLE_ITALIC  = 1,
    WAPI_FONT_STYLE_OBLIQUE = 2,
    WAPI_FONT_STYLE_FORCE32 = 0x7FFFFFFF
} wapi_font_style_t;

/* Bit for a style in a supported-styles bitmask. */
#define WAPI_FONT_STYLE_BIT(s)          (1u << (uint32_t)(s))
#define WAPI_FONT_STYLE_NORMAL_BIT      WAPI_FONT_STYLE_BIT(WAPI_FONT_STYLE_NORMAL)
#define WAPI_FONT_STYLE_ITALIC_BIT      WAPI_FONT_STYLE_BIT(WAPI_FONT_STYLE_ITALIC)
#define WAPI_FONT_STYLE_OBLIQUE_BIT     WAPI_FONT_STYLE_BIT(WAPI_FONT_STYLE_OBLIQUE)

/* ============================================================
 * Font Family Info
 * ============================================================
 * Describes a font family's capabilities.
 *
 * Layout (32 bytes, align 8):
 *   Offset  0: wapi_stringview_t family  Font family name (UTF-8)
 *   Offset 16: uint32_t weight_min       Minimum weight, wapi_font_weight_t
 *   Offset 20: uint32_t weight_max       Maximum weight, wapi_font_weight_t
 *   Offset 24: uint32_t supported_styles Bitmask: WAPI_FONT_STYLE_*_BIT
 *   Offset 28: int32_t  is_variable      Variable font? (wapi_bool_t)
 */

typedef struct wapi_font_info_t {
    wapi_stringview_t family;
    uint32_t          weight_min;        /* wapi_font_weight_t */
    uint32_t          weight_max;        /* wapi_font_weight_t */
    uint32_t          supported_styles;  /* Bitmask of WAPI_FONT_STYLE_*_BIT */
    wapi_bool_t       is_variable;       /* Variable font? */
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
 * @return 1 if supported, 0 if not.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_font, supports_script)
wapi_bool_t wapi_font_supports_script(wapi_stringview_t script_tag);

/**
 * Check whether a font family supports an OpenType feature.
 * The feature tag is a 4-byte tag packed as a uint32 in
 * big-endian order (e.g., 'kern' = 0x6B65726E).
 *
 * @param family        Font family name (UTF-8).
 * @param opentype_tag  OpenType feature tag (4-byte packed uint32).
 * @return 1 if supported, 0 if not.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_font, has_feature)
wapi_bool_t wapi_font_has_feature(wapi_stringview_t family,
                               uint32_t opentype_tag);

/* ============================================================
 * Raw Font Bytes
 * ============================================================
 *
 * Fetch the raw container bytes (TTF, OTF, WOFF, WOFF2, TTC, ...)
 * backing a system font face. The contents are opaque to WAPI —
 * the caller is responsible for parsing them with its own text
 * shaping / glyph rasterization stack. Returning the container
 * directly keeps the API free of format assumptions: whatever the
 * host font system hands us, we forward untouched.
 *
 * This is an async I/O op because the browser's font-access API
 * (queryLocalFonts + FontData.blob()) is Promise-based and native
 * hosts may need to mmap or decompress a WOFF2 container. Both
 * paths fit the unified wapi_io completion model naturally.
 *
 * Operation: WAPI_IO_OP_FONT_BYTES_GET
 *   addr/len   = family name (UTF-8, no NUL)
 *   addr2/len2 = caller-owned output buffer
 *   flags      = requested weight (wapi_font_weight_t, or any
 *                integer in 1..1000), or 0 for the family's default
 *   flags2     = requested style (wapi_font_style_t)
 *   completion.result = bytes written, or negative error
 *                       (truncation if the face doesn't fit in len2)
 *
 * Callers that don't know the face size can call twice: once with a
 * zero-capacity buffer to probe the size (completion.result is the
 * required size as a negative "overflow" error), then again with a
 * properly sized buffer. Batch this like any other io op. */

static inline int32_t wapi_font_get_bytes(const wapi_io_t*   io,
                                          const void*        family_name,
                                          uint32_t           family_name_len,
                                          void*              output_buffer,
                                          uint32_t           output_capacity,
                                          wapi_font_weight_t weight,
                                          wapi_font_style_t  style,
                                          uint64_t           user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_FONT_BYTES_GET;
    op.flags     = (uint32_t)weight;
    op.flags2    = (uint32_t)style;
    op.addr      = (uint64_t)(uintptr_t)family_name;
    op.len       = (uint64_t)family_name_len;
    op.addr2     = (uint64_t)(uintptr_t)output_buffer;
    op.len2      = (uint64_t)output_capacity;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/* ============================================================
 * Fallback Chain
 * ============================================================ */

/**
 * Get the number of fallback fonts for a given family.
 *
 * @param family      Font family name (UTF-8).
 * @return Number of fallback fonts, or negative on error.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_font, fallback_count)
int32_t wapi_font_fallback_count(wapi_stringview_t family);

/**
 * Get the name of a fallback font by index.
 *
 * @param family      Font family name (UTF-8).
 * @param index       Fallback index (0 .. fallback_count-1).
 * @param buf         Buffer for fallback font name (UTF-8).
 * @param buf_len     Buffer capacity.
 * @param name_len    [out] Actual name length.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_font, fallback_get)
wapi_result_t wapi_font_fallback_get(wapi_stringview_t family,
                                  uint32_t index, char* buf,
                                  wapi_size_t buf_len, wapi_size_t* name_len);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_FONT_H */
