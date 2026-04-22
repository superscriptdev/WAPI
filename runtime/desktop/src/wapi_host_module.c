/**
 * WAPI Desktop Runtime — Runtime Module Linking (wapi_module.h, spec §10)
 *
 * Per §10.1's hybrid memory model: every child has its own private
 * memory 0 (instantiated here); shared memory 1 is host-owned and
 * accessed through `shared_read` / `shared_write` (the "host-call"
 * path; zero-copy multi-memory is a later optimization).
 *
 * Module identity is the SHA-256 of the Wasm binary (§10.3). The URL
 * is a fetch hint only; this desktop runtime resolves URLs out of a
 * local path cache the CLI seeds with `--module <sha256hex>=<path>`.
 *
 * Cross-module calls (§10.4): wapi_val_t arrays on both sides of the
 * boundary. Traps inside the child are caught and surfaced as an
 * error return to the parent rather than a process-wide trap (§10.x).
 *
 * Lend / reclaim / io_proxy / allocator_proxy — the borrow system
 * and vtable-proxy plumbing — stay spec-legal `WAPI_ERR_NOSYS` for
 * now, since hello_game's AI module is a pure function that only
 * needs shared-mem-read/write; they land when the first caller needs
 * zero-copy borrows or host-mediated I/O into a child.
 */

#include "wapi_host.h"

#ifdef _WIN32
#  include <windows.h>
#  include <bcrypt.h>
#  pragma comment(lib, "bcrypt.lib")
#endif

#define MOD "wapi_module"

/* ============================================================
 * CLI-seeded hash → path cache
 * ============================================================
 * main.c calls wapi_host_module_cache_add() once per --module flag. */

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

/* Decode a 64-char hex string into 32 bytes. Returns true on success. */
static bool decode_hash_hex(const char* hex, uint8_t out[32]) {
    for (int i = 0; i < 32; i++) {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

bool wapi_host_module_cache_add(const char* spec) {
    /* spec = "<64-char hex>=<filesystem path>" */
    if (!spec) return false;
    const char* eq = strchr(spec, '=');
    if (!eq || (eq - spec) != 64) return false;
    uint8_t hash[32];
    char    hex[65];
    memcpy(hex, spec, 64);
    hex[64] = '\0';
    if (!decode_hash_hex(hex, hash)) return false;
    for (int i = 0; i < WAPI_MODULE_CACHE_MAX; i++) {
        if (g_rt.module_cache[i].in_use) continue;
        memcpy(g_rt.module_cache[i].hash, hash, 32);
        size_t path_len = strlen(eq + 1);
        if (path_len >= sizeof(g_rt.module_cache[i].path)) return false;
        memcpy(g_rt.module_cache[i].path, eq + 1, path_len + 1);
        g_rt.module_cache[i].in_use = true;
        return true;
    }
    return false;
}

static wapi_module_cache_entry_t* cache_find(const uint8_t hash[32]) {
    for (int i = 0; i < WAPI_MODULE_CACHE_MAX; i++) {
        if (!g_rt.module_cache[i].in_use) continue;
        if (memcmp(g_rt.module_cache[i].hash, hash, 32) == 0)
            return &g_rt.module_cache[i];
    }
    return NULL;
}

/* ============================================================
 * SHA-256 via BCrypt (Windows); backends elsewhere plug in later.
 * ============================================================ */

static bool sha256_bytes(const void* data, size_t len, uint8_t out[32]) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg = NULL;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0) {
        if (BCryptHash(alg, NULL, 0, (PUCHAR)data, (ULONG)len, out, 32) == 0)
            ok = true;
        BCryptCloseAlgorithmProvider(alg, 0);
    }
    return ok;
#else
    /* No non-Windows backend yet; hosts that want module linking on
     * macOS/Linux wire CommonCrypto / EVP_sha256. */
    (void)data; (void)len; (void)out;
    return false;
#endif
}

/* ============================================================
 * Shared memory pool (memory 1)
 * ============================================================
 * Lazy-allocated bump region with per-allocation records so we can
 * free / realloc / usable_size. No fragmentation management yet — a
 * free leaves a hole that is *not* reclaimed by the bump cursor;
 * realloc always returns a fresh region. Good enough for
 * hello_game's tens-of-KB working set; grows later if needed. */

