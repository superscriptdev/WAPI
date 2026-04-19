/**
 * WAPI SDL Runtime - Filesystem
 *
 * Standard C / POSIX I/O backed. Pre-opened directories are mapped
 * from command-line args in main.c. Blocks path traversal with
 * a simple ".." check on relative paths.
 */

#include "wapi_host.h"
#include <sys/stat.h>

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

static int32_t errno_to_wapi(void) {
    switch (errno) {
    case ENOENT:       return WAPI_ERR_NOENT;
    case EACCES:       return WAPI_ERR_ACCES;
    case EEXIST:       return WAPI_ERR_EXIST;
#ifdef ENOTDIR
    case ENOTDIR:      return WAPI_ERR_NOTDIR;
#endif
#ifdef EISDIR
    case EISDIR:       return WAPI_ERR_ISDIR;
#endif
#ifdef ENOSPC
    case ENOSPC:       return WAPI_ERR_NOSPC;
#endif
    case ENOMEM:       return WAPI_ERR_NOMEM;
#ifdef ENAMETOOLONG
    case ENAMETOOLONG: return WAPI_ERR_NAMETOOLONG;
#endif
#ifdef ENOTEMPTY
    case ENOTEMPTY:    return WAPI_ERR_NOTEMPTY;
#endif
    case EIO:          return WAPI_ERR_IO;
    case EBADF:        return WAPI_ERR_BADF;
    case EINVAL:       return WAPI_ERR_INVAL;
#ifdef EBUSY
    case EBUSY:        return WAPI_ERR_BUSY;
#endif
    default:           return WAPI_ERR_UNKNOWN;
    }
}

static bool resolve_path(int32_t dir_fd,
                         const char* rel, uint32_t rel_len,
                         char* out, size_t out_size) {
    if (!wapi_handle_valid(dir_fd, WAPI_HTYPE_PREOPEN_DIR) &&
        !wapi_handle_valid(dir_fd, WAPI_HTYPE_DIRECTORY)) {
        return false;
    }
    const char* base = NULL;
    if (g_rt.handles[dir_fd].type == WAPI_HTYPE_PREOPEN_DIR) {
        for (int i = 0; i < g_rt.preopen_count; i++) {
            if (g_rt.preopens[i].handle == dir_fd) {
                base = g_rt.preopens[i].host_path; break;
            }
        }
        if (!base) return false;
    } else {
        base = g_rt.handles[dir_fd].data.dir.path;
    }
    if (rel_len == 0) {
        snprintf(out, out_size, "%s", base);
        return true;
    }
    char rel_buf[512];
    uint32_t n = rel_len < 511 ? rel_len : 511;
    memcpy(rel_buf, rel, n);
    rel_buf[n] = '\0';
    if (rel_buf[0] == '/' || rel_buf[0] == '\\') return false;
    if (strstr(rel_buf, "..")) return false;
    snprintf(out, out_size, "%s%c%s", base, PATH_SEP, rel_buf);
    return true;
}

/* ---- Preopens ---- */

static int32_t host_fs_preopen_count(wasm_exec_env_t env) {
    (void)env;
    return g_rt.preopen_count;
}

static int32_t host_fs_preopen_path(wasm_exec_env_t env,
                                    int32_t index,
                                    uint32_t buf_ptr, uint32_t buf_len,
                                    uint32_t path_len_ptr) {
    (void)env;
    if (index < 0 || index >= g_rt.preopen_count) return WAPI_ERR_RANGE;
    const char* path = g_rt.preopens[index].guest_path;
    uint32_t len = (uint32_t)strlen(path);
    wapi_wasm_write_u32(path_len_ptr, len);
    uint32_t copy = len < buf_len ? len : buf_len;
    if (copy > 0) wapi_wasm_write_bytes(buf_ptr, path, copy);
    return WAPI_OK;
}

static int32_t host_fs_preopen_handle(wasm_exec_env_t env, int32_t index) {
    (void)env;
    if (index < 0 || index >= g_rt.preopen_count) return 0;
    return g_rt.preopens[index].handle;
}

