// WAPI headless host, service-worker side.
//
// Parallel implementation of the core WAPI ABI for running service
// modules inside the extension's background service worker. NOT a
// shared-code refactor of wapi_shim.js — the shim is ~6500 lines of
// class state tightly coupled to window/document/canvas/WebGPU. This
// file implements the same wire contracts (memory layouts, opcode
// dispatch, vtable shapes) against only APIs that exist in
// ServiceWorkerGlobalScope.
//
// v1 capability surface (what service modules can import):
//   wapi         — capability/abi/panic/vtable getters
//   wapi_env     — empty args/env, getRandomValues, exit
//   wapi_memory  — bump + free-list allocator (same shape as shim)
//   wapi_io      — submit/cancel/poll/wait/flush with opcode dispatch
//                  supported ops: NOP, LOG, TIMEOUT, HTTP_FETCH,
//                                 CONNECT/SEND/RECV (WebSocket),
//                                 COMPRESS_PROCESS
//                  everything else → WAPI_ERR_NOTSUP
//   wapi_clock   — time_get, resolution, perf_counter, perf_frequency
//                  yield/sleep are no-ops (cannot block SW event loop)
//   wapi_module  — load/prefetch/is_cached/release/get_hash routed to
//                  the SW-local bytes cache (IDB); join deferred to
//                  sw.js since it needs the refcount map
//
// Deferred to v1.1 (modules that import these will fail to link,
// which is the correct "this module is not a headless service"
// signal rather than runtime surprises):
//   wapi_crypto (already NOTSUP in shim — async/sync impedance)
//   wapi_thread (sync primitives), wapi_filesystem, wapi_kvstorage, wapi_sysinfo,
//   wapi_notifications, wapi_codec, and every
//   UI/hardware/user-gesture capability.
//
// Consumed by sw.js via importScripts('./sw_host.js'). Exposes
// self.createSwHost(opts) — classic-script-safe.

