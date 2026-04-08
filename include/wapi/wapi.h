/**
 * WAPI - Core
 * Version 1.0.0
 *
 * One binary. One ABI. Every platform.
 *
 * This header defines the complete WAPI core: types, I/O operations,
 * events, execution context, and capability detection. Every capability
 * module includes this header and nothing else from the core.
 *
 * Architecture:
 *   wapi.h        Core types, I/O, events, context, capabilities
 *   wapi_*.h      Independent capability modules (include "wapi.h")
 *
 * The WAPI is a C-style calling convention document.
 * Wasm modules import these functions and the host implements them.
 * GPU access uses webgpu.h directly (from webgpu-native/webgpu-headers).
 */

#ifndef WAPI_H
#define WAPI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ################################################################
 * PART 1 — TYPES
 *
 * Foundational types used across all capability modules.
 * All structs have pinned byte-level layouts with little-endian encoding.
 * All handles are i32 values validated by the host on every call.
 * Errors are i32 return codes; zero is success, negative is failure.
 * ################################################################ */

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
 * Layout (16 bytes, align 8):
 *   Offset  0: uint64_t data   (linear memory address of UTF-8 bytes)
 *   Offset  8: uint32_t length (byte count, or WAPI_STRLEN for null-terminated)
 *   Offset 12: uint32_t _pad
 *
 * Semantics:
 *   {NULL, WAPI_STRLEN} = absent / null value (the default)
 *   {ptr,  WAPI_STRLEN} = null-terminated C string
 *   {ptr,  N}         = explicit-length string (no null terminator required)
 *   {any,  0}         = empty string
 */

#define WAPI_STRLEN ((wapi_size_t)UINT32_MAX)

/**
 * String view. The data field is uint64_t (not a pointer) so the
 * layout is identical for wasm32 and wasm64. Wasm32 modules
 * zero-extend their 32-bit addresses.
 *
 * Layout (16 bytes, align 8):
 *   Offset  0: uint64_t data    (linear memory address of UTF-8 bytes)
 *   Offset  8: uint32_t length  (byte count, or WAPI_STRLEN for null-terminated)
 *   Offset 12: uint32_t _pad
 */
typedef struct wapi_string_view_t {
    uint64_t    data;       /* Linear memory address of UTF-8 bytes */
    wapi_size_t length;
    uint32_t    _pad;
} wapi_string_view_t;

#define WAPI_STRING_VIEW_INIT { 0, WAPI_STRLEN, 0 }

/** Create a string view from a C string literal. */
#define WAPI_STR(s) ((wapi_string_view_t){ (uintptr_t)(s), WAPI_STRLEN, 0 })

/** Create a string view with explicit length. */
#define WAPI_STRN(s, n) ((wapi_string_view_t){ (uintptr_t)(s), (n), 0 })

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
    /* Reserved for future use */
    WAPI_STYPE_FORCE32 = 0x7FFFFFFF
} wapi_stype_t;

/**
 * Chained struct header. The next field is uint64_t (linear memory
 * address) so the layout is identical for wasm32 and wasm64.
 *
 * Layout (16 bytes, align 8):
 *   Offset  0: uint64_t   next   (address of next chained struct, or 0)
 *   Offset  8: wapi_stype_t sType (struct type tag)
 *   Offset 12: uint32_t   _pad
 */
