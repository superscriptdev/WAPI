// WAPI extension background service worker.
//
// Hosts the cross-origin wasm module cache: hash-addressed modules
// shared across every site that uses WAPI, plus per-module hit/miss
// counters surfaced in the popup. Today only metadata is tracked —
// the actual wasm bytes will land in Cache API (or a `bytes` IDB
// store) once wapi_module.load is wired through the shim. The
// message API below is the contract that wiring will use.
//
// Producers (future): an isolated-world content_script bridge will
// receive window.postMessage events from the shim (which runs in the
// page MAIN world and has no chrome.runtime access) and forward them
// here. The popup is a direct consumer.

const DB_NAME = 'wapi-cache';
const DB_VERSION = 1;
const STORE = 'modules';

function openDB() {
    return new Promise((resolve, reject) => {
        const req = indexedDB.open(DB_NAME, DB_VERSION);
        req.onupgradeneeded = () => {
            const db = req.result;
            if (!db.objectStoreNames.contains(STORE)) {
                db.createObjectStore(STORE, { keyPath: 'hash' });
            }
        };
        req.onsuccess = () => resolve(req.result);
        req.onerror = () => reject(req.error);
    });
}

function store(db, mode = 'readonly') {
    return db.transaction(STORE, mode).objectStore(STORE);
}

function pwrap(req) {
    return new Promise((resolve, reject) => {
        req.onsuccess = () => resolve(req.result);
        req.onerror = () => reject(req.error);
    });
}

async function listAll() {
    const db = await openDB();
    return pwrap(store(db).getAll());
}

async function getOne(hash) {
    const db = await openDB();
    return pwrap(store(db).get(hash));
}

async function clearAll() {
    const db = await openDB();
    return pwrap(store(db, 'readwrite').clear());
}

async function registerModule({ hash, url, size }) {
    if (!hash) throw new Error('hash required');
    const db = await openDB();
    const s = store(db, 'readwrite');
    const existing = await pwrap(s.get(hash));
    const now = Date.now();
    const rec = existing || {
        hash, url: url || '', size: size || 0,
        addedAt: now, lastUsedAt: now,
        hits: 0, misses: 0,
    };
    if (existing) {
        if (url) rec.url = url;
        if (size) rec.size = size;
    }
    await pwrap(s.put(rec));
    return rec;
}

async function bumpHit({ hash }) {
    if (!hash) throw new Error('hash required');
    const db = await openDB();
    const s = store(db, 'readwrite');
    const rec = await pwrap(s.get(hash));
    if (!rec) return null;
    rec.hits = (rec.hits || 0) + 1;
    rec.lastUsedAt = Date.now();
    await pwrap(s.put(rec));
    return rec;
}

// Producer-friendly upsert: first sighting of a hash counts as a
// miss + register; every subsequent sighting counts as a hit. This
// is the model the page-side shim uses today (it always fetches and
// just records what it saw); a future cache-aware path can call
// `modules.hit` / `modules.miss` directly to distinguish "served
// from cache" from "fetched from network".
async function touch({ hash, url, size }) {
    if (!hash) throw new Error('hash required');
    const db = await openDB();
    const s = store(db, 'readwrite');
    const rec = await pwrap(s.get(hash));
    const now = Date.now();
    if (!rec) {
        const fresh = {
            hash, url: url || '', size: size || 0,
            addedAt: now, lastUsedAt: now,
            hits: 0, misses: 1,
        };
        await pwrap(s.put(fresh));
        return fresh;
    }
    rec.hits = (rec.hits || 0) + 1;
    rec.lastUsedAt = now;
    if (url && !rec.url) rec.url = url;
    if (size && !rec.size) rec.size = size;
    await pwrap(s.put(rec));
    return rec;
}

async function bumpMiss({ hash, url, size }) {
    if (!hash) throw new Error('hash required');
    const db = await openDB();
    const s = store(db, 'readwrite');
    let rec = await pwrap(s.get(hash));
    const now = Date.now();
    if (!rec) {
        rec = {
            hash, url: url || '', size: size || 0,
            addedAt: now, lastUsedAt: now,
            hits: 0, misses: 1,
        };
    } else {
        rec.misses = (rec.misses || 0) + 1;
        rec.lastUsedAt = now;
        if (url) rec.url = url;
        if (size) rec.size = size;
    }
    await pwrap(s.put(rec));
    return rec;
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

const handlers = {
    'modules.list':     ()  => listAll(),
    'modules.stats':    ()  => statsAll(),
    'modules.get':      (m) => getOne(m.hash),
    'modules.register': (m) => registerModule(m),
    'modules.touch':    (m) => touch(m),
    'modules.hit':      (m) => bumpHit(m),
    'modules.miss':     (m) => bumpMiss(m),
    'modules.clear':    ()  => clearAll(),
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
