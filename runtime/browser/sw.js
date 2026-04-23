// WAPI extension background service worker.
//
// Hosts the cross-origin wasm module cache: hash-addressed modules
// shared across every site that uses WAPI, plus per-module hit/miss
// counters surfaced in the popup.
//
// Two object stores in one IDB database (wapi-cache):
//   modules — metadata records: {hash, url, size, addedAt, lastUsedAt, hits, misses}
//   bytes   — {hash, bytes: Uint8Array} — the actual wasm payloads
//
// The split lets the popup page through metadata cheaply without
// dragging megabytes of wasm into memory. Listing only touches
// `modules`; the `bytes` store is only read on explicit fetch.
//
// Message API (all sent via chrome.runtime.sendMessage):
//   modules.list                       → Array<MetaRecord>
//   modules.stats                      → {count, totalSize, totalHits, totalMisses, hitrate}
//   modules.get   {hash}               → MetaRecord | null
//   modules.fetch {hash}               → {bytesB64, size} | null  (bumps hits on hit)
//   modules.store {hash, url, bytesB64}→ MetaRecord              (bumps misses, stores bytes)
//   modules.clear                      → undefined
//
// Bytes cross the chrome.runtime.sendMessage boundary as base64
// because chrome.runtime uses JSON, not structured clone. They are
// stored natively as Uint8Array in IDB, so the base64 hop only
// happens at the messaging boundary.
//
// Services API:
//   services.join    {hashHex, name, url}   → {handle} | {error}
//   services.release {handle}               → {ok: bool}
//   services.list                           → Array<ServiceRecord>
//   services.stats                          → {count, totalUsers}
//
// A service is a shared, long-lived wasm instance hosted inside this
// SW via sw_host.js. Joins are refcounted by (hash, name); a service
// is torn down when its refcount hits zero. Modules that import any
// capability sw_host doesn't provide will fail to instantiate — the
// correct "this isn't a headless service" signal.

importScripts('./sw_host.js');

const DB_NAME = 'wapi-cache';
const DB_VERSION = 2;
const META_STORE = 'modules';
const BYTES_STORE = 'bytes';

function openDB() {
    return new Promise((resolve, reject) => {
        const req = indexedDB.open(DB_NAME, DB_VERSION);
        req.onupgradeneeded = () => {
            const db = req.result;
            if (!db.objectStoreNames.contains(META_STORE)) {
                db.createObjectStore(META_STORE, { keyPath: 'hash' });
            }
            if (!db.objectStoreNames.contains(BYTES_STORE)) {
                db.createObjectStore(BYTES_STORE, { keyPath: 'hash' });
            }
        };
        req.onsuccess = () => resolve(req.result);
        req.onerror = () => reject(req.error);
    });
}

function metaStore(db, mode = 'readonly') {
    return db.transaction(META_STORE, mode).objectStore(META_STORE);
}

function bytesStore(db, mode = 'readonly') {
    return db.transaction(BYTES_STORE, mode).objectStore(BYTES_STORE);
}

function pwrap(req) {
    return new Promise((resolve, reject) => {
        req.onsuccess = () => resolve(req.result);
        req.onerror = () => reject(req.error);
    });
}

// Chunked base64 encode — String.fromCharCode.apply blows the call
// stack above ~100k entries, so we walk the Uint8Array in 32 KB slices.
function bytesToB64(u8) {
    let s = '';
    const CHUNK = 0x8000;
    for (let i = 0; i < u8.length; i += CHUNK) {
        s += String.fromCharCode.apply(null, u8.subarray(i, i + CHUNK));
    }
    return btoa(s);
}

function b64ToBytes(b64) {
    const bin = atob(b64);
    const u8 = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) u8[i] = bin.charCodeAt(i);
    return u8;
}

async function listAll() {
    const db = await openDB();
    return pwrap(metaStore(db).getAll());
}

async function getOne(hash) {
    const db = await openDB();
    return pwrap(metaStore(db).get(hash));
}

async function clearAll() {
    const db = await openDB();
    await pwrap(metaStore(db, 'readwrite').clear());
    await pwrap(bytesStore(db, 'readwrite').clear());
}