typedef struct wapi_chained_struct_t {
    uint64_t      next;     /* Linear memory address of next struct, or 0 */
    wapi_stype_t  sType;
    uint32_t      _pad;
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
 * Permission State
 * ============================================================ */

typedef enum wapi_perm_state_t {
    WAPI_PERM_GRANTED  = 0,  /* Permission granted */
    WAPI_PERM_DENIED   = 1,  /* Permission denied (do not prompt again) */
    WAPI_PERM_PROMPT   = 2,  /* Permission not yet requested */
    WAPI_PERM_FORCE32  = 0x7FFFFFFF
} wapi_perm_state_t;

/* ============================================================
 * Utility Macros
 * ============================================================ */

/** Check if a result is an error. */
#define WAPI_FAILED(result) ((result) < 0)

/** Check if a result is success. */
#define WAPI_SUCCEEDED(result) ((result) >= 0)

/** Check if a handle is valid. */
#define WAPI_HANDLE_VALID(h) ((h) != WAPI_HANDLE_INVALID)

/* ################################################################
 * PART 2 — ASYNC I/O
 *
 * Defines the I/O operation descriptor and opcodes for ALL async
 * operations across the entire platform. Operations are submitted
 * via the wapi_io_submit host import (or wapi_io_t vtable for
 * build-time library composition) and completions arrive as
 * WAPI_EVENT_IO_COMPLETION events in the unified event queue.
 *
 * The host decides sync vs async behavior:
 *   - Sync: executes immediately, pushes completion before returning.
 *   - Async: queues the operation and completes later.
 * Build-time libraries using the wapi_io_t vtable can wrap, log,
 * throttle, or mock I/O. The module code is identical in all cases.
 *
 * Field mapping convention for wapi_io_op_t:
 *   fd         Handle (file, device, session, codec, port)
 *   flags      Primary flags (open_flags, transport, type mask)
 *   flags2     Secondary params (filter_count, port, endpoint)
 *   addr/len   Primary buffer (data, path, config struct ptr)
 *   addr2/len2 Secondary buffer (output buf, challenge)
 *   offset     File offset, timeout_ns, timestamp_us, packed params
 *   result_ptr Output pointer (handle, length, state)
 *   user_data  Correlation token (echoed in completion event)
 *
 * Import module: "wapi_io"
 * ################################################################ */

/* ============================================================
 * Operation Types
 * ============================================================
 *
 * Opcode ranges:
 *   0x000-0x03F  Core (filesystem, net, timer, audio)
 *   0x040-0x07F  Extended network (net ext, P2P)
 *   0x080-0x0DF  Hardware I/O (serial, MIDI, BT, USB, NFC, camera)
 *   0x100-0x17F  Media (codec, video, speech, screen capture)
 *   0x180-0x1FF  User interaction (dialog, auth, pickers)
 *   0x200-0x27F  Spatial (XR, geolocation)
 *   0x800-0xFFF  Vendor / platform-specific extensions
 */

typedef enum wapi_io_opcode_t {

    /* ---- Core: General (0x00-0x09) ---- */
    WAPI_IO_OP_NOP          = 0x00,  /* No operation (fence) */
    WAPI_IO_OP_READ         = 0x01,  /* fd, offset, addr/len -> result_ptr=bytes_read */
    WAPI_IO_OP_WRITE        = 0x02,  /* fd, offset, addr/len -> result_ptr=bytes_written */
    WAPI_IO_OP_OPEN         = 0x03,  /* addr/len=path, flags, flags2=mode -> result_ptr=fd */
    WAPI_IO_OP_CLOSE        = 0x04,  /* fd */
    WAPI_IO_OP_STAT         = 0x05,  /* fd -> result_ptr=stat_buf */
    WAPI_IO_OP_LOG          = 0x06,  /* flags=level(WAPI_LOG_*), addr/len=message, addr2/len2=tag (fire-and-forget, no completion) */

    /* ---- Core: Network (0x0A-0x0F) ---- */
    WAPI_IO_OP_CONNECT      = 0x0A,  /* addr/len=url, flags=transport -> result_ptr=conn */
    WAPI_IO_OP_ACCEPT       = 0x0B,  /* fd=listener -> result_ptr=conn */
    WAPI_IO_OP_SEND         = 0x0C,  /* fd, addr/len=data -> result_ptr=bytes_sent */
    WAPI_IO_OP_RECV         = 0x0D,  /* fd, addr/len=buf -> result_ptr=bytes_recv */

    /* ---- Core: Timers (0x14-0x15) ---- */
    WAPI_IO_OP_TIMEOUT      = 0x14,  /* offset=duration_ns */
    WAPI_IO_OP_TIMEOUT_ABS  = 0x15,  /* offset=abs_time_ns */

    /* ---- Core: Audio (0x1E-0x1F) ---- */
    WAPI_IO_OP_AUDIO_WRITE  = 0x1E,  /* fd=stream, addr/len=samples */
    WAPI_IO_OP_AUDIO_READ   = 0x1F,  /* fd=stream, addr/len=buf -> result_ptr=bytes_read */

    /* ---- Extended Network (0x040-0x04F) ---- */
    WAPI_IO_OP_NET_LISTEN         = 0x040, /* addr/len=bind_addr, flags=transport, flags2=port<<16|backlog -> result_ptr=listener */
    WAPI_IO_OP_NET_SEND_DATAGRAM  = 0x041, /* fd=conn, addr/len=data */
    WAPI_IO_OP_NET_RECV_DATAGRAM  = 0x042, /* fd=conn, addr/len=buf -> result_ptr=recv_len */
    WAPI_IO_OP_NET_STREAM_OPEN    = 0x043, /* fd=conn, flags=stream_type -> result_ptr=stream */
    WAPI_IO_OP_NET_STREAM_ACCEPT  = 0x044, /* fd=conn -> result_ptr=stream */
    WAPI_IO_OP_NET_RESOLVE        = 0x045, /* addr/len=host, addr2/len2=addrs_buf -> result_ptr=count */

    /* ---- P2P / WebRTC (0x050-0x05F) ---- */
    WAPI_IO_OP_P2P_CREATE              = 0x050, /* addr/len=config -> result_ptr=conn */
    WAPI_IO_OP_P2P_CREATE_OFFER        = 0x051, /* fd=conn, addr/len=sdp_buf -> result_ptr=sdp_len */
    WAPI_IO_OP_P2P_CREATE_ANSWER       = 0x052, /* fd=conn, addr/len=sdp_buf -> result_ptr=sdp_len */
    WAPI_IO_OP_P2P_ADD_ICE_CANDIDATE   = 0x053, /* fd=conn, addr/len=candidate */
    WAPI_IO_OP_P2P_SEND                = 0x054, /* fd=conn, addr/len=data */
    WAPI_IO_OP_P2P_SET_REMOTE_DESC     = 0x055, /* fd=conn, addr/len=sdp */

    /* ---- Serial (0x080-0x08F) ---- */
    WAPI_IO_OP_SERIAL_REQUEST_PORT = 0x080, /* -> result_ptr=port_handle */
    WAPI_IO_OP_SERIAL_OPEN         = 0x081, /* fd=port, addr/len=config_ptr */
    WAPI_IO_OP_SERIAL_READ         = 0x082, /* fd=port, addr/len=buf -> result_ptr=bytes_read */
    WAPI_IO_OP_SERIAL_WRITE        = 0x083, /* fd=port, addr/len=data */

    /* ---- MIDI (0x090-0x09F) ---- */
    WAPI_IO_OP_MIDI_REQUEST_ACCESS = 0x090, /* flags=sysex_flag */
    WAPI_IO_OP_MIDI_OPEN_PORT      = 0x091, /* flags=port_type, flags2=port_index -> result_ptr=port_handle */
    WAPI_IO_OP_MIDI_SEND           = 0x092, /* fd=port, addr/len=data */
    WAPI_IO_OP_MIDI_RECV           = 0x093, /* fd=port, addr/len=buf -> result_ptr=msg_len */

    /* ---- Bluetooth (0x0A0-0x0AF) ---- */
    WAPI_IO_OP_BT_REQUEST_DEVICE       = 0x0A0, /* addr/len=filters, flags2=filter_count -> result_ptr=device */
    WAPI_IO_OP_BT_CONNECT              = 0x0A1, /* fd=device */
    WAPI_IO_OP_BT_READ_VALUE           = 0x0A2, /* fd=characteristic, addr/len=buf -> result_ptr=val_len */
    WAPI_IO_OP_BT_WRITE_VALUE          = 0x0A3, /* fd=characteristic, addr/len=data */
    WAPI_IO_OP_BT_START_NOTIFICATIONS  = 0x0A4, /* fd=characteristic */
    WAPI_IO_OP_BT_GET_SERVICE          = 0x0A5, /* fd=device, addr/len=uuid -> result_ptr=service */
    WAPI_IO_OP_BT_GET_CHARACTERISTIC   = 0x0A6, /* fd=service, addr/len=uuid -> result_ptr=char */

    /* ---- USB (0x0B0-0x0BF) ---- */
    WAPI_IO_OP_USB_REQUEST_DEVICE   = 0x0B0, /* addr/len=filters, flags2=filter_count -> result_ptr=device */
    WAPI_IO_OP_USB_OPEN             = 0x0B1, /* fd=device */
    WAPI_IO_OP_USB_CLAIM_INTERFACE  = 0x0B2, /* fd=device, flags=interface_num */
    WAPI_IO_OP_USB_TRANSFER_IN      = 0x0B3, /* fd=device, addr/len=buf, flags=endpoint -> result_ptr=transferred */
    WAPI_IO_OP_USB_TRANSFER_OUT     = 0x0B4, /* fd=device, addr/len=data, flags=endpoint -> result_ptr=transferred */
    WAPI_IO_OP_USB_CONTROL_TRANSFER = 0x0B5, /* fd=device, offset=packed_setup, addr/len=buf -> result_ptr=transferred */

    /* ---- NFC (0x0C0-0x0CF) ---- */
    WAPI_IO_OP_NFC_SCAN_START = 0x0C0, /* (completions arrive per tag discovered) */
    WAPI_IO_OP_NFC_WRITE      = 0x0C1, /* addr/len=records, flags=record_count */

    /* ---- Camera (0x0D0-0x0DF) ---- */
    WAPI_IO_OP_CAMERA_OPEN       = 0x0D0, /* addr/len=config -> result_ptr=camera_handle */
    WAPI_IO_OP_CAMERA_READ_FRAME = 0x0D1, /* fd=camera, addr/len=buf, addr2/len2=frame_info -> result_ptr=data_size */

    /* ---- Codec (0x100-0x10F) ---- */
    WAPI_IO_OP_CODEC_DECODE     = 0x100, /* fd=codec, offset=timestamp_us, addr/len=data, flags=chunk_flags */
    WAPI_IO_OP_CODEC_ENCODE     = 0x101, /* fd=codec, offset=timestamp_us, addr/len=data */
    WAPI_IO_OP_CODEC_GET_OUTPUT = 0x102, /* fd=codec, addr/len=buf -> result_ptr=out_len */
    WAPI_IO_OP_CODEC_FLUSH      = 0x103, /* fd=codec */

    /* ---- Video (0x110-0x11F) ---- */
    WAPI_IO_OP_VIDEO_CREATE = 0x110, /* addr/len=desc -> result_ptr=video_handle */
    WAPI_IO_OP_VIDEO_SEEK   = 0x111, /* fd=video, offset=time_ns */

    /* ---- Speech (0x120-0x12F) ---- */
    WAPI_IO_OP_SPEECH_SPEAK            = 0x120, /* addr/len=utterance -> result_ptr=id */
    WAPI_IO_OP_SPEECH_RECOGNIZE_START  = 0x121, /* addr/len=lang, flags=continuous -> result_ptr=session */
    WAPI_IO_OP_SPEECH_RECOGNIZE_RESULT = 0x122, /* fd=session, addr/len=buf -> result_ptr=text_len */

    /* ---- Screen Capture (0x130-0x13F) ---- */
    WAPI_IO_OP_CAPTURE_REQUEST   = 0x130, /* flags=source_type -> result_ptr=capture_handle */
    WAPI_IO_OP_CAPTURE_GET_FRAME = 0x131, /* fd=capture, addr/len=buf, addr2/len2=frame_info */

    /* ---- Dialog (0x180-0x18F) ---- */
    WAPI_IO_OP_DIALOG_OPEN_FILE   = 0x180, /* addr/len=default_path, addr2/len2=result_buf, offset=filters_ptr, flags2=filter_count -> result_ptr=result_len */
    WAPI_IO_OP_DIALOG_SAVE_FILE   = 0x181, /* addr/len=default_path, addr2/len2=result_buf, offset=filters_ptr, flags2=filter_count -> result_ptr=result_len */
    WAPI_IO_OP_DIALOG_OPEN_FOLDER = 0x182, /* addr/len=default_path, addr2/len2=result_buf -> result_ptr=result_len */
    WAPI_IO_OP_DIALOG_MESSAGE_BOX = 0x183, /* addr/len=title, addr2/len2=message, flags=type, flags2=buttons -> result_ptr=button_id */

    /* ---- Permissions & Auth (0x190-0x19F) ---- */
    WAPI_IO_OP_PERM_REQUEST            = 0x190, /* addr/len=capability -> result_ptr=state */
    WAPI_IO_OP_BIO_AUTHENTICATE        = 0x191, /* addr/len=reason, flags=bio_type_mask */
    WAPI_IO_OP_AUTHN_CREATE_CREDENTIAL = 0x192, /* addr/len=rp_id, addr2/len2=challenge, result_ptr=user_entity */
    WAPI_IO_OP_AUTHN_GET_ASSERTION     = 0x193, /* addr/len=rp_id, addr2/len2=challenge */

    /* ---- Pickers (0x1A0-0x1AF) ---- */
    WAPI_IO_OP_CONTACTS_PICK       = 0x1A0, /* addr/len=results_buf, flags=properties_mask, flags2=allow_multiple -> result_ptr=count */
    WAPI_IO_OP_EYEDROPPER_PICK     = 0x1A1, /* -> result_ptr=rgba */
    WAPI_IO_OP_PAY_REQUEST_PAYMENT = 0x1A2, /* addr/len=request_desc, addr2/len2=token_buf -> result_ptr=token_len */

    /* ---- XR (0x200-0x20F) ---- */
    WAPI_IO_OP_XR_REQUEST_SESSION = 0x200, /* flags=session_type -> result_ptr=session */
    WAPI_IO_OP_XR_WAIT_FRAME      = 0x201, /* fd=session, addr/len=views_buf, addr2/len2=state, flags2=max_views */
    WAPI_IO_OP_XR_HIT_TEST        = 0x202, /* fd=session, addr/len=origin(12), addr2/len2=direction(12) -> result_ptr=pose */

    /* ---- Geolocation (0x210-0x21F) ---- */
    WAPI_IO_OP_GEO_GET_POSITION   = 0x210, /* offset=timeout_ms, flags=accuracy -> result_ptr=position */
    WAPI_IO_OP_GEO_WATCH_POSITION = 0x211, /* flags=accuracy -> result_ptr=watch_handle */

    WAPI_IO_OP_FORCE32 = 0x7FFFFFFF
} wapi_io_opcode_t;

/* ============================================================
 * Log Levels (for WAPI_IO_OP_LOG)
 * ============================================================ */

#define WAPI_LOG_DEBUG  0
#define WAPI_LOG_INFO   1
#define WAPI_LOG_WARN   2
#define WAPI_LOG_ERROR  3

/* ============================================================
 * Operation Descriptor
 * ============================================================
 * Fixed-size 80-byte descriptor. The meaning of fields depends
 * on the opcode. user_data is echoed back in the completion event.
 *
 * Address fields (addr, addr2, result_ptr) are uint64_t so the
 * layout is identical for wasm32 and wasm64. Wasm32 modules
 * zero-extend their 32-bit addresses into 64-bit fields.
 *
 * Layout (80 bytes, align 8):
 *   Offset  0: uint32_t opcode
 *   Offset  4: uint32_t flags
 *   Offset  8: int32_t  fd          (file descriptor / handle)
 *   Offset 12: uint32_t _pad0
 *   Offset 16: uint64_t offset      (file offset, or timeout_ns)
 *   Offset 24: uint64_t addr        (pointer to buffer)
 *   Offset 32: uint32_t len         (buffer length)
 *   Offset 36: uint32_t _pad1
 *   Offset 40: uint64_t addr2       (pointer to second buffer / path)
 *   Offset 48: uint32_t len2        (second buffer length)
 *   Offset 52: uint32_t flags2      (operation-specific flags)
 *   Offset 56: uint64_t user_data   (opaque, echoed in completion)
 *   Offset 64: uint64_t result_ptr  (pointer for output values)
 *   Offset 72: uint8_t  reserved[8]
 */

typedef struct wapi_io_op_t {
    uint32_t    opcode;     /* wapi_io_opcode_t */
    uint32_t    flags;      /* Operation flags */
    int32_t     fd;         /* Handle / file descriptor */
    uint32_t    _pad0;
    uint64_t    offset;     /* File offset or timeout in nanoseconds */
    uint64_t    addr;       /* Pointer to buffer */
    uint32_t    len;        /* Buffer length */
    uint32_t    _pad1;
    uint64_t    addr2;      /* Second pointer (path for open, etc.) */
    uint32_t    len2;       /* Second length */
    uint32_t    flags2;     /* Additional operation-specific flags */
    uint64_t    user_data;  /* Echoed in completion, for correlation */
    uint64_t    result_ptr; /* Pointer to write output (bytes read, fd, etc.) */
    uint8_t     reserved[8];
} wapi_io_op_t;

/* ################################################################
 * PART 3 — EVENTS
 *
 * Defines the event types, per-event structs, and the 128-byte event
 * union for all platform events: input, lifecycle, I/O completions,
 * device changes, drag-and-drop, and more.
 *
 * Event delivery is through the I/O imports (wapi_io_poll /
 * wapi_io_wait). There is no separate event system.
 * All events -- whether initiated by the module (I/O completions) or
 * pushed by the host (input, lifecycle) -- arrive through the same
 * poll/wait imports (wapi_io_poll / wapi_io_wait).
 * ################################################################ */

/* ============================================================
 * Event Types
 * ============================================================
 * Event type ranges follow SDL3 conventions for familiarity.
 */

typedef enum wapi_event_type_t {
    WAPI_EVENT_NONE = 0,

    /* Application lifecycle (0x100-0x1FF) */
    WAPI_EVENT_QUIT             = 0x100,
    WAPI_EVENT_TERMINATING      = 0x101,
    WAPI_EVENT_LOW_MEMORY       = 0x102,
    WAPI_EVENT_WILL_ENTER_BG    = 0x103,
    WAPI_EVENT_DID_ENTER_BG     = 0x104,
    WAPI_EVENT_WILL_ENTER_FG    = 0x105,
    WAPI_EVENT_DID_ENTER_FG     = 0x106,

    /* Surface events (0x200-0x20F) */
    WAPI_EVENT_SURFACE_RESIZED      = 0x0200,
    WAPI_EVENT_SURFACE_DPI_CHANGED  = 0x020A,

    /* Window events (0x210-0x2FF) */
    WAPI_EVENT_WINDOW_CLOSE         = 0x0210,
    WAPI_EVENT_WINDOW_FOCUS_GAINED  = 0x0211,
    WAPI_EVENT_WINDOW_FOCUS_LOST    = 0x0212,
    WAPI_EVENT_WINDOW_SHOWN         = 0x0213,
    WAPI_EVENT_WINDOW_HIDDEN        = 0x0214,
    WAPI_EVENT_WINDOW_MINIMIZED     = 0x0215,
    WAPI_EVENT_WINDOW_MAXIMIZED     = 0x0216,
    WAPI_EVENT_WINDOW_RESTORED      = 0x0217,
    WAPI_EVENT_WINDOW_MOVED         = 0x0218,

    /* Keyboard events (0x300-0x3FF) */
    WAPI_EVENT_KEY_DOWN         = 0x300,
    WAPI_EVENT_KEY_UP           = 0x301,
    WAPI_EVENT_TEXT_INPUT        = 0x302,
    WAPI_EVENT_TEXT_EDITING      = 0x303,

    /* Mouse events (0x400-0x4FF) */
    WAPI_EVENT_MOUSE_MOTION     = 0x400,
    WAPI_EVENT_MOUSE_BUTTON_DOWN = 0x401,
    WAPI_EVENT_MOUSE_BUTTON_UP  = 0x402,
    WAPI_EVENT_MOUSE_WHEEL      = 0x403,

    /* Input device lifecycle events (0x500-0x5FF) */
    WAPI_EVENT_DEVICE_ADDED     = 0x500,
    WAPI_EVENT_DEVICE_REMOVED   = 0x501,

    /* Touch events (0x700-0x7FF) */
    WAPI_EVENT_TOUCH_DOWN       = 0x700,
    WAPI_EVENT_TOUCH_UP         = 0x701,
    WAPI_EVENT_TOUCH_MOTION     = 0x702,
    WAPI_EVENT_TOUCH_CANCELED   = 0x703,

    /* Gamepad events (0x650-0x6FF) */
    WAPI_EVENT_GAMEPAD_AXIS           = 0x652,
    WAPI_EVENT_GAMEPAD_BUTTON_DOWN    = 0x653,
    WAPI_EVENT_GAMEPAD_BUTTON_UP      = 0x654,
    WAPI_EVENT_GAMEPAD_SENSOR         = 0x655,
    WAPI_EVENT_GAMEPAD_TOUCHPAD_DOWN  = 0x656,
    WAPI_EVENT_GAMEPAD_TOUCHPAD_UP    = 0x657,
    WAPI_EVENT_GAMEPAD_TOUCHPAD_MOTION = 0x658,
    WAPI_EVENT_GAMEPAD_REMAPPED       = 0x659,

    /* Touch gesture events (0x750-0x7FF) */
    WAPI_EVENT_GESTURE          = 0x750,

    /* Pen/stylus events (0x800-0x8FF) */
    WAPI_EVENT_PEN_DOWN            = 0x800,
    WAPI_EVENT_PEN_UP              = 0x801,
    WAPI_EVENT_PEN_MOTION          = 0x802,
    WAPI_EVENT_PEN_PROXIMITY_IN    = 0x803,
    WAPI_EVENT_PEN_PROXIMITY_OUT   = 0x804,
    WAPI_EVENT_PEN_BUTTON_DOWN     = 0x805,
    WAPI_EVENT_PEN_BUTTON_UP       = 0x806,

    /* Drop events (0x1000-0x10FF) */
    WAPI_EVENT_DROP_FILE        = 0x1000,
    WAPI_EVENT_DROP_TEXT        = 0x1001,
    WAPI_EVENT_DROP_BEGIN       = 0x1002,
    WAPI_EVENT_DROP_COMPLETE    = 0x1003,

    /* Display events (0x1800-0x18FF) */
    WAPI_EVENT_DISPLAY_ADDED    = 0x1800,
    WAPI_EVENT_DISPLAY_REMOVED  = 0x1801,
    WAPI_EVENT_DISPLAY_CHANGED  = 0x1802,

    /* Theme events (0x1810-0x181F) */
    WAPI_EVENT_THEME_CHANGED    = 0x1810,

    /* System tray events (0x1820-0x182F) */
    WAPI_EVENT_TRAY_CLICK       = 0x1820,
    WAPI_EVENT_TRAY_MENU        = 0x1821,

    /* Global hotkey events (0x1830-0x183F) */
    WAPI_EVENT_HOTKEY           = 0x1830,

    /* File watcher events (0x1840-0x184F) */
    WAPI_EVENT_FILE_CHANGED     = 0x1840,

    /* Font events (0x1860-0x186F) */
    WAPI_EVENT_FONT_ADDED       = 0x1860,
    WAPI_EVENT_FONT_REMOVED     = 0x1861,

    /* Menu events (0x1850-0x185F) */
    WAPI_EVENT_MENU_SELECT      = 0x1850,

    /* Content tree events (0x1900-0x190F) */
    WAPI_EVENT_CONTENT_ACTIVATE     = 0x1900,  /* User activated a node (Enter/click) */
    WAPI_EVENT_CONTENT_FOCUS        = 0x1901,  /* Keyboard focus moved to a node (Tab) */
    WAPI_EVENT_CONTENT_VALUE_CHANGE = 0x1902,  /* User changed a node's value */

    /* I/O completion events (0x2000-0x20FF) */
    WAPI_EVENT_IO_COMPLETION    = 0x2000,

    /* User events (0x8000-0xFFFF) */
    WAPI_EVENT_USER             = 0x8000,

    WAPI_EVENT_FORCE32          = 0x7FFFFFFF
} wapi_event_type_t;

/* ============================================================
 * Scan Codes (Physical Keys)
 * ============================================================
 * Based on USB HID Usage Tables, matching W3C UIEvents code values.
 * Only the most common keys are listed; the full set is in the spec.
 */

typedef enum wapi_scancode_t {
    WAPI_SCANCODE_UNKNOWN = 0,

    /* Letters */
    WAPI_SCANCODE_A = 4,  WAPI_SCANCODE_B = 5,  WAPI_SCANCODE_C = 6,
    WAPI_SCANCODE_D = 7,  WAPI_SCANCODE_E = 8,  WAPI_SCANCODE_F = 9,
    WAPI_SCANCODE_G = 10, WAPI_SCANCODE_H = 11, WAPI_SCANCODE_I = 12,
    WAPI_SCANCODE_J = 13, WAPI_SCANCODE_K = 14, WAPI_SCANCODE_L = 15,
    WAPI_SCANCODE_M = 16, WAPI_SCANCODE_N = 17, WAPI_SCANCODE_O = 18,
    WAPI_SCANCODE_P = 19, WAPI_SCANCODE_Q = 20, WAPI_SCANCODE_R = 21,
    WAPI_SCANCODE_S = 22, WAPI_SCANCODE_T = 23, WAPI_SCANCODE_U = 24,
    WAPI_SCANCODE_V = 25, WAPI_SCANCODE_W = 26, WAPI_SCANCODE_X = 27,
    WAPI_SCANCODE_Y = 28, WAPI_SCANCODE_Z = 29,

    /* Numbers */
    WAPI_SCANCODE_1 = 30, WAPI_SCANCODE_2 = 31, WAPI_SCANCODE_3 = 32,
    WAPI_SCANCODE_4 = 33, WAPI_SCANCODE_5 = 34, WAPI_SCANCODE_6 = 35,
    WAPI_SCANCODE_7 = 36, WAPI_SCANCODE_8 = 37, WAPI_SCANCODE_9 = 38,
    WAPI_SCANCODE_0 = 39,

    /* Control keys */
    WAPI_SCANCODE_RETURN    = 40,
    WAPI_SCANCODE_ESCAPE    = 41,
    WAPI_SCANCODE_BACKSPACE = 42,
    WAPI_SCANCODE_TAB       = 43,
    WAPI_SCANCODE_SPACE     = 44,

    /* Modifiers */
    WAPI_SCANCODE_LCTRL  = 224, WAPI_SCANCODE_LSHIFT = 225,
    WAPI_SCANCODE_LALT   = 226, WAPI_SCANCODE_LGUI   = 227,
    WAPI_SCANCODE_RCTRL  = 228, WAPI_SCANCODE_RSHIFT = 229,
    WAPI_SCANCODE_RALT   = 230, WAPI_SCANCODE_RGUI   = 231,

    /* Function keys */
    WAPI_SCANCODE_F1  = 58,  WAPI_SCANCODE_F2  = 59,
    WAPI_SCANCODE_F3  = 60,  WAPI_SCANCODE_F4  = 61,
    WAPI_SCANCODE_F5  = 62,  WAPI_SCANCODE_F6  = 63,
    WAPI_SCANCODE_F7  = 64,  WAPI_SCANCODE_F8  = 65,
    WAPI_SCANCODE_F9  = 66,  WAPI_SCANCODE_F10 = 67,
    WAPI_SCANCODE_F11 = 68,  WAPI_SCANCODE_F12 = 69,

    /* Navigation */
    WAPI_SCANCODE_INSERT    = 73,
    WAPI_SCANCODE_HOME      = 74,
    WAPI_SCANCODE_PAGEUP    = 75,
    WAPI_SCANCODE_DELETE    = 76,
    WAPI_SCANCODE_END       = 77,
    WAPI_SCANCODE_PAGEDOWN  = 78,
    WAPI_SCANCODE_RIGHT     = 79,
    WAPI_SCANCODE_LEFT      = 80,
    WAPI_SCANCODE_DOWN      = 81,
    WAPI_SCANCODE_UP        = 82,

    WAPI_SCANCODE_FORCE32   = 0x7FFFFFFF
} wapi_scancode_t;

/* ============================================================
 * Modifier Flags
 * ============================================================ */

#define WAPI_KMOD_NONE   0x0000
#define WAPI_KMOD_LSHIFT 0x0001
#define WAPI_KMOD_RSHIFT 0x0002
#define WAPI_KMOD_LCTRL  0x0040
#define WAPI_KMOD_RCTRL  0x0080
#define WAPI_KMOD_LALT   0x0100
#define WAPI_KMOD_RALT   0x0200
#define WAPI_KMOD_LGUI   0x0400
#define WAPI_KMOD_RGUI   0x0800
#define WAPI_KMOD_CAPS   0x2000
#define WAPI_KMOD_NUM    0x1000

#define WAPI_KMOD_SHIFT  (WAPI_KMOD_LSHIFT | WAPI_KMOD_RSHIFT)
#define WAPI_KMOD_CTRL   (WAPI_KMOD_LCTRL  | WAPI_KMOD_RCTRL)
#define WAPI_KMOD_ALT    (WAPI_KMOD_LALT   | WAPI_KMOD_RALT)
#define WAPI_KMOD_GUI    (WAPI_KMOD_LGUI   | WAPI_KMOD_RGUI)

/* Platform action modifier: Cmd on macOS, Ctrl everywhere else.
 * The host sets this bit on key events so modules can test a single
 * flag for copy/paste/save/undo without platform-specific logic. */
#define WAPI_KMOD_ACTION 0x4000

/* ============================================================
 * Mouse Buttons
 * ============================================================ */

#define WAPI_MOUSE_BUTTON_LEFT   1
#define WAPI_MOUSE_BUTTON_MIDDLE 2
#define WAPI_MOUSE_BUTTON_RIGHT  3
#define WAPI_MOUSE_BUTTON_X1     4
#define WAPI_MOUSE_BUTTON_X2     5

/* ============================================================
 * Gamepad Buttons and Axes (matching SDL3 GamepadButton/Axis)
 * ============================================================ */

typedef enum wapi_gamepad_button_t {
    WAPI_GAMEPAD_BUTTON_SOUTH  = 0,  /* A / Cross */
    WAPI_GAMEPAD_BUTTON_EAST   = 1,  /* B / Circle */
    WAPI_GAMEPAD_BUTTON_WEST   = 2,  /* X / Square */
    WAPI_GAMEPAD_BUTTON_NORTH  = 3,  /* Y / Triangle */
    WAPI_GAMEPAD_BUTTON_BACK   = 4,
    WAPI_GAMEPAD_BUTTON_GUIDE  = 5,
    WAPI_GAMEPAD_BUTTON_START  = 6,
    WAPI_GAMEPAD_BUTTON_LSTICK = 7,
    WAPI_GAMEPAD_BUTTON_RSTICK = 8,
    WAPI_GAMEPAD_BUTTON_LSHOULDER = 9,
    WAPI_GAMEPAD_BUTTON_RSHOULDER = 10,
    WAPI_GAMEPAD_BUTTON_DPAD_UP    = 11,
    WAPI_GAMEPAD_BUTTON_DPAD_DOWN  = 12,
    WAPI_GAMEPAD_BUTTON_DPAD_LEFT  = 13,
    WAPI_GAMEPAD_BUTTON_DPAD_RIGHT = 14,
    WAPI_GAMEPAD_BUTTON_FORCE32    = 0x7FFFFFFF
} wapi_gamepad_button_t;

typedef enum wapi_gamepad_axis_t {
    WAPI_GAMEPAD_AXIS_LEFTX        = 0,
    WAPI_GAMEPAD_AXIS_LEFTY        = 1,
    WAPI_GAMEPAD_AXIS_RIGHTX       = 2,
    WAPI_GAMEPAD_AXIS_RIGHTY       = 3,
    WAPI_GAMEPAD_AXIS_LEFT_TRIGGER = 4,
    WAPI_GAMEPAD_AXIS_RIGHT_TRIGGER = 5,
    WAPI_GAMEPAD_AXIS_FORCE32      = 0x7FFFFFFF
} wapi_gamepad_axis_t;

/* ============================================================
 * Event Structures
 * ============================================================
 * All event structs share a common header.
 * The event union is padded to 128 bytes (matching SDL3).
 */

/** Common event header (16 bytes, align 8) */
typedef struct wapi_event_common_t {
    uint32_t    type;        /* wapi_event_type_t */
    uint32_t    surface_id;  /* Surface handle this event belongs to */
    uint64_t    timestamp;   /* Nanoseconds (monotonic clock) */
} wapi_event_common_t;

/** Keyboard event */
typedef struct wapi_keyboard_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    keyboard_handle; /* Keyboard device handle */
    uint32_t    scancode;    /* wapi_scancode_t (physical key) */
    uint32_t    keycode;     /* Virtual key code (layout-dependent) */
    uint16_t    mod;         /* Modifier flags (WAPI_KMOD_*) */
    uint8_t     down;        /* 1 = pressed, 0 = released */
    uint8_t     repeat;      /* 1 = key repeat */
} wapi_keyboard_event_t;

