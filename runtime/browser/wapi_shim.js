/**
 * WAPI - Browser Shim
 * Maps WAPI ABI imports to browser Web APIs.
 *
 * Usage:
 *   const wapi = new WAPI();
 *   await wapi.load("module.wasm", { args: ["--flag"], preopens: { "/data": filesMap } });
 *
 * This file implements every import module defined by the WAPI ABI headers:
 *   wapi, wapi_env, wapi_memory, wapi_io, wapi_clock, wapi_fs,
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
const WAPI_STRLEN = 0xFFFFFFFF;          // 32-bit sentinel for _readString
const WAPI_STRLEN64 = 0xFFFFFFFFFFFFFFFFn; // 64-bit sentinel in wapi_string_view_t

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
const WAPI_IO_OP_HTTP_FETCH               = 0x060;
const WAPI_IO_OP_COMPRESS_PROCESS         = 0x140;
const WAPI_IO_OP_FONT_GET_BYTES           = 0x150;
const WAPI_IO_OP_NETWORK_LISTEN           = 0x040;
const WAPI_IO_OP_NETWORK_CHANNEL_OPEN     = 0x043;
const WAPI_IO_OP_NETWORK_CHANNEL_ACCEPT   = 0x044;
const WAPI_IO_OP_NETWORK_RESOLVE          = 0x045;

// Log levels (used with WAPI_IO_OP_LOG)
const WAPI_LOG_DEBUG = 0;
const WAPI_LOG_INFO  = 1;
const WAPI_LOG_WARN  = 2;
const WAPI_LOG_ERROR = 3;

// Network qualities — passed in op.flags for CONNECT / LISTEN / CHANNEL_OPEN.
// The platform picks any transport that satisfies the requested bits, or
// rejects the op with WAPI_ERR_NOTSUP. See wapi_network.h for the spec.
const WAPI_NET_RELIABLE        = 1 << 0;
const WAPI_NET_ORDERED         = 1 << 1;
const WAPI_NET_MESSAGE_FRAMED  = 1 << 2;
const WAPI_NET_ENCRYPTED       = 1 << 3;
const WAPI_NET_MULTIPLEXED     = 1 << 4;
const WAPI_NET_LOW_LATENCY     = 1 << 5;
const WAPI_NET_BROADCAST       = 1 << 6;

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
const WAPI_EVENT_POINTER_DOWN          = 0x900;
const WAPI_EVENT_POINTER_UP            = 0x901;
const WAPI_EVENT_POINTER_MOTION        = 0x902;
const WAPI_EVENT_POINTER_CANCEL        = 0x903;
const WAPI_EVENT_POINTER_ENTER         = 0x904;
const WAPI_EVENT_POINTER_LEAVE         = 0x905;
const WAPI_EVENT_IO_COMPLETION         = 0x2000;

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

// GPU texture formats (matching WGPUTextureFormat values from webgpu.h)
const WAPI_GPU_FORMAT_RGBA8_UNORM      = 0x0016;
const WAPI_GPU_FORMAT_RGBA8_UNORM_SRGB = 0x0017;
const WAPI_GPU_FORMAT_BGRA8_UNORM      = 0x001B;
const WAPI_GPU_FORMAT_BGRA8_UNORM_SRGB = 0x001C;

// Surface flags
const WAPI_SURFACE_FLAG_RESIZABLE  = 0x0001;
const WAPI_SURFACE_FLAG_HIGH_DPI   = 0x0010;

// ---------------------------------------------------------------------------
// WebGPU enum mapping tables (C integer <-> browser string)
// ---------------------------------------------------------------------------

const WGPU_TEXTURE_FORMAT_MAP = {
    0x00: undefined,       // Undefined
    0x01: "r8unorm", 0x02: "r8snorm", 0x03: "r8uint", 0x04: "r8sint",
    0x05: "r16unorm", 0x06: "r16snorm", 0x07: "r16uint", 0x08: "r16sint", 0x09: "r16float",
    0x0A: "rg8unorm", 0x0B: "rg8snorm", 0x0C: "rg8uint", 0x0D: "rg8sint",
    0x0E: "r32float", 0x0F: "r32uint", 0x10: "r32sint",
    0x11: "rg16unorm", 0x12: "rg16snorm", 0x13: "rg16uint", 0x14: "rg16sint", 0x15: "rg16float",
    0x16: "rgba8unorm", 0x17: "rgba8unorm-srgb", 0x18: "rgba8snorm", 0x19: "rgba8uint", 0x1A: "rgba8sint",
    0x1B: "bgra8unorm", 0x1C: "bgra8unorm-srgb",
    0x1D: "rgb10a2uint", 0x1E: "rgb10a2unorm", 0x1F: "rg11b10ufloat", 0x20: "rgb9e5ufloat",
    0x21: "rg32float", 0x22: "rg32uint", 0x23: "rg32sint",
    0x24: "rgba16unorm", 0x25: "rgba16snorm", 0x26: "rgba16uint", 0x27: "rgba16sint", 0x28: "rgba16float",
    0x29: "rgba32float", 0x2A: "rgba32uint", 0x2B: "rgba32sint",
    0x2C: "stencil8", 0x2D: "depth16unorm", 0x2E: "depth24plus", 0x2F: "depth24plus-stencil8",
    0x30: "depth32float", 0x31: "depth32float-stencil8",
    0x32: "bc1-rgba-unorm", 0x33: "bc1-rgba-unorm-srgb", 0x34: "bc2-rgba-unorm", 0x35: "bc2-rgba-unorm-srgb",
    0x36: "bc3-rgba-unorm", 0x37: "bc3-rgba-unorm-srgb", 0x38: "bc4-r-unorm", 0x39: "bc4-r-snorm",
    0x3A: "bc5-rg-unorm", 0x3B: "bc5-rg-snorm", 0x3C: "bc6h-rgb-ufloat", 0x3D: "bc6h-rgb-float",
    0x3E: "bc7-rgba-unorm", 0x3F: "bc7-rgba-unorm-srgb",
    0x40: "etc2-rgb8unorm", 0x41: "etc2-rgb8unorm-srgb", 0x42: "etc2-rgb8a1unorm", 0x43: "etc2-rgb8a1unorm-srgb",
    0x44: "etc2-rgba8unorm", 0x45: "etc2-rgba8unorm-srgb",
    0x46: "eac-r11unorm", 0x47: "eac-r11snorm", 0x48: "eac-rg11unorm", 0x49: "eac-rg11snorm",
    0x4A: "astc-4x4-unorm", 0x4B: "astc-4x4-unorm-srgb", 0x4C: "astc-5x4-unorm", 0x4D: "astc-5x4-unorm-srgb",
    0x4E: "astc-5x5-unorm", 0x4F: "astc-5x5-unorm-srgb", 0x50: "astc-6x5-unorm", 0x51: "astc-6x5-unorm-srgb",
    0x52: "astc-6x6-unorm", 0x53: "astc-6x6-unorm-srgb", 0x54: "astc-8x5-unorm", 0x55: "astc-8x5-unorm-srgb",
    0x56: "astc-8x6-unorm", 0x57: "astc-8x6-unorm-srgb", 0x58: "astc-8x8-unorm", 0x59: "astc-8x8-unorm-srgb",
    0x5A: "astc-10x5-unorm", 0x5B: "astc-10x5-unorm-srgb", 0x5C: "astc-10x6-unorm", 0x5D: "astc-10x6-unorm-srgb",
    0x5E: "astc-10x8-unorm", 0x5F: "astc-10x8-unorm-srgb", 0x60: "astc-10x10-unorm", 0x61: "astc-10x10-unorm-srgb",
    0x62: "astc-12x10-unorm", 0x63: "astc-12x10-unorm-srgb", 0x64: "astc-12x12-unorm", 0x65: "astc-12x12-unorm-srgb",
};

// Reverse map for browser format string -> C enum value
const WGPU_TEXTURE_FORMAT_REV = {};
for (const [k, v] of Object.entries(WGPU_TEXTURE_FORMAT_MAP)) {
    if (v) WGPU_TEXTURE_FORMAT_REV[v] = Number(k);
}

const WGPU_PRIMITIVE_TOPOLOGY = {
    0x01: "point-list", 0x02: "line-list", 0x03: "line-strip",
    0x04: "triangle-list", 0x05: "triangle-strip",
};

const WGPU_FRONT_FACE = { 0x01: "ccw", 0x02: "cw" };
const WGPU_CULL_MODE = { 0x01: "none", 0x02: "front", 0x03: "back" };

const WGPU_LOAD_OP = { 0x01: "load", 0x02: "clear" };
const WGPU_STORE_OP = { 0x01: "store", 0x02: "discard" };

const WGPU_BLEND_FACTOR = {
    0x01: "zero", 0x02: "one", 0x03: "src", 0x04: "one-minus-src",
    0x05: "src-alpha", 0x06: "one-minus-src-alpha",
    0x07: "dst", 0x08: "one-minus-dst",
    0x09: "dst-alpha", 0x0A: "one-minus-dst-alpha",
    0x0B: "src-alpha-saturated", 0x0C: "constant", 0x0D: "one-minus-constant",
    0x0E: "src1", 0x0F: "one-minus-src1",
    0x10: "src1-alpha", 0x11: "one-minus-src1-alpha",
};

const WGPU_BLEND_OP = {
    0x01: "add", 0x02: "subtract", 0x03: "reverse-subtract", 0x04: "min", 0x05: "max",
};

const WGPU_COMPARE_FUNCTION = {
    0x01: "never", 0x02: "less", 0x03: "equal", 0x04: "less-equal",
    0x05: "greater", 0x06: "not-equal", 0x07: "greater-equal", 0x08: "always",
};

const WGPU_STENCIL_OP = {
    0x01: "keep", 0x02: "zero", 0x03: "replace", 0x04: "invert",
    0x05: "increment-clamp", 0x06: "decrement-clamp",
    0x07: "increment-wrap", 0x08: "decrement-wrap",
};

const WGPU_INDEX_FORMAT = { 0x01: "uint16", 0x02: "uint32" };

const WGPU_VERTEX_FORMAT = {
    0x01: "uint8",    0x02: "uint8x2",   0x03: "uint8x4",
    0x04: "sint8",    0x05: "sint8x2",   0x06: "sint8x4",
    0x07: "unorm8",   0x08: "unorm8x2",  0x09: "unorm8x4",
    0x0A: "snorm8",   0x0B: "snorm8x2",  0x0C: "snorm8x4",
    0x0D: "uint16",   0x0E: "uint16x2",  0x0F: "uint16x4",
    0x10: "sint16",   0x11: "sint16x2",  0x12: "sint16x4",
    0x13: "unorm16",  0x14: "unorm16x2", 0x15: "unorm16x4",
    0x16: "snorm16",  0x17: "snorm16x2", 0x18: "snorm16x4",
    0x19: "float16",  0x1A: "float16x2", 0x1B: "float16x4",
    0x1C: "float32",  0x1D: "float32x2", 0x1E: "float32x3", 0x1F: "float32x4",
    0x20: "uint32",   0x21: "uint32x2",  0x22: "uint32x3",  0x23: "uint32x4",
    0x24: "sint32",   0x25: "sint32x2",  0x26: "sint32x3",  0x27: "sint32x4",
    0x28: "unorm10-10-10-2",
};

const WGPU_VERTEX_STEP_MODE = { 0x01: "vertex", 0x02: "instance" };

const WGPU_ADDRESS_MODE = { 0x01: "clamp-to-edge", 0x02: "repeat", 0x03: "mirror-repeat" };
const WGPU_FILTER_MODE = { 0x01: "nearest", 0x02: "linear" };
const WGPU_MIPMAP_FILTER_MODE = { 0x01: "nearest", 0x02: "linear" };

const WGPU_TEXTURE_DIMENSION = { 0x01: "1d", 0x02: "2d", 0x03: "3d" };
const WGPU_TEXTURE_VIEW_DIMENSION = {
    0x01: "1d", 0x02: "2d", 0x03: "2d-array", 0x04: "cube", 0x05: "cube-array", 0x06: "3d",
};
const WGPU_TEXTURE_ASPECT = { 0x01: "all", 0x02: "stencil-only", 0x03: "depth-only" };

const WGPU_STYPE_SHADER_SOURCE_WGSL = 0x02;

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
    "Minus": 45, "Equal": 46, "BracketLeft": 47, "BracketRight": 48, "Backslash": 49,
    "Semicolon": 51, "Quote": 52, "Backquote": 53, "Comma": 54, "Period": 55, "Slash": 56,
    "CapsLock": 57,
    // IntlBackslash is the key left of Z on ISO layouts (also present on
    // some Nordic keyboards). Map to the same Grave scancode so layouts
    // that place ½/§ there still toggle the debugger.
    "IntlBackslash": 53,
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

// Since WAPI format values now match WGPUTextureFormat, we can use the
// shared WGPU_TEXTURE_FORMAT_MAP for both wapi_gpu and wapi_wgpu.
function tpFormatToGPU(fmt) {
    return WGPU_TEXTURE_FORMAT_MAP[fmt] || "bgra8unorm";
}

function gpuFormatToTP(fmt) {
    return WGPU_TEXTURE_FORMAT_REV[fmt] || WAPI_GPU_FORMAT_BGRA8_UNORM;
}

// ---------------------------------------------------------------------------
// WAPI browser-extension bridge
// ---------------------------------------------------------------------------
// Round-trip messaging to the WAPI extension service worker, which holds
// the cross-origin content-addressed wasm module cache. The page shim runs
// in the MAIN world and has no chrome.runtime access, so it talks to the
// isolated-world bridge via window.postMessage, which forwards to sw.js.
//
// Protocol:
//   page → bridge:  { source: 'wapi',       type, reqId?, ...payload }
//   bridge → page:  { source: 'wapi-reply', reqId, result | error }
//
// Fire-and-forget: omit reqId. Round-trip: supply unique reqId and wait
// for the matching reply. A 5s timeout treats silence as "no extension
// installed" and resolves null so the shim falls back to the network.

const _wapiExtReqs = new Map();
let _wapiExtNextReqId = 1;
let _wapiExtListenerAttached = false;

function _wapiExtEnsureListener() {
    if (_wapiExtListenerAttached || typeof window === "undefined") return;
    _wapiExtListenerAttached = true;
    window.addEventListener("message", (ev) => {
        if (ev.source !== window) return;
        const m = ev.data;
        if (!m || m.source !== "wapi-reply") return;
        const pending = _wapiExtReqs.get(m.reqId);
        if (!pending) return;
        _wapiExtReqs.delete(m.reqId);
        clearTimeout(pending.timer);
        if (m.error) pending.reject(new Error(m.error));
        else pending.resolve(m.result);
    });
}

// Round-trip to the extension. Resolves with the SW's result (may be null
// on a cache miss) or null if no extension answers within timeoutMs.
function _wapiExtSend(type, payload, timeoutMs = 5000) {
    if (typeof window === "undefined") return Promise.resolve(null);
    _wapiExtEnsureListener();
    const reqId = _wapiExtNextReqId++;
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => {
            _wapiExtReqs.delete(reqId);
            resolve(null);
        }, timeoutMs);
        _wapiExtReqs.set(reqId, { resolve, reject, timer });
        window.postMessage({ source: "wapi", type, reqId, ...payload }, "*");
    });
}

// Fire-and-forget: used by modules.store where the shim doesn't care
// whether the extension is present or when the write lands.
function _wapiExtPost(type, payload) {
    if (typeof window === "undefined") return;
    window.postMessage({ source: "wapi", type, ...payload }, "*");
}

// Base64 helpers — chrome.runtime.sendMessage uses JSON, so bytes cross
// the extension boundary as base64. window.postMessage supports structured
// clone, but the bridge re-serializes on its way to sw.js, so we match
// the SW's encoding here.
function _wapiBytesToB64(u8) {
    let s = "";
    const CHUNK = 0x8000;
    for (let i = 0; i < u8.length; i += CHUNK) {
        s += String.fromCharCode.apply(null, u8.subarray(i, i + CHUNK));
    }
    return btoa(s);
}

function _wapiB64ToBytes(b64) {
    const bin = atob(b64);
    const u8 = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) u8[i] = bin.charCodeAt(i);
    return u8;
}

function _wapiHexHash(u8) {
    let s = "";
    for (let i = 0; i < u8.length; i++) s += u8[i].toString(16).padStart(2, "0");
    return s;
}

// Content-addressed wasm-module cache driven by wapi_module.prefetch.
// Shared across all WAPI instances on the page — WebAssembly.Module
// objects are position-independent and safe to hand to instantiate()
// from any instance. Entries:
//   pending  — { state: "pending",  promise: Promise<void> }
//   ready    — { state: "ready",    module: WebAssembly.Module, hash: string }
//   failed   — { state: "failed",   error: string }
// Keyed by lowercase hex hash.
const _wapiModuleCache = new Map();

// Page-local state for wapi_module.join (service mode). Services are
// hosted inside the extension service worker; join is fundamentally an
// async round-trip (postMessage → bridge → SW) but wasm imports are sync.
// We resolve this with a poll loop: the first call kicks off the SW
// services.join request and returns WAPI_ERR_AGAIN; subsequent calls
// return the same AGAIN until the SW replies, at which point the entry
// flips to "ready" and join writes the handle and returns WAPI_OK.
//
// Keyed by "<hashHex>:<name>". Entries:
//   pending — { state: "pending",  promise: Promise<void> }
//   ready   — { state: "ready",    handle: i32 }  (handle is SW-side service handle)
//   failed  — { state: "failed",   error: string }
const _wapiServiceJoins = new Map();

// Fetch bytes by hash through the extension cache, then network as a
// fallback. Verifies the SHA-256 matches the requested hash before
// compiling. Used by wapi_module.prefetch. Returns a WebAssembly.Module.
async function _wapiFetchAndCompile(hashHex, url) {
    let bytes = null;
    let gotFromCache = false;

    const resp = await _wapiExtSend("modules.fetch", { hash: hashHex });
    if (resp && resp.bytesB64) {
        bytes = _wapiB64ToBytes(resp.bytesB64);
        gotFromCache = true;
    }

    if (!bytes) {
        if (!url) throw new Error("module not cached and no url provided");
        const response = await fetch(url);
        if (!response.ok) {
            throw new Error(`fetch ${url} failed: ${response.status}`);
        }
        const ab = await response.arrayBuffer();
        bytes = new Uint8Array(ab);
    }

    if (typeof crypto !== "undefined" && crypto.subtle) {
        const digest = await crypto.subtle.digest("SHA-256", bytes);
        const got = _wapiHexHash(new Uint8Array(digest));
        if (got !== hashHex) {
            throw new Error(`hash mismatch: expected ${hashHex}, got ${got}`);
        }
    }

    if (!gotFromCache) {
        _wapiExtPost("modules.store", {
            hash: hashHex,
            url: url || "",
            bytesB64: _wapiBytesToB64(bytes),
        });
    }

    return WebAssembly.compile(bytes);
}

// Read a 32-byte wapi_module_hash_t at `ptr` from the given Uint8Array
// view and return the lowercase hex string. Returns null if ptr is 0.
function _wapiReadModuleHash(u8, ptr) {
    if (ptr === 0) return null;
    return _wapiHexHash(u8.subarray(ptr, ptr + 32));
}

// ---------------------------------------------------------------------------
// WAPI - the main shim class
// ---------------------------------------------------------------------------

class WAPI {
    // Sentinel thrown by proc_exit to unwind the wasm call stack.
    // Caught around _start so we can distinguish "clean exit" from real errors.
    _ProcExit = class ProcExit {
        constructor(code) { this.code = code; }
    };

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
        // Per-frame GPU draw stats (reset externally each frame)
        this._drawStats = { draws: 0, totalVerts: 0 };
        // When non-null, individual draw/scissor calls get recorded to this array (for one-shot tracing)
        this._drawTrace = null;
        this._keyState = new Set();  // set of currently-pressed scancodes
        this._modState = 0;
        this._mouseX = 0;
        this._mouseY = 0;
        this._mouseButtons = 0;

        // Pointer state (unified)
        this._pointerX = 0;
        this._pointerY = 0;
        this._pointerButtons = 0;

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

        // Vtable pointers (lazily built after instantiation)
        this._ioVtablePtr = 0;
        this._allocVtablePtr = 0;
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

    /**
     * Read a wapi_string_view_t struct (16 bytes) from linear memory.
     *   Offset 0: uint64_t data   (linear memory address)
     *   Offset 8: uint64_t length (byte count, or WAPI_STRLEN64 for null-terminated)
     * Returns a JS string, or null if the view is absent ({NULL, WAPI_STRLEN64}).
     */
    _readStringView(svPtr) {
        this._refreshViews();
        const dataPtr = Number(this._dv.getBigUint64(svPtr, true));
        const lenBig  = this._dv.getBigUint64(svPtr + 8, true);
        if (dataPtr === 0 && lenBig === WAPI_STRLEN64) return null; // absent
        if (lenBig === WAPI_STRLEN64) {
            return this._readString(dataPtr, WAPI_STRLEN); // null-terminated
        }
        return this._readString(dataPtr, Number(lenBig));
    }

    /**
     * Read a WGPUStringView struct (8 bytes, wasm32 C ABI) from linear memory.
     *   Offset 0: const char* data  (4 bytes, wasm32 pointer)
     *   Offset 4: size_t length     (4 bytes, wasm32 size_t)
     * Returns a JS string, or "" if pointer is NULL.
     * NOTE: This is DIFFERENT from _readStringView which reads wapi_string_view_t (16 bytes).
     */
    _readWGPUStringView(ptr) {
        this._refreshViews();
        const dataPtr = this._dv.getUint32(ptr, true);
        const len = this._dv.getUint32(ptr + 4, true);
        if (dataPtr === 0) return "";
        if (len === 0xFFFFFFFF) { // SIZE_MAX sentinel
            return this._readString(dataPtr, WAPI_STRLEN);
        }
        return this._readString(dataPtr, len);
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
    // Typed wasm function reference helper (portable, no WebAssembly.Function)
    // -----------------------------------------------------------------------

    /**
     * Create a wasm function reference with the correct type signature.
     * Builds a tiny wasm module that imports a JS function and re-exports
     * it as a typed wasm function, suitable for table.set().
     *
     * @param {string[]} params - Param types: 'i32', 'i64', 'f32', 'f64'
     * @param {string[]} results - Result types (same options)
     * @param {Function} impl - JS implementation function
     * @returns {Function} Typed wasm function reference
     */
    _makeWasmFunc(params, results, impl) {
        const T = { 'i32': 0x7f, 'i64': 0x7e, 'f32': 0x7d, 'f64': 0x7c };
        const bytes = [];

        // Header
        bytes.push(0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00);

        // Type section (id=1): one func type
        const type = [0x01, 0x60,
            params.length, ...params.map(p => T[p]),
            results.length, ...results.map(r => T[r])];
        bytes.push(0x01, type.length, ...type);

        // Import section (id=2): import "env"."f" as func type 0
        const imp = [0x01, 0x03, 0x65, 0x6e, 0x76, 0x01, 0x66, 0x00, 0x00];
        bytes.push(0x02, imp.length, ...imp);

        // Function section (id=3): one function, type 0
        bytes.push(0x03, 0x02, 0x01, 0x00);

        // Export section (id=7): export func index 1 as "w"
        bytes.push(0x07, 0x05, 0x01, 0x01, 0x77, 0x00, 0x01);

        // Code section (id=10): wrapper body that forwards all params to import
        const body = [0x00]; // 0 local decls
        for (let i = 0; i < params.length; i++) body.push(0x20, i); // local.get i
        body.push(0x10, 0x00, 0x0b); // call 0, end
        const code = [0x01, body.length, ...body];
        bytes.push(0x0a, code.length, ...code);

        const mod = new WebAssembly.Module(new Uint8Array(bytes));
        const inst = new WebAssembly.Instance(mod, { env: { f: impl } });
        return inst.exports.w;
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
        // Surface, window, display, and input always available in browser
        caps.push("wapi.surface", "wapi.window", "wapi.display", "wapi.input");

        if (typeof AudioContext !== "undefined" || typeof webkitAudioContext !== "undefined") {
            caps.push("wapi.audio");
        }
        // Content via Canvas2D
        caps.push("wapi.content");

        if (typeof navigator !== "undefined" && navigator.clipboard) {
            caps.push("wapi.clipboard");
        }
        if (typeof WebSocket !== "undefined" || typeof fetch !== "undefined") {
            caps.push("wapi.network");
        }
        if (typeof fetch !== "undefined") {
            caps.push("wapi.http");
        }
        if (typeof DecompressionStream !== "undefined" || typeof CompressionStream !== "undefined") {
            caps.push("wapi.compression");
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

        // Power management (battery, wake lock, idle, saver, thermal)
        caps.push("wapi.power");

        // Haptics / vibration
        if (typeof navigator !== "undefined" && navigator.vibrate) {
            caps.push("wapi.haptics");
        }

        // Screen orientation
        if (typeof screen !== "undefined" && screen.orientation) {
            caps.push("wapi.orientation");
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

        // Screen capture (getDisplayMedia)
        if (typeof navigator !== "undefined" && navigator.mediaDevices &&
            navigator.mediaDevices.getDisplayMedia) {
            caps.push("wapi.screen_capture");
        }

        // Media session
        if (typeof navigator !== "undefined" && navigator.mediaSession) {
            caps.push("wapi.media_session");
        }

        // DnD always available in browsers
        caps.push("wapi.dnd");

        // Browser can provide sysinfo and dialog (alert/confirm/prompt)
        caps.push("wapi.sysinfo", "wapi.dialog");

        // Module loading (dynamic import of child wasm modules)
        caps.push("wapi.module");

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
            // wapi_string_view_t passed by value → pointer to 16-byte struct on wasm32
            capability_supported(svPtr) {
                const name = self._readStringView(svPtr);
                if (!name) return 0;
                return supportedCaps.includes(name) ? 1 : 0;
            },

            capability_version(svPtr, versionPtr) {
                const name = self._readStringView(svPtr);
                if (!name || !supportedCaps.includes(name)) {
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
                const written = self._writeString(bufPtr, Number(bufLen), name);
                self._writeU64(nameLenPtr, written);
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

            panic_report(msgPtr, msgLen) {
                const msg = self._readString(msgPtr, Number(msgLen));
                console.error(`[WAPI PANIC] ${msg}`);
            },

            io_get() {
                if (self._ioVtablePtr) return self._ioVtablePtr;

                const table = self.instance.exports.__indirect_function_table;
                if (!table) {
                    console.error("[WAPI] Module must export __indirect_function_table for vtable support");
                    return 0;
                }

                const mf = (p, r, fn) => self._makeWasmFunc(p, r, fn);

                // Grow table to fit 8 new entries
                const baseIdx = table.length;
                table.grow(8);

                // submit: (impl, ops, count) -> i32
                table.set(baseIdx + 0, mf(['i32','i32','i64'], ['i32'],
                    (_impl, opsPtr, count) => wapi_io.submit(opsPtr, Number(count))));
                // cancel: (impl, user_data) -> i32
                table.set(baseIdx + 1, mf(['i32','i64'], ['i32'],
                    (_impl, userData) => wapi_io.cancel(userData)));
                // poll: (impl, event) -> i32
                table.set(baseIdx + 2, mf(['i32','i32'], ['i32'],
                    (_impl, eventPtr) => wapi_io.poll(eventPtr)));
                // wait: (impl, event, timeout_ms) -> i32
                table.set(baseIdx + 3, mf(['i32','i32','i32'], ['i32'],
                    (_impl, eventPtr, timeoutMs) => wapi_io.wait(eventPtr, timeoutMs)));
                // flush: (impl, event_type) -> void
                table.set(baseIdx + 4, mf(['i32','i32'], [],
                    (_impl, eventType) => wapi_io.flush(eventType)));
                // capability_supported: (impl, name_sv_ptr) -> i32
                table.set(baseIdx + 5, mf(['i32','i32'], ['i32'],
                    (_impl, svPtr) => {
                        const name = self._readStringView(svPtr);
                        if (!name) return 0;
                        return supportedCaps.includes(name) ? 1 : 0;
                    }));
                // capability_version: (impl, name_sv_ptr, version_ptr) -> i32
                table.set(baseIdx + 6, mf(['i32','i32','i32'], ['i32'],
                    (_impl, svPtr, versionPtr) => wapi.capability_version(svPtr, versionPtr)));
                // perm_query: (impl, cap_sv_ptr, state_ptr) -> i32
                table.set(baseIdx + 7, mf(['i32','i32','i32'], ['i32'],
                    (_impl, _capSvPtr, statePtr) => {
                        self._writeU32(statePtr, 0);
                        return WAPI_ERR_NOTSUP;
                    }));

                // Allocate 36 bytes in linear memory for wapi_io_t
                // Layout: impl(4) submit(4) cancel(4) poll(4) wait(4)
                //         flush(4) capability_supported(4) capability_version(4) perm_query(4)
                const ptr = self._hostAlloc(36, 4);
                if (!ptr) return 0;
                self._refreshViews();
                self._dv.setUint32(ptr +  0, 0,            true); // impl (unused)
                self._dv.setUint32(ptr +  4, baseIdx + 0,  true); // submit
                self._dv.setUint32(ptr +  8, baseIdx + 1,  true); // cancel
                self._dv.setUint32(ptr + 12, baseIdx + 2,  true); // poll
                self._dv.setUint32(ptr + 16, baseIdx + 3,  true); // wait
                self._dv.setUint32(ptr + 20, baseIdx + 4,  true); // flush
                self._dv.setUint32(ptr + 24, baseIdx + 5,  true); // capability_supported
                self._dv.setUint32(ptr + 28, baseIdx + 6,  true); // capability_version
                self._dv.setUint32(ptr + 32, baseIdx + 7,  true); // perm_query

                self._ioVtablePtr = ptr;
                return ptr;
            },

            allocator_get() {
                if (self._allocVtablePtr) return self._allocVtablePtr;

                const table = self.instance.exports.__indirect_function_table;
                if (!table) {
                    console.error("[WAPI] Module must export __indirect_function_table for vtable support");
                    return 0;
                }

                const mf = (p, r, fn) => self._makeWasmFunc(p, r, fn);

                // Grow table to fit 3 new entries
                const baseIdx = table.length;
                table.grow(3);

                // alloc_fn: (impl, size, align) -> i32
                table.set(baseIdx + 0, mf(['i32','i64','i64'], ['i32'],
                    (_impl, size, align) => wapi_memory.alloc(Number(size), Number(align))));
                // free_fn: (impl, ptr) -> void
                table.set(baseIdx + 1, mf(['i32','i32'], [],
                    (_impl, ptr) => wapi_memory.free(ptr)));
                // realloc_fn: (impl, ptr, new_size, align) -> i32
                table.set(baseIdx + 2, mf(['i32','i32','i64','i64'], ['i32'],
                    (_impl, ptr, newSize, align) => wapi_memory.realloc(ptr, Number(newSize), Number(align))));

                // Allocate 16 bytes in linear memory for wapi_allocator_t
                // Layout: impl(4) alloc_fn(4) free_fn(4) realloc_fn(4)
                const ptr = self._hostAlloc(16, 4);
                if (!ptr) return 0;
                self._refreshViews();
                self._dv.setUint32(ptr +  0, 0,            true); // impl (unused)
                self._dv.setUint32(ptr +  4, baseIdx + 0,  true); // alloc_fn
                self._dv.setUint32(ptr +  8, baseIdx + 1,  true); // free_fn
                self._dv.setUint32(ptr + 12, baseIdx + 2,  true); // realloc_fn

                self._allocVtablePtr = ptr;
                return ptr;
            },

            // Direct page-level allocator for .NET VirtualAlloc.
            // Bypasses the C runtime malloc/calloc (which may be broken on
            // this WASI SDK version) and uses WebAssembly.Memory.grow().
            // Returns a pointer to zeroed memory (WASM pages are always zeroed).
            wasm_alloc(size) {
                if (size <= 0) return 0;
                const pageSize = 65536;
                const pagesNeeded = Math.ceil(size / pageSize);
                const mem = self.memory;
                const oldPages = mem.buffer.byteLength / pageSize;
                const result = mem.grow(pagesNeeded);
                if (result === -1) {
                    console.error(`[WAPI] wasm_alloc: memory.grow(${pagesNeeded}) failed`);
                    return 0;
                }
                self._refreshViews();
                const addr = oldPages * pageSize;
                console.log(`[WAPI] wasm_alloc(${size}) => 0x${addr.toString(16)} (${pagesNeeded} pages, total ${oldPages + pagesNeeded} pages = ${((oldPages + pagesNeeded) * pageSize / 1048576).toFixed(1)}MB)`);
                return addr;
            },

            wasm_free(_ptr, _size) {
                // WASM memory cannot be shrunk — no-op
            },
        };

        // -------------------------------------------------------------------
        // wapi_env (environment)
        // -------------------------------------------------------------------
        const wapi_env = {
            args_count() {
                return self._args.length;
            },

            // (i32 index, i32 buf, i64 buf_len, i32 arg_len_ptr) -> i32
            args_get(index, bufPtr, bufLen, argLenPtr) {
                if (index < 0 || index >= self._args.length) return WAPI_ERR_OVERFLOW;
                const arg = self._args[index];
                const written = self._writeString(bufPtr, Number(bufLen), arg);
                self._writeU64(argLenPtr, written);
                return WAPI_OK;
            },

            environ_count() {
                return Object.keys(self._env).length;
            },

            // (i32 index, i32 buf, i64 buf_len, i32 var_len_ptr) -> i32
            environ_get(index, bufPtr, bufLen, varLenPtr) {
                const keys = Object.keys(self._env);
                if (index < 0 || index >= keys.length) return WAPI_ERR_OVERFLOW;
                const entry = keys[index] + "=" + self._env[keys[index]];
                const written = self._writeString(bufPtr, Number(bufLen), entry);
                self._writeU64(varLenPtr, written);
                return WAPI_OK;
            },

            // (i32 sv_ptr, i32 buf, i64 buf_len, i32 val_len_ptr) -> i32
            // name is wapi_string_view_t passed indirectly (pointer)
            getenv(svPtr, bufPtr, bufLen, valLenPtr) {
                const name = self._readStringView(svPtr);
                if (!name || !(name in self._env)) return WAPI_ERR_NOENT;
                const val = self._env[name];
                const written = self._writeString(bufPtr, Number(bufLen), val);
                self._writeU64(valLenPtr, written);
                return WAPI_OK;
            },

            // (i32 buf, i64 len) -> i32
            random_get(bufPtr, len) {
                self._refreshViews();
                const n = Number(len);
                const sub = self._u8.subarray(bufPtr, bufPtr + n);
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
                throw new Error(`wapi_exit(${code})`);
            },

            // (i32 sv_ptr) -> i32
            open_url(svPtr) {
                const url = self._readStringView(svPtr);
                if (!url) return WAPI_ERR_INVAL;
                try { window.open(url, "_blank"); return WAPI_OK; }
                catch (_) { return WAPI_ERR_NOTSUP; }
            },

            // (i32 key_sv_ptr, i32 buf, i64 buf_len, i32 val_len_ptr) -> i32
            host_get(keySvPtr, bufPtr, bufLen, valLenPtr) {
                const key = self._readStringView(keySvPtr);
                const hostInfo = {
                    "os.family":       "browser",
                    "runtime.name":    "wapi-browser",
                    "runtime.version": "0.1.0",
                    "device.form":     "desktop",
                    "browser.engine":  navigator.userAgent.includes("Chrome") ? "chromium"
                                     : navigator.userAgent.includes("Firefox") ? "gecko"
                                     : navigator.userAgent.includes("Safari") ? "webkit"
                                     : "unknown",
                    "locale":          navigator.language || "en-US",
                };
                if (!key || !(key in hostInfo)) return WAPI_ERR_NOENT;
                const val = hostInfo[key];
                const written = self._writeString(bufPtr, Number(bufLen), val);
                self._writeU64(valLenPtr, written);
                return WAPI_OK;
            },

            // (i32 buf, i64 buf_len, i32 msg_len_ptr) -> i32
            get_error(bufPtr, bufLen, msgLenPtr) {
                const msg = self._lastError;
                const written = self._writeString(bufPtr, Number(bufLen), msg);
                self._writeU64(msgLenPtr, written);
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
        // Implementation functions for the wapi_io_t vtable.
        // All events (I/O completions, input, lifecycle) come through poll/wait.
        // Accessed by modules via: const wapi_io_t* io = wapi_io_get();
        // -------------------------------------------------------------------
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
            submit(opsPtr, count) {
                self._refreshViews();
                let submitted = 0;
                for (let i = 0; i < count; i++) {
                    // wapi_io_op_t: 80 bytes, 8-byte aligned
                    //   0:opcode u32  4:flags u32  8:fd i32  12:flags2 u32
                    //  16:offset u64 24:addr u64  32:len u64
                    //  40:addr2 u64  48:len2 u64  56:user_data u64
                    //  64:result_ptr u64  72:reserved u64
                    const base = opsPtr + i * 80;
                    const opcode    = self._dv.getUint32(base + 0, true);
                    const flags     = self._dv.getUint32(base + 4, true);
                    const fd        = self._dv.getInt32(base + 8, true);
                    const flags2    = self._dv.getUint32(base + 12, true);
                    const offset    = self._dv.getBigUint64(base + 16, true);
                    const addr      = Number(self._dv.getBigUint64(base + 24, true));
                    const len       = Number(self._dv.getBigUint64(base + 32, true));
                    const addr2     = Number(self._dv.getBigUint64(base + 40, true));
                    const len2      = Number(self._dv.getBigUint64(base + 48, true));
                    const userData  = self._dv.getBigUint64(base + 56, true);
                    const resultPtr = Number(self._dv.getBigUint64(base + 64, true));

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
                            // flags = WAPI_NET_* qualities. The shim picks any
                            // browser-available transport that satisfies them,
                            // or completes with WAPI_ERR_NOTSUP. Capability gate
                            // is enforced first so a host that didn't grant
                            // wapi_network can never open a socket.
                            if (!supportedCaps.includes("wapi.network")) {
                                _pushIoCompletion(userData, WAPI_ERR_NOTSUP, 0);
                                break;
                            }
                            const address = self._readString(addr, len);
                            const qualities = flags;
                            // Browsers can satisfy reliable+ordered+framed+encrypted
                            // via WebSocket(wss). Anything else (raw datagrams,
                            // listen, broadcast, multiplexed streams, plaintext)
                            // is not reachable from this host.
                            const wsRequired =
                                WAPI_NET_RELIABLE | WAPI_NET_ORDERED |
                                WAPI_NET_MESSAGE_FRAMED | WAPI_NET_ENCRYPTED;
                            const wsForbidden =
                                WAPI_NET_BROADCAST | WAPI_NET_MULTIPLEXED;
                            const wsCompatible =
                                ((qualities & wsRequired) === wsRequired) &&
                                ((qualities & wsForbidden) === 0);
                            if (!wsCompatible) {
                                _pushIoCompletion(userData, WAPI_ERR_NOTSUP, 0);
                                break;
                            }
                            try {
                                const ws = new WebSocket(address);
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
                            break;
                        }

                        case WAPI_IO_OP_NETWORK_LISTEN:
                        case WAPI_IO_OP_ACCEPT:
                        case WAPI_IO_OP_NETWORK_CHANNEL_OPEN:
                        case WAPI_IO_OP_NETWORK_CHANNEL_ACCEPT:
                        case WAPI_IO_OP_NETWORK_RESOLVE:
                            // Browsers can't listen for inbound connections,
                            // can't open multiplexed channels (no WebTransport
                            // in the shim yet), and have no DNS API. The host
                            // capability check still runs first so non-network
                            // sandboxes return the same NOTSUP they would for
                            // CONNECT instead of leaking an "unsupported here"
                            // signal that callers might confuse with denial.
                            if (!supportedCaps.includes("wapi.network")) {
                                _pushIoCompletion(userData, WAPI_ERR_NOTSUP, 0);
                                break;
                            }
                            _pushIoCompletion(userData, WAPI_ERR_NOTSUP, 0);
                            break;

                        case WAPI_IO_OP_SEND: {
                            const obj = self.handles.get(fd);
                            if (!obj) { _pushIoCompletion(userData, WAPI_ERR_BADF, 0); break; }
                            if (obj.type === "websocket" && obj.ws) {
                                if (obj.ws.readyState !== WebSocket.OPEN) {
                                    _pushIoCompletion(userData, WAPI_ERR_PIPE, 0);
                                    break;
                                }
                                self._refreshViews();
                                obj.ws.send(self._u8.slice(addr, addr + len));
                                if (resultPtr) self._writeU32(resultPtr, len);
                                _pushIoCompletion(userData, len, 0);
                                break;
                            }
                            _pushIoCompletion(userData, WAPI_ERR_BADF, 0);
                            break;
                        }

                        case WAPI_IO_OP_RECV: {
                            const obj = self.handles.get(fd);
                            if (!obj) { _pushIoCompletion(userData, WAPI_ERR_BADF, 0); break; }
                            if (obj.type === "websocket" && obj.ws) {
                                if (obj.ws._recvQueue.length === 0) {
                                    _pushIoCompletion(userData, WAPI_ERR_AGAIN, 0);
                                    break;
                                }
                                const chunk = obj.ws._recvQueue.shift();
                                const copyLen = Math.min(chunk.length, len);
                                self._refreshViews();
                                self._u8.set(chunk.subarray(0, copyLen), addr);
                                if (copyLen < chunk.length) {
                                    obj.ws._recvQueue.unshift(chunk.subarray(copyLen));
                                }
                                if (resultPtr) self._writeU32(resultPtr, copyLen);
                                _pushIoCompletion(userData, copyLen, 0);
                                break;
                            }
                            _pushIoCompletion(userData, WAPI_ERR_BADF, 0);
                            break;
                        }

                        case WAPI_IO_OP_HTTP_FETCH:
                            // Gate on the wapi.http capability — if the host
                            // didn't advertise support (see _detectCapabilities),
                            // the op is rejected rather than silently forwarded.
                            if (!supportedCaps.includes("wapi.http")) {
                                _pushIoCompletion(userData, WAPI_ERR_NOTSUP, 0);
                                break;
                            }
                            wapi_http.dispatch_fetch(addr, len, addr2, len2, flags, userData);
                            break;

                        case WAPI_IO_OP_COMPRESS_PROCESS:
                            if (!supportedCaps.includes("wapi.compression")) {
                                _pushIoCompletion(userData, WAPI_ERR_NOTSUP, 0);
                                break;
                            }
                            wapi_compression.dispatch_process(addr, len, addr2, len2, flags, flags2, userData);
                            break;

                        case WAPI_IO_OP_FONT_GET_BYTES:
                            if (!supportedCaps.includes("wapi.font")) {
                                _pushIoCompletion(userData, WAPI_ERR_NOTSUP, 0);
                                break;
                            }
                            wapi_font_bytes.dispatch(addr, len, addr2, len2, flags, flags2, userData);
                            break;

                        default:
                            _pushIoCompletion(userData, WAPI_ERR_NOTSUP, 0);
                            break;
                    }
                    submitted++;
                }
                return submitted;
            },

            cancel(userData) {
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

            poll(eventPtr) {
                self._pollGamepads();
                if (self._eventQueue.length === 0) return 0;
                const ev = self._eventQueue.shift();
                self._writeEvent(eventPtr, ev);
                return 1;
            },

            wait(eventPtr, timeoutMs) {
                // Cannot block in browser; just poll
                return wapi_io.poll(eventPtr);
            },

            flush(eventType) {
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
        // wapi_filesystem (filesystem - MEMFS)
        // -------------------------------------------------------------------
        const wapi_filesystem = {
            preopen_count() {
                return self._preopens.length;
            },

            // (i32 index, i32 buf, i64 buf_len, i32 path_len_ptr) -> i32
            preopen_path(index, bufPtr, bufLen, pathLenPtr) {
                if (index < 0 || index >= self._preopens.length) return WAPI_ERR_OVERFLOW;
                const path = self._preopens[index].path;
                const written = self._writeString(bufPtr, Number(bufLen), path);
                self._writeU64(pathLenPtr, written);
                return WAPI_OK;
            },

            preopen_handle(index) {
                if (index < 0 || index >= self._preopens.length) return WAPI_HANDLE_INVALID;
                return self._preopens[index].handle;
            },

            // (i32 dir_fd, i32 path_sv_ptr, i32 oflags, i32 fdflags, i32 fd_out) -> i32
            open(dirFd, pathSvPtr, oflags, fdflags, fdOutPtr) {
                const relPath = self._readStringView(pathSvPtr);
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

            // (i32 fd, i32 buf, i64 len, i32 bytes_read_ptr) -> i32
            read(fd, bufPtr, len, bytesReadPtr) {
                const n = Number(len);
                if (fd === WAPI_STDIN) {
                    self._writeU64(bytesReadPtr, 0);
                    return WAPI_OK;
                }
                const entry = self.handles.get(fd);
                if (!entry || !entry.node) return WAPI_ERR_BADF;
                if (entry.node.type !== WAPI_FILETYPE_REGULAR) return WAPI_ERR_ISDIR;

                const avail = entry.node.data.length - entry.position;
                const readLen = Math.min(n, avail);
                if (readLen > 0) {
                    self._refreshViews();
                    self._u8.set(
                        entry.node.data.subarray(entry.position, entry.position + readLen),
                        bufPtr
                    );
                    entry.position += readLen;
                }
                self._writeU64(bytesReadPtr, readLen);
                return WAPI_OK;
            },

            // (i32 fd, i32 buf, i64 len, i32 bytes_written_ptr) -> i32
            write(fd, bufPtr, len, bytesWrittenPtr) {
                const n = Number(len);
                self._refreshViews();
                if (fd === WAPI_STDOUT || fd === WAPI_STDERR) {
                    const text = self._readString(bufPtr, n);
                    if (fd === WAPI_STDERR) console.error(text);
                    else console.log(text);
                    self._writeU64(bytesWrittenPtr, n);
                    return WAPI_OK;
                }
                const entry = self.handles.get(fd);
                if (!entry || !entry.node) return WAPI_ERR_BADF;
                if (entry.node.type !== WAPI_FILETYPE_REGULAR) return WAPI_ERR_ISDIR;

                const node = entry.node;
                const writeData = self._u8.slice(bufPtr, bufPtr + n);
                const needed = entry.position + n;
                if (needed > node.data.length) {
                    const newBuf = new Uint8Array(needed);
                    newBuf.set(node.data);
                    node.data = newBuf;
                }
                node.data.set(writeData, entry.position);
                entry.position += n;
                node.mtime = Date.now();
                self._writeU64(bytesWrittenPtr, n);
                return WAPI_OK;
            },

            // (i32 fd, i32 buf, i64 len, i64 offset, i32 bytes_read_ptr) -> i32
            pread(fd, bufPtr, len, offset, bytesReadPtr) {
                const entry = self.handles.get(fd);
                if (!entry || !entry.node) return WAPI_ERR_BADF;
                const n = Number(len);
                const off = Number(offset);
                const avail = entry.node.data.length - off;
                const readLen = Math.min(n, Math.max(0, avail));
                if (readLen > 0) {
                    self._refreshViews();
                    self._u8.set(entry.node.data.subarray(off, off + readLen), bufPtr);
                }
                self._writeU64(bytesReadPtr, readLen);
                return WAPI_OK;
            },

            // (i32 fd, i32 buf, i64 len, i64 offset, i32 bytes_written_ptr) -> i32
            pwrite(fd, bufPtr, len, offset, bytesWrittenPtr) {
                const entry = self.handles.get(fd);
                if (!entry || !entry.node) return WAPI_ERR_BADF;
                const n = Number(len);
                const off = Number(offset);
                self._refreshViews();
                const writeData = self._u8.slice(bufPtr, bufPtr + n);
                const needed = off + n;
                if (needed > entry.node.data.length) {
                    const newBuf = new Uint8Array(needed);
                    newBuf.set(entry.node.data);
                    entry.node.data = newBuf;
                }
                entry.node.data.set(writeData, off);
                entry.node.mtime = Date.now();
                self._writeU64(bytesWrittenPtr, n);
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

            // (i32 dir_fd, i32 path_sv_ptr, i32 stat_ptr) -> i32
            path_stat(dirFd, pathSvPtr, statPtr) {
                let basePath = "/";
                if (dirFd >= 4) {
                    const dirEntry = self.handles.get(dirFd);
                    if (dirEntry && dirEntry.path) basePath = dirEntry.path;
                }
                const relPath = self._readStringView(pathSvPtr);
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

            // (i32 dir_fd, i32 path_sv_ptr) -> i32
            mkdir(dirFd, pathSvPtr) {
                let basePath = "/";
                if (dirFd >= 4) {
                    const dirEntry = self.handles.get(dirFd);
                    if (dirEntry && dirEntry.path) basePath = dirEntry.path;
                }
                const relPath = self._readStringView(pathSvPtr);
                const fullPath = self.memfs._normPath(basePath, relPath);
                if (self.memfs.stat(fullPath)) return WAPI_ERR_EXIST;
                const node = self.memfs.mkdirp(fullPath);
                return node ? WAPI_OK : WAPI_ERR_IO;
            },

            // (i32 dir_fd, i32 path_sv_ptr) -> i32
            rmdir(dirFd, pathSvPtr) {
                let basePath = "/";
                if (dirFd >= 4) {
                    const dirEntry = self.handles.get(dirFd);
                    if (dirEntry && dirEntry.path) basePath = dirEntry.path;
                }
                const relPath = self._readStringView(pathSvPtr);
                const fullPath = self.memfs._normPath(basePath, relPath);
                const node = self.memfs.stat(fullPath);
                if (!node) return WAPI_ERR_NOENT;
                if (node.type !== WAPI_FILETYPE_DIRECTORY) return WAPI_ERR_NOTDIR;
                if (node.children.size > 0) return WAPI_ERR_NOTEMPTY;
                const { parent, name } = self.memfs._resolveParent(fullPath);
                if (parent) parent.children.delete(name);
                return WAPI_OK;
            },

            // (i32 dir_fd, i32 path_sv_ptr) -> i32
            unlink(dirFd, pathSvPtr) {
                let basePath = "/";
                if (dirFd >= 4) {
                    const dirEntry = self.handles.get(dirFd);
                    if (dirEntry && dirEntry.path) basePath = dirEntry.path;
                }
                const relPath = self._readStringView(pathSvPtr);
                const fullPath = self.memfs._normPath(basePath, relPath);
                const node = self.memfs.stat(fullPath);
                if (!node) return WAPI_ERR_NOENT;
                if (node.type === WAPI_FILETYPE_DIRECTORY) return WAPI_ERR_ISDIR;
                const { parent, name } = self.memfs._resolveParent(fullPath);
                if (parent) parent.children.delete(name);
                return WAPI_OK;
            },

            // (i32 old_dir_fd, i32 old_path_sv_ptr, i32 new_dir_fd, i32 new_path_sv_ptr) -> i32
            rename(oldDirFd, oldPathSvPtr, newDirFd, newPathSvPtr) {
                let oldBase = "/", newBase = "/";
                if (oldDirFd >= 4) {
                    const e = self.handles.get(oldDirFd);
                    if (e && e.path) oldBase = e.path;
                }
                if (newDirFd >= 4) {
                    const e = self.handles.get(newDirFd);
                    if (e && e.path) newBase = e.path;
                }
                const oldPath = self.memfs._normPath(oldBase, self._readStringView(oldPathSvPtr));
                const newPath = self.memfs._normPath(newBase, self._readStringView(newPathSvPtr));

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

            // (i32 fd, i32 buf, i64 buf_len, i64 cookie, i32 used_ptr) -> i32
            readdir(fd, bufPtr, bufLen, cookie, usedPtr) {
                const entry = self.handles.get(fd);
                if (!entry || !entry.node || entry.node.type !== WAPI_FILETYPE_DIRECTORY) return WAPI_ERR_BADF;

                const cap = Number(bufLen);
                const entries = Array.from(entry.node.children.values());
                let offset = 0;
                let idx = Number(cookie);
                self._refreshViews();

                while (idx < entries.length && offset + 24 < cap) {
                    const child = entries[idx];
                    const nameBytes = new TextEncoder().encode(child.name);
                    const entrySize = 24 + nameBytes.length;
                    if (offset + entrySize > cap) break;

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

                self._writeU64(usedPtr, offset);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_network has no separate import module. All network operations
        // (CONNECT, LISTEN, ACCEPT, CHANNEL_OPEN, CHANNEL_ACCEPT, RESOLVE,
        // SEND, RECV, CLOSE) flow through wapi_io.submit by opcode. The
        // dispatch lives inline in wapi_io.submit above and is gated by the
        // "wapi_network" capability — see wapi_network.h for the spec.
        // -------------------------------------------------------------------

        // -------------------------------------------------------------------
        // wapi_http (one-shot HTTP requests)
        //
        // Backs WAPI_IO_OP_HTTP_FETCH. wapi_io.submit forwards op fields to
        // wapi_http.dispatch_fetch which kicks the async fetch and pushes
        // a WAPI_EVENT_IO_COMPLETION record onto the shared _eventQueue
        // when the response settles.
        // -------------------------------------------------------------------
        const _httpMethods = ["GET", "POST", "PUT", "DELETE", "HEAD"];

        const wapi_http = {
            dispatch_fetch(addr, len, addr2, len2, method, userData) {
                const url = self._readString(addr, len);
                const verb = _httpMethods[method] || "GET";

                fetch(url, { method: verb, mode: "cors" })
                    .then(async (r) => {
                        const buf = new Uint8Array(await r.arrayBuffer());
                        self._refreshViews();
                        const n = Math.min(len2, buf.byteLength);
                        if (n > 0) self._u8.set(buf.subarray(0, n), addr2);
                        _pushIoCompletion(userData, r.ok ? n : -1, r.status);
                    })
                    .catch((err) => {
                        console.warn("[wapi_http]", url, err && (err.message || err));
                        _pushIoCompletion(userData, -1, 0);
                    });
            },
        };

        // -------------------------------------------------------------------
        // wapi_compression (gzip / deflate / deflate-raw via Compression Streams)
        //
        // Backs WAPI_IO_OP_COMPRESS_PROCESS. wapi_io.submit forwards op
        // fields to wapi_compression.dispatch_process which streams the
        // input through CompressionStream / DecompressionStream and pushes
        // a WAPI_EVENT_IO_COMPLETION record when the result is ready.
        // -------------------------------------------------------------------
        const _compressFormats = ["gzip", "deflate", "deflate-raw"];

        // -------------------------------------------------------------------
        // wapi_font_bytes (raw container fetch)
        //
        // Backs WAPI_IO_OP_FONT_GET_BYTES. Uses the Font Access API
        // (navigator.queryLocalFonts) to pull raw ttf/otf/woff bytes for
        // a system font face. The bytes are returned opaque — the caller
        // parses them with its own shaper/rasterizer. Weight + style are
        // best-effort filters; exact match is not guaranteed.
        // -------------------------------------------------------------------
        const wapi_font_bytes = {
            dispatch(addr, len, addr2, len2, weight, styleFlags, userData) {
                if (typeof navigator === "undefined" || !navigator.queryLocalFonts) {
                    _pushIoCompletion(userData, WAPI_ERR_NOTSUP, 0);
                    return;
                }
                const family = self._readString(addr, len);

                (async () => {
                    try {
                        const fonts = await navigator.queryLocalFonts();
                        // Linear scan. Font counts are in the hundreds
                        // at most, and this op runs once per face load —
                        // a hash lookup would be more code than it saves.
                        let match = null;
                        let fallback = null;
                        const wantItalic = (styleFlags & 0x0002) !== 0;
                        for (let i = 0; i < fonts.length; i++) {
                            const f = fonts[i];
                            if (f.family !== family) continue;
                            if (fallback === null) fallback = f;
                            // Style/subfamily strings vary per platform;
                            // match loosely on italic + weight substrings.
                            const style = (f.style || "").toLowerCase();
                            const italicOk = wantItalic
                                ? style.includes("italic") || style.includes("oblique")
                                : !(style.includes("italic") || style.includes("oblique"));
                            if (!italicOk) continue;
                            if (weight === 0) { match = f; break; }
                            if (style.includes(String(weight))) { match = f; break; }
                        }
                        const picked = match || fallback;
                        if (!picked) {
                            _pushIoCompletion(userData, WAPI_ERR_NOENT, 0);
                            return;
                        }
                        const blob = await picked.blob();
                        const buf = new Uint8Array(await blob.arrayBuffer());
                        self._refreshViews();
                        if (buf.byteLength > len2) {
                            // Probe or undersized buffer — report required size
                            // as the absolute value of the negative result so
                            // callers can reallocate and retry.
                            _pushIoCompletion(userData, -buf.byteLength, 0);
                            return;
                        }
                        if (buf.byteLength > 0) self._u8.set(buf, addr2);
                        _pushIoCompletion(userData, buf.byteLength, 0);
                    } catch (err) {
                        console.warn("[wapi_font_bytes]", family, err && (err.message || err));
                        _pushIoCompletion(userData, WAPI_ERR_ACCES, 0);
                    }
                })();
            },
        };

        const wapi_compression = {
            dispatch_process(addr, len, addr2, len2, algo, mode, userData) {
                const format = _compressFormats[algo];
                if (!format) { _pushIoCompletion(userData, -1, 0); return; }

                const decompress = mode === 1;
                const Stream = decompress
                    ? (typeof DecompressionStream !== "undefined" ? DecompressionStream : null)
                    : (typeof CompressionStream   !== "undefined" ? CompressionStream   : null);
                if (!Stream) { _pushIoCompletion(userData, -1, 0); return; }

                // Snapshot input bytes now — linear memory may grow during await.
                self._refreshViews();
                const input = self._u8.slice(addr, addr + len);

                (async () => {
                    try {
                        const stream = new Blob([input]).stream().pipeThrough(new Stream(format));
                        const out = new Uint8Array(await new Response(stream).arrayBuffer());

                        self._refreshViews();
                        const n = Math.min(len2, out.byteLength);
                        if (n > 0) self._u8.set(out.subarray(0, n), addr2);
                        _pushIoCompletion(userData, out.byteLength > len2 ? -2 : n, 0);
                    } catch (err) {
                        console.warn("[wapi_compression]", err && (err.message || err));
                        _pushIoCompletion(userData, -1, 0);
                    }
                })();
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
                // Read wapi_gpu_surface_config_t (unified ABI layout, 32 bytes):
                //   Offset  0: uint64_t nextInChain  (8 bytes)
                //   Offset  8: int32_t  surface      (4 bytes)
                //   Offset 12: int32_t  device       (4 bytes)
                //   Offset 16: uint32_t format       (4 bytes)
                //   Offset 20: uint32_t present_mode (4 bytes)
                //   Offset 24: uint32_t usage        (4 bytes)
                //   Offset 28: uint32_t _pad         (4 bytes)
                const surfaceHandle = self._readI32(configPtr + 8);
                const deviceHandle = self._readI32(configPtr + 12);
                const format = self._readU32(configPtr + 16);
                const presentMode = self._readU32(configPtr + 20);
                const usage = self._readU32(configPtr + 24);

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

                const th = self.handles.insert({ type: "gpu_texture", gpuObj: texture });
                const vh = self.handles.insert({ type: "gpu_texture_view", gpuObj: view });

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

            // (i32 texture_view, f32 r, f32 g, f32 b, f32 a) -> i32
            clear(textureViewHandle, r, g, b, a) {
                const entry = self.handles.get(textureViewHandle);
                if (!entry || !entry.gpuObj) return WAPI_ERR_BADF;
                const device = self._gpuDevice;
                if (!device) return WAPI_ERR_BADF;
                const encoder = device.createCommandEncoder();
                const pass = encoder.beginRenderPass({
                    colorAttachments: [{
                        view: entry.gpuObj,
                        clearValue: { r, g, b, a },
                        loadOp: 'clear',
                        storeOp: 'store',
                    }],
                });
                pass.end();
                device.queue.submit([encoder.finish()]);
                return WAPI_OK;
            },

            get_proc_address(namePtr, nameLen) {
                // In the browser, WebGPU functions are not exposed as a proc
                // table. Return 0 (NULL) -- modules should use the WAPI bridge.
                return 0;
            },
        };

        // -------------------------------------------------------------------
        // wapi_wgpu (standard webgpu.h functions via wasm32 C ABI)
        // -------------------------------------------------------------------
        // All handles are i32 (wasm32 pointers). Structs use wasm32 C ABI
        // (4-byte pointers, 4-byte size_t), NOT the WAPI unified ABI.

        // Helper: resolve a GPU handle from the handle table
        // Supports both wapi_gpu entries ({type, device/queue/texture/view}) and
        // wapi_wgpu entries ({type, gpuObj}).
        const gpuH = (h) => {
            const obj = self.handles.get(h);
            if (!obj) return null;
            if (obj.gpuObj) return obj.gpuObj;
            // Legacy wapi_gpu bridge entries
            return obj.device || obj.queue || obj.texture || obj.view || null;
        };

        // Helper: insert a GPU object into the handle table, return i32 handle
        const gpuInsert = (type, gpuObj) => {
            return self.handles.insert({ type, gpuObj });
        };

        // Helper: read WGPUBlendComponent (12 bytes) at ptr
        //   Offset 0: WGPUBlendOperation operation (u32)
        //   Offset 4: WGPUBlendFactor srcFactor (u32)
        //   Offset 8: WGPUBlendFactor dstFactor (u32)
        const readBlendComponent = (ptr) => ({
            operation: WGPU_BLEND_OP[self._readU32(ptr)] || "add",
            srcFactor: WGPU_BLEND_FACTOR[self._readU32(ptr + 4)] || "one",
            dstFactor: WGPU_BLEND_FACTOR[self._readU32(ptr + 8)] || "zero",
        });

        const wapi_wgpu = {
            // --- Device creation functions ---

            // WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device, const WGPUShaderModuleDescriptor* descriptor)
            // Wasm sig: (i32, i32) -> i32
            device_create_shader_module(device, descPtr) {
                self._refreshViews();
                const dev = gpuH(device);
                if (!dev) { console.error("[wapi_wgpu] device_create_shader_module: invalid device handle", device); return 0; }

                // WGPUShaderModuleDescriptor (12 bytes):
                //   Offset 0: WGPUChainedStruct* nextInChain (4)
                //   Offset 4: WGPUStringView label (8)
                const chainPtr = self._readU32(descPtr);
                const label = self._readWGPUStringView(descPtr + 4);

                // Follow chain to find WGPUShaderSourceWGSL (sType=0x02)
                let code = null;
                let p = chainPtr;
                while (p) {
                    // WGPUChainedStruct: next(4), sType(4)
                    const nextP = self._readU32(p);
                    const sType = self._readU32(p + 4);
                    if (sType === WGPU_STYPE_SHADER_SOURCE_WGSL) {
                        // WGPUShaderSourceWGSL: chain(8) + code WGPUStringView(8)
                        code = self._readWGPUStringView(p + 8);
                        break;
                    }
                    p = nextP;
                }

                if (code === null) {
                    console.error("[wapi_wgpu] device_create_shader_module: no WGSL source in chain");
                    return 0;
                }

                const desc = { code };
                if (label) desc.label = label;
                const sm = dev.createShaderModule(desc);
                return gpuInsert("gpu_shader_module", sm);
            },

            // WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor)
            // Wasm sig: (i32, i32) -> i32
            device_create_render_pipeline(device, descPtr) {
                self._refreshViews();
                const dev = gpuH(device);
                if (!dev) return 0;

                // WGPURenderPipelineDescriptor (96 bytes on wasm32):
                //   Offset  0: nextInChain (4)
                //   Offset  4: label WGPUStringView (8)
                //   Offset 12: layout (4) - WGPUPipelineLayout handle
                //   Offset 16: vertex WGPUVertexState (32 bytes inline)
                //   Offset 48: primitive WGPUPrimitiveState (24 bytes inline)
                //   Offset 72: depthStencil ptr (4)
                //   Offset 76: multisample WGPUMultisampleState (16 bytes inline)
                //   Offset 92: fragment ptr (4)

                const label = self._readWGPUStringView(descPtr + 4);
                const layoutHandle = self._readU32(descPtr + 12);

                // --- Vertex State (offset 16, 32 bytes) ---
                const vp = descPtr + 16;
                // WGPUVertexState:
                //   Offset 0: nextInChain (4)
                //   Offset 4: module (4)
                //   Offset 8: entryPoint WGPUStringView (8)
                //   Offset 16: constantCount (4)
                //   Offset 20: constants ptr (4)
                //   Offset 24: bufferCount (4)
                //   Offset 28: buffers ptr (4)
                const vertexModule = gpuH(self._readU32(vp + 4));
                const vertexEntry = self._readWGPUStringView(vp + 8);
                const bufferCount = self._readU32(vp + 24);
                const buffersPtr = self._readU32(vp + 28);

                const vertexBuffers = [];
                for (let i = 0; i < bufferCount; i++) {
                    // WGPUVertexBufferLayout (24 bytes):
                    //   Offset 0: nextInChain (4)
                    //   Offset 4: stepMode (4)
                    //   Offset 8: arrayStride (8, uint64)
                    //   Offset 16: attributeCount (4)
                    //   Offset 20: attributes ptr (4)
                    const bp = buffersPtr + i * 24;
                    const stepMode = WGPU_VERTEX_STEP_MODE[self._readU32(bp + 4)] || "vertex";
                    const arrayStride = Number(self._dv.getBigUint64(bp + 8, true));
                    const attrCount = self._readU32(bp + 16);
                    const attrPtr = self._readU32(bp + 20);

                    const attributes = [];
                    for (let j = 0; j < attrCount; j++) {
                        // WGPUVertexAttribute (24 bytes, align 8):
                        //   Offset 0: nextInChain (4)
                        //   Offset 4: format (4)
                        //   Offset 8: offset (8, uint64)
                        //   Offset 16: shaderLocation (4)
                        //   Offset 20: pad (4)
                        const ap = attrPtr + j * 24;
                        attributes.push({
                            format: WGPU_VERTEX_FORMAT[self._readU32(ap + 4)] || "float32",
                            offset: Number(self._dv.getBigUint64(ap + 8, true)),
                            shaderLocation: self._readU32(ap + 16),
                        });
                    }
                    vertexBuffers.push({ arrayStride, stepMode, attributes });
                }

                const vertexState = { module: vertexModule, buffers: vertexBuffers };
                if (vertexEntry) vertexState.entryPoint = vertexEntry;

                // --- Primitive State (offset 48, 24 bytes) ---
                const pp = descPtr + 48;
                // WGPUPrimitiveState:
                //   Offset 0: nextInChain (4)
                //   Offset 4: topology (4)
                //   Offset 8: stripIndexFormat (4)
                //   Offset 12: frontFace (4)
                //   Offset 16: cullMode (4)
                //   Offset 20: unclippedDepth (4)
                const topologyVal = self._readU32(pp + 4);
                const primitiveState = {
                    topology: WGPU_PRIMITIVE_TOPOLOGY[topologyVal] || "triangle-list",
                };
                const stripFmt = self._readU32(pp + 8);
                if (stripFmt) primitiveState.stripIndexFormat = WGPU_INDEX_FORMAT[stripFmt];
                const ff = self._readU32(pp + 12);
                if (ff) primitiveState.frontFace = WGPU_FRONT_FACE[ff];
                const cm = self._readU32(pp + 16);
                if (cm) primitiveState.cullMode = WGPU_CULL_MODE[cm];

                // --- Depth/Stencil (offset 72, pointer) ---
                const dsPtr = self._readU32(descPtr + 72);
                let depthStencil = undefined;
                if (dsPtr) {
                    // WGPUDepthStencilState (68 bytes):
                    //   Offset 0: nextInChain (4)
                    //   Offset 4: format (4)
                    //   Offset 8: depthWriteEnabled (4, WGPUOptionalBool)
                    //   Offset 12: depthCompare (4)
                    //   Offset 16: stencilFront (16 bytes: compare,failOp,depthFailOp,passOp)
                    //   Offset 32: stencilBack (16 bytes)
                    //   Offset 48: stencilReadMask (4)
                    //   Offset 52: stencilWriteMask (4)
                    //   Offset 56: depthBias (4, i32)
                    //   Offset 60: depthBiasSlopeScale (4, f32)
                    //   Offset 64: depthBiasClamp (4, f32)
                    const readStencilFace = (sp) => ({
                        compare: WGPU_COMPARE_FUNCTION[self._readU32(sp)] || "always",
                        failOp: WGPU_STENCIL_OP[self._readU32(sp + 4)] || "keep",
                        depthFailOp: WGPU_STENCIL_OP[self._readU32(sp + 8)] || "keep",
                        passOp: WGPU_STENCIL_OP[self._readU32(sp + 12)] || "keep",
                    });
                    const depthWriteVal = self._readU32(dsPtr + 8);
                    depthStencil = {
                        format: WGPU_TEXTURE_FORMAT_MAP[self._readU32(dsPtr + 4)] || "depth24plus",
                        depthWriteEnabled: depthWriteVal === 1, // WGPUOptionalBool: 0=undef, 1=true, 2=false
                        depthCompare: WGPU_COMPARE_FUNCTION[self._readU32(dsPtr + 12)] || "always",
                        stencilFront: readStencilFace(dsPtr + 16),
                        stencilBack: readStencilFace(dsPtr + 32),
                        stencilReadMask: self._readU32(dsPtr + 48),
                        stencilWriteMask: self._readU32(dsPtr + 52),
                        depthBias: self._readI32(dsPtr + 56),
                        depthBiasSlopeScale: self._dv.getFloat32(dsPtr + 60, true),
                        depthBiasClamp: self._dv.getFloat32(dsPtr + 64, true),
                    };
                }

                // --- Multisample State (offset 76, 16 bytes inline) ---
                const mp = descPtr + 76;
                // WGPUMultisampleState:
                //   Offset 0: nextInChain (4)
                //   Offset 4: count (4)
                //   Offset 8: mask (4)
                //   Offset 12: alphaToCoverageEnabled (4)
                const msCount = self._readU32(mp + 4) || 1;
                const msMask = self._readU32(mp + 8) || 0xFFFFFFFF;
                const multisample = {
                    count: msCount,
                    mask: msMask,
                    alphaToCoverageEnabled: self._readU32(mp + 12) !== 0,
                };

                // --- Fragment State (offset 92, pointer) ---
                const fragPtr = self._readU32(descPtr + 92);
                let fragment = undefined;
                if (fragPtr) {
                    // WGPUFragmentState (32 bytes):
                    //   Offset 0: nextInChain (4)
                    //   Offset 4: module (4)
                    //   Offset 8: entryPoint WGPUStringView (8)
                    //   Offset 16: constantCount (4)
                    //   Offset 20: constants ptr (4)
                    //   Offset 24: targetCount (4)
                    //   Offset 28: targets ptr (4)
                    const fragModule = gpuH(self._readU32(fragPtr + 4));
                    const fragEntry = self._readWGPUStringView(fragPtr + 8);
                    const targetCount = self._readU32(fragPtr + 24);
                    const targetsPtr = self._readU32(fragPtr + 28);

                    const targets = [];
                    for (let i = 0; i < targetCount; i++) {
                        // WGPUColorTargetState (24 bytes, align 8):
                        //   Offset 0: nextInChain (4)
                        //   Offset 4: format (4)
                        //   Offset 8: blend ptr (4)
                        //   Offset 12: _pad (4)
                        //   Offset 16: writeMask (8, uint64 WGPUColorWriteMask)
                        const tp = targetsPtr + i * 24;
                        const fmt = self._readU32(tp + 4);
                        const blendPtr = self._readU32(tp + 8);
                        const writeMask = Number(self._dv.getBigUint64(tp + 16, true));

                        const target = {
                            format: WGPU_TEXTURE_FORMAT_MAP[fmt] || "bgra8unorm",
                        };

                        if (blendPtr) {
                            // WGPUBlendState (24 bytes): color(12) + alpha(12)
                            target.blend = {
                                color: readBlendComponent(blendPtr),
                                alpha: readBlendComponent(blendPtr + 12),
                            };
                        }

                        if (writeMask !== 0) target.writeMask = writeMask;
                        targets.push(target);
                    }

                    fragment = { module: fragModule, targets };
                    if (fragEntry) fragment.entryPoint = fragEntry;
                }

                // Build the pipeline descriptor
                const pipelineDesc = {
                    vertex: vertexState,
                    primitive: primitiveState,
                    multisample,
                };
                if (label) pipelineDesc.label = label;
                if (layoutHandle) {
                    pipelineDesc.layout = gpuH(layoutHandle);
                } else {
                    pipelineDesc.layout = "auto";
                }
                if (depthStencil) pipelineDesc.depthStencil = depthStencil;
                if (fragment) pipelineDesc.fragment = fragment;

                const pipeline = dev.createRenderPipeline(pipelineDesc);
                return gpuInsert("gpu_render_pipeline", pipeline);
            },

            // WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor)
            device_create_compute_pipeline(device, descPtr) {
                self._refreshViews();
                const dev = gpuH(device);
                if (!dev) return 0;
                // WGPUComputePipelineDescriptor:
                //   Offset 0: nextInChain (4)
                //   Offset 4: label (8)
                //   Offset 12: layout (4)
                //   Offset 16: compute WGPUComputeState inline
                //     WGPUComputeState:
                //       Offset 0: nextInChain (4)
                //       Offset 4: module (4)
                //       Offset 8: entryPoint (8)
                //       Offset 16: constantCount (4)
                //       Offset 20: constants ptr (4)
                const label = self._readWGPUStringView(descPtr + 4);
                const layoutHandle = self._readU32(descPtr + 12);
                const cp = descPtr + 16;
                const computeModule = gpuH(self._readU32(cp + 4));
                const computeEntry = self._readWGPUStringView(cp + 8);

                const desc = {
                    layout: layoutHandle ? gpuH(layoutHandle) : "auto",
                    compute: { module: computeModule },
                };
                if (label) desc.label = label;
                if (computeEntry) desc.compute.entryPoint = computeEntry;

                const pipeline = dev.createComputePipeline(desc);
                return gpuInsert("gpu_compute_pipeline", pipeline);
            },

            // WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device, const WGPUCommandEncoderDescriptor* descriptor)
            device_create_command_encoder(device, descPtr) {
                self._refreshViews();
                const dev = gpuH(device);
                if (!dev) return 0;
                // WGPUCommandEncoderDescriptor (12 bytes):
                //   Offset 0: nextInChain (4)
                //   Offset 4: label (8)
                const desc = {};
                if (descPtr) {
                    const label = self._readWGPUStringView(descPtr + 4);
                    if (label) desc.label = label;
                }
                const encoder = dev.createCommandEncoder(desc);
                return gpuInsert("gpu_command_encoder", encoder);
            },

            // WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor* descriptor)
            device_create_buffer(device, descPtr) {
                self._refreshViews();
                const dev = gpuH(device);
                if (!dev) return 0;
                // WGPUBufferDescriptor (40 bytes, align 8):
                //   Offset 0: nextInChain (4)
                //   Offset 4: label WGPUStringView (8)
                //   Offset 12: _pad (4)
                //   Offset 16: usage (8, uint64 WGPUBufferUsage)
                //   Offset 24: size (8, uint64)
                //   Offset 32: mappedAtCreation (4)
                const label = self._readWGPUStringView(descPtr + 4);
                const usage = Number(self._dv.getBigUint64(descPtr + 16, true));
                const size = Number(self._dv.getBigUint64(descPtr + 24, true));
                const mappedAtCreation = self._readU32(descPtr + 32) !== 0;

                const desc = { size, usage, mappedAtCreation };
                if (label) desc.label = label;
                const buffer = dev.createBuffer(desc);
                return gpuInsert("gpu_buffer", buffer);
            },

            // WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor* descriptor)
            device_create_texture(device, descPtr) {
                self._refreshViews();
                const dev = gpuH(device);
                if (!dev) return 0;
                // WGPUTextureDescriptor (60 bytes, align 8):
                //   Offset 0: nextInChain (4)
                //   Offset 4: label WGPUStringView (8)
                //   Offset 12: _pad (4)
                //   Offset 16: usage (8, uint64 WGPUTextureUsage)
                //   Offset 24: dimension (4)
                //   Offset 28: size WGPUExtent3D (12: w,h,d)
                //   Offset 40: format (4)
                //   Offset 44: mipLevelCount (4)
                //   Offset 48: sampleCount (4)
                //   Offset 52: viewFormatCount (4)
                //   Offset 56: viewFormats ptr (4)
                const label = self._readWGPUStringView(descPtr + 4);
                const usage = Number(self._dv.getBigUint64(descPtr + 16, true));
                const dimension = WGPU_TEXTURE_DIMENSION[self._readU32(descPtr + 24)] || "2d";
                const width = self._readU32(descPtr + 28);
                const height = self._readU32(descPtr + 32);
                const depthOrArrayLayers = self._readU32(descPtr + 36);
                const format = WGPU_TEXTURE_FORMAT_MAP[self._readU32(descPtr + 40)] || "rgba8unorm";
                const mipLevelCount = self._readU32(descPtr + 44) || 1;
                const sampleCount = self._readU32(descPtr + 48) || 1;

                const desc = {
                    size: { width, height, depthOrArrayLayers },
                    format, usage, dimension, mipLevelCount, sampleCount,
                };
                if (label) desc.label = label;
                const texture = dev.createTexture(desc);
                return gpuInsert("gpu_texture", texture);
            },

            // WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor* descriptor)
            device_create_sampler(device, descPtr) {
                self._refreshViews();
                const dev = gpuH(device);
                if (!dev) return 0;
                // WGPUSamplerDescriptor:
                //   Offset 0: nextInChain (4)
                //   Offset 4: label (8)
                //   Offset 12: addressModeU (4)
                //   Offset 16: addressModeV (4)
                //   Offset 20: addressModeW (4)
                //   Offset 24: magFilter (4)
                //   Offset 28: minFilter (4)
                //   Offset 32: mipmapFilter (4)
                //   Offset 36: lodMinClamp (f32)
                //   Offset 40: lodMaxClamp (f32)
                //   Offset 44: compare (4)
                //   Offset 48: maxAnisotropy (u16)
                const label = self._readWGPUStringView(descPtr + 4);
                const desc = {
                    addressModeU: WGPU_ADDRESS_MODE[self._readU32(descPtr + 12)] || "clamp-to-edge",
                    addressModeV: WGPU_ADDRESS_MODE[self._readU32(descPtr + 16)] || "clamp-to-edge",
                    addressModeW: WGPU_ADDRESS_MODE[self._readU32(descPtr + 20)] || "clamp-to-edge",
                    magFilter: WGPU_FILTER_MODE[self._readU32(descPtr + 24)] || "nearest",
                    minFilter: WGPU_FILTER_MODE[self._readU32(descPtr + 28)] || "nearest",
                    mipmapFilter: WGPU_MIPMAP_FILTER_MODE[self._readU32(descPtr + 32)] || "nearest",
                    lodMinClamp: self._dv.getFloat32(descPtr + 36, true),
                    lodMaxClamp: self._dv.getFloat32(descPtr + 40, true),
                    maxAnisotropy: self._dv.getUint16(descPtr + 48, true) || 1,
                };
                const cmp = self._readU32(descPtr + 44);
                if (cmp) desc.compare = WGPU_COMPARE_FUNCTION[cmp];
                if (label) desc.label = label;
                const sampler = dev.createSampler(desc);
                return gpuInsert("gpu_sampler", sampler);
            },

            // WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(...)
            device_create_bind_group_layout(device, descPtr) {
                self._refreshViews();
                const dev = gpuH(device);
                if (!dev) return 0;
                // WGPUBindGroupLayoutDescriptor:
                //   Offset 0: nextInChain (4)
                //   Offset 4: label (8)
                //   Offset 12: entryCount (4)
                //   Offset 16: entries ptr (4)
                const label = self._readWGPUStringView(descPtr + 4);
                const entryCount = self._readU32(descPtr + 12);
                const entriesPtr = self._readU32(descPtr + 16);

                // WGPUBindGroupLayoutEntry (88 bytes, align 8):
                //   Offset 0: nextInChain (4)
                //   Offset 4: binding (4)
                //   Offset 8: visibility (8, WGPUShaderStage = uint64)
                //   Offset 16: bindingArraySize (4)
                //   Offset 20: pad (4)
                //   Offset 24: buffer (24 bytes: nextInChain@0, type@4, hasDynamicOffset@8, pad@12, minBindingSize@16)
                //   Offset 48: sampler (8 bytes: nextInChain@0, type@4)
                //   Offset 56: texture (16 bytes: nextInChain@0, sampleType@4, viewDimension@8, multisampled@12)
                //   Offset 72: storageTexture (16 bytes: nextInChain@0, access@4, format@8, viewDimension@12)
                const ENTRY_SIZE = 88;
                const entries = [];
                for (let i = 0; i < entryCount; i++) {
                    const ep = entriesPtr + i * ENTRY_SIZE;
                    const binding = self._readU32(ep + 4);
                    // visibility is uint64 at offset 8 — read low 32 bits (high bits unused for Vertex|Fragment|Compute)
                    const visibility = self._readU32(ep + 8);
                    const entry = { binding, visibility };

                    // Buffer binding layout at offset 24
                    const bufType = self._readU32(ep + 24 + 4);
                    if (bufType > 1) { // > Undefined(1) means it's set
                        const BUFFER_TYPE = { 2: "uniform", 3: "storage", 4: "read-only-storage" };
                        entry.buffer = {
                            type: BUFFER_TYPE[bufType] || "uniform",
                            hasDynamicOffset: self._readU32(ep + 24 + 8) !== 0,
                            minBindingSize: Number(self._dv.getBigUint64(ep + 24 + 16, true)),
                        };
                    }

                    // Sampler binding layout at offset 48
                    const samplerType = self._readU32(ep + 48 + 4);
                    if (samplerType > 1) { // > Undefined(1)
                        const SAMPLER_TYPE = { 2: "filtering", 3: "non-filtering", 4: "comparison" };
                        entry.sampler = { type: SAMPLER_TYPE[samplerType] || "filtering" };
                    }

                    // Texture binding layout at offset 56
                    const texSampleType = self._readU32(ep + 56 + 4);
                    if (texSampleType > 1) { // > Undefined(1)
                        const TEX_SAMPLE_TYPE = { 2: "float", 3: "unfilterable-float", 4: "depth", 5: "sint", 6: "uint" };
                        const texViewDim = self._readU32(ep + 56 + 8);
                        entry.texture = {
                            sampleType: TEX_SAMPLE_TYPE[texSampleType] || "float",
                            viewDimension: WGPU_TEXTURE_VIEW_DIMENSION[texViewDim] || "2d",
                            multisampled: self._readU32(ep + 56 + 12) !== 0,
                        };
                    }

                    // Storage texture binding layout at offset 72
                    const stAccess = self._readU32(ep + 72 + 4);
                    if (stAccess > 1) { // > Undefined(1)
                        const ST_ACCESS = { 2: "write-only", 3: "read-only", 4: "read-write" };
                        const stFmt = self._readU32(ep + 72 + 8);
                        const stViewDim = self._readU32(ep + 72 + 12);
                        entry.storageTexture = {
                            access: ST_ACCESS[stAccess] || "write-only",
                            format: WGPU_TEXTURE_FORMAT_MAP[stFmt] || "rgba8unorm",
                            viewDimension: WGPU_TEXTURE_VIEW_DIMENSION[stViewDim] || "2d",
                        };
                    }

                    entries.push(entry);
                }

                const desc = { entries };
                if (label) desc.label = label;
                const layout = dev.createBindGroupLayout(desc);
                return gpuInsert("gpu_bind_group_layout", layout);
            },

            // WGPUBindGroup wgpuDeviceCreateBindGroup(...)
            device_create_bind_group(device, descPtr) {
                self._refreshViews();
                const dev = gpuH(device);
                if (!dev) return 0;
                // WGPUBindGroupDescriptor:
                //   Offset 0: nextInChain (4)
                //   Offset 4: label (8)
                //   Offset 12: layout (4)
                //   Offset 16: entryCount (4)
                //   Offset 20: entries ptr (4)
                const label = self._readWGPUStringView(descPtr + 4);
                const layoutH = self._readU32(descPtr + 12);
                const entryCount = self._readU32(descPtr + 16);
                const entriesPtr = self._readU32(descPtr + 20);

                const entries = [];
                for (let i = 0; i < entryCount; i++) {
                    // WGPUBindGroupEntry (40 bytes, align 8):
                    //   Offset 0: nextInChain (4)
                    //   Offset 4: binding (4)
                    //   Offset 8: buffer (4, handle)
                    //   Offset 12: pad (4)
                    //   Offset 16: offset (8, u64)
                    //   Offset 24: size (8, u64)
                    //   Offset 32: sampler (4, handle)
                    //   Offset 36: textureView (4, handle)
                    const ep = entriesPtr + i * 40;
                    const binding = self._readU32(ep + 4);
                    const bufH = self._readU32(ep + 8);
                    const offset = Number(self._dv.getBigUint64(ep + 16, true));
                    const size = Number(self._dv.getBigUint64(ep + 24, true));
                    const samplerH = self._readU32(ep + 32);
                    const texViewH = self._readU32(ep + 36);

                    const entry = { binding };
                    if (bufH) {
                        entry.resource = { buffer: gpuH(bufH), offset, size };
                    } else if (samplerH) {
                        entry.resource = gpuH(samplerH);
                    } else if (texViewH) {
                        entry.resource = gpuH(texViewH);
                    }
                    entries.push(entry);
                }

                const desc = { layout: gpuH(layoutH), entries };
                if (label) desc.label = label;
                const bg = dev.createBindGroup(desc);
                return gpuInsert("gpu_bind_group", bg);
            },

            // WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(...)
            device_create_pipeline_layout(device, descPtr) {
                self._refreshViews();
                const dev = gpuH(device);
                if (!dev) return 0;
                // WGPUPipelineLayoutDescriptor:
                //   Offset 0: nextInChain (4)
                //   Offset 4: label (8)
                //   Offset 12: bindGroupLayoutCount (4)
                //   Offset 16: bindGroupLayouts ptr (4)
                const label = self._readWGPUStringView(descPtr + 4);
                const count = self._readU32(descPtr + 12);
                const layoutsPtr = self._readU32(descPtr + 16);

                const bindGroupLayouts = [];
                for (let i = 0; i < count; i++) {
                    const h = self._readU32(layoutsPtr + i * 4);
                    bindGroupLayouts.push(gpuH(h));
                }

                const desc = { bindGroupLayouts };
                if (label) desc.label = label;
                const layout = dev.createPipelineLayout(desc);
                return gpuInsert("gpu_pipeline_layout", layout);
            },

            // WGPUQuerySet wgpuDeviceCreateQuerySet(...)
            device_create_query_set(device, descPtr) {
                self._refreshViews();
                const dev = gpuH(device);
                if (!dev) return 0;
                // WGPUQuerySetDescriptor:
                //   Offset 0: nextInChain (4)
                //   Offset 4: label (8)
                //   Offset 12: type (4)
                //   Offset 16: count (4)
                const label = self._readWGPUStringView(descPtr + 4);
                const type = self._readU32(descPtr + 12) === 1 ? "timestamp" : "occlusion";
                const count = self._readU32(descPtr + 16);

                const desc = { type, count };
                if (label) desc.label = label;
                const qs = dev.createQuerySet(desc);
                return gpuInsert("gpu_query_set", qs);
            },

            // --- Queue functions ---

            // void wgpuQueueSubmit(WGPUQueue queue, size_t commandCount, const WGPUCommandBuffer* commands)
            // Wasm sig: (i32, i32, i32) -> void
            queue_submit(queue, commandCount, commandsPtr) {
                self._refreshViews();
                const q = gpuH(queue);
                if (!q) return;

                const cmdBufs = [];
                for (let i = 0; i < commandCount; i++) {
                    const h = self._readU32(commandsPtr + i * 4);
                    const cb = gpuH(h);
                    if (cb) cmdBufs.push(cb);
                }
                if (self._drawTrace) {
                    self._drawTrace.push(`SUBMIT(${commandCount} cmdBufs)`);
                }
                self._drawStats.submits = (self._drawStats.submits | 0) + 1;
                q.submit(cmdBufs);
            },

            // void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, const void* data, size_t size)
            // Wasm sig: (i32, i32, i64, i32, i32) -> void
            queue_write_buffer(queue, buffer, bufferOffsetLo, bufferOffsetHi, dataPtr, size) {
                self._refreshViews();
                const q = gpuH(queue);
                const buf = gpuH(buffer);
                if (!q || !buf) return;
                // On wasm32 with 64-bit args, they may be split. Handle both cases.
                let bufferOffset, data, dataSize;
                if (typeof bufferOffsetHi === 'number' && bufferOffsetHi > 0xFFFF) {
                    // Arguments: queue, buffer, bufferOffset(i64 as single BigInt), dataPtr, size
                    // This path handles when the runtime passes i64 as BigInt
                    bufferOffset = Number(bufferOffsetLo);
                    data = bufferOffsetHi; // actually dataPtr
                    dataSize = dataPtr;    // actually size
                } else {
                    bufferOffset = Number(bufferOffsetLo);
                    data = dataPtr;
                    dataSize = size;
                }
                const src = new Uint8Array(self.memory.buffer, data, dataSize);
                if (self._drawTrace) {
                    self._drawTrace.push(`writeBuffer(buf#${buffer}, off=${bufferOffset}, size=${dataSize})`);
                }
                self._drawStats.writes = (self._drawStats.writes | 0) + 1;
                q.writeBuffer(buf, bufferOffset, src);
            },

            // void wgpuQueueWriteTexture(WGPUQueue queue, const WGPUTexelCopyTextureInfo* destination, const void* data, size_t dataSize, const WGPUTexelCopyBufferLayout* dataLayout, const WGPUExtent3D* writeSize)
            queue_write_texture(queue, destPtr, dataPtr, dataSize, layoutPtr, sizePtr) {
                self._refreshViews();
                const q = gpuH(queue);
                if (!q) return;
                // WGPUTexelCopyTextureInfo (no nextInChain, 24 bytes):
                //   Offset 0: texture (4)
                //   Offset 4: mipLevel (4)
                //   Offset 8: origin WGPUOrigin3D (12: x,y,z)
                //   Offset 20: aspect (4)
                const tex = gpuH(self._readU32(destPtr));
                const destination = {
                    texture: tex,
                    mipLevel: self._readU32(destPtr + 4),
                    origin: {
                        x: self._readU32(destPtr + 8),
                        y: self._readU32(destPtr + 12),
                        z: self._readU32(destPtr + 16),
                    },
                    aspect: WGPU_TEXTURE_ASPECT[self._readU32(destPtr + 20)] || "all",
                };
                // WGPUTexelCopyBufferLayout (no nextInChain, 16 bytes):
                //   Offset 0: offset (8, u64)
                //   Offset 8: bytesPerRow (4)
                //   Offset 12: rowsPerImage (4)
                const dataLayout = {
                    offset: Number(self._dv.getBigUint64(layoutPtr, true)),
                    bytesPerRow: self._readU32(layoutPtr + 8),
                    rowsPerImage: self._readU32(layoutPtr + 12),
                };
                // WGPUExtent3D
                const writeSize = {
                    width: self._readU32(sizePtr),
                    height: self._readU32(sizePtr + 4),
                    depthOrArrayLayers: self._readU32(sizePtr + 8),
                };
                const src = new Uint8Array(self.memory.buffer, dataPtr, dataSize);
                q.writeTexture(destination, src, dataLayout, writeSize);
            },

            // --- Command Encoder functions ---

            // WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder encoder, const WGPURenderPassDescriptor* descriptor)
            command_encoder_begin_render_pass(encoder, descPtr) {
                self._refreshViews();
                const enc = gpuH(encoder);
                if (!enc) return 0;

                // WGPURenderPassDescriptor (32 bytes):
                //   Offset 0: nextInChain (4)
                //   Offset 4: label (8)
                //   Offset 12: colorAttachmentCount (4)
                //   Offset 16: colorAttachments ptr (4)
                //   Offset 20: depthStencilAttachment ptr (4)
                //   Offset 24: occlusionQuerySet (4)
                //   Offset 28: timestampWrites ptr (4)
                const label = self._readWGPUStringView(descPtr + 4);
                const colorCount = self._readU32(descPtr + 12);
                const colorPtr = self._readU32(descPtr + 16);
                const dsAttachPtr = self._readU32(descPtr + 20);

                const colorAttachments = [];
                for (let i = 0; i < colorCount; i++) {
                    // WGPURenderPassColorAttachment (56 bytes, align 8):
                    //   Offset 0: nextInChain (4), Offset 4: view (4),
                    //   Offset 8: depthSlice (4), Offset 12: resolveTarget (4),
                    //   Offset 16: loadOp (4), Offset 20: storeOp (4),
                    //   Offset 24: clearValue.r/g/b/a (4x double = 32 bytes)
                    const ap = colorPtr + i * 56;
                    const viewH = self._readU32(ap + 4);
                    const depthSlice = self._readU32(ap + 8);
                    const resolveH = self._readU32(ap + 12);
                    const loadOp = WGPU_LOAD_OP[self._readU32(ap + 16)] || "clear";
                    const storeOp = WGPU_STORE_OP[self._readU32(ap + 20)] || "store";
                    const clearR = self._dv.getFloat64(ap + 24, true);
                    const clearG = self._dv.getFloat64(ap + 32, true);
                    const clearB = self._dv.getFloat64(ap + 40, true);
                    const clearA = self._dv.getFloat64(ap + 48, true);

                    const attach = {
                        view: gpuH(viewH),
                        loadOp,
                        storeOp,
                        clearValue: { r: clearR, g: clearG, b: clearB, a: clearA },
                    };
                    if (resolveH) attach.resolveTarget = gpuH(resolveH);
                    colorAttachments.push(attach);
                }

                const rpDesc = { colorAttachments };
                if (label) rpDesc.label = label;

                if (dsAttachPtr) {
                    // WGPURenderPassDepthStencilAttachment (40 bytes):
                    //   Offset 0: nextInChain (4)
                    //   Offset 4: view (4)
                    //   Offset 8: depthLoadOp (4)
                    //   Offset 12: depthStoreOp (4)
                    //   Offset 16: depthClearValue (f32)
                    //   Offset 20: depthReadOnly (4)
                    //   Offset 24: stencilLoadOp (4)
                    //   Offset 28: stencilStoreOp (4)
                    //   Offset 32: stencilClearValue (4)
                    //   Offset 36: stencilReadOnly (4)
                    rpDesc.depthStencilAttachment = {
                        view: gpuH(self._readU32(dsAttachPtr + 4)),
                        depthLoadOp: WGPU_LOAD_OP[self._readU32(dsAttachPtr + 8)] || "clear",
                        depthStoreOp: WGPU_STORE_OP[self._readU32(dsAttachPtr + 12)] || "store",
                        depthClearValue: self._dv.getFloat32(dsAttachPtr + 16, true),
                        depthReadOnly: self._readU32(dsAttachPtr + 20) !== 0,
                        stencilLoadOp: WGPU_LOAD_OP[self._readU32(dsAttachPtr + 24)] || "clear",
                        stencilStoreOp: WGPU_STORE_OP[self._readU32(dsAttachPtr + 28)] || "store",
                        stencilClearValue: self._readU32(dsAttachPtr + 32),
                        stencilReadOnly: self._readU32(dsAttachPtr + 36) !== 0,
                    };
                }

                const pass = enc.beginRenderPass(rpDesc);
                return gpuInsert("gpu_render_pass_encoder", pass);
            },

            // WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder encoder, const WGPUComputePassDescriptor* descriptor)
            command_encoder_begin_compute_pass(encoder, descPtr) {
                self._refreshViews();
                const enc = gpuH(encoder);
                if (!enc) return 0;
                const desc = {};
                if (descPtr) {
                    const label = self._readWGPUStringView(descPtr + 4);
                    if (label) desc.label = label;
                }
                const pass = enc.beginComputePass(desc);
                return gpuInsert("gpu_compute_pass_encoder", pass);
            },

            // void wgpuCommandEncoderCopyBufferToBuffer(...)
            command_encoder_copy_buffer_to_buffer(encoder, source, sourceOffsetLo, sourceOffsetHi, destination, destinationOffsetLo, destinationOffsetHi, sizeLo, sizeHi) {
                const enc = gpuH(encoder);
                if (!enc) return;
                const src = gpuH(source);
                const dst = gpuH(destination);
                enc.copyBufferToBuffer(src, Number(sourceOffsetLo), dst, Number(destinationOffsetLo), Number(sizeLo));
            },

            // void wgpuCommandEncoderCopyBufferToTexture(encoder, source, destination, copySize)
            command_encoder_copy_buffer_to_texture(encoder, sourcePtr, destPtr, copySizePtr) {
                self._refreshViews();
                const enc = gpuH(encoder);
                if (!enc) return;
                // WGPUTexelCopyBufferInfo (no nextInChain, 24 bytes):
                //   Offset 0: layout.offset (8, u64)
                //   Offset 8: layout.bytesPerRow (4)
                //   Offset 12: layout.rowsPerImage (4)
                //   Offset 16: buffer (4)
                const source = {
                    buffer: gpuH(self._readU32(sourcePtr + 16)),
                    offset: Number(self._dv.getBigUint64(sourcePtr, true)),
                    bytesPerRow: self._readU32(sourcePtr + 8),
                    rowsPerImage: self._readU32(sourcePtr + 12),
                };
                // WGPUTexelCopyTextureInfo (no nextInChain, 24 bytes):
                const tex = gpuH(self._readU32(destPtr));
                const destination = {
                    texture: tex,
                    mipLevel: self._readU32(destPtr + 4),
                    origin: {
                        x: self._readU32(destPtr + 8),
                        y: self._readU32(destPtr + 12),
                        z: self._readU32(destPtr + 16),
                    },
                };
                const copySize = {
                    width: self._readU32(copySizePtr),
                    height: self._readU32(copySizePtr + 4),
                    depthOrArrayLayers: self._readU32(copySizePtr + 8),
                };
                enc.copyBufferToTexture(source, destination, copySize);
            },

            // void wgpuCommandEncoderCopyTextureToBuffer(encoder, source, destination, copySize)
            command_encoder_copy_texture_to_buffer(encoder, sourcePtr, destPtr, copySizePtr) {
                self._refreshViews();
                const enc = gpuH(encoder);
                if (!enc) return;
                // WGPUTexelCopyTextureInfo (no nextInChain):
                const tex = gpuH(self._readU32(sourcePtr));
                const source = {
                    texture: tex,
                    mipLevel: self._readU32(sourcePtr + 4),
                    origin: {
                        x: self._readU32(sourcePtr + 8),
                        y: self._readU32(sourcePtr + 12),
                        z: self._readU32(sourcePtr + 16),
                    },
                };
                // WGPUTexelCopyBufferInfo (no nextInChain):
                const destination = {
                    buffer: gpuH(self._readU32(destPtr + 16)),
                    offset: Number(self._dv.getBigUint64(destPtr, true)),
                    bytesPerRow: self._readU32(destPtr + 8),
                    rowsPerImage: self._readU32(destPtr + 12),
                };
                const copySize = {
                    width: self._readU32(copySizePtr),
                    height: self._readU32(copySizePtr + 4),
                    depthOrArrayLayers: self._readU32(copySizePtr + 8),
                };
                enc.copyTextureToBuffer(source, destination, copySize);
            },

            // void wgpuCommandEncoderCopyTextureToTexture(encoder, source, destination, copySize)
            command_encoder_copy_texture_to_texture(encoder, sourcePtr, destPtr, copySizePtr) {
                self._refreshViews();
                const enc = gpuH(encoder);
                if (!enc) return;
                // WGPUTexelCopyTextureInfo (no nextInChain):
                const srcTex = gpuH(self._readU32(sourcePtr));
                const source = {
                    texture: srcTex,
                    mipLevel: self._readU32(sourcePtr + 4),
                    origin: {
                        x: self._readU32(sourcePtr + 8),
                        y: self._readU32(sourcePtr + 12),
                        z: self._readU32(sourcePtr + 16),
                    },
                };
                const dstTex = gpuH(self._readU32(destPtr));
                const destination = {
                    texture: dstTex,
                    mipLevel: self._readU32(destPtr + 4),
                    origin: {
                        x: self._readU32(destPtr + 8),
                        y: self._readU32(destPtr + 12),
                        z: self._readU32(destPtr + 16),
                    },
                };
                const copySize = {
                    width: self._readU32(copySizePtr),
                    height: self._readU32(copySizePtr + 4),
                    depthOrArrayLayers: self._readU32(copySizePtr + 8),
                };
                enc.copyTextureToTexture(source, destination, copySize);
            },

            // WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder encoder, const WGPUCommandBufferDescriptor* descriptor)
            command_encoder_finish(encoder, descPtr) {
                self._refreshViews();
                const enc = gpuH(encoder);
                if (!enc) return 0;
                const desc = {};
                if (descPtr) {
                    const label = self._readWGPUStringView(descPtr + 4);
                    if (label) desc.label = label;
                }
                const cmdBuf = enc.finish(desc);
                return gpuInsert("gpu_command_buffer", cmdBuf);
            },

            // --- Render Pass Encoder functions ---

            // void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder encoder, WGPURenderPipeline pipeline)
            render_pass_set_pipeline(encoder, pipeline) {
                const pass = gpuH(encoder);
                if (pass) pass.setPipeline(gpuH(pipeline));
            },

            // void wgpuRenderPassEncoderSetBindGroup(encoder, groupIndex, group, dynamicOffsetCount, dynamicOffsets)
            render_pass_set_bind_group(encoder, groupIndex, group, dynamicOffsetCount, dynamicOffsetsPtr) {
                self._refreshViews();
                const pass = gpuH(encoder);
                if (!pass) return;
                const bg = gpuH(group);
                const offsets = [];
                for (let i = 0; i < dynamicOffsetCount; i++) {
                    offsets.push(self._readU32(dynamicOffsetsPtr + i * 4));
                }
                pass.setBindGroup(groupIndex, bg, offsets);
            },

            // void wgpuRenderPassEncoderSetVertexBuffer(encoder, slot, buffer, offset, size)
            render_pass_set_vertex_buffer(encoder, slot, buffer, offsetLo, offsetHi, sizeLo, sizeHi) {
                const pass = gpuH(encoder);
                if (!pass) return;
                const buf = gpuH(buffer);
                // wasm32 passes uint64_t as two i32 args
                const offset = Number(offsetLo);
                const size = Number(sizeLo);
                pass.setVertexBuffer(slot, buf, offset, size === 0 ? undefined : size);
            },

            // void wgpuRenderPassEncoderSetIndexBuffer(encoder, buffer, format, offset, size)
            render_pass_set_index_buffer(encoder, buffer, format, offsetLo, offsetHi, sizeLo, sizeHi) {
                const pass = gpuH(encoder);
                if (!pass) return;
                const buf = gpuH(buffer);
                const fmt = WGPU_INDEX_FORMAT[format] || "uint16";
                const offset = Number(offsetLo);
                const size = Number(sizeLo);
                pass.setIndexBuffer(buf, fmt, offset, size === 0 ? undefined : size);
            },

            // void wgpuRenderPassEncoderSetViewport(encoder, x, y, width, height, minDepth, maxDepth)
            render_pass_set_viewport(encoder, x, y, width, height, minDepth, maxDepth) {
                const pass = gpuH(encoder);
                if (pass) pass.setViewport(x, y, width, height, minDepth, maxDepth);
            },

            // void wgpuRenderPassEncoderSetScissorRect(encoder, x, y, width, height)
            render_pass_set_scissor_rect(encoder, x, y, width, height) {
                const pass = gpuH(encoder);
                if (!pass) return;
                if (self._drawTrace) {
                    self._drawTrace.push(`scissor(${x},${y},${width},${height})`);
                }
                pass.setScissorRect(x, y, width, height);
            },

            // void wgpuRenderPassEncoderDraw(encoder, vertexCount, instanceCount, firstVertex, firstInstance)
            render_pass_draw(encoder, vertexCount, instanceCount, firstVertex, firstInstance) {
                const pass = gpuH(encoder);
                if (!pass) return;
                if (self._drawTrace) {
                    self._drawTrace.push(`draw(vtx=${vertexCount}, first=${firstVertex}, inst=${instanceCount})`);
                }
                self._drawStats.draws++;
                self._drawStats.totalVerts += vertexCount;
                pass.draw(vertexCount, instanceCount, firstVertex, firstInstance);
            },

            // void wgpuRenderPassEncoderDrawIndexed(encoder, indexCount, instanceCount, firstIndex, baseVertex, firstInstance)
            render_pass_draw_indexed(encoder, indexCount, instanceCount, firstIndex, baseVertex, firstInstance) {
                const pass = gpuH(encoder);
                if (pass) pass.drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
            },

            // void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder encoder)
            render_pass_end(encoder) {
                const pass = gpuH(encoder);
                if (pass) pass.end();
            },

            // --- Compute Pass Encoder functions ---

            compute_pass_set_pipeline(encoder, pipeline) {
                const pass = gpuH(encoder);
                if (pass) pass.setPipeline(gpuH(pipeline));
            },

            compute_pass_set_bind_group(encoder, groupIndex, group, dynamicOffsetCount, dynamicOffsetsPtr) {
                self._refreshViews();
                const pass = gpuH(encoder);
                if (!pass) return;
                const bg = gpuH(group);
                const offsets = [];
                for (let i = 0; i < dynamicOffsetCount; i++) {
                    offsets.push(self._readU32(dynamicOffsetsPtr + i * 4));
                }
                pass.setBindGroup(groupIndex, bg, offsets);
            },

            compute_pass_dispatch_workgroups(encoder, x, y, z) {
                const pass = gpuH(encoder);
                if (pass) pass.dispatchWorkgroups(x, y, z);
            },

            compute_pass_end(encoder) {
                const pass = gpuH(encoder);
                if (pass) pass.end();
            },

            // --- Buffer functions ---

            buffer_get_mapped_range(buffer, offsetLo, offsetHi, sizeLo, sizeHi) {
                // Returns a pointer into wasm linear memory where the mapped data is
                // This is complex in browser WebGPU - the mapped range is a JS ArrayBuffer,
                // not wasm memory. We'd need to copy. Return 0 for now.
                // TODO: proper buffer mapping with staging
                return 0;
            },

            buffer_unmap(buffer) {
                const buf = gpuH(buffer);
                if (buf) buf.unmap();
            },

            // --- Texture functions ---

            // WGPUTextureView wgpuTextureCreateView(WGPUTexture texture, const WGPUTextureViewDescriptor* descriptor)
            texture_create_view(texture, descPtr) {
                self._refreshViews();
                const tex = gpuH(texture);
                if (!tex) return 0;
                const desc = {};
                if (descPtr) {
                    // WGPUTextureViewDescriptor:
                    //   Offset 0: nextInChain (4)
                    //   Offset 4: label (8)
                    //   Offset 12: format (4)
                    //   Offset 16: dimension (4)
                    //   Offset 20: baseMipLevel (4)
                    //   Offset 24: mipLevelCount (4)
                    //   Offset 28: baseArrayLayer (4)
                    //   Offset 32: arrayLayerCount (4)
                    //   Offset 36: aspect (4)
                    const label = self._readWGPUStringView(descPtr + 4);
                    if (label) desc.label = label;
                    const fmt = self._readU32(descPtr + 12);
                    if (fmt) desc.format = WGPU_TEXTURE_FORMAT_MAP[fmt];
                    const dim = self._readU32(descPtr + 16);
                    if (dim) desc.dimension = WGPU_TEXTURE_VIEW_DIMENSION[dim];
                    desc.baseMipLevel = self._readU32(descPtr + 20);
                    desc.mipLevelCount = self._readU32(descPtr + 24);
                    desc.baseArrayLayer = self._readU32(descPtr + 28);
                    desc.arrayLayerCount = self._readU32(descPtr + 32);
                    const aspect = self._readU32(descPtr + 36);
                    if (aspect) desc.aspect = WGPU_TEXTURE_ASPECT[aspect];
                }
                const view = tex.createView(desc);
                return gpuInsert("gpu_texture_view", view);
            },

            // --- Release functions ---
            buffer_release(h) { self.handles.remove(h); },
            texture_release(h) { self.handles.remove(h); },
            texture_view_release(h) { self.handles.remove(h); },
            sampler_release(h) { self.handles.remove(h); },
            bind_group_release(h) { self.handles.remove(h); },
            bind_group_layout_release(h) { self.handles.remove(h); },
            pipeline_layout_release(h) { self.handles.remove(h); },
            shader_module_release(h) { self.handles.remove(h); },
            render_pipeline_release(h) { self.handles.remove(h); },
            compute_pipeline_release(h) { self.handles.remove(h); },
            command_buffer_release(h) { self.handles.remove(h); },
            command_encoder_release(h) { self.handles.remove(h); },
            render_pass_release(h) { self.handles.remove(h); },
            render_pass_encoder_release(h) { self.handles.remove(h); },
            compute_pass_release(h) { self.handles.remove(h); },
            query_set_release(h) { self.handles.remove(h); },
        };

        // -------------------------------------------------------------------
        // wapi_surface (windowing - canvas)
        // -------------------------------------------------------------------
        const wapi_surface = {
            create(descPtr, surfaceOutPtr) {
                self._refreshViews();
                // Read wapi_surface_desc_t (unified ABI layout):
                //   Offset  0: uint64_t nextInChain (8 bytes)
                //   Offset  8: int32_t  width       (4 bytes)
                //   Offset 12: int32_t  height      (4 bytes)
                //   Offset 16: uint64_t flags       (8 bytes)
                //   Offset 24: uint32_t _pad        (4 bytes)
                const nextInChainPtr = Number(self._readU64(descPtr + 0));
                const width    = self._readI32(descPtr + 8);
                const height   = self._readI32(descPtr + 12);
                const flags    = Number(self._readU64(descPtr + 16));

                // Follow nextInChain to find wapi_window_config_t
                let title = "WAPI Application";
                if (nextInChainPtr) {
                    // wapi_chained_struct_t header (16 bytes):
                    //   Offset  0: uint64_t next  (8 bytes)
                    //   Offset  8: uint32_t sType  (4 bytes)
                    //   Offset 12: uint32_t _pad   (4 bytes)
                    const sType = self._readU32(nextInChainPtr + 8);
                    if (sType === 0x0001) { // WAPI_STYPE_WINDOW_CONFIG
                        // wapi_window_config_t:
                        //   Offset  0: chain (16 bytes)
                        //   Offset 16: wapi_string_view_t title (16 bytes)
                        //   Offset 32: uint32_t window_flags (4 bytes)
                        //   Offset 36: uint32_t _pad (4 bytes)
                        const t = self._readStringView(nextInChainPtr + 16);
                        if (t) title = t;
                    }
                }

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
                console.log("[WAPI shim] get_dpi_scale called, returning", info.dpr);
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

            // (i32 surface, i32 sv_ptr) -> i32
            // title is wapi_string_view_t passed indirectly
            set_title(surfaceHandle, svPtr) {
                const title = self._readStringView(svPtr);
                if (title) document.title = title;
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
        // Synthetic singleton device handles. The browser exposes a single
        // logical mouse/keyboard/pointer, so device_open returns these fixed
        // handles and subsequent calls route to the shared DOM-event state.
        const WAPI_INPUT_HANDLE_MOUSE    = 0x10000001;
        const WAPI_INPUT_HANDLE_KEYBOARD = 0x10000002;
        const WAPI_INPUT_HANDLE_POINTER  = 0x10000003;

        const wapi_input = {
            // ---- Device enumeration ------------------------------------
            device_count(type) {
                // type: 0=mouse, 1=keyboard, 2=touch, 3=pen, 4=gamepad, 5=pointer, 6=hid
                if (type === 0 || type === 1 || type === 5) return 1;
                return 0;
            },
            device_open(type, index, outHandlePtr) {
                if (index !== 0) return WAPI_ERR_RANGE;
                let handle;
                if (type === 0)      handle = WAPI_INPUT_HANDLE_MOUSE;
                else if (type === 1) handle = WAPI_INPUT_HANDLE_KEYBOARD;
                else if (type === 5) handle = WAPI_INPUT_HANDLE_POINTER;
                else return WAPI_ERR_NOTSUP;
                self._writeI32(outHandlePtr, handle);
                return WAPI_OK;
            },
            device_close(handle) { return WAPI_OK; },
            device_get_type(handle) {
                if (handle === WAPI_INPUT_HANDLE_MOUSE)    return 0;
                if (handle === WAPI_INPUT_HANDLE_KEYBOARD) return 1;
                if (handle === WAPI_INPUT_HANDLE_POINTER)  return 5;
                return -1;
            },
            device_get_uid(handle, uidPtr) {
                self._refreshViews();
                for (let i = 0; i < 16; i++) self._u8[uidPtr + i] = 0;
                self._u8[uidPtr + 0] = handle & 0xFF;
                return WAPI_OK;
            },
            device_get_name(handle, bufPtr, bufLen, nameLenPtr) {
                let name = "unknown";
                if (handle === WAPI_INPUT_HANDLE_MOUSE)    name = "Browser Mouse";
                else if (handle === WAPI_INPUT_HANDLE_KEYBOARD) name = "Browser Keyboard";
                else if (handle === WAPI_INPUT_HANDLE_POINTER)  name = "Browser Pointer";
                const written = self._writeString(bufPtr, bufLen, name);
                self._writeU32(nameLenPtr, written);
                return WAPI_OK;
            },

            // ---- Mouse --------------------------------------------------
            mouse_get_info(handle, infoPtr) { return WAPI_ERR_NOTSUP; },
            mouse_get_position(handle, surface, xPtr, yPtr) {
                self._writeF32(xPtr, self._mouseX);
                self._writeF32(yPtr, self._mouseY);
                return WAPI_OK;
            },
            mouse_get_button_state(handle) {
                return self._mouseButtons;
            },
            mouse_set_relative(handle, surface, enabled) {
                const info = self._surfaces.get(surface);
                if (!info) return WAPI_ERR_BADF;
                if (enabled) info.canvas.requestPointerLock();
                else         document.exitPointerLock();
                return WAPI_OK;
            },
            mouse_warp(handle, surface, x, y) { return WAPI_ERR_NOTSUP; },
            mouse_set_cursor(handle, cursorType) {
                const cursors = [
                    'default', 'pointer', 'text', 'crosshair', 'move',
                    'ns-resize', 'ew-resize', 'nwse-resize', 'nesw-resize',
                    'not-allowed', 'wait', 'grab', 'grabbing', 'none',
                ];
                const css = cursors[cursorType] || 'default';
                for (const [, info] of self._surfaces) {
                    if (info.canvas) info.canvas.style.cursor = css;
                }
                return WAPI_OK;
            },
            mouse_set_cursor_image(handle, dataPtr, w, h, hotX, hotY) { return WAPI_ERR_NOTSUP; },

            // ---- Keyboard -----------------------------------------------
            keyboard_key_pressed(handle, scancode) {
                return self._keyState.has(scancode) ? 1 : 0;
            },
            keyboard_get_modstate(handle) {
                return self._modState;
            },

            // ---- Text input (surface-scoped, void return) --------------
            start_textinput(surface) { /* no-op in browser */ },
            stop_textinput(surface)  { /* no-op in browser */ },

            // ---- Touch --------------------------------------------------
            touch_get_info(handle, infoPtr)               { return WAPI_ERR_NOTSUP; },
            touch_finger_count(handle)                    { return 0; },
            touch_get_finger(handle, fingerIndex, statePtr) { return WAPI_ERR_NOTSUP; },

            // ---- Pen ----------------------------------------------------
            pen_get_info(handle, infoPtr)           { return WAPI_ERR_NOTSUP; },
            pen_get_axis(handle, axis, valuePtr)    { return WAPI_ERR_NOTSUP; },
            pen_get_position(handle, xPtr, yPtr)    { return WAPI_ERR_NOTSUP; },

            // ---- Gamepad ------------------------------------------------
            gamepad_get_info(handle, infoPtr)                        { return WAPI_ERR_NOTSUP; },
            gamepad_get_button(handle, button)                       { return 0; },
            gamepad_get_axis(handle, axis, valuePtr)                 { return WAPI_ERR_NOTSUP; },
            gamepad_rumble(handle, lowFreq, highFreq, durationMs)    { return WAPI_ERR_NOTSUP; },
            gamepad_rumble_triggers(handle, left, right, durationMs) { return WAPI_ERR_NOTSUP; },
            gamepad_set_led(handle, r, g, b)                         { return WAPI_ERR_NOTSUP; },
            gamepad_enable_sensor(handle, sensorType, enabled)       { return WAPI_ERR_NOTSUP; },
            gamepad_get_sensor_data(handle, sensorType, dataPtr)     { return WAPI_ERR_NOTSUP; },
            gamepad_get_touchpad_finger(handle, touchpad, finger, statePtr) { return WAPI_ERR_NOTSUP; },
            gamepad_get_battery(handle, percentPtr)                  { return -1; },

            // ---- Pointer (unified) --------------------------------------
            pointer_get_info(handle, infoPtr) {
                self._refreshViews();
                self._u8[infoPtr + 0] = 1; // has_pressure
                self._u8[infoPtr + 1] = 1; // has_tilt
                self._u8[infoPtr + 2] = 1; // has_twist
                self._u8[infoPtr + 3] = 1; // has_width_height
                return WAPI_OK;
            },
            pointer_get_position(handle, surface, xPtr, yPtr) {
                self._writeF32(xPtr, self._pointerX);
                self._writeF32(yPtr, self._pointerY);
                return WAPI_OK;
            },
            pointer_get_buttons(handle) {
                return self._pointerButtons;
            },

            // ---- HID ----------------------------------------------------
            hid_request_device(vendorId, productId, usagePage, outHandlePtr) { return WAPI_ERR_NOTSUP; },
            hid_get_info(handle, infoPtr)                                    { return WAPI_ERR_NOTSUP; },
            hid_send_report(handle, reportId, dataPtr, dataLen)              { return WAPI_ERR_NOTSUP; },
            hid_send_feature_report(handle, reportId, dataPtr, dataLen)      { return WAPI_ERR_NOTSUP; },
            hid_receive_report(handle, bufPtr, bufLen, bytesReadPtr)         { return WAPI_ERR_NOTSUP; },

            // ---- IME ----------------------------------------------------
            ime_enable(surfaceId, hint)                          { return WAPI_ERR_NOTSUP; },
            ime_disable(surfaceId)                               { return WAPI_ERR_NOTSUP; },
            ime_set_candidate_rect(surfaceId, x, y, w, h)        { return WAPI_ERR_NOTSUP; },
            ime_commit(surfaceId)                                { return WAPI_ERR_NOTSUP; },
            ime_cancel(surfaceId)                                { return WAPI_ERR_NOTSUP; },
            ime_read_text(sequence, bufPtr, bufLen, outLenPtr)   { return WAPI_ERR_NOTSUP; },
            ime_read_segment(sequence, index, outPtr)            { return WAPI_ERR_NOTSUP; },

            // ---- Hotkey -------------------------------------------------
            hotkey_register(bindingPtr, outIdPtr)   { return WAPI_ERR_NOTSUP; },
            hotkey_unregister(id)                   { return WAPI_ERR_NOTSUP; },
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
            format_count() {
                let n = 0;
                if (self._clipboardText.length > 0) n++;
                if (self._clipboardHtml.length > 0) n++;
                return n;
            },

            format_name(index, bufPtr, bufLen, outLenPtr) {
                const mimes = [];
                if (self._clipboardText.length > 0) mimes.push("text/plain");
                if (self._clipboardHtml.length > 0) mimes.push("text/html");
                if (index < 0 || index >= mimes.length) return WAPI_ERR_RANGE;
                const written = self._writeString(bufPtr, bufLen, mimes[index]);
                self._writeU32(outLenPtr, written);
                return WAPI_OK;
            },

            has_format(mimeSvPtr) {
                const mime = self._readStringView(mimeSvPtr);
                if (mime === "text/plain") return self._clipboardText.length > 0 ? 1 : 0;
                if (mime === "text/html")  return self._clipboardHtml.length > 0 ? 1 : 0;
                return 0;
            },

            read(mimeSvPtr, bufPtr, bufLen, bytesWrittenPtr) {
                const mime = self._readStringView(mimeSvPtr);
                let data = "";
                if (mime === "text/plain")      data = self._clipboardText;
                else if (mime === "text/html")  data = self._clipboardHtml;
                else return WAPI_ERR_NOENT;

                if (data.length === 0) return WAPI_ERR_NOENT;
                const written = self._writeString(bufPtr, bufLen, data);
                self._writeU32(bytesWrittenPtr, written);
                return WAPI_OK;
            },

            set(itemsPtr, count) {
                self._refreshViews();
                self._clipboardText = "";
                self._clipboardHtml = "";
                // wapi_clipboard_item_t is 32 bytes:
                //   Offset  0: wapi_stringview_t mime   (16 bytes)
                //   Offset 16: uint64_t           data  (8 bytes, linear memory address)
                //   Offset 24: wapi_size_t        data_len (4 bytes)
                //   Offset 28: uint32_t           _pad
                for (let i = 0; i < count; i++) {
                    const itemPtr = itemsPtr + i * 32;
                    const mime = self._readStringView(itemPtr);
                    const dataAddr = Number(self._dv.getBigUint64(itemPtr + 16, true));
                    const dataLen = self._dv.getUint32(itemPtr + 24, true);
                    const text = self._readString(dataAddr, dataLen);
                    if (mime === "text/plain") {
                        self._clipboardText = text;
                        if (navigator.clipboard && navigator.clipboard.writeText) {
                            navigator.clipboard.writeText(text).catch(() => {});
                        }
                    } else if (mime === "text/html") {
                        self._clipboardHtml = text;
                    }
                }
                return WAPI_OK;
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
                //   u32 weight_max(12), u32 supported_styles(16), i32 is_variable(20)
                // We can't write the family name pointer directly (it's in wasm memory)
                // For now, write zeros for the pointer fields and fill weight info
                self._refreshViews();
                self._writeU32(infoPtr + 0, 0);      // family ptr (can't set from here meaningfully)
                self._writeU32(infoPtr + 4, 0);      // family_len
                self._writeU32(infoPtr + 8, 100);    // weight_min
                self._writeU32(infoPtr + 12, 900);   // weight_max
                self._writeU32(infoPtr + 16, 0x0007); // NORMAL_BIT | ITALIC_BIT | OBLIQUE_BIT
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
            blit(video, tex, x, y, w, h) { return WAPI_ERR_NOTSUP; },
            bind_audio(video, stream) { return WAPI_ERR_NOTSUP; },
            set_volume(video, vol) { return WAPI_ERR_NOTSUP; },
            set_muted(video, muted) { return WAPI_ERR_NOTSUP; },
            set_loop(video, loop) { return WAPI_ERR_NOTSUP; },
            set_playback_rate(video, rate) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_module (runtime module linking - stub)
        // -------------------------------------------------------------------
        // Content-addressed module cache. prefetch() populates it in
        // the background; is_cached() polls; load() instantiates a
        // ready entry synchronously. Calls and shared-memory paths
        // remain NOTSUP — scope is bootstrap cache, not full runtime
        // linking.
        const wapi_module = {
            // (i32 hash_ptr, i32 url_sv_ptr, i32 module_out_ptr) -> i32
            //
            // wapi_string_view_t is passed by hidden byval pointer in
            // the wasm32 ABI, so url_sv_ptr is a single pointer to the
            // 16-byte struct, not a (data, len) pair.
            load(hashPtr, urlSvPtr, modulePtr) {
                self._refreshViews();
                const hashHex = _wapiReadModuleHash(self._u8, hashPtr);
                if (!hashHex) return WAPI_ERR_INVAL;

                const entry = _wapiModuleCache.get(hashHex);
                if (!entry || entry.state !== "ready") {
                    return WAPI_ERR_NOENT;
                }

                try {
                    const imports = self._buildImports();
                    // WebAssembly.instantiate is async when given a
                    // Module, but instantiate() with a compiled Module
                    // has a sync form: `new WebAssembly.Instance(...)`.
                    const inst = new WebAssembly.Instance(entry.module, imports);
                    const handle = self.handles.insert({
                        type: "module",
                        instance: inst,
                        module: entry.module,
                        hash: hashHex,
                    });
                    self._writeI32(modulePtr, handle);
                    return WAPI_OK;
                } catch (e) {
                    console.error("[WAPI] wapi_module.load instantiate failed:", e);
                    return WAPI_ERR_IO;
                }
            },
            get_func(mod, nameSvPtr, funcPtr) { return WAPI_ERR_NOTSUP; },
            get_desc(mod, descPtr) { return WAPI_ERR_NOTSUP; },
            get_hash(mod, hashPtr) {
                self._refreshViews();
                const entry = self.handles.get(mod);
                if (!entry || entry.type !== "module") return WAPI_ERR_INVAL;
                const hex = entry.hash;
                for (let i = 0; i < 32; i++) {
                    self._u8[hashPtr + i] = parseInt(hex.substr(i * 2, 2), 16);
                }
                return WAPI_OK;
            },
            release(mod) {
                const entry = self.handles.get(mod);
                if (!entry) return WAPI_OK;
                if (entry.type === "module") {
                    self.handles.remove(mod);
                } else if (entry.type === "service") {
                    // Notify SW to decrement refcount; drop local handle
                    // regardless of reply (fire-and-forget).
                    _wapiExtPost("services.release", { handle: entry.swHandle });
                    self.handles.remove(mod);
                }
                return WAPI_OK;
            },

            // (i32 hash_ptr, i32 url_sv_ptr, i32 name_sv_ptr, i32 module_out_ptr) -> i32
            //
            // Service-mode join. First call kicks off an async SW round
            // trip and returns WAPI_ERR_AGAIN. Caller polls by re-calling
            // with the same args until WAPI_OK is returned and *module_out_ptr
            // is populated with a service handle. See _wapiServiceJoins for
            // the state machine.
            join(hashPtr, urlSvPtr, nameSvPtr, modulePtr) {
                self._refreshViews();
                const hashHex = _wapiReadModuleHash(self._u8, hashPtr);
                if (!hashHex) return WAPI_ERR_INVAL;
                const url = urlSvPtr ? self._readStringView(urlSvPtr) : null;
                const name = nameSvPtr ? self._readStringView(nameSvPtr) : "";
                const key = hashHex + ":" + (name || "");

                const entry = _wapiServiceJoins.get(key);
                if (entry) {
                    if (entry.state === "pending") return WAPI_ERR_AGAIN;
                    if (entry.state === "failed") {
                        _wapiServiceJoins.delete(key);
                        return WAPI_ERR_IO;
                    }
                    // ready
                    const handle = self.handles.insert({
                        type: "service",
                        swHandle: entry.handle,
                        hash: hashHex,
                        name,
                    });
                    self._writeI32(modulePtr, handle);
                    _wapiServiceJoins.delete(key);
                    return WAPI_OK;
                }

                // Kick off SW round trip.
                _wapiServiceJoins.set(key, { state: "pending" });
                _wapiExtSend("services.join", { hashHex, name, url }).then(
                    (resp) => {
                        if (resp && typeof resp.handle === "number") {
                            _wapiServiceJoins.set(key, {
                                state: "ready",
                                handle: resp.handle,
                            });
                        } else {
                            _wapiServiceJoins.set(key, {
                                state: "failed",
                                error: (resp && resp.error) || "no response",
                            });
                        }
                    },
                    (err) => {
                        _wapiServiceJoins.set(key, {
                            state: "failed",
                            error: String(err && err.message || err),
                        });
                    },
                );
                return WAPI_ERR_AGAIN;
            },
            call(mod, func, argsPtr, nargs, resultsPtr, nresults) { return WAPI_ERR_NOTSUP; },
            // Shared memory
            shared_alloc(size, align) { return 0; },
            shared_free(offset) { return WAPI_ERR_NOTSUP; },
            shared_realloc(offset, newSize, align) { return 0; },
            shared_usable_size(offset) { return 0; },
            shared_read(srcOffset, dstPtr, len) { return WAPI_ERR_NOTSUP; },
            shared_write(dstOffset, srcPtr, len) { return WAPI_ERR_NOTSUP; },
            // Borrow system
            lend(mod, offset, flags, borrowPtr) { return WAPI_ERR_NOTSUP; },
            reclaim(borrow) { return WAPI_ERR_NOTSUP; },
            // Explicit copy
            copy_in(mod, srcPtr, len, childPtrOut) { return WAPI_ERR_NOTSUP; },

            // (i32 hash_ptr) -> i32
            is_cached(hashPtr) {
                self._refreshViews();
                const hashHex = _wapiReadModuleHash(self._u8, hashPtr);
                if (!hashHex) return 0;
                const entry = _wapiModuleCache.get(hashHex);
                return entry && entry.state === "ready" ? 1 : 0;
            },

            // (i32 hash_ptr, i32 url_sv_ptr) -> i32
            //
            // Non-blocking: kicks off the fetch+verify+compile in the
            // background and returns WAPI_OK immediately. Caller polls
            // is_cached(), then calls load() once ready. Re-entering
            // prefetch for a hash already pending or ready is a no-op.
            prefetch(hashPtr, urlSvPtr) {
                self._refreshViews();
                const hashHex = _wapiReadModuleHash(self._u8, hashPtr);
                if (!hashHex) return WAPI_ERR_INVAL;

                const existing = _wapiModuleCache.get(hashHex);
                if (existing && (existing.state === "ready" || existing.state === "pending")) {
                    return WAPI_OK;
                }

                const url = urlSvPtr ? self._readStringView(urlSvPtr) : null;
                const promise = _wapiFetchAndCompile(hashHex, url).then(
                    (mod) => {
                        _wapiModuleCache.set(hashHex, {
                            state: "ready",
                            module: mod,
                            hash: hashHex,
                        });
                    },
                    (err) => {
                        console.error(`[WAPI] prefetch ${hashHex} failed:`, err);
                        _wapiModuleCache.set(hashHex, {
                            state: "failed",
                            error: String(err && err.message || err),
                        });
                    },
                );
                _wapiModuleCache.set(hashHex, { state: "pending", promise });
                return WAPI_OK;
            },
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
            scheme_add(schemeSvPtr) { return WAPI_ERR_NOTSUP; },
            scheme_remove(schemeSvPtr) { return WAPI_ERR_NOTSUP; },
            filetype_add(descPtr) { return WAPI_ERR_NOTSUP; },
            filetype_remove(extSvPtr) { return WAPI_ERR_NOTSUP; },
            preview_add(extSvPtr) { return WAPI_ERR_NOTSUP; },
            scheme_isdefault(schemeSvPtr) { return 0; },
            filetype_isdefault(extSvPtr) { return 0; },
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
            query_decode(queryPtr, resultPtr) { return WAPI_ERR_NOTSUP; },
            query_encode(queryPtr, resultPtr) { return WAPI_ERR_NOTSUP; },
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
        // wapi_power (battery, wake lock, idle, saver, thermal - stubs)
        // -------------------------------------------------------------------
        const wapi_power = {
            get_info(infoPtr) { return WAPI_ERR_NOTSUP; },
            wake_acquire(type, lockPtr) { return WAPI_ERR_NOTSUP; },
            wake_release(lock) { return WAPI_ERR_NOTSUP; },
            idle_start(thresholdMs) { return WAPI_ERR_NOTSUP; },
            idle_stop() { return WAPI_ERR_NOTSUP; },
            idle_get(statePtr) { return WAPI_ERR_NOTSUP; },
            saver_get(statePtr) { return WAPI_ERR_NOTSUP; },
            thermal_get(statePtr) { return WAPI_ERR_NOTSUP; },
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
            icon_read(iconHandle, bufPtr, bufLen, outLenPtr) { return WAPI_ERR_NOTSUP; },
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

        // -------------------------------------------------------------------
        // wapi_window (OS window management - stub)
        // -------------------------------------------------------------------
        const wapi_window = {
            set_title(surface, titlePtr, titleLen) { return WAPI_ERR_NOTSUP; },
            get_size_logical(surface, widthPtr, heightPtr) { return WAPI_ERR_NOTSUP; },
            set_fullscreen(surface, fullscreen) { return WAPI_ERR_NOTSUP; },
            set_visible(surface, visible) { return WAPI_ERR_NOTSUP; },
            minimize(surface) { return WAPI_ERR_NOTSUP; },
            maximize(surface) { return WAPI_ERR_NOTSUP; },
            restore(surface) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_display (display enumeration - stub)
        // -------------------------------------------------------------------
        const wapi_display = {
            display_count() { return 0; },
            display_get_info(index, infoPtr) { return WAPI_ERR_NOTSUP; },
            display_get_subpixels(index, subpixelsPtr, maxCount, countPtr) { return WAPI_ERR_NOTSUP; },
            display_get_usable_bounds(index, xPtr, yPtr, wPtr, hPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_dialog (native file/message/color/font dialogs - stub)
        // -------------------------------------------------------------------
        const wapi_dialog = {
            open_file(filtersPtr, filterCount, defPathPtr, defPathLen, flags, bufPtr, bufLen, resultLenPtr) { return WAPI_ERR_NOTSUP; },
            save_file(filtersPtr, filterCount, defPathPtr, defPathLen, bufPtr, bufLen, resultLenPtr) { return WAPI_ERR_NOTSUP; },
            open_folder(defPathPtr, defPathLen, bufPtr, bufLen, resultLenPtr) { return WAPI_ERR_NOTSUP; },
            message_box(type, titlePtr, titleLen, msgPtr, msgLen, buttons, resultPtr) { return WAPI_ERR_NOTSUP; },
            simple_message_box(titlePtr, titleLen, msgPtr, msgLen) { return WAPI_ERR_NOTSUP; },
            pick_color(titlePtr, titleLen, initialRgba, flags, resultRgbaPtr) { return WAPI_ERR_NOTSUP; },
            pick_font(titlePtr, titleLen, ioPtr, nameBufPtr, nameCap) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_menu (native menus - stub)
        // -------------------------------------------------------------------
        const wapi_menu = {
            menu_create(outHandlePtr) { return WAPI_ERR_NOTSUP; },
            menu_add_item(menu, itemPtr) { return WAPI_ERR_NOTSUP; },
            menu_add_submenu(menu, labelPtr, labelLen, submenu) { return WAPI_ERR_NOTSUP; },
            menu_show_context(menu, surface, x, y) { return WAPI_ERR_NOTSUP; },
            menu_set_bar(surface, menu) { return WAPI_ERR_NOTSUP; },
            menu_destroy(menu) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_tray (system tray - stub)
        // -------------------------------------------------------------------
        const wapi_tray = {
            tray_create(iconDataPtr, iconLen, tooltipPtr, tooltipLen, outHandlePtr) { return WAPI_ERR_NOTSUP; },
            tray_destroy(handle) { return WAPI_ERR_NOTSUP; },
            tray_set_icon(handle, iconDataPtr, iconLen) { return WAPI_ERR_NOTSUP; },
            tray_set_tooltip(handle, textPtr, textLen) { return WAPI_ERR_NOTSUP; },
            tray_set_menu(handle, itemsPtr, itemCount) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_theme (system appearance preferences)
        // -------------------------------------------------------------------
        const wapi_theme = {
            theme_is_dark() {
                return (typeof matchMedia === "function" &&
                        matchMedia("(prefers-color-scheme: dark)").matches) ? 1 : 0;
            },
            theme_get_accent_color(rgbaPtr) { return WAPI_ERR_NOTSUP; },
            theme_get_contrast_preference() {
                if (typeof matchMedia !== "function") return 0;
                if (matchMedia("(prefers-contrast: more)").matches) return 1;
                if (matchMedia("(prefers-contrast: less)").matches) return 2;
                return 0;
            },
            theme_get_reduced_motion() {
                return (typeof matchMedia === "function" &&
                        matchMedia("(prefers-reduced-motion: reduce)").matches) ? 1 : 0;
            },
            theme_get_font_scale(scalePtr) {
                self._refreshViews();
                self._dv.setFloat32(scalePtr, 1.0, true);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_sysinfo (system/platform information)
        // -------------------------------------------------------------------
        const wapi_sysinfo = {
            sysinfo_get(infoPtr) { return WAPI_ERR_NOTSUP; },
            sysinfo_get_locale(bufPtr, bufLen, lenPtr) {
                const locale = (typeof navigator !== "undefined" && navigator.language) || "en-US";
                const written = self._writeString(bufPtr, bufLen, locale);
                self._writeU32(lenPtr, written);
                return WAPI_OK;
            },
            sysinfo_get_timezone(bufPtr, bufLen, lenPtr) {
                let tz = "UTC";
                try { tz = Intl.DateTimeFormat().resolvedOptions().timeZone || "UTC"; } catch (_) {}
                const written = self._writeString(bufPtr, bufLen, tz);
                self._writeU32(lenPtr, written);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_process (subprocess spawn - stub, not available in browser)
        // -------------------------------------------------------------------
        const wapi_process = {
            create(descPtr, processPtr) { return WAPI_ERR_NOTSUP; },
            get_stdin(process, pipePtr) { return WAPI_ERR_NOTSUP; },
            get_stdout(process, pipePtr) { return WAPI_ERR_NOTSUP; },
            get_stderr(process, pipePtr) { return WAPI_ERR_NOTSUP; },
            pipe_write(pipe, bufPtr, len, writtenPtr) { return WAPI_ERR_NOTSUP; },
            pipe_read(pipe, bufPtr, len, bytesReadPtr) { return WAPI_ERR_NOTSUP; },
            pipe_close(pipe) { return WAPI_ERR_NOTSUP; },
            wait(process, block, exitCodePtr) { return WAPI_ERR_NOTSUP; },
            kill(process) { return WAPI_ERR_NOTSUP; },
            destroy(process) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_thread (threads and sync primitives - stub, needs SAB + workers)
        // -------------------------------------------------------------------
        const wapi_thread = {
            create(descPtr, threadPtr) { return WAPI_ERR_NOTSUP; },
            join(thread, exitCodePtr) { return WAPI_ERR_NOTSUP; },
            detach(thread) { return WAPI_ERR_NOTSUP; },
            get_state(thread, statePtr) { return WAPI_ERR_NOTSUP; },
            current_id() { return 1n; },
            get_id(thread) { return 0n; },
            set_qos(qos) { return WAPI_ERR_NOTSUP; },
            tls_create(destructor, slotPtr) { return WAPI_ERR_NOTSUP; },
            tls_destroy(slot) { return WAPI_ERR_NOTSUP; },
            tls_set(slot, value) { return WAPI_ERR_NOTSUP; },
            tls_get(slot) { return 0; },
            mutex_create(mutexPtr) { return WAPI_ERR_NOTSUP; },
            mutex_destroy(mutex) { return WAPI_ERR_NOTSUP; },
            mutex_lock(mutex) { return WAPI_ERR_NOTSUP; },
            mutex_try_lock(mutex) { return WAPI_ERR_NOTSUP; },
            mutex_unlock(mutex) { return WAPI_ERR_NOTSUP; },
            rwlock_create(rwlockPtr) { return WAPI_ERR_NOTSUP; },
            rwlock_destroy(rwlock) { return WAPI_ERR_NOTSUP; },
            rwlock_read_lock(rwlock) { return WAPI_ERR_NOTSUP; },
            rwlock_try_read_lock(rwlock) { return WAPI_ERR_NOTSUP; },
            rwlock_write_lock(rwlock) { return WAPI_ERR_NOTSUP; },
            rwlock_try_write_lock(rwlock) { return WAPI_ERR_NOTSUP; },
            rwlock_unlock(rwlock) { return WAPI_ERR_NOTSUP; },
            sem_create(initialValue, semPtr) { return WAPI_ERR_NOTSUP; },
            sem_destroy(sem) { return WAPI_ERR_NOTSUP; },
            sem_wait(sem) { return WAPI_ERR_NOTSUP; },
            sem_try_wait(sem) { return WAPI_ERR_NOTSUP; },
            sem_wait_timeout(sem, timeoutNs) { return WAPI_ERR_NOTSUP; },
            sem_signal(sem) { return WAPI_ERR_NOTSUP; },
            sem_get_value(sem) { return 0; },
            cond_create(condPtr) { return WAPI_ERR_NOTSUP; },
            cond_destroy(cond) { return WAPI_ERR_NOTSUP; },
            cond_wait(cond, mutex) { return WAPI_ERR_NOTSUP; },
            cond_wait_timeout(cond, mutex, timeoutNs) { return WAPI_ERR_NOTSUP; },
            cond_signal(cond) { return WAPI_ERR_NOTSUP; },
            cond_broadcast(cond) { return WAPI_ERR_NOTSUP; },
            barrier_create(count, barrierPtr) { return WAPI_ERR_NOTSUP; },
            barrier_destroy(barrier) { return WAPI_ERR_NOTSUP; },
            barrier_wait(barrier) { return WAPI_ERR_NOTSUP; },
            call_once(onceFlagPtr, initFunc) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_fwatch (filewatcher - stub)
        // -------------------------------------------------------------------
        const wapi_fwatch = {
            fwatch_add(pathPtr, pathLen, recursive, outHandlePtr) { return WAPI_ERR_NOTSUP; },
            fwatch_remove(handle) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_eyedrop (screen color picker - stub)
        // -------------------------------------------------------------------
        const wapi_eyedrop = {
            eyedropper_pick(rgbaPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_plugin (audio plugin host API - stub)
        // -------------------------------------------------------------------
        const wapi_plugin = {
            param_set(paramId, value) { return WAPI_ERR_NOTSUP; },
            param_get(paramId) { return 0.0; },
            request_gui_resize(width, height) { return WAPI_ERR_NOTSUP; },
            send_midi(status, data1, data2) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wasi_snapshot_preview1 — WASI compatibility layer
        // -------------------------------------------------------------------
        // Maps WASI preview1 calls to the existing WAPI MemFS, HandleTable,
        // env, and clock implementations. Enables languages that target WASI
        // (C#/NativeAOT, Rust, Go, etc.) to run on the WAPI browser shim.
        //
        // WASI errno values (different from WAPI error codes):
        const WASI_ESUCCESS     = 0;
        const WASI_E2BIG        = 1;
        const WASI_EACCES       = 2;
        const WASI_EBADF        = 8;
        const WASI_EEXIST       = 20;
        const WASI_EINVAL       = 28;
        const WASI_EISDIR       = 31;
        const WASI_ENOENT       = 44;
        const WASI_ENOSYS       = 52;
        const WASI_ENOTDIR      = 54;
        const WASI_ENOTEMPTY    = 55;
        const WASI_EOVERFLOW    = 61;

        // WASI file types
        const WASI_FILETYPE_UNKNOWN          = 0;
        const WASI_FILETYPE_BLOCK_DEVICE     = 1;
        const WASI_FILETYPE_CHARACTER_DEVICE = 2;
        const WASI_FILETYPE_DIRECTORY        = 3;
        const WASI_FILETYPE_REGULAR_FILE     = 4;
        const WASI_FILETYPE_SOCKET_DGRAM     = 5;
        const WASI_FILETYPE_SOCKET_STREAM    = 6;
        const WASI_FILETYPE_SYMBOLIC_LINK    = 7;

        // WASI preopened directory type
        const WASI_PREOPENTYPE_DIR = 0;

        // WASI whence
        const WASI_WHENCE_SET = 0;
        const WASI_WHENCE_CUR = 1;
        const WASI_WHENCE_END = 2;

        // WASI oflags
        const WASI_OFLAG_CREAT   = 0x0001;
        const WASI_OFLAG_DIRECTORY = 0x0002;
        const WASI_OFLAG_EXCL    = 0x0004;
        const WASI_OFLAG_TRUNC   = 0x0008;

        // WASI clock IDs
        const WASI_CLOCK_REALTIME           = 0;
        const WASI_CLOCK_MONOTONIC          = 1;
        const WASI_CLOCK_PROCESS_CPUTIME_ID = 2;
        const WASI_CLOCK_THREAD_CPUTIME_ID  = 3;

        // Helper: read WASI iovec array { buf: i32, buf_len: i32 } and return total bytes + ranges
        const readIovecs = (iovsPtr, iovsLen) => {
            const vecs = [];
            for (let i = 0; i < iovsLen; i++) {
                const buf = self._readU32(iovsPtr + i * 8);
                const len = self._readU32(iovsPtr + i * 8 + 4);
                vecs.push({ buf, len });
            }
            return vecs;
        };

        // Helper: get FDEntry from WAPI handle table (handles STDIN=0, STDOUT=1, STDERR=2 in WASI numbering)
        // WASI uses 0=stdin, 1=stdout, 2=stderr; WAPI uses 1=stdin, 2=stdout, 3=stderr
        // Preopens start at fd=3 in WASI, fd=4 in WAPI
        const wasiFdToWapi = (wasiFd) => {
            if (wasiFd <= 2) return wasiFd + 1; // 0->1(stdin), 1->2(stdout), 2->3(stderr)
            return wasiFd + 1; // 3->4, 4->5, etc. (preopens)
        };

        // Helper: get node type as WASI filetype
        const wasiFileType = (node) => {
            if (!node) return WASI_FILETYPE_UNKNOWN;
            if (node.type === WAPI_FILETYPE_DIRECTORY) return WASI_FILETYPE_DIRECTORY;
            if (node.type === WAPI_FILETYPE_REGULAR) return WASI_FILETYPE_REGULAR_FILE;
            return WASI_FILETYPE_UNKNOWN;
        };

        // Helper: write WASI filestat (64 bytes) to memory
        // struct __wasi_filestat_t {
        //   dev: u64, ino: u64, filetype: u8, _pad: 7 bytes,
        //   nlink: u64, size: u64, atim: u64, mtim: u64, ctim: u64
        // }
        const writeWasiFilestat = (ptr, node) => {
            self._refreshViews();
            const dv = self._dv;
            dv.setBigUint64(ptr +  0, 0n, true);                       // dev
            dv.setBigUint64(ptr +  8, BigInt(node.ino), true);         // ino
            dv.setUint8(ptr + 16, wasiFileType(node));                  // filetype
            // 7 bytes padding (17-23)
            for (let i = 17; i < 24; i++) self._u8[i + ptr - ptr] = 0;
            self._u8[ptr + 17] = 0; self._u8[ptr + 18] = 0; self._u8[ptr + 19] = 0;
            self._u8[ptr + 20] = 0; self._u8[ptr + 21] = 0; self._u8[ptr + 22] = 0;
            self._u8[ptr + 23] = 0;
            dv.setBigUint64(ptr + 24, 1n, true);                       // nlink
            const sz = node.data ? BigInt(node.data.length) : 0n;
            dv.setBigUint64(ptr + 32, sz, true);                       // size
            const tim = BigInt(node.mtime) * 1_000_000n;
            dv.setBigUint64(ptr + 40, tim, true);                      // atim
            dv.setBigUint64(ptr + 48, tim, true);                      // mtim
            dv.setBigUint64(ptr + 56, BigInt(node.ctime) * 1_000_000n, true); // ctim
        };

        // Helper: resolve a path relative to a WASI dir fd
        const wasiResolvePath = (wasiFd, pathPtr, pathLen) => {
            const relPath = self._readString(pathPtr, pathLen);
            const wapiFd = wasiFdToWapi(wasiFd);
            const entry = self.handles.get(wapiFd);
            if (!entry || !entry.path) return null;
            return self.memfs._normPath(entry.path, relPath);
        };

        const wasi_snapshot_preview1 = {
            // --- Args ---
            // (argv_ptr: i32, argv_buf_ptr: i32) -> errno
            args_get(argvPtr, argvBufPtr) {
                self._refreshViews();
                let bufOffset = argvBufPtr;
                for (let i = 0; i < self._args.length; i++) {
                    self._writeU32(argvPtr + i * 4, bufOffset);
                    const encoded = new TextEncoder().encode(self._args[i]);
                    self._u8.set(encoded, bufOffset);
                    self._u8[bufOffset + encoded.length] = 0; // null terminator
                    bufOffset += encoded.length + 1;
                }
                return WASI_ESUCCESS;
            },

            // (argc_ptr: i32, argv_buf_size_ptr: i32) -> errno
            args_sizes_get(argcPtr, argvBufSizePtr) {
                self._refreshViews();
                self._writeU32(argcPtr, self._args.length);
                let totalSize = 0;
                for (const arg of self._args) {
                    totalSize += new TextEncoder().encode(arg).length + 1; // +1 for null
                }
                self._writeU32(argvBufSizePtr, totalSize);
                return WASI_ESUCCESS;
            },

            // --- Environment ---
            // (environ_ptr: i32, environ_buf_ptr: i32) -> errno
            environ_get(environPtr, environBufPtr) {
                self._refreshViews();
                const entries = Object.entries(self._env);
                let bufOffset = environBufPtr;
                for (let i = 0; i < entries.length; i++) {
                    self._writeU32(environPtr + i * 4, bufOffset);
                    const str = `${entries[i][0]}=${entries[i][1]}`;
                    const encoded = new TextEncoder().encode(str);
                    self._u8.set(encoded, bufOffset);
                    self._u8[bufOffset + encoded.length] = 0;
                    bufOffset += encoded.length + 1;
                }
                return WASI_ESUCCESS;
            },

            // (environ_count_ptr: i32, environ_buf_size_ptr: i32) -> errno
            environ_sizes_get(environCountPtr, environBufSizePtr) {
                self._refreshViews();
                const entries = Object.entries(self._env);
                self._writeU32(environCountPtr, entries.length);
                let totalSize = 0;
                for (const [k, v] of entries) {
                    totalSize += new TextEncoder().encode(`${k}=${v}`).length + 1;
                }
                self._writeU32(environBufSizePtr, totalSize);
                return WASI_ESUCCESS;
            },

            // --- Clock ---
            // (id: i32, precision: i64, time_ptr: i32) -> errno
            clock_time_get(id, precisionLo, precisionHi, timePtr) {
                self._refreshViews();
                // Handle i64 arg splitting: if 3 args, timePtr is arg 2
                if (timePtr === undefined) { timePtr = precisionHi; }
                let ns;
                if (id === WASI_CLOCK_REALTIME) {
                    ns = BigInt(Date.now()) * 1_000_000n;
                } else {
                    // MONOTONIC, PROCESS_CPUTIME, THREAD_CPUTIME all use perf.now
                    ns = BigInt(Math.round(performance.now() * 1_000_000));
                }
                self._dv.setBigUint64(timePtr, ns, true);
                return WASI_ESUCCESS;
            },

            // (id: i32, resolution_ptr: i32) -> errno
            clock_res_get(id, resPtr) {
                self._refreshViews();
                const res = (id === WASI_CLOCK_REALTIME) ? 1_000_000n : 1_000n;
                self._dv.setBigUint64(resPtr, res, true);
                return WASI_ESUCCESS;
            },

            // --- File Descriptor operations ---

            // (fd: i32, iovs: i32, iovs_len: i32, nread_ptr: i32) -> errno
            fd_read(fd, iovsPtr, iovsLen, nreadPtr) {
                self._refreshViews();
                const wapiFd = wasiFdToWapi(fd);
                if (wapiFd === WAPI_STDIN) {
                    // stdin: return 0 bytes
                    self._writeU32(nreadPtr, 0);
                    return WASI_ESUCCESS;
                }
                const entry = self.handles.get(wapiFd);
                if (!entry || !entry.node) return WASI_EBADF;
                if (entry.node.type === WAPI_FILETYPE_DIRECTORY) return WASI_EISDIR;

                const vecs = readIovecs(iovsPtr, iovsLen);
                let totalRead = 0;
                const data = entry.node.data;

                for (const { buf, len } of vecs) {
                    if (entry.position >= data.length) break;
                    const avail = Math.min(len, data.length - entry.position);
                    self._u8.set(data.subarray(entry.position, entry.position + avail), buf);
                    entry.position += avail;
                    totalRead += avail;
                    if (avail < len) break;
                }

                self._writeU32(nreadPtr, totalRead);
                return WASI_ESUCCESS;
            },

            // (fd: i32, iovs: i32, iovs_len: i32, nwritten_ptr: i32) -> errno
            fd_write(fd, iovsPtr, iovsLen, nwrittenPtr) {
                self._refreshViews();
                const wapiFd = wasiFdToWapi(fd);
                const vecs = readIovecs(iovsPtr, iovsLen);

                // stdout/stderr → console
                if (wapiFd === WAPI_STDOUT || wapiFd === WAPI_STDERR) {
                    let totalWritten = 0;
                    let text = "";
                    for (const { buf, len } of vecs) {
                        text += new TextDecoder().decode(self._u8.subarray(buf, buf + len));
                        totalWritten += len;
                    }
                    if (wapiFd === WAPI_STDERR) {
                        console.error(text);
                    } else {
                        console.log(text);
                    }
                    self._writeU32(nwrittenPtr, totalWritten);
                    return WASI_ESUCCESS;
                }

                const entry = self.handles.get(wapiFd);
                if (!entry || !entry.node) return WASI_EBADF;
                if (entry.node.type === WAPI_FILETYPE_DIRECTORY) return WASI_EISDIR;

                let totalWritten = 0;
                for (const { buf, len } of vecs) {
                    const needed = entry.position + len;
                    if (needed > entry.node.data.length) {
                        const newData = new Uint8Array(needed);
                        newData.set(entry.node.data);
                        entry.node.data = newData;
                    }
                    entry.node.data.set(self._u8.subarray(buf, buf + len), entry.position);
                    entry.position += len;
                    totalWritten += len;
                    entry.node.mtime = Date.now();
                }

                self._writeU32(nwrittenPtr, totalWritten);
                return WASI_ESUCCESS;
            },

            // (fd: i32, iovs: i32, iovs_len: i32, offset: i64, nread_ptr: i32) -> errno
            fd_pread(fd, iovsPtr, iovsLen, offsetLo, offsetHi, nreadPtr) {
                self._refreshViews();
                if (nreadPtr === undefined) { nreadPtr = offsetHi; offsetHi = 0; }
                const wapiFd = wasiFdToWapi(fd);
                const entry = self.handles.get(wapiFd);
                if (!entry || !entry.node) return WASI_EBADF;

                const offset = Number(offsetLo);
                const vecs = readIovecs(iovsPtr, iovsLen);
                let totalRead = 0;
                let pos = offset;
                const data = entry.node.data;

                for (const { buf, len } of vecs) {
                    if (pos >= data.length) break;
                    const avail = Math.min(len, data.length - pos);
                    self._u8.set(data.subarray(pos, pos + avail), buf);
                    pos += avail;
                    totalRead += avail;
                    if (avail < len) break;
                }

                self._writeU32(nreadPtr, totalRead);
                return WASI_ESUCCESS;
            },

            // (fd: i32, iovs: i32, iovs_len: i32, offset: i64, nwritten_ptr: i32) -> errno
            fd_pwrite(fd, iovsPtr, iovsLen, offsetLo, offsetHi, nwrittenPtr) {
                self._refreshViews();
                if (nwrittenPtr === undefined) { nwrittenPtr = offsetHi; offsetHi = 0; }
                const wapiFd = wasiFdToWapi(fd);
                const entry = self.handles.get(wapiFd);
                if (!entry || !entry.node) return WASI_EBADF;

                let pos = Number(offsetLo);
                const vecs = readIovecs(iovsPtr, iovsLen);
                let totalWritten = 0;

                for (const { buf, len } of vecs) {
                    const needed = pos + len;
                    if (needed > entry.node.data.length) {
                        const newData = new Uint8Array(needed);
                        newData.set(entry.node.data);
                        entry.node.data = newData;
                    }
                    entry.node.data.set(self._u8.subarray(buf, buf + len), pos);
                    pos += len;
                    totalWritten += len;
                    entry.node.mtime = Date.now();
                }

                self._writeU32(nwrittenPtr, totalWritten);
                return WASI_ESUCCESS;
            },

            // (fd: i32, offset: i64, whence: i32, newoffset_ptr: i32) -> errno
            fd_seek(fd, offsetLo, offsetHi, whence, newoffsetPtr) {
                self._refreshViews();
                // Handle i64 arg splitting
                if (newoffsetPtr === undefined) { newoffsetPtr = whence; whence = offsetHi; }
                const wapiFd = wasiFdToWapi(fd);
                // Allow seek on stdout/stderr (just return 0)
                if (wapiFd === WAPI_STDOUT || wapiFd === WAPI_STDERR) {
                    self._dv.setBigUint64(newoffsetPtr, 0n, true);
                    return WASI_ESUCCESS;
                }
                const entry = self.handles.get(wapiFd);
                if (!entry || !entry.node) return WASI_EBADF;

                const offset = Number(offsetLo);
                let newPos;
                switch (whence) {
                    case WASI_WHENCE_SET: newPos = offset; break;
                    case WASI_WHENCE_CUR: newPos = entry.position + offset; break;
                    case WASI_WHENCE_END:
                        newPos = (entry.node.data ? entry.node.data.length : 0) + offset;
                        break;
                    default: return WASI_EINVAL;
                }
                if (newPos < 0) return WASI_EINVAL;
                entry.position = newPos;
                self._dv.setBigUint64(newoffsetPtr, BigInt(newPos), true);
                return WASI_ESUCCESS;
            },

            // (fd: i32) -> errno
            fd_close(fd) {
                const wapiFd = wasiFdToWapi(fd);
                if (wapiFd <= 3) return WASI_ESUCCESS; // don't close stdio
                if (!self.handles.has(wapiFd)) return WASI_EBADF;
                self.handles.remove(wapiFd);
                return WASI_ESUCCESS;
            },

            // (fd: i32) -> errno
            fd_sync(fd) {
                return WASI_ESUCCESS; // no-op for in-memory FS
            },

            // (fd: i32) -> errno
            fd_datasync(fd) {
                return WASI_ESUCCESS;
            },

            // (fd: i32, offset: i64, len: i64, advice: i32) -> errno
            fd_advise(fd, offsetLo, offsetHi, lenLo, lenHi, advice) {
                return WASI_ESUCCESS; // no-op
            },

            // (fd: i32, offset: i64, len: i64) -> errno
            fd_allocate(fd, offsetLo, offsetHi, lenLo, lenHi) {
                return WASI_ESUCCESS; // no-op
            },

            // (fd: i32, buf_ptr: i32) -> errno
            // Writes __wasi_fdstat_t (24 bytes):
            //   filetype: u8, fdflags: u16 (at offset 2), rights_base: u64, rights_inheriting: u64
            fd_fdstat_get(fd, bufPtr) {
                self._refreshViews();
                const wapiFd = wasiFdToWapi(fd);
                let filetype = WASI_FILETYPE_UNKNOWN;
                let fdflags = 0;

                if (wapiFd === WAPI_STDIN) {
                    filetype = WASI_FILETYPE_CHARACTER_DEVICE;
                } else if (wapiFd === WAPI_STDOUT || wapiFd === WAPI_STDERR) {
                    filetype = WASI_FILETYPE_CHARACTER_DEVICE;
                    fdflags = 0x0001; // append
                } else {
                    const entry = self.handles.get(wapiFd);
                    if (!entry) return WASI_EBADF;
                    if (entry.node) {
                        filetype = wasiFileType(entry.node);
                    } else if (entry.path !== undefined) {
                        filetype = WASI_FILETYPE_DIRECTORY; // preopen
                    }
                }

                self._u8[bufPtr + 0] = filetype;
                self._u8[bufPtr + 1] = 0;
                self._dv.setUint16(bufPtr + 2, fdflags, true);
                // Grant all rights
                self._dv.setBigUint64(bufPtr + 8, 0xFFFFFFFFFFFFFFFFn, true);  // rights_base
                self._dv.setBigUint64(bufPtr + 16, 0xFFFFFFFFFFFFFFFFn, true); // rights_inheriting
                return WASI_ESUCCESS;
            },

            // (fd: i32, fdflags: i32) -> errno
            fd_fdstat_set_flags(fd, fdflags) {
                return WASI_ESUCCESS; // no-op
            },

            // (fd: i32, rights_base: i64, rights_inheriting: i64) -> errno
            fd_fdstat_set_rights(fd, rbLo, rbHi, riLo, riHi) {
                return WASI_ESUCCESS; // no-op, we grant all rights
            },

            // (fd: i32, buf_ptr: i32) -> errno
            // Writes __wasi_filestat_t (64 bytes)
            fd_filestat_get(fd, bufPtr) {
                self._refreshViews();
                const wapiFd = wasiFdToWapi(fd);
                if (wapiFd <= 3) {
                    // stdio: write zeroed stat with character device type
                    for (let i = 0; i < 64; i++) self._u8[bufPtr + i] = 0;
                    self._u8[bufPtr + 16] = WASI_FILETYPE_CHARACTER_DEVICE;
                    return WASI_ESUCCESS;
                }
                const entry = self.handles.get(wapiFd);
                if (!entry || !entry.node) return WASI_EBADF;
                writeWasiFilestat(bufPtr, entry.node);
                return WASI_ESUCCESS;
            },

            // (fd: i32, size: i64) -> errno
            fd_filestat_set_size(fd, sizeLo, sizeHi) {
                const wapiFd = wasiFdToWapi(fd);
                const entry = self.handles.get(wapiFd);
                if (!entry || !entry.node) return WASI_EBADF;
                const newSize = Number(sizeLo);
                const newData = new Uint8Array(newSize);
                if (entry.node.data) {
                    newData.set(entry.node.data.subarray(0, Math.min(entry.node.data.length, newSize)));
                }
                entry.node.data = newData;
                entry.node.mtime = Date.now();
                return WASI_ESUCCESS;
            },

            // (fd: i32, atim: i64, mtim: i64, fst_flags: i32) -> errno
            fd_filestat_set_times(fd, atimLo, atimHi, mtimLo, mtimHi, fstFlags) {
                // Simplified: just update mtime to now
                const wapiFd = wasiFdToWapi(fd);
                const entry = self.handles.get(wapiFd);
                if (entry && entry.node) entry.node.mtime = Date.now();
                return WASI_ESUCCESS;
            },

            // (fd: i32, buf: i32, buf_len: i32, cookie: i64, bufused_ptr: i32) -> errno
            fd_readdir(fd, bufPtr, bufLen, cookieLo, cookieHi, bufusedPtr) {
                self._refreshViews();
                if (bufusedPtr === undefined) { bufusedPtr = cookieHi; }
                const wapiFd = wasiFdToWapi(fd);
                const entry = self.handles.get(wapiFd);
                if (!entry) return WASI_EBADF;
                const node = entry.node;
                if (!node || node.type !== WAPI_FILETYPE_DIRECTORY) return WASI_ENOTDIR;

                const cookie = Number(cookieLo);
                const children = Array.from(node.children.entries());
                let offset = 0;

                for (let i = cookie; i < children.length && offset < bufLen; i++) {
                    const [name, child] = children[i];
                    const nameBytes = new TextEncoder().encode(name);
                    // WASI dirent: d_next(u64) + d_ino(u64) + d_namlen(u32) + d_type(u8) + padding(3) = 24 bytes + name
                    const entrySize = 24 + nameBytes.length;
                    const writeSize = Math.min(entrySize, bufLen - offset);

                    if (writeSize >= 24) {
                        const p = bufPtr + offset;
                        self._dv.setBigUint64(p, BigInt(i + 1), true);     // d_next
                        self._dv.setBigUint64(p + 8, BigInt(child.ino), true); // d_ino
                        self._dv.setUint32(p + 16, nameBytes.length, true); // d_namlen
                        self._u8[p + 20] = wasiFileType(child);            // d_type
                        self._u8[p + 21] = 0; self._u8[p + 22] = 0; self._u8[p + 23] = 0; // padding
                        // Write name (may be truncated)
                        const nameCopy = Math.min(nameBytes.length, writeSize - 24);
                        if (nameCopy > 0) self._u8.set(nameBytes.subarray(0, nameCopy), p + 24);
                    }
                    offset += entrySize; // report full size even if truncated
                }

                self._writeU32(bufusedPtr, Math.min(offset, bufLen));
                return WASI_ESUCCESS;
            },

            // (fd: i32, buf_ptr: i32) -> errno
            // Writes __wasi_prestat_t: tag(u8) + pad(3) + dir_namelen(u32) = 8 bytes
            fd_prestat_get(fd, bufPtr) {
                self._refreshViews();
                // WASI preopens are at fd 3, 4, 5... mapping to WAPI 4, 5, 6...
                const preIdx = fd - 3;
                if (preIdx < 0 || preIdx >= self._preopens.length) return WASI_EBADF;
                const pathBytes = new TextEncoder().encode(self._preopens[preIdx].path);
                self._writeU32(bufPtr, WASI_PREOPENTYPE_DIR);      // tag
                self._writeU32(bufPtr + 4, pathBytes.length);       // dir name length
                return WASI_ESUCCESS;
            },

            // (fd: i32, path_ptr: i32, path_len: i32) -> errno
            fd_prestat_dir_name(fd, pathPtr, pathLen) {
                self._refreshViews();
                const preIdx = fd - 3;
                if (preIdx < 0 || preIdx >= self._preopens.length) return WASI_EBADF;
                const pathBytes = new TextEncoder().encode(self._preopens[preIdx].path);
                const copyLen = Math.min(pathLen, pathBytes.length);
                self._u8.set(pathBytes.subarray(0, copyLen), pathPtr);
                return WASI_ESUCCESS;
            },

            // (fd: i32) -> errno
            fd_renumber(from, to) {
                const wapiFrom = wasiFdToWapi(from);
                const wapiTo = wasiFdToWapi(to);
                const entry = self.handles.get(wapiFrom);
                if (!entry) return WASI_EBADF;
                self.handles._map.set(wapiTo, entry);
                self.handles._map.delete(wapiFrom);
                return WASI_ESUCCESS;
            },

            // (fd: i32, offset: i64, whence: i32, newoffset_ptr: i32) -> errno
            fd_tell(fd, offsetPtr) {
                self._refreshViews();
                const wapiFd = wasiFdToWapi(fd);
                const entry = self.handles.get(wapiFd);
                if (!entry) return WASI_EBADF;
                self._dv.setBigUint64(offsetPtr, BigInt(entry.position || 0), true);
                return WASI_ESUCCESS;
            },

            // --- Path operations ---

            // (fd: i32, dirflags: i32, path: i32, path_len: i32, oflags: i32,
            //  fs_rights_base: i64, fs_rights_inheriting: i64, fdflags: i32, fd_ptr: i32) -> errno
            path_open(fd, dirflags, pathPtr, pathLen, oflags, rbLo, rbHi, riLo, riHi, fdflags, fdOutPtr) {
                self._refreshViews();
                // Handle i64 arg splitting — rights are i64 and may be split
                // Depending on the wasm runtime, i64 args may arrive as BigInt or split
                // For WASI, rights_base and rights_inheriting are u64
                // Simplified: ignore rights, just use oflags and fdflags
                if (fdOutPtr === undefined) {
                    // Args may be: fd, dirflags, pathPtr, pathLen, oflags, rbLo, rbHi, riLo, riHi, fdflags, fdOutPtr
                    // or: fd, dirflags, pathPtr, pathLen, oflags, rights_base(bigint), rights_inheriting(bigint), fdflags, fdOutPtr
                    fdOutPtr = fdflags;
                    fdflags = riHi;
                }

                const fullPath = wasiResolvePath(fd, pathPtr, pathLen);
                if (!fullPath) return WASI_EBADF;

                const isCreate = (oflags & WASI_OFLAG_CREAT) !== 0;
                const isDir    = (oflags & WASI_OFLAG_DIRECTORY) !== 0;
                const isExcl   = (oflags & WASI_OFLAG_EXCL) !== 0;
                const isTrunc  = (oflags & WASI_OFLAG_TRUNC) !== 0;

                let node = self.memfs._resolve(fullPath);

                if (node && isExcl && isCreate) return WASI_EEXIST;

                if (!node) {
                    if (!isCreate) return WASI_ENOENT;
                    if (isDir) {
                        node = self.memfs.mkdirp(fullPath);
                    } else {
                        node = self.memfs.createFile(fullPath, new Uint8Array(0));
                    }
                    if (!node) return WASI_ENOENT;
                }

                if (isDir && node.type !== WAPI_FILETYPE_DIRECTORY) return WASI_ENOTDIR;

                if (isTrunc && node.type === WAPI_FILETYPE_REGULAR) {
                    node.data = new Uint8Array(0);
                    node.mtime = Date.now();
                }

                const fdEntry = new FDEntry(node, fullPath, fdflags || 0);
                const h = self.handles.insert(fdEntry);
                // Write WASI fd (WAPI handle - 1)
                self._writeU32(fdOutPtr, h - 1);
                return WASI_ESUCCESS;
            },

            // (fd: i32, flags: i32, path: i32, path_len: i32, buf_ptr: i32) -> errno
            path_filestat_get(fd, flags, pathPtr, pathLen, bufPtr) {
                self._refreshViews();
                const fullPath = wasiResolvePath(fd, pathPtr, pathLen);
                if (!fullPath) return WASI_EBADF;
                const node = self.memfs._resolve(fullPath);
                if (!node) return WASI_ENOENT;
                writeWasiFilestat(bufPtr, node);
                return WASI_ESUCCESS;
            },

            // (fd: i32, flags: i32, path: i32, path_len: i32, atim: i64, mtim: i64, fst_flags: i32) -> errno
            path_filestat_set_times(fd, flags, pathPtr, pathLen, atimLo, atimHi, mtimLo, mtimHi, fstFlags) {
                const fullPath = wasiResolvePath(fd, pathPtr, pathLen);
                if (!fullPath) return WASI_EBADF;
                const node = self.memfs._resolve(fullPath);
                if (!node) return WASI_ENOENT;
                node.mtime = Date.now();
                return WASI_ESUCCESS;
            },

            // (fd: i32, path: i32, path_len: i32) -> errno
            path_create_directory(fd, pathPtr, pathLen) {
                self._refreshViews();
                const fullPath = wasiResolvePath(fd, pathPtr, pathLen);
                if (!fullPath) return WASI_EBADF;
                if (self.memfs._resolve(fullPath)) return WASI_EEXIST;
                const node = self.memfs.mkdirp(fullPath);
                return node ? WASI_ESUCCESS : WASI_ENOENT;
            },

            // (fd: i32, path: i32, path_len: i32) -> errno
            path_remove_directory(fd, pathPtr, pathLen) {
                self._refreshViews();
                const fullPath = wasiResolvePath(fd, pathPtr, pathLen);
                if (!fullPath) return WASI_EBADF;
                const node = self.memfs._resolve(fullPath);
                if (!node) return WASI_ENOENT;
                if (node.type !== WAPI_FILETYPE_DIRECTORY) return WASI_ENOTDIR;
                if (node.children && node.children.size > 0) return WASI_ENOTEMPTY;
                const { parent, name } = self.memfs._resolveParent(fullPath);
                if (parent) parent.children.delete(name);
                return WASI_ESUCCESS;
            },

            // (fd: i32, path: i32, path_len: i32) -> errno
            path_unlink_file(fd, pathPtr, pathLen) {
                self._refreshViews();
                const fullPath = wasiResolvePath(fd, pathPtr, pathLen);
                if (!fullPath) return WASI_EBADF;
                const node = self.memfs._resolve(fullPath);
                if (!node) return WASI_ENOENT;
                if (node.type === WAPI_FILETYPE_DIRECTORY) return WASI_EISDIR;
                const { parent, name } = self.memfs._resolveParent(fullPath);
                if (parent) parent.children.delete(name);
                return WASI_ESUCCESS;
            },

            // (old_fd: i32, old_flags: i32, old_path: i32, old_path_len: i32,
            //  new_fd: i32, new_path: i32, new_path_len: i32) -> errno
            path_rename(oldFd, oldFlags, oldPathPtr, oldPathLen, newFd, newPathPtr, newPathLen) {
                self._refreshViews();
                const oldPath = wasiResolvePath(oldFd, oldPathPtr, oldPathLen);
                const newPath = wasiResolvePath(newFd, newPathPtr, newPathLen);
                if (!oldPath || !newPath) return WASI_EBADF;

                const node = self.memfs._resolve(oldPath);
                if (!node) return WASI_ENOENT;

                // Remove from old location
                const { parent: oldParent, name: oldName } = self.memfs._resolveParent(oldPath);
                if (oldParent) oldParent.children.delete(oldName);

                // Add to new location
                const { parent: newParent, name: newName } = self.memfs._resolveParent(newPath);
                if (newParent) {
                    node.name = newName;
                    newParent.children.set(newName, node);
                }

                return WASI_ESUCCESS;
            },

            // (old_fd: i32, old_path: i32, old_path_len: i32, new_path: i32, new_path_len: i32) -> errno
            path_symlink(oldPathPtr, oldPathLen, fd, newPathPtr, newPathLen) {
                return WASI_ENOSYS; // symlinks not supported in MemFS
            },

            // (fd: i32, path: i32, path_len: i32, buf: i32, buf_len: i32, bufused: i32) -> errno
            path_readlink(fd, pathPtr, pathLen, bufPtr, bufLen, bufusedPtr) {
                return WASI_ENOSYS; // no symlinks
            },

            // (fd: i32, old_path: i32, old_path_len: i32, new_fd: i32, new_path: i32, new_path_len: i32) -> errno
            path_link(oldFd, oldFlags, oldPathPtr, oldPathLen, newFd, newPathPtr, newPathLen) {
                return WASI_ENOSYS; // hard links not supported
            },

            // --- Random ---
            // (buf: i32, buf_len: i32) -> errno
            random_get(bufPtr, bufLen) {
                self._refreshViews();
                const buf = self._u8.subarray(bufPtr, bufPtr + bufLen);
                crypto.getRandomValues(buf);
                return WASI_ESUCCESS;
            },

            // --- Process ---
            // (code: i32) -> noreturn
            proc_exit(code) {
                // proc_exit is called by _Exit/_start after Main() returns.
                // Since proc_exit is noreturn in C, the wasm has an `unreachable`
                // instruction after this call. We throw a sentinel to unwind the
                // wasm stack cleanly; the JS catch around _start recognizes it.
                console.log(`[WASI] proc_exit(${code})`);
                if (code !== 0) {
                    self._running = false;
                    if (self._frameHandle) {
                        cancelAnimationFrame(self._frameHandle);
                        self._frameHandle = 0;
                    }
                }
                throw new self._ProcExit(code);
            },

            proc_raise(sig) {
                return WASI_ENOSYS;
            },

            // --- Scheduling ---
            sched_yield() {
                return WASI_ESUCCESS;
            },

            // --- Poll ---
            // (in: i32, out: i32, nsubscriptions: i32, nevents_ptr: i32) -> errno
            poll_oneoff(inPtr, outPtr, nsubscriptions, neventsPtr) {
                self._refreshViews();
                // Minimal implementation: process clock subscriptions as immediate timeouts
                let nevents = 0;
                for (let i = 0; i < nsubscriptions; i++) {
                    // __wasi_subscription_t: userdata(u64) + u.tag(u8) + pad(7) + u.clock/fd_read/fd_write union
                    const subPtr = inPtr + i * 48;
                    const userdata = self._dv.getBigUint64(subPtr, true);
                    const tag = self._u8[subPtr + 8];

                    // __wasi_event_t at outPtr: userdata(u64) + error(u16) + type(u8) + pad(5) + fd_readwrite(16)
                    const evPtr = outPtr + nevents * 32;
                    self._dv.setBigUint64(evPtr, userdata, true);         // userdata
                    self._dv.setUint16(evPtr + 8, 0, true);              // error = success
                    self._u8[evPtr + 10] = tag;                          // type = same as subscription tag
                    // zero padding
                    for (let j = 11; j < 32; j++) self._u8[evPtr + j] = 0;
                    nevents++;
                }
                self._writeU32(neventsPtr, nevents);
                return WASI_ESUCCESS;
            },

            // --- Sockets (unsupported) ---
            sock_accept(fd, flags, fdPtr) { return WASI_ENOSYS; },
            sock_recv(fd, riDataPtr, riDataLen, riFlags, roDataLenPtr, roFlagsPtr) { return WASI_ENOSYS; },
            sock_send(fd, siDataPtr, siDataLen, siFlags, soDataLenPtr) { return WASI_ENOSYS; },
            sock_shutdown(fd, how) { return WASI_ENOSYS; },
        };

        // wapi_http, wapi_compression, and wapi_network are NOT returned as
        // separate import modules. All three flow through wapi_io.submit via
        // their respective opcodes, matching the header spec. The wasm module
        // never imports them by name; the shim's only network surface is
        // wapi_io. Exposing them here would invite duplicate, out-of-spec
        // surface area and break the capability-gating contract: a host that
        // didn't grant wapi_network would still be reachable via the legacy
        // module import.
        return { wapi, wapi_env, wapi_memory, wapi_clock, wapi_filesystem,
                 wapi_gpu, wapi_wgpu, wapi_surface, wapi_input, wapi_audio, wapi_content, wapi_text,
                 wapi_clipboard, wapi_kv, wapi_font, wapi_crypto, wapi_video, wapi_module,
                 wapi_notify, wapi_geo, wapi_sensor, wapi_speech, wapi_bio,
                 wapi_share, wapi_pay, wapi_usb, wapi_midi, wapi_bt, wapi_camera, wapi_xr,
                 wapi_register, wapi_taskbar, wapi_power, wapi_orient,
                 wapi_codec, wapi_media, wapi_encode,
                 wapi_authn, wapi_netinfo, wapi_haptic,
                 wapi_serial, wapi_capture, wapi_contacts,
                 wapi_barcode, wapi_nfc, wapi_dnd,
                 wapi_window, wapi_display, wapi_dialog, wapi_menu, wapi_tray,
                 wapi_theme, wapi_sysinfo, wapi_process, wapi_thread,
                 wapi_fwatch, wapi_eyedrop, wapi_plugin,
                 wasi_snapshot_preview1 };
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
                // wapi_keyboard_event_t layout after common header (16 bytes):
                //   Offset 16: uint32_t keyboard_handle
                //   Offset 20: uint32_t scancode
                //   Offset 24: uint32_t keycode
                //   Offset 28: uint16_t mod
                //   Offset 30: uint8_t  down
                //   Offset 31: uint8_t  repeat
                this._dv.setUint32(ptr + 16, ev.keyboard_handle || 0, true);
                this._dv.setUint32(ptr + 20, ev.scancode || 0, true);
                this._dv.setUint32(ptr + 24, ev.keycode || 0, true);
                this._dv.setUint16(ptr + 28, ev.mod || 0, true);
                this._u8[ptr + 30] = ev.down ? 1 : 0;
                this._u8[ptr + 31] = ev.repeat ? 1 : 0;
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

            case WAPI_EVENT_POINTER_DOWN:
            case WAPI_EVENT_POINTER_UP:
            case WAPI_EVENT_POINTER_MOTION:
            case WAPI_EVENT_POINTER_CANCEL:
            case WAPI_EVENT_POINTER_ENTER:
            case WAPI_EVENT_POINTER_LEAVE:
                this._dv.setInt32(ptr + 16, ev.pointer_id || 0, true);
                this._u8[ptr + 20] = ev.pointer_type || 0;
                this._u8[ptr + 21] = ev.button || 0;
                this._u8[ptr + 22] = ev.buttons || 0;
                this._u8[ptr + 23] = 0;
                this._dv.setFloat32(ptr + 24, ev.x || 0, true);
                this._dv.setFloat32(ptr + 28, ev.y || 0, true);
                this._dv.setFloat32(ptr + 32, ev.dx || 0, true);
                this._dv.setFloat32(ptr + 36, ev.dy || 0, true);
                this._dv.setFloat32(ptr + 40, ev.pressure || 0, true);
                this._dv.setFloat32(ptr + 44, ev.tilt_x || 0, true);
                this._dv.setFloat32(ptr + 48, ev.tilt_y || 0, true);
                this._dv.setFloat32(ptr + 52, ev.twist || 0, true);
                this._dv.setFloat32(ptr + 56, ev.width || 1, true);
                this._dv.setFloat32(ptr + 60, ev.height || 1, true);
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

            case WAPI_EVENT_IO_COMPLETION:
                // wapi_io_event_t after 16-byte common header:
                //   16: int32_t   result
                //   20: uint32_t  flags
                //   24: uint64_t  user_data
                this._dv.setInt32(ptr + 16, ev.result | 0, true);
                this._dv.setUint32(ptr + 20, ev.flags >>> 0, true);
                this._dv.setBigUint64(ptr + 24, BigInt(ev.userData || 0n), true);
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
            // One-shot diagnostic to surface unknown key codes so we can map
            // the ½/§ key on whatever layout the user has.
            if (!self.__wapiKeyLog) self.__wapiKeyLog = 0;
            if (self.__wapiKeyLog < 20) {
                console.log("[WAPI key] code=" + e.code + " key=" + e.key + " scancode=" + sc);
                self.__wapiKeyLog++;
            }
            self._keyState.add(sc);
            self._modState = domModToTP(e);
            // Swallow keys the app wants to handle itself but the browser
            // would otherwise intercept. F12 is intentionally NOT swallowed
            // — it's left for DevTools. The PanGui debugger is opened with
            // the ½/Grave/Backquote key instead (code "Backquote" or
            // "IntlBackslash"), and we preventDefault on that so the key
            // doesn't also get typed into any focused input field.
            if (e.code === "Backquote" || e.code === "IntlBackslash") {
                e.preventDefault();
            }
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
        console.log("[WAPI] _setupInputListeners called, canvas:", canvas, "surface:", surfaceHandle);
        // Use window-level listeners for mouse events since the canvas
        // fills the viewport and position:fixed canvases can have
        // hit-testing issues in some browsers.
        // One-shot diagnostic so we can verify coordinate spaces on the
        // first mousedown. Remove once cursor alignment is confirmed.
        let __wapiCursorDebug = 3;
        window.addEventListener("mousemove", (e) => {
            self._mouseX = e.clientX;
            self._mouseY = e.clientY;
            if (__wapiCursorDebug > 0) {
                const info = self._surfaces.get(sid);
                const rect = canvas.getBoundingClientRect();
                console.log("[WAPI cursor]",
                    "clientX=" + e.clientX, "clientY=" + e.clientY,
                    "| canvas.width=" + canvas.width, "height=" + canvas.height,
                    "| css rect=" + rect.width.toFixed(1) + "x" + rect.height.toFixed(1),
                    "at (" + rect.left.toFixed(1) + "," + rect.top.toFixed(1) + ")",
                    "| dpr=" + (info ? info.dpr : "?"),
                    "| window.dpr=" + window.devicePixelRatio);
                __wapiCursorDebug--;
            }
            self._eventQueue.push({
                type: WAPI_EVENT_MOUSE_MOTION,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
                mouse_id: 0,
                button_state: self._mouseButtons,
                x: e.clientX,
                y: e.clientY,
                xrel: e.movementX,
                yrel: e.movementY,
            });
        });

        window.addEventListener("mousedown", (e) => {
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
                x: e.clientX,
                y: e.clientY,
            });
        });

        window.addEventListener("mouseup", (e) => {
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
                x: e.clientX,
                y: e.clientY,
            });
        });

        window.addEventListener("wheel", (e) => {
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

        // Touch (coordinates in surface pixels, not normalized)
        canvas.addEventListener("touchstart", (e) => {
            for (const t of e.changedTouches) {
                const rect = canvas.getBoundingClientRect();
                self._eventQueue.push({
                    type: WAPI_EVENT_TOUCH_DOWN,
                    surface_id: sid,
                    timestamp: performance.now() * 1_000_000,
                    touch_id: 0,
                    finger_id: t.identifier,
                    x: t.clientX - rect.left,
                    y: t.clientY - rect.top,
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
                    x: t.clientX - rect.left,
                    y: t.clientY - rect.top,
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
                    x: t.clientX - rect.left,
                    y: t.clientY - rect.top,
                    dx: 0, dy: 0,
                    pressure: 0,
                });
            }
        });

        // Pointer (unified) -- uses native W3C PointerEvent API
        const pointerTypeMap = { "mouse": 0, "touch": 1, "pen": 2 };
        function makePointerEvent(type, e) {
            const ptype = pointerTypeMap[e.pointerType] || 0;
            const pid = ptype === 0 ? 0 : (ptype === 2 ? -1 : e.pointerId);
            self._pointerX = e.offsetX;
            self._pointerY = e.offsetY;
            self._pointerButtons = e.buttons;
            self._eventQueue.push({
                type: type,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
                pointer_id: pid,
                pointer_type: ptype,
                button: domButtonToTP(e.button),
                buttons: e.buttons,
                x: e.offsetX,
                y: e.offsetY,
                dx: e.movementX || 0,
                dy: e.movementY || 0,
                pressure: e.pressure,
                tilt_x: e.tiltX || 0,
                tilt_y: e.tiltY || 0,
                twist: e.twist || 0,
                width: e.width || 1,
                height: e.height || 1,
            });
        }
        canvas.addEventListener("pointerdown", (e) => {
            makePointerEvent(WAPI_EVENT_POINTER_DOWN, e);
        });
        canvas.addEventListener("pointerup", (e) => {
            makePointerEvent(WAPI_EVENT_POINTER_UP, e);
        });
        canvas.addEventListener("pointermove", (e) => {
            makePointerEvent(WAPI_EVENT_POINTER_MOTION, e);
        });
        canvas.addEventListener("pointercancel", (e) => {
            makePointerEvent(WAPI_EVENT_POINTER_CANCEL, e);
        });
        canvas.addEventListener("pointerenter", (e) => {
            makePointerEvent(WAPI_EVENT_POINTER_ENTER, e);
        });
        canvas.addEventListener("pointerleave", (e) => {
            makePointerEvent(WAPI_EVENT_POINTER_LEAVE, e);
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

        // Fetch and compile the wasm module.
        //
        // We always materialize bytes (instead of instantiateStreaming)
        // so we can sha256-hash them and round-trip through the WAPI
        // browser extension's content-addressed cache. The streaming-
        // vs-buffered delta is dominated by compile + _start, so it's
        // in the noise for any non-trivial module.
        //
        // SRI mode: when config.hash is supplied, the site has committed
        // to a specific wasm binary. We try the extension cache first
        // (skipping the network on hit), then fall back to the URL,
        // verifying that the fetched bytes hash matches before compile.
        let wasmModule, wasmInstance;
        let wasmBytes = null;
        let wasmUrl = "";
        let gotFromCache = false;
        const expectedHash = typeof config.hash === "string"
            ? config.hash.toLowerCase()
            : null;

        if (wasmSource instanceof WebAssembly.Module) {
            wasmModule = wasmSource;
        } else if (wasmSource instanceof ArrayBuffer) {
            wasmBytes = wasmSource;
            wasmModule = await WebAssembly.compile(wasmBytes);
        } else if (ArrayBuffer.isView(wasmSource)) {
            wasmBytes = wasmSource.buffer.slice(
                wasmSource.byteOffset,
                wasmSource.byteOffset + wasmSource.byteLength
            );
            wasmModule = await WebAssembly.compile(wasmBytes);
        } else {
            wasmUrl = wasmSource instanceof Response
                ? (wasmSource.url || "")
                : String(wasmSource);

            if (expectedHash) {
                const resp = await _wapiExtSend("modules.fetch", { hash: expectedHash });
                if (resp && resp.bytesB64) {
                    wasmBytes = _wapiB64ToBytes(resp.bytesB64).buffer;
                    gotFromCache = true;
                }
            }

            if (!gotFromCache) {
                const response = wasmSource instanceof Response
                    ? wasmSource
                    : await fetch(wasmSource);
                wasmBytes = await response.arrayBuffer();
            }
            wasmModule = await WebAssembly.compile(wasmBytes);
        }

        if (!wasmInstance) {
            wasmInstance = await WebAssembly.instantiate(wasmModule, imports);
        }

        // Hash the bytes (for SRI verification and cache insert).
        // Failure must never break the load — unless we had an
        // expectedHash that mismatched, which is a hard error.
        if (wasmBytes && typeof crypto !== "undefined" && crypto.subtle) {
            let hex = null;
            try {
                const digest = await crypto.subtle.digest("SHA-256", wasmBytes);
                hex = _wapiHexHash(new Uint8Array(digest));
            } catch (e) {
                // Crypto unavailable — skip verification and caching.
            }

            if (hex) {
                if (expectedHash && hex !== expectedHash) {
                    throw new Error(
                        `[WAPI] module hash mismatch: expected ${expectedHash}, got ${hex}`
                    );
                }
                // Store into the extension cache on miss. On hit the
                // SW already has the bytes; re-storing would double-
                // count the miss counter. Fire-and-forget — the shim
                // doesn't need confirmation.
                if (!gotFromCache) {
                    _wapiExtPost("modules.store", {
                        hash: hex,
                        url: wasmUrl,
                        bytesB64: _wapiBytesToB64(new Uint8Array(wasmBytes)),
                    });
                }
            }
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

        // Initialize WASI runtime
        if (wasmInstance.exports._initialize) {
            console.log('[WAPI] calling _initialize...');
            wasmInstance.exports._initialize();
            console.log('[WAPI] _initialize done');
        } else if (wasmInstance.exports._start) {
            console.log(`[WAPI] calling _start... (memory: ${(this.memory.buffer.byteLength / 1048576).toFixed(1)}MB)`);
            try {
                wasmInstance.exports._start();
                console.log(`[WAPI] _start done (memory: ${(this.memory.buffer.byteLength / 1048576).toFixed(1)}MB)`);
            } catch (e) {
                if (e instanceof this._ProcExit) {
                    console.log(`[WAPI] _start exited with code ${e.code} (memory: ${(this.memory.buffer.byteLength / 1048576).toFixed(1)}MB)`);
                    if (e.code !== 0) throw new Error(`WASI process exited with code ${e.code}`);
                } else {
                    throw e; // re-throw real errors
                }
            }
        }

        // Call wapi_main if exported
        if (wasmInstance.exports.wapi_main) {
            console.log('[WAPI] calling wapi_main...');
            const result = wasmInstance.exports.wapi_main();
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
// Register globally. This file is loaded as a classic script (both by
// runtime/browser/index.html and by the WAPI browser extension's
// content_scripts entry), so we cannot use ES `export` syntax — that
// would be a parse error. Consumers grab the class off the global:
//
//     <script src="./wapi_shim.js"></script>
//     <script>const wapi = new window.WAPI();</script>
//
// CommonJS branch is kept for the rare node-side test harness.
if (typeof module !== "undefined" && module.exports) {
    module.exports = { WAPI };
}
globalThis.WAPI = WAPI;

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
            const wapi = new WAPI();

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
