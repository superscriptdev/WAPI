/**
 * WAPI Desktop Runtime - Device / Display Reference Database
 *
 * Cross-platform panel reference keyed by typed identifiers. A row
 * describes one physical panel; the keys listed under it are every
 * identifier that resolves to that panel on any platform.
 *
 * No OS exposes the fields this DB carries (sub-pixel layout, true
 * panel class, DCI-P3 coverage percent, precise gamut coverages),
 * so DB lookup is the primary path on every backend — the host's
 * platform query produces a key, the DB produces the facts.
 *
 * # File format (TSV)
 *
 *   Entry line      <name>\t<field>:<value>\t<field>:<value>...
 *   Key line        (indented 2+ spaces) <namespace>:<key>
 *   Comment         # anything, to end of line
 *   Blank           ignored
 *
 * An entry belongs to every key line that immediately follows it
 * (until the next entry line or EOF). Duplicate keys across entries
 * are resolved latest-wins.
 *
 * Example:
 *
 *   Dell U2723QE\tpanel:ips_lcd\tdiagonal_mm:685\tcoverage_p3:98
 *     edid:DEL40F9
 *
 *   iPhone 15 Pro\tpanel:oled\tdiagonal_mm:155\tenvelope:rounded_rect\thas_touch:1
 *     apple:iPhone16,1
 *
 *   Samsung QN90C 75"\tpanel:va_lcd\thdr:hdr1400
 *     edid:SAM7177
 *     smarttv:samsung/QN90C-75
 *
 * # Fields
 *
 *   subpixels       Semicolon-list of "<C>@<x>,<y>" entries. See
 *                   wapi_display.h for the vocabulary. Omit for
 *                   displays without a per-pixel sub-pixel grid
 *                   (CRTs, DLP, e-ink, projectors).
 *   panel           ips_lcd | va_lcd | tn_lcd | stn_lcd |
 *                   passive_lcd | oled | woled | qdoled | microled |
 *                   crt_shadow | crt_trinitron | crt_slot | plasma |
 *                   eink_carta | eink_kaleido | eink_gallery |
 *                   dlp | lcos | projector_other
 *   width_mm        Visible-area physical width in integer mm.
 *   height_mm       Visible-area physical height in integer mm.
 *   diagonal_mm     Integer mm; omit if width_mm+height_mm are set
 *                   (the accessor derives it).
 *   peak_sdr_nits   Measured SDR peak in integer cd/m².
 *   peak_hdr_nits   Measured HDR peak in integer cd/m². 0 = not
 *                   HDR-capable (no cheap-panel tier enum; if you
 *                   don't know the real peak, leave this unset).
 *   min_mnits       Measured floor in milli-cd/m² (1/1000 nit).
 *                   OLED floors hit ~0.5 mnits; LCD ~50-500.
 *   primaries       "<rx>,<ry>;<gx>,<gy>;<bx>,<by>;<wx>,<wy>" —
 *                   CIE xy coordinates of R/G/B/W primaries as
 *                   decimals (0.0..1.0). Example (sRGB):
 *                   primaries:0.640,0.330;0.300,0.600;0.150,0.060;0.3127,0.3290
 *   coverage_srgb   0..100, percent of sRGB covered.
 *   coverage_p3     0..100, percent of DCI-P3 covered.
 *   coverage_rec2020 0..100, percent of BT.2020 covered.
 *   coverage_adobe  0..100, percent of AdobeRGB covered.
 *   white_point     d50 | d55 | d65 | d75 | d93 | <kelvin-integer>
 *   refresh         "<min>,<max>" in Hz (decimals allowed for
 *                   47.952 / 59.94 / 119.88). "<hz>" for fixed
 *                   refresh. Examples: refresh:60 | refresh:48,144
 *                   | refresh:47.952,240
 *   update          fast | slow | very_slow
 *                   Omit for anything not e-ink; "fast" is the
 *                   default effective value at runtime.
 *   envelope        full | rounded_rect | circle
 *   corner_radius   Four u16 pixels (TL,TR,BR,BL), e.g.
 *                   corner_radius:50,50,50,50
 *   cutouts         ";"-separated list of x,y,w,h quadruples, e.g.
 *                   cutouts:1005,33,225,82;0,0,80,30
 *   has_touch       0 | 1
 *   has_stylus      0 | 1
 *   stylus_pressure integer (pressure levels; 0 = unknown)
 *   stylus_tilt     0 | 1
 *
 * # Key namespaces
 *
 *   edid      7-char <PnP><Product> from EDID bytes 8-11
 *             (desktop monitors, laptop internal panels with EDID,
 *             TVs over HDMI, etc.)
 *   apple     Apple device model identifier from sysctlbyname
 *             "hw.machine" (iPhone16,1 | MacBookPro18,1 | Watch6,1
 *             | AppleTV11,1 | RealityDevice14,1). Identifies the
 *             built-in panel of that device.
 *   android   "<manufacturer>/<model>" lowercase from Build.*
 *             (samsung/sm-s928b)
 *   dmi       "<vendor>/<product>" from DMI / SMBIOS product info;
 *             identifies a chassis, not a panel. Secondary key for
 *             devices whose panel varies by SKU.
 *   dt        Linux device-tree model string ("raspberrypi,4-model-b")
 *   smarttv   "<vendor>/<model>" for Smart-TV platform APIs
 *             (Tizen, webOS, Fire TV, Roku)
 *   console   "<vendor>/<model>" for fixed console SKUs
 *             (sony/ps5, nintendo/switch-oled)
 *
 * # Load order (entries with the same key: later source wins)
 *
 *   1. $WAPI_DEVICEDB
 *   2. <executable-dir>/data/devicedb.txt
 *   3. ~/.wapi/devicedb.txt
 */