/** Text input event */
typedef struct wapi_text_input_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    char        text[32];    /* UTF-8 text input (null-terminated) */
} wapi_text_input_event_t;

/** Mouse motion event */
typedef struct wapi_mouse_motion_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    mouse_handle; /* Mouse device handle */
    uint32_t    button_state;/* Button state mask */
    float       x;           /* Position relative to surface */
    float       y;
    float       xrel;        /* Relative motion */
    float       yrel;
} wapi_mouse_motion_event_t;

/** Mouse button event */
typedef struct wapi_mouse_button_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    mouse_id;
    uint8_t     button;      /* WAPI_MOUSE_BUTTON_* */
    uint8_t     down;        /* 1 = pressed */
    uint8_t     clicks;      /* 1 = single, 2 = double, etc. */
    uint8_t     _pad;
    float       x;
    float       y;
} wapi_mouse_button_event_t;

/** Mouse wheel event */
typedef struct wapi_mouse_wheel_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    mouse_handle;
    uint32_t    _pad;
    float       x;           /* Horizontal scroll (positive = right) */
    float       y;           /* Vertical scroll (positive = away from user) */
} wapi_mouse_wheel_event_t;

/** Touch event */
typedef struct wapi_touch_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    int32_t     touch_handle; /* Touch device handle */
    int32_t     finger_index; /* 0-based finger index */
    float       x;            /* Normalized 0..1 */
    float       y;
    float       dx;           /* Normalized -1..1 */
    float       dy;
    float       pressure;     /* Normalized 0..1 */
    uint32_t    _pad;
} wapi_touch_event_t;

