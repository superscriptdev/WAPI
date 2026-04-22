/**
 * WAPI - Random
 * Version 1.0.0
 *
 * Cryptographically-secure random bytes and PRNG seeding.
 *
 * Random is its own capability (WAPI_CAP_RANDOM) rather than part
 * of wapi_env because not every runtime can supply it:
 *
 *   - Desktop hosts: BCryptGenRandom / getrandom(2) / SecRandomCopyBytes.
 *   - Bare-metal embedded: may have a hardware TRNG, may not. A host
 *     without a secure source MUST report WAPI_CAP_RANDOM as DENIED
 *     and reject the imports with WAPI_ERR_NOSYS.
 *   - Sandboxed child modules: the parent may deliberately withhold
 *     entropy so child code is deterministic.
 *
 * Guests that need random bytes gate on WAPI_CAP_RANDOM and use the
 * three imports below. The capability has *no* inline helpers — it
 * is a pure host-capability surface.
 *
 * Import module: "wapi_random"
 */

#ifndef WAPI_RANDOM_H
#define WAPI_RANDOM_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Fill a buffer with cryptographically-secure random bytes.
 *
 * May block briefly on entropy-starved hosts (e.g. fresh Linux boot
 * before the kernel RNG has seeded). Hosts that cannot block or have
 * no source at all return WAPI_ERR_UNAVAIL / WAPI_ERR_NOSYS.
 *
 * @param buf  Buffer to fill.
 * @param len  Number of random bytes.
 * @return WAPI_OK on success, WAPI_ERR_NOSYS if no source, WAPI_ERR_AGAIN
 *         if the source is temporarily unready.
 *
 * Wasm signature: (i32, i64) -> i32
 */
WAPI_IMPORT(wapi_random, get)
wapi_result_t wapi_random_get(void* buf, wapi_size_t len);

/**
 * Fill a buffer with cryptographically-secure random bytes, never blocking.
 *
 * Returns WAPI_ERR_AGAIN if the kernel RNG has not yet seeded and the
 * host would otherwise block. On desktop this is essentially identical
 * to wapi_random_get; on Linux it maps to getrandom(..., GRND_NONBLOCK).
 *
 * @param buf  Buffer to fill.
 * @param len  Number of random bytes.
 * @return WAPI_OK, WAPI_ERR_AGAIN (not yet seeded), or WAPI_ERR_NOSYS.
 *
 * Wasm signature: (i32, i64) -> i32
 */
WAPI_IMPORT(wapi_random, get_nonblock)
wapi_result_t wapi_random_get_nonblock(void* buf, wapi_size_t len);

/**
 * Fill a buffer with low-quality seed material for a guest PRNG.
 *
 * Distinct from wapi_random_get because the host may implement this
 * via a boot-time mix (uptime + ADC noise + device serial) even when
 * it cannot supply cryptographic randomness. The bytes are *not*
 * suitable for keys / tokens / nonces — only for seeding deterministic
 * RNGs where the guest wants variety across runs.
 *
 * @param buf  Buffer to fill.
 * @param len  Number of seed bytes.
 * @return WAPI_OK on success, WAPI_ERR_NOSYS if no source at all.
 *
 * Wasm signature: (i32, i64) -> i32
 */
WAPI_IMPORT(wapi_random, fill_seed)
wapi_result_t wapi_random_fill_seed(void* buf, wapi_size_t len);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_RANDOM_H */
