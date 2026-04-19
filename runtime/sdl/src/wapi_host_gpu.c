/**
 * WAPI SDL Runtime - GPU (WebGPU via Dawn)
 *
 * Identical in spirit to the desktop runtime's wgpu-native port: both
 * implement the webgpu-headers C API. Differences vs. desktop:
 *
 *   - A single WGPUInstance is shared across all device requests and
 *     stored in g_rt.wgpu_instance; it is released at shutdown.
 *   - SDL3 native window properties feed into
 *     WGPUSurfaceSourceWindowsHWND / Xlib / Wayland / MetalLayer chains
 *     to produce platform surfaces, same as desktop.
 *
 * Dawn's webgpu.h exposes WGPUSurfaceSourceXxx (modern spec) rather
 * than wgpu-native's older WGPUSurfaceDescriptorFromXxx names, but the
 * ABI sTypes / field layouts match the spec and compile identically.
 */

#include "wapi_host.h"

#ifdef _WIN32
  #include <windows.h>
#endif

static WGPUInstance ensure_instance(void) {
    if (g_rt.wgpu_instance) return g_rt.wgpu_instance;
    WGPUInstanceDescriptor desc = {0};
    g_rt.wgpu_instance = wgpuCreateInstance(&desc);
    return g_rt.wgpu_instance;
}

static WGPUSurface create_wgpu_surface_from_sdl(WGPUInstance instance,
                                                SDL_Window* window) {
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    if (!props) return NULL;

#ifdef _WIN32
    HWND hwnd = (HWND)SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    HINSTANCE hinst = (HINSTANCE)SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, GetModuleHandle(NULL));
    if (!hwnd) return NULL;
    WGPUSurfaceSourceWindowsHWND src = {
        .chain = { .next = NULL, .sType = WGPUSType_SurfaceSourceWindowsHWND },
        .hinstance = hinst, .hwnd = hwnd,
    };
    WGPUSurfaceDescriptor d = {
        .nextInChain = (const WGPUChainedStruct*)&src,
        .label = { .data = "WAPI Surface", .length = 12 },
    };
    return wgpuInstanceCreateSurface(instance, &d);

#elif defined(__APPLE__)
    /* SDL3 on macOS: create/get the Metal view & its CAMetalLayer. */
    SDL_MetalView view = SDL_Metal_CreateView(window);
    if (!view) return NULL;
    void* layer = SDL_Metal_GetLayer(view);
    if (!layer) return NULL;
    WGPUSurfaceSourceMetalLayer src = {
        .chain = { .next = NULL, .sType = WGPUSType_SurfaceSourceMetalLayer },
        .layer = layer,
    };
    WGPUSurfaceDescriptor d = {
        .nextInChain = (const WGPUChainedStruct*)&src,
        .label = { .data = "WAPI Surface", .length = 12 },
    };
    return wgpuInstanceCreateSurface(instance, &d);

#elif defined(__linux__)
    void* wl_display = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
    void* wl_surface = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
    if (wl_display && wl_surface) {
        WGPUSurfaceSourceWaylandSurface src = {
            .chain = { .next = NULL, .sType = WGPUSType_SurfaceSourceWaylandSurface },
            .display = wl_display, .surface = wl_surface,
        };
        WGPUSurfaceDescriptor d = {
            .nextInChain = (const WGPUChainedStruct*)&src,
            .label = { .data = "WAPI Surface", .length = 12 },
        };
        return wgpuInstanceCreateSurface(instance, &d);
    }
    void* x_display = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    int64_t x_window = SDL_GetNumberProperty(
        props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (x_display && x_window) {
        WGPUSurfaceSourceXlibWindow src = {
            .chain = { .next = NULL, .sType = WGPUSType_SurfaceSourceXlibWindow },
            .display = x_display, .window = (uint64_t)x_window,
        };
        WGPUSurfaceDescriptor d = {
            .nextInChain = (const WGPUChainedStruct*)&src,
            .label = { .data = "WAPI Surface", .length = 12 },
        };
        return wgpuInstanceCreateSurface(instance, &d);
    }
    return NULL;
#else
    return NULL;
#endif
}

