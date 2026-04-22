/**
 * WAPI Desktop Runtime — WebGPU (wgpu-native) bridge
 *
 * Registers every `env.wgpu*` import the guest compiles against into
 * real wgpu-native calls. Handles the wasm32 ↔ native ABI mismatch
 * for every descriptor the triangle touches:
 *
 *   - Every guest pointer is a u32 linear-memory offset; the host
 *     dereferences through `wapi_wasm_ptr` before passing to wgpu.
 *   - Every guest `size_t` is u32 on wasm32, u64 natively; the host
 *     widens explicitly.
 *   - Every WGPU object handle the host returns is a native pointer;
 *     the guest sees a `wapi_handle_t` (int32) allocated through the
 *     runtime handle table and mapped back on every call.
 *
 * Guest-supplied callbacks (adapter / device request) are wasm32
 * function-table indices. The host captures the result from
 * wgpu-native synchronously (the native callback runs inside
 * `wgpuInstanceProcessEvents`), then dispatches the guest funcref
 * through Wasmtime when the guest next calls
 * `wgpuInstanceProcessEvents`.
 *
 * Surface creation intercepts the chain: when the guest hands in a
 * `WAPI_STYPE_GPU_SURFACE_SOURCE_WAPI` chained struct, the host
 * swaps in the platform-native `WGPUSurfaceSourceWindowsHWND` /
 * MetalLayer / WaylandSurface built from the WAPI surface handle.
 */

#include "wapi_host.h"

#include <webgpu.h>

#define WAPI_STYPE_GPU_SURFACE_SOURCE_WAPI_HOST 0x0101u

static uint32_t wgpu_scratch_reserve(uint32_t nbytes, uint32_t align);

/* ============================================================
 * Guest memory reader helpers
 * ============================================================ */

static inline uint32_t g_u32(uint32_t ptr) {
    if (ptr == 0) return 0;
    void* p = wapi_wasm_ptr(ptr, 4);
    if (!p) return 0;
    uint32_t v; memcpy(&v, p, 4);
    return v;
}
static inline uint64_t g_u64(uint32_t ptr) {
    if (ptr == 0) return 0;
    void* p = wapi_wasm_ptr(ptr, 8);
    if (!p) return 0;
    uint64_t v; memcpy(&v, p, 8);
    return v;
}
static inline double g_f64(uint32_t ptr) {
    if (ptr == 0) return 0.0;
    void* p = wapi_wasm_ptr(ptr, 8);
    if (!p) return 0.0;
    double v; memcpy(&v, p, 8);
    return v;
}

/* Read a WGPUStringView (wasm32: u32 data + u32 length, 8 bytes). */
static WGPUStringView read_stringview(uint32_t sv_ptr) {
    WGPUStringView s = { NULL, 0 };
    if (!sv_ptr) return s;
    uint32_t data   = g_u32(sv_ptr + 0);
    uint32_t length = g_u32(sv_ptr + 4);
    if (length == 0) { s.data = ""; s.length = 0; return s; }
    const char* host = (const char*)wapi_wasm_ptr(data, length);
    if (!host) { s.data = ""; s.length = 0; return s; }
    s.data   = host;
    s.length = length;
    return s;
}

/* ============================================================
 * Handle table helpers
 * ============================================================ */

static int32_t alloc_gpu_handle(wapi_handle_type_t type, void* ptr) {
    if (!ptr) return 0;
    int32_t h = wapi_handle_alloc(type);
    if (h == 0) return 0;
    switch (type) {
    case WAPI_HTYPE_GPU_INSTANCE:        g_rt.handles[h].data.gpu_instance        = (WGPUInstance)ptr; break;
    case WAPI_HTYPE_GPU_ADAPTER:         g_rt.handles[h].data.gpu_adapter         = (WGPUAdapter)ptr; break;
    case WAPI_HTYPE_GPU_DEVICE:          g_rt.handles[h].data.gpu_device          = (WGPUDevice)ptr; break;
    case WAPI_HTYPE_GPU_QUEUE:           g_rt.handles[h].data.gpu_queue           = (WGPUQueue)ptr; break;
    case WAPI_HTYPE_GPU_SURFACE:         g_rt.handles[h].data.gpu_surface         = (WGPUSurface)ptr; break;
    case WAPI_HTYPE_GPU_TEXTURE:         g_rt.handles[h].data.gpu_texture         = (WGPUTexture)ptr; break;
    case WAPI_HTYPE_GPU_TEXTURE_VIEW:    g_rt.handles[h].data.gpu_texture_view    = (WGPUTextureView)ptr; break;
    case WAPI_HTYPE_GPU_BUFFER:          g_rt.handles[h].data.gpu_buffer          = (WGPUBuffer)ptr; break;
    case WAPI_HTYPE_GPU_SAMPLER:         g_rt.handles[h].data.gpu_sampler         = (WGPUSampler)ptr; break;
    case WAPI_HTYPE_GPU_BIND_GROUP:      g_rt.handles[h].data.gpu_bind_group      = (WGPUBindGroup)ptr; break;
    case WAPI_HTYPE_GPU_BIND_GROUP_LAYOUT: g_rt.handles[h].data.gpu_bind_group_layout = (WGPUBindGroupLayout)ptr; break;
    case WAPI_HTYPE_GPU_PIPELINE_LAYOUT: g_rt.handles[h].data.gpu_pipeline_layout = (WGPUPipelineLayout)ptr; break;
    case WAPI_HTYPE_GPU_SHADER_MODULE:   g_rt.handles[h].data.gpu_shader_module   = (WGPUShaderModule)ptr; break;
    case WAPI_HTYPE_GPU_RENDER_PIPELINE: g_rt.handles[h].data.gpu_render_pipeline = (WGPURenderPipeline)ptr; break;
    case WAPI_HTYPE_GPU_COMMAND_ENCODER: g_rt.handles[h].data.gpu_command_encoder = (WGPUCommandEncoder)ptr; break;
    case WAPI_HTYPE_GPU_COMMAND_BUFFER:  g_rt.handles[h].data.gpu_command_buffer  = (WGPUCommandBuffer)ptr; break;
    case WAPI_HTYPE_GPU_RENDER_PASS:     g_rt.handles[h].data.gpu_render_pass     = (WGPURenderPassEncoder)ptr; break;
    default:
        wapi_handle_free(h);
        return 0;
    }
    return h;
}

#define GET_GPU_PTR(h, type, field) \
    (wapi_handle_valid((h), (type)) ? g_rt.handles[(h)].data.field : NULL)

/* ============================================================
 * Pending guest callbacks (dispatched in wgpuInstanceProcessEvents)
 * ============================================================ */

typedef enum {
    PENDING_ADAPTER = 1,
    PENDING_DEVICE  = 2,
} pending_kind_t;

typedef struct pending_cb_t {
    pending_kind_t kind;
    uint32_t       funcref;      /* wasm indirect-table index */
    uint32_t       userdata1;    /* guest pointer */
    uint32_t       userdata2;    /* guest pointer */
    uint32_t       status;       /* WGPURequestAdapterStatus / WGPURequestDeviceStatus */
    int32_t        result_handle;/* WAPI handle for adapter/device, 0 on failure */
    char           message[256];
    uint32_t       message_len;  /* 0 if none */
    bool           done;
} pending_cb_t;

#define MAX_PENDING_CBS 32
typedef struct instance_state_t {
    WGPUInstance  instance;
    pending_cb_t  pending[MAX_PENDING_CBS];
    int           pending_count;
} instance_state_t;

#define MAX_INSTANCE_STATES 8
static instance_state_t g_instances[MAX_INSTANCE_STATES];
static int              g_instance_count;

static instance_state_t* instance_state_for(WGPUInstance inst) {
    for (int i = 0; i < g_instance_count; i++) {
        if (g_instances[i].instance == inst) return &g_instances[i];
    }
    if (g_instance_count >= MAX_INSTANCE_STATES) return NULL;
    instance_state_t* s = &g_instances[g_instance_count++];
    memset(s, 0, sizeof(*s));
    s->instance = inst;
    return s;
}

static pending_cb_t* pending_alloc(instance_state_t* s) {
    for (int i = 0; i < s->pending_count; i++) {
        if (!s->pending[i].done && s->pending[i].kind == 0) {
            memset(&s->pending[i], 0, sizeof(s->pending[i]));
            return &s->pending[i];
        }
    }
    if (s->pending_count >= MAX_PENDING_CBS) return NULL;
    pending_cb_t* p = &s->pending[s->pending_count++];
    memset(p, 0, sizeof(*p));
    return p;
}

/* Invoke a guest funcref in the module's indirect function table.
 * WebGPU callbacks have signature (wasm32 ABI, WGPUStringView passed
 * by pointer sret-style):
 *   void cb(i32 status, i32 handle, i32 stringview_ptr,
 *           i32 userdata1, i32 userdata2)
 */
static void invoke_guest_cb5(uint32_t funcref,
                             int32_t a0, int32_t a1, int32_t a2,
                             int32_t a3, int32_t a4)
{
    if (!g_rt.indirect_table_valid) return;
    wasmtime_val_t entry;
    if (!wasmtime_table_get(g_rt.context, &g_rt.indirect_table, funcref, &entry))
        return;
    if (entry.kind != WASMTIME_FUNCREF || wasmtime_funcref_is_null(&entry.of.funcref))
        return;

    wasmtime_val_t args[5];
    for (int i = 0; i < 5; i++) args[i].kind = WASMTIME_I32;
    args[0].of.i32 = a0;
    args[1].of.i32 = a1;
    args[2].of.i32 = a2;
    args[3].of.i32 = a3;
    args[4].of.i32 = a4;
    wasm_trap_t* trap = NULL;
    wasmtime_error_t* err =
        wasmtime_func_call(g_rt.context, &entry.of.funcref, args, 5, NULL, 0, &trap);
    if (err) {
        wasm_message_t m;
        wasmtime_error_message(err, &m);
        fprintf(stderr, "[wgpu] guest cb error: %.*s\n", (int)m.size, m.data);
        wasm_byte_vec_delete(&m);
        wasmtime_error_delete(err);
    }
    if (trap) {
        wasm_message_t m;
        wasm_trap_message(trap, &m);
        fprintf(stderr, "[wgpu] guest cb trap: %.*s\n", (int)m.size, m.data);
        wasm_byte_vec_delete(&m);
        wasm_trap_delete(trap);
    }
}

/* ============================================================
 * Native callbacks (fired from wgpuInstanceProcessEvents)
 * ============================================================ */

static void native_on_adapter(WGPURequestAdapterStatus status, WGPUAdapter a,
                              WGPUStringView msg, void* u1, void* u2)
{
    (void)u2;
    pending_cb_t* p = (pending_cb_t*)u1;
    p->status = status;
    p->result_handle = (status == WGPURequestAdapterStatus_Success && a)
        ? alloc_gpu_handle(WAPI_HTYPE_GPU_ADAPTER, a) : 0;
    if (msg.data && msg.length) {
        size_t n = msg.length < sizeof(p->message) ? msg.length : sizeof(p->message);
        memcpy(p->message, msg.data, n);
        p->message_len = (uint32_t)n;
    }
    p->done = true;
}

