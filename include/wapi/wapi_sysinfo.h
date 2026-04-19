/**
 * WAPI - System Information
 * Version 1.0.0
 *
 * Query host platform, device class, CPU/hardware capabilities,
 * and display preferences. Provides a portable snapshot of the
 * runtime environment without exposing fingerprint-level detail.
 * Locale and timezone live in wapi_env (user environment).
 *
 * CPU feature detection is granular: modules query specific
 * instruction set extensions (SSE4.1, AVX2, NEON, etc.) just
 * as GPU features are queried granularly via WebGPU. WAPI is
 * not exclusively a Wasm interface -- native targets need to
 * select concrete code paths based on real hardware features.
 *
 * Maps to: Navigator / Screen API (Web),
 *          UIDevice / ProcessInfo (iOS), Build / Configuration (Android),
 *          GetSystemInfo / GetUserDefaultLCID (Windows),
 *          uname / locale (Linux),
 *          SDL_GetCPUCacheLineSize / SDL_GetSystemRAM (SDL3)
 *
 * Import module: "wapi_sysinfo"
 */

#ifndef WAPI_SYSINFO_H
#define WAPI_SYSINFO_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Operating System
 * ============================================================
 * What kernel/OS the module observes. A VM presenting Linux =
 * WAPI_OS_LINUX. See wapi_env_t in wapi_env.h for the environment axis
 * (browser, app sandbox, container) — those are not OSes. */

typedef enum wapi_os_t {
    WAPI_OS_UNKNOWN = 0,
    WAPI_OS_WINDOWS = 1,
    WAPI_OS_MACOS   = 2,
    WAPI_OS_LINUX   = 3,
    WAPI_OS_IOS     = 4,
    WAPI_OS_ANDROID = 5,
    WAPI_OS_OTHER   = 6,
    WAPI_OS_FORCE32 = 0x7FFFFFFF
} wapi_os_t;

/* ============================================================
 * Device Class
 * ============================================================ */

typedef enum wapi_device_class_t {
    WAPI_DEVICE_CLASS_UNKNOWN    = 0,
    WAPI_DEVICE_CLASS_DESKTOP    = 1,
    WAPI_DEVICE_CLASS_LAPTOP     = 2,
    WAPI_DEVICE_CLASS_TABLET     = 3,
    WAPI_DEVICE_CLASS_PHONE      = 4,
    WAPI_DEVICE_CLASS_CONSOLE    = 5,
    WAPI_DEVICE_CLASS_TV         = 6,
    WAPI_DEVICE_CLASS_XR_HEADSET = 7,
    WAPI_DEVICE_CLASS_FORCE32    = 0x7FFFFFFF
} wapi_device_class_t;

/* ============================================================
 * CPU Architecture
 * ============================================================
 * The underlying CPU architecture. For Wasm targets this
 * reports the host CPU, not "wasm32" -- the module may use
 * this for adaptive data layout or to display system info.
 */

typedef enum wapi_cpu_arch_t {
    WAPI_CPU_ARCH_UNKNOWN = 0,
    WAPI_CPU_ARCH_X86     = 1,  /* 32-bit x86 */
    WAPI_CPU_ARCH_X86_64  = 2,  /* 64-bit x86 */
    WAPI_CPU_ARCH_ARM     = 3,  /* 32-bit ARM */
    WAPI_CPU_ARCH_ARM64   = 4,  /* 64-bit ARM (AArch64) */
    WAPI_CPU_ARCH_RISCV64 = 5,  /* 64-bit RISC-V */
    WAPI_CPU_ARCH_WASM32  = 6,  /* Wasm (no native CPU info available) */
    WAPI_CPU_ARCH_FORCE32 = 0x7FFFFFFF
} wapi_cpu_arch_t;

/* ============================================================
 * CPU Feature Flags (64-bit bitmask)
 * ============================================================
 * Granular instruction set detection, grouped by architecture.
 * A host sets every flag the hardware actually supports.
 * Flags from other architectures read as 0.
 *
 * This mirrors how WebGPU exposes granular GPU features --
 * the module checks exactly what it needs and picks code paths.
 */