/** Gamepad axis event */
typedef struct wapi_gamepad_axis_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    gamepad_handle; /* Gamepad device handle */
    uint8_t     axis;        /* wapi_gamepad_axis_t */
    uint8_t     _pad[3];
    int16_t     value;       /* -32768 to 32767 */
    uint16_t    _pad2;
    uint32_t    _pad3;
} wapi_gamepad_axis_event_t;

/** Gamepad button event */
typedef struct wapi_gamepad_button_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    gamepad_handle;
    uint8_t     button;      /* wapi_gamepad_button_t */
    uint8_t     down;
    uint8_t     _pad[2];
} wapi_gamepad_button_event_t;

/** Device lifecycle event (DEVICE_ADDED / DEVICE_REMOVED) */
typedef struct wapi_device_event_t {
    uint32_t    type;
    uint32_t    surface_id;     /* 0 for device events */
    uint64_t    timestamp;
    uint32_t    device_type;    /* wapi_device_type_t (defined in wapi_input.h) */
    int32_t     device_handle;  /* Handle if open, WAPI_HANDLE_INVALID otherwise */
    uint8_t     uid[16];        /* Stable device identity */
} wapi_device_event_t;

/** Surface/window event (resize, move, focus, etc.) */
typedef struct wapi_surface_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    int32_t     data1;       /* Width for resize, x for move */
    int32_t     data2;       /* Height for resize, y for move */
} wapi_surface_event_t;

