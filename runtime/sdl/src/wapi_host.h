/**
 * WAPI SDL Runtime - Shared Host Types
 *
 * Global handle table, Wasm memory helpers, and shared state used by
 * all wapi_host_*.c implementation files. Built on:
 *   - SDL3 (no SDL_GPU) for windowing / input / audio
 *   - Dawn (webgpu.h) for GPU
 *   - WAMR for WebAssembly execution
 *
 * Capabilities SDL3 cannot back return WAPI_ERR_NOTCAPABLE.
 */

#ifndef WAPI_HOST_H
#define WAPI_HOST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* WAMR */
#include <wasm_export.h>

/* SDL3 */
#include <SDL3/SDL.h>

/* Dawn / WebGPU */
#include <webgpu/webgpu.h>

/* WAPI ABI */
#include <wapi/wapi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Handle Table
 * ============================================================
 * Maps wapi_handle_t (int32_t > 0) to host-side objects.
 * Handle 0 is invalid. Handles 1-3 are stdin/stdout/stderr.
 * Handle 4+ are pre-opened directories, then dynamically allocated.
 */

#define WAPI_MAX_HANDLES 4096

typedef enum wapi_handle_type_t {
    WAPI_HTYPE_FREE = 0,
    WAPI_HTYPE_FILE,
    WAPI_HTYPE_DIRECTORY,
    WAPI_HTYPE_SURFACE,
    WAPI_HTYPE_GPU_DEVICE,
    WAPI_HTYPE_GPU_QUEUE,
    WAPI_HTYPE_GPU_TEXTURE,
    WAPI_HTYPE_GPU_TEXTURE_VIEW,
    WAPI_HTYPE_AUDIO_DEVICE,
    WAPI_HTYPE_AUDIO_STREAM,
    WAPI_HTYPE_PREOPEN_DIR,
    WAPI_HTYPE_IO_QUEUE,
    WAPI_HTYPE_CONTENT,
    WAPI_HTYPE_CRYPTO_HASH_CTX,
    WAPI_HTYPE_CRYPTO_KEY,
    WAPI_HTYPE_MODULE,
    WAPI_HTYPE_LEASE,
    WAPI_HTYPE_HAPTIC,
    WAPI_HTYPE_SENSOR,
} wapi_handle_type_t;

/* ---- I/O Queue ---- */
#define WAPI_IO_QUEUE_MAX_OPS 256

typedef struct wapi_io_completion_entry_t {
    uint64_t user_data;
    int32_t  result;
    uint32_t flags;
} wapi_io_completion_entry_t;

typedef struct wapi_io_queue_t {
    wapi_io_completion_entry_t completions[WAPI_IO_QUEUE_MAX_OPS];
    int head;
    int tail;
    int count;
    int capacity;
    struct {
        uint64_t user_data;
        uint64_t deadline_ns;
        bool     active;
    } timeouts[64];
    int timeout_count;
} wapi_io_queue_t;

/* ---- Crypto ---- */
typedef struct wapi_crypto_hash_ctx_t {
    uint32_t algo;
    uint8_t  state[256];
    size_t   state_size;
} wapi_crypto_hash_ctx_t;

typedef struct wapi_crypto_key_t {
    uint8_t  data[512];
    size_t   len;
    uint32_t usages;
} wapi_crypto_key_t;

/* ---- Memory allocator state ---- */
typedef struct wapi_mem_alloc_t {
    uint32_t offset;
    uint32_t size;
} wapi_mem_alloc_t;

#define WAPI_MEM_MAX_ALLOCS 4096

typedef struct wapi_handle_entry_t {
    wapi_handle_type_t type;
    union {
        FILE*               file;
        struct {
            char             path[512];
        } dir;
        SDL_Window*         window;
        struct {
            WGPUDevice       device;
            WGPUAdapter      adapter;
            WGPUInstance     instance;
        } gpu_device;
        WGPUQueue           gpu_queue;
        WGPUTexture         gpu_texture;
        WGPUTextureView     gpu_texture_view;
        SDL_AudioDeviceID   audio_device_id;
        SDL_AudioStream*    audio_stream;
        wapi_io_queue_t*    io_queue;
        wapi_crypto_hash_ctx_t crypto_hash;
        wapi_crypto_key_t   crypto_key;
        SDL_Haptic*         haptic;
        SDL_Sensor*         sensor;
    } data;
} wapi_handle_entry_t;