#include "wapi_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define DDB_NAME_LEN         64
#define DDB_SUBPIXEL_MAX      8
#define DDB_CUTOUTS_MAX       4
#define DDB_KEYS_PER_ENTRY    4
#define DDB_KEY_MAX_LEN      62   /* fits longest Android dm string we'll accept */

/* Wire-format sub-pixel entry (4B): matches wapi_subpixel_t. */
typedef struct wapi_devicedb_sp_t {
    uint8_t color;   /* 0=R 1=G 2=B 3=W */
    uint8_t x;       /* 0..255 */
    uint8_t y;       /* 0..255 */
    uint8_t _pad;
} wapi_devicedb_sp_t;

/* Wire-format cutout rect (16B): matches wapi_display_cutout_t. */
typedef struct wapi_devicedb_cutout_t {
    int32_t x, y, w, h;
} wapi_devicedb_cutout_t;

typedef struct wapi_devicedb_key_t {
    uint8_t kind;                    /* wapi_devicedb_kind_t; 0 = unused slot */
    uint8_t len;                     /* bytes in data[] */
    char    data[DDB_KEY_MAX_LEN];   /* not NUL-terminated */
} wapi_devicedb_key_t;

typedef struct wapi_devicedb_entry_t {
    char     name[DDB_NAME_LEN];

    wapi_devicedb_key_t keys[DDB_KEYS_PER_ENTRY];
    uint8_t             key_count;

    /* Sub-pixel grid. subpixel_count == 0 ⇒ no per-pixel grid
     * (CRTs, DLP, e-ink, projectors). */
    wapi_devicedb_sp_t  subpixels[DDB_SUBPIXEL_MAX];
    uint8_t             subpixel_count;

    /* Display geometry. envelope values mirror
     * wapi_display_envelope_t. corner_radius[] TL,TR,BR,BL. */
    uint32_t envelope;
    uint16_t corner_radius_px[4];
    wapi_devicedb_cutout_t cutouts[DDB_CUTOUTS_MAX];
    uint8_t                cutout_count;

    /* Physical size (mm). */
    uint32_t width_mm;
    uint32_t height_mm;
    uint32_t diagonal_mm;

    /* Panel technology (enum numbering matches public header). */
    uint32_t panel_class;

    /* Luminance. peak_* in integer cd/m² (nits); min in
     * milli-cd/m² to preserve OLED floor precision. */
    uint32_t peak_sdr_cd_m2;
    uint32_t peak_hdr_cd_m2;
    uint32_t min_mcd_m2;

    /* CIE xy primaries, u16 fixed-point / 65535 = 0.0..1.0. */
    uint16_t primary_rx, primary_ry;
    uint16_t primary_gx, primary_gy;
    uint16_t primary_bx, primary_by;
    uint16_t primary_wx, primary_wy;

    /* Gamut coverage percents 0..100. */
    uint8_t  coverage_srgb;
    uint8_t  coverage_p3;
    uint8_t  coverage_rec2020;
    uint8_t  coverage_adobe_rgb;

    uint32_t white_point_k;

    /* Refresh range in milliHertz. 0,0 = fixed refresh. */
    uint32_t refresh_min_mhz;
    uint32_t refresh_max_mhz;

    /* Update class (enum matches public header). */
    uint32_t update_class;

    /* Input on this surface. */
    uint8_t  has_touch;
    uint8_t  has_stylus;
    uint8_t  stylus_has_tilt;
    uint8_t  _pad0;
    uint32_t stylus_pressure_levels;

    uint32_t flags;
} wapi_devicedb_entry_t;

