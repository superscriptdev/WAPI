/**
 * WAPI Desktop Runtime - Environment and Process
 *
 * Module: "wapi_env" (per wapi_env.h).
 *   args_count, args_get, environ_count, environ_get, getenv,
 *   exit, open_url, get_locale, get_timezone, get_error.
 *
 * Random lives in its own module "wapi_random" (wapi_host_random.c)
 * because not every runtime can supply entropy.
 *
 * Every `wapi_size_t` param is wasm i64; every `wapi_size_t*` out
 * pointer targets a u64; every `wapi_stringview_t` arrives as a
 * pointer to the 16-byte {u64 data, u64 length} struct in wasm
 * memory (not expanded into ptr+len i32s).
 */

#include "wapi_host.h"
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
extern char** environ;
#endif

#define MOD "wapi_env"

/* Read a wapi_stringview_t at sv_ptr into NUL-terminated buf.
 * Returns written length (excluding NUL), or -1 on invalid. */
static int32_t read_stringview(uint32_t sv_ptr, char* out, size_t out_cap) {
    if (out_cap == 0) return -1;
    void* sv = wapi_wasm_ptr(sv_ptr, 16);
    if (!sv) return -1;
    uint64_t data, length;
    memcpy(&data,   (uint8_t*)sv + 0, 8);
    memcpy(&length, (uint8_t*)sv + 8, 8);
    if (data == 0) { out[0] = '\0'; return 0; }

    size_t copy;
    if (length == UINT64_MAX) {
        const char* s = (const char*)wapi_wasm_ptr((uint32_t)data, 1);
        if (!s) return -1;
        copy = strnlen(s, out_cap - 1);
        memcpy(out, s, copy);
    } else {
        copy = (size_t)length < out_cap - 1 ? (size_t)length : out_cap - 1;
        const char* s = (const char*)wapi_wasm_ptr((uint32_t)data, (uint32_t)copy);
        if (!s && copy > 0) return -1;
        if (s) memcpy(out, s, copy);
    }
    out[copy] = '\0';
    return (int32_t)copy;
}

/* Write a UTF-8 value to a guest buffer.  `len_ptr` (u64) always
 * receives the FULL length of the value so callers can detect
 * truncation and re-query with a bigger buffer. */
static void write_bounded(uint32_t buf_ptr, uint64_t buf_len,
                          uint32_t len_ptr, const char* val, uint64_t val_len)
{
    wapi_wasm_write_u64(len_ptr, val_len);
    uint64_t copy = val_len < buf_len ? val_len : buf_len;
    if (copy > 0) wapi_wasm_write_bytes(buf_ptr, val, (uint32_t)copy);
}

/* ============================================================
 * Command-Line Arguments
 * ============================================================ */

/* args_count: () -> i32 */
static wasm_trap_t* host_args_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(g_rt.app_argc);
    return NULL;
}

/* args_get: (i32 index, i32 buf, i64 buf_len, i32 arg_len_out) -> i32 */
static wasm_trap_t* host_args_get(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  index   = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint64_t buf_len = WAPI_ARG_U64(2);
    uint32_t len_ptr = WAPI_ARG_U32(3);

    if (index < 0 || index >= g_rt.app_argc) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; }
    const char* arg = g_rt.app_argv[index];
    write_bounded(buf_ptr, buf_len, len_ptr, arg, (uint64_t)strlen(arg));
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Environment Variables
 * ============================================================ */

#ifdef _WIN32
static int count_environ(void) {
    LPCH env_block = GetEnvironmentStringsA();
    if (!env_block) return 0;
    int count = 0;
    const char* p = env_block;
    while (*p) { count++; p += strlen(p) + 1; }
    FreeEnvironmentStringsA(env_block);
    return count;
}