/* ============================================================
 * Event Queue
 * ============================================================ */

#define WAPI_EVENT_QUEUE_SIZE 256

typedef struct wapi_host_event_t {
    uint8_t data[128];
} wapi_host_event_t;

typedef struct wapi_event_queue_t {
    wapi_host_event_t events[WAPI_EVENT_QUEUE_SIZE];
    int             head;
    int             tail;
    int             count;
} wapi_event_queue_t;

/* ============================================================
 * GPU Surface State
 * ============================================================ */

#define WAPI_MAX_GPU_SURFACES 16

typedef struct wapi_gpu_surface_state_t {
    int32_t         wapi_surface_handle;
    WGPUSurface     wgpu_surface;
    WGPUDevice      wgpu_device;
    uint32_t        format;
    uint32_t        present_mode;
    uint32_t        usage;
    uint32_t        width;
    uint32_t        height;
    bool            configured;
} wapi_gpu_surface_state_t;

/* ============================================================
 * Pre-opened Directory
 * ============================================================ */

#define WAPI_MAX_PREOPENS 32

typedef struct wapi_preopen_t {
    char        guest_path[256];
    char        host_path[512];
    int32_t     handle;
} wapi_preopen_t;

/* ============================================================
 * Runtime State (global singleton)
 * ============================================================ */

typedef struct wapi_runtime_t {
    /* WAMR */
    wasm_module_t           module;
    wasm_module_inst_t      module_inst;
    wasm_exec_env_t         exec_env;
    uint8_t*                wasm_bytes;
    size_t                  wasm_size;

    /* Handle table */
    wapi_handle_entry_t     handles[WAPI_MAX_HANDLES];
    int32_t                 next_handle;

    /* Event queue */
    wapi_event_queue_t      event_queue;

    /* GPU surface tracking */
    wapi_gpu_surface_state_t gpu_surfaces[WAPI_MAX_GPU_SURFACES];
    int                     gpu_surface_count;

    /* Shared WebGPU instance (lazy-created) */
    WGPUInstance            wgpu_instance;

    /* Pre-opened directories */
    wapi_preopen_t          preopens[WAPI_MAX_PREOPENS];
    int                     preopen_count;

    /* Application args passed to the Wasm module */
    int                     app_argc;
    char**                  app_argv;

    /* Keyboard state (256 scancodes) */
    bool                    key_state[256];
    uint16_t                mod_state;

    /* Mouse state */
    float                   mouse_x;
    float                   mouse_y;
    uint32_t                mouse_buttons;

    /* Pointer state */
    float                   pointer_x;
    float                   pointer_y;
    uint32_t                pointer_buttons;

    /* Host memory allocator (bump in wasm linear memory) */
    wapi_mem_alloc_t        mem_allocs[WAPI_MEM_MAX_ALLOCS];
    int                     mem_alloc_count;
    uint32_t                mem_heap_top;
    bool                    mem_initialized;

    /* Last error message */
    char                    last_error[512];

    /* Running state */
    bool                    running;
    bool                    has_wapi_frame;
} wapi_runtime_t;

extern wapi_runtime_t g_rt;

/* ============================================================
 * Handle Table Operations
 * ============================================================ */

static inline int32_t wapi_handle_alloc(wapi_handle_type_t type) {
    for (int i = g_rt.next_handle; i < WAPI_MAX_HANDLES; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_FREE) {
            g_rt.handles[i].type = type;
            memset(&g_rt.handles[i].data, 0, sizeof(g_rt.handles[i].data));
            g_rt.next_handle = i + 1;
            return (int32_t)i;
        }
    }
    for (int i = 4; i < g_rt.next_handle; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_FREE) {
            g_rt.handles[i].type = type;
            memset(&g_rt.handles[i].data, 0, sizeof(g_rt.handles[i].data));
            g_rt.next_handle = i + 1;
            return (int32_t)i;
        }
    }
    return 0;
}