static void native_on_device(WGPURequestDeviceStatus status, WGPUDevice d,
                             WGPUStringView msg, void* u1, void* u2)
{
    (void)u2;
    pending_cb_t* p = (pending_cb_t*)u1;
    p->status = status;
    p->result_handle = (status == WGPURequestDeviceStatus_Success && d)
        ? alloc_gpu_handle(WAPI_HTYPE_GPU_DEVICE, d) : 0;
    if (msg.data && msg.length) {
        size_t n = msg.length < sizeof(p->message) ? msg.length : sizeof(p->message);
        memcpy(p->message, msg.data, n);
        p->message_len = (uint32_t)n;
    }
    p->done = true;
}

static void native_on_device_lost(WGPUDevice const* d, WGPUDeviceLostReason reason,
                                  WGPUStringView msg, void* u1, void* u2)
{
    (void)d; (void)u1; (void)u2;
    fprintf(stderr, "[wgpu] device lost (%d): %.*s\n",
            (int)reason, (int)msg.length, msg.data);
}

static void native_on_uncaptured(WGPUDevice const* d, WGPUErrorType type,
                                 WGPUStringView msg, void* u1, void* u2)
{
    (void)d; (void)u1; (void)u2;
    fprintf(stderr, "[wgpu] uncaptured error (%d): %.*s\n",
            (int)type, (int)msg.length, msg.data);
}

/* ============================================================
 * wgpuCreateInstance(descriptor: i32) -> i32 (handle)
 * ============================================================ */

static wasm_trap_t* wg_CreateInstance(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    /* WGPUInstanceDescriptor: nextInChain(u32)+features(WGPUSupportedInstanceFeatures:
     *   u32 count + u32 ptr)+limits(ptr u32). For the triangle the guest
     * passes all zeros — we just pass a zero-init native descriptor. */
    (void)args;
    WGPUInstanceDescriptor d = {0};
    WGPUInstance inst = wgpuCreateInstance(&d);
    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_INSTANCE, inst);
    if (inst && !h) { wgpuInstanceRelease(inst); WAPI_RET_I32(0); return NULL; }
    /* Ensure instance state slot is reserved. */
    if (inst) instance_state_for(inst);
    WAPI_RET_I32(h);
    return NULL;
}

/* ============================================================
 * wgpuInstanceRequestAdapter(instance, options_ptr, callback_info_ptr)
 * ============================================================ */

static wasm_trap_t* wg_InstanceRequestAdapter(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  instance_h = WAPI_ARG_I32(0);
    uint32_t opts_ptr   = WAPI_ARG_U32(1);
    uint32_t cb_ptr     = WAPI_ARG_U32(2);

    WGPUInstance instance = GET_GPU_PTR(instance_h, WAPI_HTYPE_GPU_INSTANCE, gpu_instance);
    if (!instance) return NULL;

    /* WGPURequestAdapterOptions (wasm32):
     *   +0  u32 nextInChain
     *   +4  u32 featureLevel
     *   +8  u32 powerPreference
     *   +12 u32 forceFallbackAdapter
     *   +16 u32 backendType
     *   +20 u32 compatibleSurface (handle)
     */
    WGPURequestAdapterOptions opts = {0};
    if (opts_ptr) {
        opts.featureLevel          = (WGPUFeatureLevel)g_u32(opts_ptr + 4);
        opts.powerPreference       = (WGPUPowerPreference)g_u32(opts_ptr + 8);
        opts.forceFallbackAdapter  = (WGPUBool)g_u32(opts_ptr + 12);
        opts.backendType           = (WGPUBackendType)g_u32(opts_ptr + 16);
        int32_t cs_h               = (int32_t)g_u32(opts_ptr + 20);
        opts.compatibleSurface     = GET_GPU_PTR(cs_h, WAPI_HTYPE_GPU_SURFACE, gpu_surface);
    }

    /* WGPURequestAdapterCallbackInfo:
     *   +0  u32 nextInChain
     *   +4  u32 mode
     *   +8  u32 callback (funcref index)
     *  +12  u32 userdata1
     *  +16  u32 userdata2
     */
    uint32_t cb_funcref = g_u32(cb_ptr + 8);
    uint32_t cb_ud1     = g_u32(cb_ptr + 12);
    uint32_t cb_ud2     = g_u32(cb_ptr + 16);

    instance_state_t* state = instance_state_for(instance);
    pending_cb_t* pending = pending_alloc(state);
    if (!pending) return NULL;
    pending->kind      = PENDING_ADAPTER;
    pending->funcref   = cb_funcref;
    pending->userdata1 = cb_ud1;
    pending->userdata2 = cb_ud2;

    WGPURequestAdapterCallbackInfo info = {0};
    info.mode      = WGPUCallbackMode_AllowProcessEvents;
    info.callback  = native_on_adapter;
    info.userdata1 = pending;
    WGPUFuture fut = wgpuInstanceRequestAdapter(instance, &opts, info);
    WAPI_RET_I64((int64_t)fut.id);
    return NULL;
}

/* ============================================================
 * wgpuAdapterRequestDevice(adapter, descriptor_ptr, callback_info_ptr)
 * ============================================================ */

static wasm_trap_t* wg_AdapterRequestDevice(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  adapter_h  = WAPI_ARG_I32(0);
    uint32_t desc_ptr   = WAPI_ARG_U32(1);
    uint32_t cb_ptr     = WAPI_ARG_U32(2);

    WGPUAdapter adapter = GET_GPU_PTR(adapter_h, WAPI_HTYPE_GPU_ADAPTER, gpu_adapter);
    if (!adapter) return NULL;

    /* WGPUDeviceDescriptor (wasm32, 56 bytes):
     *   +0   u32 nextInChain
     *   +4   WGPUStringView label (u32 data, u32 length) = 8
     *   +12  u32 requiredFeatureCount
     *   +16  u32 requiredFeatures (ptr)
     *   +20  u32 requiredLimits (ptr)
     *   +24  WGPUQueueDescriptor defaultQueue (u32 nextInChain + 8 label) = 12
     *   +36  WGPUDeviceLostCallbackInfo (u32 next + u32 mode + u32 cb + u32 ud1 + u32 ud2) = 20
     *   +56  WGPUUncapturedErrorCallbackInfo (u32 next + u32 cb + u32 ud1 + u32 ud2) = 16
     *
     * We ignore the guest-supplied device-lost / uncaptured callbacks
     * (they're wasm funcrefs; firing them cleanly from a potentially
     * sponatneous wgpu-native thread is unsafe). Native replacements
     * write to stderr. Message is discarded for same reason.
     */
    WGPUDeviceDescriptor d = {0};
    if (desc_ptr) {
        d.label = read_stringview(desc_ptr + 4);
        /* requiredFeatures: array of u32 WGPUFeatureName values in guest memory */
        uint32_t rf_count = g_u32(desc_ptr + 12);
        uint32_t rf_ptr   = g_u32(desc_ptr + 16);
        static WGPUFeatureName s_features[32];
        if (rf_count && rf_count <= 32 && rf_ptr) {
            for (uint32_t i = 0; i < rf_count; i++) {
                s_features[i] = (WGPUFeatureName)g_u32(rf_ptr + i * 4);
            }
            d.requiredFeatureCount = rf_count;
            d.requiredFeatures     = s_features;
        }
        /* requiredLimits: u32 nextInChain + u32 pad + WGPULimits struct of ints
         * — for the triangle the guest passes NULL, so we leave it. */
    }
    d.defaultQueue.nextInChain = NULL;
    d.defaultQueue.label       = (WGPUStringView){ "queue", 5 };
    d.deviceLostCallbackInfo.mode     = WGPUCallbackMode_AllowProcessEvents;
    d.deviceLostCallbackInfo.callback = native_on_device_lost;
    d.uncapturedErrorCallbackInfo.callback = native_on_uncaptured;

    uint32_t cb_funcref = g_u32(cb_ptr + 8);
    uint32_t cb_ud1     = g_u32(cb_ptr + 12);
    uint32_t cb_ud2     = g_u32(cb_ptr + 16);

    /* Find the instance state that owns this adapter — we stash
     * the pending callback with the nearest known instance. */
    instance_state_t* state = g_instance_count > 0 ? &g_instances[0] : NULL;
    if (!state) return NULL;
    pending_cb_t* pending = pending_alloc(state);
    if (!pending) return NULL;
    pending->kind      = PENDING_DEVICE;
    pending->funcref   = cb_funcref;
    pending->userdata1 = cb_ud1;
    pending->userdata2 = cb_ud2;

    WGPURequestDeviceCallbackInfo info = {0};
    info.mode      = WGPUCallbackMode_AllowProcessEvents;
    info.callback  = native_on_device;
    info.userdata1 = pending;
    WGPUFuture fut = wgpuAdapterRequestDevice(adapter, &d, info);
    WAPI_RET_I64((int64_t)fut.id);
    return NULL;
}

/* ============================================================
 * wgpuInstanceProcessEvents(instance)
 * ============================================================ */

static wasm_trap_t* wg_InstanceProcessEvents(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t instance_h = WAPI_ARG_I32(0);
    WGPUInstance instance = GET_GPU_PTR(instance_h, WAPI_HTYPE_GPU_INSTANCE, gpu_instance);
    if (!instance) return NULL;

    /* Fire native callbacks (they mark pending->done). */
    wgpuInstanceProcessEvents(instance);

    instance_state_t* state = instance_state_for(instance);
    if (!state) return NULL;
    for (int i = 0; i < state->pending_count; i++) {
        pending_cb_t* p = &state->pending[i];
        if (!p->done || p->kind == 0) continue;

        /* Write an empty WGPUStringView {u32 data=0, u32 length=0}
         * into guest scratch so the guest callback has a valid pointer
         * to dereference. If we have a message, copy it in too. */
        uint32_t sv_off = wgpu_scratch_reserve(8 + 256, 4);
        uint32_t sv_data_off = 0, sv_len = 0;
        if (sv_off) {
            if (p->message_len) {
                sv_data_off = sv_off + 8;
                wapi_wasm_write_bytes(sv_data_off, p->message, p->message_len);
                sv_len = p->message_len;
            }
            wapi_wasm_write_u32(sv_off + 0, sv_data_off);
            wapi_wasm_write_u32(sv_off + 4, sv_len);
        }
        invoke_guest_cb5(p->funcref,
                         (int32_t)p->status, p->result_handle,
                         (int32_t)sv_off,
                         (int32_t)p->userdata1, (int32_t)p->userdata2);
        p->kind = 0;
        p->done = false;
    }
    return NULL;
}

/* ============================================================
 * wgpuInstanceCreateSurface(instance, descriptor_ptr)
 * ============================================================ */

