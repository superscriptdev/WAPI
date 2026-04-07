/**
 * WAPI Desktop Runtime - Font Enumeration
 *
 * Implements all wapi_font.* imports.
 *
 * Platform-specific font enumeration:
 *   Windows: EnumFontFamiliesExA
 *   macOS:   CTFontManagerCopyAvailableFontFamilyNames (CoreText)
 *   Linux:   fontconfig (FcFontList)
 *
 * The system font list is cached on first call to family_count().
 */

#include "wapi_host.h"

/* ============================================================
 * Static Font Cache
 * ============================================================ */

#define WAPI_FONT_MAX_FAMILIES 1024
#define WAPI_FONT_NAME_MAX     128

typedef struct wapi_font_entry_t {
    char name[WAPI_FONT_NAME_MAX];
    uint32_t name_len;
} wapi_font_entry_t;

static wapi_font_entry_t s_fonts[WAPI_FONT_MAX_FAMILIES];
static int               s_font_count = 0;
static bool              s_fonts_loaded = false;

/* ============================================================
 * Platform Font Enumeration
 * ============================================================ */

#if defined(_WIN32)
/* ---- Windows: EnumFontFamiliesExA ---- */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static int CALLBACK enum_font_cb(const LOGFONTA* lf, const TEXTMETRICA* tm,
                                 DWORD font_type, LPARAM lparam)
{
    (void)tm; (void)font_type; (void)lparam;

    if (s_font_count >= WAPI_FONT_MAX_FAMILIES) return 0; /* stop */

    /* Skip duplicates (EnumFontFamiliesEx can report the same family
       multiple times for different char-sets). */
    for (int i = 0; i < s_font_count; i++) {
        if (strcmp(s_fonts[i].name, lf->lfFaceName) == 0)
            return 1; /* continue */
    }

    /* Skip names starting with '@' (vertical-writing variants) */
    if (lf->lfFaceName[0] == '@') return 1;

    size_t len = strlen(lf->lfFaceName);
    if (len >= WAPI_FONT_NAME_MAX) len = WAPI_FONT_NAME_MAX - 1;
    memcpy(s_fonts[s_font_count].name, lf->lfFaceName, len);
    s_fonts[s_font_count].name[len] = '\0';
    s_fonts[s_font_count].name_len  = (uint32_t)len;
    s_font_count++;
    return 1; /* continue enumeration */
}

static void enumerate_system_fonts(void) {
    LOGFONTA lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;

    HDC hdc = GetDC(NULL);
    EnumFontFamiliesExA(hdc, &lf, (FONTENUMPROCA)enum_font_cb, 0, 0);
    ReleaseDC(NULL, hdc);
}

#elif defined(__APPLE__)
/* ---- macOS / iOS: CoreText ---- */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

static void enumerate_system_fonts(void) {
    CFArrayRef names = CTFontManagerCopyAvailableFontFamilyNames();
    if (!names) return;

    CFIndex count = CFArrayGetCount(names);
    for (CFIndex i = 0; i < count && s_font_count < WAPI_FONT_MAX_FAMILIES; i++) {
        CFStringRef cfname = (CFStringRef)CFArrayGetValueAtIndex(names, i);
        char buf[WAPI_FONT_NAME_MAX];
        if (CFStringGetCString(cfname, buf, sizeof(buf), kCFStringEncodingUTF8)) {
            /* Skip names starting with '.' (hidden system fonts) */
            if (buf[0] == '.') continue;

            size_t len = strlen(buf);
            if (len >= WAPI_FONT_NAME_MAX) len = WAPI_FONT_NAME_MAX - 1;
            memcpy(s_fonts[s_font_count].name, buf, len);
            s_fonts[s_font_count].name[len] = '\0';
            s_fonts[s_font_count].name_len  = (uint32_t)len;
            s_font_count++;
        }
    }

    CFRelease(names);
}

#elif defined(__linux__)
/* ---- Linux: fontconfig ---- */

#include <fontconfig/fontconfig.h>

static void enumerate_system_fonts(void) {
    if (!FcInit()) return;

    FcPattern*   pat = FcPatternCreate();
    FcObjectSet* os  = FcObjectSetBuild(FC_FAMILY, (char*)0);
    FcFontSet*   fs  = FcFontList(NULL, pat, os);

    if (fs) {
        for (int i = 0; i < fs->nfont && s_font_count < WAPI_FONT_MAX_FAMILIES; i++) {
            FcChar8* family = NULL;
            if (FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, &family) == FcResultMatch
                && family)
            {
                /* Skip duplicates */
                bool dup = false;
                for (int j = 0; j < s_font_count; j++) {
                    if (strcmp(s_fonts[j].name, (const char*)family) == 0) {
                        dup = true;
                        break;
                    }
                }
                if (dup) continue;

                size_t len = strlen((const char*)family);
                if (len >= WAPI_FONT_NAME_MAX) len = WAPI_FONT_NAME_MAX - 1;
                memcpy(s_fonts[s_font_count].name, family, len);
                s_fonts[s_font_count].name[len] = '\0';
                s_fonts[s_font_count].name_len  = (uint32_t)len;
                s_font_count++;
            }
        }
        FcFontSetDestroy(fs);
    }

    if (os)  FcObjectSetDestroy(os);
    if (pat) FcPatternDestroy(pat);
}

