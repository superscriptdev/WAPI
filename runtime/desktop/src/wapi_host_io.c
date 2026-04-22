/**
 * WAPI Desktop Runtime - I/O Bridge (central opcode dispatcher)
 *
 * Registers "wapi_io_bridge.*" as the host import module per wapi.h §4.
 * The wasm reactor shim wraps these into a wapi_io_t vtable that
 * modules obtain via wapi_io_get().
 *
 * Host imports (ten):
 *   submit(ops_ptr: i32, count: i64)       -> i32
 *   cancel(user_data: i64)                 -> i32
 *   poll(event_ptr: i32)                   -> i32
 *   wait(event_ptr: i32, timeout_ms: i32)  -> i32
 *   flush(event_type: i32)                 -> void
 *   cap_supported(name_ptr, name_len)      -> i32 (bool)
 *   cap_version(name_ptr, name_len, ver_ptr) -> i32
 *   cap_query(name_ptr, name_len, state_ptr) -> i32
 *   namespace_register(name_ptr, name_len, out_id_ptr) -> i32
 *   namespace_name(id, buf, buf_len, out_len_ptr) -> i32
 *
 * submit() dispatches on opcode to per-capability handler
 * functions declared in wapi_host.h and implemented across the
 * various wapi_host_*.c files. Ops whose handler isn't yet
 * implemented complete with WAPI_ERR_NOSYS and the
 * WAPI_IO_CQE_F_NOSYS flag set, per spec.
 *
 * The 80-byte wapi_io_op_t layout is:
 *   +0  u32 opcode         +4  u32 flags
 *   +8  i32 fd             +12 u32 flags2
 *   +16 u64 offset         +24 u64 addr
 *   +32 u64 len            +40 u64 addr2
 *   +48 u64 len2           +56 u64 user_data
 *   +64 u64 result_ptr     +72 u8[8] reserved
 */

#include "wapi_host.h"

/* ============================================================
 * Opcodes (must match wapi.h WAPI_IO_OP_*)
 * ============================================================ */

#define OP_NOP                       0x00
#define OP_CAP_REQUEST               0x01
#define OP_READ                      0x02
#define OP_WRITE                     0x03
#define OP_OPEN                      0x04
#define OP_CLOSE                     0x05
#define OP_STAT                      0x06
#define OP_LOG                       0x07
#define OP_FWATCH_ADD                0x08
#define OP_FWATCH_REMOVE             0x09
#define OP_CONNECT                   0x0A
#define OP_ACCEPT                    0x0B
#define OP_SEND                      0x0C
#define OP_RECV                      0x0D
#define OP_NETWORK_LISTEN            0x0E
#define OP_NETWORK_CHANNEL_OPEN      0x0F
#define OP_NETWORK_CHANNEL_ACCEPT    0x10
#define OP_NETWORK_RESOLVE           0x11
#define OP_TIMEOUT                   0x14
#define OP_TIMEOUT_ABS               0x15
#define OP_AUDIO_WRITE               0x1E
#define OP_AUDIO_READ                0x1F
#define OP_HTTP_FETCH                0x060
#define OP_SERIAL_PORT_REQUEST       0x080
#define OP_SERIAL_OPEN               0x081
#define OP_SERIAL_READ               0x082
#define OP_SERIAL_WRITE              0x083
#define OP_MIDI_ACCESS_REQUEST       0x090
#define OP_MIDI_PORT_OPEN            0x091
#define OP_MIDI_SEND                 0x092
#define OP_MIDI_RECV                 0x093
#define OP_BT_DEVICE_REQUEST         0x0A0
#define OP_BT_CONNECT                0x0A1
#define OP_BT_VALUE_READ             0x0A2
#define OP_BT_VALUE_WRITE            0x0A3
#define OP_BT_NOTIFICATIONS_START    0x0A4
#define OP_BT_SERVICE_GET            0x0A5
#define OP_BT_CHARACTERISTIC_GET     0x0A6
#define OP_USB_DEVICE_REQUEST        0x0B0
#define OP_USB_OPEN                  0x0B1
#define OP_USB_INTERFACE_CLAIM       0x0B2
#define OP_USB_TRANSFER_IN           0x0B3
#define OP_USB_TRANSFER_OUT          0x0B4
#define OP_USB_CONTROL_TRANSFER      0x0B5
#define OP_NFC_SCAN_START            0x0C0
#define OP_NFC_WRITE                 0x0C1
#define OP_CAMERA_OPEN               0x0D0
#define OP_CAMERA_FRAME_READ         0x0D1
#define OP_CODEC_DECODE              0x100
#define OP_CODEC_ENCODE              0x101
#define OP_CODEC_OUTPUT_GET          0x102
#define OP_CODEC_FLUSH               0x103
#define OP_VIDEO_CREATE              0x110
#define OP_VIDEO_SEEK                0x111
#define OP_SPEECH_SPEAK              0x120
#define OP_SPEECH_RECOGNIZE_START    0x121
#define OP_SPEECH_RECOGNIZE_RESULT   0x122
#define OP_CAPTURE_REQUEST           0x130
#define OP_CAPTURE_FRAME_GET         0x131
#define OP_COMPRESS_PROCESS          0x140
#define OP_FONT_BYTES_GET            0x150
#define OP_DIALOG_FILE_OPEN          0x180
#define OP_DIALOG_FILE_SAVE          0x181
#define OP_DIALOG_FOLDER_OPEN        0x182
#define OP_DIALOG_MESSAGEBOX         0x183
#define OP_DIALOG_PICK_COLOR         0x184
#define OP_DIALOG_PICK_FONT          0x185
#define OP_BIO_AUTHENTICATE          0x191
#define OP_AUTHN_CREDENTIAL_CREATE   0x192
#define OP_AUTHN_ASSERTION_GET       0x193
#define OP_CONTACTS_PICK             0x1A0
#define OP_EYEDROPPER_PICK           0x1A1
#define OP_PAY_PAYMENT_REQUEST       0x1A2
#define OP_CONTACTS_ICON_READ        0x1A3
#define OP_XR_SESSION_REQUEST        0x200
#define OP_XR_FRAME_WAIT             0x201
#define OP_XR_HIT_TEST               0x202
#define OP_GEO_POSITION_GET          0x210
#define OP_GEO_POSITION_WATCH        0x211
#define OP_SANDBOX_OPEN              0x2A0
#define OP_SANDBOX_STAT              0x2A1
#define OP_SANDBOX_DELETE            0x2A2
#define OP_SANDBOX_FWATCH_ADD        0x2A3
#define OP_SANDBOX_FWATCH_REMOVE     0x2A4
#define OP_CACHE_OPEN                0x2B0
#define OP_CACHE_STAT                0x2B1
#define OP_CACHE_DELETE              0x2B2
#define OP_CRYPTO_HASH               0x2C0
#define OP_CRYPTO_HASH_CREATE        0x2C1
#define OP_CRYPTO_ENCRYPT            0x2C2
#define OP_CRYPTO_DECRYPT            0x2C3
#define OP_CRYPTO_SIGN               0x2C4
#define OP_CRYPTO_VERIFY             0x2C5
#define OP_CRYPTO_DERIVE_KEY         0x2C6
#define OP_CRYPTO_KEY_IMPORT_RAW     0x2C7
#define OP_CRYPTO_KEY_GENERATE       0x2C8
#define OP_CRYPTO_KEY_GENERATE_PAIR  0x2C9
#define OP_BARCODE_DETECT_IMAGE      0x2D8
#define OP_BARCODE_DETECT_CAMERA     0x2D9
#define OP_POWER_INFO_GET            0x2E8
#define OP_POWER_WAKE_ACQUIRE        0x2E9
#define OP_POWER_IDLE_START          0x2EA
#define OP_SENSOR_START              0x2F0
#define OP_NOTIFY_SHOW               0x2F8
#define OP_FONT_FAMILY_INFO          0x2FC
#define OP_TRANSFER_OFFER            0x310
#define OP_TRANSFER_READ             0x311