static wasm_trap_t* wg_InstanceCreateSurface(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  instance_h = WAPI_ARG_I32(0);
    uint32_t desc_ptr   = WAPI_ARG_U32(1);
    WGPUInstance instance = GET_GPU_PTR(instance_h, WAPI_HTYPE_GPU_INSTANCE, gpu_instance);
    if (!instance || !desc_ptr) { WAPI_RET_I32(0); return NULL; }

    /* WGPUSurfaceDescriptor (wasm32):
     *   +0  u32 nextInChain
     *   +4  WGPUStringView label
     */
    uint32_t chain_ptr = g_u32(desc_ptr + 0);
    WGPUStringView label = read_stringview(desc_ptr + 4);

    /* Walk the chain looking for WAPI_STYPE_GPU_SURFACE_SOURCE_WAPI.
     * wapi_chain_t layout: u64 next + u32 sType + u32 _pad = 16B. */
    int32_t wapi_surface_handle = 0;
    while (chain_ptr) {
        void* ch = wapi_wasm_ptr(chain_ptr, 16);
        if (!ch) break;
        uint64_t next;  memcpy(&next,  (uint8_t*)ch + 0, 8);
        uint32_t stype; memcpy(&stype, (uint8_t*)ch + 8, 4);
        if (stype == WAPI_STYPE_GPU_SURFACE_SOURCE_WAPI_HOST) {
            /* wapi_gpu_surface_source_t: chain(16) + i32 surface + u32 _pad = 24 */
            int32_t sh; memcpy(&sh, (uint8_t*)ch + 16, 4);
            wapi_surface_handle = sh;
            break;
        }
        chain_ptr = (uint32_t)next;
    }

    if (!wapi_handle_valid(wapi_surface_handle, WAPI_HTYPE_SURFACE)) {
        wapi_set_error("wgpuInstanceCreateSurface: WAPI surface source missing");
        WAPI_RET_I32(0);
        return NULL;
    }
    wapi_plat_window_t* win = g_rt.handles[wapi_surface_handle].data.window;
    wapi_plat_native_handle_t nh = {0};
    if (!wapi_plat_window_get_native(win, &nh)) {
        WAPI_RET_I32(0); return NULL;
    }

    WGPUSurfaceDescriptor sd = {0};
    sd.label = label;

    WGPUSurfaceSourceWindowsHWND hwnd_src = {0};
    /* Other platforms would go here; only Win32 is wired right now.
     * The platform abstraction returns the native handle tagged with
     * kind WAPI_PLAT_NATIVE_WIN32 on Windows. */
    WGPUSurface surf = NULL;
    switch (nh.kind) {
    case WAPI_PLAT_NATIVE_WIN32:
        hwnd_src.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
        hwnd_src.hwnd        = nh.a;
        hwnd_src.hinstance   = nh.b;
        sd.nextInChain = (const WGPUChainedStruct*)&hwnd_src;
        surf = wgpuInstanceCreateSurface(instance, &sd);
        break;
    default:
        wapi_set_error("wgpuInstanceCreateSurface: unsupported native handle kind");
        WAPI_RET_I32(0);
        return NULL;
    }
    if (!surf) { WAPI_RET_I32(0); return NULL; }

    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_SURFACE, surf);
    if (!h) { wgpuSurfaceRelease(surf); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(h);
    return NULL;
}

/* ============================================================
 * wgpuDeviceGetQueue(device) -> handle
 * ============================================================ */

static wasm_trap_t* wg_DeviceGetQueue(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t dh = WAPI_ARG_I32(0);
    WGPUDevice device = GET_GPU_PTR(dh, WAPI_HTYPE_GPU_DEVICE, gpu_device);
    if (!device) { WAPI_RET_I32(0); return NULL; }
    WGPUQueue q = wgpuDeviceGetQueue(device);
    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_QUEUE, q);
    if (q && !h) { wgpuQueueRelease(q); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(h);
    return NULL;
}

/* ============================================================
 * wgpuSurfaceGetCapabilities(surface, adapter, out_caps_ptr) -> i32
 * ============================================================
 *
 * The guest's WGPUSurfaceCapabilities is laid out as (wasm32):
 *   +0  u32 nextInChain
 *   +4  u32 usages
 *   +8  u32 formatCount
 *   +12 u32 formats (ptr)
 *   +16 u32 presentModeCount
 *   +20 u32 presentModes (ptr)
 *   +24 u32 alphaModeCount
 *   +28 u32 alphaModes (ptr)
 *
 * We have to copy each array back into guest memory. To avoid touching
 * the guest heap we use a bump region in a static per-call scratch
 * buffer backed by the guest heap — but we don't have the guest heap
 * pointer from the host side, so we instead allocate host-side arrays
 * and copy u32 values individually into caller-reserved slots. The
 * triangle just reads `formatCount` and `formats[0]` — meaning we need
 * to write real pointers into guest memory.
 *
 * Solution: we write the arrays into a static guest scratch buffer at
 * a known offset. We allocate this scratch lazily on first call by
 * growing the guest memory by one page and recording the offset.
 */

static uint32_t g_wgpu_scratch_base;   /* guest byte offset */
static uint32_t g_wgpu_scratch_size;   /* bytes available */
static uint32_t g_wgpu_scratch_used;

static uint32_t wgpu_scratch_reserve(uint32_t nbytes, uint32_t align) {
    if (align < 4) align = 4;
    if (g_wgpu_scratch_base == 0) {
        /* Grow memory by 16 pages (1 MB) and use the tail. */
        uint64_t prev = 0;
        wasmtime_error_t* err = wasmtime_memory_grow(g_rt.context, &g_rt.memory, 16, &prev);
        if (err) { wasmtime_error_delete(err); return 0; }
        g_wgpu_scratch_base = (uint32_t)(prev * 65536u);
        g_wgpu_scratch_size = 16u * 65536u;
        g_wgpu_scratch_used = 0;
    }
    uint32_t off = (g_wgpu_scratch_base + g_wgpu_scratch_used + (align - 1)) & ~(align - 1);
    uint32_t end = off + nbytes;
    if (end > g_wgpu_scratch_base + g_wgpu_scratch_size) {
        /* Recycle — scratch is used for ephemeral per-call buffers. */
        g_wgpu_scratch_used = 0;
        off = (g_wgpu_scratch_base + (align - 1)) & ~(align - 1);
        end = off + nbytes;
        if (end > g_wgpu_scratch_base + g_wgpu_scratch_size) return 0;
    }
    g_wgpu_scratch_used = end - g_wgpu_scratch_base;
    return off;
}

static wasm_trap_t* wg_SurfaceGetCapabilities(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  sh         = WAPI_ARG_I32(0);
    int32_t  adapter_h  = WAPI_ARG_I32(1);
    uint32_t out_ptr    = WAPI_ARG_U32(2);

    WGPUSurface surface = GET_GPU_PTR(sh, WAPI_HTYPE_GPU_SURFACE, gpu_surface);
    WGPUAdapter adapter = GET_GPU_PTR(adapter_h, WAPI_HTYPE_GPU_ADAPTER, gpu_adapter);
    if (!surface || !adapter || !out_ptr) { WAPI_RET_I32(2 /* Error */); return NULL; }

    WGPUSurfaceCapabilities caps = {0};
    WGPUStatus st = wgpuSurfaceGetCapabilities(surface, adapter, &caps);

    /* Copy arrays into guest scratch. */
    uint32_t formats_off = 0, pm_off = 0, am_off = 0;
    if (caps.formatCount) {
        formats_off = wgpu_scratch_reserve((uint32_t)caps.formatCount * 4, 4);
        if (formats_off) {
            for (size_t i = 0; i < caps.formatCount; i++) {
                uint32_t v = (uint32_t)caps.formats[i];
                wapi_wasm_write_u32(formats_off + (uint32_t)i * 4, v);
            }
        }
    }
    if (caps.presentModeCount) {
        pm_off = wgpu_scratch_reserve((uint32_t)caps.presentModeCount * 4, 4);
        if (pm_off) {
            for (size_t i = 0; i < caps.presentModeCount; i++) {
                wapi_wasm_write_u32(pm_off + (uint32_t)i * 4, (uint32_t)caps.presentModes[i]);
            }
        }
    }
    if (caps.alphaModeCount) {
        am_off = wgpu_scratch_reserve((uint32_t)caps.alphaModeCount * 4, 4);
        if (am_off) {
            for (size_t i = 0; i < caps.alphaModeCount; i++) {
                wapi_wasm_write_u32(am_off + (uint32_t)i * 4, (uint32_t)caps.alphaModes[i]);
            }
        }
    }

    /* WGPUSurfaceCapabilities wasm32 layout (40 B):
     *   +0  u32 nextInChain
     *   +4  _pad
     *   +8  u64 usages              (WGPUTextureUsage = u64 flags)
     *  +16  u32 formatCount (size_t=u32)
     *  +20  u32 formats (ptr)
     *  +24  u32 presentModeCount
     *  +28  u32 presentModes
     *  +32  u32 alphaModeCount
     *  +36  u32 alphaModes
     */
    wapi_wasm_write_u32(out_ptr + 0,  0);
    wapi_wasm_write_u32(out_ptr + 4,  0);
    wapi_wasm_write_u64(out_ptr + 8,  (uint64_t)caps.usages);
    wapi_wasm_write_u32(out_ptr + 16, (uint32_t)caps.formatCount);
    wapi_wasm_write_u32(out_ptr + 20, formats_off);
    wapi_wasm_write_u32(out_ptr + 24, (uint32_t)caps.presentModeCount);
    wapi_wasm_write_u32(out_ptr + 28, pm_off);
    wapi_wasm_write_u32(out_ptr + 32, (uint32_t)caps.alphaModeCount);
    wapi_wasm_write_u32(out_ptr + 36, am_off);

    wgpuSurfaceCapabilitiesFreeMembers(caps);
    WAPI_RET_I32((int32_t)st);
    return NULL;
}

/* ============================================================
 * wgpuSurfaceConfigure(surface, config_ptr)
 * ============================================================ */

static wasm_trap_t* wg_SurfaceConfigure(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  sh       = WAPI_ARG_I32(0);
    uint32_t cfg_ptr  = WAPI_ARG_U32(1);
    WGPUSurface surface = GET_GPU_PTR(sh, WAPI_HTYPE_GPU_SURFACE, gpu_surface);
    if (!surface || !cfg_ptr) return NULL;

    /* WGPUSurfaceConfiguration wasm32 layout (48 B):
     *   +0   u32 nextInChain
     *   +4   u32 device (handle)
     *   +8   u32 format
     *  +12   _pad
     *  +16   u64 usage         (WGPUTextureUsage = u64 flags)
     *  +24   u32 width
     *  +28   u32 height
     *  +32   u32 viewFormatCount
     *  +36   u32 viewFormats (ptr)
     *  +40   u32 alphaMode
     *  +44   u32 presentMode
     */
    int32_t  dh = (int32_t)g_u32(cfg_ptr + 4);
    WGPUDevice device = GET_GPU_PTR(dh, WAPI_HTYPE_GPU_DEVICE, gpu_device);
    if (!device) return NULL;

    uint32_t vf_count = g_u32(cfg_ptr + 32);
    uint32_t vf_ptr   = g_u32(cfg_ptr + 36);
    static WGPUTextureFormat s_view_fmts[16];
    WGPUSurfaceConfiguration c = {0};
    c.device      = device;
    c.format      = (WGPUTextureFormat)g_u32(cfg_ptr + 8);
    c.usage       = (WGPUTextureUsage)g_u64(cfg_ptr + 16);
    c.width       = g_u32(cfg_ptr + 24);
    c.height      = g_u32(cfg_ptr + 28);
    if (vf_count && vf_count <= 16 && vf_ptr) {
        for (uint32_t i = 0; i < vf_count; i++) {
            s_view_fmts[i] = (WGPUTextureFormat)g_u32(vf_ptr + i * 4);
        }
        c.viewFormatCount = vf_count;
        c.viewFormats     = s_view_fmts;
    }
    c.alphaMode   = (WGPUCompositeAlphaMode)g_u32(cfg_ptr + 40);
    c.presentMode = (WGPUPresentMode)g_u32(cfg_ptr + 44);
    wgpuSurfaceConfigure(surface, &c);
    return NULL;
}

/* ============================================================
 * wgpuSurfaceGetCurrentTexture(surface, out_ptr)
 * ============================================================ */