static inline bool wapi_handle_valid(int32_t h, wapi_handle_type_t expected) {
    if (h <= 0 || h >= WAPI_MAX_HANDLES) return false;
    if (g_rt.handles[h].type == WAPI_HTYPE_FREE) return false;
    if (expected != WAPI_HTYPE_FREE && g_rt.handles[h].type != expected) return false;
    return true;
}

static inline bool wapi_handle_valid_any(int32_t h) {
    if (h <= 0 || h >= WAPI_MAX_HANDLES) return false;
    return g_rt.handles[h].type != WAPI_HTYPE_FREE;
}

static inline void wapi_handle_free(int32_t h) {
    if (h > 0 && h < WAPI_MAX_HANDLES) {
        g_rt.handles[h].type = WAPI_HTYPE_FREE;
        memset(&g_rt.handles[h].data, 0, sizeof(g_rt.handles[h].data));
    }
}

static inline wapi_handle_entry_t* wapi_handle_get(int32_t h) {
    if (h > 0 && h < WAPI_MAX_HANDLES && g_rt.handles[h].type != WAPI_HTYPE_FREE) {
        return &g_rt.handles[h];
    }
    return NULL;
}

/* ============================================================
 * Wasm Memory Helpers (WAMR)
 * ============================================================
 * Guest pointers are uint32_t offsets into module-inst linear memory.
 * WAMR provides address translation and validation APIs; we wrap them.
 */

static inline bool wapi_wasm_validate_ptr(uint32_t ptr, uint32_t len) {
    if (len == 0) return true;
    return wasm_runtime_validate_app_addr(g_rt.module_inst,
                                          (uint64_t)ptr, (uint64_t)len);
}

static inline void* wapi_wasm_ptr(uint32_t guest_ptr, uint32_t len) {
    if (!wapi_wasm_validate_ptr(guest_ptr, len)) return NULL;
    return wasm_runtime_addr_app_to_native(g_rt.module_inst, (uint64_t)guest_ptr);
}

static inline bool wapi_wasm_read_i32(uint32_t ptr, int32_t* out) {
    void* host = wapi_wasm_ptr(ptr, 4);
    if (!host) return false;
    memcpy(out, host, 4);
    return true;
}

static inline bool wapi_wasm_write_i32(uint32_t ptr, int32_t val) {
    void* host = wapi_wasm_ptr(ptr, 4);
    if (!host) return false;
    memcpy(host, &val, 4);
    return true;
}

static inline bool wapi_wasm_write_u32(uint32_t ptr, uint32_t val) {
    void* host = wapi_wasm_ptr(ptr, 4);
    if (!host) return false;
    memcpy(host, &val, 4);
    return true;
}

static inline bool wapi_wasm_write_u64(uint32_t ptr, uint64_t val) {
    void* host = wapi_wasm_ptr(ptr, 8);
    if (!host) return false;
    memcpy(host, &val, 8);
    return true;
}

static inline bool wapi_wasm_write_f32(uint32_t ptr, float val) {
    void* host = wapi_wasm_ptr(ptr, 4);
    if (!host) return false;
    memcpy(host, &val, 4);
    return true;
}

static inline const char* wapi_wasm_read_string(uint32_t ptr, uint32_t len) {
    if (len == 0) return "";
    void* host = wapi_wasm_ptr(ptr, len);
    return (const char*)host;
}

static inline bool wapi_wasm_write_bytes(uint32_t ptr, const void* src, uint32_t len) {
    if (len == 0) return true;
    void* host = wapi_wasm_ptr(ptr, len);
    if (!host) return false;
    memcpy(host, src, len);
    return true;
}

/* ============================================================
 * Event Queue Operations
 * ============================================================ */

static inline bool wapi_event_queue_push(const wapi_host_event_t* ev) {
    wapi_event_queue_t* q = &g_rt.event_queue;
    if (q->count >= WAPI_EVENT_QUEUE_SIZE) return false;
    q->events[q->tail] = *ev;
    q->tail = (q->tail + 1) % WAPI_EVENT_QUEUE_SIZE;
    q->count++;
    return true;
}

