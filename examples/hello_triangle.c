/**
 * WAPI - Hello Triangle Example
 *
 * Renders a colored triangle using WebGPU via the WAPI platform.
 *
 * Compile with:
 *   zig cc --target=wasm32-freestanding -O2 -nostdlib \
 *     -I../include -c hello_triangle.c -o hello_triangle.o
 *   zig wasm-ld hello_triangle.o -o ../runtime/browser/hello_triangle.wasm \
 *     --no-entry --export=wapi_main --export=wapi_frame \
 *     --export-memory --export-table --growable-table --allow-undefined
 */

#include <wapi/wapi.h>
#include <wapi/wapi_surface.h>
#include <wapi/wapi_window.h>

/* Suppress webgpu.h function declarations -- wapi_gpu.h provides them via WAPI_IMPORT */
#define WGPU_SKIP_DECLARATIONS
#define WGPU_SKIP_PROCS
#include <wapi/webgpu.h>
#include <wapi/wapi_gpu.h>

/* ============================================================
 * Application State
 * ============================================================ */

static const wapi_io_t* g_io;
static wapi_handle_t g_surface;
static wapi_handle_t g_gpu_device;
static wapi_handle_t g_gpu_queue;
static int32_t     g_width;
static int32_t     g_height;
static int         g_running;

/* WebGPU objects (handles are i32 on wasm32) */
static WGPUShaderModule   g_shader;
static WGPURenderPipeline g_pipeline;
static wapi_gpu_texture_format_t g_surface_format;

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
 * Initialization
 * ============================================================ */

static wapi_result_t init(void) {
    wapi_result_t res;

    /* Obtain the I/O vtable */
    g_io = wapi_io_get();
    if (!g_io) return WAPI_ERR_UNKNOWN;

    /* Check required capabilities */
    if (!wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_GPU)) ||
        !wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_SURFACE)) ||
        !wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_WINDOW)) ||
        !wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_INPUT))) {
        return WAPI_ERR_NOTCAPABLE;
    }

    /* Create a windowed surface */
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

    /* Request a GPU device */
    wapi_gpu_device_desc_t gpu_desc = {
        .nextInChain             = 0,
        .power_preference        = WAPI_GPU_POWER_DEFAULT,
        .required_features_count = 0,
        .required_features       = 0,
    };
    res = wapi_gpu_request_device(&gpu_desc, &g_gpu_device);
    if (WAPI_FAILED(res)) return res;

    /* Get the device's queue */
    res = wapi_gpu_get_queue(g_gpu_device, &g_gpu_queue);
    if (WAPI_FAILED(res)) return res;

    /* Configure the surface */
    wapi_gpu_surface_preferred_format(g_surface, &g_surface_format);

    wapi_gpu_surface_desc_t surface_config = {
        .nextInChain  = 0,
        .surface      = g_surface,
        .device       = g_gpu_device,
        .format       = g_surface_format,
        .present_mode = WAPI_GPU_PRESENT_FIFO,
        .usage        = 0x10, /* WGPUTextureUsage_RenderAttachment */
    };
    res = wapi_gpu_configure_surface(&surface_config);
    if (WAPI_FAILED(res)) return res;

    /* Create shader module */
    WGPUShaderSourceWGSL wgsl_source = {
        .chain = { .next = (WGPUChainedStruct*)0, .sType = WGPUSType_ShaderSourceWGSL },
        .code  = { .data = g_wgsl_source, .length = sizeof(g_wgsl_source) - 1 },
    };
    WGPUShaderModuleDescriptor shader_desc = {
        .nextInChain = (WGPUChainedStruct*)&wgsl_source,
        .label       = { .data = "triangle_shader", .length = 15 },
    };
    g_shader = wgpuDeviceCreateShaderModule((WGPUDevice)g_gpu_device, &shader_desc);
    if (!g_shader) return WAPI_ERR_UNKNOWN;

    /* Create render pipeline */
    WGPUColorTargetState color_target = {
        .nextInChain = (WGPUChainedStruct*)0,
        .format      = (WGPUTextureFormat)g_surface_format,
        .blend       = (WGPUBlendState*)0,
        .writeMask   = 0xF, /* WGPUColorWriteMask_All */
    };
    WGPUFragmentState fragment_state = {
        .nextInChain   = (WGPUChainedStruct*)0,
        .module        = g_shader,
        .entryPoint    = { .data = "fs", .length = 2 },
        .constantCount = 0,
        .constants     = (WGPUConstantEntry*)0,
        .targetCount   = 1,
        .targets       = &color_target,
    };
    WGPURenderPipelineDescriptor pipeline_desc = {
        .nextInChain = (WGPUChainedStruct*)0,
        .label       = { .data = "triangle_pipeline", .length = 17 },
        .layout      = (WGPUPipelineLayout)0, /* auto layout */
        .vertex      = {
            .nextInChain   = (WGPUChainedStruct*)0,
            .module        = g_shader,
            .entryPoint    = { .data = "vs", .length = 2 },
            .constantCount = 0,
            .constants     = (WGPUConstantEntry*)0,
            .bufferCount   = 0,
            .buffers       = (WGPUVertexBufferLayout*)0,
        },
        .primitive = {
            .nextInChain    = (WGPUChainedStruct*)0,
            .topology       = WGPUPrimitiveTopology_TriangleList,
            .stripIndexFormat = WGPUIndexFormat_Undefined,
            .frontFace      = WGPUFrontFace_CCW,
            .cullMode        = WGPUCullMode_None,
        },
        .depthStencil = (WGPUDepthStencilState*)0,
        .multisample  = {
            .nextInChain            = (WGPUChainedStruct*)0,
            .count                  = 1,
            .mask                   = 0xFFFFFFFF,
            .alphaToCoverageEnabled = 0,
        },
        .fragment = &fragment_state,
    };
    g_pipeline = wgpuDeviceCreateRenderPipeline((WGPUDevice)g_gpu_device, &pipeline_desc);
    if (!g_pipeline) return WAPI_ERR_UNKNOWN;

    g_running = 1;
    return WAPI_OK;
}