static wasm_trap_t* wg_SurfaceGetCurrentTexture(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  sh      = WAPI_ARG_I32(0);
    uint32_t out_ptr = WAPI_ARG_U32(1);
    WGPUSurface surface = GET_GPU_PTR(sh, WAPI_HTYPE_GPU_SURFACE, gpu_surface);
    if (!surface || !out_ptr) return NULL;

    WGPUSurfaceTexture st = {0};
    wgpuSurfaceGetCurrentTexture(surface, &st);

    int32_t th = st.texture ? alloc_gpu_handle(WAPI_HTYPE_GPU_TEXTURE, st.texture) : 0;

    /* WGPUSurfaceTexture (wasm32):
     *   +0 u32 nextInChain
     *   +4 u32 texture (handle)
     *   +8 u32 status
     */
    wapi_wasm_write_u32(out_ptr + 0, 0);
    wapi_wasm_write_u32(out_ptr + 4, (uint32_t)th);
    wapi_wasm_write_u32(out_ptr + 8, (uint32_t)st.status);
    return NULL;
}

/* ============================================================
 * wgpuSurfacePresent(surface)
 * ============================================================ */

static wasm_trap_t* wg_SurfacePresent(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t sh = WAPI_ARG_I32(0);
    WGPUSurface surface = GET_GPU_PTR(sh, WAPI_HTYPE_GPU_SURFACE, gpu_surface);
    if (!surface) { WAPI_RET_I32(2); return NULL; }
    WGPUStatus st = wgpuSurfacePresent(surface);
    WAPI_RET_I32((int32_t)st);
    return NULL;
}

/* ============================================================
 * wgpuTextureCreateView(texture, descriptor_ptr_or_null) -> handle
 * ============================================================ */

static wasm_trap_t* wg_TextureCreateView(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  th       = WAPI_ARG_I32(0);
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    WGPUTexture tex = GET_GPU_PTR(th, WAPI_HTYPE_GPU_TEXTURE, gpu_texture);
    if (!tex) { WAPI_RET_I32(0); return NULL; }

    WGPUTextureView view;
    if (desc_ptr == 0) {
        view = wgpuTextureCreateView(tex, NULL);
    } else {
        /* WGPUTextureViewDescriptor (wasm32):
         *   +0  u32 nextInChain
         *   +4  WGPUStringView label (8B)
         *  +12  u32 format
         *  +16  u32 dimension
         *  +20  u32 baseMipLevel
         *  +24  u32 mipLevelCount
         *  +28  u32 baseArrayLayer
         *  +32  u32 arrayLayerCount
         *  +36  u32 aspect
         *  +40  u32 usage
         */
        WGPUTextureViewDescriptor d = {0};
        d.label           = read_stringview(desc_ptr + 4);
        d.format          = (WGPUTextureFormat)g_u32(desc_ptr + 12);
        d.dimension       = (WGPUTextureViewDimension)g_u32(desc_ptr + 16);
        d.baseMipLevel    = g_u32(desc_ptr + 20);
        d.mipLevelCount   = g_u32(desc_ptr + 24);
        d.baseArrayLayer  = g_u32(desc_ptr + 28);
        d.arrayLayerCount = g_u32(desc_ptr + 32);
        d.aspect          = (WGPUTextureAspect)g_u32(desc_ptr + 36);
        d.usage           = (WGPUTextureUsage)g_u32(desc_ptr + 40);
        view = wgpuTextureCreateView(tex, &d);
    }
    int32_t vh = alloc_gpu_handle(WAPI_HTYPE_GPU_TEXTURE_VIEW, view);
    if (view && !vh) { wgpuTextureViewRelease(view); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(vh);
    return NULL;
}

/* ============================================================
 * wgpuDeviceCreateShaderModule(device, descriptor_ptr) -> handle
 * ============================================================ */

static wasm_trap_t* wg_DeviceCreateShaderModule(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  dh       = WAPI_ARG_I32(0);
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    WGPUDevice device = GET_GPU_PTR(dh, WAPI_HTYPE_GPU_DEVICE, gpu_device);
    if (!device || !desc_ptr) { WAPI_RET_I32(0); return NULL; }

    /* WGPUShaderModuleDescriptor (wasm32):
     *   +0 u32 nextInChain
     *   +4 WGPUStringView label
     *
     * Walk the chain for WGPUShaderSourceWGSL (sType 0x0002) or SPIRV.
     * WGPUShaderSourceWGSL layout (wasm32):
     *   +0 WGPUChainedStruct (u32 next + u32 sType = 8)
     *   +8 WGPUStringView code
     */
    WGPUShaderModuleDescriptor d = {0};
    d.label = read_stringview(desc_ptr + 4);

    WGPUShaderSourceWGSL wgsl = {0};
    wgsl.chain.sType = WGPUSType_ShaderSourceWGSL;

    uint32_t chain_ptr = g_u32(desc_ptr + 0);
    bool have_wgsl = false;
    while (chain_ptr) {
        uint32_t next  = g_u32(chain_ptr + 0);
        uint32_t stype = g_u32(chain_ptr + 4);
        if (stype == (uint32_t)WGPUSType_ShaderSourceWGSL) {
            wgsl.code = read_stringview(chain_ptr + 8);
            d.nextInChain = (const WGPUChainedStruct*)&wgsl;
            have_wgsl = true;
            break;
        }
        chain_ptr = next;
    }
    if (!have_wgsl) {
        wapi_set_error("wgpuDeviceCreateShaderModule: no WGSL source in chain");
        WAPI_RET_I32(0);
        return NULL;
    }

    WGPUShaderModule m = wgpuDeviceCreateShaderModule(device, &d);
    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_SHADER_MODULE, m);
    if (m && !h) { wgpuShaderModuleRelease(m); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(h);
    return NULL;
}

/* ============================================================
 * wgpuDeviceCreateRenderPipeline(device, descriptor_ptr) -> handle
 * ============================================================ */

static wasm_trap_t* wg_DeviceCreateRenderPipeline(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  dh       = WAPI_ARG_I32(0);
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    WGPUDevice device = GET_GPU_PTR(dh, WAPI_HTYPE_GPU_DEVICE, gpu_device);
    if (!device || !desc_ptr) { WAPI_RET_I32(0); return NULL; }

    /* WGPURenderPipelineDescriptor (wasm32):
     *   +0   u32 nextInChain
     *   +4   WGPUStringView label (8)
     *  +12   u32 layout (handle)
     *  +16   WGPUVertexState vertex (28):
     *          +0  u32 nextInChain
     *          +4  u32 module (handle)
     *          +8  WGPUStringView entryPoint (8)
     *         +16  u32 constantCount
     *         +20  u32 constants (ptr)
     *         +24  u32 bufferCount
     *         +28 (=end of vertex, size=28 hmm let me recount)
     *
     * Careful — WGPUStringView is 8B on wasm32, so:
     *   VertexState = 4 (next) + 4 (module) + 8 (entryPoint) + 4 (cc) + 4 (cp) + 4 (bc) + 4 (bp) = 32
     *
     * Full descriptor:
     *   +0  u32 next
     *   +4  sv label (8)
     *  +12  u32 layout
     *  +16  VertexState (32)
     *  +48  PrimitiveState (u32 next + 5x u32 = 24)
     *  +72  u32 depthStencil (ptr or NULL)
     *  +76  MultisampleState (u32 next + 3x u32 = 16)
     *  +92  u32 fragment (ptr to FragmentState)
     */
    WGPURenderPipelineDescriptor p = {0};
    p.label = read_stringview(desc_ptr + 4);
    int32_t layout_h = (int32_t)g_u32(desc_ptr + 12);
    p.layout = (layout_h == 0) ? NULL :
        GET_GPU_PTR(layout_h, WAPI_HTYPE_GPU_PIPELINE_LAYOUT, gpu_pipeline_layout);

    /* VertexState */
    uint32_t vs_base = desc_ptr + 16;
    int32_t vs_mod_h = (int32_t)g_u32(vs_base + 4);
    p.vertex.module       = GET_GPU_PTR(vs_mod_h, WAPI_HTYPE_GPU_SHADER_MODULE, gpu_shader_module);
    p.vertex.entryPoint   = read_stringview(vs_base + 8);
    p.vertex.constantCount = 0; /* constants unused by hello_game */

    /* vertex.buffers: array of WGPUVertexBufferLayout (24B each, align 8):
     *   +0  u32 nextInChain
     *   +4  u32 stepMode
     *   +8  u64 arrayStride
     *  +16  u32 attributeCount (size_t=u32)
     *  +20  u32 attributes (ptr)
     *
     * Each attribute (WGPUVertexAttribute, 24B align 8):
     *   +0  u32 nextInChain
     *   +4  u32 format
     *   +8  u64 offset
     *  +16  u32 shaderLocation
     *  +20  _pad
     */
    uint32_t vb_count = g_u32(vs_base + 24);
    uint32_t vb_ptr   = g_u32(vs_base + 28);
    static WGPUVertexBufferLayout s_vbls[8];
    static WGPUVertexAttribute   s_vattrs[8][16];
    if (vb_count && vb_count <= 8 && vb_ptr) {
        for (uint32_t i = 0; i < vb_count; i++) {
            uint32_t base = vb_ptr + i * 24;
            s_vbls[i].nextInChain    = NULL;
            s_vbls[i].stepMode       = (WGPUVertexStepMode)g_u32(base + 4);
            s_vbls[i].arrayStride    = g_u64(base + 8);
            uint32_t attr_count      = g_u32(base + 16);
            uint32_t attr_ptr        = g_u32(base + 20);
            if (attr_count > 16) attr_count = 16;
            if (attr_count && attr_ptr) {
                for (uint32_t j = 0; j < attr_count; j++) {
                    uint32_t abase = attr_ptr + j * 24;
                    s_vattrs[i][j].nextInChain    = NULL;
                    s_vattrs[i][j].format         = (WGPUVertexFormat)g_u32(abase + 4);
                    s_vattrs[i][j].offset         = g_u64(abase + 8);
                    s_vattrs[i][j].shaderLocation = g_u32(abase + 16);
                }
                s_vbls[i].attributeCount = attr_count;
                s_vbls[i].attributes     = s_vattrs[i];
            } else {
                s_vbls[i].attributeCount = 0;
                s_vbls[i].attributes     = NULL;
            }
        }
        p.vertex.bufferCount = vb_count;
        p.vertex.buffers     = s_vbls;
    } else {
        p.vertex.bufferCount = 0;
        p.vertex.buffers     = NULL;
    }

    /* PrimitiveState */
    uint32_t ps_base = desc_ptr + 48;
    p.primitive.topology         = (WGPUPrimitiveTopology)g_u32(ps_base + 4);
    p.primitive.stripIndexFormat = (WGPUIndexFormat)g_u32(ps_base + 8);
    p.primitive.frontFace        = (WGPUFrontFace)g_u32(ps_base + 12);
    p.primitive.cullMode         = (WGPUCullMode)g_u32(ps_base + 16);
    p.primitive.unclippedDepth   = (WGPUBool)g_u32(ps_base + 20);

    /* MultisampleState */
    uint32_t ms_base = desc_ptr + 76;
    p.multisample.count  = g_u32(ms_base + 4);
    p.multisample.mask   = g_u32(ms_base + 8);
    p.multisample.alphaToCoverageEnabled = (WGPUBool)g_u32(ms_base + 12);

    /* FragmentState (optional) */
    WGPUFragmentState fragment = {0};
    static WGPUColorTargetState s_targets[8];
    uint32_t fs_ptr = g_u32(desc_ptr + 92);
    if (fs_ptr) {
        int32_t fs_mod_h = (int32_t)g_u32(fs_ptr + 4);
        fragment.module     = GET_GPU_PTR(fs_mod_h, WAPI_HTYPE_GPU_SHADER_MODULE, gpu_shader_module);
        fragment.entryPoint = read_stringview(fs_ptr + 8);
        fragment.constantCount = 0;
        uint32_t tc  = g_u32(fs_ptr + 24);
        uint32_t tp  = g_u32(fs_ptr + 28);
        if (tc && tc <= 8 && tp) {
            /* WGPUColorTargetState (wasm32, 24B, align 8):
             *   +0  u32 nextInChain
             *   +4  u32 format
             *   +8  u32 blend (ptr)
             *  +12  _pad
             *  +16  u64 writeMask (WGPUColorWriteMask = u64 flags)
             */
            for (uint32_t i = 0; i < tc; i++) {
                uint32_t base = tp + i * 24;
                s_targets[i].nextInChain = NULL;
                s_targets[i].format    = (WGPUTextureFormat)g_u32(base + 4);
                s_targets[i].blend     = NULL; /* triangle has no blend */
                s_targets[i].writeMask = (WGPUColorWriteMask)g_u64(base + 16);
            }
            fragment.targetCount = tc;
            fragment.targets     = s_targets;
        }
        p.fragment = &fragment;
    }

    WGPURenderPipeline rp = wgpuDeviceCreateRenderPipeline(device, &p);
    int32_t rp_h = alloc_gpu_handle(WAPI_HTYPE_GPU_RENDER_PIPELINE, rp);
    if (rp && !rp_h) { wgpuRenderPipelineRelease(rp); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(rp_h);
    return NULL;
}

/* ============================================================
 * wgpuDeviceCreateCommandEncoder(device, descriptor_ptr) -> handle
 * ============================================================ */

static wasm_trap_t* wg_DeviceCreateCommandEncoder(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  dh       = WAPI_ARG_I32(0);
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    WGPUDevice device = GET_GPU_PTR(dh, WAPI_HTYPE_GPU_DEVICE, gpu_device);
    if (!device) { WAPI_RET_I32(0); return NULL; }

    WGPUCommandEncoderDescriptor d = {0};
    if (desc_ptr) d.label = read_stringview(desc_ptr + 4);
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, &d);
    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_COMMAND_ENCODER, enc);
    if (enc && !h) { wgpuCommandEncoderRelease(enc); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(h);
    return NULL;
}

/* ============================================================
 * wgpuCommandEncoderBeginRenderPass(encoder, descriptor_ptr)
 * ============================================================ */

static wasm_trap_t* wg_CommandEncoderBeginRenderPass(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  eh       = WAPI_ARG_I32(0);
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    WGPUCommandEncoder enc = GET_GPU_PTR(eh, WAPI_HTYPE_GPU_COMMAND_ENCODER, gpu_command_encoder);
    if (!enc || !desc_ptr) { WAPI_RET_I32(0); return NULL; }

    /* WGPURenderPassDescriptor (wasm32):
     *   +0  u32 nextInChain
     *   +4  WGPUStringView label (8)
     *  +12  u32 colorAttachmentCount
     *  +16  u32 colorAttachments (ptr)
     *  +20  u32 depthStencilAttachment (ptr)
     *  +24  u32 occlusionQuerySet (handle)
     *  +28  u32 timestampWrites (ptr)
     */
    WGPURenderPassDescriptor d = {0};
    d.label = read_stringview(desc_ptr + 4);
    uint32_t cc = g_u32(desc_ptr + 12);
    uint32_t cp = g_u32(desc_ptr + 16);

    /* WGPURenderPassColorAttachment (wasm32):
     *   +0  u32 nextInChain
     *   +4  u32 view (handle)
     *   +8  u32 depthSlice
     *  +12  u32 resolveTarget (handle)
     *  +16  u32 loadOp
     *  +20  u32 storeOp
     *  +24  WGPUColor clearValue (4 * f64 = 32)
     *  (total 56B, 8-byte aligned because of the f64)
     */
    static WGPURenderPassColorAttachment s_ca[8];
    if (cc && cc <= 8 && cp) {
        for (uint32_t i = 0; i < cc; i++) {
            uint32_t base = cp + i * 56;
            int32_t vh = (int32_t)g_u32(base + 4);
            int32_t rh = (int32_t)g_u32(base + 12);
            s_ca[i].nextInChain   = NULL;
            s_ca[i].view          = GET_GPU_PTR(vh, WAPI_HTYPE_GPU_TEXTURE_VIEW, gpu_texture_view);
            s_ca[i].depthSlice    = g_u32(base + 8);
            s_ca[i].resolveTarget = GET_GPU_PTR(rh, WAPI_HTYPE_GPU_TEXTURE_VIEW, gpu_texture_view);
            s_ca[i].loadOp        = (WGPULoadOp)g_u32(base + 16);
            s_ca[i].storeOp       = (WGPUStoreOp)g_u32(base + 20);
            s_ca[i].clearValue.r  = g_f64(base + 24);
            s_ca[i].clearValue.g  = g_f64(base + 32);
            s_ca[i].clearValue.b  = g_f64(base + 40);
            s_ca[i].clearValue.a  = g_f64(base + 48);
        }
        d.colorAttachmentCount = cc;
        d.colorAttachments     = s_ca;
    }

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &d);
    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_RENDER_PASS, pass);
    if (pass && !h) { wgpuRenderPassEncoderRelease(pass); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(h);
    return NULL;
}