/* Log levels (WAPI_LOG_*) */
#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3

/* I/O completion flags (WAPI_IO_CQE_F_*) */
#define CQE_F_MORE      0x0001
#define CQE_F_OVERFLOW  0x0002
#define CQE_F_INLINE    0x0004
#define CQE_F_NOSYS     0x0008

#define WAPI_EVENT_IO_COMPLETION 0x2000

/* ============================================================
 * Op dispatcher context — handed to every handler so handlers
 * don't need to re-parse the op. Handlers set completion via
 * *out_result and *out_flags.
 * ============================================================ */

/* op_ctx_t is defined in wapi_host.h so per-capability handlers can
 * share the exact layout without forward-decl duplication. */

/* ============================================================
 * Forward decls for per-capability handlers.
 *
 * Each handler owns the completion semantics — it fills
 * ctx->result, ctx->cqe_flags, and optionally inline payload.
 * Handlers not yet implemented are stubbed here to NOSYS via
 * the default path in dispatch().
 * ============================================================ */

/* Implemented in wapi_host_io.c (this file) below */
static void op_nop        (op_ctx_t* c);
static void op_timeout    (op_ctx_t* c);
static void op_timeout_abs(op_ctx_t* c);
static void op_log        (op_ctx_t* c);

/* File I/O — handled below for simple cases (READ/WRITE on FILE handle),
 * OPEN/CLOSE/STAT/FWATCH_* route to wapi_host_fs.c. */
static void op_read       (op_ctx_t* c);
static void op_write      (op_ctx_t* c);

/* Handlers defined in sibling files. Each returns via op_ctx_t:
 * extern declarations for future turns — each file provides these. */
