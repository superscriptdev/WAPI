/**
 * WAPI - Async I/O
 * Version 1.0.0
 *
 * Defines the I/O operation descriptor and opcodes for ALL async
 * operations across the entire platform. Operations are submitted
 * through the wapi_io_t vtable (provided in wapi_context_t) and
 * completions arrive as WAPI_EVENT_IO_COMPLETION events in the
 * unified event queue.
 *
 * The vtable implementation decides sync vs async behavior:
 *   - A sync vtable executes immediately, pushes completion before returning.
 *   - An async vtable queues the operation and completes later.
 *   - A wrapper vtable can log, throttle, mock, or sandbox.
 * The module code is identical in all cases.
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
 */

#ifndef WAPI_IO_H
#define WAPI_IO_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

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
 * Fixed-size 64-byte descriptor. The meaning of fields depends
 * on the opcode. user_data is echoed back in the completion event.
 *
 * Layout (64 bytes, align 8):
 *   Offset  0: uint32_t opcode
 *   Offset  4: uint32_t flags
 *   Offset  8: int32_t  fd          (file descriptor / handle)
 *   Offset 12: uint32_t _pad0
 *   Offset 16: uint64_t offset      (file offset, or timeout_ns)
 *   Offset 24: uint32_t addr        (pointer to buffer)
 *   Offset 28: uint32_t len         (buffer length)
 *   Offset 32: uint32_t addr2       (pointer to second buffer / path)
 *   Offset 36: uint32_t len2        (second buffer length)
 *   Offset 40: uint64_t user_data   (opaque, echoed in completion)
 *   Offset 48: uint32_t result_ptr  (pointer for output values)
 *   Offset 52: uint32_t flags2      (operation-specific flags)
 *   Offset 56: uint8_t  reserved[8]
 */

typedef struct wapi_io_op_t {
    uint32_t    opcode;     /* wapi_io_opcode_t */
    uint32_t    flags;      /* Operation flags */
    int32_t     fd;         /* Handle / file descriptor */
    uint32_t    _pad0;
    uint64_t    offset;     /* File offset or timeout in nanoseconds */
    uint32_t    addr;       /* Pointer to buffer (as i32 for wasm) */
    uint32_t    len;        /* Buffer length */
    uint32_t    addr2;      /* Second pointer (path for open, etc.) */
    uint32_t    len2;       /* Second length */
    uint64_t    user_data;  /* Echoed in completion, for correlation */
    uint32_t    result_ptr; /* Pointer to write output (bytes read, fd, etc.) */
    uint32_t    flags2;     /* Additional operation-specific flags */
    uint8_t     reserved[8];
} wapi_io_op_t;

/* Compile-time layout verification */
_Static_assert(sizeof(wapi_io_op_t) == 64, "wapi_io_op_t must be 64 bytes");

#ifdef __cplusplus
}
#endif

#endif /* WAPI_IO_H */