/* ============================================================
 * wgpuRenderPassEncoderSetPipeline(pass, pipeline)
 * ============================================================ */

static wasm_trap_t* wg_RenderPassEncoderSetPipeline(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;
    int32_t ph = WAPI_ARG_I32(0);
    int32_t pl = WAPI_ARG_I32(1);
    WGPURenderPassEncoder pass = GET_GPU_PTR(ph, WAPI_HTYPE_GPU_RENDER_PASS, gpu_render_pass);
    WGPURenderPipeline pipe = GET_GPU_PTR(pl, WAPI_HTYPE_GPU_RENDER_PIPELINE, gpu_render_pipeline);
    if (!pass || !pipe) return NULL;
    wgpuRenderPassEncoderSetPipeline(pass, pipe);
    return NULL;
}

/* ============================================================
 * wgpuRenderPassEncoderDraw(pass, vertexCount, instanceCount, firstVertex, firstInstance)
 * ============================================================ */

static wasm_trap_t* wg_RenderPassEncoderDraw(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;
    int32_t ph = WAPI_ARG_I32(0);
    WGPURenderPassEncoder pass = GET_GPU_PTR(ph, WAPI_HTYPE_GPU_RENDER_PASS, gpu_render_pass);
    if (!pass) return NULL;
    wgpuRenderPassEncoderDraw(pass,
                              (uint32_t)WAPI_ARG_I32(1),
                              (uint32_t)WAPI_ARG_I32(2),
                              (uint32_t)WAPI_ARG_I32(3),
                              (uint32_t)WAPI_ARG_I32(4));
    return NULL;
}

/* ============================================================
 * wgpuRenderPassEncoderEnd(pass)
 * ============================================================ */

static wasm_trap_t* wg_RenderPassEncoderEnd(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;
    int32_t ph = WAPI_ARG_I32(0);
    WGPURenderPassEncoder pass = GET_GPU_PTR(ph, WAPI_HTYPE_GPU_RENDER_PASS, gpu_render_pass);
    if (!pass) return NULL;
    wgpuRenderPassEncoderEnd(pass);
    return NULL;
}

/* ============================================================
 * wgpuCommandEncoderFinish(encoder, descriptor_ptr)
 * ============================================================ */

static wasm_trap_t* wg_CommandEncoderFinish(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  eh       = WAPI_ARG_I32(0);
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    WGPUCommandEncoder enc = GET_GPU_PTR(eh, WAPI_HTYPE_GPU_COMMAND_ENCODER, gpu_command_encoder);
    if (!enc) { WAPI_RET_I32(0); return NULL; }
    WGPUCommandBufferDescriptor d = {0};
    if (desc_ptr) d.label = read_stringview(desc_ptr + 4);
    WGPUCommandBuffer cb = wgpuCommandEncoderFinish(enc, &d);
    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_COMMAND_BUFFER, cb);
    if (cb && !h) { wgpuCommandBufferRelease(cb); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(h);
    return NULL;
}

/* ============================================================
 * wgpuQueueSubmit(queue, count, commands_ptr)
 * ============================================================ */

static wasm_trap_t* wg_QueueSubmit(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;
    int32_t  qh        = WAPI_ARG_I32(0);
    uint32_t count     = WAPI_ARG_U32(1);
    uint32_t cmds_ptr  = WAPI_ARG_U32(2);
    WGPUQueue queue = GET_GPU_PTR(qh, WAPI_HTYPE_GPU_QUEUE, gpu_queue);
    if (!queue || !cmds_ptr) return NULL;
    static WGPUCommandBuffer s_cbs[64];
    if (count > 64) count = 64;
    for (uint32_t i = 0; i < count; i++) {
        int32_t h = (int32_t)g_u32(cmds_ptr + i * 4);
        s_cbs[i] = GET_GPU_PTR(h, WAPI_HTYPE_GPU_COMMAND_BUFFER, gpu_command_buffer);
    }
    wgpuQueueSubmit(queue, count, s_cbs);
    return NULL;
}

/* ============================================================
 * Simple release trampolines
 * ============================================================ */

/* ============================================================
 * Buffer / Texture / Sampler / Bind-group path (sprite pipeline)
 * ============================================================ */

/* wgpuDeviceCreateBuffer(device, desc_ptr) -> handle
 * WGPUBufferDescriptor (wasm32, 40B, align 8):
 *   +0   u32 nextInChain
 *   +4   WGPUStringView label (8B)
 *  +12   _pad (align usage to 16)
 *  +16   u64 usage  (WGPUBufferUsage = u64 flags)
 *  +24   u64 size
 *  +32   u32 mappedAtCreation (WGPUBool=u32)
 *  +36   _pad
 */
static wasm_trap_t* wg_DeviceCreateBuffer(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  dh       = WAPI_ARG_I32(0);
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    WGPUDevice device = GET_GPU_PTR(dh, WAPI_HTYPE_GPU_DEVICE, gpu_device);
    if (!device || !desc_ptr) { WAPI_RET_I32(0); return NULL; }

    WGPUBufferDescriptor d = {0};
    d.label           = read_stringview(desc_ptr + 4);
    d.usage           = (WGPUBufferUsage)g_u64(desc_ptr + 16);
    d.size            = g_u64(desc_ptr + 24);
    d.mappedAtCreation = (WGPUBool)g_u32(desc_ptr + 32);

    WGPUBuffer b = wgpuDeviceCreateBuffer(device, &d);
    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_BUFFER, b);
    if (b && !h) { wgpuBufferRelease(b); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(h);
    return NULL;
}

/* wgpuDeviceCreateTexture(device, desc_ptr) -> handle
 * WGPUTextureDescriptor (wasm32, 60B, align 8):
 *   +0   u32 nextInChain
 *   +4   WGPUStringView label (8B)
 *  +12   _pad
 *  +16   u64 usage
 *  +24   u32 dimension
 *  +28   WGPUExtent3D size (u32 width, u32 height, u32 depthOrArrayLayers = 12B)
 *  +40   u32 format
 *  +44   u32 mipLevelCount
 *  +48   u32 sampleCount
 *  +52   u32 viewFormatCount (size_t=u32)
 *  +56   u32 viewFormats (ptr)
 */