#define DDB_FLAG_HAS_SUBPIXELS  0x00000001
#define DDB_FLAG_HAS_WIDTH      0x00000002
#define DDB_FLAG_HAS_HEIGHT     0x00000004
#define DDB_FLAG_HAS_DIAGONAL   0x00000008
#define DDB_FLAG_HAS_PANEL      0x00000010
#define DDB_FLAG_HAS_PEAKSDR    0x00000020
#define DDB_FLAG_HAS_PEAKHDR    0x00000040
#define DDB_FLAG_HAS_MIN        0x00000080
#define DDB_FLAG_HAS_PRIMARIES  0x00000100
#define DDB_FLAG_HAS_COV_SRGB   0x00000200
#define DDB_FLAG_HAS_COV_P3     0x00000400
#define DDB_FLAG_HAS_COV_REC20  0x00000800
#define DDB_FLAG_HAS_COV_ADOBE  0x00001000
#define DDB_FLAG_HAS_WHITE      0x00002000
#define DDB_FLAG_HAS_REFRESH    0x00004000
#define DDB_FLAG_HAS_UPDATE     0x00008000
#define DDB_FLAG_HAS_ENVELOPE   0x00010000
#define DDB_FLAG_HAS_CORNERS    0x00020000
#define DDB_FLAG_HAS_CUTOUTS    0x00040000
#define DDB_FLAG_HAS_TOUCH      0x00080000
#define DDB_FLAG_HAS_STYLUS     0x00100000

/* Dynamic entry table. Grown 2× on demand; lookup is linear scan
 * over entries × their keys. At the sizes this DB reaches in
 * practice (<10k entries) the scan cost is a few hundred µs per
 * display-enumeration; displays are enumerated once at startup plus
 * on hot-plug. A hash index is an obvious future upgrade if the
 * scan ever shows in profiles. */
static wapi_devicedb_entry_t* g_ddb;
static int                    g_ddb_count;
static int                    g_ddb_capacity;

static bool ddb_reserve(int need) {
    if (need <= g_ddb_capacity) return true;
    int cap = g_ddb_capacity ? g_ddb_capacity : 128;
    while (cap < need) cap *= 2;
    void* p = realloc(g_ddb, (size_t)cap * sizeof(*g_ddb));
    if (!p) return false;
    g_ddb = p;
    g_ddb_capacity = cap;
    return true;
}

/* ============================================================
 * Small parsing helpers
 * ============================================================ */

static void rtrim(char* s, size_t* len_io) {
    size_t l = *len_io;
    while (l > 0 && (s[l-1] == ' ' || s[l-1] == '\t' ||
                     s[l-1] == '\r' || s[l-1] == '\n')) l--;
    s[l] = 0;
    *len_io = l;
}

/* Parse a "<C>@<x>,<y>;..." list into a sub-pixel array. */
static int parse_subpixels(const char* v, wapi_devicedb_sp_t* out, int max) {
    int n = 0;
    while (*v && n < max) {
        while (*v == ' ' || *v == '\t') v++;
        if (!*v) break;
        uint8_t col;
        switch (*v) {
        case 'R': case 'r': col = 0; break;
        case 'G': case 'g': col = 1; break;
        case 'B': case 'b': col = 2; break;
        case 'W': case 'w': col = 3; break;
        default: return n;
        }
        v++;
        if (*v != '@') return n;
        v++;
        char* end = NULL;
        long x = strtol(v, &end, 10);
        if (end == v || !end || *end != ',') return n;
        v = end + 1;
        long y = strtol(v, &end, 10);
        if (end == v || !end) return n;
        v = end;
        if (x < 0) x = 0; if (x > 255) x = 255;
        if (y < 0) y = 0; if (y > 255) y = 255;
        out[n].color = col;
        out[n].x = (uint8_t)x;
        out[n].y = (uint8_t)y;
        out[n]._pad = 0;
        n++;
        if (*v == ';') v++;
    }
    return n;
}

/* Parse "<x>,<y>,<w>,<h>;..." list into cutout rects. */
static int parse_cutouts(const char* v, wapi_devicedb_cutout_t* out, int max) {
    int n = 0;
    while (*v && n < max) {
        long vals[4]; int got = 0;
        char* end = NULL;
        for (int i = 0; i < 4; i++) {
            while (*v == ' ' || *v == '\t') v++;
            vals[i] = strtol(v, &end, 10);
            if (end == v || !end) return n;
            v = end;
            got++;
            if (i < 3) {
                if (*v != ',') return n;
                v++;
            }
        }
        if (got != 4) return n;
        out[n].x = (int32_t)vals[0];
        out[n].y = (int32_t)vals[1];
        out[n].w = (int32_t)vals[2];
        out[n].h = (int32_t)vals[3];
        n++;
        if (*v == ';') v++;
    }
    return n;
}

/* Parse "<r0>,<r1>,<r2>,<r3>" → 4 u16 into out. */
static bool parse_corners(const char* v, uint16_t out[4]) {
    char* end = NULL;
    for (int i = 0; i < 4; i++) {
        while (*v == ' ' || *v == '\t') v++;
        long r = strtol(v, &end, 10);
        if (end == v || !end) return false;
        if (r < 0) r = 0; if (r > 65535) r = 65535;
        out[i] = (uint16_t)r;
        v = end;
        if (i < 3) {
            if (*v != ',') return false;
            v++;
        }
    }
    return true;
}