/* ---- async callbacks ---- */

typedef struct { WGPUAdapter adapter; bool done; WGPURequestAdapterStatus status; } adapter_req_t;
typedef struct { WGPUDevice  device;  bool done; WGPURequestDeviceStatus  status; } device_req_t;

static void on_adapter(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                       WGPUStringView msg, void* u1, void* u2) {
    (void)msg; (void)u2;
    adapter_req_t* r = (adapter_req_t*)u1;
    r->status = status; r->adapter = adapter; r->done = true;
}

static void on_device(WGPURequestDeviceStatus status, WGPUDevice device,
                      WGPUStringView msg, void* u1, void* u2) {
    (void)msg; (void)u2;
    device_req_t* r = (device_req_t*)u1;
    r->status = status; r->device = device; r->done = true;
}

static void on_device_lost(WGPUDevice const* dev, WGPUDeviceLostReason reason,
                           WGPUStringView msg, void* u1, void* u2) {
    (void)dev; (void)u1; (void)u2;
    fprintf(stderr, "[WAPI GPU] Device lost (reason %d): %.*s\n",
            (int)reason, (int)msg.length, msg.data);
}

static void on_uncaught(WGPUDevice const* dev, WGPUErrorType type,
                        WGPUStringView msg, void* u1, void* u2) {
    (void)dev; (void)u1; (void)u2;
    fprintf(stderr, "[WAPI GPU] Error (type %d): %.*s\n",
            (int)type, (int)msg.length, msg.data);
}

/* ---- imports ---- */

static int32_t host_request_device(wasm_exec_env_t env,
                                   uint32_t desc_ptr, uint32_t device_out_ptr) {
    (void)env;
    uint32_t power_pref = 0;
    if (desc_ptr != 0) {
        void* desc = wapi_wasm_ptr(desc_ptr, 16);
        if (desc) memcpy(&power_pref, (uint8_t*)desc + 4, 4);
    }
    WGPUInstance instance = ensure_instance();
    if (!instance) { wapi_set_error("wgpuCreateInstance failed"); return WAPI_ERR_NOTCAPABLE; }

    WGPURequestAdapterOptions aopts = { .powerPreference = (WGPUPowerPreference)power_pref };
    adapter_req_t aq = {0};
    WGPURequestAdapterCallbackInfo acb = {
        .mode = WGPUCallbackMode_AllowSpontaneous,
        .callback = on_adapter, .userdata1 = &aq,
    };
    wgpuInstanceRequestAdapter(instance, &aopts, acb);
    wgpuInstanceProcessEvents(instance);
    if (!aq.done || !aq.adapter) { wapi_set_error("no adapter"); return WAPI_ERR_NOTCAPABLE; }

    WGPUDeviceDescriptor ddesc = {
        .label = { .data = "WAPI Device", .length = 11 },
        .deviceLostCallbackInfo = {
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .callback = on_device_lost,
        },
        .uncapturedErrorCallbackInfo = { .callback = on_uncaught },
    };
    device_req_t dq = {0};
    WGPURequestDeviceCallbackInfo dcb = {
        .mode = WGPUCallbackMode_AllowSpontaneous,
        .callback = on_device, .userdata1 = &dq,
    };
    wgpuAdapterRequestDevice(aq.adapter, &ddesc, dcb);
    wgpuInstanceProcessEvents(instance);
    if (!dq.done || !dq.device) {
        wgpuAdapterRelease(aq.adapter);
        wapi_set_error("no device");
        return WAPI_ERR_NOTCAPABLE;
    }

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_GPU_DEVICE);
    if (h == 0) {
        wgpuDeviceRelease(dq.device); wgpuAdapterRelease(aq.adapter);
        return WAPI_ERR_NOMEM;
    }
    g_rt.handles[h].data.gpu_device.device   = dq.device;
    g_rt.handles[h].data.gpu_device.adapter  = aq.adapter;
    g_rt.handles[h].data.gpu_device.instance = instance;  /* shared */
    wapi_wasm_write_i32(device_out_ptr, h);
    return WAPI_OK;
}

