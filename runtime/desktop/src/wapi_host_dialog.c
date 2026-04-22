/**
 * WAPI Desktop Runtime — System Dialogs (wapi_dialog.h)
 *
 * Synchronous op-ctx handlers for the WAPI_IO_OP_DIALOG_* opcodes,
 * backed by Win32 Common Item Dialogs (IFileOpenDialog /
 * IFileSaveDialog), MessageBoxW, and ChooseColor / ChooseFont.
 *
 * All dialogs run modal on the calling thread (which is the wasm
 * dispatch thread). That blocks the event pump for the duration of
 * the dialog — deliberate, since these are user-intent acks and the
 * I/O bridge treats the completion as immediately-available once
 * `submit` returns. A broker UI on a background thread will replace
 * this when Phase C lands.
 *
 * Completion conventions mirror wapi_dialog.h:
 *   FILE_OPEN / FILE_SAVE / FOLDER_OPEN:
 *     - On accept: result = bytes written into addr2/len2; buffer
 *       is one or more NUL-terminated UTF-8 paths, double-NUL at end.
 *     - On cancel: result = 0, buffer untouched.
 *   MESSAGEBOX:
 *     - result = button id (WAPI_MSGBOX_RESULT_*).
 *     - Inline in completion payload bytes 0..3 (WAPI_IO_CQE_F_INLINE).
 *   PICK_COLOR:
 *     - result = 0 (picked) / WAPI_ERR_CANCELED.
 *     - RGBA inlines at payload bytes 0..3 on success.
 *   PICK_FONT:
 *     - Writes the wapi_dialog_font_t at `offset` in place; copies
 *       family into addr2/len2. result = family bytes written.
 *
 * `op_ctx_t` addresses are guest (wasm) pointers; use wapi_wasm_ptr
 * to get a host-native pointer before reading/writing.
 */

#include "wapi_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define COBJMACROS

#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <objbase.h>
#include <wchar.h>

#pragma comment(lib, "ole32")
#pragma comment(lib, "uuid")
#pragma comment(lib, "comdlg32")
#pragma comment(lib, "shell32")

/* --- Local CLSIDs / IIDs (some SDK variants don't export these) --- */
static const CLSID WAPI_CLSID_FileOpenDialog = { 0xDC1C5A9C,0xE88A,0x4DDE,{0xA5,0xA1,0x60,0xF8,0x2A,0x20,0xAE,0xF7} };
static const CLSID WAPI_CLSID_FileSaveDialog = { 0xC0B4E2F3,0xBA21,0x4773,{0x8D,0xBA,0x33,0x5E,0xC9,0x46,0xEB,0x8B} };
static const IID   WAPI_IID_IFileOpenDialog  = { 0xD57C7288,0xD4AD,0x4768,{0xBE,0x02,0x9D,0x96,0x95,0x32,0xD9,0x60} };
static const IID   WAPI_IID_IFileSaveDialog  = { 0x84BCCD23,0x5FDE,0x4CDB,{0xAE,0xA4,0xAF,0x64,0xB8,0x3D,0x78,0xAB} };
static const IID   WAPI_IID_IShellItem       = { 0x43826D1E,0xE718,0x42EE,{0xBC,0x55,0xA1,0xE2,0x61,0xC3,0x7B,0xFE} };
static const IID   WAPI_IID_IShellItemArray  = { 0xB63EA76D,0x1F85,0x456F,{0xA1,0x9C,0x48,0x15,0x9E,0xFA,0x85,0x8B} };

/* Dialog filter structure from wapi_dialog.h — 32B:
 *   +0  stringview name     (u64 data, u64 length) = 16B
 *   +16 stringview pattern  (u64 data, u64 length) = 16B
 */

/* Mirrors of flags/enums from wapi_dialog.h. */
#define WAPI_MB_INFO    0
#define WAPI_MB_WARNING 1
#define WAPI_MB_ERROR   2

#define WAPI_MB_OK              0
#define WAPI_MB_OK_CANCEL       1
#define WAPI_MB_YES_NO          2
#define WAPI_MB_YES_NO_CANCEL   3

#define WAPI_MB_RESULT_OK     0
#define WAPI_MB_RESULT_CANCEL 1
#define WAPI_MB_RESULT_YES    2
#define WAPI_MB_RESULT_NO     3

