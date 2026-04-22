/**
 * WAPI Desktop Runtime - System Information (wapi_sysinfo.h)
 *
 * Synchronous snapshot imports. The struct is 128B and carries a
 * UTF-8 OS version string that we stash in guest memory via the
 * shared-memory pool helpers so the guest gets a live pointer.
 */

#include "wapi_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>

/* Mirrors of wapi_sysinfo.h enums. */
#define WAPI_OS_WINDOWS       1
#define WAPI_CPU_ARCH_X86_64  2
#define WAPI_CPU_ARCH_X86     1
#define WAPI_CPU_ARCH_ARM64   4

#define CPU_X86_SSE        ((uint64_t)1 <<  0)
#define CPU_X86_SSE2       ((uint64_t)1 <<  1)
#define CPU_X86_SSE3       ((uint64_t)1 <<  2)
#define CPU_X86_SSE41      ((uint64_t)1 <<  3)
#define CPU_X86_SSE42      ((uint64_t)1 <<  4)
#define CPU_X86_AVX        ((uint64_t)1 <<  5)
#define CPU_X86_AVX2       ((uint64_t)1 <<  6)
#define CPU_X86_AVX512F    ((uint64_t)1 <<  7)
#define CPU_X86_FMA        ((uint64_t)1 <<  8)
#define CPU_X86_BMI1       ((uint64_t)1 <<  9)
#define CPU_X86_BMI2       ((uint64_t)1 << 10)
#define CPU_X86_POPCNT     ((uint64_t)1 << 11)
#define CPU_X86_LZCNT      ((uint64_t)1 << 12)
#define CPU_X86_F16C       ((uint64_t)1 << 13)
#define CPU_X86_PCLMULQDQ  ((uint64_t)1 << 14)
#define CPU_HW_AES         ((uint64_t)1 << 40)
#define CPU_HW_SHA1        ((uint64_t)1 << 41)
#define CPU_HW_SHA256      ((uint64_t)1 << 42)
#define CPU_HW_CRC32       ((uint64_t)1 << 43)

#define WAPI_CONTRAST_NORMAL 0

/* --- CPUID feature mask (x86/x64) --- */

static uint64_t cpuid_features(void) {
#if defined(_M_X64) || defined(_M_IX86)
    int regs[4];
    uint64_t f = 0;

    __cpuid(regs, 0);
    int max_std = regs[0];

    __cpuid(regs, 1);
    uint32_t ecx1 = (uint32_t)regs[2];
    uint32_t edx1 = (uint32_t)regs[3];
    if (edx1 & (1u << 25)) f |= CPU_X86_SSE;
    if (edx1 & (1u << 26)) f |= CPU_X86_SSE2;
    if (ecx1 & (1u <<  0)) f |= CPU_X86_SSE3;
    if (ecx1 & (1u << 19)) f |= CPU_X86_SSE41;
    if (ecx1 & (1u << 20)) f |= CPU_X86_SSE42;
    if (ecx1 & (1u << 28)) f |= CPU_X86_AVX;
    if (ecx1 & (1u << 12)) f |= CPU_X86_FMA;
    if (ecx1 & (1u << 23)) f |= CPU_X86_POPCNT;
    if (ecx1 & (1u << 29)) f |= CPU_X86_F16C;
    if (ecx1 & (1u <<  1)) f |= CPU_X86_PCLMULQDQ;
    if (ecx1 & (1u << 25)) f |= CPU_HW_AES;

    if (max_std >= 7) {
        __cpuidex(regs, 7, 0);
        uint32_t ebx7 = (uint32_t)regs[1];
        uint32_t ecx7 = (uint32_t)regs[2];
        (void)ecx7;
        if (ebx7 & (1u <<  3)) f |= CPU_X86_BMI1;
        if (ebx7 & (1u <<  5)) f |= CPU_X86_AVX2;
        if (ebx7 & (1u <<  8)) f |= CPU_X86_BMI2;
        if (ebx7 & (1u << 16)) f |= CPU_X86_AVX512F;
        if (ebx7 & (1u << 29)) f |= CPU_HW_SHA256 | CPU_HW_SHA1;
    }

    __cpuid(regs, 0x80000000);
    unsigned max_ext = (unsigned)regs[0];
    if (max_ext >= 0x80000001) {
        __cpuid(regs, 0x80000001);
        uint32_t ecxe = (uint32_t)regs[2];
        if (ecxe & (1u <<  5)) f |= CPU_X86_LZCNT;
    }
    /* SSE4.2 implies CRC32 on x86. */
    if (f & CPU_X86_SSE42) f |= CPU_HW_CRC32;
    return f;
#else
    return 0;
#endif
}

/* --- RAM (rounded MB) --- */

static uint32_t total_ram_mb(void) {
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) return 0;
    return (uint32_t)(ms.ullTotalPhys / (1024ULL * 1024ULL));
}