/* --- x86 / x86_64 --- */
#define WAPI_CPU_X86_SSE        ((uint64_t)1 <<  0)
#define WAPI_CPU_X86_SSE2       ((uint64_t)1 <<  1)
#define WAPI_CPU_X86_SSE3       ((uint64_t)1 <<  2)
#define WAPI_CPU_X86_SSE41      ((uint64_t)1 <<  3)
#define WAPI_CPU_X86_SSE42      ((uint64_t)1 <<  4)
#define WAPI_CPU_X86_AVX        ((uint64_t)1 <<  5)
#define WAPI_CPU_X86_AVX2       ((uint64_t)1 <<  6)
#define WAPI_CPU_X86_AVX512F    ((uint64_t)1 <<  7)  /* AVX-512 Foundation */
#define WAPI_CPU_X86_FMA        ((uint64_t)1 <<  8)
#define WAPI_CPU_X86_BMI1       ((uint64_t)1 <<  9)
#define WAPI_CPU_X86_BMI2       ((uint64_t)1 << 10)
#define WAPI_CPU_X86_POPCNT     ((uint64_t)1 << 11)
#define WAPI_CPU_X86_LZCNT      ((uint64_t)1 << 12)
#define WAPI_CPU_X86_F16C       ((uint64_t)1 << 13)
#define WAPI_CPU_X86_PCLMULQDQ  ((uint64_t)1 << 14)
#define WAPI_CPU_X86_MMX        ((uint64_t)1 << 15)

/* --- ARM / AArch64 --- */
#define WAPI_CPU_ARM_NEON       ((uint64_t)1 << 20)
#define WAPI_CPU_ARM_SVE        ((uint64_t)1 << 21)
#define WAPI_CPU_ARM_SVE2       ((uint64_t)1 << 22)
#define WAPI_CPU_ARM_DOTPROD    ((uint64_t)1 << 23)  /* SDOT/UDOT */
#define WAPI_CPU_ARM_FP16       ((uint64_t)1 << 24)  /* Half-precision float */
#define WAPI_CPU_ARM_I8MM       ((uint64_t)1 << 25)  /* Int8 matrix multiply */
#define WAPI_CPU_ARM_BF16       ((uint64_t)1 << 26)  /* BFloat16 */

/* --- RISC-V --- */
#define WAPI_CPU_RV_V           ((uint64_t)1 << 32)  /* Vector extension */
#define WAPI_CPU_RV_B           ((uint64_t)1 << 33)  /* Bit manipulation */
#define WAPI_CPU_RV_ZBKB        ((uint64_t)1 << 34)  /* Crypto bitmanip */

/* --- Cross-architecture crypto / hash --- */
#define WAPI_CPU_HW_AES         ((uint64_t)1 << 40)  /* AES-NI / ARMv8-CE / etc. */
#define WAPI_CPU_HW_SHA1        ((uint64_t)1 << 41)  /* SHA-1 acceleration */
#define WAPI_CPU_HW_SHA256      ((uint64_t)1 << 42)  /* SHA-256 acceleration */
#define WAPI_CPU_HW_CRC32       ((uint64_t)1 << 43)  /* CRC32 acceleration */

/* --- Wasm features (when running in a Wasm host) --- */
#define WAPI_CPU_WASM_SIMD128   ((uint64_t)1 << 48)  /* 128-bit SIMD */
#define WAPI_CPU_WASM_RELAXED   ((uint64_t)1 << 49)  /* Relaxed SIMD */
#define WAPI_CPU_WASM_THREADS   ((uint64_t)1 << 50)  /* Threads + shared memory */
#define WAPI_CPU_WASM_BULK_MEM  ((uint64_t)1 << 51)  /* Bulk memory ops */
#define WAPI_CPU_WASM_TAIL_CALL ((uint64_t)1 << 52)  /* Tail calls */

