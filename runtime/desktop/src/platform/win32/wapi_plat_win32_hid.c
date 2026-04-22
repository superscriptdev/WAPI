/**
 * WAPI Desktop Runtime - Win32 Raw HID Backend
 *
 * Enumerates connected HID devices via SetupDi + HidD_* and exposes
 * blocking report I/O via ReadFile/WriteFile on the device handle.
 *
 * Scope: raw HID only. Gamepad mapping (via SDL_GameControllerDB) is
 * layered on top in wapi_host_input.c; this file just serves the
 * wapi_plat_hid_* contract in wapi_plat.h.
 *
 * No hidapi dependency: we talk to hid.dll directly. SetupAPI gives
 * us the enumeration; hidclass interface GUID comes from HidD_GetHidGuid.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "wapi_plat.h"

#include <windows.h>
#include <setupapi.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment(lib, "setupapi")

/* --- Local copies of HID SDK types so we don't require hidsdi.h
 *     on every SDK variant. hid.dll / hidclass.sys is stable. --- */
typedef struct _WAPI_HIDD_ATTRIBUTES {
    ULONG  Size;
    USHORT VendorID;
    USHORT ProductID;
    USHORT VersionNumber;
} WAPI_HIDD_ATTRIBUTES;

typedef struct _WAPI_HIDP_CAPS {
    USHORT Usage;
    USHORT UsagePage;
    USHORT InputReportByteLength;
    USHORT OutputReportByteLength;
    USHORT FeatureReportByteLength;
    USHORT Reserved[17];
    USHORT NumberLinkCollectionNodes;
    USHORT NumberInputButtonCaps;
    USHORT NumberInputValueCaps;
    USHORT NumberInputDataIndices;
    USHORT NumberOutputButtonCaps;
    USHORT NumberOutputValueCaps;
    USHORT NumberOutputDataIndices;
    USHORT NumberFeatureButtonCaps;
    USHORT NumberFeatureValueCaps;
    USHORT NumberFeatureDataIndices;
} WAPI_HIDP_CAPS;

/* --- Dynamically-resolved hid.dll entrypoints --- */
typedef void (__stdcall *fn_HidD_GetHidGuid_t)(LPGUID);
typedef BOOLEAN (__stdcall *fn_HidD_GetAttributes_t)(HANDLE, WAPI_HIDD_ATTRIBUTES*);
typedef BOOLEAN (__stdcall *fn_HidD_GetPreparsedData_t)(HANDLE, void**);
typedef BOOLEAN (__stdcall *fn_HidD_FreePreparsedData_t)(void*);
typedef BOOLEAN (__stdcall *fn_HidD_GetProductString_t)(HANDLE, PVOID, ULONG);
typedef BOOLEAN (__stdcall *fn_HidD_GetManufacturerString_t)(HANDLE, PVOID, ULONG);
typedef BOOLEAN (__stdcall *fn_HidD_SetFeature_t)(HANDLE, PVOID, ULONG);
typedef BOOLEAN (__stdcall *fn_HidD_GetFeature_t)(HANDLE, PVOID, ULONG);
typedef LONG    (__stdcall *fn_HidP_GetCaps_t)(void*, WAPI_HIDP_CAPS*);

static struct {
    bool initialized;
    bool available;
    HMODULE hid_dll;
    GUID    hid_guid;
    fn_HidD_GetHidGuid_t             HidD_GetHidGuid;
    fn_HidD_GetAttributes_t          HidD_GetAttributes;
    fn_HidD_GetPreparsedData_t       HidD_GetPreparsedData;
    fn_HidD_FreePreparsedData_t      HidD_FreePreparsedData;
    fn_HidD_GetProductString_t       HidD_GetProductString;
    fn_HidD_GetManufacturerString_t  HidD_GetManufacturerString;
    fn_HidD_SetFeature_t             HidD_SetFeature;
    fn_HidD_GetFeature_t             HidD_GetFeature;
    fn_HidP_GetCaps_t                HidP_GetCaps;
} ghid;

struct wapi_plat_hid_device_t {
    HANDLE                h;
    wapi_plat_hid_info_t  info;
};

static bool hid_init(void) {
    if (ghid.initialized) return ghid.available;
    ghid.initialized = true;
    ghid.hid_dll = LoadLibraryW(L"hid.dll");
    if (!ghid.hid_dll) return false;

    #define R(sym) ghid.sym = (fn_##sym##_t)GetProcAddress(ghid.hid_dll, #sym)
    R(HidD_GetHidGuid);
    R(HidD_GetAttributes);
    R(HidD_GetPreparsedData);
    R(HidD_FreePreparsedData);
    R(HidD_GetProductString);
    R(HidD_GetManufacturerString);
    R(HidD_SetFeature);
    R(HidD_GetFeature);
    R(HidP_GetCaps);
    #undef R

    if (!ghid.HidD_GetHidGuid || !ghid.HidD_GetAttributes ||
        !ghid.HidD_GetPreparsedData || !ghid.HidD_FreePreparsedData ||
        !ghid.HidP_GetCaps) {
        return false;
    }
    ghid.HidD_GetHidGuid(&ghid.hid_guid);
    ghid.available = true;
    return true;
}