static int32_t host_get_queue(wasm_exec_env_t env,
                              int32_t dev, uint32_t queue_out_ptr) {
    (void)env;
    if (!wapi_handle_valid(dev, WAPI_HTYPE_GPU_DEVICE)) return WAPI_ERR_BADF;
    WGPUQueue q = wgpuDeviceGetQueue(g_rt.handles[dev].data.gpu_device.device);
    if (!q) return WAPI_ERR_UNKNOWN;
    int32_t h = wapi_handle_alloc(WAPI_HTYPE_GPU_QUEUE);
    if (h == 0) return WAPI_ERR_NOMEM;
    g_rt.handles[h].data.gpu_queue = q;
    wapi_wasm_write_i32(queue_out_ptr, h);
    return WAPI_OK;
}

static int32_t host_release_device(wasm_exec_env_t env, int32_t dev) {
    (void)env;
    if (!wapi_handle_valid(dev, WAPI_HTYPE_GPU_DEVICE)) return WAPI_ERR_BADF;
    wgpuDeviceRelease(g_rt.handles[dev].data.gpu_device.device);
    wgpuAdapterRelease(g_rt.handles[dev].data.gpu_device.adapter);
    /* Instance is shared — kept alive by g_rt.wgpu_instance. */
    wapi_handle_free(dev);
    return WAPI_OK;
}

static int32_t host_configure_surface(wasm_exec_env_t env, uint32_t config_ptr) {
    (void)env;
    uint8_t* cfg = (uint8_t*)wapi_wasm_ptr(config_ptr, 24);
    if (!cfg) return WAPI_ERR_INVAL;
    int32_t  sh, dh; uint32_t fmt, pm, usage;
    memcpy(&sh, cfg + 4, 4);
    memcpy(&dh, cfg + 8, 4);
    memcpy(&fmt, cfg + 12, 4);
    memcpy(&pm,  cfg + 16, 4);
    memcpy(&usage, cfg + 20, 4);
    if (!wapi_handle_valid(sh, WAPI_HTYPE_SURFACE))    return WAPI_ERR_BADF;
    if (!wapi_handle_valid(dh, WAPI_HTYPE_GPU_DEVICE)) return WAPI_ERR_BADF;

    SDL_Window*  win    = g_rt.handles[sh].data.window;
    WGPUDevice   device = g_rt.handles[dh].data.gpu_device.device;
    WGPUInstance inst   = g_rt.handles[dh].data.gpu_device.instance;

    /* Reuse any existing wgpu surface for this wapi surface. */
    wapi_gpu_surface_state_t* gs = wapi_gpu_surface_find(sh);
    WGPUSurface wsurf = gs ? gs->wgpu_surface : NULL;
    if (!wsurf) {
        wsurf = create_wgpu_surface_from_sdl(inst, win);
        if (!wsurf) return WAPI_ERR_UNKNOWN;
    }

    int w, h;
    SDL_GetWindowSizeInPixels(win, &w, &h);
    if (usage == 0) usage = WGPUTextureUsage_RenderAttachment;

    WGPUSurfaceConfiguration cfgs = {
        .device = device,
        .format = (WGPUTextureFormat)fmt,
        .usage  = (WGPUTextureUsage)usage,
        .alphaMode = WGPUCompositeAlphaMode_Auto,
        .width = (uint32_t)w, .height = (uint32_t)h,
        .presentMode = (WGPUPresentMode)pm,
    };
    wgpuSurfaceConfigure(wsurf, &cfgs);

    if (!gs) {
        if (g_rt.gpu_surface_count >= WAPI_MAX_GPU_SURFACES) {
            wgpuSurfaceRelease(wsurf);
            return WAPI_ERR_NOSPC;
        }
        gs = &g_rt.gpu_surfaces[g_rt.gpu_surface_count++];
    }
    gs->wapi_surface_handle = sh;
    gs->wgpu_surface  = wsurf;
    gs->wgpu_device   = device;
    gs->format        = fmt;
    gs->present_mode  = pm;
    gs->usage         = usage;
    gs->width         = (uint32_t)w;
    gs->height        = (uint32_t)h;
    gs->configured    = true;
    return WAPI_OK;
}