static const char* get_environ_by_index(int index) {
    static char env_buf[4096];
    LPCH env_block = GetEnvironmentStringsA();
    if (!env_block) return NULL;
    const char* p = env_block;
    int i = 0;
    while (*p) {
        if (i == index) {
            size_t len = strlen(p);
            if (len >= sizeof(env_buf)) len = sizeof(env_buf) - 1;
            memcpy(env_buf, p, len);
            env_buf[len] = '\0';
            FreeEnvironmentStringsA(env_block);
            return env_buf;
        }
        p += strlen(p) + 1;
        i++;
    }
    FreeEnvironmentStringsA(env_block);
    return NULL;
}
#else
static int count_environ(void) {
    int count = 0;
    if (environ) while (environ[count]) count++;
    return count;
}
static const char* get_environ_by_index(int index) {
    if (!environ) return NULL;
    int i = 0;
    while (environ[i]) { if (i == index) return environ[i]; i++; }
    return NULL;
}
#endif

/* environ_count: () -> i32 */
static wasm_trap_t* host_environ_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(count_environ());
    return NULL;
}

/* environ_get: (i32 index, i32 buf, i64 buf_len, i32 var_len_out) -> i32 */
static wasm_trap_t* host_environ_get(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  index   = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint64_t buf_len = WAPI_ARG_U64(2);
    uint32_t len_ptr = WAPI_ARG_U32(3);

    const char* var = get_environ_by_index(index);
    if (!var) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; }
    write_bounded(buf_ptr, buf_len, len_ptr, var, (uint64_t)strlen(var));
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* getenv: (i32 name_sv, i32 buf, i64 buf_len, i32 val_len_out) -> i32 */
static wasm_trap_t* host_getenv(
    void* env_p, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env_p; (void)caller; (void)nargs; (void)nresults;
    uint32_t name_sv = WAPI_ARG_U32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint64_t buf_len = WAPI_ARG_U64(2);
    uint32_t len_ptr = WAPI_ARG_U32(3);

    char name[256];
    if (read_stringview(name_sv, name, sizeof(name)) < 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    const char* val = getenv(name);
    if (!val) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }
    write_bounded(buf_ptr, buf_len, len_ptr, val, (uint64_t)strlen(val));
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}


/* ============================================================
 * Process Control
 * ============================================================ */

/* exit: (i32 code) -> noreturn (trap) */
static wasm_trap_t* host_exit(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)results; (void)nresults;
    int32_t code = WAPI_ARG_I32(0);
    char msg[64];
    snprintf(msg, sizeof(msg), "wapi_env_exit(%d)", code);
    g_rt.running = false;
    return wasmtime_trap_new(msg, strlen(msg));
}

/* open_url: (i32 url_sv) -> i32 */
static wasm_trap_t* host_open_url(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t url_sv = WAPI_ARG_U32(0);
    char url[2048];
    if (read_stringview(url_sv, url, sizeof(url)) < 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

#ifdef _WIN32
    wchar_t wurl[2048];
    if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 2048) <= 0) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    HINSTANCE r = ShellExecuteW(NULL, L"open", wurl, NULL, NULL, SW_SHOWNORMAL);
    WAPI_RET_I32(((INT_PTR)r > 32) ? WAPI_OK : WAPI_ERR_IO);
#else
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
#endif
    return NULL;
}

/* ============================================================
 * Locale and Timezone
 * ============================================================ */

static void detect_locale(char* out, size_t out_size) {
#ifdef _WIN32
    wchar_t wbuf[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(wbuf, LOCALE_NAME_MAX_LENGTH) > 0) {
        if (WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, out, (int)out_size, NULL, NULL) > 0) return;
    }
#else
    const char* e = getenv("LC_ALL");
    if (!e || !*e) e = getenv("LC_MESSAGES");
    if (!e || !*e) e = getenv("LANG");
    if (e && *e) {
        size_t i = 0;
        for (; i + 1 < out_size && e[i] && e[i] != '.' && e[i] != '@'; i++) {
            out[i] = (e[i] == '_') ? '-' : e[i];
        }
        out[i] = '\0';
        if (i > 0) return;
    }