#define WAPI_DLG_MULTISELECT     0x0001
#define WAPI_DLG_COLOR_FLAG_ALPHA 0x0001

/* ===== COM init (per-thread, idempotent) ===== */

static DWORD s_com_tls = TLS_OUT_OF_INDEXES;
static bool ensure_com(void) {
    if (s_com_tls == TLS_OUT_OF_INDEXES) s_com_tls = TlsAlloc();
    if (TlsGetValue(s_com_tls)) return true;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE) hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr) || hr == S_FALSE) { TlsSetValue(s_com_tls, (void*)1); return true; }
    return false;
}

/* Parent HWND: any live window in the handle table — user will see the
 * dialog centered over "the app"; single-window apps just get their
 * window. NULL falls back to desktop if none is live. */
static HWND pick_parent_hwnd(void) {
    for (int i = 1; i < WAPI_MAX_HANDLES; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_SURFACE &&
            g_rt.handles[i].data.window) {
            wapi_plat_native_handle_t n;
            if (wapi_plat_window_get_native(g_rt.handles[i].data.window, &n) &&
                n.kind == WAPI_PLAT_NATIVE_WIN32) {
                return (HWND)n.a;
            }
        }
    }
    return NULL;
}

/* ===== UTF-8 <-> UTF-16 helpers (malloc-caller-frees) ===== */

static WCHAR* utf8_to_wide(const char* s, size_t len) {
    if (len == 0 || !s) { WCHAR* w = (WCHAR*)malloc(sizeof(WCHAR)); if (w) w[0] = 0; return w; }
    int n = MultiByteToWideChar(CP_UTF8, 0, s, (int)len, NULL, 0);
    if (n <= 0) return NULL;
    WCHAR* w = (WCHAR*)malloc(sizeof(WCHAR) * (size_t)(n + 1));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, s, (int)len, w, n);
    w[n] = 0;
    return w;
}

/* Copy a WCHAR* into guest memory as UTF-8 NUL-terminated; returns
 * bytes written (including NUL) or 0 on failure. */
static size_t wide_to_guest(const WCHAR* w, uint32_t dst_ptr, uint64_t dst_cap) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return 0;
    if ((uint64_t)n > dst_cap) return 0;
    void* dst = wapi_wasm_ptr(dst_ptr, (uint32_t)n);
    if (!dst) return 0;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, (char*)dst, n, NULL, NULL);
    return (size_t)n;
}

/* ===== File filter translation =====
 * wapi_dialog_filter_t (32B) -> COMDLG_FILTERSPEC.
 * The pattern string comes in as "*.png;*.jpg"; Win32 expects the
 * same semicolon form so we pass through directly (wide-converted).
 * Caller owns the returned array and the backing wide strings — free
 * both after the dialog completes.
 */
typedef struct filter_alloc_t {
    COMDLG_FILTERSPEC* specs;
    WCHAR** strings; /* 2*count entries (name, pattern) */
    uint32_t count;
} filter_alloc_t;

static bool read_stringview(const uint8_t* base, wapi_stringview_t* out) {
    uint64_t data, len;
    memcpy(&data, base + 0, 8);
    memcpy(&len,  base + 8, 8);
    out->data = data;
    out->length = len;
    return true;
}

static bool build_filters(uint32_t filters_guest_ptr, uint32_t count,
                          filter_alloc_t* out)
{
    memset(out, 0, sizeof(*out));
    if (count == 0 || filters_guest_ptr == 0) return true;
    if (count > 256) return false; /* defensive cap */

    const uint8_t* base = (const uint8_t*)wapi_wasm_ptr(filters_guest_ptr, count * 32u);
    if (!base) return false;

    out->count   = count;
    out->specs   = (COMDLG_FILTERSPEC*)calloc(count, sizeof(COMDLG_FILTERSPEC));
    out->strings = (WCHAR**)calloc((size_t)count * 2, sizeof(WCHAR*));
    if (!out->specs || !out->strings) {
        free(out->specs); free(out->strings);
        memset(out, 0, sizeof(*out));
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        wapi_stringview_t name, patt;
        read_stringview(base + i * 32 +  0, &name);
        read_stringview(base + i * 32 + 16, &patt);
        const char* name_host = name.length ? (const char*)wapi_wasm_ptr((uint32_t)name.data, (uint32_t)name.length) : "";
        const char* patt_host = patt.length ? (const char*)wapi_wasm_ptr((uint32_t)patt.data, (uint32_t)patt.length) : "*";
        WCHAR* wname = utf8_to_wide(name_host, name.length);
        WCHAR* wpatt = utf8_to_wide(patt_host, patt.length);
        if (!wname || !wpatt) { free(wname); free(wpatt); continue; }
        out->strings[i*2 + 0] = wname;
        out->strings[i*2 + 1] = wpatt;
        out->specs[i].pszName = wname;
        out->specs[i].pszSpec = wpatt;
    }
    return true;
}

