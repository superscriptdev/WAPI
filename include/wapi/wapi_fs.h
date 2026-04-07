/**
 * WAPI - Filesystem
 * Version 1.0.0
 *
 * Capability-based filesystem access, modeled on WASI Preview 1.
 * All path operations are relative to pre-opened directory handles
 * granted by the host. No ambient filesystem authority.
 *
 * Synchronous wrappers are provided for convenience; they internally
 * use the async I/O queue. Modules that need high-throughput I/O
 * should use wapi_io.h directly with WAPI_IO_OP_READ/WRITE/OPEN opcodes.
 *
 * Import module: "wapi_fs"
 */

#ifndef WAPI_FS_H
#define WAPI_FS_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * File Types
 * ============================================================ */

typedef enum wapi_filetype_t {
    WAPI_FILETYPE_UNKNOWN          = 0,
    WAPI_FILETYPE_BLOCK_DEVICE     = 1,
    WAPI_FILETYPE_CHARACTER_DEVICE = 2,
    WAPI_FILETYPE_DIRECTORY        = 3,
    WAPI_FILETYPE_REGULAR_FILE     = 4,
    WAPI_FILETYPE_SYMBOLIC_LINK    = 7,
    WAPI_FILETYPE_FORCE32          = 0x7FFFFFFF
} wapi_filetype_t;

/* ============================================================
 * Open Flags
 * ============================================================ */

#define WAPI_FS_OFLAG_CREATE    0x0001  /* Create file if it doesn't exist */
#define WAPI_FS_OFLAG_DIRECTORY 0x0002  /* Fail if not a directory */
#define WAPI_FS_OFLAG_EXCL      0x0004  /* Fail if file exists (with CREATE) */
#define WAPI_FS_OFLAG_TRUNC     0x0008  /* Truncate file to zero length */

/* ============================================================
 * File Descriptor Flags
 * ============================================================ */

#define WAPI_FS_FDFLAG_APPEND   0x0001  /* Append mode */
#define WAPI_FS_FDFLAG_DSYNC    0x0002  /* Data integrity sync */
#define WAPI_FS_FDFLAG_NONBLOCK 0x0004  /* Non-blocking mode */
#define WAPI_FS_FDFLAG_SYNC     0x0010  /* File integrity sync */

/* ============================================================
 * Seek Whence
 * ============================================================ */

typedef enum wapi_whence_t {
    WAPI_WHENCE_SET = 0,  /* Absolute position */
    WAPI_WHENCE_CUR = 1,  /* Relative to current position */
    WAPI_WHENCE_END = 2,  /* Relative to end of file */
    WAPI_WHENCE_FORCE32 = 0x7FFFFFFF
} wapi_whence_t;

/* ============================================================
 * File Stat
 * ============================================================
 *
 * Layout (56 bytes, align 8):
 *   Offset  0: uint64_t dev       Device ID
 *   Offset  8: uint64_t ino       Inode number
 *   Offset 16: uint32_t filetype  File type enum
 *   Offset 20: uint32_t _pad0
 *   Offset 24: uint64_t nlink     Number of hard links
 *   Offset 32: uint64_t size      File size in bytes
 *   Offset 40: uint64_t atim      Access time (ns since epoch)
 *   Offset 48: uint64_t mtim      Modification time (ns since epoch)
 */

typedef struct wapi_filestat_t {
    uint64_t    dev;
    uint64_t    ino;
    uint32_t    filetype;   /* wapi_filetype_t */
    uint32_t    _pad0;
    uint64_t    nlink;
    uint64_t    size;
    uint64_t    atim;
    uint64_t    mtim;
} wapi_filestat_t;

_Static_assert(sizeof(wapi_filestat_t) == 56, "wapi_filestat_t must be 56 bytes");

/* ============================================================
 * Directory Entry
 * ============================================================
 *
 * Layout (24 bytes, align 8):
 *   Offset  0: uint64_t next     Cookie for next readdir call
 *   Offset  8: uint64_t ino      Inode number
 *   Offset 16: uint32_t namlen   Length of name (follows struct in buffer)
 *   Offset 20: uint32_t type     File type
 */

typedef struct wapi_dirent_t {
    uint64_t    next;
    uint64_t    ino;
    uint32_t    namlen;
    uint32_t    type;       /* wapi_filetype_t */
} wapi_dirent_t;

_Static_assert(sizeof(wapi_dirent_t) == 24, "wapi_dirent_t must be 24 bytes");

/* ============================================================
 * Pre-opened Directory Discovery
 * ============================================================
 * The host pre-opens directories and assigns them handles starting
 * at WAPI_FS_PREOPEN_BASE. The module discovers them at startup.
 */

#define WAPI_FS_PREOPEN_BASE 4  /* First pre-opened directory handle */

/**
 * Get the number of pre-opened directories.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_fs, preopen_count)
int32_t wapi_fs_preopen_count(void);

/**
 * Get the path of a pre-opened directory.
 *
 * @param index     Pre-open index (0-based).
 * @param buf       Buffer to receive the path (UTF-8).
 * @param buf_len   Buffer capacity.
 * @param path_len  [out] Actual path length.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, preopen_path)
wapi_result_t wapi_fs_preopen_path(int32_t index, char* buf, wapi_size_t buf_len,
                                wapi_size_t* path_len);

/**
 * Get the handle of a pre-opened directory.
 *
 * @param index  Pre-open index (0-based).
 * @return Handle, or WAPI_HANDLE_INVALID if index is out of range.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_fs, preopen_handle)
wapi_handle_t wapi_fs_preopen_handle(int32_t index);

/* ============================================================
 * Synchronous File Operations
 * ============================================================
 * Convenience wrappers. For high-throughput I/O, use wapi_io.h
 * with WAPI_IO_OP_READ/WRITE opcodes.
 */

