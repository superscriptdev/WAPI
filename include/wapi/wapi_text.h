/**
 * WAPI - Text
 * Version 1.0.0
 *
 * All text-related types and operations: declaration, shaping,
 * and layout. Single import module for the complete text pipeline.
 *
 * Shaping: UTF-8 text + font spec -> glyph runs (IDs, positions,
 *   advances, cluster mapping). Uses host HarfBuzz/DirectWrite/CoreText.
 *
 * Layout: styled text runs + constraints -> positioned lines with
 *   line breaks, hit testing, and caret positioning.
 *
 * Import module: "wapi_text"
 */

#ifndef WAPI_TEXT_H
#define WAPI_TEXT_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Text Style Properties
 * ============================================================ */

typedef enum wapi_text_font_weight_t {
    WAPI_TEXT_FONT_WEIGHT_THIN       = 100,
    WAPI_TEXT_FONT_WEIGHT_LIGHT      = 300,
    WAPI_TEXT_FONT_WEIGHT_NORMAL     = 400,
    WAPI_TEXT_FONT_WEIGHT_MEDIUM     = 500,
    WAPI_TEXT_FONT_WEIGHT_SEMIBOLD   = 600,
    WAPI_TEXT_FONT_WEIGHT_BOLD       = 700,
    WAPI_TEXT_FONT_WEIGHT_EXTRABOLD  = 800,
    WAPI_TEXT_FONT_WEIGHT_BLACK      = 900,
    WAPI_TEXT_FONT_WEIGHT_FORCE32    = 0x7FFFFFFF
} wapi_text_font_weight_t;

typedef enum wapi_text_font_style_t {
    WAPI_TEXT_FONT_STYLE_NORMAL  = 0,
    WAPI_TEXT_FONT_STYLE_ITALIC  = 1,
    WAPI_TEXT_FONT_STYLE_OBLIQUE = 2,
    WAPI_TEXT_FONT_STYLE_FORCE32 = 0x7FFFFFFF
} wapi_text_font_style_t;

typedef enum wapi_text_align_t {
    WAPI_TEXT_ALIGN_LEFT    = 0,
    WAPI_TEXT_ALIGN_CENTER  = 1,
    WAPI_TEXT_ALIGN_RIGHT   = 2,
    WAPI_TEXT_ALIGN_JUSTIFY = 3,
    WAPI_TEXT_ALIGN_START   = 4,  /* Locale-dependent */
    WAPI_TEXT_ALIGN_END     = 5,
    WAPI_TEXT_ALIGN_FORCE32 = 0x7FFFFFFF
} wapi_text_align_t;

typedef enum wapi_text_direction_t {
    WAPI_TEXT_DIR_LTR     = 0,
    WAPI_TEXT_DIR_RTL     = 1,
    WAPI_TEXT_DIR_AUTO    = 2,  /* Detect from content */
    WAPI_TEXT_DIR_FORCE32 = 0x7FFFFFFF
} wapi_text_direction_t;

/* ============================================================
 * Text Style
 * ============================================================
 *
 * Layout (48 bytes, align 4):
 *   Offset  0: wapi_string_view_t font_family  (UTF-8, NULL = system default)
 *   Offset  8: float    font_size        (logical pixels)
 *   Offset 12: uint32_t font_weight      (wapi_text_font_weight_t)
 *   Offset 16: uint32_t font_style       (wapi_text_font_style_t)
 *   Offset 20: float    line_height      (multiplier, 0 = auto)
 *   Offset 24: float    letter_spacing   (pixels, 0 = normal)
 *   Offset 28: uint32_t color            (RGBA8: 0xRRGGBBAA)
 *   Offset 32: uint32_t text_align       (wapi_text_align_t)
 *   Offset 36: uint32_t text_direction   (wapi_text_direction_t)
 *   Offset 40: wapi_string_view_t font_fallback  (comma-separated families)
 */

