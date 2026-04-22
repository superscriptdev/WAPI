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
const wapi_gpdb_entry_t* wapi_gamepaddb_lookup(const uint8_t guid[16]) {
    if (!guid || g_gpdb_count == 0) return NULL;
    for (int i = 0; i < g_gpdb_count; i++) {
        if (memcmp(g_gpdb_entries[i].guid, guid, 16) == 0) {
            return &g_gpdb_entries[i];
        }
    }
    return NULL;
}

int wapi_gamepaddb_count(void) { return g_gpdb_count; }
