/**
 * WAPI Desktop Runtime - GPU (WebGPU via wgpu-native)
 *
 * Implements: wapi_gpu.request_device, wapi_gpu.get_queue,
 *             wapi_gpu.release_device, wapi_gpu.configure_surface,
 *             wapi_gpu.surface_get_current_texture, wapi_gpu.surface_present,
 *             wapi_gpu.surface_preferred_format, wapi_gpu.get_proc_address
 *
 * Uses wgpu-native (webgpu.h) for all GPU operations.
 * Uses SDL3 window properties to create WGPUSurface objects.
 */

#include "wapi_host.h"

#ifdef _WIN32
#include <windows.h>
#endif

/* ============================================================
 * Helper: Create WGPUSurface from SDL3 Window
 * ============================================================
 * Uses SDL3 properties to get the native window handle and
 * creates a WGPUSurface from it.
 */

static WGPUSurface create_wgpu_surface_from_sdl(WGPUInstance instance,
                                                  SDL_Window* window)
{
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    if (!props) return NULL;

#ifdef _WIN32
    HWND hwnd = (HWND)SDL_GetPointerProperty(props,
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    HINSTANCE hinstance = (HINSTANCE)SDL_GetPointerProperty(props,
        SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, GetModuleHandle(NULL));

    if (!hwnd) return NULL;

    WGPUSurfaceSourceWindowsHWND hwnd_source = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_SurfaceSourceWindowsHWND,
        },
        .hinstance = hinstance,
        .hwnd = hwnd,
    };
    WGPUSurfaceDescriptor surf_desc = {
        .nextInChain = (const WGPUChainedStruct*)&hwnd_source,
        .label = {.data = "WAPI Surface", .length = 10},
    };
    return wgpuInstanceCreateSurface(instance, &surf_desc);

#elif defined(__APPLE__)
    /* macOS: get the CAMetalLayer from the Cocoa window */
    void* ns_window = SDL_GetPointerProperty(props,
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    if (!ns_window) return NULL;

    WGPUSurfaceSourceMetalLayer metal_source = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_SurfaceSourceMetalLayer,
        },
        .layer = SDL_Metal_GetLayer(SDL_Metal_CreateView(window)),
    };
    WGPUSurfaceDescriptor surf_desc = {
        .nextInChain = (const WGPUChainedStruct*)&metal_source,
        .label = {.data = "WAPI Surface", .length = 10},
    };
    return wgpuInstanceCreateSurface(instance, &surf_desc);

