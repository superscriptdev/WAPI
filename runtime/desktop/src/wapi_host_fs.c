/**
 * WAPI Desktop Runtime - Filesystem
 *
 * Implements all wapi_fs.* imports using standard C/POSIX I/O.
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

/* Build a full host path from a pre-opened dir handle and a relative path */
static bool resolve_path(int32_t dir_fd, const char* rel_path, uint32_t rel_len,
                          char* out, size_t out_size) {
    if (!wapi_handle_valid(dir_fd, WAPI_HTYPE_PREOPEN_DIR) &&
        !wapi_handle_valid(dir_fd, WAPI_HTYPE_DIRECTORY)) {
        return false;
    }

    const char* base;
    if (g_rt.handles[dir_fd].type == WAPI_HTYPE_PREOPEN_DIR) {
        /* Find the preopen entry */
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
    if (rel_len == 0) {
        snprintf(out, out_size, "%s", base);
    } else {
        /* Validate: no ".." path traversal */
        /* Simple check: block if rel_path starts with / or contains .. */
        char rel_buf[512];
        uint32_t copy = rel_len < 511 ? rel_len : 511;
        memcpy(rel_buf, rel_path, copy);
        rel_buf[copy] = '\0';

        if (rel_buf[0] == '/' || rel_buf[0] == '\\') return false;
        if (strstr(rel_buf, "..")) return false;

        snprintf(out, out_size, "%s%c%s", base, PATH_SEP, rel_buf);
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
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(g_rt.preopen_count);
    return NULL;
}

/* preopen_path: (i32 index, i32 buf, i32 buf_len, i32 path_len_out) -> i32 */
static wasm_trap_t* host_fs_preopen_path(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t index = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t buf_len = WAPI_ARG_U32(2);
    uint32_t path_len_ptr = WAPI_ARG_U32(3);

    if (index < 0 || index >= g_rt.preopen_count) {
        WAPI_RET_I32(WAPI_ERR_RANGE);
        return NULL;
    }

    const char* path = g_rt.preopens[index].guest_path;
    uint32_t len = (uint32_t)strlen(path);
    wapi_wasm_write_u32(path_len_ptr, len);

    uint32_t copy_len = len < buf_len ? len : buf_len;
    if (copy_len > 0) {
        wapi_wasm_write_bytes(buf_ptr, path, copy_len);
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* preopen_handle: (i32 index) -> i32 */
static wasm_trap_t* host_fs_preopen_handle(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t index = WAPI_ARG_I32(0);

    if (index < 0 || index >= g_rt.preopen_count) {
        WAPI_RET_I32(0); /* WAPI_HANDLE_INVALID */
        return NULL;
    }

    WAPI_RET_I32(g_rt.preopens[index].handle);
    return NULL;
}

/* ============================================================
 * File Operations
 * ============================================================ */

/* open: (i32 dir_fd, i32 path, i32 path_len, i32 oflags, i32 fdflags, i32 fd_out) -> i32 */
static wasm_trap_t* host_fs_open(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t dir_fd = WAPI_ARG_I32(0);
    uint32_t path_ptr = WAPI_ARG_U32(1);
    uint32_t path_len = WAPI_ARG_U32(2);
    uint32_t oflags = WAPI_ARG_U32(3);
    uint32_t fdflags = WAPI_ARG_U32(4);
    uint32_t fd_out_ptr = WAPI_ARG_U32(5);

    const char* rel_path = wapi_wasm_read_string(path_ptr, path_len);
    if (!rel_path) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    char host_path[1024];
    if (!resolve_path(dir_fd, rel_path, path_len, host_path, sizeof(host_path))) {
        wapi_set_error("Cannot resolve path");
        WAPI_RET_I32(WAPI_ERR_ACCES);
        return NULL;
    }

    /* Check if opening a directory */
    if (oflags & 0x0002) { /* WAPI_FS_OFLAG_DIRECTORY */
        int32_t handle = wapi_handle_alloc(WAPI_HTYPE_DIRECTORY);
        if (handle == 0) {
            WAPI_RET_I32(WAPI_ERR_NOMEM);
            return NULL;
        }
        size_t plen = strlen(host_path);
        if (plen >= sizeof(g_rt.handles[handle].data.dir.path))
            plen = sizeof(g_rt.handles[handle].data.dir.path) - 1;
        memcpy(g_rt.handles[handle].data.dir.path, host_path, plen);
        g_rt.handles[handle].data.dir.path[plen] = '\0';
        wapi_wasm_write_i32(fd_out_ptr, handle);
        WAPI_RET_I32(WAPI_OK);
        return NULL;
    }

    /* Build fopen mode string */
    const char* mode;
    if (oflags & 0x0001) { /* CREATE */
        if (oflags & 0x0008) { /* TRUNC */
            mode = (fdflags & 0x0001) ? "ab" : "wb"; /* APPEND */
        } else if (oflags & 0x0004) { /* EXCL */
            mode = "wxb";
        } else {
            mode = (fdflags & 0x0001) ? "ab" : "r+b";
            /* If r+ fails, fall back to w+ */
        }
    } else if (oflags & 0x0008) { /* TRUNC */
        mode = "wb";
    } else {
        mode = "rb";
    }

    FILE* f = fopen(host_path, mode);
    if (!f && (oflags & 0x0001) && !(oflags & 0x0004)) {
        /* CREATE without EXCL: file might not exist, try "w+b" */
        mode = (fdflags & 0x0001) ? "ab" : "w+b";
        f = fopen(host_path, mode);
    }
    if (!f) {
        wapi_set_error(strerror(errno));
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }

    int32_t handle = wapi_handle_alloc(WAPI_HTYPE_FILE);
    if (handle == 0) {
        fclose(f);
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    g_rt.handles[handle].data.file = f;
    wapi_wasm_write_i32(fd_out_ptr, handle);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* read: (i32 fd, i32 buf, i32 len, i32 bytes_read_out) -> i32 */
static wasm_trap_t* host_fs_read(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t fd = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t len = WAPI_ARG_U32(2);
    uint32_t bytes_read_ptr = WAPI_ARG_U32(3);

    /* Handle stdin */
    FILE* f;
    if (fd == 1) { /* WAPI_STDIN */
        f = stdin;
    } else if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    } else {
        f = g_rt.handles[fd].data.file;
    }

    void* buf = wapi_wasm_ptr(buf_ptr, len);
    if (!buf && len > 0) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    size_t n = fread(buf, 1, len, f);
    wapi_wasm_write_u32(bytes_read_ptr, (uint32_t)n);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* write: (i32 fd, i32 buf, i32 len, i32 bytes_written_out) -> i32 */
static wasm_trap_t* host_fs_write(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t fd = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t len = WAPI_ARG_U32(2);
    uint32_t bytes_written_ptr = WAPI_ARG_U32(3);

    FILE* f;
    if (fd == 2) { /* WAPI_STDOUT */
        f = stdout;
    } else if (fd == 3) { /* WAPI_STDERR */
        f = stderr;
    } else if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    } else {
        f = g_rt.handles[fd].data.file;
    }

    const void* buf = wapi_wasm_ptr(buf_ptr, len);
    if (!buf && len > 0) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    size_t n = fwrite(buf, 1, len, f);
    fflush(f);
    wapi_wasm_write_u32(bytes_written_ptr, (uint32_t)n);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* pread: (i32 fd, i32 buf, i32 len, i64 offset, i32 bytes_read_out) -> i32 */
static wasm_trap_t* host_fs_pread(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t fd = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t len = WAPI_ARG_U32(2);
    int64_t offset = WAPI_ARG_I64(3);
    uint32_t bytes_read_ptr = WAPI_ARG_U32(4);

    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    FILE* f = g_rt.handles[fd].data.file;
    void* buf = wapi_wasm_ptr(buf_ptr, len);
    if (!buf && len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    /* Save current position, seek, read, restore */
    long saved = ftell(f);
    if (fseek(f, (long)offset, SEEK_SET) != 0) {
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }
    size_t n = fread(buf, 1, len, f);
    fseek(f, saved, SEEK_SET);

    wapi_wasm_write_u32(bytes_read_ptr, (uint32_t)n);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* pwrite: (i32 fd, i32 buf, i32 len, i64 offset, i32 bytes_written_out) -> i32 */
static wasm_trap_t* host_fs_pwrite(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t fd = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t len = WAPI_ARG_U32(2);
    int64_t offset = WAPI_ARG_I64(3);
    uint32_t bytes_written_ptr = WAPI_ARG_U32(4);

    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    FILE* f = g_rt.handles[fd].data.file;
    const void* buf = wapi_wasm_ptr(buf_ptr, len);
    if (!buf && len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    long saved = ftell(f);
    if (fseek(f, (long)offset, SEEK_SET) != 0) {
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }
    size_t n = fwrite(buf, 1, len, f);
    fseek(f, saved, SEEK_SET);

    wapi_wasm_write_u32(bytes_written_ptr, (uint32_t)n);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* seek: (i32 fd, i64 offset, i32 whence, i32 new_offset_out) -> i32 */
static wasm_trap_t* host_fs_seek(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t fd = WAPI_ARG_I32(0);
    int64_t offset = WAPI_ARG_I64(1);
    int32_t whence = WAPI_ARG_I32(2);
    uint32_t new_offset_ptr = WAPI_ARG_U32(3);

    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    int c_whence;
    switch (whence) {
    case 0: c_whence = SEEK_SET; break;
    case 1: c_whence = SEEK_CUR; break;
    case 2: c_whence = SEEK_END; break;
    default: WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }

    FILE* f = g_rt.handles[fd].data.file;
    if (fseek(f, (long)offset, c_whence) != 0) {
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }

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
    (void)env; (void)caller;
    int32_t fd = WAPI_ARG_I32(0);

    /* Don't close stdin/stdout/stderr */
    if (fd >= 1 && fd <= 3) {
        WAPI_RET_I32(WAPI_OK);
        return NULL;
    }

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
    (void)env; (void)caller;
    int32_t fd = WAPI_ARG_I32(0);

    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }
    fflush(g_rt.handles[fd].data.file);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* stat: (i32 fd, i32 stat_ptr) -> i32 */
static wasm_trap_t* host_fs_stat(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t fd = WAPI_ARG_I32(0);
    uint32_t stat_ptr = WAPI_ARG_U32(1);

    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    FILE* f = g_rt.handles[fd].data.file;
    int fileno_val;
#ifdef _WIN32
    fileno_val = _fileno(f);
    struct _stat64 st;
    if (_fstat64(fileno_val, &st) != 0) {
#else
    fileno_val = fileno(f);
    struct stat st;
    if (fstat(fileno_val, &st) != 0) {
#endif
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }

    /* wapi_filestat_t (56 bytes):
     *   +0:  u64 dev
     *   +8:  u64 ino
     *  +16:  u32 filetype
     *  +20:  u32 _pad
     *  +24:  u64 nlink
     *  +32:  u64 size
     *  +40:  u64 atim
     *  +48:  u64 mtim
     */
    uint8_t stat_buf[56];
    memset(stat_buf, 0, 56);

    uint64_t dev = (uint64_t)st.st_dev;
    uint64_t ino = (uint64_t)st.st_ino;
    uint32_t filetype = 4; /* WAPI_FILETYPE_REGULAR_FILE */
#ifdef _WIN32
    if (st.st_mode & _S_IFDIR) filetype = 3;
#else
    if (S_ISDIR(st.st_mode)) filetype = 3;
    else if (S_ISLNK(st.st_mode)) filetype = 7;
    else if (S_ISBLK(st.st_mode)) filetype = 1;
    else if (S_ISCHR(st.st_mode)) filetype = 2;
#endif
    uint64_t nlink = (uint64_t)st.st_nlink;
    uint64_t size = (uint64_t)st.st_size;
    uint64_t atim = (uint64_t)st.st_atime * 1000000000ULL;
    uint64_t mtim = (uint64_t)st.st_mtime * 1000000000ULL;

    memcpy(stat_buf + 0, &dev, 8);
    memcpy(stat_buf + 8, &ino, 8);
    memcpy(stat_buf + 16, &filetype, 4);
    memcpy(stat_buf + 24, &nlink, 8);
    memcpy(stat_buf + 32, &size, 8);
    memcpy(stat_buf + 40, &atim, 8);
    memcpy(stat_buf + 48, &mtim, 8);

    wapi_wasm_write_bytes(stat_ptr, stat_buf, 56);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* path_stat: (i32 dir_fd, i32 path, i32 path_len, i32 stat_ptr) -> i32 */
static wasm_trap_t* host_fs_path_stat(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t dir_fd = WAPI_ARG_I32(0);
    uint32_t path_ptr = WAPI_ARG_U32(1);
    uint32_t path_len = WAPI_ARG_U32(2);
    uint32_t stat_ptr = WAPI_ARG_U32(3);

    const char* rel_path = wapi_wasm_read_string(path_ptr, path_len);
    if (!rel_path) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    char host_path[1024];
    if (!resolve_path(dir_fd, rel_path, path_len, host_path, sizeof(host_path))) {
        WAPI_RET_I32(WAPI_ERR_ACCES);
        return NULL;
    }

#ifdef _WIN32
    struct _stat64 st;
    if (_stat64(host_path, &st) != 0) {
#else
    struct stat st;
    if (stat(host_path, &st) != 0) {
#endif
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }

    uint8_t stat_buf[56];
    memset(stat_buf, 0, 56);

    uint64_t dev = (uint64_t)st.st_dev;
    uint64_t ino = (uint64_t)st.st_ino;
    uint32_t filetype = 4;
#ifdef _WIN32
    if (st.st_mode & _S_IFDIR) filetype = 3;
#else
    if (S_ISDIR(st.st_mode)) filetype = 3;
    else if (S_ISLNK(st.st_mode)) filetype = 7;
#endif
    uint64_t nlink = (uint64_t)st.st_nlink;
    uint64_t size = (uint64_t)st.st_size;
    uint64_t atim = (uint64_t)st.st_atime * 1000000000ULL;
    uint64_t mtim = (uint64_t)st.st_mtime * 1000000000ULL;

    memcpy(stat_buf + 0, &dev, 8);
    memcpy(stat_buf + 8, &ino, 8);
    memcpy(stat_buf + 16, &filetype, 4);
    memcpy(stat_buf + 24, &nlink, 8);
    memcpy(stat_buf + 32, &size, 8);
    memcpy(stat_buf + 40, &atim, 8);
    memcpy(stat_buf + 48, &mtim, 8);

    wapi_wasm_write_bytes(stat_ptr, stat_buf, 56);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* set_size: (i32 fd, i64 size) -> i32 */
static wasm_trap_t* host_fs_set_size(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t fd = WAPI_ARG_I32(0);
    int64_t size = WAPI_ARG_I64(1);

    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    FILE* f = g_rt.handles[fd].data.file;
    fflush(f);
#ifdef _WIN32
    int result = _chsize_s(_fileno(f), size);
    if (result != 0) {
#else
    int result = ftruncate(fileno(f), (off_t)size);
    if (result != 0) {
#endif
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* mkdir: (i32 dir_fd, i32 path, i32 path_len) -> i32 */
static wasm_trap_t* host_fs_mkdir(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t dir_fd = WAPI_ARG_I32(0);
    uint32_t path_ptr = WAPI_ARG_U32(1);
    uint32_t path_len = WAPI_ARG_U32(2);

    const char* rel = wapi_wasm_read_string(path_ptr, path_len);
    if (!rel) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    char host_path[1024];
    if (!resolve_path(dir_fd, rel, path_len, host_path, sizeof(host_path))) {
        WAPI_RET_I32(WAPI_ERR_ACCES);
        return NULL;
    }

#ifdef _WIN32
    if (_mkdir(host_path) != 0) {
#else
    if (mkdir(host_path, 0755) != 0) {
#endif
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* rmdir: (i32 dir_fd, i32 path, i32 path_len) -> i32 */
static wasm_trap_t* host_fs_rmdir(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t dir_fd = WAPI_ARG_I32(0);
    uint32_t path_ptr = WAPI_ARG_U32(1);
    uint32_t path_len = WAPI_ARG_U32(2);

    const char* rel = wapi_wasm_read_string(path_ptr, path_len);
    if (!rel) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    char host_path[1024];
    if (!resolve_path(dir_fd, rel, path_len, host_path, sizeof(host_path))) {
        WAPI_RET_I32(WAPI_ERR_ACCES);
        return NULL;
    }

#ifdef _WIN32
    if (_rmdir(host_path) != 0) {
#else
    if (rmdir(host_path) != 0) {
#endif
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* unlink: (i32 dir_fd, i32 path, i32 path_len) -> i32 */
static wasm_trap_t* host_fs_unlink(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t dir_fd = WAPI_ARG_I32(0);
    uint32_t path_ptr = WAPI_ARG_U32(1);
    uint32_t path_len = WAPI_ARG_U32(2);

    const char* rel = wapi_wasm_read_string(path_ptr, path_len);
    if (!rel) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    char host_path[1024];
    if (!resolve_path(dir_fd, rel, path_len, host_path, sizeof(host_path))) {
        WAPI_RET_I32(WAPI_ERR_ACCES);
        return NULL;
    }

    if (remove(host_path) != 0) {
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* rename: (i32 old_dir, i32 old_path, i32 old_len, i32 new_dir, i32 new_path, i32 new_len) -> i32 */
static wasm_trap_t* host_fs_rename(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t old_dir = WAPI_ARG_I32(0);
    uint32_t old_path_ptr = WAPI_ARG_U32(1);
    uint32_t old_path_len = WAPI_ARG_U32(2);
    int32_t new_dir = WAPI_ARG_I32(3);
    uint32_t new_path_ptr = WAPI_ARG_U32(4);
    uint32_t new_path_len = WAPI_ARG_U32(5);

    const char* old_rel = wapi_wasm_read_string(old_path_ptr, old_path_len);
    const char* new_rel = wapi_wasm_read_string(new_path_ptr, new_path_len);
    if (!old_rel || !new_rel) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    char old_host[1024], new_host[1024];
    if (!resolve_path(old_dir, old_rel, old_path_len, old_host, sizeof(old_host)) ||
        !resolve_path(new_dir, new_rel, new_path_len, new_host, sizeof(new_host))) {
        WAPI_RET_I32(WAPI_ERR_ACCES);
        return NULL;
    }

    if (rename(old_host, new_host) != 0) {
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* readdir: (i32 fd, i32 buf, i32 buf_len, i64 cookie, i32 used_out) -> i32 */
static wasm_trap_t* host_fs_readdir(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t fd = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t buf_len = WAPI_ARG_U32(2);
    uint64_t cookie = WAPI_ARG_U64(3);
    uint32_t used_ptr = WAPI_ARG_U32(4);

    const char* dir_path = NULL;
    if (wapi_handle_valid(fd, WAPI_HTYPE_DIRECTORY)) {
        dir_path = g_rt.handles[fd].data.dir.path;
    } else if (wapi_handle_valid(fd, WAPI_HTYPE_PREOPEN_DIR)) {
        for (int i = 0; i < g_rt.preopen_count; i++) {
            if (g_rt.preopens[i].handle == fd) {
                dir_path = g_rt.preopens[i].host_path;
                break;
            }
        }
    }

    if (!dir_path) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    void* buf = wapi_wasm_ptr(buf_ptr, buf_len);
    if (!buf && buf_len > 0) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

#ifdef _WIN32
    /* Windows directory listing */
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(search_path, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        wapi_wasm_write_u32(used_ptr, 0);
        WAPI_RET_I32(WAPI_OK);
        return NULL;
    }

    uint32_t used = 0;
    uint64_t entry_index = 0;
    do {
        if (entry_index < cookie) {
            entry_index++;
            continue;
        }

        uint32_t namlen = (uint32_t)strlen(ffd.cFileName);
        uint32_t entry_size = 24 + namlen; /* dirent + name */

        if (used + entry_size > buf_len) break;

        /* wapi_dirent_t (24 bytes):
         *   +0: u64 next
         *   +8: u64 ino
         *  +16: u32 namlen
         *  +20: u32 type
         */
        uint8_t entry[24];
        uint64_t next_cookie = entry_index + 1;
        uint64_t ino = 0;
        uint32_t type = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 3 : 4;

        memcpy(entry + 0, &next_cookie, 8);
        memcpy(entry + 8, &ino, 8);
        memcpy(entry + 16, &namlen, 4);
        memcpy(entry + 20, &type, 4);

        wapi_wasm_write_bytes(buf_ptr + used, entry, 24);
        wapi_wasm_write_bytes(buf_ptr + used + 24, ffd.cFileName, namlen);
        used += entry_size;
        entry_index++;
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);
    wapi_wasm_write_u32(used_ptr, used);
    WAPI_RET_I32(WAPI_OK);
#else
    /* POSIX directory listing */
    DIR* dir = opendir(dir_path);
    if (!dir) {
        wapi_wasm_write_u32(used_ptr, 0);
        WAPI_RET_I32(errno_to_tp());
        return NULL;
    }

    uint32_t used = 0;
    uint64_t entry_index = 0;
    struct dirent* de;
    while ((de = readdir(dir)) != NULL) {
        if (entry_index < cookie) {
            entry_index++;
            continue;
        }

        uint32_t namlen = (uint32_t)strlen(de->d_name);
        uint32_t entry_size = 24 + namlen;

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

        memcpy(entry + 0, &next_cookie, 8);
        memcpy(entry + 8, &ino, 8);
        memcpy(entry + 16, &namlen, 4);
        memcpy(entry + 20, &type, 4);

        wapi_wasm_write_bytes(buf_ptr + used, entry, 24);
        wapi_wasm_write_bytes(buf_ptr + used + 24, de->d_name, namlen);
        used += entry_size;
        entry_index++;
    }

    closedir(dir);
    wapi_wasm_write_u32(used_ptr, used);
    WAPI_RET_I32(WAPI_OK);
#endif

    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_fs(wasmtime_linker_t* linker) {
    WAPI_DEFINE_0_1(linker, "wapi_fs", "preopen_count", host_fs_preopen_count);
    WAPI_DEFINE_4_1(linker, "wapi_fs", "preopen_path",  host_fs_preopen_path);
    WAPI_DEFINE_1_1(linker, "wapi_fs", "preopen_handle", host_fs_preopen_handle);
    WAPI_DEFINE_6_1(linker, "wapi_fs", "open",           host_fs_open);
    WAPI_DEFINE_4_1(linker, "wapi_fs", "read",           host_fs_read);
    WAPI_DEFINE_4_1(linker, "wapi_fs", "write",          host_fs_write);

    /* pread: (i32, i32, i32, i64, i32) -> i32 */
    wapi_linker_define(linker, "wapi_fs", "pread", host_fs_pread,
        5, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    /* pwrite: (i32, i32, i32, i64, i32) -> i32 */
    wapi_linker_define(linker, "wapi_fs", "pwrite", host_fs_pwrite,
        5, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    /* seek: (i32, i64, i32, i32) -> i32 */
    wapi_linker_define(linker, "wapi_fs", "seek", host_fs_seek,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I64,WASM_I32,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_1_1(linker, "wapi_fs", "close", host_fs_close);
    WAPI_DEFINE_1_1(linker, "wapi_fs", "sync",  host_fs_sync);
    WAPI_DEFINE_2_1(linker, "wapi_fs", "stat",  host_fs_stat);
    WAPI_DEFINE_4_1(linker, "wapi_fs", "path_stat", host_fs_path_stat);

    /* set_size: (i32, i64) -> i32 */
    wapi_linker_define(linker, "wapi_fs", "set_size", host_fs_set_size,
        2, (wasm_valkind_t[]){WASM_I32,WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_3_1(linker, "wapi_fs", "mkdir",   host_fs_mkdir);
    WAPI_DEFINE_3_1(linker, "wapi_fs", "rmdir",   host_fs_rmdir);
    WAPI_DEFINE_3_1(linker, "wapi_fs", "unlink",  host_fs_unlink);
    WAPI_DEFINE_6_1(linker, "wapi_fs", "rename",  host_fs_rename);

    /* readdir: (i32, i32, i32, i64, i32) -> i32 */
    wapi_linker_define(linker, "wapi_fs", "readdir", host_fs_readdir,
        5, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
}
