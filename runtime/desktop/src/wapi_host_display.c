/**
 * WAPI Desktop Runtime - Display (wapi_display.h)
 *
 * Enumerate monitors via EnumDisplayMonitors, fill the 56B info
 * struct. Sub-pixel layout is left unknown (WAPI_ERR_NOTSUP) since
 * Windows doesn't expose it portably.
 */

#include "wapi_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellscalingapi.h>

#pragma comment(lib, "shcore")

/* ============================================================
 * EDID fetch + PnP+Product key derivation
 * ============================================================
 * Resolve an HMONITOR to its 128-byte EDID base block by walking
 * from the monitor's \\?\DISPLAY# device-interface path to the
 * Enum\DISPLAY\<model>\<instance>\Device Parameters registry key.
 * The raw EDID stream has never been a formal contract on Windows,
 * but this registry path is what SetupDi ultimately resolves to
 * and is stable across Win7..Win11.
 *
 * The DB key is the 7-char ASCII token <PnP><Product> — 3 uppercase
 * letters unpacked from EDID bytes 8-9 (5 bits per letter, packed
 * big-endian; letter value 1..26 maps to A..Z) plus 4 uppercase hex
 * digits printed from the little-endian u16 at bytes 10-11. This
 * matches Windows' own MONITOR\<PnP><Prod> device-id token, so the
 * same 7 chars appear in Device Manager and in linuxhw/EDID's
 * directory structure. */

static bool edid_registry_path_from_device_id(const WCHAR* device_id,
                                              WCHAR* out, size_t cap)
{
    /* device_id looks like:
     *   \\?\DISPLAY#GSM5A25#6&7abfd267&0&UID53505#{e6f07b5f-...}
     * We want:
     *   SYSTEM\CurrentControlSet\Enum\DISPLAY\GSM5A25\6&7abfd267&0&UID53505\Device Parameters
     */
    const WCHAR* p = device_id;
    if (wcsncmp(p, L"\\\\?\\", 4) == 0) p += 4;
    if (wcsncmp(p, L"DISPLAY#", 8) != 0) return false;
    p += 8;

    const WCHAR* model_end = wcschr(p, L'#');
    if (!model_end) return false;
    const WCHAR* inst = model_end + 1;
    const WCHAR* inst_end = wcschr(inst, L'#');
    size_t model_len = (size_t)(model_end - p);
    size_t inst_len  = inst_end ? (size_t)(inst_end - inst) : wcslen(inst);

    int written = _snwprintf_s(out, cap, _TRUNCATE,
        L"SYSTEM\\CurrentControlSet\\Enum\\DISPLAY\\%.*s\\%.*s\\Device Parameters",
        (int)model_len, p, (int)inst_len, inst);
    return written > 0;
}

static int edid_read_from_reg(const WCHAR* subkey, uint8_t* out, int cap) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return 0;
    DWORD type = 0, size = (DWORD)cap;
    LSTATUS st = RegQueryValueExW(k, L"EDID", NULL, &type, (LPBYTE)out, &size);
    RegCloseKey(k);
    if (st != ERROR_SUCCESS || type != REG_BINARY) return 0;
    return (int)size;
}

/* Fetch EDID for a monitor whose GDI name is "\\.\DISPLAY<N>". */
static int edid_for_device(const WCHAR* gdi_device, uint8_t* out, int cap) {
    DISPLAY_DEVICEW mon; memset(&mon, 0, sizeof(mon));
    mon.cb = sizeof(mon);
    /* EDD_GET_DEVICE_INTERFACE_NAME puts the "\\?\DISPLAY#..." path
     * into DeviceID, which is what we need. */
    if (!EnumDisplayDevicesW(gdi_device, 0, &mon, 0x00000001 /* EDD_GET_DEVICE_INTERFACE_NAME */))
        return 0;

    WCHAR subkey[512];
    if (!edid_registry_path_from_device_id(mon.DeviceID, subkey,
                                           sizeof(subkey)/sizeof(WCHAR)))
        return 0;
    return edid_read_from_reg(subkey, out, cap);
}

