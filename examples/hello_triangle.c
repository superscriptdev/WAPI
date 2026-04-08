/**
 * WAPI - Hello Triangle Example
 *
 * A minimal graphical application that demonstrates the Graphical preset:
 * creates a surface, gets a GPU device, and renders a colored triangle.
 *
 * This example shows the core render loop pattern:
 *   1. Create surface
 *   2. Request GPU device
 *   3. Configure surface for GPU rendering
 *   4. Each frame: get texture, encode commands, submit, present
 *   5. Handle input events
 *
 * Note: This example uses the WAPI GPU bridge functions for surface/device
 * setup, and would use webgpu.h functions (via direct wapi_wgpu imports or
 * wapi_gpu_get_proc_address) for actual rendering commands. The rendering
 * commands shown here are pseudocode demonstrating the intended flow.
 *
 * Compile with:
 *   clang --target=wasm32 -O2 -nostdlib \
 *     -I../include -o hello_triangle.wasm hello_triangle.c
 */

#include <wapi/wapi.h>

/* ============================================================
 * Application State
 * ============================================================ */

static wapi_handle_t g_surface;
static wapi_handle_t g_gpu_device;
static wapi_handle_t g_gpu_queue;
static int32_t     g_width;
static int32_t     g_height;
static int         g_running;

/* ============================================================
 * Initialization
 * ============================================================ */

static wapi_result_t init(void) {
    wapi_result_t res;

    /* Check capabilities */
    if (!wapi_preset_supported(WAPI_PRESET_GRAPHICAL)) {
        return WAPI_ERR_NOTCAPABLE;
    }

    /* Create a windowed surface */
    wapi_window_config_t window_cfg = {
        .chain       = { .next = (void*)0, .sType = WAPI_STYPE_WINDOW_CONFIG },
        .title       = "Hello Triangle",
        .title_len   = 14,
        .window_flags = WAPI_WINDOW_FLAG_RESIZABLE,
    };
    wapi_surface_desc_t surface_desc = {
        .nextInChain = (uintptr_t)&window_cfg,
        .width       = 800,
        .height      = 600,
        .flags       = WAPI_SURFACE_FLAG_HIGH_DPI,
    };
    res = wapi_surface_create(&surface_desc, &g_surface);
    if (WAPI_FAILED(res)) return res;

    wapi_surface_get_size(g_surface, &g_width, &g_height);

    /* Request a GPU device */
    wapi_gpu_device_desc_t gpu_desc = {
        .nextInChain             = (void*)0,
        .power_preference        = WAPI_GPU_POWER_DEFAULT,
        .required_features_count = 0,
        .required_features       = (void*)0,
    };
    res = wapi_gpu_request_device(&gpu_desc, &g_gpu_device);
    if (WAPI_FAILED(res)) return res;

    /* Get the device's queue */
    res = wapi_gpu_get_queue(g_gpu_device, &g_gpu_queue);
    if (WAPI_FAILED(res)) return res;

    /* Configure the surface for GPU rendering */
    wapi_gpu_texture_format_t preferred_format;
    wapi_gpu_surface_preferred_format(g_surface, &preferred_format);

    wapi_gpu_surface_config_t surface_config = {
        .nextInChain  = (void*)0,
        .surface      = g_surface,
        .device       = g_gpu_device,
        .format       = preferred_format,
        .present_mode = WAPI_GPU_PRESENT_FIFO,  /* VSync */
        .usage        = 0x10, /* WGPUTextureUsage_RenderAttachment */
    };
    res = wapi_gpu_configure_surface(&surface_config);
    if (WAPI_FAILED(res)) return res;

    g_running = 1;
    return WAPI_OK;
}

/* ============================================================
 * Event Processing
 * ============================================================ */

static void process_events(void) {
    wapi_event_t event;
    while (wapi_io_poll(&event)) {
        switch (event.type) {
            case WAPI_EVENT_WINDOW_CLOSE:
                g_running = 0;
                break;

            case WAPI_EVENT_SURFACE_RESIZED:
                g_width  = event.surface.data1;
                g_height = event.surface.data2;
                /* Reconfigure surface with new size */
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
 * ============================================================
 *
 * In a real application, you would use webgpu.h functions obtained
 * via wapi_gpu_get_proc_address(). The flow below shows the intended
 * pattern with the WAPI GPU bridge functions.
 *
 * The actual WebGPU commands (createShaderModule, createRenderPipeline,
 * beginRenderPass, draw, end, finish, submit) use the standard
 * wgpu* function signatures from webgpu.h.
 */

static wapi_result_t render_frame(void) {
    wapi_result_t res;

    /* Get the current back-buffer texture */
    wapi_handle_t texture, texture_view;
    res = wapi_gpu_surface_get_current_texture(g_surface, &texture, &texture_view);
    if (WAPI_FAILED(res)) return res;

    /*
     * Here you would use webgpu.h functions to:
     *
     * 1. Create a command encoder:
     *    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &desc);
     *
     * 2. Begin a render pass with the surface texture as color attachment:
     *    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
     *
     * 3. Set the pipeline (created once during init with triangle vertex shader):
     *    wgpuRenderPassEncoderSetPipeline(pass, trianglePipeline);
     *
     * 4. Draw the triangle:
     *    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
     *
     * 5. End the pass and submit:
     *    wgpuRenderPassEncoderEnd(pass);
     *    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &finishDesc);
     *    wgpuQueueSubmit(queue, 1, &cmd);
     */

    /* Present the rendered frame */
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
        return WAPI_ERR_CANCELED;  /* Signal the host to exit */
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

    /*
     * For hosts that don't use the wapi_frame callback model,
     * the module can run its own loop:
     *
     * while (g_running) {
     *     wapi_timestamp_t now;
     *     wapi_clock_time_get(WAPI_CLOCK_MONOTONIC, &now);
     *     wapi_frame(now);
     * }
     */

    return WAPI_OK;
}
