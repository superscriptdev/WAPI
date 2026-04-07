/**
 * WAPI - Contacts Capability
 * Version 1.0.0
 *
 * Access device contact information via a picker interface.
 *
 * Maps to: Contact Picker API (Web), CNContactPickerViewController (iOS),
 *          ContactsContract (Android)
 *
 * Import module: "wapi_contacts"
 *
 * Query availability with wapi_capability_supported("wapi.contacts", 12)
 */

#ifndef WAPI_CONTACTS_H
#define WAPI_CONTACTS_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Contact Types
 * ============================================================ */

typedef enum wapi_contact_prop_t {
    WAPI_CONTACT_NAME    = 0x01,
    WAPI_CONTACT_EMAIL   = 0x02,
    WAPI_CONTACT_TEL     = 0x04,
    WAPI_CONTACT_ADDRESS = 0x08,
    WAPI_CONTACT_ICON    = 0x10,
    WAPI_CONTACT_FORCE32 = 0x7FFFFFFF
} wapi_contact_prop_t;

/* ============================================================
 * Contact Functions
 * ============================================================ */

/**
 * Show a contact picker and retrieve selected contacts.
 *
 * @param properties_mask  Bitmask of wapi_contact_prop_t fields to request.
 * @param multiple         Non-zero to allow multiple contact selection.
 * @param results_buf      Buffer to receive serialized contact data.
 * @param results_buf_len  Size of the results buffer.
 * @return Number of contacts selected on success, or negative error code.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_contacts, pick)
wapi_result_t wapi_contacts_pick(uint32_t properties_mask, wapi_bool_t multiple,
                                 void* results_buf, wapi_size_t results_buf_len);

/**
 * Check if the contacts picker is available on this platform.
 *
 * @return Non-zero if available, zero otherwise.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_contacts, is_available)
wapi_bool_t wapi_contacts_is_available(void);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CONTACTS_H */