typedef struct wapi_text_style_t {
    wapi_string_view_t font_family;
    float           font_size;
    uint32_t        font_weight;
    uint32_t        font_style;
    float           line_height;
    float           letter_spacing;
    uint32_t        color;
    uint32_t        text_align;
    uint32_t        text_direction;
    wapi_string_view_t font_fallback;
} wapi_text_style_t;

/* ============================================================
 * Text Run
 * ============================================================
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: wapi_string_view_t text  UTF-8 text data
 *   Offset  8: ptr      style       Pointer to wapi_text_style_t
 *   Offset 12: uint32_t _reserved
 */

typedef struct wapi_text_run_t {
    wapi_string_view_t       text;
    const wapi_text_style_t* style;
    uint32_t                 _reserved;
} wapi_text_run_t;

/* ============================================================
 * Text Descriptor
 * ============================================================
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: ptr      nextInChain
 *   Offset  4: ptr      runs        Array of wapi_text_run_t
 *   Offset  8: uint32_t run_count
 *   Offset 12: uint32_t _reserved
 */

typedef struct wapi_text_desc_t {
    wapi_chained_struct_t*   nextInChain;
    const wapi_text_run_t*   runs;
    uint32_t                 run_count;
    uint32_t                 _reserved;
} wapi_text_desc_t;

/* ============================================================
 * Text Shaping Types
 * ============================================================ */

/**
 * Font descriptor for shaping.
 * Specifies which font to shape with and optional OpenType features.
 *
 * Layout (28 bytes, align 4):
 *   Offset  0: wapi_string_view_t family
 *   Offset  8: float    size            (logical pixels)
 *   Offset 12: uint32_t weight          (wapi_text_font_weight_t)
 *   Offset 16: uint32_t style           (wapi_text_font_style_t)
 *   Offset 20: ptr      features        (OpenType tags, packed uint32)
 *   Offset 24: uint32_t feature_count
 */
typedef struct wapi_text_font_desc_t {
    wapi_string_view_t family;
    float           size;
    uint32_t        weight;
    uint32_t        style;
    const uint32_t* features;
    uint32_t        feature_count;
} wapi_text_font_desc_t;

/**
 * Per-glyph information from shaping.
 *
 * Layout (8 bytes, align 4):
 *   Offset 0: uint32_t glyph_id    Font-specific glyph index
 *   Offset 4: uint32_t cluster     UTF-8 byte offset of source cluster
 */
typedef struct wapi_text_glyph_info_t {
    uint32_t glyph_id;
    uint32_t cluster;
} wapi_text_glyph_info_t;

_Static_assert(sizeof(wapi_text_glyph_info_t) == 8,
    "wapi_text_glyph_info_t must be 8 bytes");

/**
 * Per-glyph positioning from shaping.
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: float x_advance
 *   Offset  4: float y_advance    (0 for horizontal text)
 *   Offset  8: float x_offset
 *   Offset 12: float y_offset
 */
typedef struct wapi_text_glyph_position_t {
    float x_advance;
    float y_advance;
    float x_offset;
    float y_offset;
} wapi_text_glyph_position_t;

_Static_assert(sizeof(wapi_text_glyph_position_t) == 16,
    "wapi_text_glyph_position_t must be 16 bytes");

/**
 * Font metrics at a given size.
 *
 * Layout (36 bytes, align 4):
 *   All values in logical pixels at the shaped font size.
 */
typedef struct wapi_text_font_metrics_t {
    float ascent;
    float descent;
    float line_gap;
    float cap_height;
    float x_height;
    float underline_offset;
    float underline_thickness;
    float strikeout_offset;
    float strikeout_thickness;
} wapi_text_font_metrics_t;

_Static_assert(sizeof(wapi_text_font_metrics_t) == 36,
    "wapi_text_font_metrics_t must be 36 bytes");

/* ============================================================
 * Text Shaping Functions
 * ============================================================ */