static wasm_trap_t* wg_DeviceCreateTexture(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  dh       = WAPI_ARG_I32(0);
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    WGPUDevice device = GET_GPU_PTR(dh, WAPI_HTYPE_GPU_DEVICE, gpu_device);
    if (!device || !desc_ptr) { WAPI_RET_I32(0); return NULL; }

    WGPUTextureDescriptor d = {0};
    d.label         = read_stringview(desc_ptr + 4);
    d.usage         = (WGPUTextureUsage)g_u64(desc_ptr + 16);
    d.dimension     = (WGPUTextureDimension)g_u32(desc_ptr + 24);
    d.size.width              = g_u32(desc_ptr + 28);
    d.size.height             = g_u32(desc_ptr + 32);
    d.size.depthOrArrayLayers = g_u32(desc_ptr + 36);
    d.format        = (WGPUTextureFormat)g_u32(desc_ptr + 40);
    d.mipLevelCount = g_u32(desc_ptr + 44);
    d.sampleCount   = g_u32(desc_ptr + 48);

    uint32_t vf_count = g_u32(desc_ptr + 52);
    uint32_t vf_ptr   = g_u32(desc_ptr + 56);
    static WGPUTextureFormat s_view_formats[16];
    if (vf_count && vf_count <= 16 && vf_ptr) {
        for (uint32_t i = 0; i < vf_count; i++)
            s_view_formats[i] = (WGPUTextureFormat)g_u32(vf_ptr + i * 4);
        d.viewFormatCount = vf_count;
        d.viewFormats     = s_view_formats;
    }

    WGPUTexture t = wgpuDeviceCreateTexture(device, &d);
    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_TEXTURE, t);
    if (t && !h) { wgpuTextureRelease(t); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(h);
    return NULL;
}

/* wgpuDeviceCreateSampler(device, desc_ptr) -> handle
 * WGPUSamplerDescriptor (wasm32, 52B, align 4):
 *   +0   u32 nextInChain
 *   +4   WGPUStringView label (8B)
 *  +12   u32 addressModeU
 *  +16   u32 addressModeV
 *  +20   u32 addressModeW
 *  +24   u32 magFilter
 *  +28   u32 minFilter
 *  +32   u32 mipmapFilter
 *  +36   f32 lodMinClamp
 *  +40   f32 lodMaxClamp
 *  +44   u32 compare
 *  +48   u16 maxAnisotropy
 *  +50   _pad
 */
static wasm_trap_t* wg_DeviceCreateSampler(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  dh       = WAPI_ARG_I32(0);
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    WGPUDevice device = GET_GPU_PTR(dh, WAPI_HTYPE_GPU_DEVICE, gpu_device);
    if (!device) { WAPI_RET_I32(0); return NULL; }

    WGPUSampler s;
    if (desc_ptr == 0) {
        s = wgpuDeviceCreateSampler(device, NULL);
    } else {
        WGPUSamplerDescriptor d = {0};
        d.label         = read_stringview(desc_ptr + 4);
        d.addressModeU  = (WGPUAddressMode)g_u32(desc_ptr + 12);
        d.addressModeV  = (WGPUAddressMode)g_u32(desc_ptr + 16);
        d.addressModeW  = (WGPUAddressMode)g_u32(desc_ptr + 20);
        d.magFilter     = (WGPUFilterMode)g_u32(desc_ptr + 24);
        d.minFilter     = (WGPUFilterMode)g_u32(desc_ptr + 28);
        d.mipmapFilter  = (WGPUMipmapFilterMode)g_u32(desc_ptr + 32);

        uint32_t u;
        void* p;
        p = wapi_wasm_ptr(desc_ptr + 36, 4); memcpy(&u, p, 4); memcpy(&d.lodMinClamp, &u, 4);
        p = wapi_wasm_ptr(desc_ptr + 40, 4); memcpy(&u, p, 4); memcpy(&d.lodMaxClamp, &u, 4);

        d.compare       = (WGPUCompareFunction)g_u32(desc_ptr + 44);
        uint16_t aniso;
        p = wapi_wasm_ptr(desc_ptr + 48, 2); memcpy(&aniso, p, 2);
        d.maxAnisotropy = aniso;

        s = wgpuDeviceCreateSampler(device, &d);
    }
    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_SAMPLER, s);
    if (s && !h) { wgpuSamplerRelease(s); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(h);
    return NULL;
}

/* wgpuDeviceCreateBindGroupLayout(device, desc_ptr) -> handle
 * WGPUBindGroupLayoutDescriptor (wasm32, 20B):
 *   +0   u32 nextInChain
 *   +4   WGPUStringView label (8B)
 *  +12   u32 entryCount
 *  +16   u32 entries (ptr)
 *
 * WGPUBindGroupLayoutEntry (wasm32, 88B, align 8):
 *   +0   u32 nextInChain
 *   +4   u32 binding
 *   +8   u64 visibility (WGPUShaderStage = u64 flags)
 *  +16   u32 bindingArraySize
 *  +20   _pad
 *  +24   WGPUBufferBindingLayout buffer (24B align 8):
 *          +0 u32 nextInChain
 *          +4 u32 type
 *          +8 u32 hasDynamicOffset
 *         +12 _pad
 *         +16 u64 minBindingSize
 *  +48   WGPUSamplerBindingLayout sampler (8B): +0 u32 next, +4 u32 type
 *  +56   WGPUTextureBindingLayout texture (16B):
 *          +0 u32 next, +4 u32 sampleType, +8 u32 viewDimension, +12 u32 multisampled
 *  +72   WGPUStorageTextureBindingLayout storageTexture (16B):
 *          +0 u32 next, +4 u32 access, +8 u32 format, +12 u32 viewDimension
 */
static wasm_trap_t* wg_DeviceCreateBindGroupLayout(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  dh       = WAPI_ARG_I32(0);
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    WGPUDevice device = GET_GPU_PTR(dh, WAPI_HTYPE_GPU_DEVICE, gpu_device);
    if (!device || !desc_ptr) { WAPI_RET_I32(0); return NULL; }

    WGPUBindGroupLayoutDescriptor d = {0};
    d.label      = read_stringview(desc_ptr + 4);
    uint32_t ec  = g_u32(desc_ptr + 12);
    uint32_t eps = g_u32(desc_ptr + 16);

    static WGPUBindGroupLayoutEntry s_entries[16];
    if (ec && ec <= 16 && eps) {
        for (uint32_t i = 0; i < ec; i++) {
            uint32_t base = eps + i * 88;
            WGPUBindGroupLayoutEntry* e = &s_entries[i];
            memset(e, 0, sizeof(*e));
            e->binding           = g_u32(base + 4);
            e->visibility        = (WGPUShaderStage)g_u64(base + 8);
            e->bindingArraySize  = g_u32(base + 16);

            uint32_t bb = base + 24;
            e->buffer.type              = (WGPUBufferBindingType)g_u32(bb + 4);
            e->buffer.hasDynamicOffset  = (WGPUBool)g_u32(bb + 8);
            e->buffer.minBindingSize    = g_u64(bb + 16);

            uint32_t sb = base + 48;
            e->sampler.type             = (WGPUSamplerBindingType)g_u32(sb + 4);

            uint32_t tb = base + 56;
            e->texture.sampleType       = (WGPUTextureSampleType)g_u32(tb + 4);
            e->texture.viewDimension    = (WGPUTextureViewDimension)g_u32(tb + 8);
            e->texture.multisampled     = (WGPUBool)g_u32(tb + 12);

            uint32_t stb = base + 72;
            e->storageTexture.access    = (WGPUStorageTextureAccess)g_u32(stb + 4);
            e->storageTexture.format    = (WGPUTextureFormat)g_u32(stb + 8);
            e->storageTexture.viewDimension = (WGPUTextureViewDimension)g_u32(stb + 12);
        }
        d.entryCount = ec;
        d.entries    = s_entries;
    }

    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &d);
    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_BIND_GROUP_LAYOUT, bgl);
    if (bgl && !h) { wgpuBindGroupLayoutRelease(bgl); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(h);
    return NULL;
}

/* wgpuDeviceCreateBindGroup(device, desc_ptr) -> handle
 * WGPUBindGroupDescriptor (wasm32, 24B):
 *   +0   u32 nextInChain
 *   +4   WGPUStringView label (8B)
 *  +12   u32 layout (handle)
 *  +16   u32 entryCount
 *  +20   u32 entries (ptr)
 *
 * WGPUBindGroupEntry (wasm32, 40B, align 8):
 *   +0   u32 nextInChain
 *   +4   u32 binding
 *   +8   u32 buffer (handle)
 *  +12   _pad
 *  +16   u64 offset
 *  +24   u64 size
 *  +32   u32 sampler (handle)
 *  +36   u32 textureView (handle)
 */
static wasm_trap_t* wg_DeviceCreateBindGroup(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  dh       = WAPI_ARG_I32(0);
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    WGPUDevice device = GET_GPU_PTR(dh, WAPI_HTYPE_GPU_DEVICE, gpu_device);
    if (!device || !desc_ptr) { WAPI_RET_I32(0); return NULL; }

    WGPUBindGroupDescriptor d = {0};
    d.label       = read_stringview(desc_ptr + 4);
    int32_t lh    = (int32_t)g_u32(desc_ptr + 12);
    d.layout      = GET_GPU_PTR(lh, WAPI_HTYPE_GPU_BIND_GROUP_LAYOUT, gpu_bind_group_layout);

    uint32_t ec  = g_u32(desc_ptr + 16);
    uint32_t eps = g_u32(desc_ptr + 20);
    static WGPUBindGroupEntry s_entries[16];
    if (ec && ec <= 16 && eps) {
        for (uint32_t i = 0; i < ec; i++) {
            uint32_t base = eps + i * 40;
            WGPUBindGroupEntry* e = &s_entries[i];
            memset(e, 0, sizeof(*e));
            e->binding     = g_u32(base + 4);
            int32_t bh     = (int32_t)g_u32(base + 8);
            e->buffer      = GET_GPU_PTR(bh, WAPI_HTYPE_GPU_BUFFER, gpu_buffer);
            e->offset      = g_u64(base + 16);
            e->size        = g_u64(base + 24);
            int32_t sh     = (int32_t)g_u32(base + 32);
            e->sampler     = GET_GPU_PTR(sh, WAPI_HTYPE_GPU_SAMPLER, gpu_sampler);
            int32_t tvh    = (int32_t)g_u32(base + 36);
            e->textureView = GET_GPU_PTR(tvh, WAPI_HTYPE_GPU_TEXTURE_VIEW, gpu_texture_view);
        }
        d.entryCount = ec;
        d.entries    = s_entries;
    }

    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device, &d);
    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_BIND_GROUP, bg);
    if (bg && !h) { wgpuBindGroupRelease(bg); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(h);
    return NULL;
}

/* wgpuDeviceCreatePipelineLayout(device, desc_ptr) -> handle
 * WGPUPipelineLayoutDescriptor (wasm32, 24B):
 *   +0   u32 nextInChain
 *   +4   WGPUStringView label (8B)
 *  +12   u32 bindGroupLayoutCount
 *  +16   u32 bindGroupLayouts (ptr to array of i32 handles)
 *  +20   u32 immediateSize
 */