static uint32_t page_size_bytes(void) {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    return (uint32_t)si.dwPageSize;
}

static uint32_t logical_cpu_count(void) {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    return (uint32_t)si.dwNumberOfProcessors;
}

/* Physical core count via GetLogicalProcessorInformation. */
static uint32_t physical_cpu_count(void) {
    DWORD len = 0;
    GetLogicalProcessorInformation(NULL, &len);
    if (len == 0) return logical_cpu_count();
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buf =
        (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(len);
    if (!buf) return logical_cpu_count();
    uint32_t phys = 0;
    if (GetLogicalProcessorInformation(buf, &len)) {
        DWORD n = len / sizeof(*buf);
        for (DWORD i = 0; i < n; i++) {
            if (buf[i].Relationship == RelationProcessorCore) phys++;
        }
    }
    free(buf);
    return phys ? phys : logical_cpu_count();
}

static uint32_t cpu_cache_line(void) {
    DWORD len = 0;
    GetLogicalProcessorInformation(NULL, &len);
    if (len == 0) return 64;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buf =
        (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(len);
    if (!buf) return 64;
    uint32_t line = 0;
    if (GetLogicalProcessorInformation(buf, &len)) {
        DWORD n = len / sizeof(*buf);
        for (DWORD i = 0; i < n; i++) {
            if (buf[i].Relationship == RelationCache &&
                buf[i].Cache.Level == 1) {
                line = buf[i].Cache.LineSize;
                break;
            }
        }
    }
    free(buf);
    return line ? line : 64;
}

/* OS version string (e.g. "10.0.26200"). Uses RtlGetVersion to bypass
 * the compatibility-shim version lie that GetVersionExW serves. */
typedef LONG (WINAPI *rtl_get_version_t)(OSVERSIONINFOW*);

static uint32_t os_version_str(char* buf, uint32_t buf_cap) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return 0;
    rtl_get_version_t RtlGetVersion =
        (rtl_get_version_t)GetProcAddress(ntdll, "RtlGetVersion");
    OSVERSIONINFOW ov = {0};
    ov.dwOSVersionInfoSize = sizeof(ov);
    if (!RtlGetVersion || RtlGetVersion(&ov) != 0) return 0;
    int n = _snprintf_s(buf, buf_cap, _TRUNCATE, "%lu.%lu.%lu",
                        ov.dwMajorVersion, ov.dwMinorVersion, ov.dwBuildNumber);
    return n > 0 ? (uint32_t)n : 0;
}

/* Dark-mode + accent-color queries reused by wapi_theme; keep local
 * static so the theme module can share via forward decls. */
static uint32_t win_dark_mode(void) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    DWORD v = 1, sz = sizeof(v);
    DWORD dark = 0;
    if (RegQueryValueExW(k, L"AppsUseLightTheme", NULL, NULL,
                         (LPBYTE)&v, &sz) == ERROR_SUCCESS) {
        dark = (v == 0) ? 1u : 0u;
    }
    RegCloseKey(k);
    return dark;
}

static uint32_t win_accent_rgba(void) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return 0;
    typedef HRESULT (WINAPI *fn_t)(DWORD*, BOOL*);
    fn_t DwmGetColorizationColor = (fn_t)GetProcAddress(dwm, "DwmGetColorizationColor");
    uint32_t rgba = 0;
    if (DwmGetColorizationColor) {
        DWORD color = 0; BOOL op = FALSE;
        if (SUCCEEDED(DwmGetColorizationColor(&color, &op))) {
            /* color is 0xAARRGGBB; convert to 0xRRGGBBAA. */
            uint8_t a = (uint8_t)(color >> 24);
            uint8_t r = (uint8_t)(color >> 16);
            uint8_t g = (uint8_t)(color >>  8);
            uint8_t b = (uint8_t)(color      );
            rgba = ((uint32_t)r      ) | ((uint32_t)g <<  8) |
                   ((uint32_t)b << 16) | ((uint32_t)a << 24);
        }
    }
    return rgba;
}

/* Shared with theme host. */
uint32_t wapi_host_win_dark_mode(void)  { return win_dark_mode(); }
uint32_t wapi_host_win_accent_rgba(void){ return win_accent_rgba(); }

/* Session is remote (RDP / Quick Assist) */
static uint32_t is_remote_session(void) {
    return GetSystemMetrics(SM_REMOTESESSION) ? 1u : 0u;
}

/* ============================================================
 * sysinfo_get
 * ============================================================ */