/** Drop event */
typedef struct wapi_drop_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint64_t    data;        /* Linear memory address of file path or text (valid until next poll) */
    wapi_size_t data_len;
    uint32_t    _pad;
} wapi_drop_event_t;

/** Pen/stylus event */
typedef struct wapi_pen_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    pen_handle;
    uint8_t     tool_type;   /* wapi_pen_tool_t */
    uint8_t     button;      /* Barrel button index (0 = none) */
    uint8_t     _pad[2];
    float       x;
    float       y;
    float       pressure;    /* 0.0 to 1.0 */
    float       tilt_x;     /* -90 to 90 degrees */
    float       tilt_y;     /* -90 to 90 degrees */
    float       twist;      /* 0 to 360 degrees */
    float       distance;   /* Distance from surface (0.0 to 1.0) */
    uint32_t    _pad2;
} wapi_pen_event_t;

/** Gamepad sensor event */
typedef struct wapi_gamepad_sensor_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    gamepad_handle;
    uint32_t    sensor;      /* wapi_gamepad_sensor_t */
    float       data[3];     /* x, y, z */
    uint32_t    _pad;
} wapi_gamepad_sensor_event_t;

/** Gamepad touchpad event */
typedef struct wapi_gamepad_touchpad_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    gamepad_handle;
    uint8_t     touchpad;
    uint8_t     finger;
    uint8_t     _pad[2];
    float       x;
    float       y;
    float       pressure;
    uint32_t    _pad2;
} wapi_gamepad_touchpad_event_t;

/** Gesture event */
typedef struct wapi_gesture_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    gesture_type; /* wapi_gesture_type_t */
    uint32_t    _pad;
    float       magnitude;   /* Scale for pinch, angle for rotate */
    float       x;
    float       y;
    uint32_t    _pad2;
} wapi_gesture_event_t;

/** Display event */
typedef struct wapi_display_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    display_id;
    uint32_t    _pad;
} wapi_display_event_t;

/** Hotkey event */
typedef struct wapi_hotkey_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    hotkey_id;
    uint32_t    _pad;
} wapi_hotkey_event_t;

/** Tray event */
typedef struct wapi_tray_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    tray_handle;
    uint32_t    item_id;     /* Menu item ID for TRAY_MENU events */
} wapi_tray_event_t;

/** File watcher event */
typedef struct wapi_fwatch_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    watch_handle;
    uint32_t    change_type; /* wapi_fwatch_change_t */
    uint32_t    path_len;
    uint32_t    _pad;
    uint64_t    path_ptr;    /* Linear memory address of changed path */
} wapi_fwatch_event_t;

/** Font change event */
typedef struct wapi_font_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint64_t    family_ptr;  /* Linear memory address of family name (UTF-8) */
    uint32_t    family_len;
    uint32_t    _pad;
} wapi_font_event_t;

/** Menu event */
typedef struct wapi_menu_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    menu_handle;
    uint32_t    item_id;
} wapi_menu_event_t;

/** Content tree event (activate, focus, value change) */
typedef struct wapi_content_event_t {
    uint32_t    type;
    uint32_t    surface_id;
    uint64_t    timestamp;
    uint32_t    node_id;        /* App-assigned node ID */
    uint32_t    _reserved;
} wapi_content_event_t;

/** I/O completion event -- async operation finished */
typedef struct wapi_io_event_t {
    uint32_t    type;           /* WAPI_EVENT_IO_COMPLETION */
    uint32_t    surface_id;     /* Always 0 for I/O events */
    uint64_t    timestamp;
    int32_t     result;         /* Bytes transferred, new fd, or negative error */
    uint32_t    flags;          /* Completion flags (WAPI_IO_CQE_F_*) */
    uint64_t    user_data;      /* Echoed from wapi_io_op_t */
} wapi_io_event_t;

/* I/O completion flags */
#define WAPI_IO_CQE_F_MORE      0x0001  /* More completions coming for this op */
#define WAPI_IO_CQE_F_OVERFLOW  0x0002  /* Completion queue overflowed */

/* ============================================================
 * Event Union (128 bytes, padded)
 * ============================================================ */

typedef union wapi_event_t {
    uint32_t                          type;
    wapi_event_common_t               common;
    wapi_keyboard_event_t             key;
    wapi_text_input_event_t           text;
    wapi_mouse_motion_event_t         motion;
    wapi_mouse_button_event_t         button;
    wapi_mouse_wheel_event_t          wheel;
    wapi_touch_event_t                touch;
    wapi_gamepad_axis_event_t         gaxis;
    wapi_gamepad_button_event_t       gbutton;
    wapi_gamepad_sensor_event_t       gsensor;
    wapi_gamepad_touchpad_event_t     gtouchpad;
    wapi_device_event_t               device;
    wapi_surface_event_t              surface;
    wapi_drop_event_t                 drop;
    wapi_pen_event_t                  pen;
    wapi_gesture_event_t              gesture;
    wapi_display_event_t              display;
    wapi_hotkey_event_t               hotkey;
    wapi_tray_event_t                 tray;
    wapi_fwatch_event_t               fwatch;
    wapi_font_event_t                 font;
    wapi_menu_event_t                 menu;
    wapi_content_event_t              content;
    wapi_io_event_t                   io;
    uint8_t                           _padding[128];
} wapi_event_t;

/* ################################################################
 * PART 4 — CONTEXT, I/O IMPORTS, AND VTABLE TYPES
 *
 * The execution context is minimal: GPU device handle and flags.
 * I/O and allocation are NOT ambient in context. Instead:
 *
 *   - Main apps call I/O host imports directly (wapi_io_submit, etc.)
 *   - Build-time libraries take vtables as explicit parameters
 *   - Runtime modules use host imports; parent controls via I/O policy
 *
 * Vtable types (wapi_allocator_t, wapi_io_t, wapi_panic_handler_t)
 * are defined here for use by build-time library composition, but
 * they are NOT part of any context struct.
 * ################################################################ */

/* ============================================================
 * Allocator Vtable (Build-Time Library Convention)
 * ============================================================
 * Function table with opaque context pointer. Used when build-time
 * linked libraries accept an explicit allocator parameter (Zig-style).
 * Pass explicitly to functions that need it.
 *
 * The `impl` pointer carries per-instance state (arena position,
 * pool free list, etc.) -- without it, you can't have two different
 * allocator instances alive simultaneously.
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: ptr  impl        Opaque implementation context
 *   Offset  4: ptr  alloc_fn    (impl, size, align) -> ptr
 *   Offset  8: ptr  free_fn     (impl, ptr) -> void
 *   Offset 12: ptr  realloc_fn  (impl, ptr, new_size, align) -> ptr
 */
typedef struct wapi_allocator_t {
    void* impl;
    void* (*alloc_fn)(void* impl, wapi_size_t size, wapi_size_t align);
    void  (*free_fn)(void* impl, void* ptr);
    void* (*realloc_fn)(void* impl, void* ptr, wapi_size_t new_size, wapi_size_t align);
} wapi_allocator_t;

/* ============================================================
 * I/O Vtable (Build-Time Library Convention)
 * ============================================================
 * Function table for I/O operations. Used when build-time linked
 * libraries accept an explicit I/O parameter. NOT part of
 * Pass explicitly to functions that need I/O.
 *
 * Main applications use host imports directly (see wapi_io_submit,
 * wapi_io_poll, wapi_io_wait below). This vtable is for libraries
 * that need I/O injection without coupling to host imports.
 *
 * Layout (24 bytes, align 4):
 *   Offset  0: ptr  impl       Opaque implementation context
 *   Offset  4: ptr  submit     (impl, ops, count) -> i32
 *   Offset  8: ptr  cancel     (impl, user_data) -> i32
 *   Offset 12: ptr  poll       (impl, event) -> i32
 *   Offset 16: ptr  wait       (impl, event, timeout_ms) -> i32
 *   Offset 20: ptr  flush      (impl, event_type) -> void
 */
typedef struct wapi_io_t {
    void*         impl;
    int32_t       (*submit)(void* impl, const wapi_io_op_t* ops,
                            wapi_size_t count);
    wapi_result_t (*cancel)(void* impl, uint64_t user_data);
    int32_t       (*poll)(void* impl, wapi_event_t* event);
    int32_t       (*wait)(void* impl, wapi_event_t* event,
                          int32_t timeout_ms);
    void          (*flush)(void* impl, uint32_t event_type);
} wapi_io_t;

/* ============================================================
 * Panic Handler Vtable (Build-Time Library Convention)
 * ============================================================
 * Function table for panic reporting. Used when build-time linked
 * libraries accept an explicit panic handler parameter. NOT part
 * Pass explicitly to functions that need panic handling.
 *
 * Main applications use the wapi_panic_report host import directly.
 *
 * Layout (8 bytes, align 4):
 *   Offset  0: ptr  impl   Opaque implementation context
 *   Offset  4: ptr  fn     (impl, msg, msg_len) -> void
 */
typedef struct wapi_panic_handler_t {
    void* impl;
    void  (*fn)(void* impl, const char* msg, wapi_size_t msg_len);
} wapi_panic_handler_t;

/* ============================================================
 * Direct I/O Host Imports
 * ============================================================
 * Main applications call these directly instead of using an I/O
 * vtable. The host routes operations based on the calling module.
 * For runtime (isolated) modules, the host enforces the parent's
 * I/O policy (set via wapi_module_set_io_policy).
 *
 * Import module: "wapi_io"
 */

/** Submit I/O operations. Returns number submitted, or negative on error. */
WAPI_IMPORT(wapi_io, submit)
int32_t wapi_io_submit(const wapi_io_op_t* ops, wapi_size_t count);

/** Cancel a pending I/O operation by user_data. */
WAPI_IMPORT(wapi_io, cancel)
wapi_result_t wapi_io_cancel(uint64_t user_data);

/** Poll for the next event (non-blocking). Returns 1 if event available. */
WAPI_IMPORT(wapi_io, poll)
int32_t wapi_io_poll(wapi_event_t* event);