/* FNV-1a 128-bit fold of the device instance path → stable 16-byte uid. */
static void path_to_uid(const WCHAR* path, uint8_t uid[16]) {
    uint64_t h1 = 0xcbf29ce484222325ULL;
    uint64_t h2 = 0x84222325cbf29ce4ULL;
    for (const WCHAR* p = path; *p; p++) {
        h1 ^= (uint64_t)(uint16_t)*p;
        h1 *= 0x100000001b3ULL;
        h2 ^= (uint64_t)(uint16_t)*p << 1;
        h2 *= 0x100000001b3ULL;
    }
    memcpy(uid + 0, &h1, 8);
    memcpy(uid + 8, &h2, 8);
}

/* Query info from an open hid file handle. Populates everything
 * except uid and name (caller already has those from SetupDi). */
static bool fill_info_from_handle(HANDLE h, wapi_plat_hid_info_t* out) {
    WAPI_HIDD_ATTRIBUTES attr = {0};
    attr.Size = sizeof(attr);
    if (!ghid.HidD_GetAttributes(h, &attr)) return false;
    out->vendor_id  = attr.VendorID;
    out->product_id = attr.ProductID;
    out->version    = attr.VersionNumber;

    void* pp = NULL;
    if (ghid.HidD_GetPreparsedData(h, &pp)) {
        WAPI_HIDP_CAPS caps = {0};
        if (ghid.HidP_GetCaps(pp, &caps) == 0x00110000 /* HIDP_STATUS_SUCCESS */) {
            out->usage_page         = caps.UsagePage;
            out->usage              = caps.Usage;
            out->max_input_report   = caps.InputReportByteLength;
            out->max_output_report  = caps.OutputReportByteLength;
            out->max_feature_report = caps.FeatureReportByteLength;
        }
        ghid.HidD_FreePreparsedData(pp);
    }
    return true;
}

int wapi_plat_hid_enumerate(uint16_t vf, uint16_t pf, uint16_t upf,
                            wapi_plat_hid_info_t* out, int max)
{
    if (!hid_init()) return 0;

    HDEVINFO dev_set = SetupDiGetClassDevsW(&ghid.hid_guid, NULL, NULL,
                                            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (dev_set == INVALID_HANDLE_VALUE) return 0;

    int total = 0;
    SP_DEVICE_INTERFACE_DATA ifd = {0};
    ifd.cbSize = sizeof(ifd);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(dev_set, NULL, &ghid.hid_guid, i, &ifd); i++) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(dev_set, &ifd, NULL, 0, &need, NULL);
        if (need == 0) continue;

        SP_DEVICE_INTERFACE_DETAIL_DATA_W* det =
            (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)calloc(1, need);
        if (!det) continue;
        det->cbSize = sizeof(*det);

        if (!SetupDiGetDeviceInterfaceDetailW(dev_set, &ifd, det, need, NULL, NULL)) {
            free(det); continue;
        }

        HANDLE h = CreateFileW(det->DevicePath,
                               GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING,
                               FILE_FLAG_OVERLAPPED, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            /* Retry read-only — some system HIDs (Precision Touchpad,
             * keyboards held by the class driver) deny R/W access. */
            h = CreateFileW(det->DevicePath, 0,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, 0, NULL);
        }
        if (h == INVALID_HANDLE_VALUE) { free(det); continue; }

        wapi_plat_hid_info_t info; memset(&info, 0, sizeof(info));
        if (fill_info_from_handle(h, &info)) {
            bool match =
                (vf  == 0 || info.vendor_id  == vf) &&
                (pf  == 0 || info.product_id == pf) &&
                (upf == 0 || info.usage_page == upf);
            if (match) {
                path_to_uid(det->DevicePath, info.uid);
                if (ghid.HidD_GetProductString) {
                    WCHAR wname[128] = {0};
                    if (ghid.HidD_GetProductString(h, wname, sizeof(wname))) {
                        int n = WideCharToMultiByte(CP_UTF8, 0, wname, -1,
                                                    info.name, (int)sizeof(info.name) - 1,
                                                    NULL, NULL);
                        if (n > 0) info.name[n] = 0;
                    }
                }
                if (info.name[0] == 0) {
                    snprintf(info.name, sizeof(info.name), "HID %04X:%04X",
                             info.vendor_id, info.product_id);
                }
                if (out && total < max) out[total] = info;
                total++;
            }
        }
        CloseHandle(h);
        free(det);
    }
    SetupDiDestroyDeviceInfoList(dev_set);
    return total;
}

