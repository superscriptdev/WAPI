/**
 * WAPI - Hello Triangle
 *
 * Renders a colored triangle using WebGPU on a WAPI surface.
 *
 * WAPI contributes only the windowing (wapi_surface / wapi_window),
 * event loop (wapi_io), and the chained `wapi_gpu_surface_source_t`
 * that binds a WGPUSurface to a WAPI surface handle. Everything else
 * is standard webgpu.h.
 *
 * Compile:
 *   zig cc --target=wasm32-freestanding -O2 -nostdlib \
 *     -I../include -I<wgpu-native-include> \
 *     -c hello_triangle.c -o hello_triangle.o
 *   zig wasm-ld hello_triangle.o wapi_reactor.o \
 *     -o hello_triangle.wasm \
 *     --no-entry --export=wapi_main --export=wapi_frame \
 *     --export-memory --export-table --growable-table --allow-undefined
 */

#include <wapi/wapi.h>
#include <wapi/wapi_surface.h>
#include <wapi/wapi_window.h>
#include <wapi/wapi_input.h>
#include <wapi/wapi_gpu.h>
#include <webgpu/webgpu.h>

/* ============================================================
 * Application State
 * ============================================================ */

static const wapi_io_t* g_io;

static wapi_handle_t    g_surface;
static int32_t          g_width;
static int32_t          g_height;
static int              g_running;

static WGPUInstance      g_instance;
static WGPUAdapter       g_adapter;
static WGPUDevice        g_device;
static WGPUQueue         g_queue;
static WGPUSurface       g_wgpu_surface;
static WGPUTextureFormat g_surface_format;
static WGPUShaderModule  g_shader;
static WGPURenderPipeline g_pipeline;

/* ============================================================
 * WGSL Shader Source
 * ============================================================ */

static const char g_wgsl_source[] =
    "@vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {\n"
    "    var pos = array<vec2f, 3>(\n"
    "        vec2f( 0.0,  0.5),\n"
    "        vec2f(-0.5, -0.5),\n"
    "        vec2f( 0.5, -0.5)\n"
    "    );\n"
    "    return vec4f(pos[i], 0.0, 1.0);\n"
    "}\n"
    "@fragment fn fs() -> @location(0) vec4f {\n"
    "    return vec4f(1.0, 0.0, 0.0, 1.0);\n"
    "}\n";

/* ============================================================
 * Async request helpers
 * ============================================================ */

typedef struct { WGPUAdapter adapter; int done; WGPURequestAdapterStatus status; } adapter_req_t;
typedef struct { WGPUDevice  device;  int done; WGPURequestDeviceStatus  status; } device_req_t;

static void on_adapter(WGPURequestAdapterStatus status, WGPUAdapter a,
                       WGPUStringView msg, void* u1, void* u2) {
    (void)msg; (void)u2;
    adapter_req_t* r = (adapter_req_t*)u1;
    r->adapter = a;
    r->status  = status;
    r->done    = 1;
}

static void on_device(WGPURequestDeviceStatus status, WGPUDevice d,
                      WGPUStringView msg, void* u1, void* u2) {
    (void)msg; (void)u2;
    device_req_t* r = (device_req_t*)u1;
    r->device = d;
    r->status = status;
    r->done   = 1;
}

static void on_device_lost(WGPUDevice const* d, WGPUDeviceLostReason reason,
                           WGPUStringView msg, void* u1, void* u2) {
    (void)d; (void)reason; (void)msg; (void)u1; (void)u2;
}

static void on_uncaptured(WGPUDevice const* d, WGPUErrorType type,
                          WGPUStringView msg, void* u1, void* u2) {
    (void)d; (void)type; (void)msg; (void)u1; (void)u2;
}

/* ============================================================
 * Initialization
 * ============================================================ */