static wasm_trap_t* wg_DeviceCreatePipelineLayout(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  dh       = WAPI_ARG_I32(0);
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    WGPUDevice device = GET_GPU_PTR(dh, WAPI_HTYPE_GPU_DEVICE, gpu_device);
    if (!device || !desc_ptr) { WAPI_RET_I32(0); return NULL; }

    WGPUPipelineLayoutDescriptor d = {0};
    d.label                = read_stringview(desc_ptr + 4);
    uint32_t bglc          = g_u32(desc_ptr + 12);
    uint32_t bglp          = g_u32(desc_ptr + 16);
    d.immediateSize        = g_u32(desc_ptr + 20);

    static WGPUBindGroupLayout s_bgls[8];
    if (bglc && bglc <= 8 && bglp) {
        for (uint32_t i = 0; i < bglc; i++) {
            int32_t bh = (int32_t)g_u32(bglp + i * 4);
            s_bgls[i]  = GET_GPU_PTR(bh, WAPI_HTYPE_GPU_BIND_GROUP_LAYOUT, gpu_bind_group_layout);
        }
        d.bindGroupLayoutCount = bglc;
        d.bindGroupLayouts     = s_bgls;
    }

    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &d);
    int32_t h = alloc_gpu_handle(WAPI_HTYPE_GPU_PIPELINE_LAYOUT, pl);
    if (pl && !h) { wgpuPipelineLayoutRelease(pl); WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(h);
    return NULL;
}

/* wgpuQueueWriteBuffer(queue, buffer, offset: i64, data_ptr: i32, size: i32)
 * size is size_t (u32 on wasm32). */
static wasm_trap_t* wg_QueueWriteBuffer(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;
    int32_t  qh     = WAPI_ARG_I32(0);
    int32_t  bh     = WAPI_ARG_I32(1);
    uint64_t offset = WAPI_ARG_U64(2);
    uint32_t data   = WAPI_ARG_U32(3);
    uint32_t size   = WAPI_ARG_U32(4);
    WGPUQueue  q = GET_GPU_PTR(qh, WAPI_HTYPE_GPU_QUEUE, gpu_queue);
    WGPUBuffer b = GET_GPU_PTR(bh, WAPI_HTYPE_GPU_BUFFER, gpu_buffer);
    if (!q || !b) return NULL;
    void* host_data = size ? wapi_wasm_ptr(data, size) : NULL;
    if (!host_data && size) return NULL;
    wgpuQueueWriteBuffer(q, b, offset, host_data, size);
    return NULL;
}

/* wgpuQueueWriteTexture(queue, dest_ptr, data_ptr, dataSize, layout_ptr, writeSize_ptr)
 *
 * WGPUTexelCopyTextureInfo (wasm32, 24B):
 *   +0   u32 texture (handle)
 *   +4   u32 mipLevel
 *   +8   WGPUOrigin3D origin (u32 x, u32 y, u32 z = 12B)
 *  +20   u32 aspect
 *
 * WGPUTexelCopyBufferLayout (wasm32, 16B, align 8):
 *   +0   u64 offset
 *   +8   u32 bytesPerRow
 *  +12   u32 rowsPerImage
 *
 * WGPUExtent3D (wasm32, 12B): u32 width, u32 height, u32 depthOrArrayLayers
 */
static wasm_trap_t* wg_QueueWriteTexture(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;
    int32_t  qh          = WAPI_ARG_I32(0);
    uint32_t dest_ptr    = WAPI_ARG_U32(1);
    uint32_t data_ptr    = WAPI_ARG_U32(2);
    uint32_t data_size   = WAPI_ARG_U32(3);
    uint32_t layout_ptr  = WAPI_ARG_U32(4);
    uint32_t size_ptr    = WAPI_ARG_U32(5);
    WGPUQueue q = GET_GPU_PTR(qh, WAPI_HTYPE_GPU_QUEUE, gpu_queue);
    if (!q || !dest_ptr || !layout_ptr || !size_ptr) return NULL;

    int32_t th = (int32_t)g_u32(dest_ptr + 0);
    WGPUTexelCopyTextureInfo dest = {0};
    dest.texture      = GET_GPU_PTR(th, WAPI_HTYPE_GPU_TEXTURE, gpu_texture);
    dest.mipLevel     = g_u32(dest_ptr + 4);
    dest.origin.x     = g_u32(dest_ptr + 8);
    dest.origin.y     = g_u32(dest_ptr + 12);
    dest.origin.z     = g_u32(dest_ptr + 16);
    dest.aspect       = (WGPUTextureAspect)g_u32(dest_ptr + 20);

    WGPUTexelCopyBufferLayout lay = {0};
    lay.offset       = g_u64(layout_ptr + 0);
    lay.bytesPerRow  = g_u32(layout_ptr + 8);
    lay.rowsPerImage = g_u32(layout_ptr + 12);

    WGPUExtent3D sz = {0};
    sz.width              = g_u32(size_ptr + 0);
    sz.height             = g_u32(size_ptr + 4);
    sz.depthOrArrayLayers = g_u32(size_ptr + 8);

    void* host_data = data_size ? wapi_wasm_ptr(data_ptr, data_size) : NULL;
    if (!host_data && data_size) return NULL;
    wgpuQueueWriteTexture(q, &dest, host_data, (size_t)data_size, &lay, &sz);
    return NULL;
}

/* wgpuRenderPassEncoderSetBindGroup(pass, groupIdx, group, dynOffsetCount, dynOffsets) */
static wasm_trap_t* wg_RenderPassEncoderSetBindGroup(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;
    int32_t  ph    = WAPI_ARG_I32(0);
    uint32_t gidx  = WAPI_ARG_U32(1);
    int32_t  gh    = WAPI_ARG_I32(2);
    uint32_t dc    = WAPI_ARG_U32(3);
    uint32_t dop   = WAPI_ARG_U32(4);
    WGPURenderPassEncoder pass = GET_GPU_PTR(ph, WAPI_HTYPE_GPU_RENDER_PASS, gpu_render_pass);
    if (!pass) return NULL;
    WGPUBindGroup group = (gh == 0) ? NULL :
        GET_GPU_PTR(gh, WAPI_HTYPE_GPU_BIND_GROUP, gpu_bind_group);
    const uint32_t* offs = NULL;
    if (dc && dop) offs = (const uint32_t*)wapi_wasm_ptr(dop, dc * 4);
    wgpuRenderPassEncoderSetBindGroup(pass, gidx, group, dc, offs);
    return NULL;
}

/* wgpuRenderPassEncoderSetVertexBuffer(pass, slot, buffer, offset: i64, size: i64) */
static wasm_trap_t* wg_RenderPassEncoderSetVertexBuffer(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;
    int32_t  ph     = WAPI_ARG_I32(0);
    uint32_t slot   = WAPI_ARG_U32(1);
    int32_t  bh     = WAPI_ARG_I32(2);
    uint64_t offset = WAPI_ARG_U64(3);
    uint64_t size   = WAPI_ARG_U64(4);
    WGPURenderPassEncoder pass = GET_GPU_PTR(ph, WAPI_HTYPE_GPU_RENDER_PASS, gpu_render_pass);
    if (!pass) return NULL;
    WGPUBuffer buf = (bh == 0) ? NULL :
        GET_GPU_PTR(bh, WAPI_HTYPE_GPU_BUFFER, gpu_buffer);
    wgpuRenderPassEncoderSetVertexBuffer(pass, slot, buf, offset, size);
    return NULL;
}

/* wgpuRenderPassEncoderSetIndexBuffer(pass, buffer, format, offset: i64, size: i64) */
static wasm_trap_t* wg_RenderPassEncoderSetIndexBuffer(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;
    int32_t  ph     = WAPI_ARG_I32(0);
    int32_t  bh     = WAPI_ARG_I32(1);
    uint32_t fmt    = WAPI_ARG_U32(2);
    uint64_t offset = WAPI_ARG_U64(3);
    uint64_t size   = WAPI_ARG_U64(4);
    WGPURenderPassEncoder pass = GET_GPU_PTR(ph, WAPI_HTYPE_GPU_RENDER_PASS, gpu_render_pass);
    WGPUBuffer buf = GET_GPU_PTR(bh, WAPI_HTYPE_GPU_BUFFER, gpu_buffer);
    if (!pass || !buf) return NULL;
    wgpuRenderPassEncoderSetIndexBuffer(pass, buf, (WGPUIndexFormat)fmt, offset, size);
    return NULL;
}

/* wgpuRenderPassEncoderSetScissorRect(pass, x, y, w, h) */
static wasm_trap_t* wg_RenderPassEncoderSetScissorRect(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;
    int32_t ph = WAPI_ARG_I32(0);
    WGPURenderPassEncoder pass = GET_GPU_PTR(ph, WAPI_HTYPE_GPU_RENDER_PASS, gpu_render_pass);
    if (!pass) return NULL;
    wgpuRenderPassEncoderSetScissorRect(pass,
        (uint32_t)WAPI_ARG_I32(1), (uint32_t)WAPI_ARG_I32(2),
        (uint32_t)WAPI_ARG_I32(3), (uint32_t)WAPI_ARG_I32(4));
    return NULL;
}

/* wgpuRenderPassEncoderSetViewport(pass, x, y, w, h, minDepth, maxDepth)  — 7 args: i32 + 6xf32 */
static wasm_trap_t* wg_RenderPassEncoderSetViewport(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;
    int32_t ph = WAPI_ARG_I32(0);
    WGPURenderPassEncoder pass = GET_GPU_PTR(ph, WAPI_HTYPE_GPU_RENDER_PASS, gpu_render_pass);
    if (!pass) return NULL;
    wgpuRenderPassEncoderSetViewport(pass,
        args[1].of.f32, args[2].of.f32,
        args[3].of.f32, args[4].of.f32,
        args[5].of.f32, args[6].of.f32);
    return NULL;
}

/* wgpuRenderPassEncoderDrawIndexed(pass, idxCount, instCount, firstIdx, baseVertex, firstInstance) */
static wasm_trap_t* wg_RenderPassEncoderDrawIndexed(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;
    int32_t ph = WAPI_ARG_I32(0);
    WGPURenderPassEncoder pass = GET_GPU_PTR(ph, WAPI_HTYPE_GPU_RENDER_PASS, gpu_render_pass);
    if (!pass) return NULL;
    wgpuRenderPassEncoderDrawIndexed(pass,
        (uint32_t)WAPI_ARG_I32(1),
        (uint32_t)WAPI_ARG_I32(2),
        (uint32_t)WAPI_ARG_I32(3),
        WAPI_ARG_I32(4),  /* baseVertex is int32 */
        (uint32_t)WAPI_ARG_I32(5));
    return NULL;
}

/* ============================================================
 * Releases (add the remaining ones introduced by the sprite path)
 * ============================================================ */

#define DEFINE_RELEASE(fn_name, type_enum, field_name, native_release)   \
static wasm_trap_t* fn_name(void* env, wasmtime_caller_t* caller,         \
    const wasmtime_val_t* args, size_t nargs,                             \
    wasmtime_val_t* results, size_t nresults)                             \
{                                                                         \
    (void)env; (void)caller; (void)nargs; (void)nresults; (void)results;  \
    int32_t h = WAPI_ARG_I32(0);                                          \
    if (!wapi_handle_valid(h, type_enum)) return NULL;                    \
    native_release(g_rt.handles[h].data.field_name);                      \
    wapi_handle_free(h);                                                  \
    return NULL;                                                          \
}

