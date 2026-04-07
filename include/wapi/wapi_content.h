/**
 * WAPI - Content Tree Declaration
 * Version 1.0.0
 *
 * A semantic content tree that the app builds in Wasm memory.
 * The host reads it for accessibility, keyboard navigation,
 * indexing/crawling, and screen reader support.
 *
 * The app owns the memory, writes nodes directly, and bumps
 * version numbers for change detection. The host never writes
 * to this buffer.
 *
 * Architecture:
 *   Layer 1 (this module): DECLARE content tree
 *     - a11y, keyboard nav, indexing, crawling
 *     - app renders itself via GPU
 *
 *   Layer 2 (future): RENDER the declared tree
 *     - host renders text, decodes images, plays media
 *     - optional: app can still render itself and skip this
 *
 * Import module: "wapi_content"
 */

#ifndef WAPI_CONTENT_H
#define WAPI_CONTENT_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Content Node Types
 * ============================================================ */

typedef enum wapi_content_node_type_t {
    WAPI_CONTENT_NODE_NONE       = 0,   /* Empty / unused slot */
    WAPI_CONTENT_NODE_ROOT       = 1,   /* Root container (one per tree) */
    WAPI_CONTENT_NODE_GROUP      = 2,   /* Generic grouping */
    WAPI_CONTENT_NODE_TEXT       = 3,   /* Text content */
    WAPI_CONTENT_NODE_IMAGE      = 4,   /* Image */
    WAPI_CONTENT_NODE_BUTTON     = 5,   /* Interactive button */
    WAPI_CONTENT_NODE_LINK       = 6,   /* Navigation link */
    WAPI_CONTENT_NODE_HEADING    = 7,   /* Heading (level 1-6 via detail) */
    WAPI_CONTENT_NODE_TEXT_INPUT = 8,   /* Text input field */
    WAPI_CONTENT_NODE_CHECKBOX   = 9,   /* Checkbox / toggle */
    WAPI_CONTENT_NODE_SLIDER     = 10,  /* Range / slider */
    WAPI_CONTENT_NODE_LIST       = 11,  /* List container */
    WAPI_CONTENT_NODE_LIST_ITEM  = 12,  /* List item */
    WAPI_CONTENT_NODE_NAVIGATION = 13,  /* Navigation landmark */
    WAPI_CONTENT_NODE_MAIN       = 14,  /* Main content landmark */
    WAPI_CONTENT_NODE_REGION     = 15,  /* Generic landmark region */
    WAPI_CONTENT_NODE_SEPARATOR  = 16,  /* Visual / logical separator */
    WAPI_CONTENT_NODE_FORCE32    = 0x7FFFFFFF
} wapi_content_node_type_t;

/* ============================================================
 * Node State Flags
 * ============================================================ */

#define WAPI_CONTENT_STATE_DISABLED  0x0001  /* Non-interactive */
#define WAPI_CONTENT_STATE_CHECKED   0x0002  /* Checkbox / toggle is on */
#define WAPI_CONTENT_STATE_EXPANDED  0x0004  /* Expandable section is open */
#define WAPI_CONTENT_STATE_SELECTED  0x0008  /* Item is selected */
#define WAPI_CONTENT_STATE_FOCUSED   0x0010  /* Has keyboard focus (host-managed) */
#define WAPI_CONTENT_STATE_HIDDEN    0x0020  /* Hidden from a11y tree */
#define WAPI_CONTENT_STATE_REQUIRED  0x0040  /* Input is required */
#define WAPI_CONTENT_STATE_INVALID   0x0080  /* Input validation failed */

/* ============================================================
 * Content Node (fixed-size)
 * ============================================================
 *
 * Layout (64 bytes, align 4):
 *   Offset  0: uint32_t type           wapi_content_node_type_t
 *   Offset  4: uint32_t id             App-assigned stable ID
 *   Offset  8: uint32_t version        Per-node change counter
 *   Offset 12: uint32_t first_child    Index (UINT32_MAX = none)
 *   Offset 16: uint32_t next_sibling   Index (UINT32_MAX = none)
 *   Offset 20: uint32_t state_flags    WAPI_CONTENT_STATE_* bitmask
 *   Offset 24: wapi_string_view_t label  Accessible name (UTF-8)
 *   Offset 32: ptr      value          Type-specific value
 *   Offset 36: uint32_t value_len
 *   Offset 40: float    bounds_x       Bounding rect X
 *   Offset 44: float    bounds_y       Bounding rect Y
 *   Offset 48: float    bounds_w       Bounding rect width
 *   Offset 52: float    bounds_h       Bounding rect height
 *   Offset 56: int32_t  tab_index      Keyboard nav order (-1 = skip)
 *   Offset 60: uint32_t detail         Type-specific (heading level, etc.)
 *
 * How value and detail are interpreted per node type:
 *   TEXT       : value = UTF-8 text content
 *   IMAGE      : value = URL or image data,  detail = source type (0=URL, 1=memory)
 *   LINK       : value = href URL (UTF-8)
 *   HEADING    : detail = level (1-6)
 *   TEXT_INPUT : value = current value,       detail = max_length
 *   SLIDER     : detail = packed step_u16 | max_u16
 *   LIST       : detail = item count
 */

typedef struct wapi_content_node_t {
    uint32_t    type;
    uint32_t    id;
    uint32_t    version;
    uint32_t    first_child;
    uint32_t    next_sibling;
    uint32_t    state_flags;
    wapi_string_view_t label;
    const void* value;
    wapi_size_t value_len;
    float       bounds_x;
    float       bounds_y;
    float       bounds_w;
    float       bounds_h;
    int32_t     tab_index;
    uint32_t    detail;
} wapi_content_node_t;

#ifdef __wasm__
_Static_assert(sizeof(wapi_content_node_t) == 64,
               "wapi_content_node_t must be 64 bytes on wasm32");
#endif

/* ============================================================
 * Content Tree Buffer
 * ============================================================
 *
 * Layout (8 bytes header, align 4):
 *   Offset 0: uint32_t version   Global version (structural changes)
 *   Offset 4: uint32_t count     Number of active nodes
 *
 * Followed by count x wapi_content_node_t (64 bytes each).
 * Node 0 is always the root.
 */

typedef struct wapi_content_tree_t {
    uint32_t version;
    uint32_t count;
    /* wapi_content_node_t nodes[] follow in memory */
} wapi_content_tree_t;

/* ============================================================
 * Content Functions
 * ============================================================ */

/**
 * Register the content tree buffer with the host.
 * Call once at startup. The app owns the memory and writes
 * to it freely. The host reads it when version changes.
 *
 * @param tree      Pointer to wapi_content_tree_t in Wasm memory.
 * @param capacity  Maximum number of nodes the buffer can hold.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_content, register_tree)
wapi_result_t wapi_content_register_tree(wapi_content_tree_t* tree,
                                         uint32_t capacity);

/**
 * Notify the host that the tree has been updated.
 * Optional -- the host may also poll the version number.
 * Useful for immediate a11y tree synchronization.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_content, notify)
wapi_result_t wapi_content_notify(void);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CONTENT_H */
