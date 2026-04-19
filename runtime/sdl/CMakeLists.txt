cmake_minimum_required(VERSION 3.20)
project(wapi_sdl_runtime C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Source files
# ============================================================

set(SOURCES
    src/main.c

    # SDL3-backed capabilities
    src/wapi_host_capability.c
    src/wapi_host_env.c
    src/wapi_host_memory.c
    src/wapi_host_io.c
    src/wapi_host_clock.c
    src/wapi_host_fs.c
    src/wapi_host_surface.c
    src/wapi_host_window.c
    src/wapi_host_display.c
    src/wapi_host_input.c
    src/wapi_host_audio.c
    src/wapi_host_haptics.c
    src/wapi_host_power.c
    src/wapi_host_sensors.c
    src/wapi_host_transfer.c
    src/wapi_host_seat.c

    # Dawn-backed capabilities
    src/wapi_host_gpu.c
)

# Capabilities SDL3 / Dawn cannot back are intentionally NOT registered.
# Modules that import them will fail at instantiation — apps are expected
# to gate their use behind wapi.cap_supported() queries.

add_executable(wapi_sdl_runtime ${SOURCES})

# ============================================================
# Include paths
# ============================================================

target_include_directories(wapi_sdl_runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================
# SDL3 (windowing / input / audio / clipboard / sensors ... no SDL_GPU)
# ============================================================

find_package(SDL3 REQUIRED CONFIG)
target_link_libraries(wapi_sdl_runtime PRIVATE SDL3::SDL3)

# ============================================================
# WAMR (WebAssembly Micro Runtime)
# ============================================================
# Expected to be added as a submodule or git-fetched source tree.
# Set WAMR_ROOT_DIR to point at the wasm-micro-runtime checkout.
#
# Example: -DWAMR_ROOT_DIR=/path/to/wasm-micro-runtime

if(NOT DEFINED WAMR_ROOT_DIR)
    if(DEFINED ENV{WAMR_ROOT_DIR})
        set(WAMR_ROOT_DIR $ENV{WAMR_ROOT_DIR})
    endif()
endif()

if(NOT WAMR_ROOT_DIR OR NOT EXISTS ${WAMR_ROOT_DIR}/core/iwasm/include/wasm_export.h)
    message(FATAL_ERROR
        "WAMR source not found. Set -DWAMR_ROOT_DIR=<path> to the "
        "wasm-micro-runtime checkout (with core/iwasm/include/wasm_export.h).")
endif()

# Platform selection for WAMR build
if(WIN32)
    set(WAMR_BUILD_PLATFORM "windows")
elseif(APPLE)
    set(WAMR_BUILD_PLATFORM "darwin")
else()
    set(WAMR_BUILD_PLATFORM "linux")
endif()

# Target architecture
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WAMR_BUILD_TARGET "X86_64")
else()
    set(WAMR_BUILD_TARGET "X86_32")
endif()

# Interpreter mode + fast-interp + AOT (via LLVM if available)
set(WAMR_BUILD_INTERP       1)
set(WAMR_BUILD_FAST_INTERP  1)
set(WAMR_BUILD_AOT          1)
set(WAMR_BUILD_JIT          0)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI    0)   # WAPI does not use WASI
set(WAMR_BUILD_MULTI_MODULE 0)
set(WAMR_BUILD_SIMD         1)
set(WAMR_BUILD_REF_TYPES    1)
set(WAMR_BUILD_BULK_MEMORY  1)
set(WAMR_DISABLE_HW_BOUND_CHECK 0)

include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)
add_library(vmlib STATIC ${WAMR_RUNTIME_LIB_SOURCE})
target_include_directories(wapi_sdl_runtime PRIVATE
    ${WAMR_ROOT_DIR}/core/iwasm/include
)
target_link_libraries(wapi_sdl_runtime PRIVATE vmlib)

# ============================================================
# Dawn (WebGPU native, Google)
# ============================================================
# Expects a pre-built Dawn install directory with:
#   include/dawn/webgpu.h
#   include/webgpu/webgpu.h
#   lib/{webgpu_dawn|dawn_native|dawn_proc}.{lib,so,dylib}
#
# Point DAWN_DIR at the install prefix. See Dawn's CMake README.

if(NOT DEFINED DAWN_DIR)
    if(DEFINED ENV{DAWN_DIR})
        set(DAWN_DIR $ENV{DAWN_DIR})
    endif()
endif()

find_path(DAWN_INCLUDE_DIR dawn/webgpu.h
    HINTS ${DAWN_DIR} $ENV{DAWN_DIR}
    PATH_SUFFIXES include
)

# The unified dawn library (statically linked wgpu implementation + proc table).
find_library(DAWN_LIBRARY
    NAMES webgpu_dawn dawn_native
    HINTS ${DAWN_DIR} $ENV{DAWN_DIR}
    PATH_SUFFIXES lib lib64
)

if(NOT DAWN_INCLUDE_DIR OR NOT DAWN_LIBRARY)
    message(FATAL_ERROR
        "Dawn not found. Set -DDAWN_DIR=<path> to a Dawn install prefix.\n"
        "  DAWN_INCLUDE_DIR=${DAWN_INCLUDE_DIR}\n"
        "  DAWN_LIBRARY=${DAWN_LIBRARY}")
endif()

target_include_directories(wapi_sdl_runtime PRIVATE ${DAWN_INCLUDE_DIR})
target_link_libraries(wapi_sdl_runtime PRIVATE ${DAWN_LIBRARY})

# ============================================================
# Platform-specific dependencies
# ============================================================

if(WIN32)
    target_link_libraries(wapi_sdl_runtime PRIVATE
        bcrypt      # BCryptGenRandom / hashing
        ws2_32      # networking (used by WAMR + some hosts)
        d3d12       # Dawn D3D12 backend
        dxgi
        dxguid
    )
    target_compile_definitions(wapi_sdl_runtime PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
elseif(APPLE)
    find_library(COCOA_LIB Cocoa)
    find_library(METAL_LIB Metal)
    find_library(QUARTZCORE_LIB QuartzCore)
    find_library(IOKIT_LIB IOKit)
    find_library(COREFOUNDATION_LIB CoreFoundation)
    find_library(SECURITY_LIB Security)
    target_link_libraries(wapi_sdl_runtime PRIVATE
        ${COCOA_LIB}
        ${METAL_LIB}
        ${QUARTZCORE_LIB}
        ${IOKIT_LIB}
        ${COREFOUNDATION_LIB}
        ${SECURITY_LIB}
    )
elseif(UNIX)
    find_package(OpenSSL)
    if(OpenSSL_FOUND)
        target_link_libraries(wapi_sdl_runtime PRIVATE OpenSSL::Crypto)
    endif()
    target_link_libraries(wapi_sdl_runtime PRIVATE
        m pthread dl
    )
endif()

# ============================================================
# Warnings
# ============================================================

if(MSVC)
    target_compile_options(wapi_sdl_runtime PRIVATE /W3)
else()
    target_compile_options(wapi_sdl_runtime PRIVATE
        -Wall -Wextra -Wno-unused-parameter
    )
endif()

install(TARGETS wapi_sdl_runtime RUNTIME DESTINATION bin)
