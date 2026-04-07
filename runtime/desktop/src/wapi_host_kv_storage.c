/**
 * WAPI Desktop Runtime - Key-Value Storage
 *
 * Implements all wapi_kv.* imports using file-based storage.
 * One file per key in the kv_storage_path directory.
 * Key names are sanitized: non-alphanumeric characters become underscores.
 *
 * Import module: "wapi_kv"
 */

#include "wapi_host.h"

#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PATH_SEP '\\'
#define KV_MKDIR(p) _mkdir(p)
#else
#include <dirent.h>
#include <unistd.h>
#define PATH_SEP '/'
#define KV_MKDIR(p) mkdir(p, 0755)
#endif

/* ============================================================
 * Helpers
 * ============================================================ */

/* Sanitize a key name: replace non-alphanumeric chars with '_' */
static bool sanitize_key(const char* key, uint32_t key_len,
                         char* out, size_t out_cap) {
    if (key_len == 0 || key_len >= out_cap) return false;
    for (uint32_t i = 0; i < key_len; i++) {
        char c = key[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9')) {
            out[i] = c;
        } else {
            out[i] = '_';
        }
    }
    out[key_len] = '\0';
    return true;
}

/* Ensure kv_storage_path is initialized and the directory exists */
static bool ensure_kv_dir(void) {
    if (g_rt.kv_storage_path[0] == '\0') {
        /* Default: ".wapi_kv" in the current working directory */
        snprintf(g_rt.kv_storage_path, sizeof(g_rt.kv_storage_path), ".wapi_kv");
    }

    /* Try to create the directory (ignore error if already exists) */
    struct stat st;
    if (stat(g_rt.kv_storage_path, &st) != 0) {
        if (KV_MKDIR(g_rt.kv_storage_path) != 0 && errno != EEXIST) {
            return false;
        }
    }
    return true;
}

/* Build the full file path for a given sanitized key name */
static bool build_key_path(const char* sanitized_key,
                           char* out, size_t out_cap) {
    int written = snprintf(out, out_cap, "%s%c%s",
                           g_rt.kv_storage_path, PATH_SEP, sanitized_key);
    return (written > 0 && (size_t)written < out_cap);
}

/* ============================================================
 * kv.get
 * ============================================================
 * Wasm signature: (i32 key, i32 key_len, i32 buf, i32 buf_len, i32 val_len_ptr) -> i32
 */