// Cache lookup. Returns bytes + size on hit, null on miss. On hit,
// bumps the hit counter and lastUsedAt on the metadata record.
async function cacheFetch({ hash }) {
    if (!hash) throw new Error('hash required');
    const db = await openDB();
    const rec = await pwrap(bytesStore(db).get(hash));
    if (!rec) return null;
    const mtx = metaStore(db, 'readwrite');
    const meta = await pwrap(mtx.get(hash));
    if (meta) {
        meta.hits = (meta.hits || 0) + 1;
        meta.lastUsedAt = Date.now();
        await pwrap(mtx.put(meta));
    }
    return { bytesB64: bytesToB64(rec.bytes), size: rec.bytes.length };
}

// Cache insert. Called when the shim fetched bytes from the network
// (or decoded them from a supplied ArrayBuffer). Stores fresh bytes
// on first sight and counts a miss. If the hash is already cached,
// treats this as a hit — the caller didn't consult cacheFetch first
// (e.g. the page pre-fetched the wasm because it didn't know the
// hash upfront), so crediting it as a hit avoids the misleading
// "miss counter goes up on every reload" effect.
async function cacheStore({ hash, url, bytesB64 }) {
    if (!hash) throw new Error('hash required');
    if (!bytesB64) throw new Error('bytesB64 required');
    const bytes = b64ToBytes(bytesB64);
    const db = await openDB();
    const mtx = metaStore(db, 'readwrite');
    const now = Date.now();
    let meta = await pwrap(mtx.get(hash));
    const alreadyCached = !!meta;

    if (!alreadyCached) {
        await pwrap(bytesStore(db, 'readwrite').put({ hash, bytes }));
        meta = {
            hash,
            url: url || '',
            size: bytes.length,
            addedAt: now,
            lastUsedAt: now,
            hits: 0,
            misses: 1,
        };
    } else {
        meta.hits = (meta.hits || 0) + 1;
        meta.lastUsedAt = now;
        if (url && !meta.url) meta.url = url;
        meta.size = bytes.length;
    }
    await pwrap(mtx.put(meta));
    return meta;
}

async function statsAll() {
    const list = await listAll();
    let totalSize = 0, totalHits = 0, totalMisses = 0;
    for (const m of list) {
        totalSize += m.size || 0;
        totalHits += m.hits || 0;
        totalMisses += m.misses || 0;
    }
    const total = totalHits + totalMisses;
    return {
        count: list.length,
        totalSize, totalHits, totalMisses,
        hitrate: total ? totalHits / total : 0,
    };
}

// -----------------------------------------------------------------------------
// Services manager
// -----------------------------------------------------------------------------
//
// services: (hash, name) → ServiceRecord. A ServiceRecord is the compiled
// Module + live Instance + host bindings + outstanding handles. Keyed by
// "<hashHex>:<name>" so two binaries can share the same `name` without
// stomping each other.
//
// handles: handle → ServiceRecord. Every services.join allocates a fresh
// handle. services.release decrements the record's refcount by 1; when it
// reaches zero, the record is dropped.

const services = new Map();
const serviceHandles = new Map();
let nextServiceHandle = 1;

// Display-only registry of services that are hosted inside a page (the
// MAIN-world shim instantiates runtime-linked modules there for speed and
// full WAPI-import coverage). The SW never instantiates these — it just
// tracks announcements so the popup can show what's currently live.
// Key: "<hashHex>:<name>". Value: {hashHex, name, url, origin, announcedAt,
// lastHeartbeat, refcount}. Entries age out when not heartbeated.
const announcedServices = new Map();
const ANNOUNCE_STALE_MS = 15000;

function reapAnnounced() {
    const now = Date.now();
    for (const [key, svc] of announcedServices) {
        if (now - svc.lastHeartbeat > ANNOUNCE_STALE_MS) {
            announcedServices.delete(key);
        }
    }
}

