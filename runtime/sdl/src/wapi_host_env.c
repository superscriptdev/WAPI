/**
 * WAPI SDL Runtime - Environment and Process
 */

#include "wapi_host.h"
#include <time.h>

#ifdef _WIN32
  #include <bcrypt.h>
  #include <windows.h>
  #pragma comment(lib, "bcrypt.lib")
#else
  #include <fcntl.h>
  #include <unistd.h>
  extern char** environ;
#endif

/* ---- Args ---- */

static int32_t host_args_count(wasm_exec_env_t env) {
    (void)env;
    return g_rt.app_argc;
}

static int32_t host_args_get(wasm_exec_env_t env,
                             int32_t index, uint32_t buf_ptr,
                             uint32_t buf_len, uint32_t arg_len_ptr) {
    (void)env;
    if (index < 0 || index >= g_rt.app_argc) return WAPI_ERR_RANGE;
    const char* arg = g_rt.app_argv[index];
    uint32_t len = (uint32_t)strlen(arg);
    wapi_wasm_write_u32(arg_len_ptr, len);
    uint32_t copy_len = len < buf_len ? len : buf_len;
    if (copy_len > 0) wapi_wasm_write_bytes(buf_ptr, arg, copy_len);
    return WAPI_OK;
}

/* ---- Environ ---- */

