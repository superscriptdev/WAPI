/**
 * WAPI - GPU (WebGPU) surface source binding
 * Version 2.0.0
 *
 * WAPI does not re-export the WebGPU API. Guests include webgpu.h
 * directly and use the standard wgpu* calls. The only WAPI-specific
 * addition is a chained `WGPUSurfaceDescriptor.nextInChain` struct
 * that references a WAPI surface handle instead of a platform-native
 * HWND / CAMetalLayer / wl_surface.
 *
 * Usage (guest):
 *
 *     wapi_handle_t wapi_surface = ...;  // from wapi_surface_create
 *
 *     wapi_gpu_surface_source_t src = {
 *         .chain   = { .next = 0, .sType = WAPI_STYPE_GPU_SURFACE_SOURCE_WAPI },
 *         .surface = wapi_surface,
 *     };
 *     WGPUSurfaceDescriptor sd = {
 *         .nextInChain = (const WGPUChainedStruct*)&src,
 *         .label       = { .data = "triangle", .length = 8 },
 *     };
 *     WGPUSurface wgpu_surface = wgpuInstanceCreateSurface(instance, &sd);
 *
 * The host recognises the `WAPI_STYPE_GPU_SURFACE_SOURCE_WAPI` sType
 * and rebuilds the appropriate platform-native surface source
 * (`WGPUSurfaceSourceWindowsHWND`, `WGPUSurfaceSourceMetalLayer`,
 * `WGPUSurfaceSourceWaylandSurface`) before calling the real
 * `wgpuInstanceCreateSurface`.
 */

#ifndef WAPI_GPU_H
#define WAPI_GPU_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Chained surface source that binds a WGPUSurface to a WAPI surface
 * handle. Attached via `WGPUSurfaceDescriptor.nextInChain`, sType
 * tag = `WAPI_STYPE_GPU_SURFACE_SOURCE_WAPI`.
 *
 * Layout (24 bytes, align 8):
 *   Offset  0: wapi_chain_t chain   (16 B)
 *   Offset 16: wapi_handle_t surface
 *   Offset 20: uint32_t _pad
 */
typedef struct wapi_gpu_surface_source_t {
    wapi_chain_t  chain;
    wapi_handle_t surface;
    uint32_t      _pad;
} wapi_gpu_surface_source_t;

#ifdef __cplusplus
}
#endif

#endif /* WAPI_GPU_H */