static int32_t host_surface_get_current_texture(wasm_exec_env_t env,
                                                int32_t sh,
                                                uint32_t tex_out, uint32_t view_out) {
    (void)env;
    wapi_gpu_surface_state_t* gs = wapi_gpu_surface_find(sh);
    if (!gs || !gs->configured) return WAPI_ERR_INVAL;
    WGPUSurfaceTexture st;
    wgpuSurfaceGetCurrentTexture(gs->wgpu_surface, &st);
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        return WAPI_ERR_UNKNOWN;
    }
    WGPUTextureView v = wgpuTextureCreateView(st.texture, NULL);
    if (!v) return WAPI_ERR_UNKNOWN;
    int32_t th = wapi_handle_alloc(WAPI_HTYPE_GPU_TEXTURE);
    int32_t vh = wapi_handle_alloc(WAPI_HTYPE_GPU_TEXTURE_VIEW);
    if (th == 0 || vh == 0) {
        wgpuTextureViewRelease(v);
        if (th) wapi_handle_free(th);
        if (vh) wapi_handle_free(vh);
        return WAPI_ERR_NOMEM;
    }
    g_rt.handles[th].data.gpu_texture = st.texture;
    g_rt.handles[vh].data.gpu_texture_view = v;
    wapi_wasm_write_i32(tex_out, th);
    wapi_wasm_write_i32(view_out, vh);
    return WAPI_OK;
}

static int32_t host_surface_present(wasm_exec_env_t env, int32_t sh) {
    (void)env;
    wapi_gpu_surface_state_t* gs = wapi_gpu_surface_find(sh);
    if (!gs || !gs->configured) return WAPI_ERR_INVAL;
    wgpuSurfacePresent(gs->wgpu_surface);
    return WAPI_OK;
}

static int32_t host_surface_preferred_format(wasm_exec_env_t env,
                                             int32_t sh, uint32_t fmt_out) {
    (void)env;
    wapi_gpu_surface_state_t* gs = wapi_gpu_surface_find(sh);
    uint32_t fmt = (gs && gs->configured) ? gs->format : 0x0057;  /* BGRA8Unorm */
    wapi_wasm_write_u32(fmt_out, fmt);
    return WAPI_OK;
}

static int32_t host_get_proc_address(wasm_exec_env_t env,
                                     uint32_t name_ptr, uint32_t name_len) {
    (void)env; (void)name_ptr; (void)name_len;
    /* Function pointers cannot cross the sandbox boundary; all GPU ops
     * flow through the wapi_gpu imports. Always return 0 (not found). */
    return 0;
}

static NativeSymbol g_symbols[] = {
    { "request_device",               (void*)host_request_device,               "(ii)i",  NULL },
    { "get_queue",                    (void*)host_get_queue,                    "(ii)i",  NULL },
    { "release_device",               (void*)host_release_device,               "(i)i",   NULL },
    { "configure_surface",            (void*)host_configure_surface,            "(i)i",   NULL },
    { "surface_get_current_texture",  (void*)host_surface_get_current_texture,  "(iii)i", NULL },
    { "surface_present",              (void*)host_surface_present,              "(i)i",   NULL },
    { "surface_preferred_format",     (void*)host_surface_preferred_format,     "(ii)i",  NULL },
    { "get_proc_address",             (void*)host_get_proc_address,             "(ii)i",  NULL },
};

wapi_cap_registration_t wapi_host_gpu_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_gpu",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
