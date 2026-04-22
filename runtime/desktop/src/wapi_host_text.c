/**
 * WAPI Desktop Runtime - Text (Shaping + Layout)
 *
 * Implements all wapi_text.* imports.
 *
 * TODO: implement shaping with DirectWrite (Win), CoreText (macOS),
 *       HarfBuzz (Linux)
 * TODO: implement layout with DirectWrite/CoreText/Pango
 *
 * Currently stubbed: all operations return WAPI_ERR_NOTSUP or
 * WAPI_HANDLE_INVALID so modules can be linked and tested.
 */

#include "wapi_host.h"

/* ============================================================
 * Shaping: shape
 * (i32 font_ptr, i32 text_sv_ptr, i32 script, i32 dir) -> i32
 * (stringview is 16B, passed by pointer per the wasm32 ABI) */
static wasm_trap_t* cb_shape(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    /* TODO: parse font_desc, shape text, return result handle */
    WAPI_RET_I32(WAPI_HANDLE_INVALID);
    return NULL;
}

/* ============================================================
 * Shaping: shape_glyph_count
 * (i32 result) -> i32
 * ============================================================ */
static wasm_trap_t* cb_shape_glyph_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(0);
    return NULL;
}

/* ============================================================
 * Shaping: shape_get_glyphs
 * (i32 result, i32 infos_ptr, i32 positions_ptr) -> i32
 * ============================================================ */
static wasm_trap_t* cb_shape_get_glyphs(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Shaping: shape_get_font_metrics
 * (i32 result, i32 metrics_ptr) -> i32
 * ============================================================ */
static wasm_trap_t* cb_shape_get_font_metrics(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Shaping: shape_destroy
 * (i32 result) -> i32
 * ============================================================ */
static wasm_trap_t* cb_shape_destroy(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Layout: layout_create
 * (i32 text_ptr, i32 constraints_ptr) -> i32
 * ============================================================ */
static wasm_trap_t* cb_layout_create(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_HANDLE_INVALID);
    return NULL;
}

/* ============================================================
 * Layout: layout_get_size
 * (i32 layout, i32 width_ptr, i32 height_ptr) -> i32
 * ============================================================ */
static wasm_trap_t* cb_layout_get_size(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Layout: layout_line_count
 * (i32 layout) -> i32
 * ============================================================ */
static wasm_trap_t* cb_layout_line_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(0);
    return NULL;
}

/* ============================================================
 * Layout: layout_get_line_info
 * (i32 layout, i32 line_idx, i32 info_ptr) -> i32
 * ============================================================ */
static wasm_trap_t* cb_layout_get_line_info(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Layout: layout_hit_test
 * (i32 layout, f32 x, f32 y, i32 result_ptr) -> i32
 * ============================================================ */
static wasm_trap_t* cb_layout_hit_test(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Layout: layout_get_caret
 * (i32 layout, i32 char_offset, i32 info_ptr) -> i32
 * ============================================================ */
static wasm_trap_t* cb_layout_get_caret(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Layout: layout_update_text
 * (i32 layout, i32 text_ptr) -> i32
 * ============================================================ */
static wasm_trap_t* cb_layout_update_text(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Layout: layout_update_constraints
 * (i32 layout, i32 constraints_ptr) -> i32
 * ============================================================ */
static wasm_trap_t* cb_layout_update_constraints(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Layout: layout_destroy
 * (i32 layout) -> i32
 * ============================================================ */
static wasm_trap_t* cb_layout_destroy(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_text(wasmtime_linker_t* linker) {
    /* Shaping */
    WAPI_DEFINE_4_1(linker, "wapi_text", "shape",                 cb_shape);
    WAPI_DEFINE_1_1(linker, "wapi_text", "shape_glyph_count",     cb_shape_glyph_count);
    WAPI_DEFINE_3_1(linker, "wapi_text", "shape_get_glyphs",      cb_shape_get_glyphs);
    WAPI_DEFINE_2_1(linker, "wapi_text", "shape_get_font_metrics", cb_shape_get_font_metrics);
    WAPI_DEFINE_1_1(linker, "wapi_text", "shape_destroy",         cb_shape_destroy);

    /* Layout */
    WAPI_DEFINE_2_1(linker, "wapi_text", "layout_create",         cb_layout_create);
    WAPI_DEFINE_3_1(linker, "wapi_text", "layout_get_size",       cb_layout_get_size);
    WAPI_DEFINE_1_1(linker, "wapi_text", "layout_line_count",     cb_layout_line_count);
    WAPI_DEFINE_3_1(linker, "wapi_text", "layout_get_line_info",  cb_layout_get_line_info);

    /* layout_hit_test: (i32, f32, f32, i32) -> i32 -- mixed types */
    wapi_linker_define(linker, "wapi_text", "layout_hit_test", cb_layout_hit_test,
        4, (wasm_valkind_t[]){WASM_I32, WASM_F32, WASM_F32, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_3_1(linker, "wapi_text", "layout_get_caret",           cb_layout_get_caret);
    WAPI_DEFINE_2_1(linker, "wapi_text", "layout_update_text",         cb_layout_update_text);
    WAPI_DEFINE_2_1(linker, "wapi_text", "layout_update_constraints",  cb_layout_update_constraints);
    WAPI_DEFINE_1_1(linker, "wapi_text", "layout_destroy",             cb_layout_destroy);
}
