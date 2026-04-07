/**
 * WAPI Desktop Runtime - Event Queue
 *
 * The event queue data structures and operations live in wapi_host.h
 * (inline functions). Event delivery (poll/wait/flush) is handled by
 * wapi_host_io.c as part of the unified wapi_io_t vtable.
 *
 * This file is kept for any future event-queue-specific helpers that
 * don't belong in the I/O vtable implementation.
 */

#include "wapi_host.h"