/* ============================================================
 * Event Processing
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
                break;
            case WAPI_EVENT_KEY_DOWN:
                if (event.key.scancode == WAPI_SCANCODE_ESCAPE) {
                    g_running = 0;
                }
                break;
            default:
                break;
        }
    }
}

/* ============================================================
 * Rendering
 * ============================================================ */

static wapi_result_t render_frame(void) {
    wapi_result_t res;

    /* Get the current back-buffer texture */
    wapi_handle_t texture, texture_view;
    res = wapi_gpu_surface_get_current_texture(g_surface, &texture, &texture_view);
    if (WAPI_FAILED(res)) return res;

    /* Create command encoder */
    WGPUCommandEncoderDescriptor enc_desc = {
        .nextInChain = (WGPUChainedStruct*)0,
        .label       = { .data = "frame_encoder", .length = 13 },
    };
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(
        (WGPUDevice)g_gpu_device, &enc_desc);

    /* Begin render pass */
    WGPURenderPassColorAttachment color_attachment = {
        .nextInChain  = (WGPUChainedStruct*)0,
        .view         = (WGPUTextureView)texture_view,
        .depthSlice   = 0xFFFFFFFF, /* WGPU_DEPTH_SLICE_UNDEFINED */
        .resolveTarget = (WGPUTextureView)0,
        .loadOp       = WGPULoadOp_Clear,
        .storeOp      = WGPUStoreOp_Store,
        .clearValue   = { 0.05, 0.05, 0.15, 1.0 }, /* dark blue */
    };
    WGPURenderPassDescriptor rp_desc = {
        .nextInChain             = (WGPUChainedStruct*)0,
        .label                   = { .data = "main_pass", .length = 9 },
        .colorAttachmentCount    = 1,
        .colorAttachments        = &color_attachment,
        .depthStencilAttachment  = (WGPURenderPassDepthStencilAttachment*)0,
        .occlusionQuerySet       = (WGPUQuerySet)0,
        .timestampWrites         = (WGPUPassTimestampWrites*)0,
    };
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rp_desc);

    /* Draw the triangle */
    wgpuRenderPassEncoderSetPipeline(pass, g_pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);

    /* Finish and submit */
    WGPUCommandBufferDescriptor cb_desc = {
        .nextInChain = (WGPUChainedStruct*)0,
        .label       = { .data = "frame_commands", .length = 14 },
    };
    WGPUCommandBuffer cmd_buf = wgpuCommandEncoderFinish(encoder, &cb_desc);
    wgpuQueueSubmit((WGPUQueue)g_gpu_queue, 1, &cmd_buf);

    /* Present */
    res = wapi_gpu_surface_present(g_surface);
    return res;
}

/* ============================================================
 * Frame Callback
 * ============================================================ */

WAPI_EXPORT(wapi_frame)
wapi_result_t wapi_frame(wapi_timestamp_t timestamp) {
    (void)timestamp;

    process_events();

    if (!g_running) {
        return WAPI_ERR_CANCELED;
    }

    render_frame();
    return WAPI_OK;
}

/* ============================================================
 * Entry Point
 * ============================================================ */

WAPI_EXPORT(wapi_main)
wapi_result_t wapi_main(void) {
    wapi_result_t res = init();
    if (WAPI_FAILED(res)) return res;
    return WAPI_OK;
}
