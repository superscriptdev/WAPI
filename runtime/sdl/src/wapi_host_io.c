/**
 * WAPI SDL Runtime - I/O submit/poll/wait/flush/cancel
 *
 * Operations are submitted via wapi_io.submit (io_op_t structs read
 * from wasm memory). Completions + SDL input/lifecycle events are
 * delivered through poll/wait from the unified event queue.
 */

#include "wapi_host.h"

/* io_op_t layout (64 bytes):
 *  0 u32 opcode  4 u32 flags   8 i32 fd    12 u32 _pad
 * 16 u64 offset 24 u32 addr   28 u32 len   32 u32 addr2   36 u32 len2
 * 40 u64 user_data 48 u32 result_ptr 52 u32 flags2
 */
#define IO_OP_SIZE 64

#define IO_OP_NOP             0
#define IO_OP_READ            1
#define IO_OP_WRITE           2
#define IO_OP_LOG             6
#define IO_OP_TIMEOUT        20
#define IO_OP_TRANSFER_OFFER 0x310
#define IO_OP_TRANSFER_READ  0x311

/* From wapi_host_transfer.c */
int32_t wapi_host_transfer_io_offer(int32_t seat, uint32_t mode,
                                    uint32_t offer_ptr, uint32_t offer_len,
                                    uint32_t* out_result);
int32_t wapi_host_transfer_io_read(int32_t seat, uint32_t mode,
                                   uint32_t mime_ptr, uint32_t mime_len,
                                   uint32_t buf_ptr, uint32_t buf_len,
                                   uint32_t* out_bytes_written);

#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3

#define IO_EVENT_TYPE 0x2000

static void io_push_completion_event(uint64_t user_data,
                                     int32_t result, uint32_t flags) {
    wapi_host_event_t ev;
    memset(&ev, 0, sizeof(ev));
    uint32_t type = IO_EVENT_TYPE;
    uint32_t surface_id = 0;
    uint64_t ts = SDL_GetTicksNS();
    memcpy(ev.data + 0,  &type, 4);
    memcpy(ev.data + 4,  &surface_id, 4);
    memcpy(ev.data + 8,  &ts, 8);
    memcpy(ev.data + 16, &result, 4);
    memcpy(ev.data + 20, &flags, 4);
    memcpy(ev.data + 24, &user_data, 8);
    wapi_event_queue_push(&ev);
}

static void io_check_timeouts(void) {
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

static wapi_io_queue_t* io_find_or_create_queue(void) {
    for (int h = 4; h < WAPI_MAX_HANDLES; h++) {
        if (g_rt.handles[h].type == WAPI_HTYPE_IO_QUEUE) {
            return g_rt.handles[h].data.io_queue;
        }
    }
    int32_t qh = wapi_handle_alloc(WAPI_HTYPE_IO_QUEUE);
    if (qh == 0) return NULL;
    wapi_io_queue_t* q = (wapi_io_queue_t*)calloc(1, sizeof(wapi_io_queue_t));
    if (!q) { wapi_handle_free(qh); return NULL; }
    q->capacity = WAPI_IO_QUEUE_MAX_OPS;
    g_rt.handles[qh].data.io_queue = q;
    return q;
}

static int32_t host_submit(wasm_exec_env_t env,
                           uint32_t ops_ptr, int32_t count) {
    (void)env;
    if (count <= 0) return 0;
    int submitted = 0;
    for (int i = 0; i < count; i++) {
        uint32_t op_addr = ops_ptr + (uint32_t)(i * IO_OP_SIZE);
        uint32_t opcode, op_flags, addr, len, addr2, len2, result_ptr, flags2;
        int32_t fd;
        uint64_t offset, user_data;
        if (!io_read_op(op_addr, &opcode, &op_flags, &fd, &offset,
                        &addr, &len, &addr2, &len2,
                        &user_data, &result_ptr, &flags2)) break;

        switch (opcode) {
        case IO_OP_NOP:
            io_push_completion_event(user_data, WAPI_OK, 0);
            submitted++;
            break;

        case IO_OP_READ: {
            if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
                io_push_completion_event(user_data, WAPI_ERR_BADF, 0);
                submitted++; break;
            }
            FILE* f = g_rt.handles[fd].data.file;
            void* buf = wapi_wasm_ptr(addr, len);
            if (!buf) {
                io_push_completion_event(user_data, WAPI_ERR_INVAL, 0);
                submitted++; break;
            }
            if (offset != 0) fseek(f, (long)offset, SEEK_SET);
            size_t n = fread(buf, 1, len, f);
            io_push_completion_event(user_data, (int32_t)n, 0);
            submitted++;
            break;
        }

        case IO_OP_WRITE: {
            if (!wapi_handle_valid(fd, WAPI_HTYPE_FILE)) {
                io_push_completion_event(user_data, WAPI_ERR_BADF, 0);
                submitted++; break;
            }
            FILE* f = g_rt.handles[fd].data.file;
            void* buf = wapi_wasm_ptr(addr, len);
            if (!buf) {
                io_push_completion_event(user_data, WAPI_ERR_INVAL, 0);
                submitted++; break;
            }
            if (offset != 0) fseek(f, (long)offset, SEEK_SET);
            size_t n = fwrite(buf, 1, len, f);
            fflush(f);
            io_push_completion_event(user_data, (int32_t)n, 0);
            submitted++;
            break;
        }

        case IO_OP_TIMEOUT: {
            wapi_io_queue_t* q = io_find_or_create_queue();
            if (!q || q->timeout_count >= 64) {
                io_push_completion_event(user_data, WAPI_ERR_NOSPC, 0);
                submitted++; break;
            }
            uint64_t now = SDL_GetTicksNS();
            int slot = -1;
            for (int t = 0; t < q->timeout_count; t++) {
                if (!q->timeouts[t].active) { slot = t; break; }
            }
            if (slot < 0) slot = q->timeout_count++;
            q->timeouts[slot].user_data   = user_data;
            q->timeouts[slot].deadline_ns = now + offset;
            q->timeouts[slot].active      = true;
            submitted++;
            break;
        }

        case IO_OP_TRANSFER_OFFER: {
            uint32_t out_result = 0;
            int32_t r = wapi_host_transfer_io_offer(fd, op_flags,
                                                    addr, len, &out_result);
            io_push_completion_event(user_data,
                                     r == WAPI_OK ? (int32_t)out_result : r,
                                     0);
            submitted++;
            break;
        }

        case IO_OP_TRANSFER_READ: {
            uint32_t bytes = 0;
            int32_t r = wapi_host_transfer_io_read(fd, op_flags,
                                                   addr, len, addr2, len2,
                                                   &bytes);
            io_push_completion_event(user_data,
                                     r == WAPI_OK ? (int32_t)bytes : r,
                                     0);
            submitted++;
            break;
        }

        case IO_OP_LOG: {
            const char* prefix;
            switch (op_flags) {
                case LOG_DEBUG: prefix = "DEBUG"; break;
                case LOG_WARN:  prefix = "WARN";  break;
                case LOG_ERROR: prefix = "ERROR"; break;
                default:        prefix = "INFO";
            }
            const char* msg = (const char*)wapi_wasm_ptr(addr, len);
            const char* tag = (addr2 && len2) ? (const char*)wapi_wasm_ptr(addr2, len2) : NULL;
            if (msg) {
                if (tag) fprintf(stderr, "[%s][%.*s] %.*s\n",
                                 prefix, (int)len2, tag, (int)len, msg);
                else     fprintf(stderr, "[%s] %.*s\n",
                                 prefix, (int)len, msg);
            }
            submitted++;
            break;
        }

        default:
            io_push_completion_event(user_data, WAPI_ERR_NOTSUP, 0);
            submitted++;
            break;
        }
    }
    return submitted;
}