static uint32_t map_panel(const char* v) {
    if (!strcmp(v, "ips_lcd"))        return 1;
    if (!strcmp(v, "va_lcd"))         return 2;
    if (!strcmp(v, "tn_lcd"))         return 3;
    if (!strcmp(v, "oled"))           return 4;
    if (!strcmp(v, "woled"))          return 5;
    if (!strcmp(v, "qdoled"))         return 6;
    if (!strcmp(v, "microled"))       return 7;
    if (!strcmp(v, "stn_lcd"))        return 8;
    if (!strcmp(v, "passive_lcd"))    return 9;
    if (!strcmp(v, "crt_shadow"))     return 10;
    if (!strcmp(v, "crt_trinitron"))  return 11;
    if (!strcmp(v, "crt_slot"))       return 12;
    if (!strcmp(v, "plasma"))         return 13;
    if (!strcmp(v, "eink_carta"))     return 14;
    if (!strcmp(v, "eink_kaleido"))   return 15;
    if (!strcmp(v, "eink_gallery"))   return 16;
    if (!strcmp(v, "dlp"))            return 17;
    if (!strcmp(v, "lcos"))           return 18;
    if (!strcmp(v, "projector_other")) return 19;
    return 0;
}
static uint32_t map_envelope(const char* v) {
    if (!strcmp(v, "full"))         return 0;
    if (!strcmp(v, "rounded_rect")) return 1;
    if (!strcmp(v, "circle"))       return 2;
    return 0;
}
static uint32_t map_update(const char* v) {
    if (!strcmp(v, "fast"))      return 1;
    if (!strcmp(v, "slow"))      return 2;
    if (!strcmp(v, "very_slow")) return 3;
    return 0;
}

/* Parse "<rx>,<ry>;<gx>,<gy>;<bx>,<by>;<wx>,<wy>" (decimals 0..1)
 * into 8 u16s fixed-point over 65535. All four primaries required;
 * returns false otherwise. */
static bool parse_primaries(const char* v, uint16_t out[8]) {
    for (int i = 0; i < 4; i++) {
        char* end = NULL;
        while (*v == ' ' || *v == '\t') v++;
        double x = strtod(v, &end); if (end == v) return false; v = end;
        if (*v != ',') return false; v++;
        double y = strtod(v, &end); if (end == v) return false; v = end;
        if (x < 0.0) x = 0.0; if (x > 1.0) x = 1.0;
        if (y < 0.0) y = 0.0; if (y > 1.0) y = 1.0;
        out[i*2 + 0] = (uint16_t)(x * 65535.0 + 0.5);
        out[i*2 + 1] = (uint16_t)(y * 65535.0 + 0.5);
        if (i < 3) {
            if (*v != ';') return false;
            v++;
        }
    }
    return true;
}

/* Parse "<min>,<max>" or "<hz>" in Hz (decimals OK) to min/max in
 * milliHertz. Fixed refresh sets min=max. Returns false on malformed. */
static bool parse_refresh(const char* v, uint32_t* min_out, uint32_t* max_out) {
    char* end = NULL;
    double mn = strtod(v, &end);
    if (end == v) return false;
    double mx = mn;
    while (*end == ' ' || *end == '\t') end++;
    if (*end == ',') {
        v = end + 1;
        mx = strtod(v, &end);
        if (end == v) return false;
    }
    if (mn < 0.0 || mx < 0.0) return false;
    if (mx < mn) { double t = mn; mn = mx; mx = t; }
    *min_out = (uint32_t)(mn * 1000.0 + 0.5);
    *max_out = (uint32_t)(mx * 1000.0 + 0.5);
    return true;
}
static uint32_t parse_white_point(const char* v) {
    if (!strcmp(v, "d50")) return 5000;
    if (!strcmp(v, "d55")) return 5500;
    if (!strcmp(v, "d65")) return 6500;
    if (!strcmp(v, "d75")) return 7500;
    if (!strcmp(v, "d93")) return 9300;
    int k = atoi(v);
    return k > 0 ? (uint32_t)k : 0;
}

static uint8_t map_kind(const char* s, size_t len) {
    #define Q(lit, val) if (len == sizeof(lit)-1 && memcmp(s, lit, len)==0) return val
    Q("edid",    WAPI_DEVICEDB_KIND_EDID);
    Q("apple",   WAPI_DEVICEDB_KIND_APPLE);
    Q("android", WAPI_DEVICEDB_KIND_ANDROID);
    Q("dmi",     WAPI_DEVICEDB_KIND_DMI);
    Q("dt",      WAPI_DEVICEDB_KIND_DT);
    Q("smarttv", WAPI_DEVICEDB_KIND_SMARTTV);
    Q("console", WAPI_DEVICEDB_KIND_CONSOLE);
    #undef Q
    return 0;
}

/* ============================================================
 * Entry assembly
 * ============================================================ */

static uint8_t parse_pct(const char* v) {
    int c = atoi(v); if (c < 0) c = 0; if (c > 100) c = 100;
    return (uint8_t)c;
}