static wasm_trap_t* h_sysinfo_get(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t info_ptr = WAPI_ARG_U32(0);

    uint8_t buf[128]; memset(buf, 0, sizeof(buf));

    uint32_t os = WAPI_OS_WINDOWS;
    uint32_t dev_class = 1; /* DESKTOP — laptop detection is noisy; DESKTOP is the safe default */

    /* Arch */
    uint32_t arch;
    SYSTEM_INFO si; GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64: arch = WAPI_CPU_ARCH_X86_64; break;
    case PROCESSOR_ARCHITECTURE_INTEL: arch = WAPI_CPU_ARCH_X86;    break;
    case PROCESSOR_ARCHITECTURE_ARM64: arch = WAPI_CPU_ARCH_ARM64;  break;
    default:                           arch = 0;                    break;
    }

    uint64_t cpu_features = cpuid_features();
    uint32_t cpu_count = logical_cpu_count();
    uint32_t phys = physical_cpu_count();
    uint32_t ram  = total_ram_mb();
    uint32_t cls  = cpu_cache_line();
    uint32_t ps   = page_size_bytes();
    uint32_t dark = win_dark_mode();
    uint32_t acc  = win_accent_rgba();
    uint32_t remote = is_remote_session();

    /* OS version string goes into guest memory at a stable offset inside
     * the result struct — append after the 128B header is impossible
     * because the buffer is fixed-size. Instead, reuse the shared-mem
     * pool: we write the struct with os_version_ptr = 0 / len = 0 and
     * let wapi_sysinfo_host_get supply the string on demand. Guests
     * that want the version call host_get("os.version"). */
    (void)os_version_str;

    uint32_t zero32 = 0;
    uint64_t zero64 = 0;
    memcpy(buf +  0, &os,           4);
    memcpy(buf +  4, &dev_class,    4);
    memcpy(buf +  8, &zero64,       8); /* os_version_ptr */
    memcpy(buf + 16, &cpu_features, 8);
    memcpy(buf + 24, &zero32,       4); /* os_version_len */
    memcpy(buf + 28, &cpu_count,    4);
    memcpy(buf + 32, &phys,         4);
    memcpy(buf + 36, &ram,          4);
    memcpy(buf + 40, &arch,         4);
    memcpy(buf + 44, &cls,          4);
    memcpy(buf + 48, &ps,           4);
    memcpy(buf + 52, &dark,         4);
    memcpy(buf + 56, &acc,          4);
    memcpy(buf + 60, &zero32,       4); /* env (see wapi_env.h; 0 = desktop/native) */
    memcpy(buf + 64, &remote,       4);

    if (!wapi_wasm_write_bytes(info_ptr, buf, sizeof(buf))) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * host_get — key/value escape hatch
 * ============================================================ */

static wasm_trap_t* h_host_get(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    /* (stringview key, char* buf, size_t buf_len, size_t* val_len) */
    uint32_t sv_ptr   = WAPI_ARG_U32(0);
    uint32_t buf_ptr  = WAPI_ARG_U32(1);
    uint64_t buf_len  = WAPI_ARG_U64(2);
    uint32_t len_ptr  = WAPI_ARG_U32(3);

    uint8_t* sv = (uint8_t*)wapi_wasm_ptr(sv_ptr, 16);
    if (!sv) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    uint64_t data, klen;
    memcpy(&data, sv + 0, 8);
    memcpy(&klen, sv + 8, 8);
    if (klen == 0 || klen > 128) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    const char* key = (const char*)wapi_wasm_ptr((uint32_t)data, (uint32_t)klen);
    if (!key) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    char value[128];
    size_t vlen = 0;
    if (klen == 9 && memcmp(key, "os.family", 9) == 0) {
        vlen = strlen(strcpy(value, "windows"));
    } else if (klen == 10 && memcmp(key, "os.version", 10) == 0) {
        vlen = os_version_str(value, sizeof(value));
    } else if (klen == 12 && memcmp(key, "runtime.name", 12) == 0) {
        vlen = strlen(strcpy(value, "wapi-desktop"));
    } else if (klen == 15 && memcmp(key, "runtime.version", 15) == 0) {
        vlen = strlen(strcpy(value, "0.1.0"));
    } else if (klen == 11 && memcmp(key, "device.form", 11) == 0) {
        vlen = strlen(strcpy(value, "desktop"));
    } else {
        WAPI_RET_I32(WAPI_ERR_NOENT); return NULL;
    }

    if (len_ptr) wapi_wasm_write_u64(len_ptr, (uint64_t)vlen);
    if (vlen > 0 && buf_len > 0) {
        uint32_t copy = (uint32_t)(vlen < buf_len ? vlen : buf_len);
        wapi_wasm_write_bytes(buf_ptr, value, copy);
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

void wapi_host_register_sysinfo(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_sysinfo", "sysinfo_get", h_sysinfo_get);
    /* host_get: (i32 sv_ptr, i32 buf, i64 buf_len, i32 len_out) -> i32 */
    wapi_linker_define(linker, "wapi_sysinfo", "host_get", h_host_get,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
}
