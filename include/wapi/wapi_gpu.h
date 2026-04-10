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
 * Layout (24 bytes, align 8):
 *   Offset  0: uint64_t nextInChain             Linear memory address, or 0
 *   Offset  8: uint32_t power_preference        (0=default, 1=low_power, 2=high_perf)
 *   Offset 12: uint32_t required_features_count
 *   Offset 16: uint64_t required_features       Linear memory address of uint32_t array
 */
typedef struct wapi_gpu_device_desc_t {
    uint64_t              nextInChain;  /* Address of wapi_chained_struct_t, or 0 */
    uint32_t              power_preference;
    uint32_t              required_features_count;
    uint64_t              required_features;  /* Address of uint32_t array */
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

/** Texture format for surface rendering (matching WGPUTextureFormat values) */
typedef enum wapi_gpu_texture_format_t {
    WAPI_GPU_FORMAT_RGBA8_UNORM       = 0x0016,  /* WGPUTextureFormat_RGBA8Unorm */
    WAPI_GPU_FORMAT_RGBA8_UNORM_SRGB  = 0x0017,  /* WGPUTextureFormat_RGBA8UnormSrgb */
    WAPI_GPU_FORMAT_BGRA8_UNORM       = 0x001B,  /* WGPUTextureFormat_BGRA8Unorm */
    WAPI_GPU_FORMAT_BGRA8_UNORM_SRGB  = 0x001C,  /* WGPUTextureFormat_BGRA8UnormSrgb */
    WAPI_GPU_FORMAT_RGBA16_FLOAT      = 0x0028,  /* WGPUTextureFormat_RGBA16Float */
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
 * Layout (32 bytes, align 8):
 *   Offset  0: uint64_t nextInChain   Linear memory address, or 0
 *   Offset  8: int32_t  surface       WAPI surface handle
 *   Offset 12: int32_t  device        WAPI GPU device handle
 *   Offset 16: uint32_t format        Texture format
 *   Offset 20: uint32_t present_mode  Present mode
 *   Offset 24: uint32_t usage         Texture usage flags (WGPUTextureUsage)
 *   Offset 28: uint32_t _pad
 */
typedef struct wapi_gpu_surface_config_t {
    uint64_t                     nextInChain;  /* Address of wapi_chained_struct_t, or 0 */
    wapi_handle_t                surface;
    wapi_handle_t                device;
    uint32_t                   format;        /* wapi_gpu_texture_format_t */
    uint32_t                   present_mode;  /* wapi_gpu_present_mode_t */
    uint32_t                   usage;         /* WGPUTextureUsage flags */
    uint32_t                   _pad;
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
 * WebGPU Direct Imports
 * ============================================================
 * Standard webgpu.h functions are imported directly, compiled
 * to direct wasm `call` instructions with zero indirection.
 * The module imports only the functions it uses.
 *
 * This is the primary path for GPU operations. For dynamic
 * lookup of extension functions or functions not listed here,
 * use wapi_gpu_get_proc_address below.
 *
 * NOTE: These declarations require webgpu.h to be included
 * first for the WGPUDevice, WGPUBuffer, etc. types. When
 * webgpu.h is not included, only get_proc_address is available.
 */

#ifdef WEBGPU_H_

/* --- Device --- */
WAPI_IMPORT(wapi_wgpu, device_create_buffer)
WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor* descriptor);

WAPI_IMPORT(wapi_wgpu, device_create_texture)
WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor* descriptor);

WAPI_IMPORT(wapi_wgpu, device_create_sampler)
WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor* descriptor);

WAPI_IMPORT(wapi_wgpu, device_create_bind_group_layout)
WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutDescriptor* descriptor);

WAPI_IMPORT(wapi_wgpu, device_create_bind_group)
WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice device, const WGPUBindGroupDescriptor* descriptor);

WAPI_IMPORT(wapi_wgpu, device_create_pipeline_layout)
WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device, const WGPUPipelineLayoutDescriptor* descriptor);

WAPI_IMPORT(wapi_wgpu, device_create_shader_module)
WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device, const WGPUShaderModuleDescriptor* descriptor);