/** Wait for the next event. timeout_ms=-1 blocks indefinitely. Returns 1 if event available. */
WAPI_IMPORT(wapi_io, wait)
int32_t wapi_io_wait(wapi_event_t* event, int32_t timeout_ms);

/** Flush queued events of a given type (0 = all). */
WAPI_IMPORT(wapi_io, flush)
void wapi_io_flush(uint32_t event_type);

/* ============================================================
 * Panic Host Import
 * ============================================================
 * Records a panic message with the host before the module traps.
 * The host knows which module is calling and can route the message
 * to the appropriate handler (stderr, console.log, parent module).
 *
 * Import module: "wapi"
 */

WAPI_IMPORT(wapi, panic_report)
void wapi_panic_report(const char* msg, wapi_size_t msg_len);

/* ============================================================
 * Panic Helper
 * ============================================================ */

/**
 * Report a panic message and trap. Does not return.
 * Calls the wapi_panic_report host import to record the message,
 * then traps unconditionally.
 */
static inline _Noreturn void wapi_panic(const char* msg, wapi_size_t msg_len) {
    wapi_panic_report(msg, msg_len);
    __builtin_trap();
}

/* ################################################################
 * PART 5 — CAPABILITY DETECTION
 *
 * A module queries which capabilities the host provides at startup.
 * No runtime identification -- only feature detection. The module
 * asks "do you support GPU?" not "are you Chrome?"
 *
 * All capabilities are equal. There is no distinction between "core"
 * and "extension" capabilities. A host supports whatever set of
 * capabilities it chooses. The spec defines capabilities under the
 * "wapi.*" namespace; third-party vendors define their own under
 * "vendor.<name>.*".
 *
 * Capability names use dot-separated namespacing:
 *   "wapi.gpu"              - Spec-defined capability
 *   "wapi.geolocation"      - Spec-defined capability
 *   "vendor.acme.feature"   - Vendor-defined capability
 *
 * Import module: "wapi"
 * ################################################################ */

/* ============================================================
 * Capability Names
 * ============================================================
 * String constants for all spec-defined capabilities.
 * These are the canonical names used with wapi_capability_supported().
 */

#define WAPI_CAP_MEMORY        "wapi.memory"
#define WAPI_CAP_FILESYSTEM    "wapi.filesystem"
#define WAPI_CAP_NETWORK       "wapi.network"
#define WAPI_CAP_CLOCK         "wapi.clock"
#define WAPI_CAP_RANDOM        "wapi.random"
#define WAPI_CAP_GPU           "wapi.gpu"
#define WAPI_CAP_SURFACE       "wapi.surface"
#define WAPI_CAP_INPUT         "wapi.input"
#define WAPI_CAP_AUDIO         "wapi.audio"
#define WAPI_CAP_CONTENT       "wapi.content"
#define WAPI_CAP_CLIPBOARD     "wapi.clipboard"
#define WAPI_CAP_IO            "wapi.io"
#define WAPI_CAP_ENV           "wapi.env"
#define WAPI_CAP_MODULE        "wapi.module"
#define WAPI_CAP_FONT          "wapi.font"
#define WAPI_CAP_VIDEO         "wapi.video"
#define WAPI_CAP_GEOLOCATION   "wapi.geolocation"
#define WAPI_CAP_NOTIFICATIONS "wapi.notifications"
#define WAPI_CAP_SENSORS       "wapi.sensors"
#define WAPI_CAP_SPEECH        "wapi.speech"
#define WAPI_CAP_CRYPTO        "wapi.crypto"
#define WAPI_CAP_BIOMETRIC     "wapi.biometric"
#define WAPI_CAP_SHARE         "wapi.share"
#define WAPI_CAP_KV_STORAGE    "wapi.kvstorage"
#define WAPI_CAP_PAYMENTS      "wapi.payments"
#define WAPI_CAP_USB           "wapi.usb"
#define WAPI_CAP_MIDI          "wapi.midi"
#define WAPI_CAP_BLUETOOTH     "wapi.bluetooth"
#define WAPI_CAP_CAMERA        "wapi.camera"
#define WAPI_CAP_XR            "wapi.xr"
#define WAPI_CAP_AUDIO_PLUGIN  "wapi.audioplugin"
#define WAPI_CAP_THREAD        "wapi.thread"
#define WAPI_CAP_SYNC          "wapi.sync"
#define WAPI_CAP_PROCESS       "wapi.process"
#define WAPI_CAP_DIALOG        "wapi.dialog"
#define WAPI_CAP_SYSINFO       "wapi.sysinfo"
#define WAPI_CAP_EYEDROP       "wapi.eyedrop"

/* ============================================================
 * Presets
 * ============================================================
 * Presets are recommended bundles that give developers a stable
 * target. A host claims conformance to a preset by supporting
 * all capabilities in it. Presets are just convenience -- the
 * module can always query capabilities individually.
 */

static const char* const WAPI_PRESET_HEADLESS[] = {
    "wapi.memory", "wapi.filesystem", "wapi.network", "wapi.clock",
    "wapi.random", "wapi.io", "wapi.env", "wapi.sysinfo",
    "wapi.thread", "wapi.sync", "wapi.process", NULL
};

static const char* const WAPI_PRESET_COMPUTE[] = {
    "wapi.memory", "wapi.filesystem", "wapi.network", "wapi.clock",
    "wapi.random", "wapi.io", "wapi.env", "wapi.sysinfo",
    "wapi.gpu", "wapi.thread", "wapi.sync", "wapi.process", NULL
};

static const char* const WAPI_PRESET_AUDIO[] = {
    "wapi.memory", "wapi.filesystem", "wapi.network", "wapi.clock",
    "wapi.random", "wapi.io", "wapi.env", "wapi.sysinfo",
    "wapi.audio", "wapi.thread", "wapi.sync", NULL
};

static const char* const WAPI_PRESET_GRAPHICAL[] = {
    "wapi.memory", "wapi.filesystem", "wapi.network", "wapi.clock",
    "wapi.random", "wapi.io", "wapi.env", "wapi.sysinfo",
    "wapi.gpu", "wapi.surface", "wapi.window", "wapi.display",
    "wapi.input", "wapi.audio", "wapi.content", "wapi.clipboard",
    "wapi.thread", "wapi.sync", "wapi.process", "wapi.dialog", NULL
};

static const char* const WAPI_PRESET_MOBILE[] = {
    "wapi.memory", "wapi.filesystem", "wapi.network", "wapi.clock",
    "wapi.random", "wapi.io", "wapi.env", "wapi.sysinfo",
    "wapi.gpu", "wapi.surface", "wapi.window", "wapi.display",
    "wapi.input", "wapi.audio", "wapi.content", "wapi.clipboard",
    "wapi.thread", "wapi.sync", "wapi.geolocation", "wapi.camera",
    "wapi.notifications", "wapi.sensors", "wapi.biometric", NULL
};

/* ============================================================
 * Capability Query Functions
 * ============================================================
 * These are the first functions a module calls at startup.
 * The host reports what it provides; the module adapts.
 *
 * All capabilities -- spec-defined and vendor-defined -- use
 * the same query mechanism. No separate "extension" API.
 */

/**
 * Query whether a capability is supported by name.
 *
 * @param name  Capability name (e.g., "wapi.gpu", "vendor.acme.feature").
 * @return WAPI_TRUE if the host provides this capability.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi, capability_supported)
wapi_bool_t wapi_capability_supported(wapi_string_view_t name);

/**
 * Get the version of a supported capability.
 *
 * @param name     Capability name.
 * @param version  [out] Version struct (major, minor, patch).
 * @return WAPI_OK on success, WAPI_ERR_NOENT if not supported.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi, capability_version)
wapi_result_t wapi_capability_version(wapi_string_view_t name,
                                      wapi_version_t* version);

/**
 * Get the number of capabilities the host supports.
 * Use with wapi_capability_name() to enumerate all capabilities.
 *
 * @return Number of supported capabilities.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi, capability_count)
int32_t wapi_capability_count(void);

/**
 * Get the name of a supported capability by index.
 * For enumerating all capabilities at startup.
 *
 * @param index     Capability index (0 .. wapi_capability_count()-1).
 * @param buf       [out] Buffer to write the name into.
 * @param buf_len   Buffer capacity in bytes.
 * @param name_len  [out] Actual name length in bytes.
 * @return WAPI_OK on success, WAPI_ERR_RANGE if index out of bounds,
 *         WAPI_ERR_NOMEM if buffer too small (name_len still set).
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi, capability_name)
wapi_result_t wapi_capability_name(uint32_t index, char* buf, wapi_size_t buf_len,
                                wapi_size_t* name_len);

/**
 * Get the WAPI version the host implements.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi, abi_version)
wapi_result_t wapi_abi_version(wapi_version_t* version);

/* ============================================================
 * Permission Queries
 * ============================================================
 * Query the current permission state for a capability without
 * triggering a user prompt. To actually request permission,
 * submit WAPI_IO_OP_PERM_REQUEST through wapi_io_submit.
 */

/**
 * Query the permission state for a capability.
 *
 * Does not trigger a prompt. Returns the current state only.
 *
 * @param capability  Capability name (e.g., "wapi.geolocation").
 * @param state       [out] Current permission state.
 * @return WAPI_OK on success, WAPI_ERR_NOENT if capability unknown.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi, perm_query)
wapi_result_t wapi_perm_query(wapi_string_view_t capability,
                              wapi_perm_state_t* state);

/* ============================================================
 * Convenience: Preset Checking
 * ============================================================ */

/**
 * Check if all capabilities in a NULL-terminated preset array
 * are supported by the host.
 *
 * Usage:
 *   if (wapi_preset_supported(WAPI_PRESET_GRAPHICAL)) { ... }
 */
static inline wapi_bool_t wapi_preset_supported(const char* const* preset) {
    for (int i = 0; preset[i] != NULL; i++) {
        if (!wapi_capability_supported(WAPI_STR(preset[i]))) return 0;
    }
    return 1;
}