extern void wapi_host_cap_request_op(op_ctx_t* c);             /* wapi_host_capability.c */
extern void wapi_host_fs_open_op(op_ctx_t* c);                 /* wapi_host_fs.c */
extern void wapi_host_fs_close_op(op_ctx_t* c);
extern void wapi_host_fs_stat_op(op_ctx_t* c);
extern void wapi_host_fs_fwatch_add_op(op_ctx_t* c);
extern void wapi_host_fs_fwatch_remove_op(op_ctx_t* c);
extern void wapi_host_net_connect_op(op_ctx_t* c);             /* wapi_host_net.c */
extern void wapi_host_net_accept_op(op_ctx_t* c);
extern void wapi_host_net_send_op(op_ctx_t* c);
extern void wapi_host_net_recv_op(op_ctx_t* c);
extern void wapi_host_net_listen_op(op_ctx_t* c);
extern void wapi_host_net_resolve_op(op_ctx_t* c);
extern void wapi_host_audio_write_op(op_ctx_t* c);             /* wapi_host_audio.c */
extern void wapi_host_audio_read_op(op_ctx_t* c);
extern void wapi_host_http_fetch_op(op_ctx_t* c);              /* wapi_host_http.c (future) */
extern void wapi_host_transfer_offer_op(op_ctx_t* c);          /* wapi_host_transfer.c */
extern void wapi_host_transfer_read_op(op_ctx_t* c);
extern void wapi_host_crypto_hash_op(op_ctx_t* c);             /* wapi_host_crypto.c */
extern void wapi_host_crypto_encrypt_op(op_ctx_t* c);
extern void wapi_host_crypto_decrypt_op(op_ctx_t* c);
extern void wapi_host_crypto_sign_op(op_ctx_t* c);
extern void wapi_host_crypto_verify_op(op_ctx_t* c);
extern void wapi_host_crypto_key_generate_op(op_ctx_t* c);
extern void wapi_host_crypto_key_generate_pair_op(op_ctx_t* c);
extern void wapi_host_crypto_key_import_raw_op(op_ctx_t* c);
extern void wapi_host_crypto_derive_key_op(op_ctx_t* c);
extern void wapi_host_crypto_hash_create_op(op_ctx_t* c);
extern void wapi_host_power_info_get_op(op_ctx_t* c);          /* wapi_host_power.c (future) */
extern void wapi_host_power_wake_acquire_op(op_ctx_t* c);
extern void wapi_host_power_idle_start_op(op_ctx_t* c);
extern void wapi_host_notify_show_op(op_ctx_t* c);             /* wapi_host_notifications.c (future) */
extern void wapi_host_font_bytes_get_op(op_ctx_t* c);          /* wapi_host_font.c */
extern void wapi_host_font_family_info_op(op_ctx_t* c);
extern void wapi_host_sensor_start_op(op_ctx_t* c);            /* wapi_host_sensors.c (future) */
extern void wapi_host_dialog_file_open_op(op_ctx_t* c);        /* wapi_host_dialog.c (future) */
extern void wapi_host_dialog_file_save_op(op_ctx_t* c);
extern void wapi_host_dialog_folder_open_op(op_ctx_t* c);
extern void wapi_host_dialog_messagebox_op(op_ctx_t* c);
extern void wapi_host_dialog_pick_color_op(op_ctx_t* c);
extern void wapi_host_dialog_pick_font_op(op_ctx_t* c);
extern void wapi_host_bio_authenticate_op(op_ctx_t* c);        /* wapi_host_biometric.c */
extern void wapi_host_authn_credential_create_op(op_ctx_t* c); /* wapi_host_authn.c */
extern void wapi_host_authn_assertion_get_op(op_ctx_t* c);
extern void wapi_host_contacts_pick_op(op_ctx_t* c);           /* wapi_host_contacts.c */
extern void wapi_host_contacts_icon_read_op(op_ctx_t* c);
extern void wapi_host_eyedropper_pick_op(op_ctx_t* c);         /* wapi_host_eyedropper.c (future) */
extern void wapi_host_pay_request_op(op_ctx_t* c);             /* wapi_host_payments.c */
extern void wapi_host_serial_port_request_op(op_ctx_t* c);     /* wapi_host_serial.c */
extern void wapi_host_serial_open_op(op_ctx_t* c);
extern void wapi_host_serial_read_op(op_ctx_t* c);
extern void wapi_host_serial_write_op(op_ctx_t* c);
extern void wapi_host_midi_access_request_op(op_ctx_t* c);     /* wapi_host_midi.c */
extern void wapi_host_midi_port_open_op(op_ctx_t* c);
extern void wapi_host_midi_send_op(op_ctx_t* c);
extern void wapi_host_midi_recv_op(op_ctx_t* c);
extern void wapi_host_bt_device_request_op(op_ctx_t* c);       /* wapi_host_bluetooth.c */
extern void wapi_host_bt_connect_op(op_ctx_t* c);
extern void wapi_host_bt_value_read_op(op_ctx_t* c);
extern void wapi_host_bt_value_write_op(op_ctx_t* c);
extern void wapi_host_bt_notifications_start_op(op_ctx_t* c);
extern void wapi_host_bt_service_get_op(op_ctx_t* c);
extern void wapi_host_bt_characteristic_get_op(op_ctx_t* c);
extern void wapi_host_usb_device_request_op(op_ctx_t* c);      /* wapi_host_usb.c */
extern void wapi_host_usb_open_op(op_ctx_t* c);
extern void wapi_host_usb_interface_claim_op(op_ctx_t* c);
extern void wapi_host_usb_transfer_in_op(op_ctx_t* c);
extern void wapi_host_usb_transfer_out_op(op_ctx_t* c);
extern void wapi_host_usb_control_transfer_op(op_ctx_t* c);
extern void wapi_host_nfc_scan_start_op(op_ctx_t* c);          /* wapi_host_nfc.c */
extern void wapi_host_nfc_write_op(op_ctx_t* c);
extern void wapi_host_camera_open_op(op_ctx_t* c);             /* wapi_host_camera.c */
extern void wapi_host_camera_frame_read_op(op_ctx_t* c);
extern void wapi_host_codec_decode_op(op_ctx_t* c);            /* wapi_host_codec.c */
extern void wapi_host_codec_encode_op(op_ctx_t* c);
extern void wapi_host_codec_output_get_op(op_ctx_t* c);
extern void wapi_host_codec_flush_op(op_ctx_t* c);
extern void wapi_host_video_create_op(op_ctx_t* c);            /* wapi_host_video.c */
extern void wapi_host_video_seek_op(op_ctx_t* c);
extern void wapi_host_compress_process_op(op_ctx_t* c);        /* wapi_host_compression.c */
extern void wapi_host_speech_speak_op(op_ctx_t* c);            /* wapi_host_speech.c */
extern void wapi_host_speech_recognize_start_op(op_ctx_t* c);
extern void wapi_host_speech_recognize_result_op(op_ctx_t* c);
extern void wapi_host_capture_request_op(op_ctx_t* c);         /* wapi_host_screen_capture.c */
extern void wapi_host_capture_frame_get_op(op_ctx_t* c);
extern void wapi_host_xr_session_request_op(op_ctx_t* c);      /* wapi_host_xr.c */
extern void wapi_host_xr_frame_wait_op(op_ctx_t* c);
extern void wapi_host_xr_hit_test_op(op_ctx_t* c);
extern void wapi_host_geo_position_get_op(op_ctx_t* c);        /* wapi_host_geolocation.c */
extern void wapi_host_geo_position_watch_op(op_ctx_t* c);
extern void wapi_host_sandbox_open_op(op_ctx_t* c);            /* wapi_host_fs.c (sandbox path) */
extern void wapi_host_sandbox_stat_op(op_ctx_t* c);
extern void wapi_host_sandbox_delete_op(op_ctx_t* c);
extern void wapi_host_sandbox_fwatch_add_op(op_ctx_t* c);
extern void wapi_host_sandbox_fwatch_remove_op(op_ctx_t* c);
extern void wapi_host_cache_open_op(op_ctx_t* c);              /* wapi_host_fs.c (cache path) */
extern void wapi_host_cache_stat_op(op_ctx_t* c);
extern void wapi_host_cache_delete_op(op_ctx_t* c);
extern void wapi_host_barcode_detect_image_op(op_ctx_t* c);    /* wapi_host_barcode.c */
extern void wapi_host_barcode_detect_camera_op(op_ctx_t* c);

