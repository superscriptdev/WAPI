/**
 * WAPI - Contacts
 * Version 2.0.0
 *
 * Access device contact information via a picker interface.
 *
 * Maps to: Contact Picker API (Web), CNContactStore (iOS/macOS),
 *          ContactsContract (Android), Windows.ApplicationModel.Contacts
 *          (Windows), libfolks (Linux)
 *
 * Import module: "wapi_contacts"
 *
 * ============================================================
 * Serialization Format (results_buf filled by host)
 * ============================================================
 *
 * All integers little-endian.
 *
 * Contact (repeated `count` times, count = return value of pick):
 *   +0  byte_len    : u32  (bytes following this field for this contact)
 *   +4  prop_count  : u16  (number of property blocks)
 *   +6  _pad        : u16
 *
 *   Property (repeated prop_count times):
 *     +0  tag         : u16  (wapi_contact_prop_t value)
 *     +2  val_count   : u16  (0 = requested but empty/withheld)
 *
 *     Value (repeated val_count times):
 *       +0  label     : u8   (wapi_contact_label_t)
 *       +1  _pad      : u8
 *       +2  len       : u16  (data length in bytes)
 *       +4  data      : [len] bytes (UTF-8, not null-terminated)
 *             padded to next 4-byte boundary
 *
 * Three states per property:
 *   Not requested       -> tag absent from contact (not in properties_mask)
 *   Requested, empty    -> tag present, val_count = 0
 *   Has data            -> tag present, val_count > 0
 *
 * Icon special case (tag = WAPI_CONTACT_ICON):
 *   val_count = 1, label = 0, len = 4, data = u32 icon handle (LE).
 *   Use wapi_contacts_icon_read() to fetch image bytes.
 *   val_count = 0 means no icon available.
 */

#ifndef WAPI_CONTACTS_H
#define WAPI_CONTACTS_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Contact Properties (bitmask)
 * ============================================================ */

typedef enum wapi_contact_prop_t {
    WAPI_CONTACT_NAME     = 0x0001,  /* Full display name (single value) */
    WAPI_CONTACT_EMAIL    = 0x0002,  /* Email addresses (multi-value, labeled) */
    WAPI_CONTACT_TEL      = 0x0004,  /* Phone numbers (multi-value, labeled) */
    WAPI_CONTACT_ADDRESS  = 0x0008,  /* Postal addresses (multi-value, labeled) */
    WAPI_CONTACT_ICON     = 0x0010,  /* Avatar handle (see icon_read) */
    WAPI_CONTACT_ORG      = 0x0020,  /* Organization / company name */
    WAPI_CONTACT_TITLE    = 0x0040,  /* Job title */
    WAPI_CONTACT_NICKNAME = 0x0080,  /* Nickname */
    WAPI_CONTACT_BIRTHDAY = 0x0100,  /* ISO 8601 date "YYYY-MM-DD" */
    WAPI_CONTACT_URL      = 0x0200,  /* URLs / websites (multi-value, labeled) */
    WAPI_CONTACT_NOTE     = 0x0400,  /* Free-text notes */
    WAPI_CONTACT_FORCE32  = 0x7FFFFFFF
} wapi_contact_prop_t;

/* ============================================================
 * Value Labels
 * ============================================================
 * Applied to multi-value properties (email, tel, address, url).
 * Single-value properties ignore the label field (set to NONE).
 * Web runtime always returns NONE (labels not exposed by spec).
 */

typedef enum wapi_contact_label_t {
    WAPI_CONTACT_LABEL_NONE   = 0,
    WAPI_CONTACT_LABEL_HOME   = 1,
    WAPI_CONTACT_LABEL_WORK   = 2,
    WAPI_CONTACT_LABEL_MOBILE = 3,
    WAPI_CONTACT_LABEL_OTHER  = 0xFF,
    WAPI_CONTACT_LABEL_FORCE32 = 0x7FFFFFFF
} wapi_contact_label_t;

/* ============================================================
 * Contact Operations (async, submitted via wapi_io_t)
 * ============================================================ */

/**
 * Submit a contact-picker request.
 * Completion: result = number of contacts selected on success, or
 * WAPI_ERR_RANGE / WAPI_ERR_CANCELED.
 */
static inline wapi_result_t wapi_contacts_pick(
    const wapi_io_t* io,
    uint32_t properties_mask, wapi_bool_t multiple,
    void* results_buf, wapi_size_t results_buf_len,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CONTACTS_PICK;
    op.flags     = properties_mask;
    op.flags2    = multiple ? 1 : 0;
    op.addr      = (uint64_t)(uintptr_t)results_buf;
    op.len       = results_buf_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/**
 * Submit an icon read for a contact.
 */
static inline wapi_result_t wapi_contacts_icon_read(
    const wapi_io_t* io,
    uint32_t icon_handle,
    void* buf, wapi_size_t buf_len,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CONTACTS_ICON_READ;
    op.fd        = (int32_t)icon_handle;
    op.addr      = (uint64_t)(uintptr_t)buf;
    op.len       = buf_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CONTACTS_H */
