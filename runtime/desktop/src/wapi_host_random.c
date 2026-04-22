/**
 * WAPI Desktop Runtime — Random (wapi_random module)
 *
 * Cryptographically-secure entropy backed by the OS RNG. Per
 * wapi_random.h, random is its own capability rather than part of
 * wapi_env, so a runtime that can't supply entropy reports
 * WAPI_CAP_RANDOM as DENIED without having to disable all of wapi_env.
 *
 * Windows: BCryptGenRandom with BCRYPT_USE_SYSTEM_PREFERRED_RNG — always
 * ready after boot, cannot block, so `get` and `get_nonblock` behave
 * identically on this backend.
 *
 * macOS: SecRandomCopyBytes (SecRandom.h, Security.framework).
 *
 * Linux: getrandom(2) with / without GRND_NONBLOCK depending on the
 * requested variant, falling back to /dev/urandom if getrandom is
 * unavailable.
 *
 * `fill_seed` shares the cryptographic source on hosts that have one.
 * Bare-metal backends (not shipped in this tree) override just that
 * entrypoint with a boot-time entropy mix.
 */

#include "wapi_host.h"

#ifdef _WIN32
#  include <windows.h>
#  include <bcrypt.h>
#  pragma comment(lib, "bcrypt.lib")
#elif defined(__APPLE__)
#  include <Security/SecRandom.h>
#else
#  include <fcntl.h>
#  include <unistd.h>
#  include <errno.h>
#  include <sys/syscall.h>
#  ifdef __linux__
#    include <sys/random.h>
#  endif
#endif

/* ============================================================
 * Backends
 * ============================================================
 * Returns 0 on success, >0 errno-ish value on failure. If
 * `nonblock` is true and the kernel RNG is not yet seeded, the
 * Linux backend returns EAGAIN instead of blocking.
 */

static int fill_random_bytes(void* buf, size_t len, bool nonblock) {
    if (len == 0) return 0;

#ifdef _WIN32
    (void)nonblock; /* BCryptGenRandom never blocks on Windows. */
    NTSTATUS st = BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)len,
                                  BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return BCRYPT_SUCCESS(st) ? 0 : 5 /* EIO */;
#elif defined(__APPLE__)
    (void)nonblock; /* SecRandomCopyBytes is non-blocking. */
    int r = SecRandomCopyBytes(kSecRandomDefault, len, buf);
    return r == errSecSuccess ? 0 : 5;
#else
    uint8_t* p = (uint8_t*)buf;
    size_t remaining = len;

#  ifdef __linux__
    unsigned flags = nonblock ? GRND_NONBLOCK : 0;
    while (remaining > 0) {
        ssize_t n = getrandom(p, remaining, flags);
        if (n > 0) { p += n; remaining -= (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && errno == EAGAIN) return EAGAIN;
        break; /* getrandom unavailable / other error — fall through */
    }
    if (remaining == 0) return 0;
#  endif

    /* Fallback: /dev/urandom. Never blocks on Linux; on other Unixes
     * may block briefly at early boot. If nonblock is requested and
     * the file isn't open-able yet, return EAGAIN. */
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return nonblock ? EAGAIN : 5;
    while (remaining > 0) {
        ssize_t n = read(fd, p, remaining);
        if (n > 0) { p += n; remaining -= (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        close(fd);
        return 5;
    }
    close(fd);
    return 0;
#endif
}

/* ============================================================
 * Imports
 * ============================================================ */

static wasm_trap_t* cb_random_get(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t buf_ptr = WAPI_ARG_U32(0);
    uint64_t len     = WAPI_ARG_U64(1);
    if (len == 0) { WAPI_RET_I32(WAPI_OK); return NULL; }
    void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)len);
    if (!buf) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    int r = fill_random_bytes(buf, (size_t)len, false);
    if (r == 0)                    WAPI_RET_I32(WAPI_OK);
    else if (r == 11 /* EAGAIN */) WAPI_RET_I32(WAPI_ERR_AGAIN);
    else                           WAPI_RET_I32(WAPI_ERR_IO);
    return NULL;
}

static wasm_trap_t* cb_random_get_nonblock(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t buf_ptr = WAPI_ARG_U32(0);
    uint64_t len     = WAPI_ARG_U64(1);
    if (len == 0) { WAPI_RET_I32(WAPI_OK); return NULL; }
    void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)len);
    if (!buf) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    int r = fill_random_bytes(buf, (size_t)len, true);
    if (r == 0)                  WAPI_RET_I32(WAPI_OK);
    else if (r == 11 /* EAGAIN */) WAPI_RET_I32(WAPI_ERR_AGAIN);
    else                         WAPI_RET_I32(WAPI_ERR_IO);
    return NULL;
}

static wasm_trap_t* cb_random_fill_seed(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    /* Desktop: same source as the cryptographic path. Embedded hosts
     * (not this tree) override with a boot-time entropy mix when no
     * secure RNG exists. */
    return cb_random_get(env, caller, args, nargs, results, nresults);
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_random(wasmtime_linker_t* linker) {
    /* get(buf: i32, len: i64) -> i32 */
    wapi_linker_define(linker, "wapi_random", "get", cb_random_get,
        2, (wasm_valkind_t[]){WASM_I32, WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});

    wapi_linker_define(linker, "wapi_random", "get_nonblock", cb_random_get_nonblock,
        2, (wasm_valkind_t[]){WASM_I32, WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});

    wapi_linker_define(linker, "wapi_random", "fill_seed", cb_random_fill_seed,
        2, (wasm_valkind_t[]){WASM_I32, WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});
}
