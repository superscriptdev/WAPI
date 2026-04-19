/**
 * WAPI - NFC
 * Version 1.0.0
 *
 * Near Field Communication for reading and writing NFC tags.
 *
 * Maps to: Web NFC API (Web), CoreNFC (iOS), NfcAdapter (Android)
 *
 * Import module: "wapi_nfc"
 */

#ifndef WAPI_NFC_H
#define WAPI_NFC_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * NFC Events
 * ============================================================ */

#define WAPI_EVENT_NFC_READ  0x1500

/* ============================================================
 * NFC Types
 * ============================================================ */

typedef enum wapi_nfc_record_type_t {
    WAPI_NFC_RECORD_TEXT         = 0,
    WAPI_NFC_RECORD_URL          = 1,
    WAPI_NFC_RECORD_MIME         = 2,
    WAPI_NFC_RECORD_ABSOLUTEURL = 3,
    WAPI_NFC_RECORD_EMPTY        = 4,
    WAPI_NFC_RECORD_UNKNOWN      = 5,
    WAPI_NFC_RECORD_FORCE32      = 0x7FFFFFFF
} wapi_nfc_record_type_t;

/* ============================================================
 * NFC Operations (async, submitted via wapi_io_t)
 * ============================================================ */

/** Submit a scan-start. Each detected tag fires a
 *  WAPI_EVENT_NFC_READ event. */
static inline wapi_result_t wapi_nfc_scan_start(
    const wapi_io_t* io, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_NFC_SCAN_START;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Stop scanning — cancels the scan_start op by its user_data. */
static inline wapi_result_t wapi_nfc_scan_stop(
    const wapi_io_t* io, uint64_t scan_user_data)
{
    return io->cancel(io->impl, scan_user_data);
}

/** Submit a write of NDEF records. */
static inline wapi_result_t wapi_nfc_write(
    const wapi_io_t* io, const void* records, uint32_t record_count,
    wapi_handle_t* out_tag, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_NFC_WRITE;
    op.addr       = (uint64_t)(uintptr_t)records;
    op.len        = (uint64_t)record_count * 32u; /* see wapi_shim for record layout */
    op.flags      = record_count;
    op.result_ptr = (uint64_t)(uintptr_t)out_tag;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/* make_read_only has no platform-standard async path; left unspec'd. */

#ifdef __cplusplus
}
#endif

#endif /* WAPI_NFC_H */