function servicesAnnounce({ hashHex, name, url, origin, refcount }) {
    if (!hashHex) return { ok: false };
    const key = serviceKey(hashHex, name);
    const now = Date.now();
    const existing = announcedServices.get(key);
    if (existing) {
        existing.lastHeartbeat = now;
        if (url)    existing.url = url;
        if (origin) existing.origin = origin;
        if (typeof refcount === 'number') existing.refcount = refcount;
    } else {
        announcedServices.set(key, {
            hashHex,
            name: name || '',
            url: url || '',
            origin: origin || '',
            announcedAt: now,
            lastHeartbeat: now,
            refcount: typeof refcount === 'number' ? refcount : 1,
        });
    }
    return { ok: true };
}

function servicesWithdraw({ hashHex, name }) {
    announcedServices.delete(serviceKey(hashHex, name));
    return { ok: true };
}

// Tracks (hash, name) keys currently mid-instantiation, so cycles during
// nested joins fail fast with WAPI_ERR_LOOP equivalence. Wired but unused
// in v1 — sw_host.js wapi_module.join is still NOTSUP, so no nested joins
// happen yet. Kept so the shape is right when that lands.
const servicesInstantiating = new Set();

function serviceKey(hashHex, name) {
    return hashHex + ':' + (name || '');
}

function hexFromBytes(u8) {
    let s = '';
    for (let i = 0; i < u8.length; i++) {
        s += u8[i].toString(16).padStart(2, '0');
    }
    return s;
}

async function cacheGetBytes(hashHex) {
    const db = await openDB();
    const rec = await pwrap(bytesStore(db).get(hashHex));
    return rec ? rec.bytes : null;
}

async function servicesJoin({ hashHex, name, url }) {
    if (!hashHex) throw new Error('hashHex required');
    const key = serviceKey(hashHex, name);

    if (servicesInstantiating.has(key)) {
        // Cycle: someone's join triggered a nested join of the same service
        // before the first one finished. WAPI_ERR_LOOP semantics.
        return { error: 'WAPI_ERR_LOOP', code: -33 };
    }

    let svc = services.get(key);
    if (!svc) {
        let bytes = await cacheGetBytes(hashHex);
        if (!bytes) {
            // Cache miss: the SW fetches the url itself, verifies SHA-256
            // against the declared hash, and stores before proceeding.
            // With no host_permissions this is subject to normal CORS; it
            // works for same-origin URLs and any endpoint that returns
            // permissive CORS headers. The page-side shim doesn't have to
            // pre-populate the cache for services.
            if (!url) return { error: 'service bytes not in cache and no url supplied' };
            try {
                const resp = await fetch(url);
                if (!resp.ok) return { error: `fetch ${url} → ${resp.status}` };
                bytes = new Uint8Array(await resp.arrayBuffer());
                const digest = new Uint8Array(
                    await crypto.subtle.digest('SHA-256', bytes)
                );
                const gotHex = hexFromBytes(digest);
                if (gotHex !== hashHex) {
                    return { error: `hash mismatch: want ${hashHex}, got ${gotHex}` };
                }
                // Populate the shared bytes cache so subsequent loads /
                // joins hit. A miss counter bump is the correct signal —
                // this was a real cache miss.
                await cacheStore({ hash: hashHex, url, bytesB64: bytesToB64(bytes) });
            } catch (e) {
                return { error: String(e && e.message || e) };
            }
        }

        servicesInstantiating.add(key);
        try {
            const module = await WebAssembly.compile(bytes);
            const host = self.createSwHost({
                name: name || hashHex.slice(0, 8),
                moduleHash: bytes.slice(0, 32), // unused in v1, hash is hex
                onLog: (level, msg) => {
                    const c = console[level] || console.log;
                    c.call(console, `[svc:${name || hashHex.slice(0, 8)}]`, msg);
                },
                onExit: (code) => {
                    console.log(`[svc:${name || hashHex.slice(0, 8)}] exit ${code}`);
                    removeService(key);
                },
            });
            const instance = await WebAssembly.instantiate(module, host.imports);
            host.bindInstance(instance);

            // Module init sequence: _initialize (if exported) then wapi_main.
            // Exceptions from wapi_main are logged but non-fatal; the module
            // may have chosen to exit via wapi_env.exit which surfaces via
            // onExit and already triggers teardown.
            try {
                if (instance.exports._initialize) instance.exports._initialize();
                if (instance.exports.wapi_main) instance.exports.wapi_main();
            } catch (e) {
                console.warn(`[svc:${name}] init threw`, e);
            }

            svc = {
                hashHex,
                name: name || '',
                url: url || '',
                module,
                instance,
                host,
                refcount: 0,
                handles: new Set(),
                startedAt: Date.now(),
            };
            services.set(key, svc);
        } finally {
            servicesInstantiating.delete(key);
        }
    }

    const handle = nextServiceHandle++;
    serviceHandles.set(handle, svc);
    svc.handles.add(handle);
    svc.refcount++;
    return { handle };
}

