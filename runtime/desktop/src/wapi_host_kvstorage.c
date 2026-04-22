/**
 * WAPI Desktop Runtime - Key-Value Storage
 *
 * Module: "wapi_kv" (per wapi_kvstorage.h).  File-backed storage:
 * one file per key under g_rt.kv_storage_path, with non-alphanumeric
 * characters in the key replaced by '_' to keep filenames safe.
 *
 * All key arguments arrive as a pointer to a 16-byte
 * wapi_stringview_t {u64 data, u64 length}.  Lengths use i64.
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

#define MOD "wapi_kv"

/* ============================================================
 * Helpers
 * ============================================================ */

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

static bool sanitize_key(const char* key, size_t key_len,
                         char* out, size_t out_cap) {
    if (key_len == 0 || key_len >= out_cap) return false;
    for (size_t i = 0; i < key_len; i++) {
        char c = key[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9'))
            out[i] = c;
        else
            out[i] = '_';
    }
    out[key_len] = '\0';
    return true;
}

static bool ensure_kv_dir(void) {
    if (g_rt.kv_storage_path[0] == '\0')
        snprintf(g_rt.kv_storage_path, sizeof(g_rt.kv_storage_path), ".wapi_kv");

    struct stat st;
    if (stat(g_rt.kv_storage_path, &st) != 0) {
        if (KV_MKDIR(g_rt.kv_storage_path) != 0 && errno != EEXIST) return false;
    }
    return true;
}

static bool build_key_path(const char* sanitized_key,
                           char* out, size_t out_cap) {
    int n = snprintf(out, out_cap, "%s%c%s",
                     g_rt.kv_storage_path, PATH_SEP, sanitized_key);
    return (n > 0 && (size_t)n < out_cap);
}

/* Resolve a stringview key to its on-disk file path.  Returns the
 * matching WAPI error code on failure, WAPI_OK on success. */
static int32_t resolve_key_path(uint32_t key_sv, char* path_out, size_t path_cap) {
    char key[256];
    int32_t klen = read_stringview(key_sv, key, sizeof(key));
    if (klen < 0) { wapi_set_error("kv: invalid key"); return WAPI_ERR_INVAL; }
    if (!ensure_kv_dir()) { wapi_set_error("kv: storage dir init failed"); return WAPI_ERR_IO; }

    char sanitized[256];
    if (!sanitize_key(key, (size_t)klen, sanitized, sizeof(sanitized))) {
        wapi_set_error("kv: key too long or empty");
        return WAPI_ERR_INVAL;
    }
    if (!build_key_path(sanitized, path_out, path_cap)) {
        wapi_set_error("kv: path too long");
        return WAPI_ERR_INVAL;
    }
    return WAPI_OK;
}

/* ============================================================
 * Imports
 * ============================================================ */

/* kv.get: (i32 key_sv, i32 buf, i64 buf_len, i32 val_len_out) -> i32 */
static wasm_trap_t* host_kv_get(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t key_sv  = WAPI_ARG_U32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint64_t buf_len = WAPI_ARG_U64(2);
    uint32_t len_ptr = WAPI_ARG_U32(3);

    char path[768];
    int32_t rc = resolve_key_path(key_sv, path, sizeof(path));
    if (rc != WAPI_OK) { WAPI_RET_I32(rc); return NULL; }

    FILE* f = fopen(path, "rb");
    if (!f) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < 0) { fclose(f); WAPI_RET_I32(WAPI_ERR_IO); return NULL; }

    wapi_wasm_write_u64(len_ptr, (uint64_t)fsize);

    uint64_t copy = (uint64_t)fsize < buf_len ? (uint64_t)fsize : buf_len;
    if (copy > 0) {
        void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)copy);
        if (!buf) { fclose(f); WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
        if (fread(buf, 1, (size_t)copy, f) != (size_t)copy) {
            fclose(f); WAPI_RET_I32(WAPI_ERR_IO); return NULL;
        }
    }
    fclose(f);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* kv.set: (i32 key_sv, i32 value, i64 val_len) -> i32 */