static bool shared_mem_ensure(void) {
    if (g_rt.shared_mem.bytes) return true;
    g_rt.shared_mem.bytes    = (uint8_t*)calloc(1, WAPI_SHARED_MEM_CAPACITY);
    g_rt.shared_mem.capacity = WAPI_SHARED_MEM_CAPACITY;
    g_rt.shared_mem.bump     = 8;  /* reserve 0..7 so offset 0 = NULL */
    return g_rt.shared_mem.bytes != NULL;
}

static uint64_t shared_alloc_internal(uint64_t size, uint64_t align) {
    if (!shared_mem_ensure()) return 0;
    if (size == 0) size = 1;
    if (align < 1)  align = 1;
    if (align > 4096) return 0;

    uint64_t off = (g_rt.shared_mem.bump + (align - 1)) & ~(uint64_t)(align - 1);
    if (off + size > g_rt.shared_mem.capacity) return 0;

    /* Record the allocation */
    for (int i = 0; i < WAPI_SHARED_MAX_ALLOCS; i++) {
        if (g_rt.shared_mem.allocs[i].in_use) continue;
        g_rt.shared_mem.allocs[i].offset = off;
        g_rt.shared_mem.allocs[i].size   = size;
        g_rt.shared_mem.allocs[i].in_use = true;
        g_rt.shared_mem.bump = off + size;
        return off;
    }
    return 0; /* alloc table full */
}

static wapi_shared_alloc_t* shared_find(uint64_t off) {
    for (int i = 0; i < WAPI_SHARED_MAX_ALLOCS; i++) {
        if (g_rt.shared_mem.allocs[i].in_use &&
            g_rt.shared_mem.allocs[i].offset == off) {
            return &g_rt.shared_mem.allocs[i];
        }
    }
    return NULL;
}

/* ============================================================
 * Module slot pool — one per loaded runtime module
 * ============================================================ */

#define MAX_MODULE_SLOTS 16
static wapi_module_slot_t g_module_slots[MAX_MODULE_SLOTS];

static wapi_module_slot_t* slot_alloc(void) {
    for (int i = 0; i < MAX_MODULE_SLOTS; i++) {
        if (!g_module_slots[i].module) return &g_module_slots[i];
    }
    return NULL;
}

static void slot_free(wapi_module_slot_t* s) {
    if (!s) return;
    if (s->module) {
        wasmtime_module_delete(s->module);
        s->module = NULL;
    }
    memset(s, 0, sizeof(*s));
}

/* Allocate a function slot index inside the module and return it
 * as a pseudo-handle (encoded as `((module_handle & 0xFFFF) << 16) | (fn_index & 0xFFFF)`
 * so a single i32 can carry both). The guest treats it as opaque. */
static int32_t module_pack_func_handle(int32_t module_handle, int func_index) {
    return (module_handle << 16) | (func_index & 0xFFFF);
}

static wapi_module_slot_t* module_unpack(int32_t packed, int* out_func_index) {
    int32_t mh = (packed >> 16) & 0xFFFF;
    if (out_func_index) *out_func_index = packed & 0xFFFF;
    if (!wapi_handle_valid(mh, WAPI_HTYPE_MODULE)) return NULL;
    return g_rt.handles[mh].data.module_slot;
}

/* ============================================================
 * load  (library mode)
 * ============================================================ */