#endif
    const char* fb = "en-US";
    size_t fl = strlen(fb);
    if (fl >= out_size) fl = out_size - 1;
    memcpy(out, fb, fl);
    out[fl] = '\0';
}

static void detect_timezone(char* out, size_t out_size) {
#ifdef _WIN32
    DYNAMIC_TIME_ZONE_INFORMATION tzi;
    if (GetDynamicTimeZoneInformation(&tzi) != TIME_ZONE_ID_INVALID) {
        if (WideCharToMultiByte(CP_UTF8, 0, tzi.TimeZoneKeyName, -1,
                                out, (int)out_size, NULL, NULL) > 0) return;
    }
#else
    const char* e = getenv("TZ");
    if (e && *e) {
        if (e[0] == ':') e++;
        size_t len = strlen(e);
        if (len >= out_size) len = out_size - 1;
        memcpy(out, e, len);
        out[len] = '\0';
        return;
    }
    FILE* f = fopen("/etc/timezone", "r");
    if (f) {
        if (fgets(out, (int)out_size, f)) {
            size_t len = strlen(out);
            while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r')) out[--len] = '\0';
            fclose(f);
            if (len > 0) return;
        }
        fclose(f);
    }
#endif
    const char* fb = "UTC";
    size_t fl = strlen(fb);
    if (fl >= out_size) fl = out_size - 1;
    memcpy(out, fb, fl);
    out[fl] = '\0';
}

/* get_locale: (i32 buf, i64 buf_len, i32 len_ptr) -> i32 */
static wasm_trap_t* host_get_locale(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t buf_ptr = WAPI_ARG_U32(0);
    uint64_t buf_len = WAPI_ARG_U64(1);
    uint32_t len_ptr = WAPI_ARG_U32(2);
    char locale[96];
    detect_locale(locale, sizeof(locale));
    write_bounded(buf_ptr, buf_len, len_ptr, locale, (uint64_t)strlen(locale));
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* get_timezone: (i32 buf, i64 buf_len, i32 len_ptr) -> i32 */
static wasm_trap_t* host_get_timezone(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t buf_ptr = WAPI_ARG_U32(0);
    uint64_t buf_len = WAPI_ARG_U64(1);
    uint32_t len_ptr = WAPI_ARG_U32(2);
    char tz[128];
    detect_timezone(tz, sizeof(tz));
    write_bounded(buf_ptr, buf_len, len_ptr, tz, (uint64_t)strlen(tz));
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Error Messages
 * ============================================================ */

/* get_error: (i32 buf, i64 buf_len, i32 msg_len_out) -> i32 */
static wasm_trap_t* host_get_error(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t buf_ptr = WAPI_ARG_U32(0);
    uint64_t buf_len = WAPI_ARG_U64(1);
    uint32_t len_ptr = WAPI_ARG_U32(2);
    write_bounded(buf_ptr, buf_len, len_ptr, g_rt.last_error,
                  (uint64_t)strlen(g_rt.last_error));
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_env(wasmtime_linker_t* linker) {
    WAPI_DEFINE_0_1(linker, MOD, "args_count",    host_args_count);

    wapi_linker_define(linker, MOD, "args_get", host_args_get,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_0_1(linker, MOD, "environ_count", host_environ_count);

    wapi_linker_define(linker, MOD, "environ_get", host_environ_get,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    wapi_linker_define(linker, MOD, "getenv", host_getenv,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_1_0(linker, MOD, "exit",     host_exit);
    WAPI_DEFINE_1_1(linker, MOD, "open_url", host_open_url);

    wapi_linker_define(linker, MOD, "get_locale", host_get_locale,
        3, (wasm_valkind_t[]){WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, MOD, "get_timezone", host_get_timezone,
        3, (wasm_valkind_t[]){WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, MOD, "get_error", host_get_error,
        3, (wasm_valkind_t[]){WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
}