function servicesRelease({ handle }) {
    const svc = serviceHandles.get(handle);
    if (!svc) return { ok: false };
    serviceHandles.delete(handle);
    svc.handles.delete(handle);
    svc.refcount--;
    if (svc.refcount <= 0) {
        removeService(serviceKey(svc.hashHex, svc.name));
    }
    return { ok: true };
}

function removeService(key) {
    const svc = services.get(key);
    if (!svc) return;
    // v1: drop references and let GC reclaim. Shutdown handshake
    // (WAPI_EVENT_TERMINATING → grace window → wapi_exit → drop) is
    // specified in wapi.h but not plumbed through sw_host.js yet.
    for (const h of svc.handles) serviceHandles.delete(h);
    services.delete(key);
}

function servicesList() {
    reapAnnounced();
    const now = Date.now();
    const rows = Array.from(services.values()).map((svc) => ({
        hashHex: svc.hashHex,
        name: svc.name,
        url: svc.url,
        users: svc.refcount,
        pending: svc.host.pendingCount(),
        startedAt: svc.startedAt,
        uptimeMs: now - svc.startedAt,
        hostedIn: 'sw',
    }));
    for (const svc of announcedServices.values()) {
        rows.push({
            hashHex: svc.hashHex,
            name: svc.name,
            url: svc.url,
            origin: svc.origin,
            users: svc.refcount,
            pending: 0,
            startedAt: svc.announcedAt,
            uptimeMs: now - svc.announcedAt,
            hostedIn: 'page',
        });
    }
    return rows;
}

function servicesStats() {
    reapAnnounced();
    let totalUsers = 0, totalPending = 0;
    for (const svc of services.values()) {
        totalUsers += svc.refcount;
        totalPending += svc.host.pendingCount();
    }
    for (const svc of announcedServices.values()) {
        totalUsers += svc.refcount;
    }
    return {
        count: services.size + announcedServices.size,
        totalUsers,
        totalPending,
    };
}

const handlers = {
    'modules.list':  ()  => listAll(),
    'modules.stats': ()  => statsAll(),
    'modules.get':   (m) => getOne(m.hash),
    'modules.fetch': (m) => cacheFetch(m),
    'modules.store': (m) => cacheStore(m),
    'modules.clear': ()  => clearAll(),

    'services.join':     (m) => servicesJoin(m),
    'services.release':  (m) => servicesRelease(m),
    'services.list':     ()  => servicesList(),
    'services.stats':    ()  => servicesStats(),
    /* Page-hosted service observability. The SW doesn't instantiate
     * these — the application's memory 1 lives in the page by spec
     * (§10 memory model), so the instance must live there too. The
     * page announces (with heartbeat) and withdraws so the popup can
     * show cross-tab activity. */
    'services.announce': (m) => servicesAnnounce(m),
    'services.withdraw': (m) => servicesWithdraw(m),
};

chrome.runtime.onMessage.addListener((msg, _sender, sendResponse) => {
    const fn = handlers[msg && msg.type];
    if (!fn) return false;
    Promise.resolve(fn(msg))
        .then((r) => sendResponse(r ?? null))
        .catch((e) => sendResponse({ error: e.message || String(e) }));
    return true; // keep channel open for async response
});

self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (e) => e.waitUntil(self.clients.claim()));
