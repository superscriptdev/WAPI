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
 *
 * Query availability with wapi_capability_supported("wapi.dialog", 11)
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

/**
 * Show a native "Open File" dialog.
 *
 * Submitted via the I/O vtable. The vtable implementation determines
 * whether this blocks or returns immediately.
 * Selected paths are written as consecutive null-terminated
 * UTF-8 strings into the buffer (double-null terminated).
 *
 * @see WAPI_IO_OP_DIALOG_FILE_OPEN
 *
 * @param filters      Array of file type filters (NULL for none).
 * @param filter_count Number of filters.
 * @param default_path Initial directory or file path (NULL = system default).
 * @param flags        WAPI_DIALOG_FLAG_* flags.
 * @param buf          [out] Buffer for selected path(s).
 * @param buf_len      Buffer capacity in bytes.
 * @param result_len   [out] Bytes written to buf.
 * @return WAPI_OK on success, WAPI_ERR_CANCELED if dismissed.
 *
 * Wasm signature: (i32, i32, i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dialog, open_file)
wapi_result_t wapi_dialog_open_file(const wapi_dialog_filter_t* filters,
                                    uint32_t filter_count,
                                    wapi_stringview_t default_path,
                                    wapi_flags_t flags,
                                    char* buf, wapi_size_t buf_len,
                                    wapi_size_t* result_len);

/**
 * Show a native "Save File" dialog.
 *
 * @see WAPI_IO_OP_DIALOG_FILE_SAVE
 *
 * @param filters        Array of file type filters (NULL for none).
 * @param filter_count   Number of filters.
 * @param default_path   Suggested file path/name (NULL = system default).
 * @param buf            [out] Buffer for the chosen save path.
 * @param buf_len        Buffer capacity.
 * @param result_len     [out] Bytes written to buf.
 * @return WAPI_OK on success, WAPI_ERR_CANCELED if dismissed.
 *
 * Wasm signature: (i32, i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dialog, save_file)
wapi_result_t wapi_dialog_save_file(const wapi_dialog_filter_t* filters,
                                    uint32_t filter_count,
                                    wapi_stringview_t default_path,
                                    char* buf, wapi_size_t buf_len,
                                    wapi_size_t* result_len);

/**
 * Show a native "Open Folder" dialog.
 *
 * @see WAPI_IO_OP_DIALOG_FOLDER_OPEN
 *
 * @param default_path  Initial directory (NULL = system default).
 * @param buf           [out] Buffer for the selected folder path.
 * @param buf_len       Buffer capacity.
 * @param result_len    [out] Bytes written to buf.
 * @return WAPI_OK on success, WAPI_ERR_CANCELED if dismissed.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dialog, open_folder)
wapi_result_t wapi_dialog_open_folder(wapi_stringview_t default_path,
                                      char* buf, wapi_size_t buf_len,
                                      wapi_size_t* result_len);

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

/**
 * Show a native message box dialog.
 *
 * Submitted via the I/O vtable. The vtable implementation determines
 * whether this blocks or returns immediately.
 *
 * @see WAPI_IO_OP_DIALOG_MESSAGEBOX
 *
 * @param type       Icon / severity.
 * @param title      Dialog title (UTF-8).
 * @param message    Dialog message body (UTF-8).
 * @param buttons    Button configuration.
 * @param result     [out] Which button was clicked (WAPI_MSGBOX_RESULT_*).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dialog, message_box)
wapi_result_t wapi_dialog_message_box(wapi_msgbox_type_t type,
                                      wapi_stringview_t title,
                                      wapi_stringview_t message,
                                      wapi_msgbox_buttons_t buttons,
                                      int32_t* result);

/**
 * Convenience: show a simple informational message box with an OK button.
 *
 * @param title      Dialog title (UTF-8).
 * @param message    Dialog message (UTF-8).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dialog, simple_message_box)
wapi_result_t wapi_dialog_simple_message_box(wapi_stringview_t title,
                                             wapi_stringview_t message);

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

/**
 * Show a native color picker dialog.
 *
 * Submitted via the I/O vtable. The vtable implementation determines
 * whether this blocks or returns immediately.
 *
 * @see WAPI_IO_OP_DIALOG_PICK_COLOR
 *
 * @param title        Dialog title (UTF-8), may be empty for OS default.
 * @param initial_rgba Starting color as 0xRRGGBBAA. Alpha ignored
 *                     unless WAPI_DIALOG_COLOR_FLAG_ALPHA is set.
 * @param flags        WAPI_DIALOG_COLOR_FLAG_* flags.
 * @param result_rgba  [out] Picked color as 0xRRGGBBAA.
 * @return WAPI_OK on success, WAPI_ERR_CANCELED if dismissed.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dialog, pick_color)
wapi_result_t wapi_dialog_pick_color(wapi_stringview_t title,
                                     uint32_t initial_rgba,
                                     wapi_flags_t flags,
                                     uint32_t* result_rgba);

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

/**
 * Show a native font picker dialog.
 *
 * The caller passes an initial selection and a buffer into which the
 * host writes the chosen family name (UTF-8). On return, `io->family`
 * points into `name_buf`.
 *
 * @see WAPI_IO_OP_DIALOG_PICK_FONT
 *
 * @param title      Dialog title (UTF-8), may be empty for OS default.
 * @param io         [in/out] Initial selection on entry, picked font on return.
 * @param name_buf   [out] Buffer receiving the UTF-8 family name.
 * @param name_cap   Capacity of name_buf in bytes.
 * @return WAPI_OK on success, WAPI_ERR_CANCELED if dismissed,
 *         WAPI_ERR_NOTSUP if the platform has no native font picker.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dialog, pick_font)
wapi_result_t wapi_dialog_pick_font(wapi_stringview_t title,
                                    wapi_dialog_font_t* io,
                                    char* name_buf,
                                    wapi_size_t name_cap);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_DIALOG_H */