static wasm_trap_t* cb_load(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t hash_ptr    = WAPI_ARG_U32(0);
    uint32_t url_sv_ptr  = WAPI_ARG_U32(1);
    uint32_t out_mod_ptr = WAPI_ARG_U32(2);
    (void)url_sv_ptr;  /* URL fetch is out of scope for desktop — CLI-seeded cache only */

    if (!hash_ptr || !out_mod_ptr) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    void* hp = wapi_wasm_ptr(hash_ptr, 32);
    if (!hp) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    uint8_t wanted_hash[32];
    memcpy(wanted_hash, hp, 32);

    wapi_module_cache_entry_t* entry = cache_find(wanted_hash);
    if (!entry) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }

    /* Read file */
    FILE* f = fopen(entry->path, "rb");
    if (!f) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); WAPI_RET_I32(WAPI_ERR_IO); return NULL; }
    uint8_t* bytes = (uint8_t*)malloc((size_t)sz);
    if (!bytes) { fclose(f); WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }
    if (fread(bytes, 1, (size_t)sz, f) != (size_t)sz) {
        free(bytes); fclose(f); WAPI_RET_I32(WAPI_ERR_IO); return NULL;
    }
    fclose(f);

    /* Verify content hash */
    uint8_t got_hash[32];
    if (!sha256_bytes(bytes, (size_t)sz, got_hash)) {
        free(bytes); WAPI_RET_I32(WAPI_ERR_IO); return NULL;
    }
    if (memcmp(got_hash, wanted_hash, 32) != 0) {
        free(bytes); WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }

    /* Compile + instantiate */
    wasmtime_module_t* module = NULL;
    wasmtime_error_t* err = wasmtime_module_new(g_rt.engine, bytes, (size_t)sz, &module);
    free(bytes);
    if (err) {
        wasmtime_error_delete(err);
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }

    wapi_module_slot_t* slot = slot_alloc();
    if (!slot) { wasmtime_module_delete(module); WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }
    memset(slot, 0, sizeof(*slot));
    slot->module = module;
    memcpy(slot->hash, got_hash, 32);

    wasm_trap_t* trap = NULL;
    err = wasmtime_linker_instantiate(g_rt.linker, g_rt.context, module, &slot->instance, &trap);
    if (err || trap) {
        if (err) wasmtime_error_delete(err);
        if (trap) wasm_trap_delete(trap);
        slot_free(slot);
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }

    /* Optional: child memory for shared_read-style copy-in */
    wasmtime_extern_t mem_extern;
    if (wasmtime_instance_export_get(g_rt.context, &slot->instance,
                                     "memory", 6, &mem_extern) &&
        mem_extern.kind == WASMTIME_EXTERN_MEMORY) {
        slot->memory = mem_extern.of.memory;
        slot->memory_valid = true;
    }

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_MODULE);
    if (h == 0) { slot_free(slot); WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }
    g_rt.handles[h].data.module_slot = slot;
    wapi_wasm_write_i32(out_mod_ptr, h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* join — service mode. Same machinery as load + name-keyed refcount;
 * deferred until a second caller actually needs it. Returns NOTSUP
 * so guests that only need library mode aren't blocked. */
static wasm_trap_t* cb_join(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * get_func
 * ============================================================ */

static wasm_trap_t* cb_get_func(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  mh       = WAPI_ARG_I32(0);
    uint32_t sv_ptr   = WAPI_ARG_U32(1);
    uint32_t out_ptr  = WAPI_ARG_U32(2);
    if (!wapi_handle_valid(mh, WAPI_HTYPE_MODULE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_module_slot_t* slot = g_rt.handles[mh].data.module_slot;
    if (!slot) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }

    /* Read wapi_stringview_t (u64 data, u64 length) from sv_ptr */
    void* svp = wapi_wasm_ptr(sv_ptr, 16);
    if (!svp) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    uint64_t name_data, name_len;
    memcpy(&name_data, (uint8_t*)svp + 0, 8);
    memcpy(&name_len,  (uint8_t*)svp + 8, 8);
    if (name_len == (uint64_t)-1) {
        const char* s = (const char*)wapi_wasm_ptr((uint32_t)name_data, 1);
        if (!s) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
        uint64_t n = 0; while (s[n]) n++;
        name_len = n;
    }
    if (name_len > 256) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    const char* name = (const char*)wapi_wasm_ptr((uint32_t)name_data, (uint32_t)name_len);
    if (!name && name_len) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    wasmtime_extern_t ext;
    if (!wasmtime_instance_export_get(g_rt.context, &slot->instance,
                                      name, (size_t)name_len, &ext) ||
        ext.kind != WASMTIME_EXTERN_FUNC) {
        if (out_ptr) wapi_wasm_write_i32(out_ptr, 0);
        WAPI_RET_I32(WAPI_ERR_NOENT); return NULL;
    }

    /* Stash the funcref in a free slot on the module */
    int fi = -1;
    for (int i = 0; i < WAPI_MODULE_MAX_FUNCS; i++) {
        if (!slot->funcs[i].in_use) { fi = i; break; }
    }
    if (fi < 0) { WAPI_RET_I32(WAPI_ERR_NOSPC); return NULL; }
    slot->funcs[fi].fn     = ext.of.func;
    slot->funcs[fi].in_use = true;

    int32_t packed = module_pack_func_handle(mh, fi);
    if (out_ptr) wapi_wasm_write_i32(out_ptr, packed);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * call
 * ============================================================ */

static wasm_trap_t* cb_call(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  mh          = WAPI_ARG_I32(0);
    int32_t  func_packed = WAPI_ARG_I32(1);
    uint32_t args_ptr    = WAPI_ARG_U32(2);
    uint64_t narg        = WAPI_ARG_U64(3);
    uint32_t res_ptr     = WAPI_ARG_U32(4);
    uint64_t nres        = WAPI_ARG_U64(5);
    (void)mh;  /* module handle is encoded in func_packed */

    int fi = 0;
    wapi_module_slot_t* slot = module_unpack(func_packed, &fi);
    if (!slot || fi < 0 || fi >= WAPI_MODULE_MAX_FUNCS || !slot->funcs[fi].in_use) {
        WAPI_RET_I32(WAPI_ERR_BADF); return NULL;
    }
    if (narg > 16 || nres > 16) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    /* Marshal args: wapi_val_t (16B) -> wasmtime_val_t */
    wasmtime_val_t wargs[16], wresults[16];
    for (uint64_t i = 0; i < narg; i++) {
        void* vp = wapi_wasm_ptr(args_ptr + (uint32_t)(i * 16), 16);
        if (!vp) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
        uint8_t kind = ((uint8_t*)vp)[0];
        wargs[i].kind = WASMTIME_I32;
        switch (kind) {
            case 0: { int32_t v; memcpy(&v, (uint8_t*)vp + 8, 4);
                      wargs[i].kind = WASMTIME_I32; wargs[i].of.i32 = v; break; }
            case 1: { int64_t v; memcpy(&v, (uint8_t*)vp + 8, 8);
                      wargs[i].kind = WASMTIME_I64; wargs[i].of.i64 = v; break; }
            case 2: { float v;   memcpy(&v, (uint8_t*)vp + 8, 4);
                      wargs[i].kind = WASMTIME_F32; wargs[i].of.f32 = v; break; }
            case 3: { double v;  memcpy(&v, (uint8_t*)vp + 8, 8);
                      wargs[i].kind = WASMTIME_F64; wargs[i].of.f64 = v; break; }
            default: WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
        }
    }

    wasm_trap_t* trap = NULL;
    wasmtime_error_t* err = wasmtime_func_call(g_rt.context, &slot->funcs[fi].fn,
                                               wargs, (size_t)narg,
                                               wresults, (size_t)nres, &trap);
    if (err) {
        wasm_message_t m; wasmtime_error_message(err, &m);
        fprintf(stderr, "[wapi_module.call] error: %.*s\n", (int)m.size, m.data);
        wasm_byte_vec_delete(&m); wasmtime_error_delete(err);
        WAPI_RET_I32(WAPI_ERR_IO); return NULL;
    }
    if (trap) {
        /* Child faulted; surface as error, do NOT re-trap (spec §10.x). */
        wasm_message_t m; wasm_trap_message(trap, &m);
        fprintf(stderr, "[wapi_module.call] child trapped: %.*s\n",
                (int)m.size, m.data);
        wasm_byte_vec_delete(&m); wasm_trap_delete(trap);
        WAPI_RET_I32(WAPI_ERR_UNKNOWN); return NULL;
    }

    /* Demarshal results back into the guest's wapi_val_t array */
    for (uint64_t i = 0; i < nres; i++) {
        uint8_t buf[16]; memset(buf, 0, 16);
        switch (wresults[i].kind) {
            case WASMTIME_I32: buf[0] = 0; memcpy(buf + 8, &wresults[i].of.i32, 4); break;
            case WASMTIME_I64: buf[0] = 1; memcpy(buf + 8, &wresults[i].of.i64, 8); break;
            case WASMTIME_F32: buf[0] = 2; memcpy(buf + 8, &wresults[i].of.f32, 4); break;
            case WASMTIME_F64: buf[0] = 3; memcpy(buf + 8, &wresults[i].of.f64, 8); break;
            default: buf[0] = 0; break;
        }
        if (!wapi_wasm_write_bytes(res_ptr + (uint32_t)(i * 16), buf, 16)) {
            WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
        }
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * get_desc / get_hash / release
 * ============================================================ */

static wasm_trap_t* cb_get_desc(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    /* Descriptor (name + version) metadata isn't baked into every wasm
     * binary. Return an empty descriptor rather than failing. */
    uint32_t desc_ptr = WAPI_ARG_U32(1);
    if (desc_ptr) {
        uint8_t zero[64] = {0};
        wapi_wasm_write_bytes(desc_ptr, zero, sizeof(zero));
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* cb_get_hash(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  mh       = WAPI_ARG_I32(0);
    uint32_t out_ptr  = WAPI_ARG_U32(1);
    if (!wapi_handle_valid(mh, WAPI_HTYPE_MODULE) || !out_ptr) {
        WAPI_RET_I32(WAPI_ERR_BADF); return NULL;
    }
    wapi_module_slot_t* slot = g_rt.handles[mh].data.module_slot;
    if (!slot) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_wasm_write_bytes(out_ptr, slot->hash, 32);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* cb_release(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t mh = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(mh, WAPI_HTYPE_MODULE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    slot_free(g_rt.handles[mh].data.module_slot);
    wapi_handle_free(mh);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Shared memory imports
 * ============================================================ */

static wasm_trap_t* cb_shared_alloc(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint64_t size  = WAPI_ARG_U64(0);
    uint64_t align = WAPI_ARG_U64(1);
    WAPI_RET_I64((int64_t)shared_alloc_internal(size, align));
    return NULL;
}

static wasm_trap_t* cb_shared_free(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint64_t off = WAPI_ARG_U64(0);
    if (off == 0) { WAPI_RET_I32(WAPI_OK); return NULL; }
    wapi_shared_alloc_t* a = shared_find(off);
    if (!a) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }
    a->in_use = false;
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* cb_shared_realloc(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint64_t off      = WAPI_ARG_U64(0);
    uint64_t new_size = WAPI_ARG_U64(1);
    uint64_t align    = WAPI_ARG_U64(2);
    if (off == 0) {
        WAPI_RET_I64((int64_t)shared_alloc_internal(new_size, align));
        return NULL;
    }
    if (new_size == 0) {
        wapi_shared_alloc_t* a = shared_find(off);
        if (a) a->in_use = false;
        WAPI_RET_I64(0);
        return NULL;
    }
    wapi_shared_alloc_t* a = shared_find(off);
    if (!a) { WAPI_RET_I64(0); return NULL; }
    uint64_t new_off = shared_alloc_internal(new_size, align);
    if (!new_off) { WAPI_RET_I64(0); return NULL; }
    uint64_t copy = (a->size < new_size) ? a->size : new_size;
    memcpy(g_rt.shared_mem.bytes + new_off,
           g_rt.shared_mem.bytes + off, (size_t)copy);
    a->in_use = false;
    WAPI_RET_I64((int64_t)new_off);
    return NULL;
}

static wasm_trap_t* cb_shared_usable_size(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint64_t off = WAPI_ARG_U64(0);
    wapi_shared_alloc_t* a = shared_find(off);
    WAPI_RET_I64(a ? (int64_t)a->size : 0);
    return NULL;
}

/* Resolve the caller's exported `memory`. The parent and each child
 * module have their own private memory-0; host callbacks that touch a
 * guest pointer arriving from this call must dereference through the
 * *caller's* memory, not g_rt.memory (which is always the parent's). */
static bool caller_memory_bytes(wasmtime_caller_t* caller,
                                uint8_t** out_base, size_t* out_size)
{
    wasmtime_extern_t ext;
    if (!wasmtime_caller_export_get(caller, "memory", 6, &ext) ||
        ext.kind != WASMTIME_EXTERN_MEMORY) return false;
    wasmtime_context_t* ctx = wasmtime_caller_context(caller);
    *out_base = wasmtime_memory_data(ctx, &ext.of.memory);
    *out_size = wasmtime_memory_data_size(ctx, &ext.of.memory);
    return true;
}

static wasm_trap_t* cb_shared_read(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)nargs; (void)nresults;
    uint64_t src_off = WAPI_ARG_U64(0);
    uint32_t dst_ptr = WAPI_ARG_U32(1);
    uint64_t len     = WAPI_ARG_U64(2);
    if (!shared_mem_ensure() || src_off + len > g_rt.shared_mem.capacity) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    uint8_t* base; size_t size;
    if (!caller_memory_bytes(caller, &base, &size) ||
        (uint64_t)dst_ptr + len > size) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    if (len) memcpy(base + dst_ptr, g_rt.shared_mem.bytes + src_off, (size_t)len);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* cb_shared_write(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)nargs; (void)nresults;
    uint64_t dst_off = WAPI_ARG_U64(0);
    uint32_t src_ptr = WAPI_ARG_U32(1);
    uint64_t len     = WAPI_ARG_U64(2);
    if (!shared_mem_ensure() || dst_off + len > g_rt.shared_mem.capacity) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    uint8_t* base; size_t size;
    if (!caller_memory_bytes(caller, &base, &size) ||
        (uint64_t)src_ptr + len > size) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    if (len) memcpy(g_rt.shared_mem.bytes + dst_off, base + src_ptr, (size_t)len);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Borrows / explicit copy / cache — deferred per spec (NOSYS OK)
 * ============================================================ */

#define CB_NOTSUP_I32(NAME) \
    static wasm_trap_t* NAME(void* env, wasmtime_caller_t* caller, \
        const wasmtime_val_t* args, size_t nargs,                   \
        wasmtime_val_t* results, size_t nresults) {                 \
        (void)env;(void)caller;(void)args;(void)nargs;(void)nresults; \
        WAPI_RET_I32(WAPI_ERR_NOTSUP); return NULL; }

CB_NOTSUP_I32(cb_lend)
CB_NOTSUP_I32(cb_reclaim)
CB_NOTSUP_I32(cb_copy_in)
CB_NOTSUP_I32(cb_prefetch)

static wasm_trap_t* cb_is_cached(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t hash_ptr = WAPI_ARG_U32(0);
    if (!hash_ptr) { WAPI_RET_I32(0); return NULL; }
    void* hp = wapi_wasm_ptr(hash_ptr, 32);
    if (!hp) { WAPI_RET_I32(0); return NULL; }
    uint8_t hash[32]; memcpy(hash, hp, 32);
    WAPI_RET_I32(cache_find(hash) ? 1 : 0);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_module(wasmtime_linker_t* linker) {
    WAPI_DEFINE_3_1(linker, MOD, "load",     cb_load);
    WAPI_DEFINE_4_1(linker, MOD, "join",     cb_join);
    WAPI_DEFINE_3_1(linker, MOD, "get_func", cb_get_func);
    WAPI_DEFINE_2_1(linker, MOD, "get_desc", cb_get_desc);
    WAPI_DEFINE_2_1(linker, MOD, "get_hash", cb_get_hash);
    WAPI_DEFINE_1_1(linker, MOD, "release",  cb_release);

    /* call: (i32 mod, i32 func, i32 args, i64 nargs, i32 res, i64 nres) -> i32 */
    wapi_linker_define(linker, MOD, "call", cb_call,
        6, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I64,WASM_I32,WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});

    /* Shared memory */
    wapi_linker_define(linker, MOD, "shared_alloc", cb_shared_alloc,
        2, (wasm_valkind_t[]){WASM_I64,WASM_I64},
        1, (wasm_valkind_t[]){WASM_I64});
    wapi_linker_define(linker, MOD, "shared_free", cb_shared_free,
        1, (wasm_valkind_t[]){WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, MOD, "shared_realloc", cb_shared_realloc,
        3, (wasm_valkind_t[]){WASM_I64,WASM_I64,WASM_I64},
        1, (wasm_valkind_t[]){WASM_I64});
    wapi_linker_define(linker, MOD, "shared_usable_size", cb_shared_usable_size,
        1, (wasm_valkind_t[]){WASM_I64},
        1, (wasm_valkind_t[]){WASM_I64});
    wapi_linker_define(linker, MOD, "shared_read", cb_shared_read,
        3, (wasm_valkind_t[]){WASM_I64,WASM_I32,WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, MOD, "shared_write", cb_shared_write,
        3, (wasm_valkind_t[]){WASM_I64,WASM_I32,WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});

    /* Borrow / copy / cache */
    wapi_linker_define(linker, MOD, "lend", cb_lend,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I64,WASM_I32,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, MOD, "reclaim", cb_reclaim);
    wapi_linker_define(linker, MOD, "copy_in", cb_copy_in,
        4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I64,WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, MOD, "is_cached", cb_is_cached);
    WAPI_DEFINE_2_1(linker, MOD, "prefetch",  cb_prefetch);
}
