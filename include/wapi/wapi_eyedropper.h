/**
 * WAPI - Eyedropper
 * Version 1.0.0
 *
 * Screen color picker.
 *
 * Maps to: EyeDropper API (Web), NSColorSampler (macOS),
 *          custom impl (Windows/Linux)
 *
 * Import module: "wapi_eyedrop"
 */

#ifndef WAPI_EYEDROPPER_H
#define WAPI_EYEDROPPER_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Eyedropper Operations (async, submitted via wapi_io_t)
 * ============================================================
 * Completion: result=WAPI_OK / WAPI_ERR_CANCELED. On success the RGBA
 * u32 arrives inline in the event's payload[0..3] and
 * WAPI_IO_CQE_F_INLINE is set in flags.
 */

/**
 * Submit an eyedropper pick. Shows the system color picker and
 * completes when the user picks a color or cancels.
 */
static inline wapi_result_t wapi_eyedropper_pick(
    const wapi_io_t* io, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_EYEDROPPER_PICK;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_EYEDROPPER_H */