/* ============================================================
 * Module Exports
 * ============================================================
 * The module must export these entry points.
 */

/**
 * Module entry point. Called by the host after instantiation.
 * The module queries capabilities and uses host imports directly
 * (wapi_io_*, wapi_gpu_*, etc.). No context struct needed --
 * everything is available through imports.
 * Returns WAPI_OK on success, or a negative error code to abort.
 *
 * Wasm signature: () -> i32
 * Exported as: "wapi_main"
 */
/* WAPI_EXPORT(wapi_main) wapi_result_t wapi_main(void); */

/**
 * Called each frame for graphical applications.
 * The host calls this at the display refresh rate.
 * Returns WAPI_OK to continue, WAPI_ERR_CANCELED to request exit.
 *
 * Exported as: "wapi_frame"
 */
/* WAPI_EXPORT(wapi_frame) wapi_result_t wapi_frame(wapi_timestamp_t timestamp); */

/* ################################################################
 * PART 7 — COMPILE-TIME LAYOUT VERIFICATION
 *
 * Every ABI struct is verified with offsetof + sizeof asserts.
 * This catches: accidentally swapped fields, wrong-sized padding,
 * implicit padding that an explicit _pad field was supposed to
 * prevent, and any divergence from the documented byte layout.
 *
 * Pointer-containing structs are gated on __wasm__ because
 * native 64-bit builds have 8-byte pointers (different offsets).
 * Pointer-free structs are verified on all platforms.
 * ################################################################ */

/* --- Version (8 bytes, align 2) --- */
_Static_assert(offsetof(wapi_version_t, major)    == 0, "");
_Static_assert(offsetof(wapi_version_t, minor)    == 2, "");
_Static_assert(offsetof(wapi_version_t, patch)    == 4, "");
_Static_assert(offsetof(wapi_version_t, reserved) == 6, "");
_Static_assert(sizeof(wapi_version_t) == 8, "wapi_version_t must be 8 bytes");

/* --- Event Union (128 bytes) --- */
_Static_assert(sizeof(wapi_event_t) == 128, "wapi_event_t must be 128 bytes");

/* --- I/O Operation (64 bytes, align 8) --- */
_Static_assert(offsetof(wapi_io_op_t, opcode)     ==  0, "");
_Static_assert(offsetof(wapi_io_op_t, flags)      ==  4, "");
_Static_assert(offsetof(wapi_io_op_t, fd)         ==  8, "");
_Static_assert(offsetof(wapi_io_op_t, _pad0)      == 12, "");
_Static_assert(offsetof(wapi_io_op_t, offset)     == 16, "");
_Static_assert(offsetof(wapi_io_op_t, addr)       == 24, "");
_Static_assert(offsetof(wapi_io_op_t, len)        == 32, "");
_Static_assert(offsetof(wapi_io_op_t, _pad1)      == 36, "");
_Static_assert(offsetof(wapi_io_op_t, addr2)      == 40, "");
_Static_assert(offsetof(wapi_io_op_t, len2)       == 48, "");
_Static_assert(offsetof(wapi_io_op_t, flags2)     == 52, "");
_Static_assert(offsetof(wapi_io_op_t, user_data)  == 56, "");
_Static_assert(offsetof(wapi_io_op_t, result_ptr) == 64, "");
_Static_assert(offsetof(wapi_io_op_t, reserved)   == 72, "");
_Static_assert(sizeof(wapi_io_op_t) == 80, "wapi_io_op_t must be 80 bytes");

/* --- Event Common Header (16 bytes, align 8) --- */
_Static_assert(offsetof(wapi_event_common_t, type)       == 0, "");
_Static_assert(offsetof(wapi_event_common_t, surface_id) == 4, "");
_Static_assert(offsetof(wapi_event_common_t, timestamp)  == 8, "");
_Static_assert(sizeof(wapi_event_common_t) == 16, "wapi_event_common_t must be 16 bytes");

/* --- Keyboard Event (32 bytes) --- */
_Static_assert(offsetof(wapi_keyboard_event_t, type)            ==  0, "");
_Static_assert(offsetof(wapi_keyboard_event_t, surface_id)      ==  4, "");
_Static_assert(offsetof(wapi_keyboard_event_t, timestamp)       ==  8, "");
_Static_assert(offsetof(wapi_keyboard_event_t, keyboard_handle) == 16, "");
_Static_assert(offsetof(wapi_keyboard_event_t, scancode)        == 20, "");
_Static_assert(offsetof(wapi_keyboard_event_t, keycode)         == 24, "");
_Static_assert(offsetof(wapi_keyboard_event_t, mod)             == 28, "");
_Static_assert(offsetof(wapi_keyboard_event_t, down)            == 30, "");
_Static_assert(offsetof(wapi_keyboard_event_t, repeat)          == 31, "");
_Static_assert(sizeof(wapi_keyboard_event_t) == 32, "");

/* --- Text Input Event (48 bytes) --- */
_Static_assert(offsetof(wapi_text_input_event_t, type)       ==  0, "");
_Static_assert(offsetof(wapi_text_input_event_t, surface_id) ==  4, "");
_Static_assert(offsetof(wapi_text_input_event_t, timestamp)  ==  8, "");
_Static_assert(offsetof(wapi_text_input_event_t, text)       == 16, "");
_Static_assert(sizeof(wapi_text_input_event_t) == 48, "");

/* --- Mouse Motion Event (40 bytes) --- */
_Static_assert(offsetof(wapi_mouse_motion_event_t, type)         ==  0, "");
_Static_assert(offsetof(wapi_mouse_motion_event_t, surface_id)   ==  4, "");
_Static_assert(offsetof(wapi_mouse_motion_event_t, timestamp)    ==  8, "");
_Static_assert(offsetof(wapi_mouse_motion_event_t, mouse_handle) == 16, "");
_Static_assert(offsetof(wapi_mouse_motion_event_t, button_state) == 20, "");
_Static_assert(offsetof(wapi_mouse_motion_event_t, x)            == 24, "");
_Static_assert(offsetof(wapi_mouse_motion_event_t, y)            == 28, "");
_Static_assert(offsetof(wapi_mouse_motion_event_t, xrel)         == 32, "");
_Static_assert(offsetof(wapi_mouse_motion_event_t, yrel)         == 36, "");
_Static_assert(sizeof(wapi_mouse_motion_event_t) == 40, "");

/* --- Mouse Button Event (32 bytes) --- */
_Static_assert(offsetof(wapi_mouse_button_event_t, type)       ==  0, "");
_Static_assert(offsetof(wapi_mouse_button_event_t, surface_id) ==  4, "");
_Static_assert(offsetof(wapi_mouse_button_event_t, timestamp)  ==  8, "");
_Static_assert(offsetof(wapi_mouse_button_event_t, mouse_id)   == 16, "");
_Static_assert(offsetof(wapi_mouse_button_event_t, button)     == 20, "");
_Static_assert(offsetof(wapi_mouse_button_event_t, down)       == 21, "");
_Static_assert(offsetof(wapi_mouse_button_event_t, clicks)     == 22, "");
_Static_assert(offsetof(wapi_mouse_button_event_t, _pad)       == 23, "");
_Static_assert(offsetof(wapi_mouse_button_event_t, x)          == 24, "");
_Static_assert(offsetof(wapi_mouse_button_event_t, y)          == 28, "");
_Static_assert(sizeof(wapi_mouse_button_event_t) == 32, "");

/* --- Mouse Wheel Event (32 bytes) --- */
_Static_assert(offsetof(wapi_mouse_wheel_event_t, type)         ==  0, "");
_Static_assert(offsetof(wapi_mouse_wheel_event_t, surface_id)   ==  4, "");
_Static_assert(offsetof(wapi_mouse_wheel_event_t, timestamp)    ==  8, "");
_Static_assert(offsetof(wapi_mouse_wheel_event_t, mouse_handle) == 16, "");
_Static_assert(offsetof(wapi_mouse_wheel_event_t, _pad)         == 20, "");
_Static_assert(offsetof(wapi_mouse_wheel_event_t, x)            == 24, "");
_Static_assert(offsetof(wapi_mouse_wheel_event_t, y)            == 28, "");
_Static_assert(sizeof(wapi_mouse_wheel_event_t) == 32, "");

/* --- Touch Event (48 bytes) --- */
_Static_assert(offsetof(wapi_touch_event_t, type)         ==  0, "");
_Static_assert(offsetof(wapi_touch_event_t, surface_id)   ==  4, "");
_Static_assert(offsetof(wapi_touch_event_t, timestamp)    ==  8, "");
_Static_assert(offsetof(wapi_touch_event_t, touch_handle) == 16, "");
_Static_assert(offsetof(wapi_touch_event_t, finger_index) == 20, "");
_Static_assert(offsetof(wapi_touch_event_t, x)            == 24, "");
_Static_assert(offsetof(wapi_touch_event_t, y)            == 28, "");
_Static_assert(offsetof(wapi_touch_event_t, dx)           == 32, "");
_Static_assert(offsetof(wapi_touch_event_t, dy)           == 36, "");
_Static_assert(offsetof(wapi_touch_event_t, pressure)     == 40, "");
_Static_assert(sizeof(wapi_touch_event_t) == 48, "");

/* --- Gamepad Axis Event (32 bytes) --- */
_Static_assert(offsetof(wapi_gamepad_axis_event_t, type)           ==  0, "");
_Static_assert(offsetof(wapi_gamepad_axis_event_t, surface_id)     ==  4, "");
_Static_assert(offsetof(wapi_gamepad_axis_event_t, timestamp)      ==  8, "");
_Static_assert(offsetof(wapi_gamepad_axis_event_t, gamepad_handle) == 16, "");
_Static_assert(offsetof(wapi_gamepad_axis_event_t, axis)           == 20, "");
_Static_assert(offsetof(wapi_gamepad_axis_event_t, _pad)           == 21, "");
_Static_assert(offsetof(wapi_gamepad_axis_event_t, value)          == 24, "");
_Static_assert(offsetof(wapi_gamepad_axis_event_t, _pad2)          == 26, "");
_Static_assert(sizeof(wapi_gamepad_axis_event_t) == 32, "");