/* A handler that signals "not yet implemented" via NOSYS. Safe
 * default while modules are being filled in. */
static void op_nosys(op_ctx_t* c) {
    c->result = WAPI_ERR_NOSYS;
    c->cqe_flags |= CQE_F_NOSYS;
}

/* ============================================================
 * Completion event
 * ============================================================ */

static void push_completion_event(const op_ctx_t* c) {
    if (c->suppress_completion) return;

    wapi_host_event_t ev; memset(&ev, 0, sizeof(ev));
    uint32_t type = WAPI_EVENT_IO_COMPLETION;
    uint32_t surface_id = 0;
    uint64_t ts = wapi_plat_time_monotonic_ns();
    uint32_t flags = c->cqe_flags;
    if (c->inline_payload) flags |= CQE_F_INLINE;

    memcpy(ev.data + 0,  &type,          4);
    memcpy(ev.data + 4,  &surface_id,    4);
    memcpy(ev.data + 8,  &ts,            8);
    memcpy(ev.data + 16, &c->result,     4);
    memcpy(ev.data + 20, &flags,         4);
    memcpy(ev.data + 24, &c->user_data,  8);

    if (c->inline_payload) {
        memcpy(ev.data + 32, c->payload, 96);
    } else if (c->result_ptr != 0 && c->result >= 0) {
        /* Some opcodes write small values (like handles) to
         * result_ptr — handler is responsible for that write
         * via wapi_wasm_write_*; nothing to copy here. */
    }

    wapi_event_queue_push(&ev);
    wapi_plat_wake();
}

/* ============================================================
 * Timeouts — implemented inline here
 * ============================================================ */

static wapi_io_queue_t* default_io_queue(void) {
    for (int h = 4; h < WAPI_MAX_HANDLES; h++) {
        if (g_rt.handles[h].type == WAPI_HTYPE_IO_QUEUE)
            return g_rt.handles[h].data.io_queue;
    }
    int32_t qh = wapi_handle_alloc(WAPI_HTYPE_IO_QUEUE);
    if (qh == 0) return NULL;
    wapi_io_queue_t* q = (wapi_io_queue_t*)calloc(1, sizeof(wapi_io_queue_t));
    if (!q) { wapi_handle_free(qh); return NULL; }
    q->capacity = WAPI_IO_QUEUE_MAX_OPS;
    g_rt.handles[qh].data.io_queue = q;
    return q;
}

void wapi_io_check_timeouts(void) {
    uint64_t now = wapi_plat_time_monotonic_ns();
    for (int h = 4; h < WAPI_MAX_HANDLES; h++) {
        if (g_rt.handles[h].type != WAPI_HTYPE_IO_QUEUE) continue;
        wapi_io_queue_t* q = g_rt.handles[h].data.io_queue;
        if (!q) continue;
        for (int i = 0; i < q->timeout_count; i++) {
            if (q->timeouts[i].active && now >= q->timeouts[i].deadline_ns) {
                op_ctx_t fake = {0};
                fake.user_data = q->timeouts[i].user_data;
                fake.result = WAPI_OK;
                push_completion_event(&fake);
                q->timeouts[i].active = false;
            }
        }
    }
}

static void op_timeout(op_ctx_t* c) {
    wapi_io_queue_t* q = default_io_queue();
    if (!q || q->timeout_count >= 64) {
        c->result = WAPI_ERR_NOSPC;
        return;
    }
    int slot = -1;
    for (int t = 0; t < q->timeout_count; t++) {
        if (!q->timeouts[t].active) { slot = t; break; }
    }
    if (slot < 0) { slot = q->timeout_count; q->timeout_count++; }
    q->timeouts[slot].user_data   = c->user_data;
    q->timeouts[slot].deadline_ns = wapi_plat_time_monotonic_ns() + c->offset;
    q->timeouts[slot].active      = true;
    c->suppress_completion = true; /* completion fires on timer expiry */
}

static void op_timeout_abs(op_ctx_t* c) {
    wapi_io_queue_t* q = default_io_queue();
    if (!q || q->timeout_count >= 64) {
        c->result = WAPI_ERR_NOSPC;
        return;
    }
    int slot = -1;
    for (int t = 0; t < q->timeout_count; t++) {
        if (!q->timeouts[t].active) { slot = t; break; }
    }
    if (slot < 0) { slot = q->timeout_count; q->timeout_count++; }
    q->timeouts[slot].user_data   = c->user_data;
    q->timeouts[slot].deadline_ns = c->offset;
    q->timeouts[slot].active      = true;
    c->suppress_completion = true;
}

/* ============================================================
 * Nop / log / read / write
 * ============================================================ */

static void op_nop(op_ctx_t* c) {
    c->result = WAPI_OK;
}

static void op_log(op_ctx_t* c) {
    const char* prefix = "INFO";
    switch (c->flags) {
    case LOG_DEBUG: prefix = "DEBUG"; break;
    case LOG_INFO:  prefix = "INFO";  break;
    case LOG_WARN:  prefix = "WARN";  break;
    case LOG_ERROR: prefix = "ERROR"; break;
    }
    const char* msg = (const char*)wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len);
    const char* tag = (c->addr2 && c->len2)
        ? (const char*)wapi_wasm_ptr((uint32_t)c->addr2, (uint32_t)c->len2)
        : NULL;
    if (msg) {
        if (tag) fprintf(stderr, "[%s][%.*s] %.*s\n", prefix, (int)c->len2, tag, (int)c->len, msg);
        else     fprintf(stderr, "[%s] %.*s\n", prefix, (int)c->len, msg);
    }
    c->suppress_completion = true; /* log is fire-and-forget */
}

static void op_read(op_ctx_t* c) {
    if (!wapi_handle_valid(c->fd, WAPI_HTYPE_FILE)) { c->result = WAPI_ERR_BADF; return; }
    FILE* f = g_rt.handles[c->fd].data.file;
    void* buf = wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len);
    if (!buf) { c->result = WAPI_ERR_INVAL; return; }
    if (c->offset != 0) fseek(f, (long)c->offset, SEEK_SET);
    size_t br = fread(buf, 1, (size_t)c->len, f);
    c->result = (int32_t)br;
    if (c->result_ptr) wapi_wasm_write_u64((uint32_t)c->result_ptr, (uint64_t)br);
}

