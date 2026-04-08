/**
 * WAPI Desktop Runtime - I/O Host Imports
 *
 * Implements the direct I/O host imports: submit, cancel, poll,
 * wait, flush. These are called by modules directly (not through
 * a vtable).
 *
 * Operations are submitted via wapi_io_submit(); completions and
 * all other events (input, lifecycle, device changes) are delivered
 * through wapi_io_poll()/wapi_io_wait().
 */

#include "wapi_host.h"

/* ============================================================
 * I/O Op Layout (read from wasm linear memory, 64 bytes each)
 *
 * Offset  0: u32 opcode
 * Offset  4: u32 flags
 * Offset  8: i32 fd
 * Offset 12: u32 pad
 * Offset 16: u64 offset
 * Offset 24: u32 addr      (wasm pointer to buffer)
 * Offset 28: u32 len
 * Offset 32: u32 addr2
 * Offset 36: u32 len2
 * Offset 40: u64 user_data
 * Offset 48: u32 result_ptr
 * Offset 52: u32 flags2
 * (total 56 used, padded to 64)
 * ============================================================ */

#define IO_OP_SIZE  64

/* I/O opcodes */
#define IO_OP_NOP     0
#define IO_OP_READ    1
#define IO_OP_WRITE   2
#define IO_OP_LOG     6
#define IO_OP_TIMEOUT 20

/* Log levels */
#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3

/* I/O completion event type (must match WAPI_EVENT_IO_COMPLETION) */
#define IO_EVENT_TYPE 0x2000

/* ============================================================
 * Helpers
 * ============================================================ */

/* Push an I/O completion as an event into the unified event queue.
 * Layout of wapi_io_event_t (32 bytes in the 128-byte event):
 *   Offset  0: u32 type       (IO_EVENT_TYPE)
 *   Offset  4: u32 surface_id (0)
 *   Offset  8: u64 timestamp
 *   Offset 16: i32 result
 *   Offset 20: u32 flags
 *   Offset 24: u64 user_data
 */
static void io_push_completion_event(uint64_t user_data,
                                     int32_t result,
                                     uint32_t flags) {
    wapi_host_event_t ev;
    memset(&ev, 0, sizeof(ev));

    uint32_t type = IO_EVENT_TYPE;
    uint32_t surface_id = 0;
    uint64_t timestamp = SDL_GetTicksNS();

    memcpy(ev.data + 0,  &type,       4);
    memcpy(ev.data + 4,  &surface_id, 4);
    memcpy(ev.data + 8,  &timestamp,  8);
    memcpy(ev.data + 16, &result,     4);
    memcpy(ev.data + 20, &flags,      4);
    memcpy(ev.data + 24, &user_data,  8);

    wapi_event_queue_push(&ev);
}

/* Check pending timeouts and push completion events for expired ones */
void wapi_io_check_timeouts(void) {
    uint64_t now = SDL_GetTicksNS();
    for (int q = 4; q < WAPI_MAX_HANDLES; q++) {
        if (g_rt.handles[q].type != WAPI_HTYPE_IO_QUEUE) continue;
        wapi_io_queue_t* ioq = g_rt.handles[q].data.io_queue;
        if (!ioq) continue;
        for (int i = 0; i < ioq->timeout_count; i++) {
            if (ioq->timeouts[i].active && now >= ioq->timeouts[i].deadline_ns) {
                io_push_completion_event(ioq->timeouts[i].user_data, WAPI_OK, 0);
                ioq->timeouts[i].active = false;
            }
        }
    }
}

/* Read an io_op_t struct from wasm memory at the given offset */
static bool io_read_op(uint32_t wasm_ptr,
                       uint32_t* opcode, uint32_t* flags, int32_t* fd,
                       uint64_t* offset, uint32_t* addr, uint32_t* len,
                       uint32_t* addr2, uint32_t* len2,
                       uint64_t* user_data, uint32_t* result_ptr,
                       uint32_t* flags2) {
    uint8_t* base = (uint8_t*)wapi_wasm_ptr(wasm_ptr, IO_OP_SIZE);
    if (!base) return false;

    memcpy(opcode,     base + 0,  4);
    memcpy(flags,      base + 4,  4);
    memcpy(fd,         base + 8,  4);
    /* skip pad at 12 */
    memcpy(offset,     base + 16, 8);
    memcpy(addr,       base + 24, 4);
    memcpy(len,        base + 28, 4);
    memcpy(addr2,      base + 32, 4);
    memcpy(len2,       base + 36, 4);
    memcpy(user_data,  base + 40, 8);
    memcpy(result_ptr, base + 48, 4);
    memcpy(flags2,     base + 52, 4);
    return true;
}

/* ============================================================
 * submit(ops_ptr: i32, count: i32) -> i32
 * ============================================================
 * Direct host import. Read io_op_t structs from wasm memory.
 * Execute synchronously where possible and push completion
 * events to the event queue.
 */