WAPI_IMPORT(wapi_wgpu, device_create_render_pipeline)
WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor);

WAPI_IMPORT(wapi_wgpu, device_create_compute_pipeline)
WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor);

WAPI_IMPORT(wapi_wgpu, device_create_command_encoder)
WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device, const WGPUCommandEncoderDescriptor* descriptor);

WAPI_IMPORT(wapi_wgpu, device_create_query_set)
WGPUQuerySet wgpuDeviceCreateQuerySet(WGPUDevice device, const WGPUQuerySetDescriptor* descriptor);

/* --- Queue --- */
WAPI_IMPORT(wapi_wgpu, queue_submit)
void wgpuQueueSubmit(WGPUQueue queue, size_t commandCount, const WGPUCommandBuffer* commands);

WAPI_IMPORT(wapi_wgpu, queue_write_buffer)
void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, const void* data, size_t size);

WAPI_IMPORT(wapi_wgpu, queue_write_texture)
void wgpuQueueWriteTexture(WGPUQueue queue, const WGPUTexelCopyTextureInfo* destination, const void* data, size_t dataSize, const WGPUTexelCopyBufferLayout* dataLayout, const WGPUExtent3D* writeSize);

/* --- Command Encoder --- */
WAPI_IMPORT(wapi_wgpu, command_encoder_begin_render_pass)
WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder encoder, const WGPURenderPassDescriptor* descriptor);

WAPI_IMPORT(wapi_wgpu, command_encoder_begin_compute_pass)
WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder encoder, const WGPUComputePassDescriptor* descriptor);

WAPI_IMPORT(wapi_wgpu, command_encoder_copy_buffer_to_buffer)
void wgpuCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder encoder, WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size);

WAPI_IMPORT(wapi_wgpu, command_encoder_copy_buffer_to_texture)
void wgpuCommandEncoderCopyBufferToTexture(WGPUCommandEncoder encoder, const WGPUTexelCopyBufferInfo* source, const WGPUTexelCopyTextureInfo* destination, const WGPUExtent3D* copySize);

WAPI_IMPORT(wapi_wgpu, command_encoder_copy_texture_to_buffer)
void wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder encoder, const WGPUTexelCopyTextureInfo* source, const WGPUTexelCopyBufferInfo* destination, const WGPUExtent3D* copySize);

WAPI_IMPORT(wapi_wgpu, command_encoder_copy_texture_to_texture)
void wgpuCommandEncoderCopyTextureToTexture(WGPUCommandEncoder encoder, const WGPUTexelCopyTextureInfo* source, const WGPUTexelCopyTextureInfo* destination, const WGPUExtent3D* copySize);

WAPI_IMPORT(wapi_wgpu, command_encoder_finish)
WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder encoder, const WGPUCommandBufferDescriptor* descriptor);

/* --- Render Pass --- */
WAPI_IMPORT(wapi_wgpu, render_pass_set_pipeline)
void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder encoder, WGPURenderPipeline pipeline);

WAPI_IMPORT(wapi_wgpu, render_pass_set_bind_group)
void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder encoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t* dynamicOffsets);

WAPI_IMPORT(wapi_wgpu, render_pass_set_vertex_buffer)
void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder encoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size);

WAPI_IMPORT(wapi_wgpu, render_pass_set_index_buffer)
void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder encoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size);

WAPI_IMPORT(wapi_wgpu, render_pass_set_viewport)
void wgpuRenderPassEncoderSetViewport(WGPURenderPassEncoder encoder, float x, float y, float width, float height, float minDepth, float maxDepth);

WAPI_IMPORT(wapi_wgpu, render_pass_set_scissor_rect)
void wgpuRenderPassEncoderSetScissorRect(WGPURenderPassEncoder encoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

WAPI_IMPORT(wapi_wgpu, render_pass_draw)
void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder encoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);

WAPI_IMPORT(wapi_wgpu, render_pass_draw_indexed)
void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder encoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance);

WAPI_IMPORT(wapi_wgpu, render_pass_end)
void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder encoder);

/* --- Compute Pass --- */
WAPI_IMPORT(wapi_wgpu, compute_pass_set_pipeline)
void wgpuComputePassEncoderSetPipeline(WGPUComputePassEncoder encoder, WGPUComputePipeline pipeline);

