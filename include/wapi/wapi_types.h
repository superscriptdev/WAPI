/**
 * WAPI - Core Types
 * Version 1.0.0
 *
 * Foundational types used across all capability modules.
 * All structs have pinned byte-level layouts with little-endian encoding.
 * All handles are i32 values validated by the host on every call.
 * Errors are i32 return codes; zero is success, negative is failure.
 */

#ifndef WAPI_TYPES_H
#define WAPI_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Wasm Import Annotations
 * ============================================================
 * When compiling to Wasm, functions are annotated with import_module
 * and import_name attributes. On native, these are no-ops (the host
 * links them as regular function symbols).
 */
#ifdef __wasm__
#define WAPI_IMPORT(module, name) \
    __attribute__((import_module(#module), import_name(#name)))
#define WAPI_EXPORT(name) \
    __attribute__((export_name(#name)))
#else
#define WAPI_IMPORT(module, name)
#define WAPI_EXPORT(name)
#endif

/* ============================================================
 * Primitive Types
 * ============================================================
 * These map directly to Wasm value types.
 * In wasm32: pointers, sizes, and handles are i32.
 */

/** Opaque handle to a host-managed resource. */
typedef int32_t wapi_handle_t;

/** Result code returned by all fallible functions. */
typedef int32_t wapi_result_t;

/** Boolean type. 0 = false, 1 = true. */
typedef int32_t wapi_bool_t;

/** Size type for buffer lengths and counts. */
typedef uint32_t wapi_size_t;

/** 64-bit timestamp in nanoseconds. */
typedef uint64_t wapi_timestamp_t;

/** 64-bit file size / offset. */
typedef uint64_t wapi_filesize_t;

/** Signed 64-bit file offset for seeking. */
typedef int64_t wapi_filedelta_t;

/** Flags type (64-bit to match webgpu.h convention). */
typedef uint64_t wapi_flags_t;

/* ============================================================
 * Handle Constants
 * ============================================================ */

/** Invalid/null handle. Returned on failure, rejected on input. */
#define WAPI_HANDLE_INVALID ((wapi_handle_t)0)

/** Standard I/O handles (pre-granted by the host). */
#define WAPI_STDIN  ((wapi_handle_t)1)
#define WAPI_STDOUT ((wapi_handle_t)2)
#define WAPI_STDERR ((wapi_handle_t)3)

/* ============================================================
 * Result Codes (Error Values)
 * ============================================================
 * Modeled after WASI Preview 1 errno values and POSIX conventions.
 * Zero is success. Negative values are errors.
 * The application can query a human-readable error message via
 * wapi_error_message() for the last error on the current thread.
 */

#define WAPI_OK                   ((wapi_result_t)  0)  /* Success */
#define WAPI_ERR_UNKNOWN          ((wapi_result_t) -1)  /* Unspecified error */
#define WAPI_ERR_INVAL            ((wapi_result_t) -2)  /* Invalid argument */
#define WAPI_ERR_BADF             ((wapi_result_t) -3)  /* Bad handle / descriptor */
#define WAPI_ERR_ACCES            ((wapi_result_t) -4)  /* Permission denied */
#define WAPI_ERR_NOENT            ((wapi_result_t) -5)  /* No such file or directory */
#define WAPI_ERR_EXIST            ((wapi_result_t) -6)  /* Already exists */
#define WAPI_ERR_NOTDIR           ((wapi_result_t) -7)  /* Not a directory */
#define WAPI_ERR_ISDIR            ((wapi_result_t) -8)  /* Is a directory */
#define WAPI_ERR_NOSPC            ((wapi_result_t) -9)  /* No space left */
#define WAPI_ERR_NOMEM            ((wapi_result_t)-10)  /* Out of memory */
#define WAPI_ERR_NAMETOOLONG      ((wapi_result_t)-11)  /* Name too long */
#define WAPI_ERR_NOTEMPTY         ((wapi_result_t)-12)  /* Directory not empty */
#define WAPI_ERR_IO               ((wapi_result_t)-13)  /* I/O error */
#define WAPI_ERR_AGAIN            ((wapi_result_t)-14)  /* Resource temporarily unavailable */
#define WAPI_ERR_BUSY             ((wapi_result_t)-15)  /* Resource busy */
#define WAPI_ERR_TIMEDOUT         ((wapi_result_t)-16)  /* Operation timed out */
#define WAPI_ERR_CONNREFUSED      ((wapi_result_t)-17)  /* Connection refused */
#define WAPI_ERR_CONNRESET        ((wapi_result_t)-18)  /* Connection reset */
#define WAPI_ERR_CONNABORTED      ((wapi_result_t)-19)  /* Connection aborted */
#define WAPI_ERR_NETUNREACH       ((wapi_result_t)-20)  /* Network unreachable */
#define WAPI_ERR_HOSTUNREACH      ((wapi_result_t)-21)  /* Host unreachable */
#define WAPI_ERR_ADDRINUSE        ((wapi_result_t)-22)  /* Address in use */
#define WAPI_ERR_PIPE             ((wapi_result_t)-23)  /* Broken pipe */
#define WAPI_ERR_NOTCAPABLE       ((wapi_result_t)-24)  /* Capability not granted */
#define WAPI_ERR_NOTSUP           ((wapi_result_t)-25)  /* Operation not supported */
#define WAPI_ERR_OVERFLOW         ((wapi_result_t)-26)  /* Value too large */
#define WAPI_ERR_CANCELED         ((wapi_result_t)-27)  /* Operation canceled */
#define WAPI_ERR_FBIG             ((wapi_result_t)-28)  /* File too large */
#define WAPI_ERR_ROFS             ((wapi_result_t)-29)  /* Read-only filesystem */
#define WAPI_ERR_RANGE            ((wapi_result_t)-30)  /* Result out of range */
#define WAPI_ERR_DEADLK           ((wapi_result_t)-31)  /* Deadlock would occur */
#define WAPI_ERR_NOSYS            ((wapi_result_t)-32)  /* Function not implemented */

/* ============================================================
 * String View
 * ============================================================
 * Non-owning reference to a UTF-8 string in linear memory.
 * Follows the webgpu.h WGPUStringView pattern.
 *
 * Layout (8 bytes, align 4):
 *   Offset 0: uint32_t data   (pointer to UTF-8 bytes)
 *   Offset 4: uint32_t length (byte count, or WAPI_STRLEN for null-terminated)
 *
 * Semantics:
 *   {NULL, WAPI_STRLEN} = absent / null value (the default)
 *   {ptr,  WAPI_STRLEN} = null-terminated C string
 *   {ptr,  N}         = explicit-length string (no null terminator required)
 *   {any,  0}         = empty string
 */

#define WAPI_STRLEN ((wapi_size_t)UINT32_MAX)

typedef struct wapi_string_view_t {
    const char* data;
    wapi_size_t   length;
} wapi_string_view_t;

#define WAPI_STRING_VIEW_INIT { NULL, WAPI_STRLEN }

/** Create a string view from a C string literal. */
#define WAPI_STR(s) ((wapi_string_view_t){ (s), WAPI_STRLEN })

/** Create a string view with explicit length. */
#define WAPI_STRN(s, n) ((wapi_string_view_t){ (s), (n) })

/* ============================================================
 * Buffer View
 * ============================================================
 * Non-owning reference to a byte buffer in linear memory.
 * Used for scatter/gather I/O (like WASI's iovec).
 *
 * Layout (8 bytes, align 4):
 *   Offset 0: uint32_t data   (pointer to bytes)
 *   Offset 4: uint32_t length (byte count)
 */

typedef struct wapi_buffer_t {
    uint8_t*  data;
    wapi_size_t length;
} wapi_buffer_t;

typedef struct wapi_const_buffer_t {
    const uint8_t* data;
    wapi_size_t      length;
} wapi_const_buffer_t;

/* ============================================================
 * Chained Struct (Forward Compatibility)
 * ============================================================
 * Following webgpu.h's nextInChain pattern for ABI-stable extension.
 * Descriptors that can be extended have a nextInChain pointer as
 * their first field. Extension structs embed wapi_chained_struct_t
 * as their first field with an sType tag identifying the extension.
 */

typedef enum wapi_stype_t {
    WAPI_STYPE_INVALID = 0,
    /* Surface / window extensions */
    WAPI_STYPE_WINDOW_CONFIG           = 0x0001,
    /* GPU extensions */
    WAPI_STYPE_GPU_SURFACE_CONFIG      = 0x0100,
    /* Audio extensions */
    WAPI_STYPE_AUDIO_SPATIAL_CONFIG    = 0x0200,
    /* Content extensions */
    WAPI_STYPE_TEXT_STYLE_INLINE       = 0x0300,
    WAPI_STYPE_IMAGE_EXTENDED          = 0x0301,
    /* Networking extensions */
    WAPI_STYPE_NET_TLS_CONFIG          = 0x0400,
    /* Module extensions */
    WAPI_STYPE_MODULE_SHARED_MEMORY    = 0x0500,
    /* Reserved for future use */
    WAPI_STYPE_FORCE32 = 0x7FFFFFFF
} wapi_stype_t;

typedef struct wapi_chained_struct_t {
    struct wapi_chained_struct_t* next;
    wapi_stype_t                  sType;
} wapi_chained_struct_t;

/* ============================================================
 * Version
 * ============================================================ */

typedef struct wapi_version_t {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    uint16_t reserved;
} wapi_version_t;

#define WAPI_ABI_VERSION_MAJOR 1
#define WAPI_ABI_VERSION_MINOR 0
#define WAPI_ABI_VERSION_PATCH 0

/* ============================================================
 * Utility Macros
 * ============================================================ */

/** Check if a result is an error. */
#define WAPI_FAILED(result) ((result) < 0)

/** Check if a result is success. */
#define WAPI_SUCCEEDED(result) ((result) >= 0)

/** Check if a handle is valid. */
#define WAPI_HANDLE_VALID(h) ((h) != WAPI_HANDLE_INVALID)

#ifdef __cplusplus
}
#endif

#endif /* WAPI_TYPES_H */