/**
 * Shape a run of text. Returns a result handle containing glyph data.
 * The host resolves the font, applies shaping (ligatures, kerning,
 * contextual alternates), and produces positioned glyphs.
 *
 * @param font       Font descriptor.
 * @param text       UTF-8 text to shape.
 * @param script     ISO 15924 script tag packed as uint32
 *                   (e.g., "Latn" = 0x4C61746E), 0 = auto-detect.
 * @param direction  Text direction (wapi_text_direction_t).
 * @return Result handle, or WAPI_HANDLE_INVALID on failure.
 *
 * Wasm signature: (i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_text, shape)
wapi_handle_t wapi_text_shape(const wapi_text_font_desc_t* font,
                              wapi_string_view_t text,
                              uint32_t script, uint32_t direction);

/**
 * Get the number of glyphs in a shaping result.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_text, shape_glyph_count)
uint32_t wapi_text_shape_glyph_count(wapi_handle_t result);

/**
 * Copy glyph data from a shaping result into app-allocated buffers.
 * Both arrays must have at least glyph_count elements.
 *
 * @param result     Result handle from wapi_text_shape.
 * @param infos      [out] Glyph info array.
 * @param positions  [out] Glyph position array.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_text, shape_get_glyphs)
wapi_result_t wapi_text_shape_get_glyphs(wapi_handle_t result,
                                         wapi_text_glyph_info_t* infos,
                                         wapi_text_glyph_position_t* positions);

/**
 * Get font metrics for the font resolved during shaping.
 * Metrics are scaled to the font size used in shaping.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_text, shape_get_font_metrics)
wapi_result_t wapi_text_shape_get_font_metrics(wapi_handle_t result,
                                               wapi_text_font_metrics_t* metrics);

/**
 * Destroy a shaping result and free host resources.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_text, shape_destroy)
wapi_result_t wapi_text_shape_destroy(wapi_handle_t result);

/* ============================================================
 * Text Layout Types
 * ============================================================ */

typedef enum wapi_text_wrap_mode_t {
    WAPI_TEXT_WRAP_WORD    = 0,   /* Break at word boundaries */
    WAPI_TEXT_WRAP_CHAR    = 1,   /* Break at any character */
    WAPI_TEXT_WRAP_NONE    = 2,   /* No wrapping (single line) */
    WAPI_TEXT_WRAP_FORCE32 = 0x7FFFFFFF
} wapi_text_wrap_mode_t;

typedef enum wapi_text_overflow_t {
    WAPI_TEXT_OVERFLOW_VISIBLE  = 0,   /* Content overflows bounds */
    WAPI_TEXT_OVERFLOW_CLIP     = 1,   /* Clip at constraint boundary */
    WAPI_TEXT_OVERFLOW_ELLIPSIS = 2,   /* Truncate with "..." */
    WAPI_TEXT_OVERFLOW_FORCE32  = 0x7FFFFFFF
} wapi_text_overflow_t;

/**
 * Layout constraints.
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: float    max_width    (0 = unconstrained)
 *   Offset  4: float    max_height   (0 = unconstrained)
 *   Offset  8: uint32_t wrap_mode    (wapi_text_wrap_mode_t)
 *   Offset 12: uint32_t overflow     (wapi_text_overflow_t)
 */
typedef struct wapi_text_layout_constraints_t {
    float    max_width;
    float    max_height;
    uint32_t wrap_mode;
    uint32_t overflow;
} wapi_text_layout_constraints_t;

_Static_assert(sizeof(wapi_text_layout_constraints_t) == 16,
    "wapi_text_layout_constraints_t must be 16 bytes");

/**
 * Per-line layout information.
 *
 * Layout (36 bytes, align 4):
 *   Offset  0: float    x_offset
 *   Offset  4: float    y_offset
 *   Offset  8: float    width
 *   Offset 12: float    height
 *   Offset 16: float    baseline
 *   Offset 20: float    ascent
 *   Offset 24: float    descent
 *   Offset 28: uint32_t start_offset   (UTF-8 byte offset)
 *   Offset 32: uint32_t end_offset     (UTF-8 byte offset)
 */
