/**
 * WAPI Desktop Runtime - Environment and Process
 *
 * Implements: wapi_env.args_count, wapi_env.args_get,
 *             wapi_env.environ_count, wapi_env.environ_get,
 *             wapi_env.getenv, wapi_env.random_get,
 *             wapi_env.exit, wapi_env.get_error
 */

#include "wapi_host.h"

#ifdef _WIN32
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#endif

/* For environ on POSIX */
#ifdef _WIN32
/* On Windows we use GetEnvironmentStrings */
#include <windows.h>
#else
extern char** environ;
#endif

/* ============================================================
 * Command-Line Arguments
 * ============================================================ */

/* args_count: () -> i32 */
static wasm_trap_t* host_args_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(g_rt.app_argc);
    return NULL;
}

/* args_get: (i32 index, i32 buf, i32 buf_len, i32 arg_len_out) -> i32 */
static wasm_trap_t* host_args_get(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t index = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t buf_len = WAPI_ARG_U32(2);
    uint32_t arg_len_ptr = WAPI_ARG_U32(3);

    if (index < 0 || index >= g_rt.app_argc) {
        WAPI_RET_I32(WAPI_ERR_RANGE);
        return NULL;
    }

    const char* arg = g_rt.app_argv[index];
    uint32_t len = (uint32_t)strlen(arg);

    wapi_wasm_write_u32(arg_len_ptr, len);

    uint32_t copy_len = len < buf_len ? len : buf_len;
    if (copy_len > 0) {
        wapi_wasm_write_bytes(buf_ptr, arg, copy_len);
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Environment Variables
 * ============================================================ */

#ifdef _WIN32
/* Count environment variables on Windows */
static int count_environ(void) {
    LPCH env_block = GetEnvironmentStringsA();
    if (!env_block) return 0;
    int count = 0;
    const char* p = env_block;
    while (*p) {
        count++;
        p += strlen(p) + 1;
    }
    FreeEnvironmentStringsA(env_block);
    return count;
}

/* Get environment variable by index on Windows */
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
    if (environ) {
        while (environ[count]) count++;
    }
    return count;
}

static const char* get_environ_by_index(int index) {
    if (!environ) return NULL;
    int count = 0;
    while (environ[count]) {
        if (count == index) return environ[count];
        count++;
    }
    return NULL;
}
#endif

/* environ_count: () -> i32 */
static wasm_trap_t* host_environ_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(count_environ());
    return NULL;
}

/* environ_get: (i32 index, i32 buf, i32 buf_len, i32 var_len_out) -> i32 */
static wasm_trap_t* host_environ_get(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t index = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t buf_len = WAPI_ARG_U32(2);
    uint32_t var_len_ptr = WAPI_ARG_U32(3);

    const char* var = get_environ_by_index(index);
    if (!var) {
        WAPI_RET_I32(WAPI_ERR_RANGE);
        return NULL;
    }

    uint32_t len = (uint32_t)strlen(var);
    wapi_wasm_write_u32(var_len_ptr, len);

    uint32_t copy_len = len < buf_len ? len : buf_len;
    if (copy_len > 0) {
        wapi_wasm_write_bytes(buf_ptr, var, copy_len);
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* getenv: (i32 name, i32 name_len, i32 buf, i32 buf_len, i32 val_len_out) -> i32 */
static wasm_trap_t* host_getenv(
    void* env_p, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env_p; (void)caller;
    uint32_t name_ptr = WAPI_ARG_U32(0);
    uint32_t name_len = WAPI_ARG_U32(1);
    uint32_t buf_ptr = WAPI_ARG_U32(2);
    uint32_t buf_len = WAPI_ARG_U32(3);
    uint32_t val_len_ptr = WAPI_ARG_U32(4);

    const char* name = wapi_wasm_read_string(name_ptr, name_len);
    if (!name) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    /* Make null-terminated copy */
    char name_buf[256];
    uint32_t copy_len = name_len < 255 ? name_len : 255;
    memcpy(name_buf, name, copy_len);
    name_buf[copy_len] = '\0';

    const char* val = getenv(name_buf);
    if (!val) {
        WAPI_RET_I32(WAPI_ERR_NOENT);
        return NULL;
    }

    uint32_t val_slen = (uint32_t)strlen(val);
    wapi_wasm_write_u32(val_len_ptr, val_slen);

    uint32_t write_len = val_slen < buf_len ? val_slen : buf_len;
    if (write_len > 0) {
        wapi_wasm_write_bytes(buf_ptr, val, write_len);
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Cryptographic Random
 * ============================================================ */

/* random_get: (i32 buf, i32 len) -> i32 */
static wasm_trap_t* host_random_get(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t buf_ptr = WAPI_ARG_U32(0);
    uint32_t len = WAPI_ARG_U32(1);

    void* host_buf = wapi_wasm_ptr(buf_ptr, len);
    if (!host_buf && len > 0) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    if (len == 0) {
        WAPI_RET_I32(WAPI_OK);
        return NULL;
    }

#ifdef _WIN32
    NTSTATUS status = BCryptGenRandom(NULL, (PUCHAR)host_buf, len,
                                       BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status)) {
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }
#else
    /* Use /dev/urandom on Unix-like systems */
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }
    size_t remaining = len;
    uint8_t* p = (uint8_t*)host_buf;
    while (remaining > 0) {
        ssize_t n = read(fd, p, remaining);
        if (n <= 0) {
            close(fd);
            WAPI_RET_I32(WAPI_ERR_IO);
            return NULL;
        }
        p += n;
        remaining -= (size_t)n;
    }
    close(fd);
#endif

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
    (void)env; (void)caller;
    int32_t code = WAPI_ARG_I32(0);

    /* Create a trap to terminate Wasm execution */
    char msg[64];
    snprintf(msg, sizeof(msg), "wapi_env_exit(%d)", code);
    g_rt.running = false;
    return wasmtime_trap_new(msg, strlen(msg));
}

/* ============================================================
 * Error Messages
 * ============================================================ */

/* get_error: (i32 buf, i32 buf_len, i32 msg_len_out) -> i32 */
static wasm_trap_t* host_get_error(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t buf_ptr = WAPI_ARG_U32(0);
    uint32_t buf_len = WAPI_ARG_U32(1);
    uint32_t msg_len_ptr = WAPI_ARG_U32(2);

    uint32_t len = (uint32_t)strlen(g_rt.last_error);
    wapi_wasm_write_u32(msg_len_ptr, len);

    uint32_t copy_len = len < buf_len ? len : buf_len;
    if (copy_len > 0) {
        wapi_wasm_write_bytes(buf_ptr, g_rt.last_error, copy_len);
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_env(wasmtime_linker_t* linker) {
    WAPI_DEFINE_0_1(linker, "wapi_env", "args_count",    host_args_count);
    WAPI_DEFINE_4_1(linker, "wapi_env", "args_get",      host_args_get);
    WAPI_DEFINE_0_1(linker, "wapi_env", "environ_count", host_environ_count);
    WAPI_DEFINE_4_1(linker, "wapi_env", "environ_get",   host_environ_get);
    WAPI_DEFINE_5_1(linker, "wapi_env", "getenv",        host_getenv);
    WAPI_DEFINE_2_1(linker, "wapi_env", "random_get",    host_random_get);

    /* exit: (i32) -> void (but we trap, so define with no returns) */
    WAPI_DEFINE_1_0(linker, "wapi_env", "exit", host_exit);

    WAPI_DEFINE_3_1(linker, "wapi_env", "get_error", host_get_error);
}