(function () {
    'use strict';

    // ---- Constants ---------------------------------------------------------

    const WAPI_OK             = 0;
    const WAPI_ERR_INVAL      = -1;
    const WAPI_ERR_NOMEM      = -2;
    const WAPI_ERR_NOTSUP     = -3;
    const WAPI_ERR_BADF       = -8;
    const WAPI_ERR_NOENT      = -9;
    const WAPI_ERR_AGAIN      = -11;
    const WAPI_ERR_OVERFLOW   = -12;
    const WAPI_ERR_IO         = -5;
    const WAPI_ERR_RANGE      = -14;

    const WAPI_STRLEN         = 0xFFFFFFFF;
    const WAPI_STRLEN64       = 0xFFFFFFFFFFFFFFFFn;

    // wapi_event_type_t (subset the io queue emits)
    const WAPI_EVENT_IO_COMPLETION = 0x200;

    // wapi_io_op_t opcodes (subset — see include/wapi/wapi_io.h)
    const WAPI_IO_OP_NOP              = 0x000;
    const WAPI_IO_OP_READ             = 0x001;
    const WAPI_IO_OP_WRITE            = 0x002;
    const WAPI_IO_OP_LOG              = 0x006;
    const WAPI_IO_OP_CONNECT          = 0x00A;
    const WAPI_IO_OP_SEND             = 0x00C;
    const WAPI_IO_OP_RECV             = 0x00D;
    const WAPI_IO_OP_TIMEOUT          = 0x014;
    const WAPI_IO_OP_HTTP_FETCH       = 0x060;
    const WAPI_IO_OP_COMPRESS_PROCESS = 0x140;

    // wapi_clock_id_t
    const WAPI_CLOCK_REALTIME  = 0;
    const WAPI_CLOCK_MONOTONIC = 1;

    // Event struct size (wapi_io_event_t is 128 bytes).
    const IO_EVENT_SIZE = 128;
    // Op struct size (wapi_io_op_t is 80 bytes).
    const IO_OP_SIZE = 80;

    // ---- Host factory ------------------------------------------------------

    /**
     * Build a WAPI host for running a single service module inside the SW.
     *
     * @param {object} opts
     * @param {string} opts.name            Service name (for logs + exit signaling).
     * @param {Uint8Array} opts.moduleHash  32-byte SHA-256 of the module bytes.
     * @param {Function} [opts.onLog]       (level, msg) logger; defaults to console.
     * @param {Function} [opts.onExit]      Called with exit code when module calls wapi_exit.
     * @param {Function} [opts.loadModuleBytes]  async (hashBytes) => Uint8Array|null for
     *                                           nested wapi_module.load.
     * @returns {{
     *   imports: object,
     *   state: object,
     *   bindInstance: (instance: WebAssembly.Instance) => void,
     *   tick: () => void,
     *   pendingCount: () => number,
     *   exitCode: () => number|null,
     * }}
     */
    function createSwHost(opts) {
        opts = opts || {};
        const logPrefix = `[wapi-sw:${opts.name || 'service'}]`;
        const logFn = opts.onLog || ((level, msg) => {
            const c = console[level] || console.log;
            c.call(console, logPrefix, msg);
        });

        // ---- Module-local state -------------------------------------------

        const state = {
            instance: null,
            memory: null,
            _u8: null,
            _u32: null,
            _dv: null,

            // Host allocator
            _allocBase: 0,
            _allocPtr: 0,
            _freeList: [],

            // Event queue (drained by poll/wait)
            _eventQueue: [],

            // Pending async ops: token → { userData, resultPtr, cancel }
            _ioPending: new Map(),
            _ioNextToken: 1,

            // Cached vtable addresses
            _ioVtablePtr: 0,
            _allocVtablePtr: 0,

            // Exit
            _exitCode: null,

            // Timing origin for perf counters
            _perfOrigin: performance.now(),
        };

        // ---- Memory helpers -----------------------------------------------

        function refreshViews() {
            const buf = state.memory.buffer;
            state._u8  = new Uint8Array(buf);
            state._u32 = new Uint32Array(buf);
            state._dv  = new DataView(buf);
        }

        function readU32(ptr) {
            return state._dv.getUint32(ptr, true);
        }
        function writeU32(ptr, val) {
            state._dv.setUint32(ptr, val >>> 0, true);
        }
        function readU64(ptr) {
            return state._dv.getBigUint64(ptr, true);
        }
        function writeU64(ptr, val) {
            state._dv.setBigUint64(ptr, BigInt(val), true);
        }

        const _decoder = new TextDecoder('utf-8');
        const _encoder = new TextEncoder();

        function readString(ptr, len) {
            if (len === WAPI_STRLEN || len === 0xFFFFFFFF) {
                // Null-terminated scan
                const u8 = state._u8;
                let end = ptr;
                while (end < u8.length && u8[end] !== 0) end++;
                return _decoder.decode(u8.subarray(ptr, end));
            }
            return _decoder.decode(state._u8.subarray(ptr, ptr + len));
        }

        // wapi_string_view_t on wasm32: 8-byte ptr + 8-byte len = 16 bytes.
        // Passed by hidden pointer in wasm32 ABI, so functions that take a
        // string view receive a single i32 pointer to the 16-byte struct.
        function readStringView(svPtr) {
            if (!svPtr) return null;
            const ptr = Number(readU64(svPtr));
            const len = readU64(svPtr + 8);
            if (ptr === 0 && len === WAPI_STRLEN64) return null;
            return readString(ptr, Number(len));
        }

        function writeString(ptr, maxLen, s) {
            if (!ptr || maxLen <= 0) return 0;
            const bytes = _encoder.encode(s);
            const n = Math.min(bytes.length, maxLen);
            state._u8.set(bytes.subarray(0, n), ptr);
            return n;
        }

        // Bump + free-list allocator. Grows wasm memory when exhausted.
        function hostAlloc(size, align) {
            if (size <= 0) return 0;
            align = align || 4;

            // Try free-list first
            for (let i = 0; i < state._freeList.length; i++) {
                const blk = state._freeList[i];
                if (blk.size >= size) {
                    const aligned = (blk.ptr + align - 1) & ~(align - 1);
                    const waste = aligned - blk.ptr;
                    if (blk.size - waste >= size) {
                        state._freeList.splice(i, 1);
                        return aligned;
                    }
                }
            }

            // Bump
            let ptr = (state._allocPtr + align - 1) & ~(align - 1);
            const end = ptr + size;
            const bufLen = state.memory.buffer.byteLength;
            if (end > bufLen) {
                const pages = Math.ceil((end - bufLen) / 65536);
                if (state.memory.grow(pages) === -1) return 0;
                refreshViews();
            }
            state._allocPtr = end;
            return ptr;
        }

        function hostFree(ptr) {
            if (!ptr) return;
            // Free-list entries carry size inferred from tracked allocations.
            // Bump allocator can't reclaim, so we just track the block for
            // potential exact-size reuse.
            // Size unknown here — drop on the floor. Modules that care should
            // use a real allocator exported from wasm.
        }

        // Build a typed wasm function that wraps a JS callable. Used to
        // populate __indirect_function_table entries for the vtables exposed
        // by wapi.io_get / wapi.allocator_get.
        //
        // Synthesizes a minimal wasm module on the fly: one import (the JS
        // function), one export ("w") that forwards to the import with the
        // declared signature.
        function makeWasmFunc(params, results, impl) {
            // Build type section: (param ... ) (result ...)
            const typeBytes = [0x60]; // func
            typeBytes.push(params.length);
            for (const p of params) typeBytes.push(p === 'i64' ? 0x7e : p === 'f32' ? 0x7d : p === 'f64' ? 0x7c : 0x7f);
            typeBytes.push(results.length);
            for (const r of results) typeBytes.push(r === 'i64' ? 0x7e : r === 'f32' ? 0x7d : r === 'f64' ? 0x7c : 0x7f);

            // Sections: type (1), import (2), export (7)
            const section = (id, body) => [id, body.length, ...body];

            const typeSec  = section(1, [1, ...typeBytes]); // 1 type
            const importSec = section(2, [
                1,                 // 1 import
                1, 0x68,           // "h" (module)
                1, 0x66,           // "f" (field)
                0, 0,              // func, typeidx 0
            ]);
            const exportSec = section(7, [
                1,                 // 1 export
                1, 0x77,           // "w"
                0, 0,              // func, funcidx 0
            ]);

            const header = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
            const bytes = new Uint8Array([...header, ...typeSec, ...importSec, ...exportSec]);

            const mod = new WebAssembly.Module(bytes);
            const inst = new WebAssembly.Instance(mod, { h: { f: impl } });
            return inst.exports.w;
        }

        // ---- Event queue --------------------------------------------------

        function pushIoCompletion(userData, result, flags) {
            state._eventQueue.push({
                type: WAPI_EVENT_IO_COMPLETION,
                userData: BigInt(userData),
                result: result | 0,
                flags: (flags || 0) >>> 0,
            });
        }

        // Write a wapi_io_event_t (128 bytes) into wasm memory at eventPtr.
        // Only the fields the service worker emits are populated; the rest
        // is zeroed.
        function writeEvent(eventPtr, ev) {
            // Zero the full event buffer first.
            state._u8.fill(0, eventPtr, eventPtr + IO_EVENT_SIZE);
            writeU32(eventPtr + 0, ev.type);
            // Padding at +4
            writeU64(eventPtr + 8, ev.userData);
            writeU32(eventPtr + 16, ev.result);
            writeU32(eventPtr + 20, ev.flags);
        }

        // ---- Op dispatch --------------------------------------------------

        // Dispatches a single op from linear memory. Sync ops push their
        // completion immediately; async ops register in _ioPending and resolve
        // later via pushIoCompletion.
        function dispatchOp(opPtr) {
            refreshViews();
            const opcode   = readU32(opPtr + 0);
            // flags        = readU32(opPtr + 4);
            const fd       = readU32(opPtr + 8) | 0;
            // flags2       = readU32(opPtr + 12);
            const offset   = readU64(opPtr + 16);
            const addr     = Number(readU64(opPtr + 24));
            const len      = Number(readU64(opPtr + 32));
            const addr2    = Number(readU64(opPtr + 40));
            const len2     = Number(readU64(opPtr + 48));
            const userData = readU64(opPtr + 56);
            const resultPtr = Number(readU64(opPtr + 64));

            switch (opcode) {
                case WAPI_IO_OP_NOP: {
                    if (resultPtr) writeU64(resultPtr, 0n);
                    pushIoCompletion(userData, WAPI_OK, 0);
                    return;
                }

                case WAPI_IO_OP_LOG: {
                    // flags2 carries log level in low bits.
                    const msg = readString(addr, len);
                    const level = (readU32(opPtr + 12) & 0x3);
                    const levelName = ['debug', 'info', 'warn', 'error'][level] || 'info';
                    logFn(levelName, msg);
                    // LOG has no completion event by design.
                    return;
                }

                case WAPI_IO_OP_TIMEOUT: {
                    const ms = Number(offset / 1000000n); // offset is ns
                    const token = state._ioNextToken++;
                    const timeoutId = setTimeout(() => {
                        if (!state._ioPending.has(token)) return;
                        state._ioPending.delete(token);
                        pushIoCompletion(userData, WAPI_OK, 0);
                    }, Math.max(0, ms));
                    state._ioPending.set(token, {
                        userData,
                        cancel: () => clearTimeout(timeoutId),
                    });
                    return;
                }

                case WAPI_IO_OP_HTTP_FETCH: {
                    // addr/len → request descriptor (URL string for now).
                    // Full wapi_http spec has a richer struct; v1 takes the
                    // bytes at addr as a UTF-8 URL.
                    const url = readString(addr, len);
                    const token = state._ioNextToken++;
                    const controller = new AbortController();
                    state._ioPending.set(token, {
                        userData,
                        cancel: () => controller.abort(),
                    });
                    fetch(url, { signal: controller.signal })
                        .then(async (resp) => {
                            const buf = new Uint8Array(await resp.arrayBuffer());
                            if (!state._ioPending.has(token)) return;
                            state._ioPending.delete(token);
                            // Write bytes to addr2 (up to len2), byte count to resultPtr.
                            const n = Math.min(buf.length, len2);
                            if (addr2 && n > 0) {
                                refreshViews();
                                state._u8.set(buf.subarray(0, n), addr2);
                            }
                            if (resultPtr) writeU64(resultPtr, BigInt(n));
                            pushIoCompletion(userData, WAPI_OK, 0);
                        })
                        .catch((e) => {
                            if (!state._ioPending.has(token)) return;
                            state._ioPending.delete(token);
                            logFn('warn', `http_fetch failed: ${e && e.message || e}`);
                            pushIoCompletion(userData, WAPI_ERR_IO, 0);
                        });
                    return;
                }

                case WAPI_IO_OP_COMPRESS_PROCESS: {
                    // flags2 low bits: 0=gzip, 1=deflate, 2=deflate-raw
                    // flags (high bit): encode vs decode
                    const flags2 = readU32(opPtr + 12);
                    const flags  = readU32(opPtr + 4);
                    const algoNames = ['gzip', 'deflate', 'deflate-raw'];
                    const algo = algoNames[flags2 & 0x3];
                    if (!algo) {
                        if (resultPtr) writeU64(resultPtr, 0n);
                        pushIoCompletion(userData, WAPI_ERR_NOTSUP, 0);
                        return;
                    }
                    const decode = (flags & 0x1) !== 0;
                    const input = state._u8.slice(addr, addr + len);
                    const token = state._ioNextToken++;
                    state._ioPending.set(token, { userData, cancel: () => {} });
                    (async () => {
                        try {
                            const stream = new Blob([input]).stream().pipeThrough(
                                decode ? new DecompressionStream(algo) : new CompressionStream(algo)
                            );
                            const buf = new Uint8Array(await new Response(stream).arrayBuffer());
                            if (!state._ioPending.has(token)) return;
                            state._ioPending.delete(token);
                            const n = Math.min(buf.length, len2);
                            if (addr2 && n > 0) {
                                refreshViews();
                                state._u8.set(buf.subarray(0, n), addr2);
                            }
                            if (resultPtr) writeU64(resultPtr, BigInt(n));
                            pushIoCompletion(userData, buf.length > len2 ? WAPI_ERR_OVERFLOW : WAPI_OK, 0);
                        } catch (e) {
                            if (!state._ioPending.has(token)) return;
                            state._ioPending.delete(token);
                            logFn('warn', `compress_process failed: ${e && e.message || e}`);
                            pushIoCompletion(userData, WAPI_ERR_IO, 0);
                        }
                    })();
                    return;
                }

                // WebSocket ops use `fd` as the WebSocket handle. Handle table
                // is a sparse JS array keyed by fd number.
                case WAPI_IO_OP_CONNECT: {
                    const url = readString(addr, len);
                    const ws = new WebSocket(url);
                    ws._recvQueue = [];
                    const handle = allocHandle(ws);
                    const token = state._ioNextToken++;
                    state._ioPending.set(token, { userData, cancel: () => ws.close() });
                    ws.onopen = () => {
                        if (!state._ioPending.has(token)) return;
                        state._ioPending.delete(token);
                        if (resultPtr) writeU64(resultPtr, BigInt(handle));
                        pushIoCompletion(userData, WAPI_OK, 0);
                    };
                    ws.onerror = () => {
                        if (!state._ioPending.has(token)) return;
                        state._ioPending.delete(token);
                        releaseHandle(handle);
                        pushIoCompletion(userData, WAPI_ERR_IO, 0);
                    };
                    ws.onmessage = async (ev) => {
                        const data = ev.data instanceof ArrayBuffer
                            ? new Uint8Array(ev.data)
                            : typeof ev.data === 'string'
                                ? _encoder.encode(ev.data)
                                : new Uint8Array(await ev.data.arrayBuffer());
                        ws._recvQueue.push(data);
                    };
                    return;
                }

                case WAPI_IO_OP_SEND: {
                    const ws = getHandle(fd);
                    if (!(ws instanceof WebSocket) || ws.readyState !== WebSocket.OPEN) {
                        pushIoCompletion(userData, WAPI_ERR_BADF, 0);
                        return;
                    }
                    ws.send(state._u8.slice(addr, addr + len));
                    if (resultPtr) writeU64(resultPtr, BigInt(len));
                    pushIoCompletion(userData, WAPI_OK, 0);
                    return;
                }

                case WAPI_IO_OP_RECV: {
                    const ws = getHandle(fd);
                    if (!(ws instanceof WebSocket)) {
                        pushIoCompletion(userData, WAPI_ERR_BADF, 0);
                        return;
                    }
                    if (ws._recvQueue.length === 0) {
                        pushIoCompletion(userData, WAPI_ERR_AGAIN, 0);
                        return;
                    }
                    const msg = ws._recvQueue.shift();
                    const n = Math.min(msg.length, len);
                    state._u8.set(msg.subarray(0, n), addr);
                    if (resultPtr) writeU64(resultPtr, BigInt(n));
                    pushIoCompletion(userData, WAPI_OK, 0);
                    return;
                }

                default:
                    if (resultPtr) writeU64(resultPtr, 0n);
                    pushIoCompletion(userData, WAPI_ERR_NOTSUP, 0);
                    return;
            }
        }

        // ---- Handle table (for WebSocket fds etc.) ------------------------

        const handles = new Map();
        let nextHandle = 1;

        function allocHandle(obj) {
            const h = nextHandle++;
            handles.set(h, obj);
            return h;
        }
        function getHandle(h) {
            return handles.get(h);
        }
        function releaseHandle(h) {
            handles.delete(h);
        }

        // ---- Capability list ----------------------------------------------

        const supportedCaps = [
            'wapi', 'wapi.env', 'wapi.memory', 'wapi.io',
            'wapi.clock', 'wapi.module',
        ];

        // ---- Import modules -----------------------------------------------

        const wapi = {
            capability_supported(svPtr) {
                const name = readStringView(svPtr);
                return name && supportedCaps.includes(name) ? 1 : 0;
            },
            capability_version(svPtr, versionPtr) {
                const name = readStringView(svPtr);
                if (!name || !supportedCaps.includes(name)) {
                    writeU32(versionPtr, 0);
                    writeU32(versionPtr + 4, 0);
                    return WAPI_ERR_NOTSUP;
                }
                state._dv.setUint16(versionPtr + 0, 1, true);
                state._dv.setUint16(versionPtr + 2, 0, true);
                state._dv.setUint16(versionPtr + 4, 0, true);
                state._dv.setUint16(versionPtr + 6, 0, true);
                return WAPI_OK;
            },
            capability_count() {
                return supportedCaps.length;
            },
            capability_name(index, bufPtr, bufLen, nameLenPtr) {
                if (index < 0 || index >= supportedCaps.length) return WAPI_ERR_OVERFLOW;
                refreshViews();
                const written = writeString(bufPtr, Number(bufLen), supportedCaps[index]);
                writeU64(nameLenPtr, BigInt(written));
                return WAPI_OK;
            },
            abi_version(versionPtr) {
                refreshViews();
                state._dv.setUint16(versionPtr + 0, 1, true);
                state._dv.setUint16(versionPtr + 2, 0, true);
                state._dv.setUint16(versionPtr + 4, 0, true);
                state._dv.setUint16(versionPtr + 6, 0, true);
                return WAPI_OK;
            },
            panic_report(msgPtr, msgLen) {
                const msg = readString(msgPtr, Number(msgLen));
                logFn('error', `PANIC: ${msg}`);
            },

            // io_get / allocator_get return vtable pointers. v1 builds them
            // lazily on first call so we don't allocate tables unless a
            // module actually uses the indirect dispatch path.
            io_get() {
                if (state._ioVtablePtr) return state._ioVtablePtr;
                const table = state.instance && state.instance.exports.__indirect_function_table;
                if (!table) {
                    logFn('error', 'module missing __indirect_function_table; vtable unavailable');
                    return 0;
                }
                const base = table.length;
                table.grow(8);
                table.set(base + 0, makeWasmFunc(['i32', 'i32', 'i64'], ['i32'],
                    (_impl, opsPtr, count) => wapi_io.submit(opsPtr, Number(count))));
                table.set(base + 1, makeWasmFunc(['i32', 'i64'], ['i32'],
                    (_impl, ud) => wapi_io.cancel(ud)));
                table.set(base + 2, makeWasmFunc(['i32', 'i32'], ['i32'],
                    (_impl, ep) => wapi_io.poll(ep)));
                table.set(base + 3, makeWasmFunc(['i32', 'i32', 'i32'], ['i32'],
                    (_impl, ep, t) => wapi_io.wait(ep, t)));
                table.set(base + 4, makeWasmFunc(['i32', 'i32'], [],
                    (_impl, et) => wapi_io.flush(et)));
                table.set(base + 5, makeWasmFunc(['i32', 'i32'], ['i32'],
                    (_impl, svPtr) => wapi.capability_supported(svPtr)));
                table.set(base + 6, makeWasmFunc(['i32', 'i32', 'i32'], ['i32'],
                    (_impl, svPtr, vp) => wapi.capability_version(svPtr, vp)));
                table.set(base + 7, makeWasmFunc(['i32', 'i32', 'i32'], ['i32'],
                    (_impl, _sv, sp) => { writeU32(sp, 0); return WAPI_ERR_NOTSUP; }));

                const ptr = hostAlloc(36, 4);
                if (!ptr) return 0;
                refreshViews();
                state._dv.setUint32(ptr + 0,  0,        true);
                state._dv.setUint32(ptr + 4,  base + 0, true);
                state._dv.setUint32(ptr + 8,  base + 1, true);
                state._dv.setUint32(ptr + 12, base + 2, true);
                state._dv.setUint32(ptr + 16, base + 3, true);
                state._dv.setUint32(ptr + 20, base + 4, true);
                state._dv.setUint32(ptr + 24, base + 5, true);
                state._dv.setUint32(ptr + 28, base + 6, true);
                state._dv.setUint32(ptr + 32, base + 7, true);
                state._ioVtablePtr = ptr;
                return ptr;
            },

            allocator_get() {
                if (state._allocVtablePtr) return state._allocVtablePtr;
                const table = state.instance && state.instance.exports.__indirect_function_table;
                if (!table) return 0;
                const base = table.length;
                table.grow(3);
                table.set(base + 0, makeWasmFunc(['i32', 'i64', 'i64'], ['i32'],
                    (_impl, size, align) => hostAlloc(Number(size), Number(align))));
                table.set(base + 1, makeWasmFunc(['i32', 'i32'], [],
                    (_impl, ptr) => hostFree(ptr)));
                table.set(base + 2, makeWasmFunc(['i32', 'i32', 'i64', 'i64'], ['i32'],
                    (_impl, ptr, newSize, align) => {
                        const np = hostAlloc(Number(newSize), Number(align));
                        if (np && ptr) {
                            refreshViews();
                            state._u8.copyWithin(np, ptr, ptr + Number(newSize));
                        }
                        return np;
                    }));

                const ptr = hostAlloc(16, 4);
                if (!ptr) return 0;
                refreshViews();
                state._dv.setUint32(ptr + 0,  0,        true);
                state._dv.setUint32(ptr + 4,  base + 0, true);
                state._dv.setUint32(ptr + 8,  base + 1, true);
                state._dv.setUint32(ptr + 12, base + 2, true);
                state._allocVtablePtr = ptr;
                return ptr;
            },
        };

        const wapi_env = {
            args_count() { return 0; },
            args_get() { return WAPI_ERR_NOENT; },
            environ_count() { return 0; },
            environ_get() { return WAPI_ERR_NOENT; },
            random(bufPtr, bufLen) {
                const n = Number(bufLen);
                refreshViews();
                crypto.getRandomValues(state._u8.subarray(bufPtr, bufPtr + n));
                return WAPI_OK;
            },
            exit(code) {
                state._exitCode = code | 0;
                if (opts.onExit) opts.onExit(state._exitCode);
                // Cannot actually stop wasm execution from imports; the
                // caller (sw.js services handler) watches exitCode() and
                // tears the instance down.
            },
        };

        const wapi_memory = {
            alloc(size, align) { return hostAlloc(Number(size), Number(align)); },
            free(ptr) { hostFree(ptr); },
            realloc(ptr, newSize, align) {
                const np = hostAlloc(Number(newSize), Number(align));
                if (np && ptr) {
                    refreshViews();
                    state._u8.copyWithin(np, ptr, ptr + Number(newSize));
                }
                return np;
            },
        };

        const wapi_io = {
            submit(opsPtr, count) {
                for (let i = 0; i < count; i++) {
                    dispatchOp(opsPtr + i * IO_OP_SIZE);
                }
                return count;
            },
            cancel(userData) {
                for (const [tok, p] of state._ioPending) {
                    if (p.userData === BigInt(userData)) {
                        if (p.cancel) try { p.cancel(); } catch {}
                        state._ioPending.delete(tok);
                        return WAPI_OK;
                    }
                }
                return WAPI_ERR_NOENT;
            },
            poll(eventPtr) {
                if (state._eventQueue.length === 0) return 0;
                refreshViews();
                const ev = state._eventQueue.shift();
                writeEvent(eventPtr, ev);
                return 1;
            },
            wait(eventPtr, _timeoutMs) {
                // Cannot actually block the SW event loop. Caller is expected
                // to yield and retry; this just peeks.
                return wapi_io.poll(eventPtr);
            },
            flush(eventType) {
                if (!eventType) { state._eventQueue.length = 0; return; }
                state._eventQueue = state._eventQueue.filter((e) => e.type !== eventType);
            },
        };

        const wapi_clock = {
            time_get(clockId, timePtr) {
                const ns = clockId === WAPI_CLOCK_MONOTONIC
                    ? BigInt(Math.round(performance.now() * 1e6))
                    : BigInt(Date.now()) * 1000000n;
                writeU64(timePtr, ns);
                return WAPI_OK;
            },
            resolution(clockId, resPtr) {
                const ns = clockId === WAPI_CLOCK_MONOTONIC ? 1000n : 1000000n;
                writeU64(resPtr, ns);
                return WAPI_OK;
            },
            perf_counter() {
                return BigInt(Math.round((performance.now() - state._perfOrigin) * 1e6));
            },
            perf_frequency() { return 1000000000n; },
            yield() { /* no-op */ },
            sleep(_ns) { /* cannot block SW event loop */ },
        };

        // wapi_module for nested loads inside a service. The lookup path
        // delegates to opts.loadModuleBytes so the services manager in sw.js
        // can wire it to the IDB bytes cache or route joins back through
        // the refcount map.
        const wapi_module = {
            load(hashPtr, urlSvPtr, outHandlePtr) {
                // Hash is 32 bytes at hashPtr. Look up bytes synchronously is
                // impossible (IDB is async); the caller should prefetch first
                // and this function then reads from an in-memory cache.
                writeU32(outHandlePtr, 0);
                return WAPI_ERR_NOTSUP;
            },
            prefetch(hashPtr, urlSvPtr) {
                // Async kick-off; parked for v1.1 until the service-side
                // nested module cache exists.
                return WAPI_ERR_NOTSUP;
            },
            is_cached(hashPtr) { return 0; },
            release(handle) { return WAPI_ERR_NOTSUP; },
            get_hash(handle, outHashPtr) { return WAPI_ERR_NOTSUP; },
            join(hashPtr, urlSvPtr, nameSvPtr, outHandlePtr) {
                // Same deferral as load.
                writeU32(outHandlePtr, 0);
                return WAPI_ERR_NOTSUP;
            },
        };

        // ---- Final imports object -----------------------------------------

        const imports = {
            wapi,
            wapi_env,
            wapi_memory,
            wapi_io,
            wapi_clock,
            wapi_module,
        };

        // ---- Lifecycle ----------------------------------------------------

        function bindInstance(instance) {
            state.instance = instance;
            state.memory = instance.exports.memory;
            if (!state.memory) {
                throw new Error('wapi sw_host: instance has no memory export');
            }
            refreshViews();

            // Seed allocator base. Prefer __heap_base if the module exports
            // it; otherwise start at 1 MB to leave room for static data.
            const heapBase = instance.exports.__heap_base;
            state._allocBase = heapBase ? (heapBase.value | 0) : 0x100000;
            state._allocPtr = state._allocBase;
        }

        function tick() {
            // Placeholder — most op completions are driven by their own
            // promises / timers. This hook exists for the services manager
            // to explicitly pump state in the future (e.g., websocket
            // heartbeat, stale pending op GC).
        }

        return {
            imports,
            state,
            bindInstance,
            tick,
            pendingCount: () => state._ioPending.size,
            exitCode: () => state._exitCode,
        };
    }

    self.createSwHost = createSwHost;
})();