typedef struct wapi_text_line_info_t {
    float       x_offset;
    float       y_offset;
    float       width;
    float       height;
    float       baseline;
    float       ascent;
    float       descent;
    uint32_t    start_offset;
    uint32_t    end_offset;
} wapi_text_line_info_t;

_Static_assert(sizeof(wapi_text_line_info_t) == 36,
    "wapi_text_line_info_t must be 36 bytes");

/**
 * Hit test result.
 *
 * Layout (20 bytes, align 4):
 *   Offset  0: uint32_t  char_offset
 *   Offset  4: uint32_t  line_index
 *   Offset  8: float     char_x
 *   Offset 12: float     char_width
 *   Offset 16: int32_t   is_trailing
 */
typedef struct wapi_text_hit_test_result_t {
    uint32_t    char_offset;
    uint32_t    line_index;
    float       char_x;
    float       char_width;
    wapi_bool_t is_trailing;
} wapi_text_hit_test_result_t;

_Static_assert(sizeof(wapi_text_hit_test_result_t) == 20,
    "wapi_text_hit_test_result_t must be 20 bytes");

/**
 * Caret position information.
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: float    x
 *   Offset  4: float    y
 *   Offset  8: float    height
 *   Offset 12: uint32_t line_index
 */
typedef struct wapi_text_caret_info_t {
    float       x;
    float       y;
    float       height;
    uint32_t    line_index;
} wapi_text_caret_info_t;

_Static_assert(sizeof(wapi_text_caret_info_t) == 16,
    "wapi_text_caret_info_t must be 16 bytes");

/* ============================================================
 * Text Layout Functions
 * ============================================================ */

/**
 * Create a text layout from styled text runs and constraints.
 * The host shapes the text internally and computes line breaks.
 *
 * @param text         Text descriptor (runs of styled text).
 * @param constraints  Layout constraints.
 * @return Layout handle, or WAPI_HANDLE_INVALID on failure.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_text, layout_create)
wapi_handle_t wapi_text_layout_create(const wapi_text_desc_t* text,
                                      const wapi_text_layout_constraints_t* constraints);

/**
 * Get the computed dimensions of the layout.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_text, layout_get_size)
wapi_result_t wapi_text_layout_get_size(wapi_handle_t layout,
                                        float* width, float* height);

/**
 * Get the number of lines in the layout.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_text, layout_line_count)
uint32_t wapi_text_layout_line_count(wapi_handle_t layout);

/**
 * Get layout information for a specific line.
 *
 * @return WAPI_OK, or WAPI_ERR_RANGE if line_index out of bounds.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_text, layout_get_line_info)
wapi_result_t wapi_text_layout_get_line_info(wapi_handle_t layout,
                                             uint32_t line_index,
                                             wapi_text_line_info_t* info);

/**
 * Hit test: find the character at a pixel position.
 * Coordinates are relative to the layout origin (0,0 = top-left).
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_text, layout_hit_test)
wapi_result_t wapi_text_layout_hit_test(wapi_handle_t layout,
                                        float x, float y,
                                        wapi_text_hit_test_result_t* result);

/**
 * Get caret position for a character offset.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_text, layout_get_caret)
wapi_result_t wapi_text_layout_get_caret(wapi_handle_t layout,
                                         uint32_t char_offset,
                                         wapi_text_caret_info_t* info);

/**
 * Update the text in an existing layout (reshapes and reflows).
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_text, layout_update_text)
wapi_result_t wapi_text_layout_update_text(wapi_handle_t layout,
                                           const wapi_text_desc_t* text);

/**
 * Update layout constraints (reflows without reshaping if text unchanged).
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_text, layout_update_constraints)
wapi_result_t wapi_text_layout_update_constraints(
    wapi_handle_t layout,
    const wapi_text_layout_constraints_t* constraints);

/**
 * Destroy a layout and free host resources.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_text, layout_destroy)
wapi_result_t wapi_text_layout_destroy(wapi_handle_t layout);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_TEXT_H */