static void op_write(op_ctx_t* c) {
    if (!wapi_handle_valid(c->fd, WAPI_HTYPE_FILE)) { c->result = WAPI_ERR_BADF; return; }
    FILE* f = g_rt.handles[c->fd].data.file;
    void* buf = wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len);
    if (!buf) { c->result = WAPI_ERR_INVAL; return; }
    if (c->offset != 0) fseek(f, (long)c->offset, SEEK_SET);
    size_t bw = fwrite(buf, 1, (size_t)c->len, f);
    fflush(f);
    c->result = (int32_t)bw;
    if (c->result_ptr) wapi_wasm_write_u64((uint32_t)c->result_ptr, (uint64_t)bw);
}

/* ============================================================
 * Capability request — routed here for now. Full impl queries
 * a capability table in the host; for phase 2 auto-grant.
 * ============================================================ */

static void op_cap_request(op_ctx_t* c) {
    /* addr/len = capability name; result_ptr = wapi_cap_state_t* */
    /* For now: auto-grant everything. Full wire-up goes through
     * wapi_host_capability.c's wapi_host_cap_request_op. */
    if (c->result_ptr) {
        uint32_t state = 0; /* WAPI_CAP_GRANTED */
        wapi_wasm_write_u32((uint32_t)c->result_ptr, state);
    }
    c->result = WAPI_OK;
}

/* ============================================================
 * Dispatch table
 * ============================================================ */

static void dispatch(op_ctx_t* c) {
    switch (c->opcode) {
    case OP_NOP:           op_nop(c); break;
    case OP_CAP_REQUEST:   op_cap_request(c); break;
    case OP_READ:          op_read(c); break;
    case OP_WRITE:         op_write(c); break;
    case OP_LOG:           op_log(c); break;
    case OP_TIMEOUT:       op_timeout(c); break;
    case OP_TIMEOUT_ABS:   op_timeout_abs(c); break;

    /* Everything below dispatches to handlers in sibling files.
     * Until each is implemented, the linker will miss the symbol
     * and fail — so for now we route to op_nosys. When a handler
     * gets implemented, flip the case to call it. */

    case OP_OPEN:                    op_nosys(c); break;
    case OP_CLOSE:                   op_nosys(c); break;
    case OP_STAT:                    op_nosys(c); break;
    case OP_FWATCH_ADD:              op_nosys(c); break;
    case OP_FWATCH_REMOVE:           op_nosys(c); break;

    case OP_CONNECT:                 op_nosys(c); break;
    case OP_ACCEPT:                  op_nosys(c); break;
    case OP_SEND:                    op_nosys(c); break;
    case OP_RECV:                    op_nosys(c); break;
    case OP_NETWORK_LISTEN:          op_nosys(c); break;
    case OP_NETWORK_CHANNEL_OPEN:    op_nosys(c); break;
    case OP_NETWORK_CHANNEL_ACCEPT:  op_nosys(c); break;
    case OP_NETWORK_RESOLVE:         op_nosys(c); break;

    case OP_AUDIO_WRITE:             op_nosys(c); break;
    case OP_AUDIO_READ:              op_nosys(c); break;

    case OP_HTTP_FETCH:              op_nosys(c); break;

    case OP_SERIAL_PORT_REQUEST:     op_nosys(c); break;
    case OP_SERIAL_OPEN:             op_nosys(c); break;
    case OP_SERIAL_READ:             op_nosys(c); break;
    case OP_SERIAL_WRITE:            op_nosys(c); break;

    case OP_MIDI_ACCESS_REQUEST:     op_nosys(c); break;
    case OP_MIDI_PORT_OPEN:          op_nosys(c); break;
    case OP_MIDI_SEND:               op_nosys(c); break;
    case OP_MIDI_RECV:               op_nosys(c); break;

    case OP_BT_DEVICE_REQUEST:       op_nosys(c); break;
    case OP_BT_CONNECT:              op_nosys(c); break;
    case OP_BT_VALUE_READ:           op_nosys(c); break;
    case OP_BT_VALUE_WRITE:          op_nosys(c); break;
    case OP_BT_NOTIFICATIONS_START:  op_nosys(c); break;
    case OP_BT_SERVICE_GET:          op_nosys(c); break;
    case OP_BT_CHARACTERISTIC_GET:   op_nosys(c); break;

    case OP_USB_DEVICE_REQUEST:      op_nosys(c); break;
    case OP_USB_OPEN:                op_nosys(c); break;
    case OP_USB_INTERFACE_CLAIM:     op_nosys(c); break;
    case OP_USB_TRANSFER_IN:         op_nosys(c); break;
    case OP_USB_TRANSFER_OUT:        op_nosys(c); break;
    case OP_USB_CONTROL_TRANSFER:    op_nosys(c); break;

    case OP_NFC_SCAN_START:          op_nosys(c); break;
    case OP_NFC_WRITE:               op_nosys(c); break;

    case OP_CAMERA_OPEN:             op_nosys(c); break;
    case OP_CAMERA_FRAME_READ:       op_nosys(c); break;

    case OP_CODEC_DECODE:            op_nosys(c); break;
    case OP_CODEC_ENCODE:            op_nosys(c); break;
    case OP_CODEC_OUTPUT_GET:        op_nosys(c); break;
    case OP_CODEC_FLUSH:             op_nosys(c); break;

    case OP_VIDEO_CREATE:            op_nosys(c); break;
    case OP_VIDEO_SEEK:              op_nosys(c); break;

    case OP_SPEECH_SPEAK:            op_nosys(c); break;
    case OP_SPEECH_RECOGNIZE_START:  op_nosys(c); break;
    case OP_SPEECH_RECOGNIZE_RESULT: op_nosys(c); break;

    case OP_CAPTURE_REQUEST:         op_nosys(c); break;
    case OP_CAPTURE_FRAME_GET:       op_nosys(c); break;

    case OP_COMPRESS_PROCESS:        wapi_host_compress_process_op(c); break;

    case OP_FONT_BYTES_GET:          op_nosys(c); break;
    case OP_FONT_FAMILY_INFO:        op_nosys(c); break;

    case OP_DIALOG_FILE_OPEN:        op_nosys(c); break;
    case OP_DIALOG_FILE_SAVE:        op_nosys(c); break;
    case OP_DIALOG_FOLDER_OPEN:      op_nosys(c); break;
    case OP_DIALOG_MESSAGEBOX:       op_nosys(c); break;
    case OP_DIALOG_PICK_COLOR:       op_nosys(c); break;
    case OP_DIALOG_PICK_FONT:        op_nosys(c); break;

    case OP_BIO_AUTHENTICATE:        op_nosys(c); break;
    case OP_AUTHN_CREDENTIAL_CREATE: op_nosys(c); break;
    case OP_AUTHN_ASSERTION_GET:     op_nosys(c); break;

    case OP_CONTACTS_PICK:           op_nosys(c); break;
    case OP_CONTACTS_ICON_READ:      op_nosys(c); break;
    case OP_EYEDROPPER_PICK:         op_nosys(c); break;
    case OP_PAY_PAYMENT_REQUEST:     op_nosys(c); break;

    case OP_XR_SESSION_REQUEST:      op_nosys(c); break;
    case OP_XR_FRAME_WAIT:           op_nosys(c); break;
    case OP_XR_HIT_TEST:             op_nosys(c); break;

    case OP_GEO_POSITION_GET:        op_nosys(c); break;
    case OP_GEO_POSITION_WATCH:      op_nosys(c); break;

    case OP_SANDBOX_OPEN:            op_nosys(c); break;
    case OP_SANDBOX_STAT:            op_nosys(c); break;
    case OP_SANDBOX_DELETE:          op_nosys(c); break;
    case OP_SANDBOX_FWATCH_ADD:      op_nosys(c); break;
    case OP_SANDBOX_FWATCH_REMOVE:   op_nosys(c); break;

    case OP_CACHE_OPEN:              op_nosys(c); break;
    case OP_CACHE_STAT:              op_nosys(c); break;
    case OP_CACHE_DELETE:            op_nosys(c); break;

    case OP_CRYPTO_HASH:             op_nosys(c); break;
    case OP_CRYPTO_HASH_CREATE:      op_nosys(c); break;
    case OP_CRYPTO_ENCRYPT:          op_nosys(c); break;
    case OP_CRYPTO_DECRYPT:          op_nosys(c); break;
    case OP_CRYPTO_SIGN:             op_nosys(c); break;
    case OP_CRYPTO_VERIFY:           op_nosys(c); break;
    case OP_CRYPTO_DERIVE_KEY:       op_nosys(c); break;
    case OP_CRYPTO_KEY_IMPORT_RAW:   op_nosys(c); break;
    case OP_CRYPTO_KEY_GENERATE:     op_nosys(c); break;
    case OP_CRYPTO_KEY_GENERATE_PAIR:op_nosys(c); break;

    case OP_BARCODE_DETECT_IMAGE:    op_nosys(c); break;
    case OP_BARCODE_DETECT_CAMERA:   op_nosys(c); break;

    case OP_POWER_INFO_GET:          op_nosys(c); break;
    case OP_POWER_WAKE_ACQUIRE:      op_nosys(c); break;
    case OP_POWER_IDLE_START:        op_nosys(c); break;

    case OP_SENSOR_START:            op_nosys(c); break;

    case OP_NOTIFY_SHOW:             op_nosys(c); break;

    case OP_TRANSFER_OFFER:          wapi_host_transfer_offer_op(c); break;
    case OP_TRANSFER_READ:           wapi_host_transfer_read_op(c);  break;

    default:
        c->result = WAPI_ERR_NOSYS;
        c->cqe_flags |= CQE_F_NOSYS;
        break;
    }
}