static wasm_trap_t* host_kv_get(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t key_ptr     = WAPI_ARG_U32(0);
    uint32_t key_len     = WAPI_ARG_U32(1);
    uint32_t buf_ptr     = WAPI_ARG_U32(2);
    uint32_t buf_len     = WAPI_ARG_U32(3);
    uint32_t val_len_ptr = WAPI_ARG_U32(4);

    const char* key = wapi_wasm_read_string(key_ptr, key_len);
    if (!key) {
        wapi_set_error("kv_get: invalid key pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    if (!ensure_kv_dir()) {
        wapi_set_error("kv_get: failed to ensure storage directory");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    char sanitized[256];
    if (!sanitize_key(key, key_len, sanitized, sizeof(sanitized))) {
        wapi_set_error("kv_get: key too long or empty");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    char path[768];
    if (!build_key_path(sanitized, path, sizeof(path))) {
        wapi_set_error("kv_get: path too long");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        WAPI_RET_I32(WAPI_ERR_NOENT);
        return NULL;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(f);
        wapi_set_error("kv_get: failed to determine value size");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    /* Write actual value length regardless of buffer size */
    wapi_wasm_write_u32(val_len_ptr, (uint32_t)file_size);

    /* Copy as much as fits into the buffer */
    uint32_t copy_len = (uint32_t)file_size;
    if (copy_len > buf_len) copy_len = buf_len;

    if (copy_len > 0) {
        void* buf = wapi_wasm_ptr(buf_ptr, copy_len);
        if (!buf) {
            fclose(f);
            wapi_set_error("kv_get: invalid buffer pointer");
            WAPI_RET_I32(WAPI_ERR_INVAL);
            return NULL;
        }
        size_t read_bytes = fread(buf, 1, copy_len, f);
        if (read_bytes != copy_len) {
            fclose(f);
            wapi_set_error("kv_get: read error");
            WAPI_RET_I32(WAPI_ERR_IO);
            return NULL;
        }
    }

    fclose(f);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * kv.set
 * ============================================================
 * Wasm signature: (i32 key, i32 key_len, i32 value, i32 val_len) -> i32
 */
static wasm_trap_t* host_kv_set(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t key_ptr = WAPI_ARG_U32(0);
    uint32_t key_len = WAPI_ARG_U32(1);
    uint32_t val_ptr = WAPI_ARG_U32(2);
    uint32_t val_len = WAPI_ARG_U32(3);

    const char* key = wapi_wasm_read_string(key_ptr, key_len);
    if (!key) {
        wapi_set_error("kv_set: invalid key pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    const void* value = wapi_wasm_ptr(val_ptr, val_len);
    if (!value && val_len > 0) {
        wapi_set_error("kv_set: invalid value pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    if (!ensure_kv_dir()) {
        wapi_set_error("kv_set: failed to ensure storage directory");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    char sanitized[256];
    if (!sanitize_key(key, key_len, sanitized, sizeof(sanitized))) {
        wapi_set_error("kv_set: key too long or empty");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    char path[768];
    if (!build_key_path(sanitized, path, sizeof(path))) {
        wapi_set_error("kv_set: path too long");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        wapi_set_error("kv_set: failed to open file for writing");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    if (val_len > 0) {
        size_t written = fwrite(value, 1, val_len, f);
        if (written != val_len) {
            fclose(f);
            wapi_set_error("kv_set: write error");
            WAPI_RET_I32(WAPI_ERR_IO);
            return NULL;
        }
    }

    fclose(f);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * kv.delete
 * ============================================================
 * Wasm signature: (i32 key, i32 key_len) -> i32
 */
static wasm_trap_t* host_kv_delete(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t key_ptr = WAPI_ARG_U32(0);
    uint32_t key_len = WAPI_ARG_U32(1);

    const char* key = wapi_wasm_read_string(key_ptr, key_len);
    if (!key) {
        wapi_set_error("kv_delete: invalid key pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    if (!ensure_kv_dir()) {
        wapi_set_error("kv_delete: failed to ensure storage directory");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    char sanitized[256];
    if (!sanitize_key(key, key_len, sanitized, sizeof(sanitized))) {
        wapi_set_error("kv_delete: key too long or empty");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    char path[768];
    if (!build_key_path(sanitized, path, sizeof(path))) {
        wapi_set_error("kv_delete: path too long");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    if (remove(path) != 0) {
        if (errno == ENOENT) {
            WAPI_RET_I32(WAPI_ERR_NOENT);
        } else {
            wapi_set_error("kv_delete: remove() failed");
            WAPI_RET_I32(WAPI_ERR_IO);
        }
        return NULL;
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * kv.has
 * ============================================================
 * Wasm signature: (i32 key, i32 key_len) -> i32
 * Returns 1 if key exists, 0 if not.
 */
static wasm_trap_t* host_kv_has(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t key_ptr = WAPI_ARG_U32(0);
    uint32_t key_len = WAPI_ARG_U32(1);

    const char* key = wapi_wasm_read_string(key_ptr, key_len);
    if (!key) {
        WAPI_RET_I32(0);
        return NULL;
    }

    if (!ensure_kv_dir()) {
        WAPI_RET_I32(0);
        return NULL;
    }

    char sanitized[256];
    if (!sanitize_key(key, key_len, sanitized, sizeof(sanitized))) {
        WAPI_RET_I32(0);
        return NULL;
    }

    char path[768];
    if (!build_key_path(sanitized, path, sizeof(path))) {
        WAPI_RET_I32(0);
        return NULL;
    }

    FILE* f = fopen(path, "rb");
    if (f) {
        fclose(f);
        WAPI_RET_I32(1);
    } else {
        WAPI_RET_I32(0);
    }

    return NULL;
}

/* ============================================================
 * kv.clear
 * ============================================================
 * Wasm signature: () -> i32
 * Removes all files in the kv storage directory.
 */
static wasm_trap_t* host_kv_clear(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;

    if (!ensure_kv_dir()) {
        wapi_set_error("kv_clear: failed to ensure storage directory");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

#ifdef _WIN32
    char pattern[768];
    snprintf(pattern, sizeof(pattern), "%s\\*", g_rt.kv_storage_path);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        WAPI_RET_I32(WAPI_OK); /* Empty or no directory */
        return NULL;
    }

    do {
        /* Skip "." and ".." */
        if (fd.cFileName[0] == '.' &&
            (fd.cFileName[1] == '\0' ||
             (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0'))) {
            continue;
        }
        /* Skip subdirectories */
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        char filepath[768];
        snprintf(filepath, sizeof(filepath), "%s\\%s",
                 g_rt.kv_storage_path, fd.cFileName);
        remove(filepath);
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
#else
    DIR* dir = opendir(g_rt.kv_storage_path);
    if (!dir) {
        WAPI_RET_I32(WAPI_OK); /* Empty or no directory */
        return NULL;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip "." and ".." */
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        char filepath[768];
        snprintf(filepath, sizeof(filepath), "%s/%s",
                 g_rt.kv_storage_path, entry->d_name);
        remove(filepath);
    }

    closedir(dir);
#endif

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * kv.count
 * ============================================================
 * Wasm signature: () -> i32
 * Returns the number of stored keys.
 */
static wasm_trap_t* host_kv_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;

    if (!ensure_kv_dir()) {
        WAPI_RET_I32(0);
        return NULL;
    }

    int32_t count = 0;

#ifdef _WIN32
    char pattern[768];
    snprintf(pattern, sizeof(pattern), "%s\\*", g_rt.kv_storage_path);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        WAPI_RET_I32(0);
        return NULL;
    }

    do {
        if (fd.cFileName[0] == '.' &&
            (fd.cFileName[1] == '\0' ||
             (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0'))) {
            continue;
        }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        count++;
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
#else
    DIR* dir = opendir(g_rt.kv_storage_path);
    if (!dir) {
        WAPI_RET_I32(0);
        return NULL;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }
        count++;
    }

    closedir(dir);
#endif

    WAPI_RET_I32(count);
    return NULL;
}

/* ============================================================
 * kv.key_at
 * ============================================================
 * Wasm signature: (i32 index, i32 buf, i32 buf_len, i32 key_len_ptr) -> i32
 * Returns the filename (key) at the given index.
 */
static wasm_trap_t* host_kv_key_at(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  index       = WAPI_ARG_I32(0);
    uint32_t buf_ptr     = WAPI_ARG_U32(1);
    uint32_t buf_len     = WAPI_ARG_U32(2);
    uint32_t key_len_ptr = WAPI_ARG_U32(3);

    if (index < 0) {
        wapi_set_error("kv_key_at: negative index");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    if (!ensure_kv_dir()) {
        wapi_set_error("kv_key_at: failed to ensure storage directory");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    const char* found_name = NULL;
    int32_t current = 0;

#ifdef _WIN32
    char pattern[768];
    snprintf(pattern, sizeof(pattern), "%s\\*", g_rt.kv_storage_path);

    /* We need a static buffer since FindFirstFile returns pointers
       into fd which is stack-local */
    static char name_buf[256];

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        wapi_set_error("kv_key_at: index out of range");
        WAPI_RET_I32(WAPI_ERR_RANGE);
        return NULL;
    }

    do {
        if (fd.cFileName[0] == '.' &&
            (fd.cFileName[1] == '\0' ||
             (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0'))) {
            continue;
        }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        if (current == index) {
            size_t nlen = strlen(fd.cFileName);
            if (nlen >= sizeof(name_buf)) nlen = sizeof(name_buf) - 1;
            memcpy(name_buf, fd.cFileName, nlen);
            name_buf[nlen] = '\0';
            found_name = name_buf;
            break;
        }
        current++;
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
#else
    DIR* dir = opendir(g_rt.kv_storage_path);
    if (!dir) {
        wapi_set_error("kv_key_at: cannot open storage directory");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    static char name_buf[256];
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        if (current == index) {
            size_t nlen = strlen(entry->d_name);
            if (nlen >= sizeof(name_buf)) nlen = sizeof(name_buf) - 1;
            memcpy(name_buf, entry->d_name, nlen);
            name_buf[nlen] = '\0';
            found_name = name_buf;
            break;
        }
        current++;
    }

    closedir(dir);
#endif

    if (!found_name) {
        wapi_set_error("kv_key_at: index out of range");
        WAPI_RET_I32(WAPI_ERR_RANGE);
        return NULL;
    }

    uint32_t name_len = (uint32_t)strlen(found_name);
    wapi_wasm_write_u32(key_len_ptr, name_len);

    uint32_t copy_len = name_len;
    if (copy_len > buf_len) copy_len = buf_len;

    if (copy_len > 0) {
        void* buf = wapi_wasm_ptr(buf_ptr, copy_len);
        if (!buf) {
            wapi_set_error("kv_key_at: invalid buffer pointer");
            WAPI_RET_I32(WAPI_ERR_INVAL);
            return NULL;
        }
        memcpy(buf, found_name, copy_len);
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_kv(wasmtime_linker_t* linker) {
    /* Ensure kv storage path is set up on first registration */
    ensure_kv_dir();

    WAPI_DEFINE_5_1(linker, "wapi_kv", "get",    host_kv_get);
    WAPI_DEFINE_4_1(linker, "wapi_kv", "set",    host_kv_set);
    WAPI_DEFINE_2_1(linker, "wapi_kv", "delete", host_kv_delete);
    WAPI_DEFINE_2_1(linker, "wapi_kv", "has",    host_kv_has);
    WAPI_DEFINE_0_1(linker, "wapi_kv", "clear",  host_kv_clear);
    WAPI_DEFINE_0_1(linker, "wapi_kv", "count",  host_kv_count);
    WAPI_DEFINE_4_1(linker, "wapi_kv", "key_at", host_kv_key_at);
}