/* ============================================================
 * System Info
 * ============================================================
 *
 * Layout (128 bytes, align 8):
 *   Offset   0: uint32_t os                  wapi_os_t
 *   Offset   4: uint32_t device_class        wapi_device_class_t
 *   Offset   8: uint64_t os_version_ptr      Linear memory address of UTF-8 version string
 *   Offset  16: uint64_t cpu_features        Bitmask of WAPI_CPU_* flags
 *   Offset  24: uint32_t os_version_len      Byte length of version string
 *   Offset  28: uint32_t cpu_count           Number of logical CPU cores
 *   Offset  32: uint32_t physical_cpu_count  Physical (non-HT) cores
 *   Offset  36: uint32_t ram_mb              Approximate RAM in megabytes
 *   Offset  40: uint32_t cpu_arch            wapi_cpu_arch_t
 *   Offset  44: uint32_t cache_line_size     CPU cache line in bytes (typically 64)
 *   Offset  48: uint32_t page_size           System memory page in bytes (typically 4096)
 *   Offset  52: uint32_t dark_mode           1 if dark mode enabled
 *   Offset  56: uint32_t accent_color_rgba   Accent color as 0xRRGGBBAA
 *   Offset  60: uint32_t env                 wapi_env_t (see wapi_env.h)
 *   Offset  64: uint32_t is_remote           1 if RDP/SSH/VNC/remote desktop
 *   Offset  68: uint8_t  _reserved[60]
 */

typedef struct wapi_sysinfo_t {
    uint32_t    os;
    uint32_t    device_class;
    uint64_t    os_version_ptr;
    uint64_t    cpu_features;
    uint32_t    os_version_len;
    uint32_t    cpu_count;
    uint32_t    physical_cpu_count;
    uint32_t    ram_mb;
    uint32_t    cpu_arch;
    uint32_t    cache_line_size;
    uint32_t    page_size;
    uint32_t    dark_mode;
    uint32_t    accent_color_rgba;
    uint32_t    env;
    uint32_t    is_remote;
    uint8_t     _reserved[60];
} wapi_sysinfo_t;

_Static_assert(sizeof(wapi_sysinfo_t) == 128,
               "wapi_sysinfo_t must be 128 bytes");
_Static_assert(_Alignof(wapi_sysinfo_t) == 8,
               "wapi_sysinfo_t must be 8-byte aligned");

/* ============================================================
 * System Info Functions
 * ============================================================ */

/**
 * Get a snapshot of system information.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sysinfo, sysinfo_get)
wapi_result_t wapi_sysinfo_get(wapi_sysinfo_t* info_ptr);

/* ============================================================
 * Host Info (Escape Hatch)
 * ============================================================
 * Key/value escape hatch for platform knowledge not covered by
 * the typed struct. Prefer capability queries or sysinfo_get;
 * use host_get only for workarounds, analytics, or
 * platform-appropriate UI conventions.
 *
 * Well-known keys (hosts SHOULD populate these):
 *   "os.family"         "windows", "macos", "linux", "android", "ios", "browser"
 *   "os.version"        "10.0.26200", "15.2", etc.
 *   "runtime.name"      "wapi-desktop", "wapi-browser", etc.
 *   "runtime.version"   Semver string
 *   "device.form"       "desktop", "mobile", "tablet", "embedded", "xr"
 *   "browser.engine"    "chromium", "gecko", "webkit" (browser hosts only)
 *
 * Unknown keys return WAPI_ERR_NOENT. Hosts may define additional
 * keys under "vendor.<name>.*".
 *
 * Wasm signature: (i32, i32, i64, i32) -> i32
 */
WAPI_IMPORT(wapi_sysinfo, host_get)
wapi_result_t wapi_sysinfo_host_get(wapi_stringview_t key,
                                    char* buf, wapi_size_t buf_len,
                                    wapi_size_t* val_len);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SYSINFO_H */