static wapi_result_t init(void) {
    wapi_result_t res;

    g_io = wapi_io_get();
    if (!g_io) return WAPI_ERR_UNKNOWN;

    if (!wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_GPU)) ||
        !wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_SURFACE)) ||
        !wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_WINDOW)) ||
        !wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_INPUT))) {
        return WAPI_ERR_NOTCAPABLE;
    }

    /* ---- 1. Window + WAPI surface ---- */
    wapi_window_desc_t window_cfg = {
        .chain        = { .next = 0, .sType = WAPI_STYPE_WINDOW_CONFIG },
        .title        = WAPI_STR("Hello Triangle"),
        .window_flags = WAPI_WINDOW_FLAG_RESIZABLE,
    };
    wapi_surface_desc_t surface_desc = {
        .nextInChain = (uint64_t)(uintptr_t)&window_cfg,
        .width       = 800,
        .height      = 600,
        .flags       = WAPI_SURFACE_FLAG_HIGH_DPI,
    };
    res = wapi_surface_create(&surface_desc, &g_surface);
    if (WAPI_FAILED(res)) return res;

    wapi_surface_get_size(g_surface, &g_width, &g_height);

    /* ---- 2. WebGPU instance / adapter / device ---- */
    WGPUInstanceDescriptor idesc = {0};
    g_instance = wgpuCreateInstance(&idesc);
    if (!g_instance) return WAPI_ERR_NOTCAPABLE;

    WGPURequestAdapterOptions aopts = {0};
    aopts.powerPreference = WGPUPowerPreference_HighPerformance;
    adapter_req_t areq = {0};
    WGPURequestAdapterCallbackInfo acb = {0};
    acb.mode      = WGPUCallbackMode_AllowProcessEvents;
    acb.callback  = on_adapter;
    acb.userdata1 = &areq;
    wgpuInstanceRequestAdapter(g_instance, &aopts, acb);
    while (!areq.done) wgpuInstanceProcessEvents(g_instance);
    if (areq.status != WGPURequestAdapterStatus_Success || !areq.adapter) {
        return WAPI_ERR_NOTCAPABLE;
    }
    g_adapter = areq.adapter;

    WGPUDeviceDescriptor ddesc = {0};
    ddesc.label.data   = "WAPI Device";
    ddesc.label.length = 11;
    ddesc.deviceLostCallbackInfo.mode     = WGPUCallbackMode_AllowProcessEvents;
    ddesc.deviceLostCallbackInfo.callback = on_device_lost;
    ddesc.uncapturedErrorCallbackInfo.callback = on_uncaptured;
    device_req_t dreq = {0};
    WGPURequestDeviceCallbackInfo dcb = {0};
    dcb.mode      = WGPUCallbackMode_AllowProcessEvents;
    dcb.callback  = on_device;
    dcb.userdata1 = &dreq;
    wgpuAdapterRequestDevice(g_adapter, &ddesc, dcb);
    while (!dreq.done) wgpuInstanceProcessEvents(g_instance);
    if (dreq.status != WGPURequestDeviceStatus_Success || !dreq.device) {
        return WAPI_ERR_NOTCAPABLE;
    }
    g_device = dreq.device;
    g_queue  = wgpuDeviceGetQueue(g_device);

    /* ---- 3. WGPUSurface over the WAPI surface ---- */
    wapi_gpu_surface_source_t src = {
        .chain   = { .next = 0, .sType = WAPI_STYPE_GPU_SURFACE_SOURCE_WAPI },
        .surface = g_surface,
    };
    WGPUSurfaceDescriptor sd = {0};
    sd.nextInChain  = (WGPUChainedStruct*)&src;
    sd.label.data   = "WAPI Surface";
    sd.label.length = 12;
    g_wgpu_surface = wgpuInstanceCreateSurface(g_instance, &sd);
    if (!g_wgpu_surface) return WAPI_ERR_UNKNOWN;

    WGPUSurfaceCapabilities caps = {0};
    wgpuSurfaceGetCapabilities(g_wgpu_surface, g_adapter, &caps);
    g_surface_format = (caps.formatCount > 0) ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;

    WGPUSurfaceConfiguration sc = {0};
    sc.device      = g_device;
    sc.format      = g_surface_format;
    sc.usage       = WGPUTextureUsage_RenderAttachment;
    sc.width       = (uint32_t)g_width;
    sc.height      = (uint32_t)g_height;
    sc.presentMode = WGPUPresentMode_Fifo;
    sc.alphaMode   = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfigure(g_wgpu_surface, &sc);

    /* ---- 4. Shader + pipeline ---- */
    WGPUShaderSourceWGSL wgsl = {0};
    wgsl.chain.sType  = WGPUSType_ShaderSourceWGSL;
    wgsl.code.data    = g_wgsl_source;
    wgsl.code.length  = sizeof(g_wgsl_source) - 1;
    WGPUShaderModuleDescriptor shader_desc = {0};
    shader_desc.nextInChain  = (WGPUChainedStruct*)&wgsl;
    shader_desc.label.data   = "triangle_shader";
    shader_desc.label.length = 15;
    g_shader = wgpuDeviceCreateShaderModule(g_device, &shader_desc);
    if (!g_shader) return WAPI_ERR_UNKNOWN;

    WGPUColorTargetState color_target = {0};
    color_target.format    = g_surface_format;
    color_target.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragment_state = {0};
    fragment_state.module           = g_shader;
    fragment_state.entryPoint.data  = "fs";
    fragment_state.entryPoint.length = 2;
    fragment_state.targetCount      = 1;
    fragment_state.targets          = &color_target;

    WGPURenderPipelineDescriptor pipeline_desc = {0};
    pipeline_desc.label.data   = "triangle_pipeline";
    pipeline_desc.label.length = 17;
    pipeline_desc.vertex.module          = g_shader;
    pipeline_desc.vertex.entryPoint.data = "vs";
    pipeline_desc.vertex.entryPoint.length = 2;
    pipeline_desc.primitive.topology   = WGPUPrimitiveTopology_TriangleList;
    pipeline_desc.primitive.frontFace  = WGPUFrontFace_CCW;
    pipeline_desc.primitive.cullMode   = WGPUCullMode_None;
    pipeline_desc.multisample.count    = 1;
    pipeline_desc.multisample.mask     = 0xFFFFFFFF;
    pipeline_desc.fragment             = &fragment_state;
    g_pipeline = wgpuDeviceCreateRenderPipeline(g_device, &pipeline_desc);
    if (!g_pipeline) return WAPI_ERR_UNKNOWN;

    g_running = 1;
    return WAPI_OK;
}