/* ============================================================
 * Read an op from wasm memory
 * ============================================================ */

static bool read_op(uint32_t wasm_ptr, op_ctx_t* c) {
    uint8_t* b = (uint8_t*)wapi_wasm_ptr(wasm_ptr, 80);
    if (!b) return false;
    memcpy(&c->opcode,     b +  0, 4);
    memcpy(&c->flags,      b +  4, 4);
    memcpy(&c->fd,         b +  8, 4);
    memcpy(&c->flags2,     b + 12, 4);
    memcpy(&c->offset,     b + 16, 8);
    memcpy(&c->addr,       b + 24, 8);
    memcpy(&c->len,        b + 32, 8);
    memcpy(&c->addr2,      b + 40, 8);
    memcpy(&c->len2,       b + 48, 8);
    memcpy(&c->user_data,  b + 56, 8);
    memcpy(&c->result_ptr, b + 64, 8);
    c->result = 0;
    c->cqe_flags = 0;
    c->inline_payload = false;
    c->suppress_completion = false;
    return true;
}

/* ============================================================
 * Host imports
 * ============================================================ */

static wasm_trap_t* cb_submit(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t ops_ptr = WAPI_ARG_U32(0);
    uint64_t count   = WAPI_ARG_U64(1);
    if (count == 0) { WAPI_RET_I32(0); return NULL; }

    int submitted = 0;
    for (uint64_t i = 0; i < count; i++) {
        op_ctx_t c = {0};
        if (!read_op(ops_ptr + (uint32_t)(i * 80), &c)) break;
        dispatch(&c);
        push_completion_event(&c);
        submitted++;
    }
    WAPI_RET_I32(submitted);
    return NULL;
}