/* ---- Open / close / read / write ---- */

static int32_t host_fs_open(wasm_exec_env_t env,
                            int32_t dir_fd, uint32_t path_ptr, uint32_t path_len,
                            uint32_t oflags, uint32_t fdflags,
                            uint32_t fd_out_ptr) {
    (void)env;
    const char* rel = (const char*)wapi_wasm_ptr(path_ptr, path_len);
    if (!rel && path_len > 0) return WAPI_ERR_INVAL;
    char host_path[1024];
    if (!resolve_path(dir_fd, rel, path_len, host_path, sizeof(host_path)))
        return WAPI_ERR_ACCES;

    if (oflags & 0x0002u) {  /* DIRECTORY */
        int32_t h = wapi_handle_alloc(WAPI_HTYPE_DIRECTORY);
        if (h == 0) return WAPI_ERR_NOMEM;
        snprintf(g_rt.handles[h].data.dir.path,
                 sizeof(g_rt.handles[h].data.dir.path), "%s", host_path);
        wapi_wasm_write_i32(fd_out_ptr, h);
        return WAPI_OK;
    }

    const char* mode;
    if (oflags & 0x0001u) {       /* CREATE */
        if (oflags & 0x0008u)       mode = (fdflags & 0x0001u) ? "ab" : "wb";
        else if (oflags & 0x0004u)  mode = "wxb";
        else                        mode = (fdflags & 0x0001u) ? "ab" : "r+b";
    } else if (oflags & 0x0008u) { /* TRUNC */
        mode = "wb";
    } else {
        mode = "rb";
    }
    FILE* f = fopen(host_path, mode);
    if (!f && (oflags & 0x0001u) && !(oflags & 0x0004u)) {
        f = fopen(host_path, (fdflags & 0x0001u) ? "ab" : "w+b");
    }
    if (!f) { wapi_set_error(strerror(errno)); return errno_to_wapi(); }

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_FILE);
    if (h == 0) { fclose(f); return WAPI_ERR_NOMEM; }
    g_rt.handles[h].data.file = f;
    wapi_wasm_write_i32(fd_out_ptr, h);
    return WAPI_OK;
}

static int32_t host_fs_read(wasm_exec_env_t env,
                            int32_t fd, uint32_t buf_ptr, uint32_t len,
                            uint32_t bytes_ptr) {
    (void)env;
    FILE* f;
    if (fd == 1) f = stdin;
    else if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) return WAPI_ERR_BADF;
    else f = g_rt.handles[fd].data.file;
    void* buf = wapi_wasm_ptr(buf_ptr, len);
    if (!buf && len > 0) return WAPI_ERR_INVAL;
    size_t n = fread(buf, 1, len, f);
    wapi_wasm_write_u32(bytes_ptr, (uint32_t)n);
    return WAPI_OK;
}

static int32_t host_fs_write(wasm_exec_env_t env,
                             int32_t fd, uint32_t buf_ptr, uint32_t len,
                             uint32_t bytes_ptr) {
    (void)env;
    FILE* f;
    if (fd == 2) f = stdout;
    else if (fd == 3) f = stderr;
    else if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) return WAPI_ERR_BADF;
    else f = g_rt.handles[fd].data.file;
    const void* buf = wapi_wasm_ptr(buf_ptr, len);
    if (!buf && len > 0) return WAPI_ERR_INVAL;
    size_t n = fwrite(buf, 1, len, f);
    fflush(f);
    wapi_wasm_write_u32(bytes_ptr, (uint32_t)n);
    return WAPI_OK;
}