static wasm_trap_t* cb_submit(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t ops_ptr  = WAPI_ARG_U32(0);
    int32_t count     = WAPI_ARG_I32(1);

    if (count <= 0) {
        WAPI_RET_I32(0);
        return NULL;
    }

    int submitted = 0;

    for (int i = 0; i < count; i++) {
        uint32_t op_addr = ops_ptr + (uint32_t)(i * IO_OP_SIZE);

        uint32_t opcode, op_flags, addr, len, addr2, len2, result_ptr, flags2;
        int32_t fd;
        uint64_t offset, user_data;

        if (!io_read_op(op_addr, &opcode, &op_flags, &fd, &offset,
                        &addr, &len, &addr2, &len2,
                        &user_data, &result_ptr, &flags2)) {
            break; /* Bad pointer, stop processing */
        }

        switch (opcode) {
        case IO_OP_NOP:
            io_push_completion_event(user_data, WAPI_OK, 0);
            submitted++;
            break;

        case IO_OP_READ: {
            if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
                io_push_completion_event(user_data, WAPI_ERR_BADF, 0);
                submitted++;
                break;
            }

            FILE* f = g_rt.handles[fd].data.file;
            void* buf = wapi_wasm_ptr(addr, len);
            if (!buf) {
                io_push_completion_event(user_data, WAPI_ERR_INVAL, 0);
                submitted++;
                break;
            }

            if (offset != 0) {
                fseek(f, (long)offset, SEEK_SET);
            }

            size_t bytes_read = fread(buf, 1, len, f);
            io_push_completion_event(user_data, (int32_t)bytes_read, 0);
            submitted++;
            break;
        }

        case IO_OP_WRITE: {
            if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
                io_push_completion_event(user_data, WAPI_ERR_BADF, 0);
                submitted++;
                break;
            }

            FILE* f = g_rt.handles[fd].data.file;
            void* buf = wapi_wasm_ptr(addr, len);
            if (!buf) {
                io_push_completion_event(user_data, WAPI_ERR_INVAL, 0);
                submitted++;
                break;
            }

            if (offset != 0) {
                fseek(f, (long)offset, SEEK_SET);
            }

            size_t bytes_written = fwrite(buf, 1, len, f);
            fflush(f);
            io_push_completion_event(user_data, (int32_t)bytes_written, 0);
            submitted++;
            break;
        }

        case IO_OP_TIMEOUT: {
            /* Find or create an I/O queue for timeout tracking.
             * For simplicity, use the first IO queue or create one. */
            wapi_io_queue_t* q = NULL;
            int32_t qh = 0;
            for (int h = 4; h < WAPI_MAX_HANDLES; h++) {
                if (g_rt.handles[h].type == WAPI_HTYPE_IO_QUEUE) {
                    q = g_rt.handles[h].data.io_queue;
                    qh = h;
                    break;
                }
            }
            if (!q) {
                /* Auto-create a default IO queue for timeout tracking */
                qh = wapi_handle_alloc(WAPI_HTYPE_IO_QUEUE);
                if (qh != 0) {
                    q = (wapi_io_queue_t*)calloc(1, sizeof(wapi_io_queue_t));
                    if (q) {
                        q->capacity = WAPI_IO_QUEUE_MAX_OPS;
                        g_rt.handles[qh].data.io_queue = q;
                    }
                }
            }
            if (!q || q->timeout_count >= 64) {
                io_push_completion_event(user_data, WAPI_ERR_NOSPC, 0);
                submitted++;
                break;
            }

            uint64_t now = SDL_GetTicksNS();
            int slot = -1;
            for (int t = 0; t < q->timeout_count; t++) {
                if (!q->timeouts[t].active) {
                    slot = t;
                    break;
                }
            }
            if (slot < 0) {
                slot = q->timeout_count;
                q->timeout_count++;
            }

            q->timeouts[slot].user_data   = user_data;
            q->timeouts[slot].deadline_ns  = now + offset;
            q->timeouts[slot].active       = true;
            submitted++;
            break;
        }

        case IO_OP_LOG: {
            const char* prefix = "INFO";
            switch (op_flags) {
            case LOG_DEBUG: prefix = "DEBUG"; break;
            case LOG_INFO:  prefix = "INFO";  break;
            case LOG_WARN:  prefix = "WARN";  break;
            case LOG_ERROR: prefix = "ERROR"; break;
            }

            const char* msg = (const char*)wapi_wasm_ptr(addr, len);
            const char* tag = (addr2 && len2) ? (const char*)wapi_wasm_ptr(addr2, len2) : NULL;

            if (msg) {
                if (tag) {
                    fprintf(stderr, "[%s][%.*s] %.*s\n", prefix, (int)len2, tag, (int)len, msg);
                } else {
                    fprintf(stderr, "[%s] %.*s\n", prefix, (int)len, msg);
                }
            }
            /* Fire-and-forget: no completion event */
            submitted++;
            break;
        }

        default:
            io_push_completion_event(user_data, WAPI_ERR_NOTSUP, 0);
            submitted++;
            break;
        }
    }

    WAPI_RET_I32(submitted);
    return NULL;
}