/* Derive the 7-char <PnP><Product> DB key from an EDID base block.
 * Returns false on obviously-synthetic EDIDs whose manufacturer
 * bytes don't decode to three A-Z letters (some capture cards /
 * virtual monitors inject zeros). */
static bool edid_to_pnp_key(const uint8_t* edid, char out[7]) {
    uint8_t b0 = edid[8], b1 = edid[9];
    int l1 = (b0 >> 2) & 0x1F;
    int l2 = ((b0 & 0x03) << 3) | ((b1 >> 5) & 0x07);
    int l3 = b1 & 0x1F;
    if (l1 < 1 || l1 > 26 || l2 < 1 || l2 > 26 || l3 < 1 || l3 > 26)
        return false;
    out[0] = (char)('A' - 1 + l1);
    out[1] = (char)('A' - 1 + l2);
    out[2] = (char)('A' - 1 + l3);
    uint16_t prod = (uint16_t)edid[10] | ((uint16_t)edid[11] << 8);
    static const char hex[] = "0123456789ABCDEF";
    out[3] = hex[(prod >> 12) & 0xF];
    out[4] = hex[(prod >>  8) & 0xF];
    out[5] = hex[(prod >>  4) & 0xF];
    out[6] = hex[(prod >>  0) & 0xF];
    return true;
}

typedef struct mon_entry_t {
    HMONITOR hmon;
    RECT     bounds;        /* monitor rect (virtual screen coords) */
    RECT     work;          /* work area (excludes taskbar)          */
    WCHAR    device[CCHDEVICENAME];
    bool     primary;
} mon_entry_t;

#define MON_MAX 16

static BOOL CALLBACK collect_cb(HMONITOR h, HDC dc, LPRECT r, LPARAM lp) {
    (void)dc; (void)r;
    struct { mon_entry_t* out; int count; int cap; }* ctx = (void*)lp;
    if (ctx->count >= ctx->cap) return TRUE;

    MONITORINFOEXW mi; mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(h, (MONITORINFO*)&mi)) return TRUE;
    mon_entry_t* e = &ctx->out[ctx->count++];
    e->hmon    = h;
    e->bounds  = mi.rcMonitor;
    e->work    = mi.rcWork;
    e->primary = (mi.dwFlags & MONITORINFOF_PRIMARY) ? true : false;
    lstrcpynW(e->device, mi.szDevice, CCHDEVICENAME);
    return TRUE;
}

static int collect_monitors(mon_entry_t* out, int cap) {
    struct { mon_entry_t* out; int count; int cap; } ctx = { out, 0, cap };
    EnumDisplayMonitors(NULL, NULL, collect_cb, (LPARAM)&ctx);
    return ctx.count;
}

static float monitor_scale(HMONITOR h) {
    HMODULE shc = LoadLibraryW(L"shcore.dll");
    if (!shc) return 1.0f;
    typedef HRESULT (WINAPI *fn_t)(HMONITOR, int, UINT*, UINT*);
    fn_t GetDpiForMonitor = (fn_t)GetProcAddress(shc, "GetDpiForMonitor");
    if (!GetDpiForMonitor) return 1.0f;
    UINT x = 96, y = 96;
    if (SUCCEEDED(GetDpiForMonitor(h, 0 /* MDT_EFFECTIVE_DPI */, &x, &y))) {
        return (float)x / 96.0f;
    }
    return 1.0f;
}

static float monitor_hz(const WCHAR* device) {
    DEVMODEW dm = {0};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(device, ENUM_CURRENT_SETTINGS, &dm)) {
        return (float)dm.dmDisplayFrequency;
    }
    return 60.0f;
}

static wasm_trap_t* h_display_count(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    mon_entry_t arr[MON_MAX];
    int n = collect_monitors(arr, MON_MAX);
    WAPI_RET_I32(n);
    return NULL;
}