static int32_t host_fs_pread(wasm_exec_env_t env,
                             int32_t fd, uint32_t buf_ptr, uint32_t len,
                             int64_t offset, uint32_t bytes_ptr) {
    (void)env;
    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) return WAPI_ERR_BADF;
    FILE* f = g_rt.handles[fd].data.file;
    void* buf = wapi_wasm_ptr(buf_ptr, len);
    if (!buf && len > 0) return WAPI_ERR_INVAL;
    long saved = ftell(f);
    if (fseek(f, (long)offset, SEEK_SET) != 0) return errno_to_wapi();
    size_t n = fread(buf, 1, len, f);
    fseek(f, saved, SEEK_SET);
    wapi_wasm_write_u32(bytes_ptr, (uint32_t)n);
    return WAPI_OK;
}

static int32_t host_fs_pwrite(wasm_exec_env_t env,
                              int32_t fd, uint32_t buf_ptr, uint32_t len,
                              int64_t offset, uint32_t bytes_ptr) {
    (void)env;
    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) return WAPI_ERR_BADF;
    FILE* f = g_rt.handles[fd].data.file;
    const void* buf = wapi_wasm_ptr(buf_ptr, len);
    if (!buf && len > 0) return WAPI_ERR_INVAL;
    long saved = ftell(f);
    if (fseek(f, (long)offset, SEEK_SET) != 0) return errno_to_wapi();
    size_t n = fwrite(buf, 1, len, f);
    fseek(f, saved, SEEK_SET);
    wapi_wasm_write_u32(bytes_ptr, (uint32_t)n);
    return WAPI_OK;
}

static int32_t host_fs_seek(wasm_exec_env_t env,
                            int32_t fd, int64_t offset, int32_t whence,
                            uint32_t new_off_ptr) {
    (void)env;
    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) return WAPI_ERR_BADF;
    int c;
    switch (whence) { case 0: c = SEEK_SET; break; case 1: c = SEEK_CUR; break;
                      case 2: c = SEEK_END; break; default: return WAPI_ERR_INVAL; }
    FILE* f = g_rt.handles[fd].data.file;
    if (fseek(f, (long)offset, c) != 0) return errno_to_wapi();
    wapi_wasm_write_u64(new_off_ptr, (uint64_t)ftell(f));
    return WAPI_OK;
}

static int32_t host_fs_close(wasm_exec_env_t env, int32_t fd) {
    (void)env;
    if (fd >= 1 && fd <= 3) return WAPI_OK;
    if (wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
        fclose(g_rt.handles[fd].data.file);
        wapi_handle_free(fd);
        return WAPI_OK;
    }
    if (wapi_handle_valid(fd, WAPI_HTYPE_DIRECTORY)) {
        wapi_handle_free(fd);
        return WAPI_OK;
    }
    return WAPI_ERR_BADF;
}

static int32_t host_fs_sync(wasm_exec_env_t env, int32_t fd) {
    (void)env;
    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) return WAPI_ERR_BADF;
    fflush(g_rt.handles[fd].data.file);
    return WAPI_OK;
}

static void write_stat(uint8_t* out, uint64_t dev, uint64_t ino,
                       uint32_t filetype, uint64_t nlink, uint64_t size,
                       uint64_t atim, uint64_t mtim) {
    memset(out, 0, 56);
    memcpy(out + 0,  &dev, 8);
    memcpy(out + 8,  &ino, 8);
    memcpy(out + 16, &filetype, 4);
    memcpy(out + 24, &nlink, 8);
    memcpy(out + 32, &size, 8);
    memcpy(out + 40, &atim, 8);
    memcpy(out + 48, &mtim, 8);
}

static int32_t host_fs_stat(wasm_exec_env_t env,
                            int32_t fd, uint32_t stat_ptr) {
    (void)env;
    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) return WAPI_ERR_BADF;
    FILE* f = g_rt.handles[fd].data.file;
#ifdef _WIN32
    struct _stat64 st;
    if (_fstat64(_fileno(f), &st) != 0) return errno_to_wapi();
    uint32_t ft = (st.st_mode & _S_IFDIR) ? 3 : 4;