static void free_filters(filter_alloc_t* a) {
    if (!a) return;
    if (a->strings) {
        for (uint32_t i = 0; i < a->count * 2; i++) free(a->strings[i]);
        free(a->strings);
    }
    free(a->specs);
    memset(a, 0, sizeof(*a));
}

/* ===== Default-path helper: set initial folder from a UTF-8 path ===== */

static void apply_default_path(IFileDialog* dlg, const char* path, uint64_t len) {
    if (!path || len == 0) return;
    WCHAR* w = utf8_to_wide(path, (size_t)len);
    if (!w) return;
    IShellItem* item = NULL;
    if (SUCCEEDED(SHCreateItemFromParsingName(w, NULL, &WAPI_IID_IShellItem, (void**)&item)) && item) {
        IFileDialog_SetFolder(dlg, item);
        IShellItem_Release(item);
    }
    free(w);
}

/* ===== FILE_OPEN / FILE_SAVE / FOLDER_OPEN ===== */

static void write_guest_path_list(const WCHAR* const* paths, uint32_t count,
                                  uint32_t dst_ptr, uint64_t dst_cap,
                                  op_ctx_t* c)
{
    /* Serialize into local UTF-8 buffer first so we can measure. */
    size_t total = 0;
    for (uint32_t i = 0; i < count; i++) {
        int n = WideCharToMultiByte(CP_UTF8, 0, paths[i], -1, NULL, 0, NULL, NULL);
        if (n <= 0) continue;
        total += (size_t)n; /* includes NUL per entry */
    }
    total += 1; /* final extra NUL */

    if ((uint64_t)total > dst_cap) { c->result = WAPI_ERR_OVERFLOW; return; }
    char* buf = (char*)malloc(total);
    if (!buf) { c->result = WAPI_ERR_NOMEM; return; }
    size_t off = 0;
    for (uint32_t i = 0; i < count; i++) {
        int n = WideCharToMultiByte(CP_UTF8, 0, paths[i], -1,
                                    buf + off, (int)(total - off), NULL, NULL);
        if (n > 0) off += (size_t)n;
    }
    if (off < total) buf[off++] = 0;

    if (!wapi_wasm_write_bytes(dst_ptr, buf, (uint32_t)total)) {
        free(buf);
        c->result = WAPI_ERR_INVAL;
        return;
    }
    free(buf);
    /* Write length to result_ptr if guest provided one. */
    if (c->result_ptr) {
        wapi_wasm_write_u64((uint32_t)c->result_ptr, (uint64_t)total);
    }
    c->result = (int32_t)total;
}