static wasm_trap_t* h_display_get_info(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  idx      = WAPI_ARG_I32(0);
    uint32_t info_ptr = WAPI_ARG_U32(1);

    mon_entry_t arr[MON_MAX];
    int n = collect_monitors(arr, MON_MAX);
    if (idx < 0 || idx >= n) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    mon_entry_t* e = &arr[idx];

    uint8_t buf[56]; memset(buf, 0, sizeof(buf));
    uint32_t display_id = (uint32_t)(idx + 1);
    int32_t  x = e->bounds.left;
    int32_t  y = e->bounds.top;
    int32_t  w = e->bounds.right  - e->bounds.left;
    int32_t  h = e->bounds.bottom - e->bounds.top;
    float    hz    = monitor_hz(e->device);
    float    scale = monitor_scale(e->hmon);

    /* Best-effort DB lookup by <PnP><Product>. Missing EDID or
     * missing DB entry = today's OS-only behaviour. */
    uint8_t edid[256];
    int edid_len = edid_for_device(e->device, edid, sizeof(edid));
    const struct wapi_devicedb_entry_t* db = NULL;
    if (edid_len >= 128) {
        char key[7];
        if (edid_to_pnp_key(edid, key)) {
            db = wapi_devicedb_lookup(WAPI_DEVICEDB_KIND_EDID, key, 7);
        }
    }

    memcpy(buf +  0, &display_id, 4);
    memcpy(buf +  4, &x,          4);
    memcpy(buf +  8, &y,          4);
    memcpy(buf + 12, &w,          4);
    memcpy(buf + 16, &h,          4);
    memcpy(buf + 20, &hz,         4);
    memcpy(buf + 24, &scale,      4);
    /* stringview name: DB supplies a friendly name when present. We
     * leave data=0/length=0 unless we can write into guest memory —
     * this host doesn't mint guest-heap allocations for the display
     * path, so names remain host_get-only for now. */
    buf[48] = e->primary ? 1 : 0;
    buf[49] = (w >= h) ? 0 : 1;    /* orientation */
    buf[50] = (uint8_t)(db ? wapi_devicedb_subpixel_count(db) : 0);
    uint16_t rot = 0;
    memcpy(buf + 52, &rot, 2);

    if (!wapi_wasm_write_bytes(info_ptr, buf, sizeof(buf))) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_display_get_subpixels(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  idx    = WAPI_ARG_I32(0);
    uint32_t sp_ptr = WAPI_ARG_U32(1);
    int32_t  max    = WAPI_ARG_I32(2);
    uint32_t cnt_ptr= WAPI_ARG_U32(3);

    mon_entry_t arr[MON_MAX];
    int n = collect_monitors(arr, MON_MAX);
    if (idx < 0 || idx >= n || max <= 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    uint8_t edid[256];
    int edid_len = edid_for_device(arr[idx].device, edid, sizeof(edid));
    if (edid_len < 128) { WAPI_RET_I32(WAPI_ERR_NOTSUP); return NULL; }
    char key[7];
    if (!edid_to_pnp_key(edid, key)) { WAPI_RET_I32(WAPI_ERR_NOTSUP); return NULL; }
    const struct wapi_devicedb_entry_t* e = wapi_devicedb_lookup(WAPI_DEVICEDB_KIND_EDID, key, 7);
    if (!e) { WAPI_RET_I32(WAPI_ERR_NOTSUP); return NULL; }

    /* DB carries the raw {color,x,y} array verbatim — no host-side
     * expansion from an enum, so exotic panel layouts (e.g. WOLED
     * with custom W offsets, per-vendor pentile variants) round-trip
     * from community submissions straight to the guest. */
    uint8_t buf[4 * 8];
    int want = max > 8 ? 8 : max;
    int written = wapi_devicedb_subpixels(e, buf, want);
    if (written <= 0) { WAPI_RET_I32(WAPI_ERR_NOTSUP); return NULL; }

    if (!wapi_wasm_write_bytes(sp_ptr, buf, (uint32_t)(written * 4))) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    if (cnt_ptr) wapi_wasm_write_i32(cnt_ptr, written);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_display_get_usable_bounds(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  idx = WAPI_ARG_I32(0);
    uint32_t xp  = WAPI_ARG_U32(1);
    uint32_t yp  = WAPI_ARG_U32(2);
    uint32_t wp  = WAPI_ARG_U32(3);
    uint32_t hp  = WAPI_ARG_U32(4);
    mon_entry_t arr[MON_MAX];
    int n = collect_monitors(arr, MON_MAX);
    if (idx < 0 || idx >= n) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    RECT* r = &arr[idx].work;
    int32_t x = r->left, y = r->top;
    int32_t w = r->right - r->left, h = r->bottom - r->top;
    if (xp) wapi_wasm_write_i32(xp, x);
    if (yp) wapi_wasm_write_i32(yp, y);
    if (wp) wapi_wasm_write_i32(wp, w);
    if (hp) wapi_wasm_write_i32(hp, h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_display_get_panel_info(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  idx      = WAPI_ARG_I32(0);
    uint32_t info_ptr = WAPI_ARG_U32(1);

    mon_entry_t arr[MON_MAX];
    int n = collect_monitors(arr, MON_MAX);
    if (idx < 0 || idx >= n) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    uint8_t edid[256];
    int edid_len = edid_for_device(arr[idx].device, edid, sizeof(edid));
    if (edid_len < 128) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }
    char key[7];
    if (!edid_to_pnp_key(edid, key)) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }
    const struct wapi_devicedb_entry_t* e = wapi_devicedb_lookup(WAPI_DEVICEDB_KIND_EDID, key, 7);
    if (!e) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }

    /* wapi_display_panel_info_t: 72 B, align 4. Byte layout matches
     * the public header exactly — see wapi_display.h for offsets. */
    uint8_t buf[72]; memset(buf, 0, sizeof(buf));
    uint32_t u;
    u = wapi_devicedb_panel_class   (e); memcpy(buf +  0, &u, 4);
    u = wapi_devicedb_width_mm      (e); memcpy(buf +  4, &u, 4);
    u = wapi_devicedb_height_mm     (e); memcpy(buf +  8, &u, 4);
    u = wapi_devicedb_diagonal_mm   (e); memcpy(buf + 12, &u, 4);
    u = wapi_devicedb_peak_sdr_cd_m2(e); memcpy(buf + 16, &u, 4);
    u = wapi_devicedb_peak_hdr_cd_m2(e); memcpy(buf + 20, &u, 4);
    u = wapi_devicedb_min_mcd_m2    (e); memcpy(buf + 24, &u, 4);
    u = wapi_devicedb_white_point_k (e); memcpy(buf + 28, &u, 4);

    uint16_t prim[8];
    wapi_devicedb_primaries(e, prim);
    memcpy(buf + 32, prim, 16);

    buf[48] = (uint8_t)wapi_devicedb_coverage_srgb      (e);
    buf[49] = (uint8_t)wapi_devicedb_coverage_p3        (e);
    buf[50] = (uint8_t)wapi_devicedb_coverage_rec2020   (e);
    buf[51] = (uint8_t)wapi_devicedb_coverage_adobe_rgb (e);

    u = wapi_devicedb_refresh_min_mhz(e); memcpy(buf + 52, &u, 4);
    u = wapi_devicedb_refresh_max_mhz(e); memcpy(buf + 56, &u, 4);
    u = wapi_devicedb_update_class   (e); memcpy(buf + 60, &u, 4);
    u = (uint32_t)wapi_devicedb_stylus_pressure_levels(e);
    memcpy(buf + 64, &u, 4);
    buf[68] = (uint8_t)wapi_devicedb_has_touch      (e);
    buf[69] = (uint8_t)wapi_devicedb_has_stylus     (e);
    buf[70] = (uint8_t)wapi_devicedb_stylus_has_tilt(e);
    /* buf[71] = 0 (padding) */

    if (!wapi_wasm_write_bytes(info_ptr, buf, sizeof(buf))) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* Physical geometry. Windows doesn't expose laptop-lid corner radii
 * or notches portably, so the DB is authoritative. A matched entry
 * supplies envelope / corner radii / cutouts; unmatched displays
 * (most external monitors) fall back to envelope=FULL / no corners /
 * no cutouts, which is correct for them. */
static const struct wapi_devicedb_entry_t* db_for_monitor(const WCHAR* device) {
    uint8_t edid[256];
    int edid_len = edid_for_device(device, edid, sizeof(edid));
    if (edid_len < 128) return NULL;
    char key[7];
    if (!edid_to_pnp_key(edid, key)) return NULL;
    return wapi_devicedb_lookup(WAPI_DEVICEDB_KIND_EDID, key, 7);
}

static wasm_trap_t* h_display_get_geometry(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  idx      = WAPI_ARG_I32(0);
    uint32_t geom_ptr = WAPI_ARG_U32(1);

    mon_entry_t arr[MON_MAX];
    int n = collect_monitors(arr, MON_MAX);
    if (idx < 0 || idx >= n) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    const struct wapi_devicedb_entry_t* db = db_for_monitor(arr[idx].device);

    /* wapi_display_geometry_t (16B): u32 envelope, u16[4] corners, u32 cutout_count */
    uint8_t buf[16];
    memset(buf, 0, sizeof(buf));
    uint32_t envelope = db ? wapi_devicedb_envelope(db) : 0;
    uint32_t cutout_count = db ? (uint32_t)wapi_devicedb_cutout_count(db) : 0;
    uint16_t corners[4] = {0};
    if (db) wapi_devicedb_corner_radii(db, corners);
    memcpy(buf +  0, &envelope, 4);
    memcpy(buf +  4, corners,   8);
    memcpy(buf + 12, &cutout_count, 4);
    if (!wapi_wasm_write_bytes(geom_ptr, buf, sizeof(buf))) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_display_get_cutouts(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  idx        = WAPI_ARG_I32(0);
    uint32_t cutouts_ptr = WAPI_ARG_U32(1);
    int32_t  max        = WAPI_ARG_I32(2);
    uint32_t count_ptr  = WAPI_ARG_U32(3);

    mon_entry_t arr[MON_MAX];
    int n = collect_monitors(arr, MON_MAX);
    if (idx < 0 || idx >= n || max < 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    const struct wapi_devicedb_entry_t* db = db_for_monitor(arr[idx].device);
    if (!db || max == 0) {
        if (count_ptr) wapi_wasm_write_i32(count_ptr, 0);
        WAPI_RET_I32(WAPI_OK);
        return NULL;
    }
    /* wapi_display_cutout_t is 16 B: {i32 x, y, w, h}. */
    uint8_t buf[16 * 4];   /* DDB cap is 4 cutouts */
    int want = max > 4 ? 4 : max;
    int written = wapi_devicedb_cutouts(db, buf, want);
    if (written > 0) {
        if (!wapi_wasm_write_bytes(cutouts_ptr, buf, (uint32_t)(written * 16))) {
            WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
        }
    }
    if (count_ptr) wapi_wasm_write_i32(count_ptr, written);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

void wapi_host_register_display(wasmtime_linker_t* linker) {
    WAPI_DEFINE_0_1(linker, "wapi_display", "display_count",         h_display_count);
    WAPI_DEFINE_2_1(linker, "wapi_display", "display_get_info",      h_display_get_info);
    WAPI_DEFINE_4_1(linker, "wapi_display", "display_get_subpixels", h_display_get_subpixels);
    WAPI_DEFINE_5_1(linker, "wapi_display", "display_get_usable_bounds", h_display_get_usable_bounds);
    WAPI_DEFINE_2_1(linker, "wapi_display", "display_get_panel_info", h_display_get_panel_info);
    WAPI_DEFINE_2_1(linker, "wapi_display", "display_get_geometry",  h_display_get_geometry);
    WAPI_DEFINE_4_1(linker, "wapi_display", "display_get_cutouts",   h_display_get_cutouts);
}