WAPI_IMPORT(wapi_wgpu, compute_pass_set_bind_group)
void wgpuComputePassEncoderSetBindGroup(WGPUComputePassEncoder encoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t* dynamicOffsets);

WAPI_IMPORT(wapi_wgpu, compute_pass_dispatch_workgroups)
void wgpuComputePassEncoderDispatchWorkgroups(WGPUComputePassEncoder encoder, uint32_t workgroupCountX, uint32_t workgroupCountY, uint32_t workgroupCountZ);

WAPI_IMPORT(wapi_wgpu, compute_pass_end)
void wgpuComputePassEncoderEnd(WGPUComputePassEncoder encoder);

/* --- Buffer --- */
WAPI_IMPORT(wapi_wgpu, buffer_get_mapped_range)
void* wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size);

WAPI_IMPORT(wapi_wgpu, buffer_unmap)
void wgpuBufferUnmap(WGPUBuffer buffer);

/* --- Texture --- */
WAPI_IMPORT(wapi_wgpu, texture_create_view)
WGPUTextureView wgpuTextureCreateView(WGPUTexture texture, const WGPUTextureViewDescriptor* descriptor);

/* --- Release functions --- */
WAPI_IMPORT(wapi_wgpu, buffer_release)
void wgpuBufferRelease(WGPUBuffer buffer);

WAPI_IMPORT(wapi_wgpu, texture_release)
void wgpuTextureRelease(WGPUTexture texture);

WAPI_IMPORT(wapi_wgpu, texture_view_release)
void wgpuTextureViewRelease(WGPUTextureView textureView);

WAPI_IMPORT(wapi_wgpu, sampler_release)
void wgpuSamplerRelease(WGPUSampler sampler);

WAPI_IMPORT(wapi_wgpu, bind_group_release)
void wgpuBindGroupRelease(WGPUBindGroup bindGroup);

WAPI_IMPORT(wapi_wgpu, bind_group_layout_release)
void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout bindGroupLayout);

WAPI_IMPORT(wapi_wgpu, pipeline_layout_release)
void wgpuPipelineLayoutRelease(WGPUPipelineLayout pipelineLayout);

WAPI_IMPORT(wapi_wgpu, shader_module_release)
void wgpuShaderModuleRelease(WGPUShaderModule shaderModule);

WAPI_IMPORT(wapi_wgpu, render_pipeline_release)
void wgpuRenderPipelineRelease(WGPURenderPipeline renderPipeline);

WAPI_IMPORT(wapi_wgpu, compute_pipeline_release)
void wgpuComputePipelineRelease(WGPUComputePipeline computePipeline);

WAPI_IMPORT(wapi_wgpu, command_buffer_release)
void wgpuCommandBufferRelease(WGPUCommandBuffer commandBuffer);

WAPI_IMPORT(wapi_wgpu, command_encoder_release)
void wgpuCommandEncoderRelease(WGPUCommandEncoder commandEncoder);

WAPI_IMPORT(wapi_wgpu, render_pass_release)
void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder encoder);

WAPI_IMPORT(wapi_wgpu, compute_pass_release)
void wgpuComputePassEncoderRelease(WGPUComputePassEncoder encoder);

WAPI_IMPORT(wapi_wgpu, query_set_release)
void wgpuQuerySetRelease(WGPUQuerySet querySet);

#endif /* WEBGPU_H_ */

/* ============================================================
 * Dynamic Function Lookup (Optional)
 * ============================================================
 * For extension functions, runtime-discovered features, or when
 * the module wants to build its own dispatch table. Not needed
 * for standard webgpu.h operations (use direct imports above).
 */

/** Function pointer type for wgpuGetProcAddress */
typedef void (*wapi_gpu_proc_t)(void);

/**
 * Get a WebGPU function pointer by name.
 * This is the WAPI equivalent of wgpuGetProcAddress.
 *
 * Use for extension functions or dynamic dispatch. For standard
 * operations, prefer the direct imports in the wapi_wgpu namespace
 * which compile to direct wasm calls with zero indirection.
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