static void set_field(wapi_devicedb_entry_t* e, const char* k, const char* v) {
    if      (!strcmp(k, "subpixels")) {
        e->subpixel_count = (uint8_t)parse_subpixels(v, e->subpixels, DDB_SUBPIXEL_MAX);
        e->flags |= DDB_FLAG_HAS_SUBPIXELS;
    }
    else if (!strcmp(k, "panel"))        { e->panel_class = map_panel(v); e->flags |= DDB_FLAG_HAS_PANEL; }
    else if (!strcmp(k, "width_mm"))     { e->width_mm    = (uint32_t)atoi(v); e->flags |= DDB_FLAG_HAS_WIDTH; }
    else if (!strcmp(k, "height_mm"))    { e->height_mm   = (uint32_t)atoi(v); e->flags |= DDB_FLAG_HAS_HEIGHT; }
    else if (!strcmp(k, "diagonal_mm"))  { e->diagonal_mm = (uint32_t)atoi(v); e->flags |= DDB_FLAG_HAS_DIAGONAL; }
    else if (!strcmp(k, "peak_sdr_nits")){ e->peak_sdr_cd_m2 = (uint32_t)atoi(v); e->flags |= DDB_FLAG_HAS_PEAKSDR; }
    else if (!strcmp(k, "peak_hdr_nits")){ e->peak_hdr_cd_m2 = (uint32_t)atoi(v); e->flags |= DDB_FLAG_HAS_PEAKHDR; }
    else if (!strcmp(k, "min_mnits"))    { e->min_mcd_m2  = (uint32_t)atoi(v); e->flags |= DDB_FLAG_HAS_MIN; }
    else if (!strcmp(k, "white_point"))  { e->white_point_k = parse_white_point(v); e->flags |= DDB_FLAG_HAS_WHITE; }
    else if (!strcmp(k, "primaries"))    {
        uint16_t p[8];
        if (parse_primaries(v, p)) {
            e->primary_rx = p[0]; e->primary_ry = p[1];
            e->primary_gx = p[2]; e->primary_gy = p[3];
            e->primary_bx = p[4]; e->primary_by = p[5];
            e->primary_wx = p[6]; e->primary_wy = p[7];
            e->flags |= DDB_FLAG_HAS_PRIMARIES;
        }
    }
    else if (!strcmp(k, "coverage_srgb"))     { e->coverage_srgb      = parse_pct(v); e->flags |= DDB_FLAG_HAS_COV_SRGB; }
    else if (!strcmp(k, "coverage_p3"))       { e->coverage_p3        = parse_pct(v); e->flags |= DDB_FLAG_HAS_COV_P3; }
    else if (!strcmp(k, "coverage_rec2020"))  { e->coverage_rec2020   = parse_pct(v); e->flags |= DDB_FLAG_HAS_COV_REC20; }
    else if (!strcmp(k, "coverage_adobe"))    { e->coverage_adobe_rgb = parse_pct(v); e->flags |= DDB_FLAG_HAS_COV_ADOBE; }
    else if (!strcmp(k, "refresh"))      {
        uint32_t mn = 0, mx = 0;
        if (parse_refresh(v, &mn, &mx)) {
            e->refresh_min_mhz = mn;
            e->refresh_max_mhz = mx;
            e->flags |= DDB_FLAG_HAS_REFRESH;
        }
    }
    else if (!strcmp(k, "update"))       { e->update_class = map_update(v); e->flags |= DDB_FLAG_HAS_UPDATE; }
    else if (!strcmp(k, "envelope"))     { e->envelope     = map_envelope(v); e->flags |= DDB_FLAG_HAS_ENVELOPE; }
    else if (!strcmp(k, "corner_radius")) {
        if (parse_corners(v, e->corner_radius_px)) e->flags |= DDB_FLAG_HAS_CORNERS;
    }
    else if (!strcmp(k, "cutouts"))      {
        e->cutout_count = (uint8_t)parse_cutouts(v, e->cutouts, DDB_CUTOUTS_MAX);
        e->flags |= DDB_FLAG_HAS_CUTOUTS;
    }
    else if (!strcmp(k, "has_touch"))    { e->has_touch  = atoi(v) ? 1 : 0; e->flags |= DDB_FLAG_HAS_TOUCH; }
    else if (!strcmp(k, "has_stylus"))   { e->has_stylus = atoi(v) ? 1 : 0; e->flags |= DDB_FLAG_HAS_STYLUS; }
    else if (!strcmp(k, "stylus_pressure")) { e->stylus_pressure_levels = (uint32_t)atoi(v); e->flags |= DDB_FLAG_HAS_STYLUS; }
    else if (!strcmp(k, "stylus_tilt"))  { e->stylus_has_tilt = atoi(v) ? 1 : 0; e->flags |= DDB_FLAG_HAS_STYLUS; }
}

