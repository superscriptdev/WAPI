/**
 * WAPI - System Dialogs
 * Version 1.0.0
 *
 * Native file open/save/folder dialogs and message boxes.
 * Dialogs are modal and are submitted via the I/O vtable. The vtable
 * implementation determines whether they block or return immediately.
 *
 * Shaped after SDL3 SDL_dialog.h + SDL_messagebox.h.
 *
 * Maps to: <input type="file"> / window.confirm (Web),
 *          NSOpenPanel / NSAlert (macOS),
 *          IFileOpenDialog / MessageBox (Windows),
 *          GTK file chooser / dialog (Linux),
 *          SDL_ShowOpenFileDialog / SDL_ShowMessageBox (SDL3)
 *
 * Import module: "wapi_dialog"
 *
 * Query availability with wapi_capability_supported("wapi.dialog", 11)
 */

#ifndef WAPI_DIALOG_H
#define WAPI_DIALOG_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * File Filter
 * ============================================================
 * Describes a selectable file type filter in the dialog.
 * e.g. { "Images", "*.png;*.jpg;*.gif" }
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: ptr      name         Display name ("Images")
 *   Offset  4: uint32_t name_len
 *   Offset  8: ptr      pattern      Semicolon-separated globs
 *   Offset 12: uint32_t pattern_len
 */

typedef struct wapi_dialog_filter_t {
    const char* name;
    wapi_size_t name_len;
    const char* pattern;
    wapi_size_t pattern_len;
} wapi_dialog_filter_t;

/* ============================================================
 * File Dialog Flags
 * ============================================================ */

#define WAPI_DIALOG_FLAG_MULTI_SELECT  0x0001  /* Allow multiple file selection */

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
 * @see WAPI_IO_OP_DIALOG_OPEN_FILE
 *
 * @param filters      Array of file type filters (NULL for none).
 * @param filter_count Number of filters.
 * @param default_path Initial directory or file path (NULL = system default).
 * @param path_len     Length of default_path.
 * @param flags        WAPI_DIALOG_FLAG_* flags.
 * @param buf          [out] Buffer for selected path(s).
 * @param buf_len      Buffer capacity in bytes.
 * @param result_len   [out] Bytes written to buf.
 * @return WAPI_OK on success, WAPI_ERR_CANCELED if dismissed.
 *
 * Wasm signature: (i32, i32, i32, i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dialog, open_file)
wapi_result_t wapi_dialog_open_file(const wapi_dialog_filter_t* filters,
                                    uint32_t filter_count,
                                    const char* default_path,
                                    wapi_size_t path_len,
                                    uint32_t flags,
                                    char* buf, wapi_size_t buf_len,
                                    wapi_size_t* result_len);

/**
 * Show a native "Save File" dialog.
 *
 * @see WAPI_IO_OP_DIALOG_SAVE_FILE
 *
 * @param filters        Array of file type filters (NULL for none).
 * @param filter_count   Number of filters.
 * @param default_path   Suggested file path/name (NULL = system default).
 * @param path_len       Length of default_path.
 * @param buf            [out] Buffer for the chosen save path.
 * @param buf_len        Buffer capacity.
 * @param result_len     [out] Bytes written to buf.
 * @return WAPI_OK on success, WAPI_ERR_CANCELED if dismissed.
 *
 * Wasm signature: (i32, i32, i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dialog, save_file)
wapi_result_t wapi_dialog_save_file(const wapi_dialog_filter_t* filters,
                                    uint32_t filter_count,
                                    const char* default_path,
                                    wapi_size_t path_len,
                                    char* buf, wapi_size_t buf_len,
                                    wapi_size_t* result_len);

/**
 * Show a native "Open Folder" dialog.
 *
 * @see WAPI_IO_OP_DIALOG_OPEN_FOLDER
 *
 * @param default_path  Initial directory (NULL = system default).
 * @param path_len      Length of default_path.
 * @param buf           [out] Buffer for the selected folder path.
 * @param buf_len       Buffer capacity.
 * @param result_len    [out] Bytes written to buf.
 * @return WAPI_OK on success, WAPI_ERR_CANCELED if dismissed.
 *
 * Wasm signature: (i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dialog, open_folder)
wapi_result_t wapi_dialog_open_folder(const char* default_path,
                                      wapi_size_t path_len,
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
 * @see WAPI_IO_OP_DIALOG_MESSAGE_BOX
 *
 * @param type       Icon / severity.
 * @param title      Dialog title (UTF-8).
 * @param title_len  Title length.
 * @param message    Dialog message body (UTF-8).
 * @param msg_len    Message length.
 * @param buttons    Button configuration.
 * @param result     [out] Which button was clicked (WAPI_MSGBOX_RESULT_*).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dialog, message_box)
wapi_result_t wapi_dialog_message_box(wapi_msgbox_type_t type,
                                      const char* title,
                                      wapi_size_t title_len,
                                      const char* message,
                                      wapi_size_t msg_len,
                                      wapi_msgbox_buttons_t buttons,
                                      int32_t* result);

/**
 * Convenience: show a simple informational message box with an OK button.
 *
 * @param title      Dialog title (UTF-8).
 * @param title_len  Title length.
 * @param message    Dialog message (UTF-8).
 * @param msg_len    Message length.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_dialog, simple_message_box)
wapi_result_t wapi_dialog_simple_message_box(const char* title,
                                             wapi_size_t title_len,
                                             const char* message,
                                             wapi_size_t msg_len);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_DIALOG_H */