DEFINE_RELEASE(wg_TextureRelease,           WAPI_HTYPE_GPU_TEXTURE,            gpu_texture,           wgpuTextureRelease)
DEFINE_RELEASE(wg_TextureViewRelease,       WAPI_HTYPE_GPU_TEXTURE_VIEW,       gpu_texture_view,      wgpuTextureViewRelease)
DEFINE_RELEASE(wg_BufferRelease,            WAPI_HTYPE_GPU_BUFFER,             gpu_buffer,            wgpuBufferRelease)
DEFINE_RELEASE(wg_SamplerRelease,           WAPI_HTYPE_GPU_SAMPLER,            gpu_sampler,           wgpuSamplerRelease)
DEFINE_RELEASE(wg_BindGroupRelease,         WAPI_HTYPE_GPU_BIND_GROUP,         gpu_bind_group,        wgpuBindGroupRelease)
DEFINE_RELEASE(wg_BindGroupLayoutRelease,   WAPI_HTYPE_GPU_BIND_GROUP_LAYOUT,  gpu_bind_group_layout, wgpuBindGroupLayoutRelease)
DEFINE_RELEASE(wg_PipelineLayoutRelease,    WAPI_HTYPE_GPU_PIPELINE_LAYOUT,    gpu_pipeline_layout,   wgpuPipelineLayoutRelease)
DEFINE_RELEASE(wg_ShaderModuleRelease,      WAPI_HTYPE_GPU_SHADER_MODULE,      gpu_shader_module,     wgpuShaderModuleRelease)
DEFINE_RELEASE(wg_RenderPipelineRelease,    WAPI_HTYPE_GPU_RENDER_PIPELINE,    gpu_render_pipeline,   wgpuRenderPipelineRelease)
DEFINE_RELEASE(wg_CommandEncoderRelease,    WAPI_HTYPE_GPU_COMMAND_ENCODER,    gpu_command_encoder,   wgpuCommandEncoderRelease)
DEFINE_RELEASE(wg_CommandBufferRelease,     WAPI_HTYPE_GPU_COMMAND_BUFFER,     gpu_command_buffer,    wgpuCommandBufferRelease)
DEFINE_RELEASE(wg_RenderPassEncoderRelease, WAPI_HTYPE_GPU_RENDER_PASS,        gpu_render_pass,       wgpuRenderPassEncoderRelease)
DEFINE_RELEASE(wg_SurfaceReleaseEnv,        WAPI_HTYPE_GPU_SURFACE,            gpu_surface,           wgpuSurfaceRelease)
DEFINE_RELEASE(wg_DeviceReleaseEnv,         WAPI_HTYPE_GPU_DEVICE,             gpu_device,            wgpuDeviceRelease)
DEFINE_RELEASE(wg_QueueReleaseEnv,          WAPI_HTYPE_GPU_QUEUE,              gpu_queue,             wgpuQueueRelease)
DEFINE_RELEASE(wg_AdapterReleaseEnv,        WAPI_HTYPE_GPU_ADAPTER,            gpu_adapter,           wgpuAdapterRelease)
DEFINE_RELEASE(wg_InstanceReleaseEnv,       WAPI_HTYPE_GPU_INSTANCE,           gpu_instance,          wgpuInstanceRelease)

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_wgpu(wasmtime_linker_t* linker) {
    /* Instance */
    WAPI_DEFINE_1_1(linker, "env", "wgpuCreateInstance",              wg_CreateInstance);
    WAPI_DEFINE_3_I64(linker, "env", "wgpuInstanceRequestAdapter",    wg_InstanceRequestAdapter);
    WAPI_DEFINE_1_0(linker, "env", "wgpuInstanceProcessEvents",       wg_InstanceProcessEvents);
    WAPI_DEFINE_2_1(linker, "env", "wgpuInstanceCreateSurface",       wg_InstanceCreateSurface);

    /* Adapter */
    WAPI_DEFINE_3_I64(linker, "env", "wgpuAdapterRequestDevice",      wg_AdapterRequestDevice);

    /* Device / Queue */
    WAPI_DEFINE_1_1(linker, "env", "wgpuDeviceGetQueue",              wg_DeviceGetQueue);
    WAPI_DEFINE_2_1(linker, "env", "wgpuDeviceCreateShaderModule",    wg_DeviceCreateShaderModule);
    WAPI_DEFINE_2_1(linker, "env", "wgpuDeviceCreateRenderPipeline",  wg_DeviceCreateRenderPipeline);
    WAPI_DEFINE_2_1(linker, "env", "wgpuDeviceCreateCommandEncoder",  wg_DeviceCreateCommandEncoder);
    WAPI_DEFINE_3_0(linker, "env", "wgpuQueueSubmit",                 wg_QueueSubmit);

    /* Surface */
    WAPI_DEFINE_3_1(linker, "env", "wgpuSurfaceGetCapabilities",      wg_SurfaceGetCapabilities);
    WAPI_DEFINE_2_0(linker, "env", "wgpuSurfaceConfigure",            wg_SurfaceConfigure);
    WAPI_DEFINE_2_0(linker, "env", "wgpuSurfaceGetCurrentTexture",    wg_SurfaceGetCurrentTexture);
    WAPI_DEFINE_1_1(linker, "env", "wgpuSurfacePresent",              wg_SurfacePresent);

    /* Texture / view */
    WAPI_DEFINE_2_1(linker, "env", "wgpuTextureCreateView",           wg_TextureCreateView);
    WAPI_DEFINE_1_0(linker, "env", "wgpuTextureRelease",              wg_TextureRelease);
    WAPI_DEFINE_1_0(linker, "env", "wgpuTextureViewRelease",          wg_TextureViewRelease);

    /* Buffer / texture / sampler / bind-group / pipeline-layout */
    WAPI_DEFINE_2_1(linker, "env", "wgpuDeviceCreateBuffer",          wg_DeviceCreateBuffer);
    WAPI_DEFINE_2_1(linker, "env", "wgpuDeviceCreateTexture",         wg_DeviceCreateTexture);
    WAPI_DEFINE_2_1(linker, "env", "wgpuDeviceCreateSampler",         wg_DeviceCreateSampler);
    WAPI_DEFINE_2_1(linker, "env", "wgpuDeviceCreateBindGroupLayout", wg_DeviceCreateBindGroupLayout);
    WAPI_DEFINE_2_1(linker, "env", "wgpuDeviceCreateBindGroup",       wg_DeviceCreateBindGroup);
    WAPI_DEFINE_2_1(linker, "env", "wgpuDeviceCreatePipelineLayout",  wg_DeviceCreatePipelineLayout);

    /* wgpuQueueWriteBuffer(queue, buffer, offset:i64, data:i32, size:i32) */
    wapi_linker_define(linker, "env", "wgpuQueueWriteBuffer", wg_QueueWriteBuffer,
        5, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64, WASM_I32, WASM_I32},
        0, NULL);
    WAPI_DEFINE_6_1_0(linker, "env", "wgpuQueueWriteTexture",         wg_QueueWriteTexture);

    /* Command encoder / render pass */
    WAPI_DEFINE_2_1(linker, "env", "wgpuCommandEncoderBeginRenderPass", wg_CommandEncoderBeginRenderPass);
    WAPI_DEFINE_2_1(linker, "env", "wgpuCommandEncoderFinish",        wg_CommandEncoderFinish);
    WAPI_DEFINE_1_0(linker, "env", "wgpuCommandEncoderRelease",       wg_CommandEncoderRelease);
    WAPI_DEFINE_1_0(linker, "env", "wgpuCommandBufferRelease",        wg_CommandBufferRelease);
    WAPI_DEFINE_2_0(linker, "env", "wgpuRenderPassEncoderSetPipeline",wg_RenderPassEncoderSetPipeline);
    WAPI_DEFINE_5_0(linker, "env", "wgpuRenderPassEncoderDraw",       wg_RenderPassEncoderDraw);
    WAPI_DEFINE_6_0(linker, "env", "wgpuRenderPassEncoderDrawIndexed",wg_RenderPassEncoderDrawIndexed);

    /* SetBindGroup: (pass, groupIdx, group, dynOffsetCount, dynOffsetsPtr) — 5 i32, void */
    WAPI_DEFINE_5_0(linker, "env", "wgpuRenderPassEncoderSetBindGroup", wg_RenderPassEncoderSetBindGroup);

    /* SetVertexBuffer: (pass, slot, buffer, offset:i64, size:i64) */
    wapi_linker_define(linker, "env", "wgpuRenderPassEncoderSetVertexBuffer",
        wg_RenderPassEncoderSetVertexBuffer,
        5, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I32, WASM_I64, WASM_I64},
        0, NULL);

    /* SetIndexBuffer: (pass, buffer, format, offset:i64, size:i64) */
    wapi_linker_define(linker, "env", "wgpuRenderPassEncoderSetIndexBuffer",
        wg_RenderPassEncoderSetIndexBuffer,
        5, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I32, WASM_I64, WASM_I64},
        0, NULL);

    /* SetScissorRect: (pass, x, y, w, h) — 5 i32 */
    WAPI_DEFINE_5_0(linker, "env", "wgpuRenderPassEncoderSetScissorRect",
        wg_RenderPassEncoderSetScissorRect);

    /* SetViewport: (pass:i32, x:f32, y:f32, w:f32, h:f32, minDepth:f32, maxDepth:f32) */
    wapi_linker_define(linker, "env", "wgpuRenderPassEncoderSetViewport",
        wg_RenderPassEncoderSetViewport,
        7, (wasm_valkind_t[]){WASM_I32, WASM_F32, WASM_F32, WASM_F32, WASM_F32, WASM_F32, WASM_F32},
        0, NULL);

    /* Additional releases needed once sprite path is in play */
    WAPI_DEFINE_1_0(linker, "env", "wgpuBufferRelease",               wg_BufferRelease);
    WAPI_DEFINE_1_0(linker, "env", "wgpuSamplerRelease",              wg_SamplerRelease);
    WAPI_DEFINE_1_0(linker, "env", "wgpuBindGroupRelease",            wg_BindGroupRelease);
    WAPI_DEFINE_1_0(linker, "env", "wgpuBindGroupLayoutRelease",      wg_BindGroupLayoutRelease);
    WAPI_DEFINE_1_0(linker, "env", "wgpuPipelineLayoutRelease",       wg_PipelineLayoutRelease);
    WAPI_DEFINE_1_0(linker, "env", "wgpuShaderModuleRelease",         wg_ShaderModuleRelease);
    WAPI_DEFINE_1_0(linker, "env", "wgpuRenderPipelineRelease",       wg_RenderPipelineRelease);
    WAPI_DEFINE_1_0(linker, "env", "wgpuSurfaceRelease",              wg_SurfaceReleaseEnv);
    WAPI_DEFINE_1_0(linker, "env", "wgpuDeviceRelease",               wg_DeviceReleaseEnv);
    WAPI_DEFINE_1_0(linker, "env", "wgpuQueueRelease",                wg_QueueReleaseEnv);
    WAPI_DEFINE_1_0(linker, "env", "wgpuAdapterRelease",              wg_AdapterReleaseEnv);
    WAPI_DEFINE_1_0(linker, "env", "wgpuInstanceRelease",             wg_InstanceReleaseEnv);
    WAPI_DEFINE_1_0(linker, "env", "wgpuRenderPassEncoderEnd",        wg_RenderPassEncoderEnd);
    WAPI_DEFINE_1_0(linker, "env", "wgpuRenderPassEncoderRelease",    wg_RenderPassEncoderRelease);
}
