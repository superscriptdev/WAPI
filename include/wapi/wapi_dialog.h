/**
 * WAPI - System Dialogs
 * Version 1.0.0
 *
 * Native file open/save/folder dialogs, message boxes, and the
 * OS-provided color and font pickers. Dialogs are modal and are
 * submitted via the I/O vtable. The vtable implementation determines
 * whether they block or return immediately.
 *
 * Shaped after SDL3 SDL_dialog.h + SDL_messagebox.h.
 *
 * Maps to: <input type="file"> / window.confirm / <input type="color"> (Web),
 *          NSOpenPanel / NSAlert / NSColorPanel / NSFontPanel (macOS),
 *          IFileOpenDialog / MessageBox / ChooseColor / ChooseFont (Windows),
 *          GTK file chooser / dialog / GtkColorChooser / GtkFontChooser (Linux),
 *          SDL_ShowOpenFileDialog / SDL_ShowMessageBox (SDL3)
 *
 * Import module: "wapi_dialog"
 */

#ifndef WAPI_DIALOG_H
#define WAPI_DIALOG_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * File Filter
 * ============================================================
 * Describes a selectable file type filter in the dialog.
 * e.g. { "Images", "*.png;*.jpg;*.gif" }
 *
 * Layout (32 bytes, align 8):
 *   Offset  0: wapi_stringview_t name     Display name ("Images")
 *   Offset 16: wapi_stringview_t pattern  Semicolon-separated globs
 */

typedef struct wapi_dialog_filter_t {
    wapi_stringview_t name;
    wapi_stringview_t pattern;
} wapi_dialog_filter_t;

/* ============================================================
 * File Dialog Flags
 * ============================================================ */

#define WAPI_DIALOG_FLAG_MULTISELECT  0x0001  /* Allow multiple file selection */

/* ============================================================
 * File Dialog Functions
 * ============================================================ */

/* ============================================================
 * File Dialogs (async, submitted via wapi_io_t)
 * Selected paths are written into the caller's buffer as consecutive
 * null-terminated UTF-8 strings (double-null terminated at the end).
 * ============================================================ */