/**
 * Open a file relative to a directory handle.
 *
 * @param dir_fd    Base directory handle (from preopens).
 * @param path      Relative path (UTF-8).
 * @param oflags    Open flags (WAPI_FS_OFLAG_*).
 * @param fdflags   File descriptor flags (WAPI_FS_FDFLAG_*).
 * @param fd        [out] New file descriptor handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, open)
wapi_result_t wapi_fs_open(wapi_handle_t dir_fd, wapi_string_view_t path,
                        uint32_t oflags, uint32_t fdflags, wapi_handle_t* fd);

/**
 * Read from a file descriptor.
 *
 * @param fd          File descriptor handle.
 * @param buf         Buffer to read into.
 * @param len         Maximum bytes to read.
 * @param bytes_read  [out] Actual bytes read.
 * @return WAPI_OK on success. bytes_read == 0 indicates EOF.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, read)
wapi_result_t wapi_fs_read(wapi_handle_t fd, void* buf, wapi_size_t len,
                        wapi_size_t* bytes_read);

/**
 * Write to a file descriptor.
 *
 * @param fd             File descriptor handle.
 * @param buf            Buffer to write from.
 * @param len            Number of bytes to write.
 * @param bytes_written  [out] Actual bytes written.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, write)
wapi_result_t wapi_fs_write(wapi_handle_t fd, const void* buf, wapi_size_t len,
                         wapi_size_t* bytes_written);

/**
 * Read from a file at a specific offset without changing the position.
 *
 * Wasm signature: (i32, i32, i32, i64, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, pread)
wapi_result_t wapi_fs_pread(wapi_handle_t fd, void* buf, wapi_size_t len,
                         wapi_filesize_t offset, wapi_size_t* bytes_read);

/**
 * Write to a file at a specific offset without changing the position.
 *
 * Wasm signature: (i32, i32, i32, i64, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, pwrite)
wapi_result_t wapi_fs_pwrite(wapi_handle_t fd, const void* buf, wapi_size_t len,
                          wapi_filesize_t offset, wapi_size_t* bytes_written);

/**
 * Seek within a file.
 *
 * @param fd          File descriptor handle.
 * @param offset      Byte offset (interpretation depends on whence).
 * @param whence      Seek origin.
 * @param new_offset  [out] New absolute position after seeking.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i64, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, seek)
wapi_result_t wapi_fs_seek(wapi_handle_t fd, wapi_filedelta_t offset,
                        wapi_whence_t whence, wapi_filesize_t* new_offset);

/**
 * Close a file descriptor.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_fs, close)
wapi_result_t wapi_fs_close(wapi_handle_t fd);

/**
 * Sync file data to storage.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_fs, sync)
wapi_result_t wapi_fs_sync(wapi_handle_t fd);

/**
 * Get file status.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, stat)
wapi_result_t wapi_fs_stat(wapi_handle_t fd, wapi_filestat_t* stat);

/**
 * Get file status by path.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, path_stat)
wapi_result_t wapi_fs_path_stat(wapi_handle_t dir_fd, wapi_string_view_t path,
                             wapi_filestat_t* stat);

/**
 * Set file size (truncate or extend).
 *
 * Wasm signature: (i32, i64) -> i32
 */
WAPI_IMPORT(wapi_fs, set_size)
wapi_result_t wapi_fs_set_size(wapi_handle_t fd, wapi_filesize_t size);

/**
 * Create a directory.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, mkdir)
wapi_result_t wapi_fs_mkdir(wapi_handle_t dir_fd, wapi_string_view_t path);

/**
 * Remove a directory (must be empty).
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, rmdir)
wapi_result_t wapi_fs_rmdir(wapi_handle_t dir_fd, wapi_string_view_t path);

/**
 * Remove a file.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, unlink)
wapi_result_t wapi_fs_unlink(wapi_handle_t dir_fd, wapi_string_view_t path);

/**
 * Rename a file or directory.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, rename)
wapi_result_t wapi_fs_rename(wapi_handle_t old_dir_fd, wapi_string_view_t old_path,
                          wapi_handle_t new_dir_fd, wapi_string_view_t new_path);

/**
 * Read directory entries.
 *
 * @param fd       Directory handle.
 * @param buf      Buffer to receive wapi_dirent_t structs followed by name bytes.
 * @param buf_len  Buffer capacity.
 * @param cookie   Position cookie (0 for start, or wapi_dirent_t.next).
 * @param used     [out] Bytes written to buffer.
 * @return WAPI_OK on success. used == 0 indicates end of directory.
 *
 * Wasm signature: (i32, i32, i32, i64, i32) -> i32
 */
WAPI_IMPORT(wapi_fs, readdir)
wapi_result_t wapi_fs_readdir(wapi_handle_t fd, void* buf, wapi_size_t buf_len,
                           uint64_t cookie, wapi_size_t* used);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_FS_H */