static wasm_trap_t* host_kv_set(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t key_sv  = WAPI_ARG_U32(0);
    uint32_t val_ptr = WAPI_ARG_U32(1);
    uint64_t val_len = WAPI_ARG_U64(2);

    char path[768];
    int32_t rc = resolve_key_path(key_sv, path, sizeof(path));
    if (rc != WAPI_OK) { WAPI_RET_I32(rc); return NULL; }

    const void* value = wapi_wasm_ptr(val_ptr, (uint32_t)val_len);
    if (!value && val_len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    FILE* f = fopen(path, "wb");
    if (!f) { WAPI_RET_I32(WAPI_ERR_IO); return NULL; }
    if (val_len > 0 && fwrite(value, 1, (size_t)val_len, f) != (size_t)val_len) {
        fclose(f); WAPI_RET_I32(WAPI_ERR_IO); return NULL;
    }
    fclose(f);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* kv.delete: (i32 key_sv) -> i32 */
static wasm_trap_t* host_kv_delete(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t key_sv = WAPI_ARG_U32(0);
    char path[768];
    int32_t rc = resolve_key_path(key_sv, path, sizeof(path));
    if (rc != WAPI_OK) { WAPI_RET_I32(rc); return NULL; }
    if (remove(path) != 0) {
        WAPI_RET_I32(errno == ENOENT ? WAPI_ERR_NOENT : WAPI_ERR_IO);
        return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* kv.has: (i32 key_sv) -> i32 (1=exists, 0=not) */
static wasm_trap_t* host_kv_has(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t key_sv = WAPI_ARG_U32(0);
    char path[768];
    if (resolve_key_path(key_sv, path, sizeof(path)) != WAPI_OK) { WAPI_RET_I32(0); return NULL; }
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); WAPI_RET_I32(1); } else WAPI_RET_I32(0);
    return NULL;
}

/* kv.clear: () -> i32 */
static wasm_trap_t* host_kv_clear(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    if (!ensure_kv_dir()) { WAPI_RET_I32(WAPI_ERR_IO); return NULL; }

#ifdef _WIN32
    char pattern[768];
    snprintf(pattern, sizeof(pattern), "%s\\*", g_rt.kv_storage_path);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) { WAPI_RET_I32(WAPI_OK); return NULL; }
    do {
        if (fd.cFileName[0] == '.' && (fd.cFileName[1] == '\0' ||
            (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0'))) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        char filepath[768];
        snprintf(filepath, sizeof(filepath), "%s\\%s", g_rt.kv_storage_path, fd.cFileName);
        remove(filepath);
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    DIR* dir = opendir(g_rt.kv_storage_path);
    if (!dir) { WAPI_RET_I32(WAPI_OK); return NULL; }
    struct dirent* e;
    while ((e = readdir(dir)) != NULL) {
        if (e->d_name[0] == '.' && (e->d_name[1] == '\0' ||
            (e->d_name[1] == '.' && e->d_name[2] == '\0'))) continue;
        char filepath[768];
        snprintf(filepath, sizeof(filepath), "%s/%s", g_rt.kv_storage_path, e->d_name);
        remove(filepath);
    }
    closedir(dir);
#endif
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* kv.count: () -> i32 */
static wasm_trap_t* host_kv_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    if (!ensure_kv_dir()) { WAPI_RET_I32(0); return NULL; }
    int32_t count = 0;

#ifdef _WIN32
    char pattern[768];
    snprintf(pattern, sizeof(pattern), "%s\\*", g_rt.kv_storage_path);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) { WAPI_RET_I32(0); return NULL; }
    do {
        if (fd.cFileName[0] == '.' && (fd.cFileName[1] == '\0' ||
            (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0'))) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        count++;
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    DIR* dir = opendir(g_rt.kv_storage_path);
    if (!dir) { WAPI_RET_I32(0); return NULL; }
    struct dirent* e;
    while ((e = readdir(dir)) != NULL) {
        if (e->d_name[0] == '.' && (e->d_name[1] == '\0' ||
            (e->d_name[1] == '.' && e->d_name[2] == '\0'))) continue;
        count++;
    }
    closedir(dir);
#endif
    WAPI_RET_I32(count);
    return NULL;
}

/* kv.key_at: (i32 index, i32 buf, i64 buf_len, i32 key_len_out) -> i32 */
static wasm_trap_t* host_kv_key_at(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  index   = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint64_t buf_len = WAPI_ARG_U64(2);
    uint32_t len_ptr = WAPI_ARG_U32(3);

    if (index < 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    if (!ensure_kv_dir()) { WAPI_RET_I32(WAPI_ERR_IO); return NULL; }

    char name_buf[256];
    name_buf[0] = '\0';
    int32_t current = 0;
    bool found = false;

#ifdef _WIN32
    char pattern[768];
    snprintf(pattern, sizeof(pattern), "%s\\*", g_rt.kv_storage_path);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; }
    do {
        if (fd.cFileName[0] == '.' && (fd.cFileName[1] == '\0' ||
            (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0'))) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (current == index) {
            size_t n = strnlen(fd.cFileName, sizeof(name_buf) - 1);
            memcpy(name_buf, fd.cFileName, n);
            name_buf[n] = '\0';
            found = true;
            break;
        }
        current++;
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    DIR* dir = opendir(g_rt.kv_storage_path);
    if (!dir) { WAPI_RET_I32(WAPI_ERR_IO); return NULL; }
    struct dirent* e;
    while ((e = readdir(dir)) != NULL) {
        if (e->d_name[0] == '.' && (e->d_name[1] == '\0' ||
            (e->d_name[1] == '.' && e->d_name[2] == '\0'))) continue;
        if (current == index) {
            size_t n = strnlen(e->d_name, sizeof(name_buf) - 1);
            memcpy(name_buf, e->d_name, n);
            name_buf[n] = '\0';
            found = true;
            break;
        }
        current++;
    }
    closedir(dir);
#endif

    if (!found) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; }
    uint64_t nlen = (uint64_t)strlen(name_buf);
    wapi_wasm_write_u64(len_ptr, nlen);
    uint64_t copy = nlen < buf_len ? nlen : buf_len;
    if (copy > 0) wapi_wasm_write_bytes(buf_ptr, name_buf, (uint32_t)copy);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_kv(wasmtime_linker_t* linker) {
    ensure_kv_dir();

    wapi_linker_define(linker, MOD, "get", host_kv_get,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, MOD, "set", host_kv_set,
        3, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, MOD, "delete", host_kv_delete);
    WAPI_DEFINE_1_1(linker, MOD, "has",    host_kv_has);
    WAPI_DEFINE_0_1(linker, MOD, "clear",  host_kv_clear);
    WAPI_DEFINE_0_1(linker, MOD, "count",  host_kv_count);
    wapi_linker_define(linker, MOD, "key_at", host_kv_key_at,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
}
