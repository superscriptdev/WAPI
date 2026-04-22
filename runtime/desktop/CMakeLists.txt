cmake_minimum_required(VERSION 3.20)
project(wapi_runtime C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================
# Cross-platform sources
# ============================================================

set(CORE_SOURCES
    src/main.c

    src/wapi_host_core.c
    src/wapi_host_env.c
    src/wapi_host_random.c
    src/wapi_host_memory.c
    src/wapi_host_io.c
    src/wapi_host_clock.c
    src/wapi_host_fs.c
    src/wapi_host_net.c
    src/wapi_host_surface.c
    src/wapi_host_window.c
    src/wapi_host_input.c
    src/wapi_host_audio.c
    src/wapi_host_wgpu.c
    src/wapi_host_text.c
    src/wapi_host_transfer.c
    src/wapi_host_seat.c
    src/wapi_host_font.c
    src/wapi_host_video.c
    src/wapi_host_kvstorage.c
    src/wapi_host_crypto.c
    src/wapi_host_compression.c
    src/wapi_host_module.c
    src/wapi_host_notifications.c
    src/wapi_host_permissions.c

    src/third_party/miniz/miniz.c
)

# ============================================================
# Per-platform backend sources
# ============================================================

if(WIN32)
    set(PLAT_SOURCES
        src/platform/win32/wapi_plat_win32.c
        src/platform/win32/wapi_plat_win32_audio.c
        src/platform/win32/wapi_plat_win32_gamepad.c
    )
elseif(APPLE)
    set(PLAT_SOURCES
        src/platform/cocoa/wapi_plat_cocoa.m
        src/platform/cocoa/wapi_plat_cocoa_audio.m
        src/platform/cocoa/wapi_plat_cocoa_gamepad.m
    )
elseif(UNIX)
    set(PLAT_SOURCES
        src/platform/wayland/wapi_plat_wayland.c
        src/platform/wayland/wapi_plat_wayland_audio.c
        src/platform/wayland/wapi_plat_wayland_gamepad.c
    )
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

add_executable(wapi_runtime ${CORE_SOURCES} ${PLAT_SOURCES})

target_include_directories(wapi_runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# ============================================================
# Wasmtime (C API)
# ============================================================

find_path(WASMTIME_INCLUDE_DIR wasmtime.h
    HINTS ${CMAKE_PREFIX_PATH} $ENV{WASMTIME_DIR}
          "C:/Dev/wasmtime/wasmtime-v44.0.0-x86_64-windows-c-api"
    PATH_SUFFIXES include)
find_library(WASMTIME_LIBRARY
    NAMES wasmtime wasmtime.dll
    HINTS ${CMAKE_PREFIX_PATH} $ENV{WASMTIME_DIR}
          "C:/Dev/wasmtime/wasmtime-v44.0.0-x86_64-windows-c-api"
    PATH_SUFFIXES lib)

if(NOT WASMTIME_INCLUDE_DIR OR NOT WASMTIME_LIBRARY)
    message(FATAL_ERROR
        "Wasmtime not found. Set CMAKE_PREFIX_PATH or WASMTIME_DIR.\n"
        "  WASMTIME_INCLUDE_DIR=${WASMTIME_INCLUDE_DIR}\n"
        "  WASMTIME_LIBRARY=${WASMTIME_LIBRARY}")
endif()

target_include_directories(wapi_runtime PRIVATE ${WASMTIME_INCLUDE_DIR})
target_link_libraries(wapi_runtime PRIVATE ${WASMTIME_LIBRARY})

# ============================================================
# WebGPU (wgpu-native or Dawn)
# ============================================================

find_path(WGPU_INCLUDE_DIR webgpu/webgpu.h
    HINTS ${CMAKE_PREFIX_PATH} $ENV{WGPU_DIR} $ENV{DAWN_DIR}
          "C:/Dev/wgpu-native"
    PATH_SUFFIXES include)
find_library(WGPU_LIBRARY
    NAMES wgpu_native webgpu_dawn dawn_native
    HINTS ${CMAKE_PREFIX_PATH} $ENV{WGPU_DIR} $ENV{DAWN_DIR}
          "C:/Dev/wgpu-native"
    PATH_SUFFIXES lib lib64)

if(NOT WGPU_INCLUDE_DIR OR NOT WGPU_LIBRARY)
    message(FATAL_ERROR
        "WebGPU library not found. Set -DWGPU_DIR or -DDAWN_DIR.\n"
        "  WGPU_INCLUDE_DIR=${WGPU_INCLUDE_DIR}\n"
        "  WGPU_LIBRARY=${WGPU_LIBRARY}")
endif()

target_include_directories(wapi_runtime PRIVATE
    ${WGPU_INCLUDE_DIR}
    ${WGPU_INCLUDE_DIR}/webgpu)
target_link_libraries(wapi_runtime PRIVATE ${WGPU_LIBRARY})

# ============================================================
# Platform-specific dependencies
# ============================================================

if(WIN32)
    target_link_libraries(wapi_runtime PRIVATE
        user32
        gdi32
        shell32
        ole32
        uuid
        imm32
        dwmapi
        winmm
        bcrypt
        ws2_32
        xinput
        avrt
        d3dcompiler
        d3d12
        dxgi
        dxguid
        opengl32
        userenv
        ntdll
    )
    target_compile_definitions(wapi_runtime PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
    )

    # Copy runtime DLLs next to the executable
    add_custom_command(TARGET wapi_runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/Dev/wasmtime/wasmtime-v44.0.0-x86_64-windows-c-api/lib/wasmtime.dll"
            "$<TARGET_FILE_DIR:wapi_runtime>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/Dev/wgpu-native/lib/wgpu_native.dll"
            "$<TARGET_FILE_DIR:wapi_runtime>")
elseif(APPLE)
    find_library(COCOA_LIB          Cocoa)
    find_library(METAL_LIB          Metal)
    find_library(QUARTZCORE_LIB     QuartzCore)
    find_library(CORETEXT_LIB       CoreText)
    find_library(COREFOUNDATION_LIB CoreFoundation)
    find_library(COREAUDIO_LIB      CoreAudio)
    find_library(AUDIOTOOLBOX_LIB   AudioToolbox)
    find_library(AUDIOUNIT_LIB      AudioUnit)
    find_library(GAMECONTROLLER_LIB GameController)
    find_library(IOKIT_LIB          IOKit)
    find_library(SECURITY_LIB       Security)
    target_link_libraries(wapi_runtime PRIVATE
        ${COCOA_LIB} ${METAL_LIB} ${QUARTZCORE_LIB} ${CORETEXT_LIB}
        ${COREFOUNDATION_LIB} ${COREAUDIO_LIB} ${AUDIOTOOLBOX_LIB}
        ${AUDIOUNIT_LIB} ${GAMECONTROLLER_LIB} ${IOKIT_LIB} ${SECURITY_LIB})
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-cursor xkbcommon)
    target_include_directories(wapi_runtime PRIVATE ${WAYLAND_INCLUDE_DIRS})
    target_link_libraries(wapi_runtime PRIVATE ${WAYLAND_LIBRARIES})

    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
    pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
    set(XDG_SHELL_XML ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml)
    set(XDG_SHELL_H   ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h)
    set(XDG_SHELL_C   ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c)
    add_custom_command(OUTPUT ${XDG_SHELL_H}
        COMMAND ${WAYLAND_SCANNER} client-header ${XDG_SHELL_XML} ${XDG_SHELL_H})
    add_custom_command(OUTPUT ${XDG_SHELL_C}
        COMMAND ${WAYLAND_SCANNER} private-code ${XDG_SHELL_XML} ${XDG_SHELL_C})
    target_sources(wapi_runtime PRIVATE ${XDG_SHELL_H} ${XDG_SHELL_C})
    target_include_directories(wapi_runtime PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

    pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
    target_include_directories(wapi_runtime PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
    target_link_libraries(wapi_runtime PRIVATE ${PIPEWIRE_LIBRARIES})

    pkg_check_modules(LIBEVDEV REQUIRED libevdev)
    pkg_check_modules(LIBUDEV  REQUIRED libudev)
    target_include_directories(wapi_runtime PRIVATE ${LIBEVDEV_INCLUDE_DIRS} ${LIBUDEV_INCLUDE_DIRS})
    target_link_libraries(wapi_runtime PRIVATE ${LIBEVDEV_LIBRARIES} ${LIBUDEV_LIBRARIES})

    find_package(OpenSSL)
    if(OpenSSL_FOUND)
        target_link_libraries(wapi_runtime PRIVATE OpenSSL::Crypto)
    endif()

    pkg_check_modules(FONTCONFIG fontconfig)
    if(FONTCONFIG_FOUND)
        target_include_directories(wapi_runtime PRIVATE ${FONTCONFIG_INCLUDE_DIRS})
        target_link_libraries(wapi_runtime PRIVATE ${FONTCONFIG_LIBRARIES})
    endif()

    target_link_libraries(wapi_runtime PRIVATE m pthread dl)
endif()

# ============================================================
# Warnings
# ============================================================

if(MSVC)
    target_compile_options(wapi_runtime PRIVATE /W3)
else()
    target_compile_options(wapi_runtime PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

install(TARGETS wapi_runtime RUNTIME DESTINATION bin)
