/**
 * WAPI - Browser Shim
 * Maps WAPI ABI imports to browser Web APIs.
 *
 * Usage:
 *   const wapi = new ThinPlatform();
 *   await wapi.load("module.wasm", { args: ["--flag"], preopens: { "/data": filesMap } });
 *
 * This file implements every import module defined by the WAPI ABI headers:
 *   wapi, wapi_env, wapi_memory, wapi_io, wapi_clock, wapi_fs, wapi_net,
 *   wapi_gpu, wapi_surface, wapi_input, wapi_audio, wapi_content, wapi_clipboard,
 *   wapi_kv, wapi_font, wapi_crypto, wapi_video, wapi_module,
 *   wapi_notify, wapi_geo, wapi_sensor, wapi_speech, wapi_bio,
 *   wapi_share, wapi_pay, wapi_usb, wapi_midi, wapi_bt, wapi_camera, wapi_xr
 */

// ---------------------------------------------------------------------------
// Constants mirrored from the C headers
// ---------------------------------------------------------------------------

const WAPI_OK                =  0;
const WAPI_ERR_UNKNOWN       = -1;
const WAPI_ERR_INVAL         = -2;
const WAPI_ERR_BADF          = -3;
const WAPI_ERR_ACCES         = -4;
const WAPI_ERR_NOENT         = -5;
const WAPI_ERR_EXIST         = -6;
const WAPI_ERR_NOTDIR        = -7;
const WAPI_ERR_ISDIR         = -8;
const WAPI_ERR_NOSPC         = -9;
const WAPI_ERR_NOMEM         = -10;
const WAPI_ERR_NAMETOOLONG   = -11;
const WAPI_ERR_NOTEMPTY      = -12;
const WAPI_ERR_IO            = -13;
const WAPI_ERR_AGAIN         = -14;
const WAPI_ERR_BUSY          = -15;
const WAPI_ERR_TIMEDOUT      = -16;
const WAPI_ERR_CONNREFUSED   = -17;
const WAPI_ERR_CONNRESET     = -18;
const WAPI_ERR_CONNABORTED   = -19;
const WAPI_ERR_NETUNREACH    = -20;
const WAPI_ERR_HOSTUNREACH   = -21;
const WAPI_ERR_ADDRINUSE     = -22;
const WAPI_ERR_PIPE          = -23;
const WAPI_ERR_NOTCAPABLE    = -24;
const WAPI_ERR_NOTSUP        = -25;
const WAPI_ERR_OVERFLOW       = -26;
const WAPI_ERR_CANCELED      = -27;
const WAPI_ERR_NOSYS         = -32;

const WAPI_HANDLE_INVALID = 0;
const WAPI_STDIN  = 1;
const WAPI_STDOUT = 2;
const WAPI_STDERR = 3;
const WAPI_STRLEN = 0xFFFFFFFF;

// (Capability constants removed -- now using string-based capability names)

// Clock IDs
const WAPI_CLOCK_MONOTONIC = 0;
const WAPI_CLOCK_REALTIME  = 1;

// File types
const WAPI_FILETYPE_UNKNOWN   = 0;
const WAPI_FILETYPE_DIRECTORY = 3;
const WAPI_FILETYPE_REGULAR   = 4;

// Open flags
const WAPI_FS_OFLAG_CREATE    = 0x0001;
const WAPI_FS_OFLAG_DIRECTORY = 0x0002;
const WAPI_FS_OFLAG_EXCL      = 0x0004;
const WAPI_FS_OFLAG_TRUNC     = 0x0008;

// Seek
const WAPI_WHENCE_SET = 0;
const WAPI_WHENCE_CUR = 1;
const WAPI_WHENCE_END = 2;

// I/O opcodes
const WAPI_IO_OP_NOP         = 0;
const WAPI_IO_OP_READ        = 1;
const WAPI_IO_OP_WRITE       = 2;
const WAPI_IO_OP_OPEN        = 3;
const WAPI_IO_OP_CLOSE       = 4;
const WAPI_IO_OP_STAT        = 5;
const WAPI_IO_OP_LOG         = 6;
const WAPI_IO_OP_CONNECT     = 10;
const WAPI_IO_OP_ACCEPT      = 11;
const WAPI_IO_OP_SEND        = 12;
const WAPI_IO_OP_RECV        = 13;
const WAPI_IO_OP_TIMEOUT     = 20;
const WAPI_IO_OP_TIMEOUT_ABS = 21;

// Log levels (used with WAPI_IO_OP_LOG)
const WAPI_LOG_DEBUG = 0;
const WAPI_LOG_INFO  = 1;
const WAPI_LOG_WARN  = 2;
const WAPI_LOG_ERROR = 3;

// Net transport
const WAPI_NET_TRANSPORT_QUIC      = 0;
const WAPI_NET_TRANSPORT_TCP       = 1;
const WAPI_NET_TRANSPORT_WEBSOCKET = 2;

// Event types
const WAPI_EVENT_NONE                  = 0;
const WAPI_EVENT_QUIT                  = 0x100;
const WAPI_EVENT_SURFACE_RESIZED       = 0x0200;
const WAPI_EVENT_SURFACE_CLOSE         = 0x0201;
const WAPI_EVENT_SURFACE_FOCUS_GAINED  = 0x0202;
const WAPI_EVENT_SURFACE_FOCUS_LOST    = 0x0203;
const WAPI_EVENT_KEY_DOWN              = 0x300;
const WAPI_EVENT_KEY_UP                = 0x301;
const WAPI_EVENT_TEXT_INPUT            = 0x302;
const WAPI_EVENT_MOUSE_MOTION          = 0x400;
const WAPI_EVENT_MOUSE_BUTTON_DOWN     = 0x401;
const WAPI_EVENT_MOUSE_BUTTON_UP       = 0x402;
const WAPI_EVENT_MOUSE_WHEEL           = 0x403;
const WAPI_EVENT_TOUCH_DOWN            = 0x700;
const WAPI_EVENT_TOUCH_UP              = 0x701;
const WAPI_EVENT_TOUCH_MOTION          = 0x702;
const WAPI_EVENT_GAMEPAD_AXIS          = 0x652;
const WAPI_EVENT_GAMEPAD_BUTTON_DOWN   = 0x653;
const WAPI_EVENT_GAMEPAD_BUTTON_UP     = 0x654;

// Cursor types
const WAPI_CURSOR_NAMES = [
    "default", "pointer", "text", "crosshair", "move",
    "ns-resize", "ew-resize", "nwse-resize", "nesw-resize",
    "not-allowed", "wait", "grab", "grabbing", "none"
];

// Audio formats
const WAPI_AUDIO_U8  = 0x0008;
const WAPI_AUDIO_S16 = 0x8010;
const WAPI_AUDIO_S32 = 0x8020;
const WAPI_AUDIO_F32 = 0x8120;

const WAPI_AUDIO_DEFAULT_PLAYBACK  = -1;
const WAPI_AUDIO_DEFAULT_RECORDING = -2;

// GPU texture formats
const WAPI_GPU_FORMAT_BGRA8_UNORM      = 0x0057;
const WAPI_GPU_FORMAT_RGBA8_UNORM      = 0x0012;
const WAPI_GPU_FORMAT_BGRA8_UNORM_SRGB = 0x0058;
const WAPI_GPU_FORMAT_RGBA8_UNORM_SRGB = 0x0013;

// Clipboard formats
const WAPI_CLIPBOARD_TEXT  = 0;
const WAPI_CLIPBOARD_HTML  = 1;
const WAPI_CLIPBOARD_IMAGE = 2;

// Surface flags
const WAPI_SURFACE_FLAG_RESIZABLE  = 0x0001;
const WAPI_SURFACE_FLAG_HIGH_DPI   = 0x0010;

// ---------------------------------------------------------------------------
// HandleTable - maps integer handles to JS objects
// ---------------------------------------------------------------------------

class HandleTable {
    constructor() {
        this._nextId = 4; // 1-3 reserved for stdin/stdout/stderr
        this._map = new Map();
    }

    insert(obj) {
        const id = this._nextId++;
        this._map.set(id, obj);
        return id;
    }

    get(id) {
        return this._map.get(id) || null;
    }

    remove(id) {
        const obj = this._map.get(id);
        this._map.delete(id);
        return obj || null;
    }

    has(id) {
        return this._map.has(id);
    }
}

// ---------------------------------------------------------------------------
// MemFS - in-memory filesystem
// ---------------------------------------------------------------------------

class MemFSNode {
    constructor(type, name) {
        this.type = type;         // WAPI_FILETYPE_DIRECTORY | WAPI_FILETYPE_REGULAR
        this.name = name;
        this.children = null;     // Map<string, MemFSNode> for directories
        this.data = null;         // Uint8Array for files
        this.ctime = Date.now();
        this.mtime = Date.now();
        this.ino = MemFSNode._inoCounter++;
        if (type === WAPI_FILETYPE_DIRECTORY) {
            this.children = new Map();
        } else {
            this.data = new Uint8Array(0);
        }
    }
}
MemFSNode._inoCounter = 1;

class MemFS {
    constructor() {
        this.root = new MemFSNode(WAPI_FILETYPE_DIRECTORY, "/");
    }

    _resolve(path) {
        if (!path || path === "/") return this.root;
        const parts = path.split("/").filter(Boolean);
        let node = this.root;
        for (const p of parts) {
            if (!node.children) return null;
            node = node.children.get(p);
            if (!node) return null;
        }
        return node;
    }

    _resolveParent(path) {
        const parts = path.split("/").filter(Boolean);
        if (parts.length === 0) return { parent: null, name: "" };
        const name = parts.pop();
        let node = this.root;
        for (const p of parts) {
            if (!node.children) return { parent: null, name };
            node = node.children.get(p);
            if (!node) return { parent: null, name };
        }
        return { parent: node, name };
    }

    _normPath(basePath, relPath) {
        if (relPath.startsWith("/")) return relPath;
        const base = basePath.endsWith("/") ? basePath : basePath + "/";
        const combined = base + relPath;
        // Normalize ../ and ./
        const parts = combined.split("/").filter(Boolean);
        const stack = [];
        for (const p of parts) {
            if (p === ".") continue;
            if (p === "..") { stack.pop(); continue; }
            stack.push(p);
        }
        return "/" + stack.join("/");
    }

    mkdirp(path) {
        const parts = path.split("/").filter(Boolean);
        let node = this.root;
        for (const p of parts) {
            if (!node.children.has(p)) {
                node.children.set(p, new MemFSNode(WAPI_FILETYPE_DIRECTORY, p));
            }
            node = node.children.get(p);
            if (node.type !== WAPI_FILETYPE_DIRECTORY) return null;
        }
        return node;
    }

    createFile(path, data) {
        const { parent, name } = this._resolveParent(path);
        if (!parent || parent.type !== WAPI_FILETYPE_DIRECTORY) return null;
        const f = new MemFSNode(WAPI_FILETYPE_REGULAR, name);
        if (data) f.data = new Uint8Array(data);
        parent.children.set(name, f);
        return f;
    }

    stat(path) {
        return this._resolve(path);
    }
}

// ---------------------------------------------------------------------------
// Open file descriptor state for MemFS
// ---------------------------------------------------------------------------

class FDEntry {
    constructor(node, path, flags) {
        this.node = node;
        this.path = path;
        this.flags = flags;
        this.position = 0;
    }
}

// ---------------------------------------------------------------------------
// DOM key code -> WAPI scancode mapping (USB HID subset)
// ---------------------------------------------------------------------------

const KEY_TO_SCANCODE = {
    "KeyA": 4, "KeyB": 5, "KeyC": 6, "KeyD": 7, "KeyE": 8, "KeyF": 9,
    "KeyG": 10, "KeyH": 11, "KeyI": 12, "KeyJ": 13, "KeyK": 14, "KeyL": 15,
    "KeyM": 16, "KeyN": 17, "KeyO": 18, "KeyP": 19, "KeyQ": 20, "KeyR": 21,
    "KeyS": 22, "KeyT": 23, "KeyU": 24, "KeyV": 25, "KeyW": 26, "KeyX": 27,
    "KeyY": 28, "KeyZ": 29,
    "Digit1": 30, "Digit2": 31, "Digit3": 32, "Digit4": 33, "Digit5": 34,
    "Digit6": 35, "Digit7": 36, "Digit8": 37, "Digit9": 38, "Digit0": 39,
    "Enter": 40, "Escape": 41, "Backspace": 42, "Tab": 43, "Space": 44,
    "ControlLeft": 224, "ShiftLeft": 225, "AltLeft": 226, "MetaLeft": 227,
    "ControlRight": 228, "ShiftRight": 229, "AltRight": 230, "MetaRight": 231,
    "F1": 58, "F2": 59, "F3": 60, "F4": 61, "F5": 62, "F6": 63,
    "F7": 64, "F8": 65, "F9": 66, "F10": 67, "F11": 68, "F12": 69,
    "Insert": 73, "Home": 74, "PageUp": 75, "Delete": 76, "End": 77,
    "PageDown": 78, "ArrowRight": 79, "ArrowLeft": 80, "ArrowDown": 81, "ArrowUp": 82,
};

function domModToTP(e) {
    let m = 0;
    if (e.shiftKey) m |= 0x0001;
    if (e.ctrlKey)  m |= 0x0040;
    if (e.altKey)   m |= 0x0100;
    if (e.metaKey)  m |= 0x0400;
    return m;
}

function domButtonToTP(b) {
    // DOM: 0=left,1=middle,2=right,3=back,4=forward
    return [1, 2, 3, 4, 5][b] || 1;
}

// ---------------------------------------------------------------------------
// GPU format helpers
// ---------------------------------------------------------------------------

function tpFormatToGPU(fmt) {
    switch (fmt) {
        case WAPI_GPU_FORMAT_BGRA8_UNORM:      return "bgra8unorm";
        case WAPI_GPU_FORMAT_RGBA8_UNORM:       return "rgba8unorm";
        case WAPI_GPU_FORMAT_BGRA8_UNORM_SRGB:  return "bgra8unorm-srgb";
        case WAPI_GPU_FORMAT_RGBA8_UNORM_SRGB:   return "rgba8unorm-srgb";
        default: return "bgra8unorm";
    }
}

function gpuFormatToTP(fmt) {
    switch (fmt) {
        case "bgra8unorm":      return WAPI_GPU_FORMAT_BGRA8_UNORM;
        case "rgba8unorm":      return WAPI_GPU_FORMAT_RGBA8_UNORM;
        case "bgra8unorm-srgb": return WAPI_GPU_FORMAT_BGRA8_UNORM_SRGB;
        case "rgba8unorm-srgb": return WAPI_GPU_FORMAT_RGBA8_UNORM_SRGB;
        default: return WAPI_GPU_FORMAT_BGRA8_UNORM;
    }
}

// ---------------------------------------------------------------------------
// ThinPlatform - the main shim class
// ---------------------------------------------------------------------------

class ThinPlatform {
    constructor() {
        this.instance = null;     // WebAssembly.Instance
        this.module = null;       // WebAssembly.Module
        this.memory = null;       // WebAssembly.Memory (from module exports)
        this.handles = new HandleTable();
        this.memfs = new MemFS();

        // Memory views - refreshed after every grow
        this._u8 = null;
        this._i32 = null;
        this._u32 = null;
        this._f32 = null;
        this._dv = null;    // DataView for unaligned/64-bit access

        // Host allocator state (bump allocator on wasm linear memory)
        this._allocBase = 0;
        this._allocPtr = 0;
        this._freeList = [];    // simple free list: [{ptr, size}]

        // Config
        this._args = ["wapi_module"];
        this._env = {};
        this._preopens = [];    // [{path, handle}]
        this._lastError = "";

        // I/O queues
        this._ioQueues = new Map(); // handle -> {completions:[], pending:[]}

        // Input event queue
        this._eventQueue = [];
        this._keyState = new Set();  // set of currently-pressed scancodes
        this._modState = 0;
        this._mouseX = 0;
        this._mouseY = 0;
        this._mouseButtons = 0;

        // Surfaces
        this._surfaces = new Map(); // handle -> {canvas, ctx, resizeObserver, ...}
        this._activeSurfaceHandle = WAPI_HANDLE_INVALID;

        // GPU state
        this._gpuAdapter = null;
        this._gpuDevice = null;
        this._gpuQueue = null;
        this._gpuSurfaceConfigs = new Map(); // surfaceHandle -> {context, config}

        // Audio state
        this._audioCtx = null;

        // Content state - Canvas2D for text/image rendering
        this._contentCanvas = null;
        this._contentCtx = null;

        // Clipboard cache (async clipboard API needs user gesture)
        this._clipboardText = "";
        this._clipboardHtml = "";

        // Frame loop
        this._running = false;
        this._frameHandle = 0;

        // Performance counter origin
        this._perfOrigin = performance.now();
    }