static void parse_entry_line(char* buf, size_t len, wapi_devicedb_entry_t* e) {
    /* First TAB-delimited token = name. Rest = field:value pairs. */
    char* cur = buf;
    int col = 0;
    while (cur && *cur) {
        char* tab = strchr(cur, '\t');
        char* tok = cur;
        if (tab) { *tab = 0; cur = tab + 1; } else cur = NULL;

        /* Trim leading/trailing spaces on each token. */
        while (*tok == ' ' || *tok == '\t') tok++;
        size_t tl = strlen(tok);
        while (tl > 0 && (tok[tl-1] == ' ' || tok[tl-1] == '\t')) tok[--tl] = 0;
        if (tl == 0) { col++; continue; }

        if (col == 0) {
            if (tl >= sizeof(e->name)) tl = sizeof(e->name) - 1;
            memcpy(e->name, tok, tl); e->name[tl] = 0;
        } else {
            char* colon = strchr(tok, ':');
            if (colon) {
                *colon = 0;
                set_field(e, tok, colon + 1);
            }
        }
        col++;
    }
    (void)len;
}

static void parse_key_line(const char* s, wapi_devicedb_entry_t* e) {
    while (*s == ' ' || *s == '\t') s++;
    const char* colon = strchr(s, ':');
    if (!colon) return;
    size_t klen = (size_t)(colon - s);
    uint8_t kind = map_kind(s, klen);
    if (kind == 0) return;

    const char* v = colon + 1;
    size_t vlen = strlen(v);
    while (vlen > 0 && (v[vlen-1] == ' ' || v[vlen-1] == '\t' ||
                        v[vlen-1] == '\r' || v[vlen-1] == '\n')) vlen--;
    if (vlen == 0 || vlen > DDB_KEY_MAX_LEN) return;

    if (e->key_count >= DDB_KEYS_PER_ENTRY) return;
    wapi_devicedb_key_t* k = &e->keys[e->key_count++];
    k->kind = kind;
    k->len  = (uint8_t)vlen;
    memcpy(k->data, v, vlen);
}

static void commit_entry(wapi_devicedb_entry_t* e) {
    if (e->key_count == 0 && e->name[0] == 0) return;

    /* Upsert: if any of this entry's keys already belong to an
     * existing row, merge field-by-field onto that row (later source
     * wins per set flag). Otherwise append. */
    for (int i = 0; i < g_ddb_count; i++) {
        wapi_devicedb_entry_t* o = &g_ddb[i];
        for (int a = 0; a < e->key_count; a++) {
            for (int b = 0; b < o->key_count; b++) {
                if (e->keys[a].kind == o->keys[b].kind &&
                    e->keys[a].len  == o->keys[b].len  &&
                    memcmp(e->keys[a].data, o->keys[b].data, e->keys[a].len) == 0)
                {
                    /* Merge scalar fields controlled by flag bits. */
                    if (e->flags & DDB_FLAG_HAS_SUBPIXELS) {
                        memcpy(o->subpixels, e->subpixels, sizeof(e->subpixels));
                        o->subpixel_count = e->subpixel_count;
                    }
                    if (e->flags & DDB_FLAG_HAS_WIDTH)    o->width_mm       = e->width_mm;
                    if (e->flags & DDB_FLAG_HAS_HEIGHT)   o->height_mm      = e->height_mm;
                    if (e->flags & DDB_FLAG_HAS_DIAGONAL) o->diagonal_mm    = e->diagonal_mm;
                    if (e->flags & DDB_FLAG_HAS_PANEL)    o->panel_class    = e->panel_class;
                    if (e->flags & DDB_FLAG_HAS_PEAKSDR)  o->peak_sdr_cd_m2 = e->peak_sdr_cd_m2;
                    if (e->flags & DDB_FLAG_HAS_PEAKHDR)  o->peak_hdr_cd_m2 = e->peak_hdr_cd_m2;
                    if (e->flags & DDB_FLAG_HAS_MIN)      o->min_mcd_m2     = e->min_mcd_m2;
                    if (e->flags & DDB_FLAG_HAS_WHITE)    o->white_point_k  = e->white_point_k;
                    if (e->flags & DDB_FLAG_HAS_PRIMARIES) {
                        o->primary_rx = e->primary_rx; o->primary_ry = e->primary_ry;
                        o->primary_gx = e->primary_gx; o->primary_gy = e->primary_gy;
                        o->primary_bx = e->primary_bx; o->primary_by = e->primary_by;
                        o->primary_wx = e->primary_wx; o->primary_wy = e->primary_wy;
                    }
                    if (e->flags & DDB_FLAG_HAS_COV_SRGB)  o->coverage_srgb      = e->coverage_srgb;
                    if (e->flags & DDB_FLAG_HAS_COV_P3)    o->coverage_p3        = e->coverage_p3;
                    if (e->flags & DDB_FLAG_HAS_COV_REC20) o->coverage_rec2020   = e->coverage_rec2020;
                    if (e->flags & DDB_FLAG_HAS_COV_ADOBE) o->coverage_adobe_rgb = e->coverage_adobe_rgb;
                    if (e->flags & DDB_FLAG_HAS_REFRESH) {
                        o->refresh_min_mhz = e->refresh_min_mhz;
                        o->refresh_max_mhz = e->refresh_max_mhz;
                    }
                    if (e->flags & DDB_FLAG_HAS_UPDATE)   o->update_class   = e->update_class;
                    if (e->flags & DDB_FLAG_HAS_ENVELOPE) o->envelope       = e->envelope;
                    if (e->flags & DDB_FLAG_HAS_CORNERS)
                        memcpy(o->corner_radius_px, e->corner_radius_px, sizeof(e->corner_radius_px));
                    if (e->flags & DDB_FLAG_HAS_CUTOUTS) {
                        memcpy(o->cutouts, e->cutouts, sizeof(e->cutouts));
                        o->cutout_count = e->cutout_count;
                    }
                    if (e->flags & DDB_FLAG_HAS_TOUCH)  o->has_touch       = e->has_touch;
                    if (e->flags & DDB_FLAG_HAS_STYLUS) {
                        o->has_stylus              = e->has_stylus;
                        o->stylus_has_tilt         = e->stylus_has_tilt;
                        o->stylus_pressure_levels  = e->stylus_pressure_levels;
                    }
                    o->flags |= e->flags;
                    if (e->name[0]) {
                        size_t nl = strlen(e->name);
                        if (nl >= sizeof(o->name)) nl = sizeof(o->name) - 1;
                        memcpy(o->name, e->name, nl); o->name[nl] = 0;
                    }
                    /* Unioned keys from the new entry. */
                    for (int c = 0; c < e->key_count && o->key_count < DDB_KEYS_PER_ENTRY; c++) {
                        bool dup = false;
                        for (int d = 0; d < o->key_count; d++) {
                            if (o->keys[d].kind == e->keys[c].kind &&
                                o->keys[d].len  == e->keys[c].len  &&
                                memcmp(o->keys[d].data, e->keys[c].data, e->keys[c].len) == 0)
                            { dup = true; break; }
                        }
                        if (!dup) o->keys[o->key_count++] = e->keys[c];
                    }
                    return;
                }
            }
        }
    }

    if (!ddb_reserve(g_ddb_count + 1)) return;
    g_ddb[g_ddb_count++] = *e;
}