#else
    struct stat st;
    if (fstat(fileno(f), &st) != 0) return errno_to_wapi();
    uint32_t ft = 4;
    if (S_ISDIR(st.st_mode))      ft = 3;
    else if (S_ISLNK(st.st_mode)) ft = 7;
    else if (S_ISBLK(st.st_mode)) ft = 1;
    else if (S_ISCHR(st.st_mode)) ft = 2;
#endif
    uint8_t buf[56];
    write_stat(buf, (uint64_t)st.st_dev, (uint64_t)st.st_ino, ft,
               (uint64_t)st.st_nlink, (uint64_t)st.st_size,
               (uint64_t)st.st_atime * 1000000000ULL,
               (uint64_t)st.st_mtime * 1000000000ULL);
    wapi_wasm_write_bytes(stat_ptr, buf, 56);
    return WAPI_OK;
}

static int32_t host_fs_path_stat(wasm_exec_env_t env,
                                 int32_t dir_fd, uint32_t path_ptr,
                                 uint32_t path_len, uint32_t stat_ptr) {
    (void)env;
    const char* rel = (const char*)wapi_wasm_ptr(path_ptr, path_len);
    if (!rel && path_len > 0) return WAPI_ERR_INVAL;
    char host_path[1024];
    if (!resolve_path(dir_fd, rel, path_len, host_path, sizeof(host_path)))
        return WAPI_ERR_ACCES;
#ifdef _WIN32
    struct _stat64 st;
    if (_stat64(host_path, &st) != 0) return errno_to_wapi();
    uint32_t ft = (st.st_mode & _S_IFDIR) ? 3 : 4;
#else
    struct stat st;
    if (stat(host_path, &st) != 0) return errno_to_wapi();
    uint32_t ft = 4;
    if (S_ISDIR(st.st_mode))      ft = 3;
    else if (S_ISLNK(st.st_mode)) ft = 7;
#endif
    uint8_t buf[56];
    write_stat(buf, (uint64_t)st.st_dev, (uint64_t)st.st_ino, ft,
               (uint64_t)st.st_nlink, (uint64_t)st.st_size,
               (uint64_t)st.st_atime * 1000000000ULL,
               (uint64_t)st.st_mtime * 1000000000ULL);
    wapi_wasm_write_bytes(stat_ptr, buf, 56);
    return WAPI_OK;
}

static int32_t host_fs_set_size(wasm_exec_env_t env,
                                int32_t fd, int64_t size) {
    (void)env;
    if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) return WAPI_ERR_BADF;
    FILE* f = g_rt.handles[fd].data.file;
    fflush(f);
#ifdef _WIN32
    int r = _chsize_s(_fileno(f), size);
#else
    int r = ftruncate(fileno(f), (off_t)size);
#endif
    return r == 0 ? WAPI_OK : errno_to_wapi();
}

static int32_t host_fs_mkdir(wasm_exec_env_t env,
                             int32_t dir_fd, uint32_t path_ptr, uint32_t path_len) {
    (void)env;
    const char* rel = (const char*)wapi_wasm_ptr(path_ptr, path_len);
    if (!rel && path_len > 0) return WAPI_ERR_INVAL;
    char host_path[1024];
    if (!resolve_path(dir_fd, rel, path_len, host_path, sizeof(host_path)))
        return WAPI_ERR_ACCES;
#ifdef _WIN32
    int r = _mkdir(host_path);
#else
    int r = mkdir(host_path, 0755);
#endif
    return r == 0 ? WAPI_OK : errno_to_wapi();
}

static int32_t host_fs_rmdir(wasm_exec_env_t env,
                             int32_t dir_fd, uint32_t path_ptr, uint32_t path_len) {
    (void)env;
    const char* rel = (const char*)wapi_wasm_ptr(path_ptr, path_len);
    if (!rel && path_len > 0) return WAPI_ERR_INVAL;
    char host_path[1024];
    if (!resolve_path(dir_fd, rel, path_len, host_path, sizeof(host_path)))
        return WAPI_ERR_ACCES;
#ifdef _WIN32
    int r = _rmdir(host_path);
#else
    int r = rmdir(host_path);
#endif
    return r == 0 ? WAPI_OK : errno_to_wapi();
}