#else
/* ---- Unknown platform: no fonts ---- */
static void enumerate_system_fonts(void) {
    /* Nothing to enumerate */
}
#endif

/* Ensure the font cache is populated (lazy init). */
static void ensure_fonts_loaded(void) {
    if (s_fonts_loaded) return;
    s_fonts_loaded = true;
    enumerate_system_fonts();
}

/* ============================================================
 * Callback: family_count
 * () -> i32
 * ============================================================ */
static wasm_trap_t* cb_family_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;

    ensure_fonts_loaded();
    WAPI_RET_I32(s_font_count);
    return NULL;
}

/* ============================================================
 * Callback: family_info
 * (i32 index, i32 info_ptr) -> i32
 *
 * Writes a wapi_font_info_t (24 bytes) into guest memory:
 *   +0:  u32 family_ptr   (guest pointer to family name bytes)
 *   +4:  u32 family_len
 *   +8:  u32 weight_min
 *  +12:  u32 weight_max
 *  +16:  u32 style_flags
 *  +20:  i32 is_variable
 *
 * The family name is written immediately after the info struct
 * (at info_ptr + 24) so the guest can read it without a second
 * allocation.
 * ============================================================ */
static wasm_trap_t* cb_family_info(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  index    = WAPI_ARG_I32(0);
    uint32_t info_ptr = WAPI_ARG_U32(1);

    ensure_fonts_loaded();

    if (index < 0 || index >= s_font_count) {
        WAPI_RET_I32(WAPI_ERR_RANGE);
        return NULL;
    }

    const wapi_font_entry_t* fe = &s_fonts[index];

    /* Write the family name bytes right after the 24-byte struct */
    uint32_t name_guest_ptr = info_ptr + 24;
    if (!wapi_wasm_write_bytes(name_guest_ptr, fe->name, fe->name_len)) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    /* Write the info struct */
    if (!wapi_wasm_write_u32(info_ptr +  0, name_guest_ptr)) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    if (!wapi_wasm_write_u32(info_ptr +  4, fe->name_len))   { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    if (!wapi_wasm_write_u32(info_ptr +  8, 100))            { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; } /* weight_min: Thin */
    if (!wapi_wasm_write_u32(info_ptr + 12, 900))            { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; } /* weight_max: Black */
    if (!wapi_wasm_write_u32(info_ptr + 16, 0))              { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; } /* style_flags: none */
    if (!wapi_wasm_write_i32(info_ptr + 20, 0))              { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; } /* is_variable: false */

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Callback: supports_script
 * (i32 tag_ptr, i32 tag_len) -> i32
 *
 * Simplified: returns 1 for "Latn", 0 otherwise.
 * ============================================================ */
static wasm_trap_t* cb_supports_script(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t tag_ptr = WAPI_ARG_U32(0);
    uint32_t tag_len = WAPI_ARG_U32(1);

    const char* tag = wapi_wasm_read_string(tag_ptr, tag_len);
    if (tag && tag_len == 4 && memcmp(tag, "Latn", 4) == 0) {
        WAPI_RET_I32(1);
    } else {
        WAPI_RET_I32(0);
    }
    return NULL;
}

/* ============================================================
 * Callback: has_feature
 * (i32 family_ptr, i32 family_len, i32 tag) -> i32
 *
 * Returns 0 (unknown / not checked).
 * ============================================================ */
static wasm_trap_t* cb_has_feature(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    (void)args;

    WAPI_RET_I32(0);
    return NULL;
}

/* ============================================================
 * Callback: fallback_count
 * (i32 family_ptr, i32 family_len) -> i32
 *
 * Returns 0 -- no fallback chain info available yet.
 * ============================================================ */
static wasm_trap_t* cb_fallback_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    (void)args;

    WAPI_RET_I32(0);
    return NULL;
}

/* ============================================================
 * Callback: fallback_get
 * (i32 family_ptr, i32 family_len, i32 index,
 *  i32 buf, i32 buf_len, i32 name_len_ptr) -> i32
 *
 * Always returns WAPI_ERR_RANGE (no fallbacks).
 * ============================================================ */
static wasm_trap_t* cb_fallback_get(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    (void)args;

    WAPI_RET_I32(WAPI_ERR_RANGE);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_font(wasmtime_linker_t* linker) {
    /* family_count: () -> i32 */
    WAPI_DEFINE_0_1(linker, "wapi_font", "family_count",    cb_family_count);

    /* family_info: (i32, i32) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_font", "family_info",     cb_family_info);

    /* supports_script: (i32, i32) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_font", "supports_script", cb_supports_script);

    /* has_feature: (i32, i32, i32) -> i32 */
    WAPI_DEFINE_3_1(linker, "wapi_font", "has_feature",     cb_has_feature);

    /* fallback_count: (i32, i32) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_font", "fallback_count",  cb_fallback_count);

    /* fallback_get: (i32, i32, i32, i32, i32, i32) -> i32 */
    WAPI_DEFINE_6_1(linker, "wapi_font", "fallback_get",    cb_fallback_get);
}
