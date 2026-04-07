cmake_minimum_required(VERSION 3.20)
project(wapi_runtime C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================
# Source files
# ============================================================

set(SOURCES
    src/main.c
    src/wapi_host_capability.c
    src/wapi_host_env.c
    src/wapi_host_memory.c
    src/wapi_host_io.c
    src/wapi_host_clock.c
    src/wapi_host_fs.c
    src/wapi_host_net.c
    src/wapi_host_surface.c
    src/wapi_host_event.c
    src/wapi_host_input.c
    src/wapi_host_audio.c
    src/wapi_host_gpu.c
    src/wapi_host_content.c
    src/wapi_host_text.c
    src/wapi_host_clipboard.c
    src/wapi_host_font.c
    src/wapi_host_video.c
    src/wapi_host_kv_storage.c
    src/wapi_host_crypto.c
    src/wapi_host_module.c
    src/wapi_host_notifications.c
    src/wapi_host_geolocation.c
    src/wapi_host_sensors.c
    src/wapi_host_speech.c
    src/wapi_host_biometric.c
    src/wapi_host_share.c
    src/wapi_host_payments.c
    src/wapi_host_usb.c
    src/wapi_host_midi.c
    src/wapi_host_bluetooth.c
    src/wapi_host_camera.c
    src/wapi_host_xr.c
    src/wapi_host_register.c
    src/wapi_host_taskbar.c
    src/wapi_host_permissions.c
    src/wapi_host_wake_lock.c
    src/wapi_host_orientation.c
    src/wapi_host_codec.c
    src/wapi_host_compression.c
    src/wapi_host_media_session.c
    src/wapi_host_media_caps.c
    src/wapi_host_encoding.c
    src/wapi_host_authn.c
    src/wapi_host_network_info.c
    src/wapi_host_battery.c
    src/wapi_host_idle.c
    src/wapi_host_haptics.c
    src/wapi_host_p2p.c
    src/wapi_host_hid.c
    src/wapi_host_serial.c
    src/wapi_host_screen_capture.c
    src/wapi_host_contacts.c
    src/wapi_host_barcode.c
    src/wapi_host_nfc.c
    src/wapi_host_dnd.c
)

add_executable(wapi_runtime ${SOURCES})

# ============================================================
# Include paths
# ============================================================

# WAPI ABI headers (wapi_types.h, wapi_capability.h, etc.)
target_include_directories(wapi_runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# ============================================================
# SDL3
# ============================================================

find_package(SDL3 REQUIRED CONFIG)
target_link_libraries(wapi_runtime PRIVATE SDL3::SDL3)

# ============================================================
# Wasmtime
# ============================================================
# Wasmtime C API can be found via pkg-config or manual paths.
# The user should set CMAKE_PREFIX_PATH to include the wasmtime
# install directory (containing include/wasmtime.h and lib/).

find_path(WASMTIME_INCLUDE_DIR wasmtime.h
    HINTS
        ${CMAKE_PREFIX_PATH}
        $ENV{WASMTIME_DIR}
    PATH_SUFFIXES include
)

find_library(WASMTIME_LIBRARY
    NAMES wasmtime
    HINTS
        ${CMAKE_PREFIX_PATH}
        $ENV{WASMTIME_DIR}
    PATH_SUFFIXES lib
)

if(NOT WASMTIME_INCLUDE_DIR OR NOT WASMTIME_LIBRARY)
    message(FATAL_ERROR
        "Wasmtime not found. Set CMAKE_PREFIX_PATH or WASMTIME_DIR.\n"
        "  WASMTIME_INCLUDE_DIR=${WASMTIME_INCLUDE_DIR}\n"
        "  WASMTIME_LIBRARY=${WASMTIME_LIBRARY}")
endif()

target_include_directories(wapi_runtime PRIVATE ${WASMTIME_INCLUDE_DIR})
target_link_libraries(wapi_runtime PRIVATE ${WASMTIME_LIBRARY})

# ============================================================
# wgpu-native
# ============================================================
# Expects webgpu.h and wgpu.h in the include path,
# and libwgpu_native in the lib path.

find_path(WGPU_INCLUDE_DIR webgpu.h
    HINTS
        ${CMAKE_PREFIX_PATH}
        $ENV{WGPU_DIR}
    PATH_SUFFIXES include
)

find_library(WGPU_LIBRARY
    NAMES wgpu_native
    HINTS
        ${CMAKE_PREFIX_PATH}
        $ENV{WGPU_DIR}
    PATH_SUFFIXES lib
)

if(NOT WGPU_INCLUDE_DIR OR NOT WGPU_LIBRARY)
    message(FATAL_ERROR
        "wgpu-native not found. Set CMAKE_PREFIX_PATH or WGPU_DIR.\n"
        "  WGPU_INCLUDE_DIR=${WGPU_INCLUDE_DIR}\n"
        "  WGPU_LIBRARY=${WGPU_LIBRARY}")
endif()

target_include_directories(wapi_runtime PRIVATE ${WGPU_INCLUDE_DIR})
target_link_libraries(wapi_runtime PRIVATE ${WGPU_LIBRARY})

# ============================================================
# Platform-specific dependencies
# ============================================================

if(WIN32)
    target_link_libraries(wapi_runtime PRIVATE
        bcrypt      # For BCryptGenRandom + crypto
        ws2_32      # For networking + wgpu-native
        userenv     # For wgpu-native
        ntdll       # For wgpu-native
        d3dcompiler # For wgpu-native DX12 backend
        opengl32    # For wgpu-native OpenGL fallback
        gdi32       # For font enumeration (EnumFontFamiliesEx)
    )
    # Suppress CRT deprecation warnings on MSVC
    target_compile_definitions(wapi_runtime PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
elseif(APPLE)
    find_library(COCOA_LIB Cocoa)
    find_library(METAL_LIB Metal)
    find_library(QUARTZCORE_LIB QuartzCore)
    find_library(CORETEXT_LIB CoreText)
    find_library(COREFOUNDATION_LIB CoreFoundation)
    find_library(SECURITY_LIB Security)
    target_link_libraries(wapi_runtime PRIVATE
        ${COCOA_LIB}
        ${METAL_LIB}
        ${QUARTZCORE_LIB}
        ${CORETEXT_LIB}
        ${COREFOUNDATION_LIB}
        ${SECURITY_LIB}
    )
elseif(UNIX)
    # fontconfig for font enumeration
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(FONTCONFIG fontconfig)
    endif()
    if(FONTCONFIG_FOUND)
        target_include_directories(wapi_runtime PRIVATE ${FONTCONFIG_INCLUDE_DIRS})
        target_link_libraries(wapi_runtime PRIVATE ${FONTCONFIG_LIBRARIES})
    endif()

    # OpenSSL for crypto
    find_package(OpenSSL)
    if(OpenSSL_FOUND)
        target_link_libraries(wapi_runtime PRIVATE OpenSSL::Crypto)
    endif()

    target_link_libraries(wapi_runtime PRIVATE
        m           # Math
        pthread     # Threading
        dl          # Dynamic loading
    )
endif()

# ============================================================
# Compiler warnings
# ============================================================

if(MSVC)
    target_compile_options(wapi_runtime PRIVATE /W3)
else()
    target_compile_options(wapi_runtime PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ============================================================
# Install
# ============================================================

install(TARGETS wapi_runtime RUNTIME DESTINATION bin)
