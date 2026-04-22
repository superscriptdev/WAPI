/**
 * WAPI Desktop Runtime - Filesystem
 *
 * Module: "wapi_filesystem" (per wapi_filesystem.h).
 *
 * All path arguments arrive as a pointer to a 16-byte
 * wapi_stringview_t {u64 data, u64 length}.  All byte-length params
 * are i64 (wapi_size_t).  Out-length pointers target u64.
 * Pre-opened directories are mapped from command-line args.
 * stdin/stdout/stderr are pre-mapped to handles 1/2/3.
 */

#include "wapi_host.h"

#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define PATH_SEP '\\'
#else
#include <dirent.h>
#include <unistd.h>
#define PATH_SEP '/'
#endif

#define MOD "wapi_filesystem"

/* ============================================================
 * Helpers
 * ============================================================ */

static wapi_result_t errno_to_tp(void) {
    switch (errno) {
    case ENOENT: return WAPI_ERR_NOENT;
    case EACCES: return WAPI_ERR_ACCES;
    case EEXIST: return WAPI_ERR_EXIST;
#ifdef ENOTDIR
    case ENOTDIR: return WAPI_ERR_NOTDIR;
#endif
#ifdef EISDIR
    case EISDIR: return WAPI_ERR_ISDIR;
#endif
#ifdef ENOSPC
    case ENOSPC: return WAPI_ERR_NOSPC;
#endif
    case ENOMEM: return WAPI_ERR_NOMEM;
#ifdef ENAMETOOLONG
    case ENAMETOOLONG: return WAPI_ERR_NAMETOOLONG;
#endif
#ifdef ENOTEMPTY
    case ENOTEMPTY: return WAPI_ERR_NOTEMPTY;
#endif
    case EIO:    return WAPI_ERR_IO;
    case EBADF:  return WAPI_ERR_BADF;
    case EINVAL: return WAPI_ERR_INVAL;
#ifdef EBUSY
    case EBUSY:  return WAPI_ERR_BUSY;
#endif
    default:     return WAPI_ERR_UNKNOWN;
    }
}

/* Read a wapi_stringview_t at sv_ptr into out (NUL-terminated).
 * Returns length written (not counting NUL), or -1 on invalid. */
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

/* Build a full host path from a pre-opened dir handle and a relative path */
static bool resolve_path(int32_t dir_fd, const char* rel_path,
                         char* out, size_t out_size) {
    if (!wapi_handle_valid(dir_fd, WAPI_HTYPE_PREOPEN_DIR) &&
        !wapi_handle_valid(dir_fd, WAPI_HTYPE_DIRECTORY)) {
        return false;
    }

    const char* base;
    if (g_rt.handles[dir_fd].type == WAPI_HTYPE_PREOPEN_DIR) {
        for (int i = 0; i < g_rt.preopen_count; i++) {
            if (g_rt.preopens[i].handle == dir_fd) {
                base = g_rt.preopens[i].host_path;
                goto found;
            }
        }
        return false;
    } else {
        base = g_rt.handles[dir_fd].data.dir.path;
    }

found:
    if (rel_path[0] == '\0') {
        snprintf(out, out_size, "%s", base);
    } else {
        /* Block absolute paths and ".." traversal */
        if (rel_path[0] == '/' || rel_path[0] == '\\') return false;
        if (strstr(rel_path, "..")) return false;
        snprintf(out, out_size, "%s%c%s", base, PATH_SEP, rel_path);
    }
    return true;
}

/* ============================================================
 * Pre-open Discovery
 * ============================================================ */

/* preopen_count: () -> i32 */
static wasm_trap_t* host_fs_preopen_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(g_rt.preopen_count);
    return NULL;
}