#ifdef _WIN32
static int count_environ(void) {
    LPCH block = GetEnvironmentStringsA();
    if (!block) return 0;
    int count = 0;
    const char* p = block;
    while (*p) { count++; p += strlen(p) + 1; }
    FreeEnvironmentStringsA(block);
    return count;
}
static const char* get_environ_by_index(int index) {
    static char env_buf[4096];
    LPCH block = GetEnvironmentStringsA();
    if (!block) return NULL;
    const char* p = block;
    int i = 0;
    while (*p) {
        if (i == index) {
            size_t len = strlen(p);
            if (len >= sizeof(env_buf)) len = sizeof(env_buf) - 1;
            memcpy(env_buf, p, len);
            env_buf[len] = '\0';
            FreeEnvironmentStringsA(block);
            return env_buf;
        }
        p += strlen(p) + 1;
        i++;
    }
    FreeEnvironmentStringsA(block);
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

static int32_t host_environ_count(wasm_exec_env_t env) {
    (void)env;
    return count_environ();
}

static int32_t host_environ_get(wasm_exec_env_t env,
                                int32_t index, uint32_t buf_ptr,
                                uint32_t buf_len, uint32_t var_len_ptr) {
    (void)env;
    const char* var = get_environ_by_index(index);
    if (!var) return WAPI_ERR_RANGE;
    uint32_t len = (uint32_t)strlen(var);
    wapi_wasm_write_u32(var_len_ptr, len);
    uint32_t copy_len = len < buf_len ? len : buf_len;
    if (copy_len > 0) wapi_wasm_write_bytes(buf_ptr, var, copy_len);
    return WAPI_OK;
}

static int32_t host_getenv(wasm_exec_env_t env,
                           uint32_t name_ptr, uint32_t name_len,
                           uint32_t buf_ptr, uint32_t buf_len,
                           uint32_t val_len_ptr) {
    (void)env;
    const char* name = (const char*)wapi_wasm_ptr(name_ptr, name_len);
    if (!name && name_len > 0) return WAPI_ERR_INVAL;

    char name_buf[256];
    uint32_t copy_len = name_len < 255 ? name_len : 255;
    if (copy_len > 0) memcpy(name_buf, name, copy_len);
    name_buf[copy_len] = '\0';

    const char* val = getenv(name_buf);
    if (!val) return WAPI_ERR_NOENT;

    uint32_t val_slen = (uint32_t)strlen(val);
    wapi_wasm_write_u32(val_len_ptr, val_slen);
    uint32_t write_len = val_slen < buf_len ? val_slen : buf_len;
    if (write_len > 0) wapi_wasm_write_bytes(buf_ptr, val, write_len);
    return WAPI_OK;
}

/* ---- Random ---- */

static int32_t host_random_get(wasm_exec_env_t env,
                               uint32_t buf_ptr, uint32_t len) {
    (void)env;
    if (len == 0) return WAPI_OK;
    void* host_buf = wapi_wasm_ptr(buf_ptr, len);
    if (!host_buf) return WAPI_ERR_INVAL;

#ifdef _WIN32
    NTSTATUS st = BCryptGenRandom(NULL, (PUCHAR)host_buf, len,
                                   BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return BCRYPT_SUCCESS(st) ? WAPI_OK : WAPI_ERR_IO;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return WAPI_ERR_IO;
    size_t remaining = len;
    uint8_t* p = (uint8_t*)host_buf;
    while (remaining > 0) {
        ssize_t n = read(fd, p, remaining);
        if (n <= 0) { close(fd); return WAPI_ERR_IO; }
        p += n; remaining -= (size_t)n;
    }
    close(fd);
    return WAPI_OK;
#endif
}

/* ---- Process Control ---- */

static void host_exit(wasm_exec_env_t env, int32_t code) {
    (void)env;
    char msg[64];
    snprintf(msg, sizeof(msg), "wapi_env_exit(%d)", code);
    g_rt.running = false;
    wasm_runtime_set_exception(g_rt.module_inst, msg);
}

/* ---- Locale / Timezone ---- */

static void detect_locale(char* out, size_t out_size) {
#ifdef _WIN32
    wchar_t wbuf[LOCALE_NAME_MAX_LENGTH];
    int wn = GetUserDefaultLocaleName(wbuf, LOCALE_NAME_MAX_LENGTH);
    if (wn > 0) {
        int n = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, out, (int)out_size, NULL, NULL);
        if (n > 0) return;
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
        int n = WideCharToMultiByte(CP_UTF8, 0, tzi.TimeZoneKeyName, -1,
                                    out, (int)out_size, NULL, NULL);
        if (n > 0) return;
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
            while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r')) {
                out[--len] = '\0';
            }
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

static int32_t host_get_locale(wasm_exec_env_t env,
                               uint32_t buf_ptr, uint32_t buf_len,
                               uint32_t len_ptr) {
    (void)env;
    char locale[96];
    detect_locale(locale, sizeof(locale));
    uint32_t len = (uint32_t)strlen(locale);
    wapi_wasm_write_u32(len_ptr, len);
    uint32_t copy_len = len < buf_len ? len : buf_len;
    if (copy_len > 0) wapi_wasm_write_bytes(buf_ptr, locale, copy_len);
    return WAPI_OK;
}

static int32_t host_get_timezone(wasm_exec_env_t env,
                                 uint32_t buf_ptr, uint32_t buf_len,
                                 uint32_t len_ptr) {
    (void)env;
    char tz[128];
    detect_timezone(tz, sizeof(tz));
    uint32_t len = (uint32_t)strlen(tz);
    wapi_wasm_write_u32(len_ptr, len);
    uint32_t copy_len = len < buf_len ? len : buf_len;
    if (copy_len > 0) wapi_wasm_write_bytes(buf_ptr, tz, copy_len);
    return WAPI_OK;
}

/* ---- Errors ---- */

static int32_t host_get_error(wasm_exec_env_t env,
                              uint32_t buf_ptr, uint32_t buf_len,
                              uint32_t msg_len_ptr) {
    (void)env;
    uint32_t len = (uint32_t)strlen(g_rt.last_error);
    wapi_wasm_write_u32(msg_len_ptr, len);
    uint32_t copy_len = len < buf_len ? len : buf_len;
    if (copy_len > 0) wapi_wasm_write_bytes(buf_ptr, g_rt.last_error, copy_len);
    return WAPI_OK;
}

static NativeSymbol g_symbols[] = {
    { "args_count",    (void*)host_args_count,    "()i",     NULL },
    { "args_get",      (void*)host_args_get,      "(iiii)i", NULL },
    { "environ_count", (void*)host_environ_count, "()i",     NULL },
    { "environ_get",   (void*)host_environ_get,   "(iiii)i", NULL },
    { "getenv",        (void*)host_getenv,        "(iiiii)i", NULL },
    { "random_get",    (void*)host_random_get,    "(ii)i",   NULL },
    { "exit",          (void*)host_exit,          "(i)",     NULL },
    { "get_locale",    (void*)host_get_locale,    "(iii)i",  NULL },
    { "get_timezone",  (void*)host_get_timezone,  "(iii)i",  NULL },
    { "get_error",     (void*)host_get_error,     "(iii)i",  NULL },
};

wapi_cap_registration_t wapi_host_env_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_env",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