static void dialog_file_open_impl(op_ctx_t* c, bool save_mode) {
    if (!ensure_com()) { c->result = WAPI_ERR_IO; return; }

    IFileDialog* dlg = NULL;
    HRESULT hr;
    if (save_mode) {
        hr = CoCreateInstance(&WAPI_CLSID_FileSaveDialog, NULL, CLSCTX_ALL,
                              &WAPI_IID_IFileSaveDialog, (void**)&dlg);
    } else {
        hr = CoCreateInstance(&WAPI_CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                              &WAPI_IID_IFileOpenDialog, (void**)&dlg);
    }
    if (FAILED(hr) || !dlg) { c->result = WAPI_ERR_IO; return; }

    DWORD opts = 0;
    IFileDialog_GetOptions(dlg, &opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
    if (save_mode) opts |= FOS_OVERWRITEPROMPT;
    if (!save_mode && (c->flags & WAPI_DLG_MULTISELECT)) opts |= FOS_ALLOWMULTISELECT;
    IFileDialog_SetOptions(dlg, opts);

    filter_alloc_t filters;
    if (!build_filters((uint32_t)c->offset, c->flags2, &filters)) {
        IFileDialog_Release(dlg);
        c->result = WAPI_ERR_INVAL;
        return;
    }
    if (filters.count > 0) {
        IFileDialog_SetFileTypes(dlg, filters.count, filters.specs);
        IFileDialog_SetFileTypeIndex(dlg, 1);
    }

    const char* default_path = c->len ? (const char*)wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len) : NULL;
    apply_default_path(dlg, default_path, c->len);

    HWND parent = pick_parent_hwnd();
    hr = IFileDialog_Show(dlg, parent);

    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        free_filters(&filters);
        IFileDialog_Release(dlg);
        c->result = 0;
        if (c->result_ptr) wapi_wasm_write_u64((uint32_t)c->result_ptr, 0);
        return;
    }
    if (FAILED(hr)) {
        free_filters(&filters);
        IFileDialog_Release(dlg);
        c->result = WAPI_ERR_IO;
        return;
    }

    if (!save_mode && (opts & FOS_ALLOWMULTISELECT)) {
        IFileOpenDialog* open = (IFileOpenDialog*)dlg;
        IShellItemArray* arr = NULL;
        if (SUCCEEDED(IFileOpenDialog_GetResults(open, &arr)) && arr) {
            DWORD n = 0;
            IShellItemArray_GetCount(arr, &n);
            WCHAR** paths = (WCHAR**)calloc(n, sizeof(WCHAR*));
            uint32_t got = 0;
            if (paths) {
                for (DWORD i = 0; i < n; i++) {
                    IShellItem* it = NULL;
                    if (SUCCEEDED(IShellItemArray_GetItemAt(arr, i, &it)) && it) {
                        WCHAR* p = NULL;
                        if (SUCCEEDED(IShellItem_GetDisplayName(it, SIGDN_FILESYSPATH, &p)) && p) {
                            paths[got++] = _wcsdup(p);
                            CoTaskMemFree(p);
                        }
                        IShellItem_Release(it);
                    }
                }
                write_guest_path_list((const WCHAR* const*)paths, got,
                                      (uint32_t)c->addr2, c->len2, c);
                for (uint32_t i = 0; i < got; i++) free(paths[i]);
                free(paths);
            }
            IShellItemArray_Release(arr);
        } else {
            c->result = WAPI_ERR_IO;
        }
    } else {
        IShellItem* it = NULL;
        if (SUCCEEDED(IFileDialog_GetResult(dlg, &it)) && it) {
            WCHAR* p = NULL;
            if (SUCCEEDED(IShellItem_GetDisplayName(it, SIGDN_FILESYSPATH, &p)) && p) {
                const WCHAR* arr[1] = { p };
                write_guest_path_list(arr, 1, (uint32_t)c->addr2, c->len2, c);
                CoTaskMemFree(p);
            } else {
                c->result = WAPI_ERR_IO;
            }
            IShellItem_Release(it);
        } else {
            c->result = WAPI_ERR_IO;
        }
    }

    free_filters(&filters);
    IFileDialog_Release(dlg);
}

void wapi_host_dialog_file_open_op(op_ctx_t* c)   { dialog_file_open_impl(c, false); }
void wapi_host_dialog_file_save_op(op_ctx_t* c)   { dialog_file_open_impl(c, true); }

void wapi_host_dialog_folder_open_op(op_ctx_t* c) {
    if (!ensure_com()) { c->result = WAPI_ERR_IO; return; }

    IFileOpenDialog* dlg = NULL;
    HRESULT hr = CoCreateInstance(&WAPI_CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                                  &WAPI_IID_IFileOpenDialog, (void**)&dlg);
    if (FAILED(hr) || !dlg) { c->result = WAPI_ERR_IO; return; }

    DWORD opts = 0;
    IFileOpenDialog_GetOptions(dlg, &opts);
    IFileOpenDialog_SetOptions(dlg, opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

    const char* default_path = c->len ? (const char*)wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len) : NULL;
    apply_default_path((IFileDialog*)dlg, default_path, c->len);

    HWND parent = pick_parent_hwnd();
    hr = IFileOpenDialog_Show(dlg, parent);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        IFileOpenDialog_Release(dlg);
        c->result = 0;
        if (c->result_ptr) wapi_wasm_write_u64((uint32_t)c->result_ptr, 0);
        return;
    }
    if (FAILED(hr)) { IFileOpenDialog_Release(dlg); c->result = WAPI_ERR_IO; return; }

    IShellItem* it = NULL;
    if (SUCCEEDED(IFileOpenDialog_GetResult(dlg, &it)) && it) {
        WCHAR* p = NULL;
        if (SUCCEEDED(IShellItem_GetDisplayName(it, SIGDN_FILESYSPATH, &p)) && p) {
            const WCHAR* arr[1] = { p };
            write_guest_path_list(arr, 1, (uint32_t)c->addr2, c->len2, c);
            CoTaskMemFree(p);
        } else { c->result = WAPI_ERR_IO; }
        IShellItem_Release(it);
    } else { c->result = WAPI_ERR_IO; }

    IFileOpenDialog_Release(dlg);
}

