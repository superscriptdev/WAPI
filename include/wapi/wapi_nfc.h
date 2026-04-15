/**
 * WAPI - NFC Capability
 * Version 1.0.0
 *
 * Near Field Communication for reading and writing NFC tags.
 *
 * Maps to: Web NFC API (Web), CoreNFC (iOS), NfcAdapter (Android)
 *
 * Import module: "wapi_nfc"
 *
 * Query availability with wapi_capability_supported("wapi.nfc", 7)
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
 * NFC Functions
 * ============================================================ */

/**
 * Start scanning for NFC tags.
 *
 * When a tag is read, a WAPI_EVENT_NFC_READ event is emitted.
 *
 * @return WAPI_OK on success, WAPI_ERR_ACCES if not permitted.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_nfc, scan_start)
wapi_result_t wapi_nfc_scan_start(void);

/**
 * Stop scanning for NFC tags.
 *
 * @return WAPI_OK on success.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_nfc, scan_stop)
wapi_result_t wapi_nfc_scan_stop(void);

/**
 * Write NDEF records to an NFC tag.
 *
 * @param records       Pointer to array of NDEF record descriptors.
 * @param record_count  Number of records to write.
 * @param tag           [out] Tag handle for the written tag.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_nfc, write)
wapi_result_t wapi_nfc_write(const void* records, uint32_t record_count,
                             wapi_handle_t* tag);

/**
 * Make an NFC tag read-only (permanent, cannot be undone).
 *
 * @param tag  Tag handle.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_nfc, make_read_only)
wapi_result_t wapi_nfc_make_read_only(wapi_handle_t tag);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_NFC_H */