/* ============================================================
 * File loader
 * ============================================================ */

static int load_file(const char* path) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return 0;

    int loaded = 0;
    char raw[4096];
    wapi_devicedb_entry_t cur; memset(&cur, 0, sizeof(cur));
    bool have_cur = false;

    while (fgets(raw, sizeof(raw), fp)) {
        size_t l = strlen(raw);
        rtrim(raw, &l);
        if (l == 0) continue;

        /* Comment? */
        const char* p = raw;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#') continue;

        /* Key line = starts with 2+ spaces (or a tab) of indent. */
        bool indented = (raw[0] == ' ' || raw[0] == '\t');
        if (indented) {
            if (have_cur) parse_key_line(raw, &cur);
            continue;
        }

        /* Non-indented line = new entry. Commit the previous. */
        if (have_cur) { commit_entry(&cur); loaded++; }
        memset(&cur, 0, sizeof(cur));
        have_cur = true;
        parse_entry_line(raw, l, &cur);
    }
    if (have_cur) { commit_entry(&cur); loaded++; }

    fclose(fp);
    return loaded;
}

static bool default_path(char* out, size_t cap) {
#ifdef _WIN32
    wchar_t wp[512];
    DWORD n = GetModuleFileNameW(NULL, wp, 512);
    if (n == 0 || n >= 512) return false;
    for (int i = (int)n - 1; i >= 0; i--) {
        if (wp[i] == L'\\' || wp[i] == L'/') { wp[i] = 0; break; }
    }
    char ap[1024];
    int an = WideCharToMultiByte(CP_UTF8, 0, wp, -1, ap, sizeof(ap), NULL, NULL);
    if (an <= 0) return false;
    snprintf(out, cap, "%s/data/devicedb.txt", ap);
    return true;
#else
    (void)out; (void)cap; return false;
#endif
}

int wapi_devicedb_load(void) {
    g_ddb_count = 0;

    const char* ov = getenv("WAPI_DEVICEDB");
    if (ov && *ov) load_file(ov);

    char buf[1024];
    if (default_path(buf, sizeof(buf))) load_file(buf);

#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
#else
    const char* home = getenv("HOME");
#endif
    if (home && *home) {
        snprintf(buf, sizeof(buf), "%s/.wapi/devicedb.txt", home);
        load_file(buf);
    }
    return g_ddb_count;
}

int wapi_devicedb_count(void) { return g_ddb_count; }

const wapi_devicedb_entry_t*
wapi_devicedb_lookup(wapi_devicedb_kind_t kind,
                     const char* key, size_t key_len)
{
    if (!key || key_len == 0 || key_len > DDB_KEY_MAX_LEN) return NULL;
    for (int i = 0; i < g_ddb_count; i++) {
        wapi_devicedb_entry_t* e = &g_ddb[i];
        for (int j = 0; j < e->key_count; j++) {
            if (e->keys[j].kind == (uint8_t)kind &&
                e->keys[j].len  == (uint8_t)key_len &&
                memcmp(e->keys[j].data, key, key_len) == 0)
                return e;
        }
    }
    return NULL;
}