static int32_t host_cancel(wasm_exec_env_t env, int64_t user_data_s) {
    (void)env;
    uint64_t user_data = (uint64_t)user_data_s;
    for (int q = 4; q < WAPI_MAX_HANDLES; q++) {
        if (g_rt.handles[q].type != WAPI_HTYPE_IO_QUEUE) continue;
        wapi_io_queue_t* ioq = g_rt.handles[q].data.io_queue;
        if (!ioq) continue;
        for (int i = 0; i < ioq->timeout_count; i++) {
            if (ioq->timeouts[i].active && ioq->timeouts[i].user_data == user_data) {
                ioq->timeouts[i].active = false;
                io_push_completion_event(user_data, WAPI_ERR_CANCELED, 0);
                return WAPI_OK;
            }
        }
    }
    return WAPI_ERR_NOENT;
}

static int32_t host_poll(wasm_exec_env_t env, uint32_t event_ptr) {
    (void)env;
    io_check_timeouts();
    wapi_host_event_t ev;
    if (wapi_event_queue_pop(&ev)) {
        wapi_wasm_write_bytes(event_ptr, ev.data, 128);
        return 1;
    }
    uint8_t zeros[128] = {0};
    wapi_wasm_write_bytes(event_ptr, zeros, 128);
    return 0;
}

static int32_t host_wait(wasm_exec_env_t env,
                         uint32_t event_ptr, int32_t timeout_ms) {
    (void)env;
    io_check_timeouts();
    wapi_host_event_t ev;
    if (wapi_event_queue_pop(&ev)) {
        wapi_wasm_write_bytes(event_ptr, ev.data, 128);
        return 1;
    }

    SDL_Event sdl_ev;
    bool got = timeout_ms < 0
               ? SDL_WaitEvent(&sdl_ev)
               : SDL_WaitEventTimeout(&sdl_ev, timeout_ms);
    if (got) wapi_input_process_sdl_event(&sdl_ev);

    io_check_timeouts();
    if (wapi_event_queue_pop(&ev)) {
        wapi_wasm_write_bytes(event_ptr, ev.data, 128);
        return 1;
    }
    uint8_t zeros[128] = {0};
    wapi_wasm_write_bytes(event_ptr, zeros, 128);
    return 0;
}

static void host_flush(wasm_exec_env_t env, uint32_t event_type) {
    (void)env;
    wapi_event_queue_flush(event_type);
}

static NativeSymbol g_symbols[] = {
    { "submit", (void*)host_submit, "(ii)i", NULL },
    { "cancel", (void*)host_cancel, "(I)i",  NULL },
    { "poll",   (void*)host_poll,   "(i)i",  NULL },
    { "wait",   (void*)host_wait,   "(ii)i", NULL },
    { "flush",  (void*)host_flush,  "(i)",   NULL },
};

wapi_cap_registration_t wapi_host_io_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_io",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