/* ============================================================
 * cancel(user_data: i64) -> i32
 * ============================================================ */
static wasm_trap_t* cb_cancel(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint64_t user_data = WAPI_ARG_U64(0);

    bool found = false;
    for (int q = 4; q < WAPI_MAX_HANDLES; q++) {
        if (g_rt.handles[q].type != WAPI_HTYPE_IO_QUEUE) continue;
        wapi_io_queue_t* ioq = g_rt.handles[q].data.io_queue;
        if (!ioq) continue;
        for (int i = 0; i < ioq->timeout_count; i++) {
            if (ioq->timeouts[i].active && ioq->timeouts[i].user_data == user_data) {
                ioq->timeouts[i].active = false;
                io_push_completion_event(user_data, WAPI_ERR_CANCELED, 0);
                found = true;
                break;
            }
        }
        if (found) break;
    }

    WAPI_RET_I32(found ? WAPI_OK : WAPI_ERR_NOENT);
    return NULL;
}

/* ============================================================
 * poll(event_ptr: i32) -> i32
 * ============================================================
 * Non-blocking poll for the next event. Returns 1 if an event
 * was written to event_ptr, 0 if no events pending.
 */
static wasm_trap_t* cb_poll(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t event_ptr = WAPI_ARG_U32(0);

    /* Check for expired I/O timeouts first */
    wapi_io_check_timeouts();

    wapi_host_event_t ev;
    if (wapi_event_queue_pop(&ev)) {
        wapi_wasm_write_bytes(event_ptr, ev.data, 128);
        WAPI_RET_I32(1);
    } else {
        uint8_t zeros[128];
        memset(zeros, 0, sizeof(zeros));
        wapi_wasm_write_bytes(event_ptr, zeros, 128);
        WAPI_RET_I32(0);
    }
    return NULL;
}

/* ============================================================
 * wait(event_ptr: i32, timeout_ms: i32) -> i32
 * ============================================================
 * Blocking wait for an event. Returns 1 if an event was written,
 * 0 on timeout. timeout_ms == -1 means wait forever.
 */
static wasm_trap_t* cb_wait(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t event_ptr = WAPI_ARG_U32(0);
    int32_t timeout_ms = WAPI_ARG_I32(1);

    /* Check I/O timeouts first */
    wapi_io_check_timeouts();

    /* Check existing queue (may have I/O completions) */
    wapi_host_event_t ev;
    if (wapi_event_queue_pop(&ev)) {
        wapi_wasm_write_bytes(event_ptr, ev.data, 128);
        WAPI_RET_I32(1);
        return NULL;
    }

    /* Wait for an SDL event (also wakes on I/O completion injection) */
    SDL_Event sdl_ev;
    bool got_event;
    if (timeout_ms < 0) {
        got_event = SDL_WaitEvent(&sdl_ev);
    } else {
        got_event = SDL_WaitEventTimeout(&sdl_ev, timeout_ms);
    }

    if (got_event) {
        wapi_input_process_sdl_event(&sdl_ev);
    }

    /* Check I/O timeouts again after waiting */
    wapi_io_check_timeouts();

    /* Try to pop from our queue */
    if (wapi_event_queue_pop(&ev)) {
        wapi_wasm_write_bytes(event_ptr, ev.data, 128);
        WAPI_RET_I32(1);
        return NULL;
    }

    /* Timeout or no event */
    uint8_t zeros[128];
    memset(zeros, 0, sizeof(zeros));
    wapi_wasm_write_bytes(event_ptr, zeros, 128);
    WAPI_RET_I32(0);
    return NULL;
}

/* ============================================================
 * flush(event_type: i32) -> void
 * ============================================================
 * Discard all pending events of a given type (0 = all).
 */
static wasm_trap_t* cb_flush(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t event_type = WAPI_ARG_U32(0);
    wapi_event_queue_flush(event_type);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================
 * Register all 5 direct I/O host imports under "wapi_io".
 */

/* Forward declaration from wapi_host_input.c */
extern void wapi_input_process_sdl_event(const SDL_Event* sdl_ev);

void wapi_host_register_io(wasmtime_linker_t* linker) {
    /* submit: (i32 ops_ptr, i32 count) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_io", "submit", cb_submit);

    /* cancel: (i64 user_data) -> i32 */
    wapi_linker_define(linker, "wapi_io", "cancel", cb_cancel,
        1, (wasm_valkind_t[]){WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});

    /* poll: (i32 event_ptr) -> i32 */
    WAPI_DEFINE_1_1(linker, "wapi_io", "poll", cb_poll);

    /* wait: (i32 event_ptr, i32 timeout_ms) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_io", "wait", cb_wait);

    /* flush: (i32 event_type) -> void */
    WAPI_DEFINE_1_0(linker, "wapi_io", "flush", cb_flush);
}