    // -----------------------------------------------------------------------
    // Memory view helpers - must be called after any memory.grow
    // -----------------------------------------------------------------------

    _refreshViews() {
        const buf = this.memory.buffer;
        this._u8  = new Uint8Array(buf);
        this._i32 = new Int32Array(buf);
        this._u32 = new Uint32Array(buf);
        this._f32 = new Float32Array(buf);
        this._dv  = new DataView(buf);
    }

    _readString(ptr, len) {
        this._refreshViews();
        if (len === WAPI_STRLEN) {
            // Null-terminated
            let end = ptr;
            while (this._u8[end] !== 0) end++;
            len = end - ptr;
        }
        return new TextDecoder().decode(this._u8.subarray(ptr, ptr + len));
    }

    _writeString(ptr, maxLen, str) {
        const encoded = new TextEncoder().encode(str);
        const writeLen = Math.min(encoded.length, maxLen);
        this._refreshViews();
        this._u8.set(encoded.subarray(0, writeLen), ptr);
        return encoded.length;
    }

    _writeU32(ptr, val) {
        this._refreshViews();
        this._dv.setUint32(ptr, val, true);
    }

    _readU32(ptr) {
        this._refreshViews();
        return this._dv.getUint32(ptr, true);
    }

    _writeI32(ptr, val) {
        this._refreshViews();
        this._dv.setInt32(ptr, val, true);
    }

    _readI32(ptr) {
        this._refreshViews();
        return this._dv.getInt32(ptr, true);
    }

    _writeU64(ptr, val) {
        this._refreshViews();
        this._dv.setBigUint64(ptr, BigInt(val), true);
    }

    _readU64(ptr) {
        this._refreshViews();
        return this._dv.getBigUint64(ptr, true);
    }

    _writeF32(ptr, val) {
        this._refreshViews();
        this._dv.setFloat32(ptr, val, true);
    }

    _readF32(ptr) {
        this._refreshViews();
        return this._dv.getFloat32(ptr, true);
    }

    // -----------------------------------------------------------------------
    // Host bump allocator (operates on wasm linear memory)
    // -----------------------------------------------------------------------

    _allocInit() {
        // We place the allocator after the module's own data.
        // Use __heap_base if exported, else start at 1MB.
        const exports = this.instance.exports;
        if (exports.__heap_base) {
            this._allocBase = exports.__heap_base.value;
        } else {
            this._allocBase = 1024 * 1024;
        }
        this._allocPtr = this._allocBase;
    }

    _hostAlloc(size, align) {
        if (size === 0) return 0;
        if (align < 1) align = 1;

        // Try free list first
        for (let i = 0; i < this._freeList.length; i++) {
            const block = this._freeList[i];
            const alignedPtr = (block.ptr + align - 1) & ~(align - 1);
            if (alignedPtr + size <= block.ptr + block.size) {
                this._freeList.splice(i, 1);
                return alignedPtr;
            }
        }

        // Bump allocate
        let ptr = (this._allocPtr + align - 1) & ~(align - 1);
        const end = ptr + size;

        // Grow memory if needed
        const pageSize = 65536;
        const currentSize = this.memory.buffer.byteLength;
        if (end > currentSize) {
            const needed = Math.ceil((end - currentSize) / pageSize);
            this.memory.grow(needed);
            this._refreshViews();
        }

        this._allocPtr = end;
        return ptr;
    }

    _hostFree(ptr) {
        // We cannot easily reclaim bump-allocated memory.
        // Store in free list for potential reuse.
        if (ptr === 0) return;
        // We don't track sizes in the bump allocator, so this is a no-op
        // unless the module uses exported malloc/free.
    }

    // If the module exports malloc/free, delegate to them.
    _moduleAlloc(size, align) {
        const exports = this.instance.exports;
        if (exports.malloc) {
            return exports.malloc(size);
        }
        return this._hostAlloc(size, align);
    }

    _moduleFree(ptr) {
        const exports = this.instance.exports;
        if (exports.free) {
            exports.free(ptr);
            return;
        }
        this._hostFree(ptr);
    }

    // -----------------------------------------------------------------------
    // Capability detection
    // -----------------------------------------------------------------------

    _detectCapabilities() {
        const caps = [
            "wapi.memory", "wapi.clock", "wapi.random",
            "wapi.io", "wapi.env", "wapi.filesystem",
        ];

        if (typeof navigator !== "undefined" && navigator.gpu) {
            caps.push("wapi.gpu");
        }
        // Surface and input always available in browser (we create a canvas)
        caps.push("wapi.surface", "wapi.input");

        if (typeof AudioContext !== "undefined" || typeof webkitAudioContext !== "undefined") {
            caps.push("wapi.audio");
        }
        // Content via Canvas2D
        caps.push("wapi.content");

        if (typeof navigator !== "undefined" && navigator.clipboard) {
            caps.push("wapi.clipboard");
        }
        if (typeof WebSocket !== "undefined" || typeof fetch !== "undefined") {
            caps.push("wapi.net");
        }

        // KV storage and font queries always available in browser
        caps.push("wapi.kv_storage");
        caps.push("wapi.font");

        // Crypto always available
        if (typeof crypto !== "undefined" && crypto.subtle) {
            caps.push("wapi.crypto");
        }
        // Geolocation
        if (typeof navigator !== "undefined" && navigator.geolocation) {
            caps.push("wapi.geolocation");
        }
        // Notifications
        if (typeof Notification !== "undefined") {
            caps.push("wapi.notifications");
        }
        // Speech
        if (typeof speechSynthesis !== "undefined") {
            caps.push("wapi.speech");
        }
        // Share
        if (typeof navigator !== "undefined" && navigator.share) {
            caps.push("wapi.share");
        }
        // MIDI
        if (typeof navigator !== "undefined" && navigator.requestMIDIAccess) {
            caps.push("wapi.midi");
        }

        // Permissions
        caps.push("wapi.permissions");

        // Network info
        if (typeof navigator !== "undefined" && navigator.onLine !== undefined) {
            caps.push("wapi.network_info");
        }

        // Battery
        if (typeof navigator !== "undefined" && navigator.getBattery) {
            caps.push("wapi.battery");
        }

        // Haptics / vibration
        if (typeof navigator !== "undefined" && navigator.vibrate) {
            caps.push("wapi.haptics");
        }

        // Screen orientation
        if (typeof screen !== "undefined" && screen.orientation) {
            caps.push("wapi.orientation");
        }

        // Wake lock
        if (typeof navigator !== "undefined" && navigator.wakeLock) {
            caps.push("wapi.wake_lock");
        }

        // WebAuthn
        if (typeof navigator !== "undefined" && navigator.credentials) {
            caps.push("wapi.authn");
        }

        // HID
        if (typeof navigator !== "undefined" && navigator.hid) {
            caps.push("wapi.hid");
        }

        // Serial
        if (typeof navigator !== "undefined" && navigator.serial) {
            caps.push("wapi.serial");
        }

        // NFC
        if (typeof NDEFReader !== "undefined") {
            caps.push("wapi.nfc");
        }

        // Barcode Detection
        if (typeof BarcodeDetector !== "undefined") {
            caps.push("wapi.barcode");
        }

        // Contacts
        if (typeof navigator !== "undefined" && navigator.contacts) {
            caps.push("wapi.contacts");
        }

        // Idle detection
        if (typeof IdleDetector !== "undefined") {
            caps.push("wapi.idle");
        }

        // Screen capture (getDisplayMedia)
        if (typeof navigator !== "undefined" && navigator.mediaDevices &&
            navigator.mediaDevices.getDisplayMedia) {
            caps.push("wapi.screen_capture");
        }

        // P2P (WebRTC)
        if (typeof RTCPeerConnection !== "undefined") {
            caps.push("wapi.p2p");
        }

        // Media session
        if (typeof navigator !== "undefined" && navigator.mediaSession) {
            caps.push("wapi.media_session");
        }

        // DnD always available in browsers
        caps.push("wapi.dnd");

        return caps;
    }

    // -----------------------------------------------------------------------
    // Build the Wasm import object
    // -----------------------------------------------------------------------