/* --- Gamepad Button Event (24 bytes) --- */
_Static_assert(offsetof(wapi_gamepad_button_event_t, type)           ==  0, "");
_Static_assert(offsetof(wapi_gamepad_button_event_t, surface_id)     ==  4, "");
_Static_assert(offsetof(wapi_gamepad_button_event_t, timestamp)      ==  8, "");
_Static_assert(offsetof(wapi_gamepad_button_event_t, gamepad_handle) == 16, "");
_Static_assert(offsetof(wapi_gamepad_button_event_t, button)         == 20, "");
_Static_assert(offsetof(wapi_gamepad_button_event_t, down)           == 21, "");
_Static_assert(offsetof(wapi_gamepad_button_event_t, _pad)           == 22, "");
_Static_assert(sizeof(wapi_gamepad_button_event_t) == 24, "");

/* --- Device Event (40 bytes) --- */
_Static_assert(offsetof(wapi_device_event_t, type)          ==  0, "");
_Static_assert(offsetof(wapi_device_event_t, surface_id)    ==  4, "");
_Static_assert(offsetof(wapi_device_event_t, timestamp)     ==  8, "");
_Static_assert(offsetof(wapi_device_event_t, device_type)   == 16, "");
_Static_assert(offsetof(wapi_device_event_t, device_handle) == 20, "");
_Static_assert(offsetof(wapi_device_event_t, uid)           == 24, "");
_Static_assert(sizeof(wapi_device_event_t) == 40, "");

/* --- Surface Event (24 bytes) --- */
_Static_assert(offsetof(wapi_surface_event_t, type)       ==  0, "");
_Static_assert(offsetof(wapi_surface_event_t, surface_id) ==  4, "");
_Static_assert(offsetof(wapi_surface_event_t, timestamp)  ==  8, "");
_Static_assert(offsetof(wapi_surface_event_t, data1)      == 16, "");
_Static_assert(offsetof(wapi_surface_event_t, data2)      == 20, "");
_Static_assert(sizeof(wapi_surface_event_t) == 24, "");

/* --- Pen Event (56 bytes) --- */
_Static_assert(offsetof(wapi_pen_event_t, type)       ==  0, "");
_Static_assert(offsetof(wapi_pen_event_t, surface_id) ==  4, "");
_Static_assert(offsetof(wapi_pen_event_t, timestamp)  ==  8, "");
_Static_assert(offsetof(wapi_pen_event_t, pen_handle) == 16, "");
_Static_assert(offsetof(wapi_pen_event_t, tool_type)  == 20, "");
_Static_assert(offsetof(wapi_pen_event_t, button)     == 21, "");
_Static_assert(offsetof(wapi_pen_event_t, _pad)       == 22, "");
_Static_assert(offsetof(wapi_pen_event_t, x)          == 24, "");
_Static_assert(offsetof(wapi_pen_event_t, y)          == 28, "");
_Static_assert(offsetof(wapi_pen_event_t, pressure)   == 32, "");
_Static_assert(offsetof(wapi_pen_event_t, tilt_x)     == 36, "");
_Static_assert(offsetof(wapi_pen_event_t, tilt_y)     == 40, "");
_Static_assert(offsetof(wapi_pen_event_t, twist)      == 44, "");
_Static_assert(offsetof(wapi_pen_event_t, distance)   == 48, "");
_Static_assert(sizeof(wapi_pen_event_t) == 56, "");

/* --- Gamepad Sensor Event (40 bytes) --- */
_Static_assert(offsetof(wapi_gamepad_sensor_event_t, type)           ==  0, "");
_Static_assert(offsetof(wapi_gamepad_sensor_event_t, surface_id)     ==  4, "");
_Static_assert(offsetof(wapi_gamepad_sensor_event_t, timestamp)      ==  8, "");
_Static_assert(offsetof(wapi_gamepad_sensor_event_t, gamepad_handle) == 16, "");
_Static_assert(offsetof(wapi_gamepad_sensor_event_t, sensor)         == 20, "");
_Static_assert(offsetof(wapi_gamepad_sensor_event_t, data)           == 24, "");
_Static_assert(sizeof(wapi_gamepad_sensor_event_t) == 40, "");

/* --- Gamepad Touchpad Event (40 bytes) --- */
_Static_assert(offsetof(wapi_gamepad_touchpad_event_t, type)           ==  0, "");
_Static_assert(offsetof(wapi_gamepad_touchpad_event_t, surface_id)     ==  4, "");
_Static_assert(offsetof(wapi_gamepad_touchpad_event_t, timestamp)      ==  8, "");
_Static_assert(offsetof(wapi_gamepad_touchpad_event_t, gamepad_handle) == 16, "");
_Static_assert(offsetof(wapi_gamepad_touchpad_event_t, touchpad)       == 20, "");
_Static_assert(offsetof(wapi_gamepad_touchpad_event_t, finger)         == 21, "");
_Static_assert(offsetof(wapi_gamepad_touchpad_event_t, _pad)           == 22, "");
_Static_assert(offsetof(wapi_gamepad_touchpad_event_t, x)              == 24, "");
_Static_assert(offsetof(wapi_gamepad_touchpad_event_t, y)              == 28, "");
_Static_assert(offsetof(wapi_gamepad_touchpad_event_t, pressure)       == 32, "");
_Static_assert(sizeof(wapi_gamepad_touchpad_event_t) == 40, "");

/* --- Gesture Event (40 bytes) --- */
_Static_assert(offsetof(wapi_gesture_event_t, type)         ==  0, "");
_Static_assert(offsetof(wapi_gesture_event_t, surface_id)   ==  4, "");
_Static_assert(offsetof(wapi_gesture_event_t, timestamp)    ==  8, "");
_Static_assert(offsetof(wapi_gesture_event_t, gesture_type) == 16, "");
_Static_assert(offsetof(wapi_gesture_event_t, _pad)         == 20, "");
_Static_assert(offsetof(wapi_gesture_event_t, magnitude)    == 24, "");
_Static_assert(offsetof(wapi_gesture_event_t, x)            == 28, "");
_Static_assert(offsetof(wapi_gesture_event_t, y)            == 32, "");
_Static_assert(sizeof(wapi_gesture_event_t) == 40, "");

/* --- I/O Completion Event (32 bytes) --- */
_Static_assert(offsetof(wapi_io_event_t, type)      ==  0, "");
_Static_assert(offsetof(wapi_io_event_t, surface_id)==  4, "");
_Static_assert(offsetof(wapi_io_event_t, timestamp) ==  8, "");
_Static_assert(offsetof(wapi_io_event_t, result)    == 16, "");
_Static_assert(offsetof(wapi_io_event_t, flags)     == 20, "");
_Static_assert(offsetof(wapi_io_event_t, user_data) == 24, "");
_Static_assert(sizeof(wapi_io_event_t) == 32, "wapi_io_event_t must be 32 bytes");

/* --- Address-containing structs (layout identical on all platforms) --- */

/* String View (16 bytes, align 8) */
_Static_assert(offsetof(wapi_string_view_t, data)   == 0, "");
_Static_assert(offsetof(wapi_string_view_t, length) == 8, "");
_Static_assert(offsetof(wapi_string_view_t, _pad)   == 12, "");
_Static_assert(sizeof(wapi_string_view_t) == 16, "wapi_string_view_t must be 16 bytes");

/* Chained Struct (16 bytes, align 8) */
_Static_assert(offsetof(wapi_chained_struct_t, next)  == 0, "");
_Static_assert(offsetof(wapi_chained_struct_t, sType) == 8, "");
_Static_assert(offsetof(wapi_chained_struct_t, _pad)  == 12, "");
_Static_assert(sizeof(wapi_chained_struct_t) == 16, "wapi_chained_struct_t must be 16 bytes");

/* --- Pointer-containing structs (wasm32 only, build-time vtables) --- */
#ifdef __wasm__

/* Allocator (16 bytes, align 4) */
_Static_assert(offsetof(wapi_allocator_t, impl)       ==  0, "");
_Static_assert(offsetof(wapi_allocator_t, alloc_fn)   ==  4, "");
_Static_assert(offsetof(wapi_allocator_t, free_fn)    ==  8, "");
_Static_assert(offsetof(wapi_allocator_t, realloc_fn) == 12, "");
_Static_assert(sizeof(wapi_allocator_t) == 16, "wapi_allocator_t must be 16 bytes");

/* I/O Vtable (24 bytes, align 4) */
_Static_assert(offsetof(wapi_io_t, impl)   ==  0, "");
_Static_assert(offsetof(wapi_io_t, submit) ==  4, "");
_Static_assert(offsetof(wapi_io_t, cancel) ==  8, "");
_Static_assert(offsetof(wapi_io_t, poll)   == 12, "");
_Static_assert(offsetof(wapi_io_t, wait)   == 16, "");
_Static_assert(offsetof(wapi_io_t, flush)  == 20, "");
_Static_assert(sizeof(wapi_io_t) == 24, "wapi_io_t must be 24 bytes");

/* Panic Handler (8 bytes, align 4) */
_Static_assert(offsetof(wapi_panic_handler_t, impl) == 0, "");
_Static_assert(offsetof(wapi_panic_handler_t, fn)   == 4, "");
_Static_assert(sizeof(wapi_panic_handler_t) == 8, "wapi_panic_handler_t must be 8 bytes");

/* Drop Event (32 bytes) */
_Static_assert(offsetof(wapi_drop_event_t, type)       ==  0, "");
_Static_assert(offsetof(wapi_drop_event_t, surface_id) ==  4, "");
_Static_assert(offsetof(wapi_drop_event_t, timestamp)  ==  8, "");
_Static_assert(offsetof(wapi_drop_event_t, data)       == 16, "");
_Static_assert(offsetof(wapi_drop_event_t, data_len)   == 24, "");
_Static_assert(sizeof(wapi_drop_event_t) == 32, "");

#endif /* __wasm__ */

#ifdef __cplusplus
}
#endif

#endif /* WAPI_H */