/* ===== MESSAGEBOX ===== */

void wapi_host_dialog_messagebox_op(op_ctx_t* c) {
    const char* title_host   = c->len  ? (const char*)wapi_wasm_ptr((uint32_t)c->addr,  (uint32_t)c->len)  : "";
    const char* message_host = c->len2 ? (const char*)wapi_wasm_ptr((uint32_t)c->addr2, (uint32_t)c->len2) : "";
    if ((c->len  && !title_host) || (c->len2 && !message_host)) { c->result = WAPI_ERR_INVAL; return; }

    WCHAR* wtitle = utf8_to_wide(title_host,   c->len);
    WCHAR* wmsg   = utf8_to_wide(message_host, c->len2);
    if (!wtitle || !wmsg) { free(wtitle); free(wmsg); c->result = WAPI_ERR_NOMEM; return; }

    UINT mb_icon = MB_ICONINFORMATION;
    switch (c->flags) {
    case WAPI_MB_WARNING: mb_icon = MB_ICONWARNING; break;
    case WAPI_MB_ERROR:   mb_icon = MB_ICONERROR;   break;
    default:              mb_icon = MB_ICONINFORMATION; break;
    }
    UINT mb_btns = MB_OK;
    switch (c->flags2) {
    case WAPI_MB_OK_CANCEL:     mb_btns = MB_OKCANCEL;     break;
    case WAPI_MB_YES_NO:        mb_btns = MB_YESNO;        break;
    case WAPI_MB_YES_NO_CANCEL: mb_btns = MB_YESNOCANCEL;  break;
    default:                    mb_btns = MB_OK;           break;
    }

    int ret = MessageBoxW(pick_parent_hwnd(), wmsg, wtitle, mb_icon | mb_btns | MB_TASKMODAL);
    free(wtitle); free(wmsg);

    uint32_t button_id = WAPI_MB_RESULT_OK;
    switch (ret) {
    case IDOK:     button_id = WAPI_MB_RESULT_OK;     break;
    case IDCANCEL: button_id = WAPI_MB_RESULT_CANCEL; break;
    case IDYES:    button_id = WAPI_MB_RESULT_YES;    break;
    case IDNO:     button_id = WAPI_MB_RESULT_NO;     break;
    default:       button_id = WAPI_MB_RESULT_OK;     break;
    }
    c->result = (int32_t)button_id;
    c->inline_payload = true;
    memcpy(c->payload, &button_id, 4);
    if (c->result_ptr) wapi_wasm_write_u64((uint32_t)c->result_ptr, (uint64_t)button_id);
}

/* ===== PICK_COLOR (classic ChooseColor) =====
 *
 * flags         = initial_rgba (R in bits 0..7 per WAPI convention,
 *                 G in 8..15, B in 16..23, A in 24..31).
 * flags2        = WAPI_DIALOG_COLOR_FLAG_ALPHA bitmask; classic
 *                 ChooseColor has no alpha UI, so we preserve the
 *                 initial alpha and only pick RGB.
 * addr/len      = title (unused — ChooseColor does not take a title).
 */

