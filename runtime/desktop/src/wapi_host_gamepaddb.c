/**
 * WAPI Desktop Runtime - SDL_GameControllerDB Loader
 *
 * Parses SDL's gamepaddb.txt (one mapping per line) into an in-memory
 * table keyed by 16-byte SDL controller GUID. Lookup is by exact GUID
 * match; once the HID report-parser lands, it will consult this table
 * to translate raw HID axes/buttons into wapi_gamepad_axis_t /
 * wapi_gamepad_button_t.
 *
 * Load order (first hit wins):
 *   1. $WAPI_GAMEPADDB (explicit override)
 *   2. <executable-dir>/data/gamepaddb.txt
 *   3. ~/.wapi/gamepaddb.txt (user override; latest pads without rebuild)
 *
 * Format (from SDL_GameControllerDB upstream):
 *   GUID,Name,a:b0,b:b1,x:b2,...,platform:Windows
 *
 * We only keep mappings whose platform field matches the current OS
 * (or is absent). All-platform mappings are a portability convention.
 */

#include "wapi_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
#define WAPI_PLATFORM_STRING "Windows"
#elif defined(__APPLE__)
#define WAPI_PLATFORM_STRING "Mac OS X"
#elif defined(__linux__)
#define WAPI_PLATFORM_STRING "Linux"
#else
#define WAPI_PLATFORM_STRING ""
#endif

#define WAPI_GPDB_MAX_FIELDS 64
#define WAPI_GPDB_NAME_LEN   64

typedef struct wapi_gpdb_field_t {
    char key[16];   /* "a", "leftx", "dpup", ... */
    char value[16]; /* "b0", "a1", "h0.8", ...   */
} wapi_gpdb_field_t;

typedef struct wapi_gpdb_entry_t {
    uint8_t           guid[16];
    char              name[WAPI_GPDB_NAME_LEN];
    wapi_gpdb_field_t fields[WAPI_GPDB_MAX_FIELDS];
    int               field_count;
} wapi_gpdb_entry_t;

static wapi_gpdb_entry_t* g_gpdb_entries;
static int                g_gpdb_count;
static int                g_gpdb_cap;

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode a 32-char hex GUID into 16 bytes. Returns true on success. */
static bool parse_guid(const char* s, uint8_t out[16]) {
    int n = 0;
    const char* p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    for (int i = 0; i < 16; i++) {
        if (!p[0] || !p[1]) return false;
        int hi = hex_nibble(p[0]);
        int lo = hex_nibble(p[1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
        p += 2;
        n++;
    }
    return n == 16;
}

static void gpdb_grow(void) {
    int new_cap = g_gpdb_cap ? g_gpdb_cap * 2 : 64;
    wapi_gpdb_entry_t* p = (wapi_gpdb_entry_t*)realloc(
        g_gpdb_entries, (size_t)new_cap * sizeof(*p));
    if (!p) return; /* silent drop: gamepadDB is advisory */
    g_gpdb_entries = p;
    g_gpdb_cap = new_cap;
}

/* Append one parsed entry to the table. Takes ownership of fields by
 * copying into place. */
static void gpdb_append(const wapi_gpdb_entry_t* e) {
    if (g_gpdb_count >= g_gpdb_cap) gpdb_grow();
    if (g_gpdb_count >= g_gpdb_cap) return;
    g_gpdb_entries[g_gpdb_count++] = *e;
}

/* Parse one mapping line. Returns true if the line produced a valid
 * entry that matched our platform. */
static bool gpdb_parse_line(const char* line, size_t len) {
    /* Strip leading whitespace + comments */
    while (len > 0 && isspace((unsigned char)*line)) { line++; len--; }
    if (len == 0 || line[0] == '#') return false;

    /* Tokenize by comma into a bounded local array. */
    char buf[1024];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, line, len);
    buf[len] = 0;
    /* Trim trailing whitespace / newline. */
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r' ||
                       buf[len-1] == ' '  || buf[len-1] == '\t')) {
        buf[--len] = 0;
    }
    if (len == 0) return false;

    wapi_gpdb_entry_t e; memset(&e, 0, sizeof(e));
    bool platform_match = false;
    bool platform_seen  = false;

    char* save = NULL;
    char* tok  = NULL;
    int   idx  = 0;

    /* Strtok_r-ish split on ',' */
    char* cur = buf;
    while (cur) {
        char* next = strchr(cur, ',');
        if (next) { *next = 0; tok = cur; cur = next + 1; }
        else      { tok = cur; cur = NULL; }

        if (idx == 0) {
            if (!parse_guid(tok, e.guid)) return false;
        } else if (idx == 1) {
            size_t nl = strlen(tok);
            if (nl >= sizeof(e.name)) nl = sizeof(e.name) - 1;
            memcpy(e.name, tok, nl);
            e.name[nl] = 0;
        } else {
            /* "key:value" — find the first ':'. */
            char* colon = strchr(tok, ':');
            if (!colon) { idx++; continue; }
            *colon = 0;
            const char* key = tok;
            const char* val = colon + 1;
            if (strcmp(key, "platform") == 0) {
                platform_seen = true;
                platform_match = (strcmp(val, WAPI_PLATFORM_STRING) == 0);
            } else if (e.field_count < WAPI_GPDB_MAX_FIELDS) {
                wapi_gpdb_field_t* f = &e.fields[e.field_count++];
                size_t kl = strlen(key);
                size_t vl = strlen(val);
                if (kl >= sizeof(f->key)) kl = sizeof(f->key) - 1;
                if (vl >= sizeof(f->value)) vl = sizeof(f->value) - 1;
                memcpy(f->key,   key, kl); f->key[kl] = 0;
                memcpy(f->value, val, vl); f->value[vl] = 0;
            }
        }
        idx++;
    }
    if (idx < 2) return false;
    /* Entries without a platform field are portable; keep them too. */
    if (platform_seen && !platform_match) return false;

    gpdb_append(&e);
    return true;
}

