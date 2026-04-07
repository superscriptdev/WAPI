/**
 * WAPI - GPU (WebGPU)
 * Version 1.0.0
 *
 * GPU compute and rendering via webgpu.h. This module does NOT
 * redefine the WebGPU API. Instead, it provides the bridge between
 * the WAPI's handle/surface model and the webgpu.h API.
 *
 * The full webgpu.h header (from webgpu-native/webgpu-headers) is
 * used directly for all GPU operations. This header only defines:
 *
 *   1. How to obtain a WGPUInstance and WGPUDevice through the
 *      WAPI's capability system.
 *   2. How to connect a WGPUDevice to a WAPI surface
 *      for rendering.
 *   3. How GPU handles map to the WAPI handle space.
 *
 * The module includes webgpu.h alongside this header and uses the
 * standard wgpu* functions for all GPU operations.
 *
 * Import module: "wapi_gpu"
 *
 * NOTE: webgpu.h is approximately 6,750 lines and defines ~150
 * functions. It is maintained by the webgpu-native project and
 * implemented by both Dawn (Google) and wgpu-native (Rust).
 * See: https://github.com/webgpu-native/webgpu-headers
 */

#ifndef WAPI_GPU_H
#define WAPI_GPU_H

#include "wapi.h"

/*
 * Include webgpu.h for the full GPU API.
 * The host implements the wgpu* function table.
 * On native: links to Dawn or wgpu-native.
 * In browser: the JS shim maps to navigator.gpu.
 */
/* #include "webgpu.h" */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * GPU Initialization
 * ============================================================
 * The WAPI provides a streamlined path to get a GPU
 * device. This avoids the boilerplate of adapter enumeration
 * and asynchronous device requests that webgpu.h requires.
 *
 * For advanced use cases (multi-adapter, specific feature
 * requirements), the module can use the full webgpu.h
 * wgpuCreateInstance -> requestAdapter -> requestDevice flow.
 */

/**
 * GPU device descriptor for the simplified creation path.
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: ptr      nextInChain
 *   Offset  4: uint32_t power_preference (0=default, 1=low_power, 2=high_perf)
 *   Offset  8: uint32_t required_features_count
 *   Offset 12: ptr      required_features (array of uint32_t feature enums)
 */
typedef struct wapi_gpu_device_desc_t {
    wapi_chained_struct_t*  nextInChain;
    uint32_t              power_preference;
    uint32_t              required_features_count;
    const uint32_t*       required_features;
} wapi_gpu_device_desc_t;

/** Power preference values (matching WGPUPowerPreference) */
#define WAPI_GPU_POWER_DEFAULT      0
#define WAPI_GPU_POWER_LOW_POWER    1
#define WAPI_GPU_POWER_HIGH_PERF    2

/**
 * Request a GPU device through the WAPI.
 *
 * This is the simplified "give me a GPU" path. The host selects
 * an appropriate adapter and creates a device. Equivalent to:
 *   wgpuCreateInstance -> wgpuInstanceRequestAdapter ->
 *   wgpuAdapterRequestDevice
 *
 * @param desc    Device descriptor (NULL for defaults).
 * @param device  [out] WAPI handle wrapping a WGPUDevice.
 * @return WAPI_OK on success, WAPI_ERR_NOTCAPABLE if no GPU available.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_gpu, request_device)
wapi_result_t wapi_gpu_request_device(const wapi_gpu_device_desc_t* desc,
                                   wapi_handle_t* device);

/**
 * Get the default queue for a GPU device.
 *
 * @param device  GPU device handle.
 * @param queue   [out] WAPI handle wrapping a WGPUQueue.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_gpu, get_queue)
wapi_result_t wapi_gpu_get_queue(wapi_handle_t device, wapi_handle_t* queue);

/**
 * Release a GPU device.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_gpu, release_device)
wapi_result_t wapi_gpu_release_device(wapi_handle_t device);

/* ============================================================
 * Surface-GPU Bridge
 * ============================================================
 * Connecting a GPU device to a WAPI surface for rendering.
 * This replaces the platform-specific WGPUSurfaceSource* structs
 * (HWND, CAMetalLayer, XCB, etc.) with a single WAPI surface handle.
 */