void wapi_host_dialog_pick_color_op(op_ctx_t* c) {
    uint32_t rgba = c->flags;
    uint8_t  r = (uint8_t)(rgba >>  0);
    uint8_t  g = (uint8_t)(rgba >>  8);
    uint8_t  b = (uint8_t)(rgba >> 16);
    uint8_t  a = (uint8_t)(rgba >> 24);

    static COLORREF custom[16];
    CHOOSECOLORW cc; memset(&cc, 0, sizeof(cc));
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = pick_parent_hwnd();
    cc.lpCustColors = custom;
    cc.rgbResult    = RGB(r, g, b);
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;

    BOOL ok = ChooseColorW(&cc);
    if (!ok) { c->result = WAPI_ERR_CANCELED; return; }

    uint8_t nr = GetRValue(cc.rgbResult);
    uint8_t ng = GetGValue(cc.rgbResult);
    uint8_t nb = GetBValue(cc.rgbResult);
    uint32_t out = (uint32_t)nr | ((uint32_t)ng << 8) | ((uint32_t)nb << 16) | ((uint32_t)a << 24);

    c->result = 0;
    c->inline_payload = true;
    memcpy(c->payload, &out, 4);
    if (c->result_ptr) wapi_wasm_write_u64((uint32_t)c->result_ptr, (uint64_t)out);
}

/* ===== PICK_FONT (ChooseFont) =====
 *
 * offset        = guest ptr to wapi_dialog_font_t (32B) — read for
 *                 initial state, written on return.
 * addr/len      = title (unused by ChooseFontW).
 * addr2/len2    = family name buffer (UTF-8, NUL-terminated).
 */

void wapi_host_dialog_pick_font_op(op_ctx_t* c) {
    /* wapi_dialog_font_t layout:
     *   +0  stringview family (u64 data, u64 length) = 16B
     *   +16 f32 size_px
     *   +20 u32 style    (0 normal, 1 italic, 2 oblique)
     *   +24 u32 weight   (100..900)
     *   +28 u32 _pad */
    if (c->offset == 0) { c->result = WAPI_ERR_INVAL; return; }
    uint8_t* font_io = (uint8_t*)wapi_wasm_ptr((uint32_t)c->offset, 32);
    if (!font_io) { c->result = WAPI_ERR_INVAL; return; }

    uint64_t family_data = 0, family_len = 0;
    float    size_px;
    uint32_t style, weight;
    memcpy(&family_data, font_io +  0, 8);
    memcpy(&family_len,  font_io +  8, 8);
    memcpy(&size_px,     font_io + 16, 4);
    memcpy(&style,       font_io + 20, 4);
    memcpy(&weight,      font_io + 24, 4);

    LOGFONTW lf; memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -(LONG)(size_px > 0.0f ? size_px : 12.0f);
    lf.lfWeight = (LONG)(weight ? weight : FW_NORMAL);
    lf.lfItalic = (style == 1 || style == 2) ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    if (family_len > 0) {
        const char* fam = (const char*)wapi_wasm_ptr((uint32_t)family_data, (uint32_t)family_len);
        if (fam) {
            int n = MultiByteToWideChar(CP_UTF8, 0, fam, (int)family_len, lf.lfFaceName, LF_FACESIZE - 1);
            if (n > 0) lf.lfFaceName[n] = 0;
        }
    }

    CHOOSEFONTW cf; memset(&cf, 0, sizeof(cf));
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner   = pick_parent_hwnd();
    cf.lpLogFont   = &lf;
    cf.Flags       = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_FORCEFONTEXIST;

    if (!ChooseFontW(&cf)) { c->result = WAPI_ERR_CANCELED; return; }

    /* Write the picked font back into the guest struct + buffer. */
    size_t name_bytes = wide_to_guest(lf.lfFaceName, (uint32_t)c->addr2, c->len2);

    /* Update in-place family stringview to point at the freshly-filled
     * buffer; size/style/weight written unconditionally. name_bytes
     * includes the trailing NUL — store length without it. */
    uint64_t new_data = c->addr2;
    uint64_t new_len  = name_bytes > 0 ? name_bytes - 1 : 0;
    memcpy(font_io +  0, &new_data, 8);
    memcpy(font_io +  8, &new_len,  8);

    float new_size_px = (float)(-lf.lfHeight);
    if (new_size_px <= 0.0f) new_size_px = size_px;
    uint32_t new_style  = lf.lfItalic ? 1u : 0u;
    uint32_t new_weight = (uint32_t)lf.lfWeight;
    memcpy(font_io + 16, &new_size_px, 4);
    memcpy(font_io + 20, &new_style,   4);
    memcpy(font_io + 24, &new_weight,  4);

    c->result = (int32_t)new_len;
    if (c->result_ptr) wapi_wasm_write_u64((uint32_t)c->result_ptr, new_len);
}