static wasm_trap_t* cb_cancel(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint64_t user_data = WAPI_ARG_U64(0);
    bool found = false;
    for (int h = 4; h < WAPI_MAX_HANDLES && !found; h++) {
        if (g_rt.handles[h].type != WAPI_HTYPE_IO_QUEUE) continue;
        wapi_io_queue_t* q = g_rt.handles[h].data.io_queue;
        if (!q) continue;
        for (int i = 0; i < q->timeout_count; i++) {
            if (q->timeouts[i].active && q->timeouts[i].user_data == user_data) {
                q->timeouts[i].active = false;
                op_ctx_t fake = {0};
                fake.user_data = user_data;
                fake.result = WAPI_ERR_CANCELED;
                push_completion_event(&fake);
                found = true;
                break;
            }
        }
    }
    WAPI_RET_I32(found ? WAPI_OK : WAPI_ERR_NOENT);
    return NULL;
}

static wasm_trap_t* cb_poll(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t event_ptr = WAPI_ARG_U32(0);
    wapi_host_pump_platform_events();
    wapi_io_check_timeouts();

    wapi_host_event_t ev;
    if (wapi_event_queue_pop(&ev)) {
        wapi_wasm_write_bytes(event_ptr, ev.data, 128);
        WAPI_RET_I32(1);
    } else {
        uint8_t zeros[128]; memset(zeros, 0, sizeof(zeros));
        wapi_wasm_write_bytes(event_ptr, zeros, 128);
        WAPI_RET_I32(0);
    }
    return NULL;
}

static wasm_trap_t* cb_wait(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t event_ptr = WAPI_ARG_U32(0);
    int32_t  timeout_ms = WAPI_ARG_I32(1);

    wapi_host_pump_platform_events();
    wapi_io_check_timeouts();

    wapi_host_event_t ev;
    if (wapi_event_queue_pop(&ev)) {
        wapi_wasm_write_bytes(event_ptr, ev.data, 128);
        WAPI_RET_I32(1); return NULL;
    }

    int64_t timeout_ns = (timeout_ms < 0) ? -1 : (int64_t)timeout_ms * 1000000LL;
    wapi_plat_wait_events(timeout_ns);
    wapi_host_pump_platform_events();
    wapi_io_check_timeouts();

    if (wapi_event_queue_pop(&ev)) {
        wapi_wasm_write_bytes(event_ptr, ev.data, 128);
        WAPI_RET_I32(1);
    } else {
        uint8_t zeros[128]; memset(zeros, 0, sizeof(zeros));
        wapi_wasm_write_bytes(event_ptr, zeros, 128);
        WAPI_RET_I32(0);
    }
    return NULL;
}

static wasm_trap_t* cb_flush(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)results; (void)nresults;
    wapi_event_queue_flush(WAPI_ARG_U32(0));
    return NULL;
}

/* Read a wapi_stringview_t from a guest pointer (16 bytes: u64 data + u64 length).
 * Returns host pointer to UTF-8 bytes (may be NULL if guest data ptr is 0). */
static const char* read_wapi_sv(uint32_t sv_ptr, uint32_t* out_len) {
    *out_len = 0;
    if (!sv_ptr) return NULL;
    void* p = wapi_wasm_ptr(sv_ptr, 16);
    if (!p) return NULL;
    uint64_t data, length;
    memcpy(&data,   (uint8_t*)p + 0, 8);
    memcpy(&length, (uint8_t*)p + 8, 8);
    if (length == (uint64_t)-1) {
        /* WAPI_STRLEN: null-terminated — measure. */
        const char* s = (const char*)wapi_wasm_ptr((uint32_t)data, 1);
        if (!s) return NULL;
        uint32_t n = 0; while (s[n]) n++;
        *out_len = n;
        return s;
    }
    if (length > 0x7FFFFFFF) return NULL;
    *out_len = (uint32_t)length;
    if (*out_len == 0) return "";
    return (const char*)wapi_wasm_ptr((uint32_t)data, *out_len);
}

typedef struct {
    const char*       name;
    wapi_cap_state_t  state;
    uint16_t          major, minor, patch;
} cap_entry_t;

static cap_entry_t g_caps[] = {
    { "wapi.env",           0, 1, 0, 0 },
    { "wapi.memory",        0, 1, 0, 0 },
    { "wapi.io",            0, 1, 0, 0 },
    { "wapi.clock",         0, 1, 0, 0 },
    { "wapi.random",        0, 1, 0, 0 },
    { "wapi.filesystem",    0, 1, 0, 0 },
    { "wapi.net",           0, 1, 0, 0 },
    { "wapi.gpu",           0, 1, 0, 0 },
    { "wapi.surface",       0, 1, 0, 0 },
    { "wapi.window",        0, 1, 0, 0 },
    { "wapi.input",         0, 1, 0, 0 },
    { "wapi.audio",         0, 1, 0, 0 },
    { "wapi.text",          0, 1, 0, 0 },
    { "wapi.clipboard",     0, 1, 0, 0 },
    { "wapi.font",          0, 1, 0, 0 },
    { "wapi.kv_storage",    0, 1, 0, 0 },
    { "wapi.crypto",        0, 1, 0, 0 },
    { "wapi.video",         0, 1, 0, 0 },
    { "wapi.module",        0, 1, 0, 0 },
    { "wapi.notifications", 0, 1, 0, 0 },
    { "wapi.permissions",   0, 1, 0, 0 },
};
#define CAP_COUNT ((int)(sizeof(g_caps) / sizeof(g_caps[0])))

static cap_entry_t* find_cap(const char* name, uint32_t len) {
    for (int i = 0; i < CAP_COUNT; i++) {
        size_t cl = strlen(g_caps[i].name);
        if (cl == len && memcmp(g_caps[i].name, name, len) == 0)
            return &g_caps[i];
    }
    return NULL;
}

typedef struct {
    char     name[128];
    uint16_t id;
    bool     in_use;
} namespace_entry_t;
static namespace_entry_t g_namespaces[64];
static uint16_t g_next_namespace_id = 1;

static wasm_trap_t* cb_cap_supported(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t sv_ptr = WAPI_ARG_U32(0);
    uint32_t nlen = 0;
    const char* name = read_wapi_sv(sv_ptr, &nlen);
    cap_entry_t* c = (name && nlen) ? find_cap(name, nlen) : NULL;
    WAPI_RET_I32(c ? 1 : 0);
    return NULL;
}