static inline bool wapi_event_queue_pop(wapi_host_event_t* ev) {
    wapi_event_queue_t* q = &g_rt.event_queue;
    if (q->count == 0) return false;
    *ev = q->events[q->head];
    q->head = (q->head + 1) % WAPI_EVENT_QUEUE_SIZE;
    q->count--;
    return true;
}

static inline void wapi_event_queue_flush(uint32_t event_type) {
    wapi_event_queue_t* q = &g_rt.event_queue;
    if (event_type == 0) {
        q->head = q->tail = q->count = 0;
        return;
    }
    wapi_host_event_t temp[WAPI_EVENT_QUEUE_SIZE];
    int new_count = 0;
    while (q->count > 0) {
        wapi_host_event_t ev;
        wapi_event_queue_pop(&ev);
        uint32_t type;
        memcpy(&type, ev.data, 4);
        if (type != event_type) {
            temp[new_count++] = ev;
        }
    }
    for (int i = 0; i < new_count; i++) {
        wapi_event_queue_push(&temp[i]);
    }
}

/* ============================================================
 * GPU Surface Lookup
 * ============================================================ */

static inline wapi_gpu_surface_state_t* wapi_gpu_surface_find(int32_t surface_handle) {
    for (int i = 0; i < g_rt.gpu_surface_count; i++) {
        if (g_rt.gpu_surfaces[i].wapi_surface_handle == surface_handle) {
            return &g_rt.gpu_surfaces[i];
        }
    }
    return NULL;
}

/* ============================================================
 * Error Helpers
 * ============================================================ */

static inline void wapi_set_error(const char* msg) {
    if (msg) {
        size_t len = strlen(msg);
        if (len >= sizeof(g_rt.last_error)) len = sizeof(g_rt.last_error) - 1;
        memcpy(g_rt.last_error, msg, len);
        g_rt.last_error[len] = '\0';
    } else {
        g_rt.last_error[0] = '\0';
    }
}

/* ============================================================
 * Registration — collect NativeSymbol[] arrays per capability
 * ============================================================
 * Each wapi_host_<cap>.c defines a static NativeSymbol array and a
 * wapi_host_<cap>_symbols() accessor that returns it plus the count.
 * main.c gathers every module's symbols and calls
 *   wasm_runtime_register_natives("<module>", symbols, count)
 * once per module.
 */

typedef struct wapi_cap_registration_t {
    const char*          module_name;
    NativeSymbol*        symbols;
    uint32_t             count;
} wapi_cap_registration_t;

/* Each host file exposes this accessor. */
#define WAPI_DECL_REGISTRATION(name) \
    wapi_cap_registration_t wapi_host_##name##_registration(void)

WAPI_DECL_REGISTRATION(capability);
WAPI_DECL_REGISTRATION(env);
WAPI_DECL_REGISTRATION(memory);
WAPI_DECL_REGISTRATION(io);
WAPI_DECL_REGISTRATION(clock);
WAPI_DECL_REGISTRATION(fs);
WAPI_DECL_REGISTRATION(surface);
WAPI_DECL_REGISTRATION(window);
WAPI_DECL_REGISTRATION(display);
WAPI_DECL_REGISTRATION(input);
WAPI_DECL_REGISTRATION(audio);
WAPI_DECL_REGISTRATION(haptics);
WAPI_DECL_REGISTRATION(power);
WAPI_DECL_REGISTRATION(sensors);
WAPI_DECL_REGISTRATION(transfer);
WAPI_DECL_REGISTRATION(seat);
WAPI_DECL_REGISTRATION(input_seat);  /* registers wapi_input.device_seat */
WAPI_DECL_REGISTRATION(gpu);

/* Unsupported capabilities (compression, crypto, encoding, module, net,
 * http, content, text, font, video, kv, notifications, geolocation,
 * speech, biometric, share, payments, usb, midi, bluetooth, camera, xr,
 * register, taskbar, permissions, orientation, codec, media_session,
 * authn, network_info, hid, serial, screen_capture, contacts, barcode,
 * nfc) are deliberately NOT registered. Apps must gate their use behind
 * wapi.cap_supported. */

/* SDL event dispatch lives in wapi_host_input.c */
void wapi_input_process_sdl_event(const SDL_Event* sdl_ev);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_HOST_H */