static int gpdb_load_file(const char* path) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return 0;
    int loaded = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        if (gpdb_parse_line(line, strlen(line))) loaded++;
    }
    fclose(fp);
    return loaded;
}

/* Best-effort lookup for "<exe-dir>/data/gamepaddb.txt". Returns true
 * if a path was written to `out`. */
static bool gpdb_default_path(char* out, size_t cap) {
#ifdef _WIN32
    wchar_t wpath[512];
    DWORD n = GetModuleFileNameW(NULL, wpath, 512);
    if (n == 0 || n >= 512) return false;
    /* Trim the executable name. */
    for (int i = (int)n - 1; i >= 0; i--) {
        if (wpath[i] == L'\\' || wpath[i] == L'/') { wpath[i] = 0; break; }
    }
    char apath[1024];
    int an = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, apath, sizeof(apath), NULL, NULL);
    if (an <= 0) return false;
    snprintf(out, cap, "%s/data/gamepaddb.txt", apath);
    return true;
#else
    (void)out; (void)cap; return false;
#endif
}

/* Public: load the gamepad DB following the documented search order.
 * Safe to call multiple times; subsequent calls are no-ops once a
 * table has been populated. Returns the number of entries in the
 * table (0 on miss — everything continues to work off XInput). */
int wapi_gamepaddb_load(void) {
    if (g_gpdb_count > 0) return g_gpdb_count;

    const char* override = getenv("WAPI_GAMEPADDB");
    if (override && *override) {
        gpdb_load_file(override);
    }

    char buf[1024];
    if (gpdb_default_path(buf, sizeof(buf))) {
        gpdb_load_file(buf);
    }

#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
#else
    const char* home = getenv("HOME");
#endif
    if (home && *home) {
        snprintf(buf, sizeof(buf), "%s/.wapi/gamepaddb.txt", home);
        gpdb_load_file(buf);
    }

    return g_gpdb_count;
}

/* Public: find a mapping by 16-byte SDL GUID. Returns NULL on miss.
 * Pointer is valid until wapi_gamepaddb_reset — i.e. for the life of
 * the process, because the table is append-only. */
static const wapi_gpdb_entry_t* gpdb_lookup(const uint8_t guid[16]) {
    if (!guid || g_gpdb_count == 0) return NULL;
    for (int i = 0; i < g_gpdb_count; i++) {
        if (memcmp(g_gpdb_entries[i].guid, guid, 16) == 0) {
            return &g_gpdb_entries[i];
        }
    }
    return NULL;
}

int wapi_gamepaddb_count(void) { return g_gpdb_count; }

/* ============================================================
 * SDL GUID construction + mapping decode
 * ============================================================
 * SDL_JoystickGetGUID produces a 16-byte blob that combines bus,
 * vendor/product/version, and a CRC. For HID pads without a CRC
 * seed (the common case) the layout is:
 *   [ 0-1] bus     (u16 LE)     0x03=USB, 0x05=Bluetooth
 *   [ 2-3] CRC16                0 when not seeded (most entries)
 *   [ 4-5] vendor  (u16 LE)
 *   [ 6-7] reserved 0
 *   [ 8-9] product (u16 LE)
 *   [10-11] reserved 0
 *   [12-13] version (u16 LE)
 *   [14]   driver_signature     0 for HID
 *   [15]   driver_data          0 for HID
 */