static inline wapi_result_t wapi_dialog_open_file(
    const wapi_io_t* io,
    const wapi_dialog_filter_t* filters, uint32_t filter_count,
    wapi_stringview_t default_path, wapi_flags_t flags,
    char* buf, wapi_size_t buf_len,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_DIALOG_FILE_OPEN;
    op.flags     = flags;
    op.flags2    = filter_count;
    op.offset    = (uint64_t)(uintptr_t)filters;
    op.addr      = default_path.data;
    op.len       = default_path.length;
    op.addr2     = (uint64_t)(uintptr_t)buf;
    op.len2      = buf_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

static inline wapi_result_t wapi_dialog_save_file(
    const wapi_io_t* io,
    const wapi_dialog_filter_t* filters, uint32_t filter_count,
    wapi_stringview_t default_path,
    char* buf, wapi_size_t buf_len,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_DIALOG_FILE_SAVE;
    op.flags2    = filter_count;
    op.offset    = (uint64_t)(uintptr_t)filters;
    op.addr      = default_path.data;
    op.len       = default_path.length;
    op.addr2     = (uint64_t)(uintptr_t)buf;
    op.len2      = buf_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

static inline wapi_result_t wapi_dialog_open_folder(
    const wapi_io_t* io, wapi_stringview_t default_path,
    char* buf, wapi_size_t buf_len,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_DIALOG_FOLDER_OPEN;
    op.addr      = default_path.data;
    op.len       = default_path.length;
    op.addr2     = (uint64_t)(uintptr_t)buf;
    op.len2      = buf_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/* ============================================================
 * Message Box
 * ============================================================ */

typedef enum wapi_msgbox_type_t {
    WAPI_MSGBOX_INFO    = 0,  /* Informational */
    WAPI_MSGBOX_WARNING = 1,  /* Warning */
    WAPI_MSGBOX_ERROR   = 2,  /* Error */
    WAPI_MSGBOX_FORCE32 = 0x7FFFFFFF
} wapi_msgbox_type_t;

/* Predefined button configurations */
typedef enum wapi_msgbox_buttons_t {
    WAPI_MSGBOX_OK              = 0,  /* [OK] */
    WAPI_MSGBOX_OK_CANCEL       = 1,  /* [OK] [Cancel] */
    WAPI_MSGBOX_YES_NO          = 2,  /* [Yes] [No] */
    WAPI_MSGBOX_YES_NO_CANCEL   = 3,  /* [Yes] [No] [Cancel] */
    WAPI_MSGBOX_BUTTONS_FORCE32 = 0x7FFFFFFF
} wapi_msgbox_buttons_t;

/* Button IDs returned by wapi_dialog_message_box */
#define WAPI_MSGBOX_RESULT_OK     0
#define WAPI_MSGBOX_RESULT_CANCEL 1
#define WAPI_MSGBOX_RESULT_YES    2
#define WAPI_MSGBOX_RESULT_NO     3

/** Submit a native message box. Button id arrives in completion
 *  event's payload bytes 0..3 (WAPI_IO_CQE_F_INLINE set) as a u32. */
static inline wapi_result_t wapi_dialog_message_box(
    const wapi_io_t* io, wapi_msgbox_type_t type,
    wapi_stringview_t title, wapi_stringview_t message,
    wapi_msgbox_buttons_t buttons, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_DIALOG_MESSAGEBOX;
    op.flags     = (uint32_t)type;
    op.flags2    = (uint32_t)buttons;
    op.addr      = title.data;
    op.len       = title.length;
    op.addr2     = message.data;
    op.len2      = message.length;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Convenience: info box with OK button. */
static inline wapi_result_t wapi_dialog_simple_message_box(
    const wapi_io_t* io, wapi_stringview_t title,
    wapi_stringview_t message, uint64_t user_data)
{
    return wapi_dialog_message_box(io, WAPI_MSGBOX_INFO, title,
                                   message, WAPI_MSGBOX_OK, user_data);
}

/* ============================================================
 * Color Picker
 * ============================================================
 * Modal OS color picker. Unlike wapi_eyedropper (which samples a
 * single pixel from the screen), this is the full-featured dialog
 * with RGB/HSV sliders, palettes, and alpha input.
 *
 * Maps to: <input type="color"> (Web, alpha N/A),
 *          NSColorPanel (macOS),
 *          ChooseColor / IColorPickerDialog (Windows),
 *          GtkColorChooserDialog (Linux)
 */

#define WAPI_DIALOG_COLOR_FLAG_ALPHA  0x0001  /* Allow alpha channel editing */

/** Submit a color-picker dialog. Picked RGBA inlines in the
 *  completion payload bytes 0..3 with WAPI_IO_CQE_F_INLINE. */
static inline wapi_result_t wapi_dialog_pick_color(
    const wapi_io_t* io, wapi_stringview_t title,
    uint32_t initial_rgba, wapi_flags_t flags, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_DIALOG_PICK_COLOR;
    op.flags     = initial_rgba;
    op.flags2    = flags;
    op.addr      = title.data;
    op.len       = title.length;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/* ============================================================
 * Font Picker
 * ============================================================
 * Modal OS font picker. Returns the chosen family, size, and style
 * flags. The host resolves the family name against its own font
 * registry -- this dialog does not hand back font file bytes.
 *
 * Maps to: NSFontPanel (macOS),
 *          ChooseFont (Windows),
 *          GtkFontChooserDialog (Linux);
 *          no web analogue -- expect WAPI_ERR_NOTSUP in the browser shim.
 */

/* Weight and style are redeclared per-capability so wapi_dialog is
 * self-contained. Values match wapi_font / wapi_text for round-tripping. */

typedef enum wapi_dialog_font_weight_t {
    WAPI_DIALOG_FONT_WEIGHT_THIN       = 100,
    WAPI_DIALOG_FONT_WEIGHT_LIGHT      = 300,
    WAPI_DIALOG_FONT_WEIGHT_NORMAL     = 400,
    WAPI_DIALOG_FONT_WEIGHT_MEDIUM     = 500,
    WAPI_DIALOG_FONT_WEIGHT_SEMIBOLD   = 600,
    WAPI_DIALOG_FONT_WEIGHT_BOLD       = 700,
    WAPI_DIALOG_FONT_WEIGHT_EXTRABOLD  = 800,
    WAPI_DIALOG_FONT_WEIGHT_BLACK      = 900,
    WAPI_DIALOG_FONT_WEIGHT_FORCE32    = 0x7FFFFFFF
} wapi_dialog_font_weight_t;

typedef enum wapi_dialog_font_style_t {
    WAPI_DIALOG_FONT_STYLE_NORMAL  = 0,
    WAPI_DIALOG_FONT_STYLE_ITALIC  = 1,
    WAPI_DIALOG_FONT_STYLE_OBLIQUE = 2,
    WAPI_DIALOG_FONT_STYLE_FORCE32 = 0x7FFFFFFF
} wapi_dialog_font_style_t;

/**
 * Initial state / result for wapi_dialog_pick_font.
 *
 * Layout (32 bytes, align 8):
 *   Offset  0: wapi_stringview_t family  UTF-8 font family name
 *   Offset 16: float    size_px          Size in pixels
 *   Offset 20: uint32_t style            wapi_dialog_font_style_t
 *   Offset 24: uint32_t weight           wapi_dialog_font_weight_t
 *   Offset 28: uint32_t _pad
 */
typedef struct wapi_dialog_font_t {
    wapi_stringview_t family;
    float    size_px;
    uint32_t style;    /* wapi_dialog_font_style_t */
    uint32_t weight;   /* wapi_dialog_font_weight_t */
    uint32_t _pad;
} wapi_dialog_font_t;

/** Submit a native font picker dialog. */
static inline wapi_result_t wapi_dialog_pick_font(
    const wapi_io_t* vtable, wapi_stringview_t title,
    wapi_dialog_font_t* font_io, char* name_buf, wapi_size_t name_cap,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_DIALOG_PICK_FONT;
    op.addr      = title.data;
    op.len       = title.length;
    op.addr2     = (uint64_t)(uintptr_t)name_buf;
    op.len2      = name_cap;
    op.offset    = (uint64_t)(uintptr_t)font_io;
    op.user_data = user_data;
    return vtable->submit(vtable->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_DIALOG_H */