wapi_plat_hid_device_t* wapi_plat_hid_open(const uint8_t uid[16]) {
    if (!hid_init() || !uid) return NULL;
    HDEVINFO dev_set = SetupDiGetClassDevsW(&ghid.hid_guid, NULL, NULL,
                                            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (dev_set == INVALID_HANDLE_VALUE) return NULL;

    wapi_plat_hid_device_t* dev = NULL;
    SP_DEVICE_INTERFACE_DATA ifd = {0};
    ifd.cbSize = sizeof(ifd);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(dev_set, NULL, &ghid.hid_guid, i, &ifd); i++) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(dev_set, &ifd, NULL, 0, &need, NULL);
        if (need == 0) continue;
        SP_DEVICE_INTERFACE_DETAIL_DATA_W* det =
            (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)calloc(1, need);
        if (!det) continue;
        det->cbSize = sizeof(*det);
        if (!SetupDiGetDeviceInterfaceDetailW(dev_set, &ifd, det, need, NULL, NULL)) {
            free(det); continue;
        }
        uint8_t path_uid[16];
        path_to_uid(det->DevicePath, path_uid);
        if (memcmp(path_uid, uid, 16) == 0) {
            HANDLE h = CreateFileW(det->DevicePath,
                                   GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING,
                                   FILE_FLAG_OVERLAPPED, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                dev = (wapi_plat_hid_device_t*)calloc(1, sizeof(*dev));
                if (!dev) { CloseHandle(h); free(det); break; }
                dev->h = h;
                if (fill_info_from_handle(h, &dev->info)) {
                    memcpy(dev->info.uid, uid, 16);
                    if (ghid.HidD_GetProductString) {
                        WCHAR wname[128] = {0};
                        if (ghid.HidD_GetProductString(h, wname, sizeof(wname))) {
                            int n = WideCharToMultiByte(CP_UTF8, 0, wname, -1,
                                                        dev->info.name,
                                                        (int)sizeof(dev->info.name) - 1,
                                                        NULL, NULL);
                            if (n > 0) dev->info.name[n] = 0;
                        }
                    }
                }
                free(det);
                break;
            }
        }
        free(det);
    }
    SetupDiDestroyDeviceInfoList(dev_set);
    return dev;
}

void wapi_plat_hid_close(wapi_plat_hid_device_t* d) {
    if (!d) return;
    if (d->h && d->h != INVALID_HANDLE_VALUE) CloseHandle(d->h);
    free(d);
}

bool wapi_plat_hid_get_info(wapi_plat_hid_device_t* d, wapi_plat_hid_info_t* out) {
    if (!d || !out) return false;
    *out = d->info;
    return true;
}

int wapi_plat_hid_read_report(wapi_plat_hid_device_t* d,
                              void* buf, int buf_len, int timeout_ms)
{
    if (!d || !d->h || d->h == INVALID_HANDLE_VALUE || !buf || buf_len <= 0) return -1;

    /* Use an OVERLAPPED read so we can honor timeout_ms. */
    OVERLAPPED ov = {0};
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!ov.hEvent) return -1;

    DWORD got = 0;
    int result;
    if (ReadFile(d->h, buf, (DWORD)buf_len, &got, &ov)) {
        result = (int)got;
    } else if (GetLastError() == ERROR_IO_PENDING) {
        DWORD w = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
        DWORD r = WaitForSingleObject(ov.hEvent, w);
        if (r == WAIT_OBJECT_0) {
            if (GetOverlappedResult(d->h, &ov, &got, FALSE)) result = (int)got;
            else                                             result = -1;
        } else {
            CancelIoEx(d->h, &ov);
            /* Wait for the cancel to drain so the OS releases the buffer. */
            GetOverlappedResult(d->h, &ov, &got, TRUE);
            result = 0; /* timeout */
        }
    } else {
        result = -1;
    }
    CloseHandle(ov.hEvent);
    return result;
}

int wapi_plat_hid_write_report(wapi_plat_hid_device_t* d, const void* data, int len) {
    if (!d || !d->h || d->h == INVALID_HANDLE_VALUE || !data || len <= 0) return -1;
    OVERLAPPED ov = {0};
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!ov.hEvent) return -1;
    DWORD wrote = 0;
    int result;
    if (WriteFile(d->h, data, (DWORD)len, &wrote, &ov)) {
        result = (int)wrote;
    } else if (GetLastError() == ERROR_IO_PENDING) {
        if (GetOverlappedResult(d->h, &ov, &wrote, TRUE)) result = (int)wrote;
        else                                              result = -1;
    } else {
        result = -1;
    }
    CloseHandle(ov.hEvent);
    return result;
}

int wapi_plat_hid_send_feature(wapi_plat_hid_device_t* d, const void* data, int len) {
    if (!d || !ghid.HidD_SetFeature || len <= 0) return -1;
    return ghid.HidD_SetFeature(d->h, (PVOID)data, (ULONG)len) ? len : -1;
}

int wapi_plat_hid_get_feature(wapi_plat_hid_device_t* d, void* buf, int buf_len) {
    if (!d || !ghid.HidD_GetFeature || buf_len <= 0) return -1;
    return ghid.HidD_GetFeature(d->h, buf, (ULONG)buf_len) ? buf_len : -1;
}