void wapi_gamepaddb_make_guid(uint16_t bus, uint16_t vid, uint16_t pid,
                              uint16_t version, uint8_t out[16]) {
    if (!out) return;
    memset(out, 0, 16);
    out[ 0] = (uint8_t)(bus & 0xFF);       out[ 1] = (uint8_t)(bus >> 8);
    out[ 4] = (uint8_t)(vid & 0xFF);       out[ 5] = (uint8_t)(vid >> 8);
    out[ 8] = (uint8_t)(pid & 0xFF);       out[ 9] = (uint8_t)(pid >> 8);
    out[12] = (uint8_t)(version & 0xFF);   out[13] = (uint8_t)(version >> 8);
}

/* ---- Decoded mapping ---- */

/* WAPI_GAMEPAD_BUTTON_* indices + string keys used by SDL. */
static const struct { const char* key; int idx; } k_btn_keys[] = {
    {"a",            0}, {"b",            1},
    {"x",            2}, {"y",            3},
    {"back",         4}, {"guide",        5},
    {"start",        6},
    {"leftstick",    7}, {"rightstick",   8},
    {"leftshoulder", 9}, {"rightshoulder",10},
    {"dpup",        11}, {"dpdown",      12},
    {"dpleft",      13}, {"dpright",     14},
};

static const struct { const char* key; int idx; } k_axis_keys[] = {
    {"leftx",        0}, {"lefty",        1},
    {"rightx",       2}, {"righty",       3},
    {"lefttrigger",  4}, {"righttrigger", 5},
};

/* Parse "bXX" | "aXX" | "hN.M" | "+aXX" | "-aXX" into the encoded slot.
 * Encoding:
 *   0            unmapped
 *   0x1000 | XX  digital button index XX
 *   0x2000 | XX  axis index XX, normal
 *   0x2800 | XX  axis index XX, inverted
 *   0x4000 | (N<<8)|M  hat N mask M (1=up,2=right,4=down,8=left)
 */
static uint16_t gpdb_encode_source(const char* v) {
    if (!v || !*v) return 0;
    bool invert = false;
    if (*v == '+') { v++; }
    else if (*v == '-') { invert = true; v++; }
    if (*v == 'b') {
        int n = atoi(v + 1);
        if (n < 0 || n > 127) return 0;
        return (uint16_t)(0x1000u | (uint16_t)n);
    }
    if (*v == 'a') {
        int n = atoi(v + 1);
        if (n < 0 || n > 127) return 0;
        uint16_t bits = invert ? 0x2800u : 0x2000u;
        /* SDL also has ~aN to indicate a full-range trigger. Treat as
         * plain axis here — the decoder normalizes based on WAPI axis. */
        const char* p = v + 1;
        while (*p && (*p >= '0' && *p <= '9')) p++;
        if (*p == '~') bits ^= 0x0800u; /* flip inversion */
        return (uint16_t)(bits | (uint16_t)n);
    }
    if (*v == 'h') {
        int n = atoi(v + 1);
        const char* dot = strchr(v, '.');
        if (!dot) return 0;
        int m = atoi(dot + 1);
        if (n < 0 || n > 7 || m < 1 || m > 15) return 0;
        return (uint16_t)(0x4000u | ((uint16_t)n << 8) | (uint16_t)m);
    }
    return 0;
}

static int gpdb_find_key(const wapi_gpdb_entry_t* e, const char* key) {
    for (int i = 0; i < e->field_count; i++) {
        if (strcmp(e->fields[i].key, key) == 0) return i;
    }
    return -1;
}

bool wapi_gamepaddb_resolve(const uint8_t guid[16], wapi_gpdb_mapping_t* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    const wapi_gpdb_entry_t* e = gpdb_lookup(guid);
    if (!e) {
        /* Also try a "transport-agnostic" lookup: some DBs record the
         * same pad under bus=0. Rebuild a zeroed-bus copy and retry. */
        uint8_t alt[16]; memcpy(alt, guid, 16); alt[0] = 0; alt[1] = 0;
        e = gpdb_lookup(alt);
        if (!e) return false;
    }
    for (size_t i = 0; i < sizeof(k_btn_keys)/sizeof(k_btn_keys[0]); i++) {
        int fi = gpdb_find_key(e, k_btn_keys[i].key);
        if (fi >= 0) out->buttons[k_btn_keys[i].idx] = gpdb_encode_source(e->fields[fi].value);
    }
    for (size_t i = 0; i < sizeof(k_axis_keys)/sizeof(k_axis_keys[0]); i++) {
        int fi = gpdb_find_key(e, k_axis_keys[i].key);
        if (fi >= 0) out->axes[k_axis_keys[i].idx] = gpdb_encode_source(e->fields[fi].value);
    }
    return true;
}