/* preopen_path: (i32 index, i32 buf, i64 buf_len, i32 path_len_out) -> i32 */
static wasm_trap_t* host_fs_preopen_path(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  index        = WAPI_ARG_I32(0);
    uint32_t buf_ptr      = WAPI_ARG_U32(1);
    uint64_t buf_len      = WAPI_ARG_U64(2);
    uint32_t path_len_ptr = WAPI_ARG_U32(3);

    if (index < 0 || index >= g_rt.preopen_count) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; }

    const char* path = g_rt.preopens[index].guest_path;
    uint64_t len = (uint64_t)strlen(path);
    wapi_wasm_write_u64(path_len_ptr, len);

    uint64_t copy = len < buf_len ? len : buf_len;
    if (copy > 0) wapi_wasm_write_bytes(buf_ptr, path, (uint32_t)copy);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* preopen_handle: (i32 index) -> i32 */
static wasm_trap_t* host_fs_preopen_handle(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t index = WAPI_ARG_I32(0);
    if (index < 0 || index >= g_rt.preopen_count) { WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(g_rt.preopens[index].handle);
    return NULL;
}

/* ============================================================
 * File Operations
 * ============================================================ */

/* open: (i32 dir_fd, i32 path_sv, i32 oflags, i32 fdflags, i32 fd_out) -> i32 */
static wasm_trap_t* host_fs_open(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  dir_fd     = WAPI_ARG_I32(0);
    uint32_t path_sv    = WAPI_ARG_U32(1);
    uint32_t oflags     = WAPI_ARG_U32(2);
    uint32_t fdflags    = WAPI_ARG_U32(3);
    uint32_t fd_out_ptr = WAPI_ARG_U32(4);

    char rel[512];
    if (read_stringview(path_sv, rel, sizeof(rel)) < 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    char host_path[1024];
    if (!resolve_path(dir_fd, rel, host_path, sizeof(host_path))) {
        wapi_set_error("Cannot resolve path");
        WAPI_RET_I32(WAPI_ERR_ACCES);
        return NULL;
    }

    if (oflags & 0x0002u) { /* DIRECTORY */
        int32_t handle = wapi_handle_alloc(WAPI_HTYPE_DIRECTORY);
        if (handle == 0) { WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }
        size_t plen = strlen(host_path);
        if (plen >= sizeof(g_rt.handles[handle].data.dir.path))
            plen = sizeof(g_rt.handles[handle].data.dir.path) - 1;
        memcpy(g_rt.handles[handle].data.dir.path, host_path, plen);
        g_rt.handles[handle].data.dir.path[plen] = '\0';
        wapi_wasm_write_i32(fd_out_ptr, handle);
        WAPI_RET_I32(WAPI_OK);
        return NULL;
    }

    const char* mode;
    if (oflags & 0x0001u) { /* CREATE */
        if (oflags & 0x0008u)       mode = (fdflags & 0x0001u) ? "ab"  : "wb";
        else if (oflags & 0x0004u)  mode = "wxb";
        else                        mode = (fdflags & 0x0001u) ? "ab"  : "r+b";
    } else if (oflags & 0x0008u)    mode = "wb";
    else                            mode = "rb";

    FILE* f = fopen(host_path, mode);
    if (!f && (oflags & 0x0001u) && !(oflags & 0x0004u)) {
        mode = (fdflags & 0x0001u) ? "ab" : "w+b";
        f = fopen(host_path, mode);
    }
    if (!f) {
        wapi_set_error(strerror(errno));
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }

    int32_t handle = wapi_handle_alloc(WAPI_HTYPE_FILE);
    if (handle == 0) { fclose(f); WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }
    g_rt.handles[handle].data.file = f;
    wapi_wasm_write_i32(fd_out_ptr, handle);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* read: (i32 fd, i32 buf, i64 len, i32 bytes_read_out) -> i32 */
static wasm_trap_t* host_fs_read(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  fd             = WAPI_ARG_I32(0);
    uint32_t buf_ptr        = WAPI_ARG_U32(1);
    uint64_t len            = WAPI_ARG_U64(2);
    uint32_t bytes_read_ptr = WAPI_ARG_U32(3);

    FILE* f;
    if (fd == 1) f = stdin;
    else if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    else f = g_rt.handles[fd].data.file;

    void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)len);
    if (!buf && len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    size_t n = fread(buf, 1, (size_t)len, f);
    wapi_wasm_write_u64(bytes_read_ptr, (uint64_t)n);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* write: (i32 fd, i32 buf, i64 len, i32 bytes_written_out) -> i32 */
static wasm_trap_t* host_fs_write(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  fd                = WAPI_ARG_I32(0);
    uint32_t buf_ptr           = WAPI_ARG_U32(1);
    uint64_t len               = WAPI_ARG_U64(2);
    uint32_t bytes_written_ptr = WAPI_ARG_U32(3);

    FILE* f;
    if      (fd == 2) f = stdout;
    else if (fd == 3) f = stderr;
    else if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    else f = g_rt.handles[fd].data.file;

    const void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)len);
    if (!buf && len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    size_t n = fwrite(buf, 1, (size_t)len, f);
    fflush(f);
    wapi_wasm_write_u64(bytes_written_ptr, (uint64_t)n);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* pread: (i32 fd, i32 buf, i64 len, i64 offset, i32 bytes_read_out) -> i32 */
static wasm_trap_t* host_fs_pread(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  fd             = WAPI_ARG_I32(0);
    uint32_t buf_ptr        = WAPI_ARG_U32(1);
    uint64_t len            = WAPI_ARG_U64(2);
    int64_t  offset         = WAPI_ARG_I64(3);
    uint32_t bytes_read_ptr = WAPI_ARG_U32(4);

    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    FILE* f = g_rt.handles[fd].data.file;
    void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)len);
    if (!buf && len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    long saved = ftell(f);
    if (fseek(f, (long)offset, SEEK_SET) != 0) { WAPI_RET_I32(errno_to_tp()); return NULL; }
    size_t n = fread(buf, 1, (size_t)len, f);
    fseek(f, saved, SEEK_SET);
    wapi_wasm_write_u64(bytes_read_ptr, (uint64_t)n);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* pwrite: (i32 fd, i32 buf, i64 len, i64 offset, i32 bytes_written_out) -> i32 */
static wasm_trap_t* host_fs_pwrite(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  fd                = WAPI_ARG_I32(0);
    uint32_t buf_ptr           = WAPI_ARG_U32(1);
    uint64_t len               = WAPI_ARG_U64(2);
    int64_t  offset            = WAPI_ARG_I64(3);
    uint32_t bytes_written_ptr = WAPI_ARG_U32(4);

    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    FILE* f = g_rt.handles[fd].data.file;
    const void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)len);
    if (!buf && len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    long saved = ftell(f);
    if (fseek(f, (long)offset, SEEK_SET) != 0) { WAPI_RET_I32(errno_to_tp()); return NULL; }
    size_t n = fwrite(buf, 1, (size_t)len, f);
    fseek(f, saved, SEEK_SET);
    wapi_wasm_write_u64(bytes_written_ptr, (uint64_t)n);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* seek: (i32 fd, i64 offset, i32 whence, i32 new_offset_out) -> i32 */
static wasm_trap_t* host_fs_seek(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  fd             = WAPI_ARG_I32(0);
    int64_t  offset         = WAPI_ARG_I64(1);
    int32_t  whence         = WAPI_ARG_I32(2);
    uint32_t new_offset_ptr = WAPI_ARG_U32(3);

    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    int c_whence;
    switch (whence) {
    case 0: c_whence = SEEK_SET; break;
    case 1: c_whence = SEEK_CUR; break;
    case 2: c_whence = SEEK_END; break;
    default: WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    FILE* f = g_rt.handles[fd].data.file;
    if (fseek(f, (long)offset, c_whence) != 0) { WAPI_RET_I32(errno_to_tp()); return NULL; }
    long pos = ftell(f);
    wapi_wasm_write_u64(new_offset_ptr, (uint64_t)pos);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* close: (i32 fd) -> i32 */
static wasm_trap_t* host_fs_close(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t fd = WAPI_ARG_I32(0);
    if (fd >= 1 && fd <= 3) { WAPI_RET_I32(WAPI_OK); return NULL; }
    if (wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
        fclose(g_rt.handles[fd].data.file);
        wapi_handle_free(fd);
        WAPI_RET_I32(WAPI_OK);
    } else if (wapi_handle_valid(fd, WAPI_HTYPE_DIRECTORY)) {
        wapi_handle_free(fd);
        WAPI_RET_I32(WAPI_OK);
    } else {
        WAPI_RET_I32(WAPI_ERR_BADF);
    }
    return NULL;
}

/* sync: (i32 fd) -> i32 */
static wasm_trap_t* host_fs_sync(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t fd = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    fflush(g_rt.handles[fd].data.file);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* Fill a wapi_filestat_t (56B) buffer from a stat struct. */
#ifdef _WIN32
static void fill_stat(uint8_t* buf, const struct _stat64* st) {
#else
static void fill_stat(uint8_t* buf, const struct stat* st) {
#endif
    memset(buf, 0, 56);
    uint64_t dev = (uint64_t)st->st_dev;
    uint64_t ino = (uint64_t)st->st_ino;
    uint32_t filetype = 4; /* REGULAR_FILE */
#ifdef _WIN32
    if (st->st_mode & _S_IFDIR) filetype = 3;
#else
    if      (S_ISDIR(st->st_mode)) filetype = 3;
    else if (S_ISLNK(st->st_mode)) filetype = 7;
    else if (S_ISBLK(st->st_mode)) filetype = 1;
    else if (S_ISCHR(st->st_mode)) filetype = 2;
#endif
    uint64_t nlink = (uint64_t)st->st_nlink;
    uint64_t size  = (uint64_t)st->st_size;
    uint64_t atim  = (uint64_t)st->st_atime * 1000000000ULL;
    uint64_t mtim  = (uint64_t)st->st_mtime * 1000000000ULL;
    memcpy(buf +  0, &dev,      8);
    memcpy(buf +  8, &ino,      8);
    memcpy(buf + 16, &filetype, 4);
    memcpy(buf + 24, &nlink,    8);
    memcpy(buf + 32, &size,     8);
    memcpy(buf + 40, &atim,     8);
    memcpy(buf + 48, &mtim,     8);
}

/* stat: (i32 fd, i32 stat_ptr) -> i32 */
static wasm_trap_t* host_fs_stat(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  fd       = WAPI_ARG_I32(0);
    uint32_t stat_ptr = WAPI_ARG_U32(1);
    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    FILE* f = g_rt.handles[fd].data.file;
#ifdef _WIN32
    struct _stat64 st;
    if (_fstat64(_fileno(f), &st) != 0) { WAPI_RET_I32(errno_to_tp()); return NULL; }
#else
    struct stat st;
    if (fstat(fileno(f), &st) != 0) { WAPI_RET_I32(errno_to_tp()); return NULL; }
#endif
    uint8_t buf[56];
    fill_stat(buf, &st);
    wapi_wasm_write_bytes(stat_ptr, buf, 56);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* path_stat: (i32 dir_fd, i32 path_sv, i32 stat_ptr) -> i32 */
static wasm_trap_t* host_fs_path_stat(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  dir_fd   = WAPI_ARG_I32(0);
    uint32_t path_sv  = WAPI_ARG_U32(1);
    uint32_t stat_ptr = WAPI_ARG_U32(2);

    char rel[512];
    if (read_stringview(path_sv, rel, sizeof(rel)) < 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    char host_path[1024];
    if (!resolve_path(dir_fd, rel, host_path, sizeof(host_path))) { WAPI_RET_I32(WAPI_ERR_ACCES); return NULL; }

#ifdef _WIN32
    struct _stat64 st;
    if (_stat64(host_path, &st) != 0) { WAPI_RET_I32(errno_to_tp()); return NULL; }
#else
    struct stat st;
    if (stat(host_path, &st) != 0) { WAPI_RET_I32(errno_to_tp()); return NULL; }
#endif
    uint8_t buf[56];
    fill_stat(buf, &st);
    wapi_wasm_write_bytes(stat_ptr, buf, 56);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* set_size: (i32 fd, i64 size) -> i32 */
static wasm_trap_t* host_fs_set_size(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t fd   = WAPI_ARG_I32(0);
    int64_t size = WAPI_ARG_I64(1);
    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    FILE* f = g_rt.handles[fd].data.file;
    fflush(f);
#ifdef _WIN32
    if (_chsize_s(_fileno(f), size) != 0) { WAPI_RET_I32(errno_to_tp()); return NULL; }
#else
    if (ftruncate(fileno(f), (off_t)size) != 0) { WAPI_RET_I32(errno_to_tp()); return NULL; }
#endif
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* mkdir / rmdir / unlink all share a path-only pattern. */
#define PATH_CMD_IMPL(NAME, CALL_EXPR) \
    static wasm_trap_t* host_fs_##NAME( \
        void* env, wasmtime_caller_t* caller, \
        const wasmtime_val_t* args, size_t nargs, \
        wasmtime_val_t* results, size_t nresults) { \
        (void)env; (void)caller; (void)nargs; (void)nresults; \
        int32_t  dir_fd  = WAPI_ARG_I32(0); \
        uint32_t path_sv = WAPI_ARG_U32(1); \
        char rel[512]; \
        if (read_stringview(path_sv, rel, sizeof(rel)) < 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; } \
        char host_path[1024]; \
        if (!resolve_path(dir_fd, rel, host_path, sizeof(host_path))) { WAPI_RET_I32(WAPI_ERR_ACCES); return NULL; } \
        if ((CALL_EXPR) != 0) { WAPI_RET_I32(errno_to_tp()); return NULL; } \
        WAPI_RET_I32(WAPI_OK); \
        return NULL; \
    }

#ifdef _WIN32
PATH_CMD_IMPL(mkdir,  _mkdir(host_path))
PATH_CMD_IMPL(rmdir,  _rmdir(host_path))
#else
PATH_CMD_IMPL(mkdir,  mkdir(host_path, 0755))
PATH_CMD_IMPL(rmdir,  rmdir(host_path))
#endif
PATH_CMD_IMPL(unlink, remove(host_path))

/* rename: (i32 old_dir, i32 old_path_sv, i32 new_dir, i32 new_path_sv) -> i32 */
static wasm_trap_t* host_fs_rename(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  old_dir     = WAPI_ARG_I32(0);
    uint32_t old_path_sv = WAPI_ARG_U32(1);
    int32_t  new_dir     = WAPI_ARG_I32(2);
    uint32_t new_path_sv = WAPI_ARG_U32(3);

    char old_rel[512], new_rel[512];
    if (read_stringview(old_path_sv, old_rel, sizeof(old_rel)) < 0 ||
        read_stringview(new_path_sv, new_rel, sizeof(new_rel)) < 0) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    char old_host[1024], new_host[1024];
    if (!resolve_path(old_dir, old_rel, old_host, sizeof(old_host)) ||
        !resolve_path(new_dir, new_rel, new_host, sizeof(new_host))) {
        WAPI_RET_I32(WAPI_ERR_ACCES); return NULL;
    }
    if (rename(old_host, new_host) != 0) { WAPI_RET_I32(errno_to_tp()); return NULL; }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* readdir: (i32 fd, i32 buf, i64 buf_len, i64 cookie, i32 used_out) -> i32 */
static wasm_trap_t* host_fs_readdir(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  fd       = WAPI_ARG_I32(0);
    uint32_t buf_ptr  = WAPI_ARG_U32(1);
    uint64_t buf_len  = WAPI_ARG_U64(2);
    uint64_t cookie   = WAPI_ARG_U64(3);
    uint32_t used_ptr = WAPI_ARG_U32(4);

    const char* dir_path = NULL;
    if (wapi_handle_valid(fd, WAPI_HTYPE_DIRECTORY)) {
        dir_path = g_rt.handles[fd].data.dir.path;
    } else if (wapi_handle_valid(fd, WAPI_HTYPE_PREOPEN_DIR)) {
        for (int i = 0; i < g_rt.preopen_count; i++) {
            if (g_rt.preopens[i].handle == fd) { dir_path = g_rt.preopens[i].host_path; break; }
        }
    }
    if (!dir_path) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }

    void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)buf_len);
    if (!buf && buf_len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    uint64_t used = 0;
    uint64_t entry_index = 0;

#ifdef _WIN32
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(search_path, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        wapi_wasm_write_u64(used_ptr, 0);
        WAPI_RET_I32(WAPI_OK);
        return NULL;
    }
    do {
        if (entry_index < cookie) { entry_index++; continue; }
        uint32_t namlen = (uint32_t)strlen(ffd.cFileName);
        uint64_t entry_size = 24 + (uint64_t)namlen;
        if (used + entry_size > buf_len) break;
        uint8_t entry[24];
        uint64_t next_cookie = entry_index + 1;
        uint64_t ino = 0;
        uint32_t type = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 3 : 4;
        memcpy(entry +  0, &next_cookie, 8);
        memcpy(entry +  8, &ino,         8);
        memcpy(entry + 16, &namlen,      4);
        memcpy(entry + 20, &type,        4);
        wapi_wasm_write_bytes(buf_ptr + (uint32_t)used,       entry,         24);
        wapi_wasm_write_bytes(buf_ptr + (uint32_t)used + 24,  ffd.cFileName, namlen);
        used += entry_size;
        entry_index++;
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    DIR* dir = opendir(dir_path);
    if (!dir) {
        wapi_wasm_write_u64(used_ptr, 0);
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }
    struct dirent* de;
    while ((de = readdir(dir)) != NULL) {
        if (entry_index < cookie) { entry_index++; continue; }
        uint32_t namlen = (uint32_t)strlen(de->d_name);
        uint64_t entry_size = 24 + (uint64_t)namlen;
        if (used + entry_size > buf_len) break;
        uint8_t entry[24];
        uint64_t next_cookie = entry_index + 1;
        uint64_t ino = (uint64_t)de->d_ino;
        uint32_t type = 0;
        switch (de->d_type) {
        case DT_DIR: type = 3; break;
        case DT_REG: type = 4; break;
        case DT_LNK: type = 7; break;
        case DT_BLK: type = 1; break;
        case DT_CHR: type = 2; break;
        default:     type = 0; break;
        }
        memcpy(entry +  0, &next_cookie, 8);
        memcpy(entry +  8, &ino,         8);
        memcpy(entry + 16, &namlen,      4);
        memcpy(entry + 20, &type,        4);
        wapi_wasm_write_bytes(buf_ptr + (uint32_t)used,      entry,      24);
        wapi_wasm_write_bytes(buf_ptr + (uint32_t)used + 24, de->d_name, namlen);
        used += entry_size;
        entry_index++;
    }
    closedir(dir);
#endif

    wapi_wasm_write_u64(used_ptr, used);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_fs(wasmtime_linker_t* linker) {
    WAPI_DEFINE_0_1(linker, MOD, "preopen_count",  host_fs_preopen_count);
    wapi_linker_define(linker, MOD, "preopen_path", host_fs_preopen_path,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, MOD, "preopen_handle", host_fs_preopen_handle);

    WAPI_DEFINE_5_1(linker, MOD, "open", host_fs_open);

    wapi_linker_define(linker, MOD, "read", host_fs_read,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, MOD, "write", host_fs_write,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, MOD, "pread", host_fs_pread,
        5, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, MOD, "pwrite", host_fs_pwrite,
        5, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, MOD, "seek", host_fs_seek,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I64,WASM_I32,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_1_1(linker, MOD, "close",     host_fs_close);
    WAPI_DEFINE_1_1(linker, MOD, "sync",      host_fs_sync);
    WAPI_DEFINE_2_1(linker, MOD, "stat",      host_fs_stat);
    WAPI_DEFINE_3_1(linker, MOD, "path_stat", host_fs_path_stat);

    wapi_linker_define(linker, MOD, "set_size", host_fs_set_size,
        2, (wasm_valkind_t[]){WASM_I32,WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_2_1(linker, MOD, "mkdir",  host_fs_mkdir);
    WAPI_DEFINE_2_1(linker, MOD, "rmdir",  host_fs_rmdir);
    WAPI_DEFINE_2_1(linker, MOD, "unlink", host_fs_unlink);
    WAPI_DEFINE_4_1(linker, MOD, "rename", host_fs_rename);

    wapi_linker_define(linker, MOD, "readdir", host_fs_readdir,
        5, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
}