/** Texture format for surface rendering (matching WGPUTextureFormat subset) */
typedef enum wapi_gpu_texture_format_t {
    WAPI_GPU_FORMAT_BGRA8_UNORM       = 0x0057,  /* WGPUTextureFormat_BGRA8Unorm */
    WAPI_GPU_FORMAT_RGBA8_UNORM       = 0x0012,  /* WGPUTextureFormat_RGBA8Unorm */
    WAPI_GPU_FORMAT_BGRA8_UNORM_SRGB  = 0x0058,  /* WGPUTextureFormat_BGRA8UnormSrgb */
    WAPI_GPU_FORMAT_RGBA8_UNORM_SRGB  = 0x0013,  /* WGPUTextureFormat_RGBA8UnormSrgb */
    WAPI_GPU_FORMAT_RGBA16_FLOAT      = 0x0021,  /* WGPUTextureFormat_RGBA16Float */
    WAPI_GPU_FORMAT_FORCE32           = 0x7FFFFFFF
} wapi_gpu_texture_format_t;

/** Present mode (matching WGPUPresentMode) */
typedef enum wapi_gpu_present_mode_t {
    WAPI_GPU_PRESENT_FIFO        = 0,  /* VSync */
    WAPI_GPU_PRESENT_FIFO_RELAXED = 1,
    WAPI_GPU_PRESENT_IMMEDIATE   = 2,  /* No VSync */
    WAPI_GPU_PRESENT_MAILBOX     = 3,  /* Triple buffering */
    WAPI_GPU_PRESENT_FORCE32     = 0x7FFFFFFF
} wapi_gpu_present_mode_t;

/**
 * Surface GPU configuration descriptor.
 *
 * Layout (24 bytes, align 4):
 *   Offset  0: ptr      nextInChain
 *   Offset  4: int32_t  surface       WAPI surface handle
 *   Offset  8: int32_t  device        WAPI GPU device handle
 *   Offset 12: uint32_t format        Texture format
 *   Offset 16: uint32_t present_mode  Present mode
 *   Offset 20: uint32_t usage         Texture usage flags (WGPUTextureUsage)
 */
typedef struct wapi_gpu_surface_config_t {
    wapi_chained_struct_t*       nextInChain;
    wapi_handle_t                surface;
    wapi_handle_t                device;
    uint32_t                   format;        /* wapi_gpu_texture_format_t */
    uint32_t                   present_mode;  /* wapi_gpu_present_mode_t */
    uint32_t                   usage;         /* WGPUTextureUsage flags */
} wapi_gpu_surface_config_t;

/**
 * Configure a WAPI surface for GPU rendering.
 * After this call, the surface can provide textures for rendering.
 *
 * @param config  Surface GPU configuration.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_gpu, configure_surface)
wapi_result_t wapi_gpu_configure_surface(const wapi_gpu_surface_config_t* config);

/**
 * Get the current texture to render to from a configured surface.
 * Call this each frame to get the back-buffer texture.
 *
 * @param surface  WAPI surface handle (must be GPU-configured).
 * @param texture  [out] WAPI handle wrapping a WGPUTexture.
 * @param view     [out] WAPI handle wrapping a WGPUTextureView.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_gpu, surface_get_current_texture)
wapi_result_t wapi_gpu_surface_get_current_texture(wapi_handle_t surface,
                                                wapi_handle_t* texture,
                                                wapi_handle_t* view);

/**
 * Present the current surface texture to the display.
 * Call after submitting all render commands for the frame.
 *
 * @param surface  WAPI surface handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_gpu, surface_present)
wapi_result_t wapi_gpu_surface_present(wapi_handle_t surface);

/**
 * Query the preferred texture format for a surface.
 * The host returns the format that avoids conversion overhead.
 *
 * @param surface  WAPI surface handle.
 * @param format   [out] Preferred texture format.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_gpu, surface_preferred_format)
wapi_result_t wapi_gpu_surface_preferred_format(wapi_handle_t surface,
                                             wapi_gpu_texture_format_t* format);

/* ============================================================
 * WebGPU Function Table
 * ============================================================
 * The host provides the full webgpu.h proc table. The module
 * retrieves function pointers at startup and calls them directly.
 *
 * This matches webgpu.h's WGPUProcTable / wgpuGetProcAddress
 * pattern. The WAPI host simply provides the proc address resolver.
 */

/** Function pointer type for wgpuGetProcAddress */
typedef void (*wapi_gpu_proc_t)(void);

/**
 * Get a WebGPU function pointer by name.
 * This is the WAPI equivalent of wgpuGetProcAddress.
 *
 * @param name      Function name (e.g., "wgpuDeviceCreateBuffer").
 * @return Function pointer, or NULL if not found.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_gpu, get_proc_address)
wapi_gpu_proc_t wapi_gpu_get_proc_address(wapi_string_view_t name);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_GPU_H */