static wasm_trap_t* cb_cap_version(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t sv_ptr  = WAPI_ARG_U32(0);
    uint32_t ver_ptr = WAPI_ARG_U32(1);
    uint32_t nlen = 0;
    const char* name = read_wapi_sv(sv_ptr, &nlen);
    cap_entry_t* c = (name && nlen) ? find_cap(name, nlen) : NULL;
    if (!c) {
        if (ver_ptr) {
            uint16_t z[4] = {0,0,0,0};
            wapi_wasm_write_bytes(ver_ptr, z, sizeof(z));
        }
        WAPI_RET_I32(WAPI_ERR_NOTCAPABLE);
        return NULL;
    }
    if (ver_ptr) {
        uint16_t v[4] = { c->major, c->minor, c->patch, 0 };
        wapi_wasm_write_bytes(ver_ptr, v, sizeof(v));
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* cb_cap_query(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t sv_ptr    = WAPI_ARG_U32(0);
    uint32_t state_ptr = WAPI_ARG_U32(1);
    uint32_t nlen = 0;
    const char* name = read_wapi_sv(sv_ptr, &nlen);
    cap_entry_t* c = (name && nlen) ? find_cap(name, nlen) : NULL;
    if (!c) {
        if (state_ptr) wapi_wasm_write_u32(state_ptr, 3 /* WAPI_CAP_DENIED */);
        WAPI_RET_I32(WAPI_ERR_NOTCAPABLE);
        return NULL;
    }
    if (state_ptr) wapi_wasm_write_u32(state_ptr, (uint32_t)c->state);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* cb_namespace_register(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t sv_ptr     = WAPI_ARG_U32(0);
    uint32_t out_id_ptr = WAPI_ARG_U32(1);
    uint32_t nlen = 0;
    const char* name = read_wapi_sv(sv_ptr, &nlen);
    if (!name || !nlen || nlen >= sizeof(g_namespaces[0].name)) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    for (size_t i = 0; i < sizeof(g_namespaces)/sizeof(g_namespaces[0]); i++) {
        if (!g_namespaces[i].in_use) continue;
        if (strlen(g_namespaces[i].name) == nlen &&
            memcmp(g_namespaces[i].name, name, nlen) == 0) {
            uint16_t id = g_namespaces[i].id;
            if (out_id_ptr) wapi_wasm_write_bytes(out_id_ptr, &id, 2);
            WAPI_RET_I32(WAPI_OK); return NULL;
        }
    }
    for (size_t i = 0; i < sizeof(g_namespaces)/sizeof(g_namespaces[0]); i++) {
        if (g_namespaces[i].in_use) continue;
        memcpy(g_namespaces[i].name, name, nlen);
        g_namespaces[i].name[nlen] = '\0';
        g_namespaces[i].id = g_next_namespace_id++;
        g_namespaces[i].in_use = true;
        uint16_t id = g_namespaces[i].id;
        if (out_id_ptr) wapi_wasm_write_bytes(out_id_ptr, &id, 2);
        WAPI_RET_I32(WAPI_OK); return NULL;
    }
    WAPI_RET_I32(WAPI_ERR_NOSPC);
    return NULL;
}

static wasm_trap_t* cb_namespace_name(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t id      = WAPI_ARG_U32(0);
    uint32_t buf     = WAPI_ARG_U32(1);
    uint64_t buf_len = WAPI_ARG_U64(2);
    uint32_t out_ptr = WAPI_ARG_U32(3);
    for (size_t i = 0; i < sizeof(g_namespaces)/sizeof(g_namespaces[0]); i++) {
        if (!g_namespaces[i].in_use || g_namespaces[i].id != (uint16_t)id) continue;
        size_t nlen = strlen(g_namespaces[i].name);
        if (out_ptr) wapi_wasm_write_u64(out_ptr, (uint64_t)nlen);
        size_t copy = (buf_len < nlen) ? (size_t)buf_len : nlen;
        if (buf && copy) wapi_wasm_write_bytes(buf, g_namespaces[i].name, (uint32_t)copy);
        WAPI_RET_I32(WAPI_OK); return NULL;
    }
    WAPI_RET_I32(WAPI_ERR_NOENT);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================
 * Module name: "wapi_io_bridge".
 */

void wapi_host_register_io(wasmtime_linker_t* linker) {
    /* submit(ops_ptr: i32, count: i64) -> i32 */
    wapi_linker_define(linker, "wapi_io_bridge", "submit", cb_submit,
        2, (wasm_valkind_t[]){WASM_I32, WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});

    /* cancel(user_data: i64) -> i32 */
    wapi_linker_define(linker, "wapi_io_bridge", "cancel", cb_cancel,
        1, (wasm_valkind_t[]){WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});

    /* poll(event_ptr: i32) -> i32 */
    WAPI_DEFINE_1_1(linker, "wapi_io_bridge", "poll",  cb_poll);

    /* wait(event_ptr: i32, timeout_ms: i32) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_io_bridge", "wait",  cb_wait);

    /* flush(event_type: i32) -> void */
    WAPI_DEFINE_1_0(linker, "wapi_io_bridge", "flush", cb_flush);

    /* wapi_stringview_t is 16 bytes — clang's wasm32 ABI passes it by
     * pointer (sret-style), so each of the below takes an i32 pointer
     * to the stringview struct in guest memory. */

    /* cap_supported(sv_ptr: i32) -> i32 */
    WAPI_DEFINE_1_1(linker, "wapi_io_bridge", "cap_supported", cb_cap_supported);

    /* cap_version(sv_ptr: i32, ver_ptr: i32) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_io_bridge", "cap_version", cb_cap_version);

    /* cap_query(sv_ptr: i32, state_ptr: i32) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_io_bridge", "cap_query", cb_cap_query);

    /* namespace_register(sv_ptr: i32, out_id_ptr: i32) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_io_bridge", "namespace_register", cb_namespace_register);

    /* namespace_name(id: i32, buf: i32, buf_len: i64, out_len: i32) -> i32 */
    wapi_linker_define(linker, "wapi_io_bridge", "namespace_name", cb_namespace_name,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
}
