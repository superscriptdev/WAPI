cmake_minimum_required(VERSION 3.20)
project(wapi_plugin_clap C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================
# CLAP headers from GitHub
# ============================================================

include(FetchContent)
FetchContent_Declare(clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG        1.2.2
)
FetchContent_MakeAvailable(clap)

# ============================================================
# Wasmtime
# ============================================================

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

# ============================================================
# Plugin shared library
# ============================================================

set(SOURCES
    src/wapi_clap_entry.c
    src/wapi_clap_plugin.c
    src/wapi_clap_process.c
    src/wapi_plugin_host.c
)

add_library(wapi_plugin MODULE ${SOURCES})

target_include_directories(wapi_plugin PRIVATE
    ${clap_SOURCE_DIR}/include
    ${WASMTIME_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(wapi_plugin PRIVATE ${WASMTIME_LIBRARY})

# CLAP plugins use .clap extension (the file is a shared library)
set_target_properties(wapi_plugin PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    C_STANDARD 11
)

# ============================================================
# Platform-specific dependencies
# ============================================================

if(WIN32)
    target_link_libraries(wapi_plugin PRIVATE bcrypt ws2_32)
    target_compile_definitions(wapi_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(APPLE)
    target_link_libraries(wapi_plugin PRIVATE "-framework CoreFoundation")
elseif(UNIX)
    target_link_libraries(wapi_plugin PRIVATE m pthread dl)
endif()

# ============================================================
# Compiler warnings
# ============================================================

if(MSVC)
    target_compile_options(wapi_plugin PRIVATE /W3)
else()
    target_compile_options(wapi_plugin PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ============================================================
# Install
# ============================================================

install(TARGETS wapi_plugin LIBRARY DESTINATION lib/clap)