/* ============================================================
 * Event handling
 * ============================================================ */

static void process_events(void) {
    wapi_event_t event;
    while (g_io->poll(g_io->impl, &event)) {
        switch (event.type) {
            case WAPI_EVENT_WINDOW_CLOSE:
                g_running = 0;
                break;
            case WAPI_EVENT_SURFACE_RESIZED:
                g_width  = event.surface.data1;
                g_height = event.surface.data2;
                if (g_width > 0 && g_height > 0) {
                    WGPUSurfaceConfiguration sc = {0};
                    sc.device      = g_device;
                    sc.format      = g_surface_format;
                    sc.usage       = WGPUTextureUsage_RenderAttachment;
                    sc.width       = (uint32_t)g_width;
                    sc.height      = (uint32_t)g_height;
                    sc.presentMode = WGPUPresentMode_Fifo;
                    sc.alphaMode   = WGPUCompositeAlphaMode_Auto;
                    wgpuSurfaceConfigure(g_wgpu_surface, &sc);
                }
                break;
            case WAPI_EVENT_KEY_DOWN:
                if (event.key.scancode == WAPI_SCANCODE_ESCAPE) g_running = 0;
                break;
            default: break;
        }
    }
}

/* ============================================================
 * Render
 * ============================================================ */

static wapi_result_t render_frame(void) {
    WGPUSurfaceTexture st;
    wgpuSurfaceGetCurrentTexture(g_wgpu_surface, &st);
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        return WAPI_ERR_UNKNOWN;
    }
    WGPUTextureView view = wgpuTextureCreateView(st.texture, 0);
    if (!view) return WAPI_ERR_UNKNOWN;

    WGPUCommandEncoderDescriptor enc_desc = {0};
    enc_desc.label.data   = "frame_encoder";
    enc_desc.label.length = 13;
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, &enc_desc);

    WGPURenderPassColorAttachment color_attachment = {0};
    color_attachment.view          = view;
    color_attachment.depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED;
    color_attachment.loadOp        = WGPULoadOp_Clear;
    color_attachment.storeOp       = WGPUStoreOp_Store;
    color_attachment.clearValue.r  = 0.05;
    color_attachment.clearValue.g  = 0.05;
    color_attachment.clearValue.b  = 0.15;
    color_attachment.clearValue.a  = 1.0;

    WGPURenderPassDescriptor rp_desc = {0};
    rp_desc.label.data            = "main_pass";
    rp_desc.label.length          = 9;
    rp_desc.colorAttachmentCount  = 1;
    rp_desc.colorAttachments      = &color_attachment;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rp_desc);
    wgpuRenderPassEncoderSetPipeline(pass, g_pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cb_desc = {0};
    cb_desc.label.data   = "frame_commands";
    cb_desc.label.length = 14;
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cb_desc);
    wgpuQueueSubmit(g_queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(encoder);

    wgpuSurfacePresent(g_wgpu_surface);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(st.texture);
    return WAPI_OK;
}

/* ============================================================
 * Frame callback
 * ============================================================ */

WAPI_EXPORT(wapi_frame)
wapi_result_t wapi_frame(wapi_timestamp_t timestamp) {
    (void)timestamp;
    process_events();
    if (!g_running) return WAPI_ERR_CANCELED;
    render_frame();
    return WAPI_OK;
}

/* ============================================================
 * Entry point
 * ============================================================ */

WAPI_EXPORT(wapi_main)
wapi_result_t wapi_main(void) {
    return init();
}