    _buildImports() {
        const self = this;

        // -------------------------------------------------------------------
        // wapi (capability)
        // -------------------------------------------------------------------
        const supportedCaps = self._detectCapabilities();

        const wapi = {
            capability_supported(namePtr, nameLen) {
                self._refreshViews();
                const name = self._readString(namePtr, nameLen);
                return supportedCaps.includes(name) ? 1 : 0;
            },

            capability_version(namePtr, nameLen, versionPtr) {
                self._refreshViews();
                const name = self._readString(namePtr, nameLen);
                if (!supportedCaps.includes(name)) {
                    self._writeU32(versionPtr, 0);
                    self._writeU32(versionPtr + 4, 0);
                    return WAPI_ERR_NOTSUP;
                }
                // All modules are v1.0.0
                self._dv.setUint16(versionPtr + 0, 1, true);
                self._dv.setUint16(versionPtr + 2, 0, true);
                self._dv.setUint16(versionPtr + 4, 0, true);
                self._dv.setUint16(versionPtr + 6, 0, true);
                return WAPI_OK;
            },

            capability_count() {
                return supportedCaps.length;
            },

            capability_name(index, bufPtr, bufLen, nameLenPtr) {
                if (index < 0 || index >= supportedCaps.length) return WAPI_ERR_OVERFLOW;
                self._refreshViews();
                const name = supportedCaps[index];
                const written = self._writeString(bufPtr, bufLen, name);
                self._writeU32(nameLenPtr, written);
                return WAPI_OK;
            },

            abi_version(versionPtr) {
                self._refreshViews();
                self._dv.setUint16(versionPtr + 0, 1, true);  // major
                self._dv.setUint16(versionPtr + 2, 0, true);  // minor
                self._dv.setUint16(versionPtr + 4, 0, true);  // patch
                self._dv.setUint16(versionPtr + 6, 0, true);  // reserved
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_env (environment)
        // -------------------------------------------------------------------
        const wapi_env = {
            args_count() {
                return self._args.length;
            },

            args_get(index, bufPtr, bufLen, argLenPtr) {
                if (index < 0 || index >= self._args.length) return WAPI_ERR_OVERFLOW;
                const arg = self._args[index];
                const written = self._writeString(bufPtr, bufLen, arg);
                self._writeU32(argLenPtr, written);
                return WAPI_OK;
            },

            environ_count() {
                return Object.keys(self._env).length;
            },

            environ_get(index, bufPtr, bufLen, varLenPtr) {
                const keys = Object.keys(self._env);
                if (index < 0 || index >= keys.length) return WAPI_ERR_OVERFLOW;
                const entry = keys[index] + "=" + self._env[keys[index]];
                const written = self._writeString(bufPtr, bufLen, entry);
                self._writeU32(varLenPtr, written);
                return WAPI_OK;
            },

            getenv(namePtr, nameLen, bufPtr, bufLen, valLenPtr) {
                const name = self._readString(namePtr, nameLen);
                if (!(name in self._env)) return WAPI_ERR_NOENT;
                const val = self._env[name];
                const written = self._writeString(bufPtr, bufLen, val);
                self._writeU32(valLenPtr, written);
                return WAPI_OK;
            },

            random_get(bufPtr, len) {
                self._refreshViews();
                const sub = self._u8.subarray(bufPtr, bufPtr + len);
                crypto.getRandomValues(sub);
                return WAPI_OK;
            },

            exit(code) {
                self._running = false;
                if (self._frameHandle) {
                    cancelAnimationFrame(self._frameHandle);
                    self._frameHandle = 0;
                }
                console.log(`[WAPI] Module exited with code ${code}`);
                // In a browser we cannot truly exit; throw to unwind.
                throw new Error(`wapi_exit(${code})`);
            },

            get_error(bufPtr, bufLen, msgLenPtr) {
                const msg = self._lastError;
                const written = self._writeString(bufPtr, bufLen, msg);
                self._writeU32(msgLenPtr, written);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_memory (allocation)
        // -------------------------------------------------------------------
        const wapi_memory = {
            alloc(size, align) {
                return self._moduleAlloc(size, align);
            },

            free(ptr) {
                self._moduleFree(ptr);
            },

            realloc(ptr, newSize, align) {
                if (ptr === 0) return self._moduleAlloc(newSize, align);
                if (newSize === 0) { self._moduleFree(ptr); return 0; }
                // Simple strategy: allocate new, copy, free old
                const newPtr = self._moduleAlloc(newSize, align);
                if (newPtr === 0) return 0;
                self._refreshViews();
                // Copy min(oldSize, newSize). We don't track old size well with
                // the bump allocator, so copy newSize bytes (safe because old
                // data is still there even if newSize > oldSize the extra is
                // just uninitialized).
                self._u8.copyWithin(newPtr, ptr, ptr + newSize);
                self._moduleFree(ptr);
                return newPtr;
            },

            usable_size(ptr) {
                // We don't track sizes in the bump allocator.
                // If the module exports malloc_usable_size, use it.
                const exports = self.instance.exports;
                if (exports.malloc_usable_size) {
                    return exports.malloc_usable_size(ptr);
                }
                return 0;
            },
        };

        // -------------------------------------------------------------------
        // wapi_io (unified I/O + event delivery)
        // The complete wapi_io_t vtable: submit, cancel, poll, wait, flush.
        // All events (I/O completions, input, lifecycle) come through poll/wait.
        // -------------------------------------------------------------------
        const WAPI_EVENT_IO_COMPLETION = 0x2000;
        const _ioPending = new Map();
        let _ioNextToken = 1;

        function _pushIoCompletion(userData, result, flags) {
            // Push a 128-byte event: wapi_io_event_t layout
            // type(4) + surface_id(4) + timestamp(8) + result(4) + flags(4) + user_data(8)
            self._eventQueue.push({
                type: WAPI_EVENT_IO_COMPLETION,
                userData, result, flags
            });
        }

        const wapi_io = {
            submit(implPtr, opsPtr, count) {
                self._refreshViews();
                let submitted = 0;
                for (let i = 0; i < count; i++) {
                    const base = opsPtr + i * 64;
                    const opcode    = self._dv.getUint32(base + 0, true);
                    const flags     = self._dv.getUint32(base + 4, true);
                    const fd        = self._dv.getInt32(base + 8, true);
                    const offset    = self._dv.getBigUint64(base + 16, true);
                    const addr      = self._dv.getUint32(base + 24, true);
                    const len       = self._dv.getUint32(base + 28, true);
                    const addr2     = self._dv.getUint32(base + 32, true);
                    const len2      = self._dv.getUint32(base + 36, true);
                    const userData  = self._dv.getBigUint64(base + 40, true);
                    const resultPtr = self._dv.getUint32(base + 48, true);
                    const flags2    = self._dv.getUint32(base + 52, true);

                    const token = _ioNextToken++;

                    switch (opcode) {
                        case WAPI_IO_OP_NOP:
                            _pushIoCompletion(userData, 0, 0);
                            break;

                        case WAPI_IO_OP_TIMEOUT: {
                            const ms = Number(offset) / 1_000_000;
                            const tid = setTimeout(() => {
                                _ioPending.delete(token);
                                _pushIoCompletion(userData, 0, 0);
                            }, ms);
                            _ioPending.set(token, { timeoutId: tid, userData });
                            break;
                        }

                        case WAPI_IO_OP_LOG: {
                            self._refreshViews();
                            const msg = new TextDecoder().decode(self._u8.subarray(addr, addr + len));
                            const tag = (addr2 && len2) ? new TextDecoder().decode(self._u8.subarray(addr2, addr2 + len2)) : null;
                            const prefix = tag ? `[${tag}]` : '';
                            switch (flags) {
                                case WAPI_LOG_DEBUG: console.debug(`${prefix} ${msg}`); break;
                                case WAPI_LOG_WARN:  console.warn(`${prefix} ${msg}`);  break;
                                case WAPI_LOG_ERROR: console.error(`${prefix} ${msg}`); break;
                                default:             console.log(`${prefix} ${msg}`);   break;
                            }
                            /* Fire-and-forget: no completion event */
                            break;
                        }

                        case WAPI_IO_OP_READ: {
                            const fdEntry = self.handles.get(fd);
                            if (!fdEntry || !fdEntry.node) {
                                _pushIoCompletion(userData, WAPI_ERR_BADF, 0);
                                break;
                            }
                            const node = fdEntry.node;
                            const readLen = Math.min(len, node.data.length - fdEntry.position);
                            if (readLen > 0) {
                                self._refreshViews();
                                self._u8.set(node.data.subarray(fdEntry.position, fdEntry.position + readLen), addr);
                                fdEntry.position += readLen;
                            }
                            if (resultPtr) self._writeU32(resultPtr, readLen);
                            _pushIoCompletion(userData, readLen, 0);
                            break;
                        }

                        case WAPI_IO_OP_WRITE: {
                            const fdEntry = self.handles.get(fd);
                            if (!fdEntry) {
                                if (fd === WAPI_STDOUT || fd === WAPI_STDERR) {
                                    self._refreshViews();
                                    const text = new TextDecoder().decode(self._u8.subarray(addr, addr + len));
                                    if (fd === WAPI_STDERR) console.error(text);
                                    else console.log(text);
                                    if (resultPtr) self._writeU32(resultPtr, len);
                                    _pushIoCompletion(userData, len, 0);
                                } else {
                                    _pushIoCompletion(userData, WAPI_ERR_BADF, 0);
                                }
                                break;
                            }
                            const node = fdEntry.node;
                            self._refreshViews();
                            const writeData = self._u8.slice(addr, addr + len);
                            const needed = fdEntry.position + len;
                            if (needed > node.data.length) {
                                const newBuf = new Uint8Array(needed);
                                newBuf.set(node.data);
                                node.data = newBuf;
                            }
                            node.data.set(writeData, fdEntry.position);
                            fdEntry.position += len;
                            node.mtime = Date.now();
                            if (resultPtr) self._writeU32(resultPtr, len);
                            _pushIoCompletion(userData, len, 0);
                            break;
                        }

                        case WAPI_IO_OP_CONNECT: {
                            const url = self._readString(addr2, len2);
                            const transport = flags2;
                            if (transport === WAPI_NET_TRANSPORT_WEBSOCKET) {
                                try {
                                    const ws = new WebSocket(url);
                                    ws._recvQueue = [];
                                    ws.binaryType = "arraybuffer";
                                    ws.onmessage = (ev) => {
                                        const data = ev.data instanceof ArrayBuffer
                                            ? new Uint8Array(ev.data)
                                            : new TextEncoder().encode(ev.data);
                                        ws._recvQueue.push(data);
                                    };
                                    ws.onopen = () => {
                                        const h = self.handles.insert({ type: "websocket", ws });
                                        if (resultPtr) self._writeI32(resultPtr, h);
                                        _pushIoCompletion(userData, h, 0);
                                        _ioPending.delete(token);
                                    };
                                    ws.onerror = () => {
                                        _pushIoCompletion(userData, WAPI_ERR_CONNREFUSED, 0);
                                        _ioPending.delete(token);
                                    };
                                    _ioPending.set(token, { userData });
                                } catch (e) {
                                    _pushIoCompletion(userData, WAPI_ERR_CONNREFUSED, 0);
                                }
                            } else {
                                const h = self.handles.insert({ type: "fetch", url, buffer: null });
                                if (resultPtr) self._writeI32(resultPtr, h);
                                _pushIoCompletion(userData, h, 0);
                            }
                            break;
                        }

                        default:
                            _pushIoCompletion(userData, WAPI_ERR_NOTSUP, 0);
                            break;
                    }
                    submitted++;
                }
                return submitted;
            },

            cancel(implPtr, userData) {
                for (const [token, pending] of _ioPending) {
                    if (pending.userData === userData) {
                        if (pending.timeoutId) clearTimeout(pending.timeoutId);
                        _ioPending.delete(token);
                        _pushIoCompletion(userData, WAPI_ERR_CANCELED, 0);
                        return WAPI_OK;
                    }
                }
                return WAPI_ERR_NOENT;
            },

            poll(implPtr, eventPtr) {
                self._pollGamepads();
                if (self._eventQueue.length === 0) return 0;
                const ev = self._eventQueue.shift();
                self._writeEvent(eventPtr, ev);
                return 1;
            },

            wait(implPtr, eventPtr, timeoutMs) {
                // Cannot block in browser; just poll
                return wapi_io.poll(implPtr, eventPtr);
            },

            flush(implPtr, eventType) {
                if (eventType === 0) {
                    self._eventQueue.length = 0;
                } else {
                    self._eventQueue = self._eventQueue.filter(e => e.type !== eventType);
                }
            },
        };

        // -------------------------------------------------------------------
        // wapi_clock (time)
        // -------------------------------------------------------------------
        const wapi_clock = {
            time_get(clockId, timePtr) {
                let ns;
                if (clockId === WAPI_CLOCK_MONOTONIC) {
                    ns = BigInt(Math.round(performance.now() * 1_000_000));
                } else {
                    // Wall clock
                    ns = BigInt(Date.now()) * 1_000_000n;
                }
                self._writeU64(timePtr, ns);
                return WAPI_OK;
            },

            resolution(clockId, resPtr) {
                // performance.now() typically has ~5us resolution in browsers
                if (clockId === WAPI_CLOCK_MONOTONIC) {
                    self._writeU64(resPtr, 1000n);  // 1 microsecond
                } else {
                    self._writeU64(resPtr, 1_000_000n);  // 1 millisecond
                }
                return WAPI_OK;
            },

            perf_counter() {
                // Return nanoseconds since perf origin
                const ms = performance.now() - self._perfOrigin;
                return BigInt(Math.round(ms * 1_000_000));
            },

            perf_frequency() {
                // performance.now() is in milliseconds, so frequency = 1e9 (ns per second)
                return 1_000_000_000n;
            },

            yield() {
                // No-op in browser main thread (cannot yield)
            },

            sleep(duration_ns) {
                // Cannot truly sleep on the main thread; log a warning.
                // In a SharedArrayBuffer + Atomics environment this could work.
                const ms = Number(duration_ns) / 1_000_000;
                console.warn(`[WAPI] wapi_sleep(${ms}ms) called on main thread - cannot block`);
            },
        };

        // -------------------------------------------------------------------
        // wapi_fs (filesystem - MEMFS)
        // -------------------------------------------------------------------
        const wapi_fs = {
            preopen_count() {
                return self._preopens.length;
            },

            preopen_path(index, bufPtr, bufLen, pathLenPtr) {
                if (index < 0 || index >= self._preopens.length) return WAPI_ERR_OVERFLOW;
                const path = self._preopens[index].path;
                const written = self._writeString(bufPtr, bufLen, path);
                self._writeU32(pathLenPtr, written);
                return WAPI_OK;
            },

            preopen_handle(index) {
                if (index < 0 || index >= self._preopens.length) return WAPI_HANDLE_INVALID;
                return self._preopens[index].handle;
            },

            open(dirFd, pathPtr, pathLen, oflags, fdflags, fdOutPtr) {
                const relPath = self._readString(pathPtr, pathLen);
                // Resolve the base directory path
                let basePath = "/";
                if (dirFd >= 4) {
                    const dirEntry = self.handles.get(dirFd);
                    if (dirEntry && dirEntry.path) basePath = dirEntry.path;
                }
                const fullPath = self.memfs._normPath(basePath, relPath);

                let node = self.memfs.stat(fullPath);
                if (!node && (oflags & WAPI_FS_OFLAG_CREATE)) {
                    if (oflags & WAPI_FS_OFLAG_DIRECTORY) {
                        node = self.memfs.mkdirp(fullPath);
                    } else {
                        node = self.memfs.createFile(fullPath, null);
                    }
                    if (!node) return WAPI_ERR_IO;
                } else if (!node) {
                    return WAPI_ERR_NOENT;
                }

                if ((oflags & WAPI_FS_OFLAG_EXCL) && node) {
                    return WAPI_ERR_EXIST;
                }

                if ((oflags & WAPI_FS_OFLAG_TRUNC) && node.type === WAPI_FILETYPE_REGULAR) {
                    node.data = new Uint8Array(0);
                    node.mtime = Date.now();
                }

                const fdEntry = new FDEntry(node, fullPath, fdflags);
                const h = self.handles.insert(fdEntry);
                self._writeI32(fdOutPtr, h);
                return WAPI_OK;
            },

            read(fd, bufPtr, len, bytesReadPtr) {
                if (fd === WAPI_STDIN) {
                    // No stdin in browser
                    self._writeU32(bytesReadPtr, 0);
                    return WAPI_OK;
                }
                const entry = self.handles.get(fd);
                if (!entry || !entry.node) return WAPI_ERR_BADF;
                if (entry.node.type !== WAPI_FILETYPE_REGULAR) return WAPI_ERR_ISDIR;

                const avail = entry.node.data.length - entry.position;
                const readLen = Math.min(len, avail);
                if (readLen > 0) {
                    self._refreshViews();
                    self._u8.set(
                        entry.node.data.subarray(entry.position, entry.position + readLen),
                        bufPtr
                    );
                    entry.position += readLen;
                }
                self._writeU32(bytesReadPtr, readLen);
                return WAPI_OK;
            },

            write(fd, bufPtr, len, bytesWrittenPtr) {
                self._refreshViews();
                if (fd === WAPI_STDOUT || fd === WAPI_STDERR) {
                    const text = self._readString(bufPtr, len);
                    if (fd === WAPI_STDERR) console.error(text);
                    else console.log(text);
                    self._writeU32(bytesWrittenPtr, len);
                    return WAPI_OK;
                }
                const entry = self.handles.get(fd);
                if (!entry || !entry.node) return WAPI_ERR_BADF;
                if (entry.node.type !== WAPI_FILETYPE_REGULAR) return WAPI_ERR_ISDIR;

                const node = entry.node;
                const writeData = self._u8.slice(bufPtr, bufPtr + len);
                const needed = entry.position + len;
                if (needed > node.data.length) {
                    const newBuf = new Uint8Array(needed);
                    newBuf.set(node.data);
                    node.data = newBuf;
                }
                node.data.set(writeData, entry.position);
                entry.position += len;
                node.mtime = Date.now();
                self._writeU32(bytesWrittenPtr, len);
                return WAPI_OK;
            },

            pread(fd, bufPtr, len, offset, bytesReadPtr) {
                const entry = self.handles.get(fd);
                if (!entry || !entry.node) return WAPI_ERR_BADF;
                const off = Number(offset);
                const avail = entry.node.data.length - off;
                const readLen = Math.min(len, Math.max(0, avail));
                if (readLen > 0) {
                    self._refreshViews();
                    self._u8.set(entry.node.data.subarray(off, off + readLen), bufPtr);
                }
                self._writeU32(bytesReadPtr, readLen);
                return WAPI_OK;
            },

            pwrite(fd, bufPtr, len, offset, bytesWrittenPtr) {
                const entry = self.handles.get(fd);
                if (!entry || !entry.node) return WAPI_ERR_BADF;
                const off = Number(offset);
                self._refreshViews();
                const writeData = self._u8.slice(bufPtr, bufPtr + len);
                const needed = off + len;
                if (needed > entry.node.data.length) {
                    const newBuf = new Uint8Array(needed);
                    newBuf.set(entry.node.data);
                    entry.node.data = newBuf;
                }
                entry.node.data.set(writeData, off);
                entry.node.mtime = Date.now();
                self._writeU32(bytesWrittenPtr, len);
                return WAPI_OK;
            },

            seek(fd, offset, whence, newOffsetPtr) {
                const entry = self.handles.get(fd);
                if (!entry || !entry.node) return WAPI_ERR_BADF;
                const off = Number(offset);
                let newPos;
                switch (whence) {
                    case WAPI_WHENCE_SET: newPos = off; break;
                    case WAPI_WHENCE_CUR: newPos = entry.position + off; break;
                    case WAPI_WHENCE_END: newPos = entry.node.data.length + off; break;
                    default: return WAPI_ERR_INVAL;
                }
                if (newPos < 0) return WAPI_ERR_INVAL;
                entry.position = newPos;
                self._writeU64(newOffsetPtr, BigInt(newPos));
                return WAPI_OK;
            },

            close(fd) {
                if (fd <= WAPI_STDERR) return WAPI_ERR_BADF;
                self.handles.remove(fd);
                return WAPI_OK;
            },

            sync(fd) {
                // MEMFS: no-op
                return WAPI_OK;
            },

            stat(fd, statPtr) {
                const entry = self.handles.get(fd);
                if (!entry || !entry.node) return WAPI_ERR_BADF;
                self._writeFileStat(statPtr, entry.node);
                return WAPI_OK;
            },

            path_stat(dirFd, pathPtr, pathLen, statPtr) {
                let basePath = "/";
                if (dirFd >= 4) {
                    const dirEntry = self.handles.get(dirFd);
                    if (dirEntry && dirEntry.path) basePath = dirEntry.path;
                }
                const relPath = self._readString(pathPtr, pathLen);
                const fullPath = self.memfs._normPath(basePath, relPath);
                const node = self.memfs.stat(fullPath);
                if (!node) return WAPI_ERR_NOENT;
                self._writeFileStat(statPtr, node);
                return WAPI_OK;
            },

            set_size(fd, size) {
                const entry = self.handles.get(fd);
                if (!entry || !entry.node) return WAPI_ERR_BADF;
                const sz = Number(size);
                const newBuf = new Uint8Array(sz);
                newBuf.set(entry.node.data.subarray(0, Math.min(entry.node.data.length, sz)));
                entry.node.data = newBuf;
                entry.node.mtime = Date.now();
                return WAPI_OK;
            },

            mkdir(dirFd, pathPtr, pathLen) {
                let basePath = "/";
                if (dirFd >= 4) {
                    const dirEntry = self.handles.get(dirFd);
                    if (dirEntry && dirEntry.path) basePath = dirEntry.path;
                }
                const relPath = self._readString(pathPtr, pathLen);
                const fullPath = self.memfs._normPath(basePath, relPath);
                if (self.memfs.stat(fullPath)) return WAPI_ERR_EXIST;
                const node = self.memfs.mkdirp(fullPath);
                return node ? WAPI_OK : WAPI_ERR_IO;
            },

            rmdir(dirFd, pathPtr, pathLen) {
                let basePath = "/";
                if (dirFd >= 4) {
                    const dirEntry = self.handles.get(dirFd);
                    if (dirEntry && dirEntry.path) basePath = dirEntry.path;
                }
                const relPath = self._readString(pathPtr, pathLen);
                const fullPath = self.memfs._normPath(basePath, relPath);
                const node = self.memfs.stat(fullPath);
                if (!node) return WAPI_ERR_NOENT;
                if (node.type !== WAPI_FILETYPE_DIRECTORY) return WAPI_ERR_NOTDIR;
                if (node.children.size > 0) return WAPI_ERR_NOTEMPTY;
                const { parent, name } = self.memfs._resolveParent(fullPath);
                if (parent) parent.children.delete(name);
                return WAPI_OK;
            },

            unlink(dirFd, pathPtr, pathLen) {
                let basePath = "/";
                if (dirFd >= 4) {
                    const dirEntry = self.handles.get(dirFd);
                    if (dirEntry && dirEntry.path) basePath = dirEntry.path;
                }
                const relPath = self._readString(pathPtr, pathLen);
                const fullPath = self.memfs._normPath(basePath, relPath);
                const node = self.memfs.stat(fullPath);
                if (!node) return WAPI_ERR_NOENT;
                if (node.type === WAPI_FILETYPE_DIRECTORY) return WAPI_ERR_ISDIR;
                const { parent, name } = self.memfs._resolveParent(fullPath);
                if (parent) parent.children.delete(name);
                return WAPI_OK;
            },

            rename(oldDirFd, oldPathPtr, oldPathLen, newDirFd, newPathPtr, newPathLen) {
                let oldBase = "/", newBase = "/";
                if (oldDirFd >= 4) {
                    const e = self.handles.get(oldDirFd);
                    if (e && e.path) oldBase = e.path;
                }
                if (newDirFd >= 4) {
                    const e = self.handles.get(newDirFd);
                    if (e && e.path) newBase = e.path;
                }
                const oldPath = self.memfs._normPath(oldBase, self._readString(oldPathPtr, oldPathLen));
                const newPath = self.memfs._normPath(newBase, self._readString(newPathPtr, newPathLen));

                const node = self.memfs.stat(oldPath);
                if (!node) return WAPI_ERR_NOENT;

                const { parent: oldParent, name: oldName } = self.memfs._resolveParent(oldPath);
                const { parent: newParent, name: newName } = self.memfs._resolveParent(newPath);
                if (!oldParent || !newParent) return WAPI_ERR_NOENT;

                oldParent.children.delete(oldName);
                node.name = newName;
                newParent.children.set(newName, node);
                return WAPI_OK;
            },

            readdir(fd, bufPtr, bufLen, cookie, usedPtr) {
                const entry = self.handles.get(fd);
                if (!entry || !entry.node || entry.node.type !== WAPI_FILETYPE_DIRECTORY) return WAPI_ERR_BADF;

                const entries = Array.from(entry.node.children.values());
                let offset = 0;
                let idx = Number(cookie);
                self._refreshViews();

                while (idx < entries.length && offset + 24 < bufLen) {
                    const child = entries[idx];
                    const nameBytes = new TextEncoder().encode(child.name);
                    const entrySize = 24 + nameBytes.length;
                    if (offset + entrySize > bufLen) break;

                    const base = bufPtr + offset;
                    // dirent_t: next(u64), ino(u64), namlen(u32), type(u32)
                    self._dv.setBigUint64(base + 0, BigInt(idx + 1), true);
                    self._dv.setBigUint64(base + 8, BigInt(child.ino), true);
                    self._dv.setUint32(base + 16, nameBytes.length, true);
                    self._dv.setUint32(base + 20, child.type, true);
                    self._u8.set(nameBytes, base + 24);

                    offset += entrySize;
                    idx++;
                }

                self._writeU32(usedPtr, offset);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_net (networking)
        // -------------------------------------------------------------------
        const wapi_net = {
            connect(descPtr, connOutPtr) {
                self._refreshViews();
                const urlPtr = self._readU32(descPtr + 0);
                const urlLen = self._readU32(descPtr + 4);
                const transport = self._readU32(descPtr + 8);
                const url = self._readString(urlPtr, urlLen);

                if (transport === WAPI_NET_TRANSPORT_WEBSOCKET) {
                    try {
                        const ws = new WebSocket(url);
                        ws._recvQueue = [];
                        ws.binaryType = "arraybuffer";
                        ws.onmessage = (ev) => {
                            const data = ev.data instanceof ArrayBuffer
                                ? new Uint8Array(ev.data)
                                : new TextEncoder().encode(ev.data);
                            ws._recvQueue.push(data);
                        };
                        const h = self.handles.insert({ type: "websocket", ws, url });
                        self._writeI32(connOutPtr, h);
                        return WAPI_OK;
                    } catch (e) {
                        self._lastError = e.message;
                        return WAPI_ERR_CONNREFUSED;
                    }
                }

                // HTTP / generic: store a fetch handle
                const h = self.handles.insert({ type: "fetch_conn", url });
                self._writeI32(connOutPtr, h);
                return WAPI_OK;
            },

            listen(descPtr, listenerOutPtr) {
                // Browsers cannot listen for incoming connections
                return WAPI_ERR_NOTSUP;
            },

            accept(listener, connOutPtr) {
                return WAPI_ERR_NOTSUP;
            },

            close(handle) {
                const obj = self.handles.remove(handle);
                if (!obj) return WAPI_ERR_BADF;
                if (obj.ws) {
                    try { obj.ws.close(); } catch (_) {}
                }
                if (obj.wt) {
                    try { obj.wt.close(); } catch (_) {}
                }
                return WAPI_OK;
            },

            stream_open(conn, type, streamOutPtr) {
                const obj = self.handles.get(conn);
                if (!obj) return WAPI_ERR_BADF;
                if (obj.wt && obj.wt.createBidirectionalStream) {
                    // WebTransport streams - would need async handling
                    return WAPI_ERR_NOTSUP;
                }
                // For WebSocket, the connection itself is the stream
                self._writeI32(streamOutPtr, conn);
                return WAPI_OK;
            },

            stream_accept(conn, streamOutPtr) {
                return WAPI_ERR_AGAIN;
            },

            send(handle, bufPtr, len, bytesSentPtr) {
                const obj = self.handles.get(handle);
                if (!obj) return WAPI_ERR_BADF;
                self._refreshViews();
                const data = self._u8.slice(bufPtr, bufPtr + len);

                if (obj.type === "websocket" && obj.ws) {
                    if (obj.ws.readyState !== WebSocket.OPEN) return WAPI_ERR_PIPE;
                    obj.ws.send(data);
                    self._writeU32(bytesSentPtr, len);
                    return WAPI_OK;
                }

                if (obj.type === "fetch_conn") {
                    // Use fetch POST for sending
                    fetch(obj.url, {
                        method: "POST",
                        body: data,
                    }).catch(() => {});
                    self._writeU32(bytesSentPtr, len);
                    return WAPI_OK;
                }

                return WAPI_ERR_BADF;
            },

            recv(handle, bufPtr, len, bytesRecvPtr) {
                const obj = self.handles.get(handle);
                if (!obj) return WAPI_ERR_BADF;

                if (obj.type === "websocket" && obj.ws) {
                    if (obj.ws._recvQueue.length === 0) {
                        self._writeU32(bytesRecvPtr, 0);
                        return WAPI_ERR_AGAIN;
                    }
                    const chunk = obj.ws._recvQueue.shift();
                    const copyLen = Math.min(chunk.length, len);
                    self._refreshViews();
                    self._u8.set(chunk.subarray(0, copyLen), bufPtr);
                    if (copyLen < chunk.length) {
                        // Push remainder back
                        obj.ws._recvQueue.unshift(chunk.subarray(copyLen));
                    }
                    self._writeU32(bytesRecvPtr, copyLen);
                    return WAPI_OK;
                }

                return WAPI_ERR_AGAIN;
            },

            send_datagram(conn, bufPtr, len) {
                return WAPI_ERR_NOTSUP;
            },

            recv_datagram(conn, bufPtr, len, recvLenPtr) {
                return WAPI_ERR_NOTSUP;
            },

            resolve(hostPtr, hostLen, addrsBufPtr, bufLen, countPtr) {
                // DNS resolution not available from browser JS
                self._writeU32(countPtr, 0);
                return WAPI_ERR_NOTSUP;
            },
        };

        // -------------------------------------------------------------------
        // wapi_gpu (WebGPU)
        // -------------------------------------------------------------------
        const wapi_gpu = {
            request_device(descPtr, deviceOutPtr) {
                // WebGPU is async; in a synchronous import we must have
                // pre-initialized the device. Return the cached handle.
                if (!self._gpuDevice) {
                    return WAPI_ERR_NOTCAPABLE;
                }
                const h = self.handles.insert({ type: "gpu_device", device: self._gpuDevice });
                self._writeI32(deviceOutPtr, h);
                return WAPI_OK;
            },

            get_queue(deviceHandle, queueOutPtr) {
                const obj = self.handles.get(deviceHandle);
                if (!obj || obj.type !== "gpu_device") return WAPI_ERR_BADF;
                const q = obj.device.queue;
                const h = self.handles.insert({ type: "gpu_queue", queue: q });
                self._writeI32(queueOutPtr, h);
                return WAPI_OK;
            },

            release_device(deviceHandle) {
                const obj = self.handles.remove(deviceHandle);
                if (!obj) return WAPI_ERR_BADF;
                // Don't actually destroy the device since we share it
                return WAPI_OK;
            },

            configure_surface(configPtr) {
                self._refreshViews();
                // Read wapi_gpu_surface_config_t (24 bytes)
                const nextInChain = self._readU32(configPtr + 0);
                const surfaceHandle = self._readI32(configPtr + 4);
                const deviceHandle = self._readI32(configPtr + 8);
                const format = self._readU32(configPtr + 12);
                const presentMode = self._readU32(configPtr + 16);
                const usage = self._readU32(configPtr + 20);

                const surfInfo = self._surfaces.get(surfaceHandle);
                if (!surfInfo) return WAPI_ERR_BADF;

                const devObj = self.handles.get(deviceHandle);
                if (!devObj || !devObj.device) return WAPI_ERR_BADF;

                const ctx = surfInfo.canvas.getContext("webgpu");
                if (!ctx) return WAPI_ERR_NOTCAPABLE;

                const gpuFormat = tpFormatToGPU(format);
                ctx.configure({
                    device: devObj.device,
                    format: gpuFormat,
                    alphaMode: "premultiplied",
                });

                self._gpuSurfaceConfigs.set(surfaceHandle, {
                    context: ctx,
                    format: gpuFormat,
                    device: devObj.device,
                });

                return WAPI_OK;
            },

            surface_get_current_texture(surfaceHandle, textureOutPtr, viewOutPtr) {
                const cfg = self._gpuSurfaceConfigs.get(surfaceHandle);
                if (!cfg) return WAPI_ERR_BADF;

                const texture = cfg.context.getCurrentTexture();
                const view = texture.createView();

                const th = self.handles.insert({ type: "gpu_texture", texture });
                const vh = self.handles.insert({ type: "gpu_texture_view", view });

                self._writeI32(textureOutPtr, th);
                self._writeI32(viewOutPtr, vh);
                return WAPI_OK;
            },

            surface_present(surfaceHandle) {
                // In WebGPU, presenting happens implicitly at the end of the
                // microtask. Nothing to do.
                return WAPI_OK;
            },

            surface_preferred_format(surfaceHandle, formatOutPtr) {
                if (navigator.gpu) {
                    const fmt = navigator.gpu.getPreferredCanvasFormat();
                    self._writeU32(formatOutPtr, gpuFormatToTP(fmt));
                } else {
                    self._writeU32(formatOutPtr, WAPI_GPU_FORMAT_BGRA8_UNORM);
                }
                return WAPI_OK;
            },

            get_proc_address(namePtr, nameLen) {
                // In the browser, WebGPU functions are not exposed as a proc
                // table. Return 0 (NULL) -- modules should use the WAPI bridge.
                return 0;
            },
        };

        // -------------------------------------------------------------------
        // wapi_surface (windowing - canvas)
        // -------------------------------------------------------------------
        const wapi_surface = {
            create(descPtr, surfaceOutPtr) {
                self._refreshViews();
                // Read wapi_surface_desc_t (24 bytes)
                const nextInChain = self._readU32(descPtr + 0);
                const titlePtr = self._readU32(descPtr + 4);
                const titleLen = self._readU32(descPtr + 8);
                const width    = self._readI32(descPtr + 12);
                const height   = self._readI32(descPtr + 16);
                const flags    = self._readU32(descPtr + 20);

                const title = titlePtr ? self._readString(titlePtr, titleLen) : "WAPI Application";

                const canvas = document.createElement("canvas");
                const dpr = (flags & WAPI_SURFACE_FLAG_HIGH_DPI) ? window.devicePixelRatio : 1;

                // In browser: fill the viewport
                canvas.style.position = "fixed";
                canvas.style.left = "0";
                canvas.style.top = "0";
                canvas.style.width = "100vw";
                canvas.style.height = "100vh";
                canvas.style.display = "block";

                const cw = width || window.innerWidth;
                const ch = height || window.innerHeight;
                canvas.width = Math.round(cw * dpr);
                canvas.height = Math.round(ch * dpr);

                document.body.appendChild(canvas);
                document.title = title;

                const h = self.handles.insert({ type: "surface" });
                const surfInfo = {
                    canvas,
                    dpr,
                    handle: h,
                    resizeObserver: null,
                };

                // ResizeObserver for resize events
                if (typeof ResizeObserver !== "undefined") {
                    surfInfo.resizeObserver = new ResizeObserver((entries) => {
                        for (const entry of entries) {
                            const cr = entry.contentRect;
                            const newDpr = window.devicePixelRatio;
                            canvas.width = Math.round(cr.width * newDpr);
                            canvas.height = Math.round(cr.height * newDpr);
                            surfInfo.dpr = newDpr;
                            self._pushSurfaceEvent(h, WAPI_EVENT_SURFACE_RESIZED,
                                canvas.width, canvas.height);
                        }
                    });
                    surfInfo.resizeObserver.observe(canvas);
                }

                // Fallback resize handler
                window.addEventListener("resize", () => {
                    const newDpr = window.devicePixelRatio;
                    canvas.width = Math.round(window.innerWidth * newDpr);
                    canvas.height = Math.round(window.innerHeight * newDpr);
                    surfInfo.dpr = newDpr;
                    self._pushSurfaceEvent(h, WAPI_EVENT_SURFACE_RESIZED,
                        canvas.width, canvas.height);
                });

                self._surfaces.set(h, surfInfo);
                if (self._activeSurfaceHandle === WAPI_HANDLE_INVALID) {
                    self._activeSurfaceHandle = h;
                    self._setupInputListeners(canvas, h);
                }

                self._writeI32(surfaceOutPtr, h);
                return WAPI_OK;
            },

            destroy(surfaceHandle) {
                const info = self._surfaces.get(surfaceHandle);
                if (!info) return WAPI_ERR_BADF;
                if (info.resizeObserver) info.resizeObserver.disconnect();
                info.canvas.remove();
                self._surfaces.delete(surfaceHandle);
                self.handles.remove(surfaceHandle);
                self._gpuSurfaceConfigs.delete(surfaceHandle);
                return WAPI_OK;
            },

            get_size(surfaceHandle, widthPtr, heightPtr) {
                const info = self._surfaces.get(surfaceHandle);
                if (!info) return WAPI_ERR_BADF;
                self._writeI32(widthPtr, info.canvas.width);
                self._writeI32(heightPtr, info.canvas.height);
                return WAPI_OK;
            },

            get_size_logical(surfaceHandle, widthPtr, heightPtr) {
                const info = self._surfaces.get(surfaceHandle);
                if (!info) return WAPI_ERR_BADF;
                self._writeI32(widthPtr, Math.round(info.canvas.width / info.dpr));
                self._writeI32(heightPtr, Math.round(info.canvas.height / info.dpr));
                return WAPI_OK;
            },

            get_dpi_scale(surfaceHandle, scalePtr) {
                const info = self._surfaces.get(surfaceHandle);
                if (!info) return WAPI_ERR_BADF;
                self._writeF32(scalePtr, info.dpr);
                return WAPI_OK;
            },

            request_size(surfaceHandle, width, height) {
                // Browsers don't support resizing the tab, but we can resize the canvas
                const info = self._surfaces.get(surfaceHandle);
                if (!info) return WAPI_ERR_BADF;
                info.canvas.width = Math.round(width * info.dpr);
                info.canvas.height = Math.round(height * info.dpr);
                return WAPI_OK;
            },

            set_title(surfaceHandle, titlePtr, titleLen) {
                const title = self._readString(titlePtr, titleLen);
                document.title = title;
                return WAPI_OK;
            },

            set_fullscreen(surfaceHandle, fullscreen) {
                const info = self._surfaces.get(surfaceHandle);
                if (!info) return WAPI_ERR_BADF;
                if (fullscreen) {
                    info.canvas.requestFullscreen().catch(() => {});
                } else {
                    if (document.fullscreenElement) {
                        document.exitFullscreen().catch(() => {});
                    }
                }
                return WAPI_OK;
            },

            set_visible(surfaceHandle, visible) {
                const info = self._surfaces.get(surfaceHandle);
                if (!info) return WAPI_ERR_BADF;
                info.canvas.style.display = visible ? "block" : "none";
                return WAPI_OK;
            },

            minimize(surfaceHandle) {
                // Cannot minimize a browser tab
                return WAPI_OK;
            },

            maximize(surfaceHandle) {
                return WAPI_OK;
            },

            restore(surfaceHandle) {
                return WAPI_OK;
            },

            set_cursor(surfaceHandle, cursorType) {
                const info = self._surfaces.get(surfaceHandle);
                if (!info) return WAPI_ERR_BADF;
                info.canvas.style.cursor = WAPI_CURSOR_NAMES[cursorType] || "default";
                return WAPI_OK;
            },

            display_count() {
                return 1; // Browser always has one display
            },

            display_info(index, widthPtr, heightPtr, hzPtr) {
                if (index !== 0) return WAPI_ERR_OVERFLOW;
                self._writeI32(widthPtr, screen.width * window.devicePixelRatio);
                self._writeI32(heightPtr, screen.height * window.devicePixelRatio);
                self._writeI32(hzPtr, 60); // Assume 60Hz; no reliable API for this
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_input (events)
        // -------------------------------------------------------------------
        const wapi_input = {
            poll_event(eventPtr) {
                // Also poll gamepads
                self._pollGamepads();

                if (self._eventQueue.length === 0) return 0;
                const ev = self._eventQueue.shift();
                self._writeEvent(eventPtr, ev);
                return 1;
            },

            wait_event(eventPtr, timeoutMs) {
                // Cannot block in browser; just poll
                return wapi_input.poll_event(eventPtr);
            },

            flush_events(eventType) {
                if (eventType === 0) {
                    self._eventQueue.length = 0;
                } else {
                    self._eventQueue = self._eventQueue.filter(e => e.type !== eventType);
                }
            },

            key_pressed(scancode) {
                return self._keyState.has(scancode) ? 1 : 0;
            },

            get_mod_state() {
                return self._modState;
            },

            mouse_position(surfaceHandle, xPtr, yPtr) {
                self._writeF32(xPtr, self._mouseX);
                self._writeF32(yPtr, self._mouseY);
                return WAPI_OK;
            },

            mouse_button_state() {
                return self._mouseButtons;
            },

            set_relative_mouse(surfaceHandle, enabled) {
                const info = self._surfaces.get(surfaceHandle);
                if (!info) return WAPI_ERR_BADF;
                if (enabled) {
                    info.canvas.requestPointerLock();
                } else {
                    document.exitPointerLock();
                }
                return WAPI_OK;
            },

            start_text_input(surfaceHandle) {
                // No-op in browser -- keyboard events always fire
            },

            stop_text_input(surfaceHandle) {
                // No-op
            },
        };

        // -------------------------------------------------------------------
        // wapi_audio (Web Audio API)
        // -------------------------------------------------------------------
        const wapi_audio = {
            open_device(deviceId, specPtr, deviceOutPtr) {
                if (!self._audioCtx) {
                    const AC = window.AudioContext || window.webkitAudioContext;
                    if (!AC) return WAPI_ERR_NOTCAPABLE;
                    self._audioCtx = new AC();
                }
                const h = self.handles.insert({
                    type: "audio_device",
                    ctx: self._audioCtx,
                    streams: [],
                    paused: true,
                });
                self._writeI32(deviceOutPtr, h);
                return WAPI_OK;
            },

            close_device(deviceHandle) {
                const obj = self.handles.remove(deviceHandle);
                if (!obj) return WAPI_ERR_BADF;
                // Don't close the shared AudioContext
                return WAPI_OK;
            },

            resume_device(deviceHandle) {
                const obj = self.handles.get(deviceHandle);
                if (!obj || obj.type !== "audio_device") return WAPI_ERR_BADF;
                obj.paused = false;
                if (obj.ctx.state === "suspended") {
                    obj.ctx.resume().catch(() => {});
                }
                return WAPI_OK;
            },

            pause_device(deviceHandle) {
                const obj = self.handles.get(deviceHandle);
                if (!obj || obj.type !== "audio_device") return WAPI_ERR_BADF;
                obj.paused = true;
                return WAPI_OK;
            },

            create_stream(srcSpecPtr, dstSpecPtr, streamOutPtr) {
                self._refreshViews();
                const srcFormat   = self._readU32(srcSpecPtr + 0);
                const srcChannels = self._readI32(srcSpecPtr + 4);
                const srcFreq     = self._readI32(srcSpecPtr + 8);

                if (!self._audioCtx) {
                    const AC = window.AudioContext || window.webkitAudioContext;
                    if (!AC) return WAPI_ERR_NOTCAPABLE;
                    self._audioCtx = new AC();
                }

                const h = self.handles.insert({
                    type: "audio_stream",
                    srcFormat,
                    channels: srcChannels,
                    sampleRate: srcFreq,
                    buffer: [],       // queued Float32Array chunks
                    totalBytes: 0,
                    deviceHandle: null,
                    scriptNode: null,
                });
                self._writeI32(streamOutPtr, h);
                return WAPI_OK;
            },

            destroy_stream(streamHandle) {
                const obj = self.handles.remove(streamHandle);
                if (!obj) return WAPI_ERR_BADF;
                if (obj.scriptNode) {
                    obj.scriptNode.disconnect();
                    obj.scriptNode = null;
                }
                return WAPI_OK;
            },

            bind_stream(deviceHandle, streamHandle) {
                const dev = self.handles.get(deviceHandle);
                const stream = self.handles.get(streamHandle);
                if (!dev || !stream) return WAPI_ERR_BADF;

                stream.deviceHandle = deviceHandle;
                const ctx = dev.ctx;
                const bufSize = 4096;
                const channels = stream.channels;

                // Use ScriptProcessorNode (AudioWorklet would be better but
                // requires a separate JS file served from a URL)
                const node = ctx.createScriptProcessor(bufSize, 0, channels);
                node.onaudioprocess = (e) => {
                    if (dev.paused) return;
                    const output = e.outputBuffer;
                    const framesNeeded = output.length;

                    for (let ch = 0; ch < channels; ch++) {
                        const out = output.getChannelData(ch);
                        let written = 0;
                        while (written < framesNeeded && stream.buffer.length > 0) {
                            const chunk = stream.buffer[0];
                            const available = chunk.length / channels - 0;
                            const toCopy = Math.min(framesNeeded - written, chunk.length / channels);
                            for (let i = 0; i < toCopy; i++) {
                                out[written + i] = chunk[i * channels + ch] || 0;
                            }
                            written += toCopy;
                            if (toCopy * channels >= chunk.length) {
                                stream.buffer.shift();
                            } else {
                                stream.buffer[0] = chunk.subarray(toCopy * channels);
                            }
                        }
                        // Fill remainder with silence
                        for (let i = written; i < framesNeeded; i++) {
                            out[i] = 0;
                        }
                    }
                };
                node.connect(ctx.destination);
                stream.scriptNode = node;
                dev.streams.push(streamHandle);

                return WAPI_OK;
            },

            unbind_stream(streamHandle) {
                const stream = self.handles.get(streamHandle);
                if (!stream) return WAPI_ERR_BADF;
                if (stream.scriptNode) {
                    stream.scriptNode.disconnect();
                    stream.scriptNode = null;
                }
                stream.deviceHandle = null;
                return WAPI_OK;
            },

            put_stream_data(streamHandle, bufPtr, len) {
                const stream = self.handles.get(streamHandle);
                if (!stream || stream.type !== "audio_stream") return WAPI_ERR_BADF;

                self._refreshViews();
                const srcFormat = stream.srcFormat;

                // Convert incoming data to Float32Array
                let floats;
                if (srcFormat === WAPI_AUDIO_F32) {
                    const count = len / 4;
                    floats = new Float32Array(count);
                    for (let i = 0; i < count; i++) {
                        floats[i] = self._dv.getFloat32(bufPtr + i * 4, true);
                    }
                } else if (srcFormat === WAPI_AUDIO_S16) {
                    const count = len / 2;
                    floats = new Float32Array(count);
                    for (let i = 0; i < count; i++) {
                        floats[i] = self._dv.getInt16(bufPtr + i * 2, true) / 32768.0;
                    }
                } else if (srcFormat === WAPI_AUDIO_U8) {
                    floats = new Float32Array(len);
                    for (let i = 0; i < len; i++) {
                        floats[i] = (self._u8[bufPtr + i] - 128) / 128.0;
                    }
                } else if (srcFormat === WAPI_AUDIO_S32) {
                    const count = len / 4;
                    floats = new Float32Array(count);
                    for (let i = 0; i < count; i++) {
                        floats[i] = self._dv.getInt32(bufPtr + i * 4, true) / 2147483648.0;
                    }
                } else {
                    return WAPI_ERR_INVAL;
                }

                stream.buffer.push(floats);
                stream.totalBytes += len;
                return WAPI_OK;
            },

            get_stream_data(streamHandle, bufPtr, len, bytesReadPtr) {
                // Recording: not yet implemented
                self._writeU32(bytesReadPtr, 0);
                return WAPI_ERR_NOTSUP;
            },

            stream_available(streamHandle) {
                const stream = self.handles.get(streamHandle);
                if (!stream) return 0;
                let total = 0;
                for (const chunk of stream.buffer) total += chunk.length * 4;
                return total;
            },

            stream_queued(streamHandle) {
                const stream = self.handles.get(streamHandle);
                if (!stream) return 0;
                // Report how much more we can accept (essentially unlimited)
                return 65536;
            },

            open_device_stream(deviceId, specPtr, deviceOutPtr, streamOutPtr) {
                let r = wapi_audio.open_device(deviceId, specPtr, deviceOutPtr);
                if (r !== WAPI_OK) return r;
                r = wapi_audio.create_stream(specPtr, 0, streamOutPtr);
                if (r !== WAPI_OK) return r;
                self._refreshViews();
                const devH = self._readI32(deviceOutPtr);
                const strH = self._readI32(streamOutPtr);
                return wapi_audio.bind_stream(devH, strH);
            },

            playback_device_count() {
                return 1; // Default device
            },

            recording_device_count() {
                return 0;
            },

            device_name(deviceId, bufPtr, bufLen, nameLenPtr) {
                const name = "Default Audio Device";
                const written = self._writeString(bufPtr, bufLen, name);
                self._writeU32(nameLenPtr, written);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_content (Content tree declaration for a11y, keyboard nav, indexing)
        // -------------------------------------------------------------------
        const wapi_content = {
            register_tree(treePtr, capacity) {
                // Store the content tree pointer for a11y reading
                self._contentTreePtr = treePtr;
                self._contentTreeCapacity = capacity;
                // TODO: set up ARIA live region or a11y overlay based on tree
                return WAPI_OK;
            },

            notify() {
                // TODO: re-read content tree from self._contentTreePtr,
                // sync with ARIA attributes / a11y overlay
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_text (Text shaping + layout)
        // -------------------------------------------------------------------
        const wapi_text = {
            // Shaping
            shape(fontPtr, textPtr, textLen, script, direction) {
                // TODO: implement with canvas measureText / OffscreenCanvas
                return 0; // WAPI_HANDLE_INVALID
            },
            shape_glyph_count(result) { return 0; },
            shape_get_glyphs(result, infosPtr, positionsPtr) { return WAPI_ERR_NOTSUP; },
            shape_get_font_metrics(result, metricsPtr) { return WAPI_ERR_NOTSUP; },
            shape_destroy(result) { return WAPI_OK; },

            // Layout
            layout_create(textPtr, constraintsPtr) {
                // TODO: implement with canvas text measurement
                return 0; // WAPI_HANDLE_INVALID
            },
            layout_get_size(layout, widthPtr, heightPtr) { return WAPI_ERR_NOTSUP; },
            layout_line_count(layout) { return 0; },
            layout_get_line_info(layout, lineIdx, infoPtr) { return WAPI_ERR_NOTSUP; },
            layout_hit_test(layout, x, y, resultPtr) { return WAPI_ERR_NOTSUP; },
            layout_get_caret(layout, charOffset, infoPtr) { return WAPI_ERR_NOTSUP; },
            layout_update_text(layout, textPtr) { return WAPI_ERR_NOTSUP; },
            layout_update_constraints(layout, constraintsPtr) { return WAPI_ERR_NOTSUP; },
            layout_destroy(layout) { return WAPI_OK; },
        };

        // -------------------------------------------------------------------
        // wapi_clipboard
        // -------------------------------------------------------------------
        const wapi_clipboard = {
            has_format(format) {
                if (format === WAPI_CLIPBOARD_TEXT) return self._clipboardText.length > 0 ? 1 : 0;
                if (format === WAPI_CLIPBOARD_HTML) return self._clipboardHtml.length > 0 ? 1 : 0;
                return 0;
            },

            read(format, bufPtr, bufLen, bytesWrittenPtr) {
                let data = "";
                if (format === WAPI_CLIPBOARD_TEXT) data = self._clipboardText;
                else if (format === WAPI_CLIPBOARD_HTML) data = self._clipboardHtml;
                else return WAPI_ERR_NOTSUP;

                if (data.length === 0) return WAPI_ERR_NOENT;

                const written = self._writeString(bufPtr, bufLen, data);
                self._writeU32(bytesWrittenPtr, written);
                return WAPI_OK;
            },

            write(format, dataPtr, len) {
                const text = self._readString(dataPtr, len);
                if (format === WAPI_CLIPBOARD_TEXT) {
                    self._clipboardText = text;
                    if (navigator.clipboard && navigator.clipboard.writeText) {
                        navigator.clipboard.writeText(text).catch(() => {});
                    }
                    return WAPI_OK;
                }
                if (format === WAPI_CLIPBOARD_HTML) {
                    self._clipboardHtml = text;
                    return WAPI_OK;
                }
                return WAPI_ERR_NOTSUP;
            },

            clear() {
                self._clipboardText = "";
                self._clipboardHtml = "";
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_kv (key-value storage via localStorage)
        // -------------------------------------------------------------------
        const wapi_kv = {
            get(keyPtr, keyLen, bufPtr, bufLen, valLenPtr) {
                const key = self._readString(keyPtr, keyLen);
                const val = localStorage.getItem("wapi_kv_" + key);
                if (val === null) return WAPI_ERR_NOENT;
                const encoded = new TextEncoder().encode(val);
                self._writeU32(valLenPtr, encoded.length);
                const copyLen = Math.min(encoded.length, bufLen);
                self._refreshViews();
                self._u8.set(encoded.subarray(0, copyLen), bufPtr);
                return WAPI_OK;
            },
            set(keyPtr, keyLen, valPtr, valLen) {
                const key = self._readString(keyPtr, keyLen);
                self._refreshViews();
                const val = new TextDecoder().decode(self._u8.subarray(valPtr, valPtr + valLen));
                localStorage.setItem("wapi_kv_" + key, val);
                return WAPI_OK;
            },
            delete(keyPtr, keyLen) {
                const key = self._readString(keyPtr, keyLen);
                localStorage.removeItem("wapi_kv_" + key);
                return WAPI_OK;
            },
            has(keyPtr, keyLen) {
                const key = self._readString(keyPtr, keyLen);
                return localStorage.getItem("wapi_kv_" + key) !== null ? 1 : 0;
            },
            clear() {
                const keys = [];
                for (let i = 0; i < localStorage.length; i++) {
                    const k = localStorage.key(i);
                    if (k.startsWith("wapi_kv_")) keys.push(k);
                }
                keys.forEach(k => localStorage.removeItem(k));
                return WAPI_OK;
            },
            count() {
                let n = 0;
                for (let i = 0; i < localStorage.length; i++) {
                    if (localStorage.key(i).startsWith("wapi_kv_")) n++;
                }
                return n;
            },
            key_at(index, bufPtr, bufLen, keyLenPtr) {
                let n = 0;
                for (let i = 0; i < localStorage.length; i++) {
                    const k = localStorage.key(i);
                    if (k.startsWith("wapi_kv_")) {
                        if (n === index) {
                            const name = k.substring(8); // remove "wapi_kv_"
                            const written = self._writeString(bufPtr, bufLen, name);
                            self._writeU32(keyLenPtr, written);
                            return WAPI_OK;
                        }
                        n++;
                    }
                }
                return WAPI_ERR_OVERFLOW;
            },
        };

        // -------------------------------------------------------------------
        // wapi_font (basic font queries)
        // -------------------------------------------------------------------
        const wapi_font = {
            family_count() {
                // Can't reliably enumerate all fonts in browser
                // Return a known set of web-safe fonts
                return 12; // serif, sans-serif, monospace, cursive, fantasy, system-ui, + common ones
            },
            family_info(index, infoPtr) {
                const fonts = ["serif", "sans-serif", "monospace", "cursive", "fantasy", "system-ui",
                               "Arial", "Times New Roman", "Courier New", "Georgia", "Verdana", "Helvetica"];
                if (index < 0 || index >= fonts.length) return WAPI_ERR_OVERFLOW;
                // Write wapi_font_info_t (24 bytes):
                //   ptr family(0), u32 family_len(4), u32 weight_min(8),
                //   u32 weight_max(12), u32 style_flags(16), i32 is_variable(20)
                // We can't write the family name pointer directly (it's in wasm memory)
                // For now, write zeros for the pointer fields and fill weight info
                self._refreshViews();
                self._writeU32(infoPtr + 0, 0);      // family ptr (can't set from here meaningfully)
                self._writeU32(infoPtr + 4, 0);      // family_len
                self._writeU32(infoPtr + 8, 100);    // weight_min
                self._writeU32(infoPtr + 12, 900);   // weight_max
                self._writeU32(infoPtr + 16, 0x0007); // HAS_NORMAL | HAS_ITALIC | HAS_OBLIQUE
                self._writeI32(infoPtr + 20, 0);     // is_variable
                return WAPI_OK;
            },
            supports_script(tagPtr, tagLen) {
                return 1; // Browser handles all scripts
            },
            has_feature(familyPtr, familyLen, tag) {
                return 0; // Unknown
            },
            fallback_count(familyPtr, familyLen) {
                return 0;
            },
            fallback_get(familyPtr, familyLen, index, bufPtr, bufLen, nameLenPtr) {
                return WAPI_ERR_OVERFLOW;
            },
        };

        // -------------------------------------------------------------------
        // wapi_crypto (Web Crypto API)
        // -------------------------------------------------------------------
        // NOTE: The Web Crypto API (crypto.subtle) is entirely async (returns
        // Promises). WAPI's synchronous C-style calling convention cannot call
        // async browser APIs on the main thread without blocking, so all
        // operations return WAPI_ERR_NOTSUP. A future async WAPI extension
        // or JSPI-based approach could enable these.
        const wapi_crypto = {
            hash(algo, dataPtr, dataLen, digestPtr, digestLenPtr) {
                return WAPI_ERR_NOTSUP;
            },
            hash_create(algo, ctxPtr) { return WAPI_ERR_NOTSUP; },
            hash_update(ctx, dataPtr, dataLen) { return WAPI_ERR_NOTSUP; },
            hash_finish(ctx, digestPtr, digestLenPtr) { return WAPI_ERR_NOTSUP; },
            key_import_raw(dataPtr, keyLen, usages, keyPtr) { return WAPI_ERR_NOTSUP; },
            key_generate(algo, usages, keyPtr) { return WAPI_ERR_NOTSUP; },
            key_generate_pair(algo, usages, pubPtr, privPtr) { return WAPI_ERR_NOTSUP; },
            key_release(key) { return WAPI_OK; },
            encrypt(algo, key, ivPtr, ivLen, ptPtr, ptLen, ctPtr, ctLenPtr) { return WAPI_ERR_NOTSUP; },
            decrypt(algo, key, ivPtr, ivLen, ctPtr, ctLen, ptPtr, ptLenPtr) { return WAPI_ERR_NOTSUP; },
            sign(algo, key, dataPtr, dataLen, sigPtr, sigLenPtr) { return WAPI_ERR_NOTSUP; },
            verify(algo, key, dataPtr, dataLen, sigPtr, sigLen) { return WAPI_ERR_NOTSUP; },
            derive_key(algo, baseKey, saltPtr, saltLen, infoPtr, infoLen, iterations, keyLen, derivedPtr, derivedLenPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_video (video/media playback - stub)
        // -------------------------------------------------------------------
        const wapi_video = {
            create(descPtr, videoPtr) { return WAPI_ERR_NOTSUP; },
            destroy(video) { return WAPI_ERR_NOTSUP; },
            get_info(video, infoPtr) { return WAPI_ERR_NOTSUP; },
            play(video) { return WAPI_ERR_NOTSUP; },
            pause(video) { return WAPI_ERR_NOTSUP; },
            seek(video, time) { return WAPI_ERR_NOTSUP; },
            get_state(video, statePtr) { return WAPI_ERR_NOTSUP; },
            get_position(video, posPtr) { return WAPI_ERR_NOTSUP; },
            get_frame_texture(video, texPtr) { return WAPI_ERR_NOTSUP; },
            render_to_texture(video, tex, x, y, w, h) { return WAPI_ERR_NOTSUP; },
            bind_audio(video, stream) { return WAPI_ERR_NOTSUP; },
            set_volume(video, vol) { return WAPI_ERR_NOTSUP; },
            set_muted(video, muted) { return WAPI_ERR_NOTSUP; },
            set_loop(video, loop) { return WAPI_ERR_NOTSUP; },
            set_playback_rate(video, rate) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_module (shared module linking - stub)
        // -------------------------------------------------------------------
        const wapi_module = {
            load(importPtr, modulePtr) { return WAPI_ERR_NOTSUP; },
            init(mod, ctxPtr) { return WAPI_ERR_NOTSUP; },
            get_func(mod, namePtr, nameLen) { return 0; },
            get_desc(mod, descPtr) { return WAPI_ERR_NOTSUP; },
            release(mod) { return WAPI_OK; },
            lend(ptr, len, leasePtr) { return WAPI_ERR_NOTSUP; },
            return_lease(lease) { return WAPI_ERR_NOTSUP; },
            is_lent(ptr, len) { return 0; },
            is_cached(namePtr, nameLen, major) { return 0; },
            prefetch(importPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_notify (notifications - stub)
        // -------------------------------------------------------------------
        const wapi_notify = {
            request_permission() { return WAPI_ERR_NOTSUP; },
            is_permitted() { return 0; },
            show(descPtr, idPtr) { return WAPI_ERR_NOTSUP; },
            close(id) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_geo (geolocation - stub, browser API is async)
        // -------------------------------------------------------------------
        // NOTE: The browser Geolocation API (navigator.geolocation) is
        // async-only (callback-based). Synchronous position queries are not
        // possible on the main thread, so get_position returns NOTSUP.
        // A future async WAPI extension could enable these.
        const wapi_geo = {
            get_position(flags, timeout_ms, positionPtr) { return WAPI_ERR_NOTSUP; },
            watch_position(flags, watchPtr) { return WAPI_ERR_NOTSUP; },
            clear_watch(watch) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_sensor (sensors - stub)
        // -------------------------------------------------------------------
        const wapi_sensor = {
            available(type) { return 0; },
            start(type, freqHz, sensorPtr) { return WAPI_ERR_NOTSUP; },
            stop(sensor) { return WAPI_ERR_NOTSUP; },
            read_xyz(sensor, readingPtr) { return WAPI_ERR_NOTSUP; },
            read_scalar(sensor, readingPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_speech (speech synthesis/recognition - stub)
        // -------------------------------------------------------------------
        const wapi_speech = {
            speak(utterancePtr, idPtr) { return WAPI_ERR_NOTSUP; },
            cancel(id) { return WAPI_ERR_NOTSUP; },
            cancel_all() { return WAPI_ERR_NOTSUP; },
            is_speaking() { return 0; },
            recognize_start(langPtr, langLen, continuous, sessionPtr) { return WAPI_ERR_NOTSUP; },
            recognize_stop(session) { return WAPI_ERR_NOTSUP; },
            recognize_result(session, bufPtr, bufLen, textLenPtr, confidencePtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_bio (biometric authentication - stub)
        // -------------------------------------------------------------------
        const wapi_bio = {
            available_types() { return 0; },
            authenticate(type, reasonPtr, reasonLen) { return WAPI_ERR_NOTSUP; },
            can_authenticate() { return 0; },
        };

        // -------------------------------------------------------------------
        // wapi_share (share sheet - stub)
        // -------------------------------------------------------------------
        const wapi_share = {
            can_share() { return 0; },
            share(dataPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_pay (payments - stub)
        // -------------------------------------------------------------------
        const wapi_pay = {
            can_make_payment() { return 0; },
            request_payment(requestPtr, tokenPtr, tokenLenPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_usb (USB - stub)
        // -------------------------------------------------------------------
        const wapi_usb = {
            request_device(filtersPtr, filterCount, devicePtr) { return WAPI_ERR_NOTSUP; },
            open(device) { return WAPI_ERR_NOTSUP; },
            close(device) { return WAPI_ERR_NOTSUP; },
            claim_interface(device, interfaceNum) { return WAPI_ERR_NOTSUP; },
            release_interface(device, interfaceNum) { return WAPI_ERR_NOTSUP; },
            transfer_in(device, endpoint, bufPtr, len, transferredPtr) { return WAPI_ERR_NOTSUP; },
            transfer_out(device, endpoint, bufPtr, len, transferredPtr) { return WAPI_ERR_NOTSUP; },
            control_transfer(device, requestType, request, value, index, bufPtr, len, transferredPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_midi (MIDI - stub)
        // -------------------------------------------------------------------
        const wapi_midi = {
            request_access(sysex) { return WAPI_ERR_NOTSUP; },
            port_count(type) { return 0; },
            port_name(type, index, bufPtr, bufLen, nameLenPtr) { return WAPI_ERR_NOTSUP; },
            open_port(type, index, portPtr) { return WAPI_ERR_NOTSUP; },
            close_port(port) { return WAPI_ERR_NOTSUP; },
            send(port, dataPtr, len) { return WAPI_ERR_NOTSUP; },
            recv(port, bufPtr, bufLen, msgLenPtr, timestampPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_bt (Bluetooth LE - stub)
        // -------------------------------------------------------------------
        const wapi_bt = {
            request_device(filtersPtr, filterCount, devicePtr) { return WAPI_ERR_NOTSUP; },
            connect(device) { return WAPI_ERR_NOTSUP; },
            disconnect(device) { return WAPI_ERR_NOTSUP; },
            get_service(device, uuidPtr, uuidLen, servicePtr) { return WAPI_ERR_NOTSUP; },
            get_characteristic(service, uuidPtr, uuidLen, charPtr) { return WAPI_ERR_NOTSUP; },
            read_value(characteristic, bufPtr, bufLen, valLenPtr) { return WAPI_ERR_NOTSUP; },
            write_value(characteristic, dataPtr, len) { return WAPI_ERR_NOTSUP; },
            start_notifications(characteristic) { return WAPI_ERR_NOTSUP; },
            stop_notifications(characteristic) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_camera (camera - stub)
        // -------------------------------------------------------------------
        const wapi_camera = {
            count() { return 0; },
            open(configPtr, cameraPtr) { return WAPI_ERR_NOTSUP; },
            close(camera) { return WAPI_ERR_NOTSUP; },
            read_frame(camera, framePtr, bufPtr, bufLen, sizePtr) { return WAPI_ERR_NOTSUP; },
            read_frame_gpu(camera, framePtr, texturePtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_xr (XR / extended reality - stub)
        // -------------------------------------------------------------------
        const wapi_xr = {
            is_supported(type) { return 0; },
            request_session(type, sessionPtr) { return WAPI_ERR_NOTSUP; },
            end_session(session) { return WAPI_ERR_NOTSUP; },
            create_ref_space(session, type, spacePtr) { return WAPI_ERR_NOTSUP; },
            wait_frame(session, statePtr, viewsPtr, maxViews) { return WAPI_ERR_NOTSUP; },
            begin_frame(session) { return WAPI_ERR_NOTSUP; },
            end_frame(session, texturesPtr, texCount) { return WAPI_ERR_NOTSUP; },
            get_controller_pose(session, space, hand, posePtr) { return WAPI_ERR_NOTSUP; },
            get_controller_state(session, hand, buttonsPtr, triggerPtr, gripPtr, thumbXPtr, thumbYPtr) { return WAPI_ERR_NOTSUP; },
            hit_test(session, space, originPtr, directionPtr, posePtr) { return WAPI_ERR_NOTSUP; },
        };

        // wapi_event: REMOVED — event delivery is now part of wapi_io vtable
        // (poll/wait/flush are on the wapi_io object above)

        // -------------------------------------------------------------------
        // wapi_register (app registration - stub)
        // -------------------------------------------------------------------
        const wapi_register = {
            url_scheme(schemePtr, schemeLen) { return WAPI_ERR_NOTSUP; },
            unregister_url_scheme(schemePtr, schemeLen) { return WAPI_ERR_NOTSUP; },
            file_type(descPtr) { return WAPI_ERR_NOTSUP; },
            unregister_file_type(extPtr, extLen) { return WAPI_ERR_NOTSUP; },
            preview_provider(extPtr, extLen) { return WAPI_ERR_NOTSUP; },
            is_default_for_scheme(schemePtr, schemeLen) { return 0; },
            is_default_for_type(extPtr, extLen) { return 0; },
        };

        // -------------------------------------------------------------------
        // wapi_taskbar (taskbar/dock - stub)
        // -------------------------------------------------------------------
        const wapi_taskbar = {
            set_progress(surface, state, value) { return WAPI_ERR_NOTSUP; },
            set_badge(count) { return WAPI_ERR_NOTSUP; },
            request_attention(surface, critical) { return WAPI_ERR_NOTSUP; },
            set_overlay_icon(surface, iconData, iconLen, desc, descLen) { return WAPI_ERR_NOTSUP; },
            clear_overlay(surface) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_perm (permissions)
        // -------------------------------------------------------------------
        const wapi_perm = {
            query(capPtr, capLen, statePtr) {
                self._refreshViews();
                self._writeU32(statePtr, 1); // GRANTED by default in browser
                return WAPI_OK;
            },
            request(capPtr, capLen, statePtr) {
                self._refreshViews();
                self._writeU32(statePtr, 1); // GRANTED
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_wake (wake lock)
        // -------------------------------------------------------------------
        const wapi_wake = {
            acquire(type, lockPtr) { return WAPI_ERR_NOTSUP; },
            release(lock) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_orient (screen orientation)
        // -------------------------------------------------------------------
        const wapi_orient = {
            get(orientPtr) {
                self._refreshViews();
                self._writeU32(orientPtr, 0); // PORTRAIT_PRIMARY
                return WAPI_OK;
            },
            lock(lockType) { return WAPI_ERR_NOTSUP; },
            unlock() { return WAPI_OK; },
        };

        // -------------------------------------------------------------------
        // wapi_codec (codec - stub)
        // -------------------------------------------------------------------
        const wapi_codec = {
            create_video(configPtr, codecPtr) { return WAPI_ERR_NOTSUP; },
            create_audio(configPtr, codecPtr) { return WAPI_ERR_NOTSUP; },
            destroy(codec) { return WAPI_ERR_NOTSUP; },
            is_supported(codecType, mode) { return WAPI_ERR_NOTSUP; },
            decode(codec, chunkPtr) { return WAPI_ERR_NOTSUP; },
            encode(codec, dataPtr, dataLen, timestampLo, timestampHi) { return WAPI_ERR_NOTSUP; },
            get_output(codec, bufPtr, bufLen, outLenPtr, tsPtr) { return WAPI_ERR_NOTSUP; },
            flush(codec) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_compress (compression - stub)
        // -------------------------------------------------------------------
        const wapi_compress = {
            create(algo, mode, level, streamPtr) { return WAPI_ERR_NOTSUP; },
            destroy(stream) { return WAPI_ERR_NOTSUP; },
            write(stream, dataPtr, len) { return WAPI_ERR_NOTSUP; },
            read(stream, bufPtr, bufLen, bytesReadPtr) { return WAPI_ERR_NOTSUP; },
            finish(stream) { return WAPI_ERR_NOTSUP; },
            oneshot(algo, mode, inPtr, inLen, outPtr, outLen, outWrittenPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_media (media session - stub)
        // -------------------------------------------------------------------
        const wapi_media = {
            set_metadata(metadataPtr) { return WAPI_ERR_NOTSUP; },
            set_playback_state(state) { return WAPI_ERR_NOTSUP; },
            set_position(position, duration) { return WAPI_ERR_NOTSUP; },
            set_actions(actionsPtr, count) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_mediacaps (media capabilities - stub)
        // -------------------------------------------------------------------
        const wapi_mediacaps = {
            query_decode(queryPtr, resultPtr) { return WAPI_ERR_NOTSUP; },
            query_encode(queryPtr, resultPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_encode (text encoding - stub)
        // -------------------------------------------------------------------
        const wapi_encode = {
            convert(from, to, inputPtr, inputLen, outputPtr, outputLen, bytesWrittenPtr) { return WAPI_ERR_NOTSUP; },
            query_size(from, to, inputPtr, inputLen, requiredLenPtr) { return WAPI_ERR_NOTSUP; },
            detect(dataPtr, len, encodingPtr, confidencePtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_authn (web authentication - stub)
        // -------------------------------------------------------------------
        const wapi_authn = {
            create_credential(rpIdPtr, rpIdLen, userPtr, challengePtr, challengeLen) { return WAPI_ERR_NOTSUP; },
            get_assertion(rpIdPtr, rpIdLen, challengePtr, challengeLen) { return WAPI_ERR_NOTSUP; },
            is_available() { return 0; },
        };

        // -------------------------------------------------------------------
        // wapi_netinfo (network info)
        // -------------------------------------------------------------------
        const wapi_netinfo = {
            get_info(infoPtr) {
                self._refreshViews();
                self._writeU32(infoPtr + 0, 0);   // UNKNOWN
                self._dv.setFloat32(infoPtr + 4, 10.0, true); // downlink_mbps
                self._writeU32(infoPtr + 8, 50);   // rtt_ms
                self._writeU32(infoPtr + 12, 0);   // save_data
                return WAPI_OK;
            },
            is_online() { return navigator.onLine ? 1 : 0; },
        };

        // -------------------------------------------------------------------
        // wapi_battery (battery status - stub)
        // -------------------------------------------------------------------
        const wapi_battery = {
            get_info(infoPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_idle (idle detection - stub)
        // -------------------------------------------------------------------
        const wapi_idle = {
            start(thresholdMs) { return WAPI_ERR_NOTSUP; },
            stop() { return WAPI_ERR_NOTSUP; },
            get_state(statePtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_haptic (haptics)
        // -------------------------------------------------------------------
        const wapi_haptic = {
            vibrate(patternPtr, patternLen) {
                if (navigator.vibrate) {
                    self._refreshViews();
                    const pattern = [];
                    for (let i = 0; i < patternLen; i++) {
                        pattern.push(self._dv.getUint32(patternPtr + i * 4, true));
                    }
                    navigator.vibrate(pattern);
                    return WAPI_OK;
                }
                return WAPI_ERR_NOTSUP;
            },
            vibrate_cancel() {
                if (navigator.vibrate) { navigator.vibrate(0); return WAPI_OK; }
                return WAPI_ERR_NOTSUP;
            },
            gamepad_vibrate(gamepadIndex, strongPtr, weakPtr, durationMs) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_p2p (peer-to-peer - stub)
        // -------------------------------------------------------------------
        const wapi_p2p = {
            create(configPtr, connPtr) { return WAPI_ERR_NOTSUP; },
            create_offer(conn, sdpBufPtr) { return WAPI_ERR_NOTSUP; },
            create_answer(conn, sdpBufPtr) { return WAPI_ERR_NOTSUP; },
            set_remote_desc(conn, sdpPtr, sdpLen) { return WAPI_ERR_NOTSUP; },
            add_ice_candidate(conn, candidatePtr, candidateLen) { return WAPI_ERR_NOTSUP; },
            send(conn, dataPtr, dataLen) { return WAPI_ERR_NOTSUP; },
            close(conn) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_hid (HID device access - stub)
        // -------------------------------------------------------------------
        const wapi_hid = {
            request_device(vendorId, productId, usagePage, devicePtr) { return WAPI_ERR_NOTSUP; },
            open(device) { return WAPI_ERR_NOTSUP; },
            close(device) { return WAPI_ERR_NOTSUP; },
            send_report(device, reportId, dataPtr, dataLen) { return WAPI_ERR_NOTSUP; },
            send_feature_report(device, reportId, dataPtr, dataLen) { return WAPI_ERR_NOTSUP; },
            receive_report(device, bufPtr, bufLen, bytesReadPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_serial (serial port - stub)
        // -------------------------------------------------------------------
        const wapi_serial = {
            request_port(portPtr) { return WAPI_ERR_NOTSUP; },
            open(port, configPtr) { return WAPI_ERR_NOTSUP; },
            close(port) { return WAPI_ERR_NOTSUP; },
            write(port, dataPtr, dataLen) { return WAPI_ERR_NOTSUP; },
            read(port, bufPtr, bufLen, bytesReadPtr) { return WAPI_ERR_NOTSUP; },
            set_signals(port, dtr, rts) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_capture (screen capture - stub)
        // -------------------------------------------------------------------
        const wapi_capture = {
            request(sourceType, capturePtr) { return WAPI_ERR_NOTSUP; },
            get_frame(capture, bufPtr, bufLen, frameInfoPtr) { return WAPI_ERR_NOTSUP; },
            stop(capture) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_contacts (contact picker - stub)
        // -------------------------------------------------------------------
        const wapi_contacts = {
            pick(propsMask, multiple, resultsBufPtr, resultsBufLen) { return WAPI_ERR_NOTSUP; },
            is_available() { return 0; },
        };

        // -------------------------------------------------------------------
        // wapi_barcode (barcode detection - stub)
        // -------------------------------------------------------------------
        const wapi_barcode = {
            detect(imageDataPtr, width, height, resultsBufPtr, maxResults) { return WAPI_ERR_NOTSUP; },
            detect_from_camera(cameraHandle, resultsBufPtr, maxResults) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_nfc (NFC - stub)
        // -------------------------------------------------------------------
        const wapi_nfc = {
            scan_start() { return WAPI_ERR_NOTSUP; },
            scan_stop() { return WAPI_ERR_NOTSUP; },
            write(recordsPtr, recordCount, tagPtr) { return WAPI_ERR_NOTSUP; },
            make_read_only(tag) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_dnd (drag and drop - stub)
        // -------------------------------------------------------------------
        const wapi_dnd = {
            start_drag(itemsPtr, itemCount, allowedEffects, iconSurface) { return WAPI_ERR_NOTSUP; },
            set_drop_effect(effect) { return WAPI_ERR_NOTSUP; },
            get_drop_data(index, bufPtr, bufLen, bytesWrittenPtr) { return WAPI_ERR_NOTSUP; },
        };

        return { wapi, wapi_env, wapi_memory, wapi_io, wapi_clock, wapi_fs, wapi_net,
                 wapi_gpu, wapi_surface, wapi_input, wapi_audio, wapi_content, wapi_text,
                 wapi_clipboard, wapi_kv, wapi_font, wapi_crypto, wapi_video, wapi_module,
                 wapi_notify, wapi_geo, wapi_sensor, wapi_speech, wapi_bio,
                 wapi_share, wapi_pay, wapi_usb, wapi_midi, wapi_bt, wapi_camera, wapi_xr,
                 wapi_register, wapi_taskbar, wapi_perm, wapi_wake, wapi_orient,
                 wapi_codec, wapi_compress, wapi_media, wapi_mediacaps, wapi_encode,
                 wapi_authn, wapi_netinfo, wapi_battery, wapi_idle, wapi_haptic,
                 wapi_p2p, wapi_hid, wapi_serial, wapi_capture, wapi_contacts,
                 wapi_barcode, wapi_nfc, wapi_dnd };
    }

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    _writeFileStat(ptr, node) {
        this._refreshViews();
        // wapi_filestat_t: 56 bytes
        this._dv.setBigUint64(ptr + 0, 0n, true);                // dev
        this._dv.setBigUint64(ptr + 8, BigInt(node.ino), true);  // ino
        this._dv.setUint32(ptr + 16, node.type, true);           // filetype
        this._dv.setUint32(ptr + 20, 0, true);                   // _pad0
        this._dv.setBigUint64(ptr + 24, 1n, true);               // nlink
        const sz = node.data ? node.data.length : 0;
        this._dv.setBigUint64(ptr + 32, BigInt(sz), true);       // size
        const atim = BigInt(node.mtime) * 1_000_000n;
        this._dv.setBigUint64(ptr + 40, atim, true);             // atim
        this._dv.setBigUint64(ptr + 48, atim, true);             // mtim
    }

    _pushSurfaceEvent(surfaceHandle, type, data1, data2) {
        this._eventQueue.push({
            type,
            surface_id: surfaceHandle,
            timestamp: performance.now() * 1_000_000,
            data1: data1 || 0,
            data2: data2 || 0,
        });
    }

    _writeEvent(ptr, ev) {
        this._refreshViews();
        // Zero out the 128-byte event union
        this._u8.fill(0, ptr, ptr + 128);

        // Common header: type(u32), surface_id(u32), timestamp(u64)
        this._dv.setUint32(ptr + 0, ev.type, true);
        this._dv.setUint32(ptr + 4, ev.surface_id || 0, true);
        this._dv.setBigUint64(ptr + 8, BigInt(Math.round(ev.timestamp || 0)), true);

        switch (ev.type) {
            case WAPI_EVENT_KEY_DOWN:
            case WAPI_EVENT_KEY_UP:
                this._dv.setUint32(ptr + 16, ev.scancode || 0, true);
                this._dv.setUint32(ptr + 20, ev.keycode || 0, true);
                this._dv.setUint16(ptr + 24, ev.mod || 0, true);
                this._u8[ptr + 26] = ev.down ? 1 : 0;
                this._u8[ptr + 27] = ev.repeat ? 1 : 0;
                break;

            case WAPI_EVENT_TEXT_INPUT:
                {
                    const encoded = new TextEncoder().encode(ev.text || "");
                    const maxLen = Math.min(encoded.length, 31);
                    this._u8.set(encoded.subarray(0, maxLen), ptr + 16);
                    this._u8[ptr + 16 + maxLen] = 0; // null terminate
                }
                break;

            case WAPI_EVENT_MOUSE_MOTION:
                this._dv.setUint32(ptr + 16, ev.mouse_id || 0, true);
                this._dv.setUint32(ptr + 20, ev.button_state || 0, true);
                this._dv.setFloat32(ptr + 24, ev.x || 0, true);
                this._dv.setFloat32(ptr + 28, ev.y || 0, true);
                this._dv.setFloat32(ptr + 32, ev.xrel || 0, true);
                this._dv.setFloat32(ptr + 36, ev.yrel || 0, true);
                break;

            case WAPI_EVENT_MOUSE_BUTTON_DOWN:
            case WAPI_EVENT_MOUSE_BUTTON_UP:
                this._dv.setUint32(ptr + 16, ev.mouse_id || 0, true);
                this._u8[ptr + 20] = ev.button || 1;
                this._u8[ptr + 21] = ev.down ? 1 : 0;
                this._u8[ptr + 22] = ev.clicks || 1;
                this._dv.setFloat32(ptr + 24, ev.x || 0, true);
                this._dv.setFloat32(ptr + 28, ev.y || 0, true);
                break;

            case WAPI_EVENT_MOUSE_WHEEL:
                this._dv.setUint32(ptr + 16, ev.mouse_id || 0, true);
                this._dv.setFloat32(ptr + 24, ev.x || 0, true);
                this._dv.setFloat32(ptr + 28, ev.y || 0, true);
                break;

            case WAPI_EVENT_TOUCH_DOWN:
            case WAPI_EVENT_TOUCH_UP:
            case WAPI_EVENT_TOUCH_MOTION:
                this._dv.setBigUint64(ptr + 16, BigInt(ev.touch_id || 0), true);
                this._dv.setBigUint64(ptr + 24, BigInt(ev.finger_id || 0), true);
                this._dv.setFloat32(ptr + 32, ev.x || 0, true);
                this._dv.setFloat32(ptr + 36, ev.y || 0, true);
                this._dv.setFloat32(ptr + 40, ev.dx || 0, true);
                this._dv.setFloat32(ptr + 44, ev.dy || 0, true);
                this._dv.setFloat32(ptr + 48, ev.pressure || 0, true);
                break;

            case WAPI_EVENT_GAMEPAD_AXIS:
                this._dv.setUint32(ptr + 16, ev.gamepad_id || 0, true);
                this._u8[ptr + 20] = ev.axis || 0;
                this._dv.setInt16(ptr + 24, ev.value || 0, true);
                break;

            case WAPI_EVENT_GAMEPAD_BUTTON_DOWN:
            case WAPI_EVENT_GAMEPAD_BUTTON_UP:
                this._dv.setUint32(ptr + 16, ev.gamepad_id || 0, true);
                this._u8[ptr + 20] = ev.button || 0;
                this._u8[ptr + 21] = ev.down ? 1 : 0;
                break;

            case WAPI_EVENT_SURFACE_RESIZED:
            case 0x0209: // MOVED
            case 0x020A: // DPI_CHANGED
                this._dv.setInt32(ptr + 16, ev.data1 || 0, true);
                this._dv.setInt32(ptr + 20, ev.data2 || 0, true);
                break;

            default:
                // Common/lifecycle events have no extra data
                break;
        }
    }

    _ensureContentCanvas() {
        if (!this._contentCanvas) {
            this._contentCanvas = document.createElement("canvas");
            this._contentCtx = this._contentCanvas.getContext("2d");
        }
    }

    // -----------------------------------------------------------------------
    // Input listener setup
    // -----------------------------------------------------------------------

    _setupInputListeners(canvas, surfaceHandle) {
        const self = this;
        const sid = surfaceHandle;

        // Keyboard
        window.addEventListener("keydown", (e) => {
            const sc = KEY_TO_SCANCODE[e.code] || 0;
            self._keyState.add(sc);
            self._modState = domModToTP(e);
            self._eventQueue.push({
                type: WAPI_EVENT_KEY_DOWN,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
                scancode: sc,
                keycode: e.keyCode,
                mod: self._modState,
                down: 1,
                repeat: e.repeat ? 1 : 0,
            });
            // Text input
            if (e.key.length === 1 && !e.ctrlKey && !e.metaKey) {
                self._eventQueue.push({
                    type: WAPI_EVENT_TEXT_INPUT,
                    surface_id: sid,
                    timestamp: performance.now() * 1_000_000,
                    text: e.key,
                });
            }
        });

        window.addEventListener("keyup", (e) => {
            const sc = KEY_TO_SCANCODE[e.code] || 0;
            self._keyState.delete(sc);
            self._modState = domModToTP(e);
            self._eventQueue.push({
                type: WAPI_EVENT_KEY_UP,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
                scancode: sc,
                keycode: e.keyCode,
                mod: self._modState,
                down: 0,
                repeat: 0,
            });
        });

        // Mouse
        canvas.addEventListener("mousemove", (e) => {
            self._mouseX = e.offsetX;
            self._mouseY = e.offsetY;
            self._eventQueue.push({
                type: WAPI_EVENT_MOUSE_MOTION,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
                mouse_id: 0,
                button_state: self._mouseButtons,
                x: e.offsetX,
                y: e.offsetY,
                xrel: e.movementX,
                yrel: e.movementY,
            });
        });

        canvas.addEventListener("mousedown", (e) => {
            const btn = domButtonToTP(e.button);
            self._mouseButtons |= (1 << btn);
            self._eventQueue.push({
                type: WAPI_EVENT_MOUSE_BUTTON_DOWN,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
                mouse_id: 0,
                button: btn,
                down: 1,
                clicks: e.detail,
                x: e.offsetX,
                y: e.offsetY,
            });
        });

        canvas.addEventListener("mouseup", (e) => {
            const btn = domButtonToTP(e.button);
            self._mouseButtons &= ~(1 << btn);
            self._eventQueue.push({
                type: WAPI_EVENT_MOUSE_BUTTON_UP,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
                mouse_id: 0,
                button: btn,
                down: 0,
                clicks: e.detail,
                x: e.offsetX,
                y: e.offsetY,
            });
        });

        canvas.addEventListener("wheel", (e) => {
            self._eventQueue.push({
                type: WAPI_EVENT_MOUSE_WHEEL,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
                mouse_id: 0,
                x: e.deltaX,
                y: e.deltaY,
            });
            e.preventDefault();
        }, { passive: false });

        // Context menu prevention
        canvas.addEventListener("contextmenu", (e) => e.preventDefault());

        // Touch
        canvas.addEventListener("touchstart", (e) => {
            for (const t of e.changedTouches) {
                const rect = canvas.getBoundingClientRect();
                self._eventQueue.push({
                    type: WAPI_EVENT_TOUCH_DOWN,
                    surface_id: sid,
                    timestamp: performance.now() * 1_000_000,
                    touch_id: 0,
                    finger_id: t.identifier,
                    x: (t.clientX - rect.left) / rect.width,
                    y: (t.clientY - rect.top) / rect.height,
                    dx: 0, dy: 0,
                    pressure: t.force || 1,
                });
            }
            e.preventDefault();
        }, { passive: false });

        canvas.addEventListener("touchmove", (e) => {
            for (const t of e.changedTouches) {
                const rect = canvas.getBoundingClientRect();
                self._eventQueue.push({
                    type: WAPI_EVENT_TOUCH_MOTION,
                    surface_id: sid,
                    timestamp: performance.now() * 1_000_000,
                    touch_id: 0,
                    finger_id: t.identifier,
                    x: (t.clientX - rect.left) / rect.width,
                    y: (t.clientY - rect.top) / rect.height,
                    dx: 0, dy: 0,
                    pressure: t.force || 1,
                });
            }
            e.preventDefault();
        }, { passive: false });

        canvas.addEventListener("touchend", (e) => {
            for (const t of e.changedTouches) {
                const rect = canvas.getBoundingClientRect();
                self._eventQueue.push({
                    type: WAPI_EVENT_TOUCH_UP,
                    surface_id: sid,
                    timestamp: performance.now() * 1_000_000,
                    touch_id: 0,
                    finger_id: t.identifier,
                    x: (t.clientX - rect.left) / rect.width,
                    y: (t.clientY - rect.top) / rect.height,
                    dx: 0, dy: 0,
                    pressure: 0,
                });
            }
        });

        // Focus
        window.addEventListener("focus", () => {
            self._pushSurfaceEvent(sid, WAPI_EVENT_SURFACE_FOCUS_GAINED, 0, 0);
        });
        window.addEventListener("blur", () => {
            self._pushSurfaceEvent(sid, WAPI_EVENT_SURFACE_FOCUS_LOST, 0, 0);
        });

        // Visibility / close
        window.addEventListener("beforeunload", () => {
            self._pushSurfaceEvent(sid, WAPI_EVENT_SURFACE_CLOSE, 0, 0);
            self._eventQueue.push({
                type: WAPI_EVENT_QUIT,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
            });
        });

        // Clipboard paste listener
        window.addEventListener("paste", (e) => {
            const text = e.clipboardData?.getData("text/plain") || "";
            if (text) self._clipboardText = text;
            const html = e.clipboardData?.getData("text/html") || "";
            if (html) self._clipboardHtml = html;
        });

        window.addEventListener("copy", () => {
            // Sync our cached clipboard to the system clipboard on copy
            if (navigator.clipboard && self._clipboardText) {
                navigator.clipboard.writeText(self._clipboardText).catch(() => {});
            }
        });
    }

    // -----------------------------------------------------------------------
    // Gamepad polling
    // -----------------------------------------------------------------------

    _pollGamepads() {
        if (!navigator.getGamepads) return;
        const gamepads = navigator.getGamepads();
        for (const gp of gamepads) {
            if (!gp) continue;
            const id = gp.index;
            const sid = this._activeSurfaceHandle;

            // Cache previous state
            if (!this._gpPrevState) this._gpPrevState = new Map();
            const prev = this._gpPrevState.get(id) || { buttons: [], axes: [] };

            // Buttons
            for (let i = 0; i < gp.buttons.length; i++) {
                const pressed = gp.buttons[i].pressed;
                const wasPressed = prev.buttons[i] || false;
                if (pressed && !wasPressed) {
                    this._eventQueue.push({
                        type: WAPI_EVENT_GAMEPAD_BUTTON_DOWN,
                        surface_id: sid,
                        timestamp: performance.now() * 1_000_000,
                        gamepad_id: id,
                        button: i,
                        down: 1,
                    });
                } else if (!pressed && wasPressed) {
                    this._eventQueue.push({
                        type: WAPI_EVENT_GAMEPAD_BUTTON_UP,
                        surface_id: sid,
                        timestamp: performance.now() * 1_000_000,
                        gamepad_id: id,
                        button: i,
                        down: 0,
                    });
                }
            }

            // Axes
            for (let i = 0; i < gp.axes.length; i++) {
                const val = Math.round(gp.axes[i] * 32767);
                const prevVal = prev.axes[i] || 0;
                if (val !== prevVal) {
                    this._eventQueue.push({
                        type: WAPI_EVENT_GAMEPAD_AXIS,
                        surface_id: sid,
                        timestamp: performance.now() * 1_000_000,
                        gamepad_id: id,
                        axis: i,
                        value: val,
                    });
                }
            }

            // Update previous state
            this._gpPrevState.set(id, {
                buttons: gp.buttons.map(b => b.pressed),
                axes: Array.from(gp.axes).map(a => Math.round(a * 32767)),
            });
        }
    }

    // -----------------------------------------------------------------------
    // WebGPU async initialization
    // -----------------------------------------------------------------------

    async _initGPU(config) {
        if (!navigator.gpu) return;
        try {
            this._gpuAdapter = await navigator.gpu.requestAdapter({
                powerPreference: config.gpuPowerPreference || "high-performance",
            });
            if (!this._gpuAdapter) return;
            this._gpuDevice = await this._gpuAdapter.requestDevice();
            this._gpuDevice.lost.then((info) => {
                console.error("[WAPI] GPU device lost:", info.message);
                this._gpuDevice = null;
            });
        } catch (e) {
            console.warn("[WAPI] WebGPU initialization failed:", e);
        }
    }

    // -----------------------------------------------------------------------
    // Pre-open filesystem directories from config
    // -----------------------------------------------------------------------

    _setupPreopens(preopens) {
        // preopens is a Map or object: { "/data": { "file.txt": Uint8Array } }
        if (!preopens) return;
        let handleIdx = 4; // WAPI_FS_PREOPEN_BASE

        for (const [path, files] of Object.entries(preopens)) {
            const dirNode = this.memfs.mkdirp(path);
            const fdEntry = new FDEntry(dirNode, path, 0);
            const h = handleIdx++;
            // Reserve this handle in the handle table at a specific ID
            this.handles._map.set(h, fdEntry);
            if (h >= this.handles._nextId) this.handles._nextId = h + 1;
            this._preopens.push({ path, handle: h });

            // Populate files
            if (files && typeof files === "object") {
                for (const [fname, content] of Object.entries(files)) {
                    const fullPath = path.endsWith("/") ? path + fname : path + "/" + fname;
                    if (content instanceof Uint8Array) {
                        this.memfs.createFile(fullPath, content);
                    } else if (typeof content === "string") {
                        this.memfs.createFile(fullPath, new TextEncoder().encode(content));
                    } else if (content && typeof content === "object") {
                        // Nested directory
                        this.memfs.mkdirp(fullPath);
                        // Recursively add files
                        const addRecursive = (basePath, obj) => {
                            for (const [k, v] of Object.entries(obj)) {
                                const fp = basePath + "/" + k;
                                if (v instanceof Uint8Array || typeof v === "string") {
                                    const data = typeof v === "string" ? new TextEncoder().encode(v) : v;
                                    this.memfs.createFile(fp, data);
                                } else if (v && typeof v === "object") {
                                    this.memfs.mkdirp(fp);
                                    addRecursive(fp, v);
                                }
                            }
                        };
                        addRecursive(fullPath, content);
                    }
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Load and instantiate a .wasm module
    // -----------------------------------------------------------------------

    /**
     * Load a Wasm module compiled against the WAPI ABI.
     *
     * @param {string|URL|ArrayBuffer|Response} wasmSource - URL to .wasm file,
     *   or an ArrayBuffer/Response containing the module bytes.
     * @param {Object} config - Configuration options.
     * @param {string[]} [config.args] - Command-line arguments.
     * @param {Object} [config.env] - Environment variables {KEY: "VALUE"}.
     * @param {Object} [config.preopens] - Pre-opened directories
     *   { "/path": { "file.txt": Uint8Array|string, ... } }.
     * @param {string} [config.gpuPowerPreference] - "low-power" or "high-performance".
     * @param {boolean} [config.runFrameLoop] - Auto-start frame loop (default true).
     * @returns {Promise<WebAssembly.Instance>}
     */
    async load(wasmSource, config = {}) {
        // Apply config
        if (config.args) this._args = config.args;
        if (config.env) this._env = config.env;

        // Parse URL params as additional args if in browser
        if (typeof URLSearchParams !== "undefined" && typeof location !== "undefined") {
            const params = new URLSearchParams(location.search);
            for (const [key, value] of params) {
                if (key === "arg") {
                    this._args.push(value);
                }
            }
        }

        // Setup filesystem
        this._setupPreopens(config.preopens);

        // Init GPU (async, must happen before instantiation)
        await this._initGPU(config);

        // Build import object
        const imports = this._buildImports();

        // Fetch and compile the wasm module
        let wasmModule, wasmInstance;

        if (wasmSource instanceof WebAssembly.Module) {
            wasmModule = wasmSource;
        } else if (wasmSource instanceof ArrayBuffer || ArrayBuffer.isView(wasmSource)) {
            wasmModule = await WebAssembly.compile(wasmSource);
        } else {
            // URL string or Response
            const response = wasmSource instanceof Response
                ? wasmSource
                : await fetch(wasmSource);

            if (WebAssembly.instantiateStreaming) {
                const result = await WebAssembly.instantiateStreaming(
                    response instanceof Response ? response : fetch(wasmSource),
                    imports
                );
                wasmModule = result.module;
                wasmInstance = result.instance;
            } else {
                const bytes = await (response instanceof Response
                    ? response.arrayBuffer()
                    : fetch(wasmSource).then(r => r.arrayBuffer()));
                wasmModule = await WebAssembly.compile(bytes);
            }
        }

        if (!wasmInstance) {
            wasmInstance = await WebAssembly.instantiate(wasmModule, imports);
        }

        this.module = wasmModule;
        this.instance = wasmInstance;

        // Get memory from exports
        this.memory = wasmInstance.exports.memory;
        if (!this.memory) {
            throw new Error("[WAPI] Wasm module must export 'memory'");
        }
        this._refreshViews();

        // Initialize host allocator
        this._allocInit();

        // Call _initialize if it exists (WASI-style reactors)
        if (wasmInstance.exports._initialize) {
            wasmInstance.exports._initialize();
        }

        // Call wapi_main if exported — pass wapi_context_t pointer
        if (wasmInstance.exports.wapi_main) {
            // Allocate wapi_io_t vtable (24 bytes) + wapi_context_t (20 bytes)
            // Vtable function pointers are 0 — modules use direct imports as fallback.
            const ioVtablePtr = this._hostAlloc(24, 4);
            this._u8.fill(0, ioVtablePtr, ioVtablePtr + 24);

            const ctxPtr = this._hostAlloc(20, 4);
            // Layout: { allocator*:4, io*:4, panic*:4, gpu_device:4, flags:4 }
            this._u8.fill(0, ctxPtr, ctxPtr + 20);
            // ctx->io = pointer to io vtable
            this._dv.setUint32(ctxPtr + 4, ioVtablePtr, true);
            // ctx->panic = 0 (NULL = runtime default)

            const result = wasmInstance.exports.wapi_main(ctxPtr);
            if (result < 0) {
                throw new Error(`[WAPI] wapi_main returned error: ${result}`);
            }
        }

        // Start frame loop
        const runLoop = config.runFrameLoop !== false;
        if (runLoop && wasmInstance.exports.wapi_frame) {
            this._startFrameLoop();
        }

        return wasmInstance;
    }

    // -----------------------------------------------------------------------
    // Frame loop
    // -----------------------------------------------------------------------

    _startFrameLoop() {
        this._running = true;
        const self = this;
        const wapi_frame = this.instance.exports.wapi_frame;

        function frame(domTimestamp) {
            if (!self._running) return;

            // Convert DOM timestamp to nanoseconds
            const ns = BigInt(Math.round(domTimestamp * 1_000_000));

            try {
                const result = wapi_frame(ns);
                if (result === WAPI_ERR_CANCELED) {
                    self._running = false;
                    console.log("[WAPI] Module requested exit via wapi_frame");
                    return;
                }
            } catch (e) {
                if (e.message && e.message.startsWith("wapi_exit")) {
                    return; // Clean exit
                }
                console.error("[WAPI] wapi_frame error:", e);
                self._running = false;
                return;
            }

            self._frameHandle = requestAnimationFrame(frame);
        }

        this._frameHandle = requestAnimationFrame(frame);
    }

    /**
     * Stop the frame loop.
     */
    stop() {
        this._running = false;
        if (this._frameHandle) {
            cancelAnimationFrame(this._frameHandle);
            this._frameHandle = 0;
        }
    }

    /**
     * Get the handle table for external use (e.g., passing GPU objects).
     */
    getHandleTable() {
        return this.handles;
    }

    /**
     * Get a reference to the in-memory filesystem.
     */
    getMemFS() {
        return this.memfs;
    }

    /**
     * Get the WebGPU device (if initialized).
     */
    getGPUDevice() {
        return this._gpuDevice;
    }

    /**
     * Write a file to the in-memory filesystem.
     */
    writeFile(path, data) {
        if (typeof data === "string") {
            data = new TextEncoder().encode(data);
        }
        this.memfs.createFile(path, data);
    }

    /**
     * Read a file from the in-memory filesystem.
     */
    readFile(path) {
        const node = this.memfs.stat(path);
        if (!node || node.type !== WAPI_FILETYPE_REGULAR) return null;
        return node.data;
    }
}

// ---------------------------------------------------------------------------
// Export for ES modules, CommonJS, or global
// ---------------------------------------------------------------------------

if (typeof module !== "undefined" && module.exports) {
    module.exports = { ThinPlatform };
} else if (typeof globalThis !== "undefined") {
    globalThis.ThinPlatform = ThinPlatform;
}

// ---------------------------------------------------------------------------
// HTML Loader Example (appended as a comment for reference, and also
// usable if this file is loaded as a <script>)
// ---------------------------------------------------------------------------

/*
<!-- wapi_loader.html - Minimal HTML loader for a WAPI Wasm module -->
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>WAPI</title>
    <style>
        * { margin: 0; padding: 0; }
        html, body { width: 100%; height: 100%; overflow: hidden; background: #000; }
    </style>
</head>
<body>
    <script src="wapi_shim.js"></script>
    <script>
        (async function() {
            const wapi = new ThinPlatform();

            // Get the wasm URL from ?wasm= query parameter, or use a default
            const params = new URLSearchParams(location.search);
            const wasmUrl = params.get("wasm") || "app.wasm";

            try {
                await wapi.load(wasmUrl, {
                    args: ["app", ...params.getAll("arg")],
                    env: {
                        WAPI_PLATFORM: "browser",
                        WAPI_USER_AGENT: navigator.userAgent,
                    },
                    preopens: {
                        "/": {},        // Root directory
                        "/tmp": {},     // Temp directory
                    },
                    gpuPowerPreference: "high-performance",
                    runFrameLoop: true,
                });
                console.log("[WAPI] Module loaded successfully");
            } catch (e) {
                if (e.message && e.message.startsWith("wapi_exit(0)")) {
                    console.log("[WAPI] Module exited cleanly");
                } else {
                    console.error("[WAPI] Failed to load module:", e);
                    document.body.innerHTML = `
                        <div style="color:#fff;padding:2em;font-family:monospace">
                            <h2>Failed to load WAPI module</h2>
                            <p>${e.message}</p>
                            <p>Make sure "${wasmUrl}" is served from this origin.</p>
                        </div>`;
                }
            }
        })();
    </script>
</body>
</html>
*/