static int32_t host_fs_unlink(wasm_exec_env_t env,
                              int32_t dir_fd, uint32_t path_ptr, uint32_t path_len) {
    (void)env;
    const char* rel = (const char*)wapi_wasm_ptr(path_ptr, path_len);
    if (!rel && path_len > 0) return WAPI_ERR_INVAL;
    char host_path[1024];
    if (!resolve_path(dir_fd, rel, path_len, host_path, sizeof(host_path)))
        return WAPI_ERR_ACCES;
    return remove(host_path) == 0 ? WAPI_OK : errno_to_wapi();
}

static int32_t host_fs_rename(wasm_exec_env_t env,
                              int32_t old_dir, uint32_t old_ptr, uint32_t old_len,
                              int32_t new_dir, uint32_t new_ptr, uint32_t new_len) {
    (void)env;
    const char* old_rel = (const char*)wapi_wasm_ptr(old_ptr, old_len);
    const char* new_rel = (const char*)wapi_wasm_ptr(new_ptr, new_len);
    if ((!old_rel && old_len > 0) || (!new_rel && new_len > 0)) return WAPI_ERR_INVAL;
    char old_host[1024], new_host[1024];
    if (!resolve_path(old_dir, old_rel, old_len, old_host, sizeof(old_host)))
        return WAPI_ERR_ACCES;
    if (!resolve_path(new_dir, new_rel, new_len, new_host, sizeof(new_host)))
        return WAPI_ERR_ACCES;
    return rename(old_host, new_host) == 0 ? WAPI_OK : errno_to_wapi();
}

static int32_t host_fs_readdir(wasm_exec_env_t env,
                               int32_t dir_fd, int64_t cookie,
                               uint32_t buf_ptr, uint32_t buf_len,
                               uint32_t bytes_written_ptr) {
    (void)env; (void)dir_fd; (void)cookie; (void)buf_ptr; (void)buf_len;
    wapi_wasm_write_u32(bytes_written_ptr, 0);
    return WAPI_ERR_NOTSUP;  /* Directory enumeration left as future work. */
}

static NativeSymbol g_symbols[] = {
    { "preopen_count",  (void*)host_fs_preopen_count,  "()i",     NULL },
    { "preopen_path",   (void*)host_fs_preopen_path,   "(iiii)i", NULL },
    { "preopen_handle", (void*)host_fs_preopen_handle, "(i)i",    NULL },
    { "open",           (void*)host_fs_open,           "(iiiiii)i", NULL },
    { "read",           (void*)host_fs_read,           "(iiii)i", NULL },
    { "write",          (void*)host_fs_write,          "(iiii)i", NULL },
    { "pread",          (void*)host_fs_pread,          "(iiiIi)i", NULL },
    { "pwrite",         (void*)host_fs_pwrite,         "(iiiIi)i", NULL },
    { "seek",           (void*)host_fs_seek,           "(iIii)i", NULL },
    { "close",          (void*)host_fs_close,          "(i)i",    NULL },
    { "sync",           (void*)host_fs_sync,           "(i)i",    NULL },
    { "stat",           (void*)host_fs_stat,           "(ii)i",   NULL },
    { "path_stat",      (void*)host_fs_path_stat,      "(iiii)i", NULL },
    { "set_size",       (void*)host_fs_set_size,       "(iI)i",   NULL },
    { "mkdir",          (void*)host_fs_mkdir,          "(iii)i",  NULL },
    { "rmdir",          (void*)host_fs_rmdir,          "(iii)i",  NULL },
    { "unlink",         (void*)host_fs_unlink,         "(iii)i",  NULL },
    { "rename",         (void*)host_fs_rename,         "(iiiiii)i", NULL },
    { "readdir",        (void*)host_fs_readdir,        "(iIiii)i", NULL },
};

wapi_cap_registration_t wapi_host_fs_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_fs",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