/* ============================================================
 * Public field accessors
 * ============================================================ */

int wapi_devicedb_subpixels(const wapi_devicedb_entry_t* e, void* out, int max) {
    if (!e || !out || max <= 0) return 0;
    int n = e->subpixel_count < max ? e->subpixel_count : max;
    memcpy(out, e->subpixels, (size_t)n * 4);
    return n;
}
int wapi_devicedb_cutouts(const wapi_devicedb_entry_t* e, void* out, int max) {
    if (!e || !out || max <= 0) return 0;
    int n = e->cutout_count < max ? e->cutout_count : max;
    memcpy(out, e->cutouts, (size_t)n * sizeof(wapi_devicedb_cutout_t));
    return n;
}
int      wapi_devicedb_subpixel_count(const wapi_devicedb_entry_t* e) { return e ? (int)e->subpixel_count : 0; }
int      wapi_devicedb_cutout_count  (const wapi_devicedb_entry_t* e) { return e ? (int)e->cutout_count : 0; }
uint32_t wapi_devicedb_envelope      (const wapi_devicedb_entry_t* e) { return e ? e->envelope : 0; }
void     wapi_devicedb_corner_radii  (const wapi_devicedb_entry_t* e, uint16_t out[4]) {
    if (!out) return;
    if (e) memcpy(out, e->corner_radius_px, sizeof(e->corner_radius_px));
    else   memset(out, 0, sizeof(uint16_t) * 4);
}
uint32_t wapi_devicedb_panel_class   (const wapi_devicedb_entry_t* e) { return e ? e->panel_class   : 0; }
uint32_t wapi_devicedb_width_mm      (const wapi_devicedb_entry_t* e) { return e ? e->width_mm      : 0; }
uint32_t wapi_devicedb_height_mm     (const wapi_devicedb_entry_t* e) { return e ? e->height_mm     : 0; }
uint32_t wapi_devicedb_diagonal_mm   (const wapi_devicedb_entry_t* e) { return e ? e->diagonal_mm   : 0; }
uint32_t wapi_devicedb_peak_sdr_cd_m2(const wapi_devicedb_entry_t* e) { return e ? e->peak_sdr_cd_m2 : 0; }
uint32_t wapi_devicedb_peak_hdr_cd_m2(const wapi_devicedb_entry_t* e) { return e ? e->peak_hdr_cd_m2 : 0; }
uint32_t wapi_devicedb_min_mcd_m2    (const wapi_devicedb_entry_t* e) { return e ? e->min_mcd_m2    : 0; }
uint32_t wapi_devicedb_white_point_k (const wapi_devicedb_entry_t* e) { return e ? e->white_point_k : 0; }
void     wapi_devicedb_primaries     (const wapi_devicedb_entry_t* e, uint16_t out[8]) {
    if (!out) return;
    if (e) {
        out[0] = e->primary_rx; out[1] = e->primary_ry;
        out[2] = e->primary_gx; out[3] = e->primary_gy;
        out[4] = e->primary_bx; out[5] = e->primary_by;
        out[6] = e->primary_wx; out[7] = e->primary_wy;
    } else {
        memset(out, 0, sizeof(uint16_t) * 8);
    }
}
uint32_t wapi_devicedb_coverage_srgb     (const wapi_devicedb_entry_t* e) { return e ? e->coverage_srgb      : 0; }
uint32_t wapi_devicedb_coverage_p3       (const wapi_devicedb_entry_t* e) { return e ? e->coverage_p3        : 0; }
uint32_t wapi_devicedb_coverage_rec2020  (const wapi_devicedb_entry_t* e) { return e ? e->coverage_rec2020   : 0; }
uint32_t wapi_devicedb_coverage_adobe_rgb(const wapi_devicedb_entry_t* e) { return e ? e->coverage_adobe_rgb : 0; }
uint32_t wapi_devicedb_refresh_min_mhz   (const wapi_devicedb_entry_t* e) { return e ? e->refresh_min_mhz    : 0; }
uint32_t wapi_devicedb_refresh_max_mhz   (const wapi_devicedb_entry_t* e) { return e ? e->refresh_max_mhz    : 0; }
uint32_t wapi_devicedb_update_class      (const wapi_devicedb_entry_t* e) { return e ? e->update_class       : 0; }
int      wapi_devicedb_has_touch         (const wapi_devicedb_entry_t* e) { return e ? (int)e->has_touch : 0; }
int      wapi_devicedb_has_stylus        (const wapi_devicedb_entry_t* e) { return e ? (int)e->has_stylus : 0; }
int      wapi_devicedb_stylus_pressure_levels(const wapi_devicedb_entry_t* e) { return e ? (int)e->stylus_pressure_levels : 0; }
int      wapi_devicedb_stylus_has_tilt   (const wapi_devicedb_entry_t* e) { return e ? (int)e->stylus_has_tilt : 0; }
const char* wapi_devicedb_name           (const wapi_devicedb_entry_t* e) { return e ? e->name : ""; }