#elif defined(__linux__)
    /* Try Wayland first, then X11 */
    void* wl_display = SDL_GetPointerProperty(props,
        SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
    void* wl_surface = SDL_GetPointerProperty(props,
        SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);

    if (wl_display && wl_surface) {
        WGPUSurfaceSourceWaylandSurface wl_src = {
            .chain = {
                .next = NULL,
                .sType = WGPUSType_SurfaceSourceWaylandSurface,
            },
            .display = wl_display,
            .surface = wl_surface,
        };
        WGPUSurfaceDescriptor surf_desc = {
            .nextInChain = (const WGPUChainedStruct*)&wl_src,
            .label = {.data = "WAPI Surface", .length = 10},
        };
        return wgpuInstanceCreateSurface(instance, &surf_desc);
    }

    /* X11 / Xlib */
    void* x11_display = SDL_GetPointerProperty(props,
        SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    int64_t x11_window = (int64_t)SDL_GetNumberProperty(props,
        SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);

    if (x11_display && x11_window) {
        WGPUSurfaceSourceXlibWindow xlib_src = {
            .chain = {
                .next = NULL,
                .sType = WGPUSType_SurfaceSourceXlibWindow,
            },
            .display = x11_display,
            .window = (uint64_t)x11_window,
        };
        WGPUSurfaceDescriptor surf_desc = {
            .nextInChain = (const WGPUChainedStruct*)&xlib_src,
            .label = {.data = "WAPI Surface", .length = 10},
        };
        return wgpuInstanceCreateSurface(instance, &surf_desc);
    }

    return NULL;
#else
    return NULL;
#endif
}

/* ============================================================
 * Adapter/Device Request Callbacks (synchronous wrappers)
 * ============================================================ */

typedef struct {
    WGPUAdapter adapter;
    bool done;
    WGPURequestAdapterStatus status;
} adapter_request_t;

static void on_adapter_request(WGPURequestAdapterStatus status,
                                WGPUAdapter adapter,
                                WGPUStringView message,
                                void* userdata1,
                                void* userdata2)
{
    (void)message; (void)userdata2;
    adapter_request_t* req = (adapter_request_t*)userdata1;
    req->status = status;
    req->adapter = adapter;
    req->done = true;
}

typedef struct {
    WGPUDevice device;
    bool done;
    WGPURequestDeviceStatus status;
} device_request_t;

static void on_device_request(WGPURequestDeviceStatus status,
                               WGPUDevice device,
                               WGPUStringView message,
                               void* userdata1,
                               void* userdata2)
{
    (void)message; (void)userdata2;
    device_request_t* req = (device_request_t*)userdata1;
    req->status = status;
    req->device = device;
    req->done = true;
}

static void on_device_lost(WGPUDevice const* device,
                            WGPUDeviceLostReason reason,
                            WGPUStringView message,
                            void* userdata1,
                            void* userdata2)
{
    (void)device; (void)userdata1; (void)userdata2;
    fprintf(stderr, "[WAPI GPU] Device lost (reason %d): %.*s\n",
            reason, (int)message.length, message.data);
}

static void on_uncaptured_error(WGPUDevice const* device,
                                 WGPUErrorType type,
                                 WGPUStringView message,
                                 void* userdata1,
                                 void* userdata2)
{
    (void)device; (void)userdata1; (void)userdata2;
    fprintf(stderr, "[WAPI GPU] Uncaptured error (type %d): %.*s\n",
            type, (int)message.length, message.data);
}

/* ============================================================
 * GPU Functions
 * ============================================================ */

/* request_device: (i32 desc_ptr, i32 device_out_ptr) -> i32 */
static wasm_trap_t* host_gpu_request_device(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t desc_ptr = WAPI_ARG_U32(0);
    uint32_t device_out_ptr = WAPI_ARG_U32(1);

    /* Read power preference from desc if provided.
     * wapi_gpu_device_desc_t wasm32 layout:
     *   +0: u32 nextInChain
     *   +4: u32 power_preference
     *   +8: u32 required_features_count
     *  +12: u32 required_features (ptr)
     */
    uint32_t power_pref = 0; /* default */
    if (desc_ptr != 0) {
        void* desc = wapi_wasm_ptr(desc_ptr, 16);
        if (desc) {
            memcpy(&power_pref, (uint8_t*)desc + 4, 4);
        }
    }

    /* Create WGPUInstance */
    WGPUInstanceDescriptor inst_desc = {0};
    WGPUInstance instance = wgpuCreateInstance(&inst_desc);
    if (!instance) {
        wapi_set_error("Failed to create WGPUInstance");
        WAPI_RET_I32(WAPI_ERR_NOTCAPABLE);
        return NULL;
    }

    /* Request adapter (synchronous via callback) */
    WGPURequestAdapterOptions adapter_opts = {
        .powerPreference = (WGPUPowerPreference)power_pref,
    };

    adapter_request_t adapter_req = {0};
    WGPURequestAdapterCallbackInfo adapter_cb = {
        .mode = WGPUCallbackMode_AllowSpontaneous,
        .callback = on_adapter_request,
        .userdata1 = &adapter_req,
    };
    wgpuInstanceRequestAdapter(instance, &adapter_opts, adapter_cb);

    /* wgpu-native completes synchronously for AllowSpontaneous mode,
     * but process events just in case */
    wgpuInstanceProcessEvents(instance);

    if (!adapter_req.done || !adapter_req.adapter) {
        wgpuInstanceRelease(instance);
        wapi_set_error("Failed to request GPU adapter");
        WAPI_RET_I32(WAPI_ERR_NOTCAPABLE);
        return NULL;
    }

    /* Request device */
    WGPUDeviceDescriptor dev_desc = {
        .label = {.data = "WAPI Device", .length = 9},
        .deviceLostCallbackInfo = {
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .callback = on_device_lost,
        },
        .uncapturedErrorCallbackInfo = {
            .callback = on_uncaptured_error,
        },
    };

    device_request_t dev_req = {0};
    WGPURequestDeviceCallbackInfo dev_cb = {
        .mode = WGPUCallbackMode_AllowSpontaneous,
        .callback = on_device_request,
        .userdata1 = &dev_req,
    };
    wgpuAdapterRequestDevice(adapter_req.adapter, &dev_desc, dev_cb);
    wgpuInstanceProcessEvents(instance);

    if (!dev_req.done || !dev_req.device) {
        wgpuAdapterRelease(adapter_req.adapter);
        wgpuInstanceRelease(instance);
        wapi_set_error("Failed to request GPU device");
        WAPI_RET_I32(WAPI_ERR_NOTCAPABLE);
        return NULL;
    }

    /* Allocate handle */
    int32_t handle = wapi_handle_alloc(WAPI_HTYPE_GPU_DEVICE);
    if (handle == 0) {
        wgpuDeviceRelease(dev_req.device);
        wgpuAdapterRelease(adapter_req.adapter);
        wgpuInstanceRelease(instance);
        wapi_set_error("Handle table full");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    g_rt.handles[handle].data.gpu_device.device = dev_req.device;
    g_rt.handles[handle].data.gpu_device.adapter = adapter_req.adapter;
    g_rt.handles[handle].data.gpu_device.instance = instance;

    wapi_wasm_write_i32(device_out_ptr, handle);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* get_queue: (i32 device, i32 queue_out) -> i32 */
static wasm_trap_t* host_gpu_get_queue(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t dev_h = WAPI_ARG_I32(0);
    uint32_t queue_out_ptr = WAPI_ARG_U32(1);

    if (!wapi_handle_valid(dev_h, WAPI_HTYPE_GPU_DEVICE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    WGPUDevice device = g_rt.handles[dev_h].data.gpu_device.device;
    WGPUQueue queue = wgpuDeviceGetQueue(device);
    if (!queue) {
        wapi_set_error("Failed to get device queue");
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }

    int32_t handle = wapi_handle_alloc(WAPI_HTYPE_GPU_QUEUE);
    if (handle == 0) {
        wapi_set_error("Handle table full");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    g_rt.handles[handle].data.gpu_queue = queue;
    wapi_wasm_write_i32(queue_out_ptr, handle);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* release_device: (i32 device) -> i32 */
static wasm_trap_t* host_gpu_release_device(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);

    if (!wapi_handle_valid(h, WAPI_HTYPE_GPU_DEVICE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    wgpuDeviceRelease(g_rt.handles[h].data.gpu_device.device);
    wgpuAdapterRelease(g_rt.handles[h].data.gpu_device.adapter);
    wgpuInstanceRelease(g_rt.handles[h].data.gpu_device.instance);
    wapi_handle_free(h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Surface-GPU Bridge
 * ============================================================ */

/* configure_surface: (i32 config_ptr) -> i32 */
static wasm_trap_t* host_gpu_configure_surface(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t config_ptr = WAPI_ARG_U32(0);

    /* wapi_gpu_surface_config_t wasm32 layout:
     *   +0: u32 nextInChain
     *   +4: i32 surface
     *   +8: i32 device
     *  +12: u32 format
     *  +16: u32 present_mode
     *  +20: u32 usage
     */
    void* config_host = wapi_wasm_ptr(config_ptr, 24);
    if (!config_host) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    int32_t surface_handle, device_handle;
    uint32_t format, present_mode, usage;
    memcpy(&surface_handle, (uint8_t*)config_host + 4, 4);
    memcpy(&device_handle,  (uint8_t*)config_host + 8, 4);
    memcpy(&format,         (uint8_t*)config_host + 12, 4);
    memcpy(&present_mode,   (uint8_t*)config_host + 16, 4);
    memcpy(&usage,          (uint8_t*)config_host + 20, 4);

    if (!wapi_handle_valid(surface_handle, WAPI_HTYPE_SURFACE)) {
        wapi_set_error("Invalid surface handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }
    if (!wapi_handle_valid(device_handle, WAPI_HTYPE_GPU_DEVICE)) {
        wapi_set_error("Invalid GPU device handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    SDL_Window* window = g_rt.handles[surface_handle].data.window;
    WGPUDevice device = g_rt.handles[device_handle].data.gpu_device.device;
    WGPUInstance instance = g_rt.handles[device_handle].data.gpu_device.instance;

    /* Create WGPUSurface from the SDL window */
    WGPUSurface wgpu_surface = create_wgpu_surface_from_sdl(instance, window);
    if (!wgpu_surface) {
        wapi_set_error("Failed to create WGPUSurface from SDL window");
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }

    /* Get window size for surface config */
    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    /* Default usage to RenderAttachment if not specified */
    if (usage == 0) usage = WGPUTextureUsage_RenderAttachment;

    /* Configure the surface */
    WGPUSurfaceConfiguration surf_config = {
        .device = device,
        .format = (WGPUTextureFormat)format,
        .usage = (WGPUTextureUsageFlags)usage,
        .alphaMode = WGPUCompositeAlphaMode_Auto,
        .width = (uint32_t)w,
        .height = (uint32_t)h,
        .presentMode = (WGPUPresentMode)present_mode,
    };
    wgpuSurfaceConfigure(wgpu_surface, &surf_config);

    /* Track this surface association */
    if (g_rt.gpu_surface_count < WAPI_MAX_GPU_SURFACES) {
        wapi_gpu_surface_state_t* gs = &g_rt.gpu_surfaces[g_rt.gpu_surface_count++];
        gs->wapi_surface_handle = surface_handle;
        gs->wgpu_surface = wgpu_surface;
        gs->wgpu_device = device;
        gs->format = format;
        gs->present_mode = present_mode;
        gs->usage = usage;
        gs->configured = true;
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* surface_get_current_texture: (i32 surface, i32 texture_out, i32 view_out) -> i32 */
static wasm_trap_t* host_gpu_surface_get_current_texture(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t surface_h = WAPI_ARG_I32(0);
    uint32_t texture_out_ptr = WAPI_ARG_U32(1);
    uint32_t view_out_ptr = WAPI_ARG_U32(2);

    wapi_gpu_surface_state_t* gs = wapi_gpu_surface_find(surface_h);
    if (!gs || !gs->configured) {
        wapi_set_error("Surface not configured for GPU");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    WGPUSurfaceTexture surf_tex;
    wgpuSurfaceGetCurrentTexture(gs->wgpu_surface, &surf_tex);
    if (surf_tex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surf_tex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        wapi_set_error("Failed to get current surface texture");
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }

    /* Create a texture view */
    WGPUTextureView view = wgpuTextureCreateView(surf_tex.texture, NULL);
    if (!view) {
        wapi_set_error("Failed to create texture view");
        WAPI_RET_I32(WAPI_ERR_UNKNOWN);
        return NULL;
    }

    /* Allocate handles for texture and view */
    int32_t tex_handle = wapi_handle_alloc(WAPI_HTYPE_GPU_TEXTURE);
    int32_t view_handle = wapi_handle_alloc(WAPI_HTYPE_GPU_TEXTURE_VIEW);
    if (tex_handle == 0 || view_handle == 0) {
        wgpuTextureViewRelease(view);
        if (tex_handle) wapi_handle_free(tex_handle);
        if (view_handle) wapi_handle_free(view_handle);
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    g_rt.handles[tex_handle].data.gpu_texture = surf_tex.texture;
    g_rt.handles[view_handle].data.gpu_texture_view = view;

    wapi_wasm_write_i32(texture_out_ptr, tex_handle);
    wapi_wasm_write_i32(view_out_ptr, view_handle);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* surface_present: (i32 surface) -> i32 */
static wasm_trap_t* host_gpu_surface_present(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t surface_h = WAPI_ARG_I32(0);

    wapi_gpu_surface_state_t* gs = wapi_gpu_surface_find(surface_h);
    if (!gs || !gs->configured) {
        wapi_set_error("Surface not configured for GPU");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    wgpuSurfacePresent(gs->wgpu_surface);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* surface_preferred_format: (i32 surface, i32 format_out) -> i32 */
static wasm_trap_t* host_gpu_surface_preferred_format(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t surface_h = WAPI_ARG_I32(0);
    uint32_t format_out_ptr = WAPI_ARG_U32(1);

    wapi_gpu_surface_state_t* gs = wapi_gpu_surface_find(surface_h);
    if (gs && gs->configured) {
        /* Return the currently configured format */
        wapi_wasm_write_u32(format_out_ptr, gs->format);
    } else {
        /* Default to BGRA8Unorm -- the most common preferred format */
        wapi_wasm_write_u32(format_out_ptr, 0x0057); /* WGPUTextureFormat_BGRA8Unorm */
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * WebGPU Proc Address
 * ============================================================ */

/* get_proc_address: (i32 name_ptr, i32 name_len) -> i32 */
static wasm_trap_t* host_gpu_get_proc_address(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t name_ptr = WAPI_ARG_U32(0);
    uint32_t name_len = WAPI_ARG_U32(1);

    const char* name = wapi_wasm_read_string(name_ptr, name_len);
    if (!name) {
        WAPI_RET_I32(0);
        return NULL;
    }

    /*
     * In a native host, we cannot pass function pointers into Wasm.
     * The get_proc_address model doesn't directly apply to a sandboxed
     * Wasm module. Instead, each wgpu function would be exposed as an
     * individual Wasm import.
     *
     * For this runtime, return 0 (not found) since all WebGPU operations
     * go through the WAPI GPU bridge functions above, not via direct
     * function pointers.
     *
     * A production runtime would expose each wgpuXxx function as a
     * separate Wasm import in a "webgpu" module.
     */
    WAPI_RET_I32(0);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_gpu(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_gpu", "request_device",              host_gpu_request_device);
    WAPI_DEFINE_2_1(linker, "wapi_gpu", "get_queue",                   host_gpu_get_queue);
    WAPI_DEFINE_1_1(linker, "wapi_gpu", "release_device",              host_gpu_release_device);
    WAPI_DEFINE_1_1(linker, "wapi_gpu", "configure_surface",           host_gpu_configure_surface);
    WAPI_DEFINE_3_1(linker, "wapi_gpu", "surface_get_current_texture", host_gpu_surface_get_current_texture);
    WAPI_DEFINE_1_1(linker, "wapi_gpu", "surface_present",             host_gpu_surface_present);
    WAPI_DEFINE_2_1(linker, "wapi_gpu", "surface_preferred_format",    host_gpu_surface_preferred_format);
    WAPI_DEFINE_2_1(linker, "wapi_gpu", "get_proc_address",            host_gpu_get_proc_address);
}
