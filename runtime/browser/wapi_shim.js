console.log("[WAPI] shim loaded (audio: AudioWorkletNode, module: page-local cache)");

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
 *   wapi_gpu, wapi_surface, wapi_input, wapi_audio, wapi_content, wapi_transfer,
 *   wapi_seat, wapi_kv, wapi_font, wapi_crypto, wapi_video, wapi_module,
 *   wapi_notify, wapi_geo, wapi_sensor, wapi_speech, wapi_bio,
 *   wapi_pay, wapi_usb, wapi_midi, wapi_bt, wapi_camera, wapi_xr
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

/* Name resolver: turn a negative WAPI_ERR_* code into a human-readable
 * string like "WAPI_ERR_TIMEDOUT (-16)". Falls back to the numeric
 * code for unmapped values. */
const WAPI_ERR_NAMES = {
    [WAPI_OK]:              "WAPI_OK",
    [WAPI_ERR_UNKNOWN]:     "WAPI_ERR_UNKNOWN",
    [WAPI_ERR_INVAL]:       "WAPI_ERR_INVAL",
    [WAPI_ERR_BADF]:        "WAPI_ERR_BADF",
    [WAPI_ERR_ACCES]:       "WAPI_ERR_ACCES",
    [WAPI_ERR_NOENT]:       "WAPI_ERR_NOENT",
    [WAPI_ERR_EXIST]:       "WAPI_ERR_EXIST",
    [WAPI_ERR_NOTDIR]:      "WAPI_ERR_NOTDIR",
    [WAPI_ERR_ISDIR]:       "WAPI_ERR_ISDIR",
    [WAPI_ERR_NOSPC]:       "WAPI_ERR_NOSPC",
    [WAPI_ERR_NOMEM]:       "WAPI_ERR_NOMEM",
    [WAPI_ERR_NAMETOOLONG]: "WAPI_ERR_NAMETOOLONG",
    [WAPI_ERR_NOTEMPTY]:    "WAPI_ERR_NOTEMPTY",
    [WAPI_ERR_IO]:          "WAPI_ERR_IO",
    [WAPI_ERR_AGAIN]:       "WAPI_ERR_AGAIN",
    [WAPI_ERR_BUSY]:        "WAPI_ERR_BUSY",
    [WAPI_ERR_TIMEDOUT]:    "WAPI_ERR_TIMEDOUT",
    [WAPI_ERR_CONNREFUSED]: "WAPI_ERR_CONNREFUSED",
    [WAPI_ERR_CONNRESET]:   "WAPI_ERR_CONNRESET",
    [WAPI_ERR_CONNABORTED]: "WAPI_ERR_CONNABORTED",
    [WAPI_ERR_NETUNREACH]:  "WAPI_ERR_NETUNREACH",
    [WAPI_ERR_HOSTUNREACH]: "WAPI_ERR_HOSTUNREACH",
    [WAPI_ERR_ADDRINUSE]:   "WAPI_ERR_ADDRINUSE",
    [WAPI_ERR_PIPE]:        "WAPI_ERR_PIPE",
    [WAPI_ERR_NOTCAPABLE]:  "WAPI_ERR_NOTCAPABLE",
    [WAPI_ERR_NOTSUP]:      "WAPI_ERR_NOTSUP",
    [WAPI_ERR_OVERFLOW]:    "WAPI_ERR_OVERFLOW",
    [WAPI_ERR_CANCELED]:    "WAPI_ERR_CANCELED",
    [WAPI_ERR_NOSYS]:       "WAPI_ERR_NOSYS",
};
function wapiErrName(code) {
    const name = WAPI_ERR_NAMES[code];
    return name ? `${name} (${code})` : `code ${code}`;
}

const WAPI_HANDLE_INVALID = 0;
const WAPI_STDIN  = 1;
const WAPI_STDOUT = 2;
const WAPI_STDERR = 3;
const WAPI_STRLEN = 0xFFFFFFFF;          // 32-bit sentinel for _readString
const WAPI_STRLEN64 = 0xFFFFFFFFFFFFFFFFn; // 64-bit sentinel in wapi_string_view_t

// Singleton TextDecoder/TextEncoder. Constructing these is non-trivial and
// they're called many times per frame from the GPU and string-IO paths.
const WAPI_TEXT_DECODER = new TextDecoder();
const WAPI_TEXT_ENCODER = new TextEncoder();

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
const WAPI_IO_OP_NOP         = 0x00;
const WAPI_IO_OP_CAP_REQUEST = 0x01;
const WAPI_IO_OP_READ        = 0x02;
const WAPI_IO_OP_WRITE       = 0x03;
const WAPI_IO_OP_OPEN        = 0x04;
const WAPI_IO_OP_CLOSE       = 0x05;
const WAPI_IO_OP_STAT        = 0x06;
const WAPI_IO_OP_LOG         = 0x07;
const WAPI_IO_OP_CONNECT     = 10;
const WAPI_IO_OP_ACCEPT      = 11;
const WAPI_IO_OP_SEND        = 12;
const WAPI_IO_OP_RECV        = 13;
const WAPI_IO_OP_TIMEOUT     = 20;
const WAPI_IO_OP_TIMEOUT_ABS = 21;
const WAPI_IO_OP_HTTP_FETCH               = 0x060;
const WAPI_IO_OP_COMPRESS_PROCESS         = 0x140;
const WAPI_IO_OP_FONT_GET_BYTES           = 0x150;
const WAPI_IO_OP_NETWORK_LISTEN           = 0x0E;
const WAPI_IO_OP_NETWORK_CHANNEL_OPEN     = 0x0F;
const WAPI_IO_OP_NETWORK_CHANNEL_ACCEPT   = 0x10;
const WAPI_IO_OP_NETWORK_RESOLVE          = 0x11;

// Core opcodes (namespace 0x0000) — mirror wapi.h's enum values.
const WAPI_IO_OP_ROLE_REQUEST             = 0x016;
const WAPI_IO_OP_ROLE_REPICK              = 0x017;
const WAPI_IO_OP_SERIAL_PORT_REQUEST      = 0x080;
const WAPI_IO_OP_SERIAL_OPEN              = 0x081;
const WAPI_IO_OP_SERIAL_READ              = 0x082;
const WAPI_IO_OP_SERIAL_WRITE             = 0x083;
// MIDI port acquisition folded into WAPI_IO_OP_ROLE_REQUEST (spec §9.10).
const WAPI_IO_OP_MIDI_SEND                = 0x092;
const WAPI_IO_OP_MIDI_RECV                = 0x093;
const WAPI_IO_OP_BT_DEVICE_REQUEST        = 0x0A0;
const WAPI_IO_OP_BT_CONNECT               = 0x0A1;
const WAPI_IO_OP_BT_VALUE_READ            = 0x0A2;
const WAPI_IO_OP_BT_VALUE_WRITE           = 0x0A3;
const WAPI_IO_OP_BT_NOTIFICATIONS_START   = 0x0A4;
const WAPI_IO_OP_BT_SERVICE_GET           = 0x0A5;
const WAPI_IO_OP_BT_CHARACTERISTIC_GET    = 0x0A6;
const WAPI_IO_OP_USB_DEVICE_REQUEST       = 0x0B0;
const WAPI_IO_OP_USB_OPEN                 = 0x0B1;
const WAPI_IO_OP_USB_INTERFACE_CLAIM      = 0x0B2;
const WAPI_IO_OP_USB_TRANSFER_IN          = 0x0B3;
const WAPI_IO_OP_USB_TRANSFER_OUT         = 0x0B4;
const WAPI_IO_OP_USB_CONTROL_TRANSFER     = 0x0B5;
const WAPI_IO_OP_NFC_SCAN_START           = 0x0C0;
const WAPI_IO_OP_NFC_WRITE                = 0x0C1;
// Camera open folded into WAPI_IO_OP_ROLE_REQUEST (spec §9.10).
const WAPI_IO_OP_CAMERA_FRAME_READ        = 0x0D1;
const WAPI_IO_OP_CODEC_DECODE             = 0x100;
const WAPI_IO_OP_CODEC_ENCODE             = 0x101;
const WAPI_IO_OP_CODEC_OUTPUT_GET         = 0x102;
const WAPI_IO_OP_CODEC_FLUSH              = 0x103;
const WAPI_IO_OP_VIDEO_CREATE             = 0x110;
const WAPI_IO_OP_VIDEO_SEEK               = 0x111;
const WAPI_IO_OP_SPEECH_SPEAK             = 0x120;
const WAPI_IO_OP_SPEECH_RECOGNIZE_START   = 0x121;
const WAPI_IO_OP_SPEECH_RECOGNIZE_RESULT  = 0x122;
const WAPI_IO_OP_CAPTURE_REQUEST          = 0x130;
const WAPI_IO_OP_CAPTURE_FRAME_GET        = 0x131;
const WAPI_IO_OP_DIALOG_FILE_OPEN         = 0x180;
const WAPI_IO_OP_DIALOG_FILE_SAVE         = 0x181;
const WAPI_IO_OP_DIALOG_FOLDER_OPEN       = 0x182;
const WAPI_IO_OP_DIALOG_MESSAGEBOX        = 0x183;
const WAPI_IO_OP_DIALOG_PICK_COLOR        = 0x184;
const WAPI_IO_OP_DIALOG_PICK_FONT         = 0x185;
const WAPI_IO_OP_BIO_AUTHENTICATE         = 0x191;
const WAPI_IO_OP_AUTHN_CREDENTIAL_CREATE  = 0x192;
const WAPI_IO_OP_AUTHN_ASSERTION_GET      = 0x193;
const WAPI_IO_OP_CONTACTS_PICK            = 0x1A0;
const WAPI_IO_OP_EYEDROPPER_PICK          = 0x1A1;
const WAPI_IO_OP_PAY_PAYMENT_REQUEST      = 0x1A2;
const WAPI_IO_OP_CONTACTS_ICON_READ       = 0x1A3;
const WAPI_IO_OP_XR_SESSION_REQUEST       = 0x200;
const WAPI_IO_OP_XR_FRAME_WAIT            = 0x201;
const WAPI_IO_OP_XR_HIT_TEST              = 0x202;
const WAPI_IO_OP_GEO_POSITION_GET         = 0x210;
const WAPI_IO_OP_GEO_POSITION_WATCH       = 0x211;

// Dedicated new namespaces (packed u32: ns<<16 | method).
const WAPI_IO_OP_FWATCH_ADD                 = 0x008;
const WAPI_IO_OP_FWATCH_REMOVE              = 0x009;
const WAPI_IO_OP_SANDBOX_FWATCH_ADD         = 0x2A3;
const WAPI_IO_OP_SANDBOX_FWATCH_REMOVE      = 0x2A4;
const WAPI_IO_OP_CRYPTO_HASH                = 0x2C0;
const WAPI_IO_OP_CRYPTO_HASH_CREATE         = 0x2C1;
const WAPI_IO_OP_CRYPTO_ENCRYPT             = 0x2C2;
const WAPI_IO_OP_CRYPTO_DECRYPT             = 0x2C3;
const WAPI_IO_OP_CRYPTO_SIGN                = 0x2C4;
const WAPI_IO_OP_CRYPTO_VERIFY              = 0x2C5;
const WAPI_IO_OP_CRYPTO_DERIVE_KEY          = 0x2C6;
const WAPI_IO_OP_CRYPTO_KEY_IMPORT_RAW      = 0x2C7;
const WAPI_IO_OP_CRYPTO_KEY_GENERATE        = 0x2C8;
const WAPI_IO_OP_CRYPTO_KEY_GENERATE_PAIR   = 0x2C9;
const WAPI_IO_OP_BARCODE_DETECT_IMAGE       = 0x2D8;
const WAPI_IO_OP_BARCODE_DETECT_CAMERA      = 0x2D9;
const WAPI_IO_OP_POWER_INFO_GET             = 0x2E8;
const WAPI_IO_OP_POWER_WAKE_ACQUIRE         = 0x2E9;
const WAPI_IO_OP_POWER_IDLE_START           = 0x2EA;
// Sensor start folded into WAPI_IO_OP_ROLE_REQUEST (spec §9.10).
const WAPI_IO_OP_NOTIFY_SHOW                = 0x2F8;
const WAPI_IO_OP_FONT_FAMILY_INFO           = 0x2FC;
const WAPI_IO_OP_TRANSFER_OFFER             = 0x310;
const WAPI_IO_OP_TRANSFER_READ              = 0x311;

// wapi_transfer mode bitmask
const WAPI_TRANSFER_LATENT  = 0x01;
const WAPI_TRANSFER_POINTED = 0x02;
const WAPI_TRANSFER_ROUTED  = 0x04;

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
const WAPI_EVENT_TRANSFER_ENTER        = 0x1600;
const WAPI_EVENT_TRANSFER_OVER         = 0x1601;
const WAPI_EVENT_TRANSFER_LEAVE        = 0x1602;
const WAPI_EVENT_TRANSFER_DELIVER      = 0x1603;
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

function _wapiAudioBytesPerSample(format) {
    switch (format) {
        case WAPI_AUDIO_U8:  return 1;
        case WAPI_AUDIO_S16: return 2;
        case WAPI_AUDIO_S32: return 4;
        case WAPI_AUDIO_F32: return 4;
        default:             return 0;
    }
}

// Role kinds (wapi_role_kind_t from wapi.h — spec §9.10)
const WAPI_ROLE_AUDIO_PLAYBACK  = 0x01;
const WAPI_ROLE_AUDIO_RECORDING = 0x02;
const WAPI_ROLE_CAMERA          = 0x03;
const WAPI_ROLE_MIDI_INPUT      = 0x04;
const WAPI_ROLE_MIDI_OUTPUT     = 0x05;
const WAPI_ROLE_KEYBOARD        = 0x06;
const WAPI_ROLE_MOUSE           = 0x07;
const WAPI_ROLE_GAMEPAD         = 0x08;
const WAPI_ROLE_HAPTIC          = 0x09;
const WAPI_ROLE_SENSOR          = 0x0A;
const WAPI_ROLE_DISPLAY         = 0x0B;
const WAPI_ROLE_HID             = 0x0C;
const WAPI_ROLE_TOUCH           = 0x0D;
const WAPI_ROLE_PEN             = 0x0E;
const WAPI_ROLE_POINTER         = 0x0F;

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

// Page-local service registry for wapi_module.join. Per spec §10 memory
// model, memory 1 belongs to the application — on the web the application
// is a single page, so the live instance MUST live here. Intra-page sharing
// is the point of service mode: multiple wasm callers that agree on
// (hash, name) see the same instance and the instance is refcounted.
//
// Keyed by "<hashHex>:<name>". Entry:
//   { state, module, instance, memory, childMemHelpers, refcount,
//     hashHex, name, url, announceTimer }
// state ∈ { "pending", "ready", "failed" }.
//
// Cross-tab visibility is via services.announce to the extension SW —
// the SW is a federated observer, not a host (the application's memory 1
// can't live in the SW without SAB+Atomics.wait to make imports sync).
const _wapiPageServices = new Map();

// Single heartbeat interval that refreshes announces for every ready
// entry in _wapiPageServices. Started on the first successful join,
// cleared when the last service is withdrawn.
let _wapiAnnounceInterval = null;
const WAPI_ANNOUNCE_HEARTBEAT_MS = 5000;

function _wapiStartHeartbeat() {
    if (_wapiAnnounceInterval) return;
    _wapiAnnounceInterval = setInterval(() => {
        let any = false;
        for (const svc of _wapiPageServices.values()) {
            if (svc.state !== "ready") continue;
            any = true;
            _wapiExtPost("services.announce", {
                hashHex: svc.hashHex,
                name:    svc.name,
                url:     svc.url,
                origin:  (typeof window !== "undefined" && window.location)
                         ? window.location.origin : "",
                refcount: svc.refcount,
            });
        }
        if (!any) { clearInterval(_wapiAnnounceInterval); _wapiAnnounceInterval = null; }
    }, WAPI_ANNOUNCE_HEARTBEAT_MS);
}

// Fetch bytes by hash through the extension cache, then network as a
// fallback. Verifies the SHA-256 matches the requested hash before
// compiling. Used by wapi_module.prefetch. Returns a WebAssembly.Module.
async function _wapiFetchAndCompile(hashHex, url) {
    let bytes = null;
    let gotFromCache = false;

    const resp = await _wapiExtSend("modules.fetch", { hash: hashHex }, 1500);
    if (resp && resp.bytesB64) {
        bytes = _wapiB64ToBytes(resp.bytesB64);
        gotFromCache = true;
        console.log(`[WAPI] cache hit for ${hashHex.slice(0,10)}… (${bytes.length} bytes)`);
    }

    if (!bytes) {
        if (!url) throw new Error("module not cached and no url provided");
        console.log(`[WAPI] network fetch ${url}`);
        const response = await fetch(url);
        if (!response.ok) {
            throw new Error(`fetch ${url} failed: ${response.status}`);
        }
        const ab = await response.arrayBuffer();
        bytes = new Uint8Array(ab);
        console.log(`[WAPI] fetched ${bytes.length} bytes from ${url}`);
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

        // wapi_module shared memory (memory 1 in the module spec).
        // A single host-owned buffer; shared_alloc bumps into it,
        // shared_read/write copies between it and the caller's memory 0.
        // Child module instances share this buffer with the parent so
        // data passed via (offset, len) is visible on both sides.
        this._sharedMem   = new Uint8Array(16 * 1024 * 1024);
        this._sharedBump  = 16;  // skip 0 so it doubles as "unallocated"
        this._sharedAllocs = new Map();  // offset -> size

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

        // Scratch arrays reused across frames to avoid per-call allocations.
        this._queueSubmitScratch = [];
    }

    // -----------------------------------------------------------------------
    // Memory view helpers - must be called after any memory.grow
    // -----------------------------------------------------------------------

    // Fast path: if the WebAssembly.Memory buffer hasn't changed identity,
    // the cached typed-array views are still valid. memory.grow replaces the
    // buffer (old one becomes detached), so identity check is sufficient.
    // Called ~hundreds of times per frame from GPU imports — allocating 5
    // new views per call showed up hot in profiles.
    _refreshViews() {
        const buf = this.memory.buffer;
        if (this._u8 != null && this._u8.buffer === buf) return;
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
        return WAPI_TEXT_DECODER.decode(this._u8.subarray(ptr, ptr + len));
    }

    _writeString(ptr, maxLen, str) {
        const encoded = WAPI_TEXT_ENCODER.encode(str);
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
            "wapi.clock", "wapi.env",
            "wapi.filesystem", "wapi.sandbox", "wapi.cache",
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

        // Additional capability surfaces wired up above.
        caps.push("wapi.window", "wapi.theme", "wapi.orientation", "wapi.encoding");
        caps.push("wapi.text", "wapi.media_session", "wapi.taskbar");
        caps.push("wapi.register"); // registerProtocolHandler
        caps.push("wapi.dnd");
        caps.push("wapi.thread"); // TLS/mutex no-ops for single-thread modules
        if (typeof EyeDropper !== "undefined") caps.push("wapi.eyedrop");
        if (typeof VideoDecoder !== "undefined" || typeof VideoEncoder !== "undefined") {
            caps.push("wapi.codec");
        }
        if (typeof document !== "undefined") caps.push("wapi.video");
        if (typeof Accelerometer !== "undefined" || typeof Gyroscope !== "undefined") {
            caps.push("wapi.sensors");
        }
        if (typeof navigator !== "undefined" && navigator.usb) caps.push("wapi.usb");
        if (typeof navigator !== "undefined" && navigator.bluetooth) caps.push("wapi.bluetooth");
        if (typeof navigator !== "undefined" && navigator.getUserMedia) caps.push("wapi.camera");
        if (typeof navigator !== "undefined" && navigator.mediaDevices &&
            navigator.mediaDevices.getUserMedia) {
            caps.push("wapi.camera");
        }
        if (typeof PaymentRequest !== "undefined") caps.push("wapi.payments");
        if (typeof window !== "undefined" && window.queryLocalFonts) caps.push("wapi.font.local");
        if (typeof IdleDetector !== "undefined") caps.push("wapi.idle");
        if (typeof navigator !== "undefined" && navigator.wakeLock) caps.push("wapi.wake");
        if (typeof navigator !== "undefined" && navigator.getBattery) caps.push("wapi.battery");

        return caps;
    }

    // -----------------------------------------------------------------------
    // wapi_module: shared memory arena + child instantiation
    // -----------------------------------------------------------------------

    // Align `off` up to `align` (power of two).
    _alignUp(off, align) {
        return (off + align - 1) & ~(align - 1);
    }

    // Bump allocator over this._sharedMem. Returns an offset (>0) or 0 on
    // failure. Tracks size so shared_free/usable_size can answer correctly
    // even though we never reclaim the space.
    _sharedAlloc(size, align) {
        if (size <= 0) return 0;
        if (align < 1) align = 1;
        const off = this._alignUp(this._sharedBump, align);
        if (off + size > this._sharedMem.length) return 0;
        this._sharedBump = off + size;
        this._sharedAllocs.set(off, size);
        return off;
    }
    _sharedFree(off) {
        // Bump allocator never reclaims; just drop the accounting entry.
        this._sharedAllocs.delete(off);
        return WAPI_OK;
    }

    // Instantiate a compiled child wasm module with imports tailored to
    // its private memory 0. The shared-memory import closures are bound
    // to the child's memory so shared_read/write copy to/from the child,
    // not the host.
    _instantiateChildModule(compiledModule, hashHex, serviceName) {
        const self = this;
        const imports = self._buildImports();
        const childCtx = { u8: null, dv: null, memory: null };

        const childU8 = () => {
            if (!childCtx.memory) return null;
            if (childCtx.u8 === null || childCtx.u8.buffer !== childCtx.memory.buffer) {
                childCtx.u8 = new Uint8Array(childCtx.memory.buffer);
                childCtx.dv = new DataView(childCtx.memory.buffer);
            }
            return childCtx.u8;
        };

        // Override only the memory-accessing wapi_module calls. Everything
        // else (including non-memory wapi_module calls like shared_alloc)
        // can stay on the parent's bindings. shared_read/write must use the
        // CHILD's memory for the dstPtr/srcPtr side of the copy.
        imports.wapi_module = Object.assign({}, imports.wapi_module, {
            shared_read(srcOffset, dstPtr, len) {
                const u8 = childU8();
                if (!u8) return WAPI_ERR_BADF;
                const s = Number(srcOffset), d = Number(dstPtr), n = Number(len);
                if (s + n > self._sharedMem.length) return WAPI_ERR_RANGE || -2;
                if (d + n > u8.length)              return WAPI_ERR_RANGE || -2;
                u8.set(self._sharedMem.subarray(s, s + n), d);
                return WAPI_OK;
            },
            shared_write(dstOffset, srcPtr, len) {
                const u8 = childU8();
                if (!u8) return WAPI_ERR_BADF;
                const d = Number(dstOffset), s = Number(srcPtr), n = Number(len);
                if (d + n > self._sharedMem.length) return WAPI_ERR_RANGE || -2;
                if (s + n > u8.length)              return WAPI_ERR_RANGE || -2;
                self._sharedMem.set(u8.subarray(s, s + n), d);
                return WAPI_OK;
            },
        });

        const inst = new WebAssembly.Instance(compiledModule, imports);
        childCtx.memory = inst.exports.memory;

        // wasi-libc's reactor modules run _initialize() (if exported) before
        // any of their other exports are invoked. hello_game's tracker
        // doesn't declare one, but be safe.
        if (typeof inst.exports._initialize === "function") {
            try { inst.exports._initialize(); } catch (e) { /* non-fatal */ }
        }

        return self.handles.insert({
            type: "module",
            instance: inst,
            module: compiledModule,
            hash: hashHex,
            name: serviceName || "",
        });
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
            cap_supported(svPtr) {
                const name = self._readStringView(svPtr);
                if (!name) return 0;
                return supportedCaps.includes(name) ? 1 : 0;
            },

            cap_version(svPtr, versionPtr) {
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

            cap_count() {
                return supportedCaps.length;
            },

            cap_name(index, bufPtr, bufLen, nameLenPtr) {
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

                // Grow table to fit 10 new entries (8 original + 2 for
                // namespace_register / namespace_name).
                const baseIdx = table.length;
                table.grow(10);

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
                // cap_supported: (impl, name_sv_ptr) -> i32
                table.set(baseIdx + 5, mf(['i32','i32'], ['i32'],
                    (_impl, svPtr) => {
                        const name = self._readStringView(svPtr);
                        if (!name) return 0;
                        return supportedCaps.includes(name) ? 1 : 0;
                    }));
                // cap_version: (impl, name_sv_ptr, version_ptr) -> i32
                table.set(baseIdx + 6, mf(['i32','i32','i32'], ['i32'],
                    (_impl, svPtr, versionPtr) => wapi.cap_version(svPtr, versionPtr)));
                // cap_query: (impl, cap_sv_ptr, state_ptr) -> i32
                table.set(baseIdx + 7, mf(['i32','i32','i32'], ['i32'],
                    (_impl, capSvPtr, statePtr) => {
                        const cap = self._readStringView(capSvPtr);
                        if (!cap) { self._writeU32(statePtr, 0); return WAPI_ERR_INVAL; }
                        // Every known capability is granted by default on the
                        // browser host — platforms that gate it will run the
                        // real prompt through WAPI_IO_OP_CAP_REQUEST.
                        // 0 = GRANTED, 1 = DENIED, 2 = PROMPT.
                        const granted = supportedCaps.includes(cap) ? 0 : 2;
                        self._writeU32(statePtr, granted);
                        return WAPI_OK;
                    }));
                // namespace_register: (impl, name_sv_ptr, out_id_ptr) -> i32
                table.set(baseIdx + 8, mf(['i32','i32','i32'], ['i32'],
                    (_impl, svPtr, outIdPtr) => {
                        const name = self._readStringView(svPtr);
                        if (!name) return WAPI_ERR_INVAL;
                        if (!self._nsRegistry) {
                            self._nsRegistry = new Map();     // name -> id
                            self._nsRegistryRev = new Map();  // id -> name
                            self._nsRegistryNext = 0x4000;
                        }
                        let id = self._nsRegistry.get(name);
                        if (id === undefined) {
                            if (self._nsRegistryNext > 0xFFFF) return WAPI_ERR_NOSPC;
                            id = self._nsRegistryNext++;
                            self._nsRegistry.set(name, id);
                            self._nsRegistryRev.set(id, name);
                        }
                        self._refreshViews();
                        self._dv.setUint16(outIdPtr, id, true);
                        return WAPI_OK;
                    }));
                // namespace_name: (impl, id, buf, buf_len, name_len_ptr) -> i32
                table.set(baseIdx + 9, mf(['i32','i32','i32','i64','i32'], ['i32'],
                    (_impl, id, bufPtr, bufLen, nameLenPtr) => {
                        const name = self._nsRegistryRev && self._nsRegistryRev.get(id);
                        if (!name) return WAPI_ERR_NOENT;
                        const written = self._writeString(bufPtr, Number(bufLen), name);
                        self._writeU64(nameLenPtr, written);
                        return WAPI_OK;
                    }));

                // Allocate 44 bytes in linear memory for wapi_io_t
                // Layout: impl(4) submit(4) cancel(4) poll(4) wait(4) flush(4)
                //         cap_supported(4) cap_version(4) cap_query(4)
                //         namespace_register(4) namespace_name(4)
                const ptr = self._hostAlloc(44, 4);
                if (!ptr) return 0;
                self._refreshViews();
                self._dv.setUint32(ptr +  0, 0,            true); // impl (unused)
                self._dv.setUint32(ptr +  4, baseIdx + 0,  true); // submit
                self._dv.setUint32(ptr +  8, baseIdx + 1,  true); // cancel
                self._dv.setUint32(ptr + 12, baseIdx + 2,  true); // poll
                self._dv.setUint32(ptr + 16, baseIdx + 3,  true); // wait
                self._dv.setUint32(ptr + 20, baseIdx + 4,  true); // flush
                self._dv.setUint32(ptr + 24, baseIdx + 5,  true); // cap_supported
                self._dv.setUint32(ptr + 28, baseIdx + 6,  true); // cap_version
                self._dv.setUint32(ptr + 32, baseIdx + 7,  true); // cap_query
                self._dv.setUint32(ptr + 36, baseIdx + 8,  true); // namespace_register
                self._dv.setUint32(ptr + 40, baseIdx + 9,  true); // namespace_name

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

            // (i32 buf, i64 buf_len, i32 len_ptr) -> i32
            get_locale(bufPtr, bufLen, lenPtr) {
                const locale = (typeof navigator !== "undefined" && navigator.language) || "en-US";
                const written = self._writeString(bufPtr, Number(bufLen), locale);
                self._writeU64(lenPtr, written);
                return WAPI_OK;
            },

            // (i32 buf, i64 buf_len, i32 len_ptr) -> i32
            get_timezone(bufPtr, bufLen, lenPtr) {
                let tz = "UTC";
                try { tz = Intl.DateTimeFormat().resolvedOptions().timeZone || "UTC"; } catch (_) {}
                const written = self._writeString(bufPtr, Number(bufLen), tz);
                self._writeU64(lenPtr, written);
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

                        default: {
                            // Route through the extensible opcode table before
                            // falling back to NOSYS. This is where dynamically-
                            // registered vendor opcodes land, along with any
                            // new core opcodes the shim has wired up via
                            // wapi.registerOpcodeHandler.
                            const h = self._opcodeHandlers && self._opcodeHandlers.get(opcode);
                            if (h) {
                                try {
                                    h({
                                        fd, flags, flags2, offset, addr, len,
                                        addr2, len2, userData, resultPtr,
                                        _pushIoCompletion, self,
                                    });
                                } catch (e) {
                                    _pushIoCompletion(userData, WAPI_ERR_IO, 0);
                                }
                            } else {
                                // Unknown opcode — surface NOSYS completion
                                // rather than trapping. Apps that expected a
                                // handler to be registered can see the flag.
                                self._eventQueue.push({
                                    type: WAPI_EVENT_IO_COMPLETION,
                                    userData, result: WAPI_ERR_NOSYS,
                                    flags: 0x0008 /* WAPI_IO_CQE_F_NOSYS */,
                                });
                            }
                            break;
                        }
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

                /* Snapshot input now — linear memory may grow during async. */
                self._refreshViews();
                const input = self._u8.slice(addr, addr + len);

                /* Sync path via pako. Required for guest code that polls
                 * inside wapi_main (can't yield to microtasks). On failure
                 * we fall through to the async CompressionStream path. */
                if (decompress && typeof pako !== "undefined") {
                    try {
                        const fn = algo === 0 ? pako.ungzip
                                 : algo === 1 ? pako.inflate
                                 : algo === 2 ? pako.inflateRaw
                                              : null;
                        if (fn) {
                            const out = fn(input);
                            self._refreshViews();
                            const n = Math.min(len2, out.byteLength);
                            if (n > 0) self._u8.set(out.subarray(0, n), addr2);
                            _pushIoCompletion(userData, out.byteLength > len2 ? -2 : n, 0);
                            return;
                        }
                    } catch (err) {
                        console.warn("[wapi_compression] sync pako failed:", err);
                        _pushIoCompletion(userData, -1, 0);
                        return;
                    }
                }

                const Stream = decompress
                    ? (typeof DecompressionStream !== "undefined" ? DecompressionStream : null)
                    : (typeof CompressionStream   !== "undefined" ? CompressionStream   : null);
                if (!Stream) { _pushIoCompletion(userData, -1, 0); return; }

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

        // Render-pass encoder cache. A render pass issues many operations
        // against the same encoder handle (setScissor, setBindGroup, draw,
        // end). Caching the last-resolved pass object skips a Map lookup
        // per call — adds up across ~15–20 render-pass ops per frame.
        // Invalidated at pass end so a stale handle can't leak into a new
        // pass that happens to reuse the integer.
        let cachedPassH = 0;
        let cachedPassObj = null;
        const passH = (h) => {
            if (h === cachedPassH && cachedPassObj !== null) return cachedPassObj;
            const obj = gpuH(h);
            cachedPassH = h;
            cachedPassObj = obj;
            return obj;
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
                let bg;
                try {
                    bg = dev.createBindGroup(desc);
                } catch (e) {
                    // One-shot probe: dump the raw entry handle fields for the
                    // failing call so we can tell which binding slot is missing
                    // its resource. Remove once the underlying C# side is fixed.
                    console.error("[WAPI/probe] createBindGroup threw; layoutH=" + layoutH + " entryCount=" + entryCount + " label=" + (label || "(none)"));
                    for (let i = 0; i < entryCount; i++) {
                        const ep = entriesPtr + i * 40;
                        console.error(
                            "[WAPI/probe]   i=" + i +
                            " binding=" + self._readU32(ep + 4) +
                            " buffer=" + self._readU32(ep + 8) +
                            " offset=" + Number(self._dv.getBigUint64(ep + 16, true)) +
                            " size=" + Number(self._dv.getBigUint64(ep + 24, true)) +
                            " sampler=" + self._readU32(ep + 32) +
                            " textureView=" + self._readU32(ep + 36));
                    }
                    throw e;
                }
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

                // Reuse a single scratch array across frames. submit() copies
                // refs internally, so we can truncate afterward.
                const cmdBufs = self._queueSubmitScratch;
                cmdBufs.length = 0;
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
                cmdBufs.length = 0;
            },

            // void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, const void* data, size_t size)
            // Wasm sig: (i32, i32, i64, i32, i32) -> void.
            // Clang's wasm32 target passes i64 as a single param (BigInt in JS),
            // size_t as i32. No _refreshViews — we slice self.memory.buffer directly.
            queue_write_buffer(queue, buffer, bufferOffset, dataPtr, size) {
                const q = gpuH(queue);
                const buf = gpuH(buffer);
                if (!q || !buf) return;
                const off = Number(bufferOffset);
                const src = new Uint8Array(self.memory.buffer, dataPtr, size);
                if (self._drawTrace) {
                    self._drawTrace.push(`writeBuffer(buf#${buffer}, off=${off}, size=${size})`);
                }
                self._drawStats.writes = (self._drawStats.writes | 0) + 1;
                q.writeBuffer(buf, off, src);
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

            // void wgpuCommandEncoderCopyBufferToBuffer(encoder, source, srcOff:u64, dest, dstOff:u64, size:u64)
            command_encoder_copy_buffer_to_buffer(encoder, source, sourceOffset, destination, destinationOffset, size) {
                const enc = gpuH(encoder);
                if (!enc) return;
                const src = gpuH(source);
                const dst = gpuH(destination);
                enc.copyBufferToBuffer(src, Number(sourceOffset), dst, Number(destinationOffset), Number(size));
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
                const pass = passH(encoder);
                if (pass) pass.setPipeline(gpuH(pipeline));
            },

            // void wgpuRenderPassEncoderSetBindGroup(encoder, groupIndex, group, dynamicOffsetCount, dynamicOffsets)
            render_pass_set_bind_group(encoder, groupIndex, group, dynamicOffsetCount, dynamicOffsetsPtr) {
                const pass = passH(encoder);
                if (!pass) return;
                const bg = gpuH(group);
                if (dynamicOffsetCount === 0) {
                    // Hot path: PanGui never uses dynamic offsets. Skip the
                    // _refreshViews call and the per-call offsets array alloc.
                    pass.setBindGroup(groupIndex, bg);
                    return;
                }
                self._refreshViews();
                const offsets = new Array(dynamicOffsetCount);
                for (let i = 0; i < dynamicOffsetCount; i++) {
                    offsets[i] = self._readU32(dynamicOffsetsPtr + i * 4);
                }
                pass.setBindGroup(groupIndex, bg, offsets);
            },

            // void wgpuRenderPassEncoderSetVertexBuffer(encoder, slot, buffer, offset:u64, size:u64)
            // wasm32: (i32, i32, i32, i64, i64) -> void. i64 → BigInt in JS.
            render_pass_set_vertex_buffer(encoder, slot, buffer, offset, size) {
                const pass = passH(encoder);
                if (!pass) return;
                const buf = gpuH(buffer);
                const off = Number(offset);
                const sz  = Number(size);
                pass.setVertexBuffer(slot, buf, off, sz === 0 ? undefined : sz);
            },

            // void wgpuRenderPassEncoderSetIndexBuffer(encoder, buffer, format, offset:u64, size:u64)
            render_pass_set_index_buffer(encoder, buffer, format, offset, size) {
                const pass = passH(encoder);
                if (!pass) return;
                const buf = gpuH(buffer);
                const fmt = WGPU_INDEX_FORMAT[format] || "uint16";
                const off = Number(offset);
                const sz  = Number(size);
                pass.setIndexBuffer(buf, fmt, off, sz === 0 ? undefined : sz);
            },

            // void wgpuRenderPassEncoderSetViewport(encoder, x, y, width, height, minDepth, maxDepth)
            render_pass_set_viewport(encoder, x, y, width, height, minDepth, maxDepth) {
                const pass = passH(encoder);
                if (pass) pass.setViewport(x, y, width, height, minDepth, maxDepth);
            },

            // void wgpuRenderPassEncoderSetScissorRect(encoder, x, y, width, height)
            render_pass_set_scissor_rect(encoder, x, y, width, height) {
                const pass = passH(encoder);
                if (!pass) return;
                if (self._drawTrace) {
                    self._drawTrace.push(`scissor(${x},${y},${width},${height})`);
                }
                pass.setScissorRect(x, y, width, height);
            },

            // void wgpuRenderPassEncoderDraw(encoder, vertexCount, instanceCount, firstVertex, firstInstance)
            render_pass_draw(encoder, vertexCount, instanceCount, firstVertex, firstInstance) {
                const pass = passH(encoder);
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
                const pass = passH(encoder);
                if (pass) pass.drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
            },

            // void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder encoder)
            render_pass_end(encoder) {
                const pass = passH(encoder);
                if (pass) pass.end();
                // Invalidate cache so a subsequent pass with an unrelated
                // handle that happens to hash to the same integer can't
                // collide.
                cachedPassH = 0;
                cachedPassObj = null;
            },

            // --- Compute Pass Encoder functions ---

            compute_pass_set_pipeline(encoder, pipeline) {
                const pass = gpuH(encoder);
                if (pass) pass.setPipeline(gpuH(pipeline));
            },

            compute_pass_set_bind_group(encoder, groupIndex, group, dynamicOffsetCount, dynamicOffsetsPtr) {
                const pass = gpuH(encoder);
                if (!pass) return;
                const bg = gpuH(group);
                if (dynamicOffsetCount === 0) {
                    pass.setBindGroup(groupIndex, bg);
                    return;
                }
                self._refreshViews();
                const offsets = new Array(dynamicOffsetCount);
                for (let i = 0; i < dynamicOffsetCount; i++) {
                    offsets[i] = self._readU32(dynamicOffsetsPtr + i * 4);
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

            // void* wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size)
            // wasm32: size_t is i32 (NOT i64). Signature: (i32, i32, i32) -> i32.
            // Full impl requires bouncing JS ArrayBuffer into linear memory; stub 0.
            buffer_get_mapped_range(buffer, offset, size) {
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

                // Fill the viewport, but allow the host page to inset the canvas
                // (e.g. to make room for a stats bar) via CSS variables.
                // IMPORTANT: canvas is a *replaced* element — with position:fixed and
                // only inset (top/left/right/bottom) set, browsers size it from its
                // intrinsic width/height attributes (in px), which would feed back
                // through DPR on each resize and explode the backing buffer. Always
                // pin an explicit CSS width/height.
                canvas.style.position = "fixed";
                canvas.style.left   = "var(--wapi-canvas-left, 0px)";
                canvas.style.top    = "var(--wapi-canvas-top, 0px)";
                canvas.style.width  = "calc(100vw - var(--wapi-canvas-left, 0px) - var(--wapi-canvas-right, 0px))";
                canvas.style.height = "calc(100vh - var(--wapi-canvas-top, 0px) - var(--wapi-canvas-bottom, 0px))";
                canvas.style.display = "block";

                document.body.appendChild(canvas);
                document.title = title;

                const rect = canvas.getBoundingClientRect();
                const cw = width || rect.width || window.innerWidth;
                const ch = height || rect.height || window.innerHeight;
                canvas.width = Math.round(cw * dpr);
                canvas.height = Math.round(ch * dpr);

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
                    const r = canvas.getBoundingClientRect();
                    canvas.width = Math.round(r.width * newDpr);
                    canvas.height = Math.round(r.height * newDpr);
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
            // ---- Endpoint lifecycle (endpoints come from ROLE_REQUEST) --
            close(handle) { return WAPI_OK; },
            role_kind(handle) {
                if (handle === WAPI_INPUT_HANDLE_MOUSE)    return WAPI_ROLE_MOUSE;
                if (handle === WAPI_INPUT_HANDLE_KEYBOARD) return WAPI_ROLE_KEYBOARD;
                if (handle === WAPI_INPUT_HANDLE_POINTER)  return WAPI_ROLE_POINTER;
                return -1;
            },
            uid(handle, uidPtr) {
                self._refreshViews();
                for (let i = 0; i < 16; i++) self._u8[uidPtr + i] = 0;
                self._u8[uidPtr + 0] = handle & 0xFF;
                return WAPI_OK;
            },
            name(handle, bufPtr, bufLen, nameLenPtr) {
                let text = "unknown";
                if (handle === WAPI_INPUT_HANDLE_MOUSE)    text = "Browser Mouse";
                else if (handle === WAPI_INPUT_HANDLE_KEYBOARD) text = "Browser Keyboard";
                else if (handle === WAPI_INPUT_HANDLE_POINTER)  text = "Browser Pointer";
                const written = self._writeString(bufPtr, bufLen, text);
                self._writeU32(nameLenPtr, written);
                return WAPI_OK;
            },
            seat(handle) { return 0; },

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

            // ---- HID -- post-grant metadata + report I/O (endpoints come from ROLE_REQUEST) -
            hid_endpoint_info(handle, infoOut, nameBuf, nameCap, nameLenOut)    { return WAPI_ERR_NOTSUP; },
            hid_serial(handle, bufPtr, bufLen, outLenPtr)                       { return WAPI_ERR_NOENT; },
            hid_report_descriptor(handle, bufPtr, bufLen, outLenPtr)            { return WAPI_ERR_NOTSUP; },
            hid_send_report(handle, reportId, dataPtr, dataLen)                 { return WAPI_ERR_NOTSUP; },
            hid_send_feature_report(handle, reportId, dataPtr, dataLen)         { return WAPI_ERR_NOTSUP; },
            hid_receive_report(handle, bufPtr, bufLen, bytesReadPtr)            { return WAPI_ERR_NOTSUP; },

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
        // AudioContext creation is gated on the module's first audio call
        // (role request or create_stream). We never call ctx.resume() from
        // a non-gesture path — Chrome prints an autoplay warning if we do.
        // Instead, window/document-level listeners retry resume() on every
        // pointer/key event; resume() on a running ctx is a no-op, so these
        // stay attached permanently.
        //
        // Audio output uses AudioWorkletNode (not ScriptProcessorNode — SPN
        // is deprecated and runs on the main thread). The worklet processor
        // is loaded once per context from a blob URL. bind_stream is sync
        // from wasm but addModule is async, so we kick off module loading
        // in ensureAudioCtx and defer actual node construction until the
        // promise resolves; queued data waits in stream.pending until then.
        const _wapiAudioWorkletSrc = `
            class WapiStreamProcessor extends AudioWorkletProcessor {
                constructor() {
                    super();
                    this._queue = [];
                    this._channels = 1;
                    this.port.onmessage = (e) => {
                        const m = e.data;
                        if (m.cmd === 'push')    this._queue.push(m.samples);
                        else if (m.cmd === 'clear') this._queue = [];
                        else if (m.cmd === 'channels') this._channels = m.n|0 || 1;
                    };
                }
                process(inputs, outputs, params) {
                    const out = outputs[0];
                    const frames = out[0].length;
                    const channels = this._channels;
                    let written = 0;
                    while (written < frames && this._queue.length > 0) {
                        const chunk = this._queue[0];
                        const chunkFrames = (chunk.length / channels) | 0;
                        const copy = Math.min(frames - written, chunkFrames);
                        for (let ch = 0; ch < out.length; ch++) {
                            const dst = out[ch];
                            const srcCh = Math.min(ch, channels - 1);
                            for (let i = 0; i < copy; i++) {
                                dst[written + i] = chunk[i * channels + srcCh] || 0;
                            }
                        }
                        written += copy;
                        if (copy >= chunkFrames) this._queue.shift();
                        else this._queue[0] = chunk.subarray(copy * channels);
                    }
                    for (let ch = 0; ch < out.length; ch++) {
                        const dst = out[ch];
                        for (let i = written; i < frames; i++) dst[i] = 0;
                    }
                    return true;
                }
            }
            registerProcessor('wapi-stream', WapiStreamProcessor);
        `;

        function ensureAudioCtx() {
            if (self._audioCtx) return self._audioCtx;
            const AC = window.AudioContext || window.webkitAudioContext;
            if (!AC) return null;
            const ctx = new AC();
            self._audioCtx = ctx;
            console.log(`[WAPI] AudioContext created (state=${ctx.state}, sampleRate=${ctx.sampleRate})`);

            const unlock = () => {
                if (ctx.state !== "running") {
                    ctx.resume().then(
                        () => console.log("[WAPI] AudioContext resumed by user gesture"),
                        (e) => console.warn("[WAPI] AudioContext resume failed:", e),
                    );
                }
            };
            for (const t of [window, document]) {
                for (const e of ["pointerdown", "keydown", "touchstart", "mousedown", "click"]) {
                    try { t.addEventListener(e, unlock, { capture: true, passive: true }); }
                    catch { /* passive rejection on some targets — ignore */ }
                }
            }

            // Kick off the worklet module. Streams that bind before this
            // resolves enqueue their chunks in stream.buffer; the resolve
            // handler materializes their AudioWorkletNodes and drains the
            // backlog.
            const blobUrl = URL.createObjectURL(
                new Blob([_wapiAudioWorkletSrc], { type: "application/javascript" })
            );
            self._audioWorkletReady = ctx.audioWorklet.addModule(blobUrl).then(
                () => {
                    console.log("[WAPI] AudioWorklet module ready");
                    self._audioWorkletReadyFlag = true;
                    for (const s of self._audioPendingBinds) _wapiMaterializeStream(s);
                    self._audioPendingBinds.length = 0;
                },
                (e) => {
                    console.error("[WAPI] AudioWorklet addModule failed:", e);
                    self._audioWorkletReadyFlag = false;
                },
            );
            return ctx;
        }

        // Build an AudioWorkletNode for a stream that was bound before the
        // worklet module resolved. Connects to destination, flushes any
        // chunks queued in stream.buffer to the node's port, and stores
        // the node on the stream so future put_stream_data goes straight
        // to the audio thread.
        function _wapiMaterializeStream(stream) {
            const ctx = self._audioCtx;
            if (!ctx || stream.node) return;
            const node = new AudioWorkletNode(ctx, "wapi-stream", {
                numberOfInputs: 0,
                numberOfOutputs: 1,
                outputChannelCount: [Math.max(1, stream.channels | 0)],
            });
            node.port.postMessage({ cmd: "channels", n: stream.channels });
            node.connect(ctx.destination);
            stream.node = node;
            // Drain any samples queued before the node existed.
            for (const chunk of stream.buffer) {
                node.port.postMessage({ cmd: "push", samples: chunk }, [chunk.buffer]);
            }
            stream.buffer.length = 0;
        }

        if (!self._audioPendingBinds) self._audioPendingBinds = [];

        // Shared creator used by the ROLE_REQUEST handler to allocate an
        // audio endpoint handle on behalf of role dispatch.
        function wapi_audio_open_default_endpoint() {
            if (!ensureAudioCtx()) return 0;
            return self.handles.insert({
                type: "audio_device",
                ctx: self._audioCtx,
                streams: [],
                paused: true,
            });
        }

        const wapi_audio = {
            endpoint_info(handle, infoOut, nameBuf, nameCap, nameLenOut) {
                const obj = self.handles.get(handle);
                if (!obj || obj.type !== "audio_device") return WAPI_ERR_BADF;
                self._refreshViews();
                // wapi_audio_endpoint_info_t (32B): native_spec(12) + form(4) + uid[16]
                const sr = obj.ctx ? (obj.ctx.sampleRate | 0) : 48000;
                self._writeU32(infoOut + 0, WAPI_AUDIO_F32);
                self._writeI32(infoOut + 4, 2);
                self._writeI32(infoOut + 8, sr);
                self._writeU32(infoOut + 12, 0); // form: unknown
                for (let i = 0; i < 16; i++) self._u8[infoOut + 16 + i] = 0;
                const text = "Default Audio Device";
                const written = self._writeString(nameBuf, nameCap, text);
                self._writeU64(nameLenOut, BigInt(text.length));
                return WAPI_OK;
            },

            close(handle) {
                const obj = self.handles.remove(handle);
                if (!obj) return WAPI_ERR_BADF;
                // Shared AudioContext is kept alive across handles.
                return WAPI_OK;
            },

            resume(handle) {
                const obj = self.handles.get(handle);
                if (!obj || obj.type !== "audio_device") return WAPI_ERR_BADF;
                obj.paused = false;
                // Don't call ctx.resume() here — wapi_main is not a user
                // gesture, so Chrome would print an autoplay warning. The
                // gesture listener in ensureAudioCtx handles resume.
                return WAPI_OK;
            },

            pause(handle) {
                const obj = self.handles.get(handle);
                if (!obj || obj.type !== "audio_device") return WAPI_ERR_BADF;
                obj.paused = true;
                return WAPI_OK;
            },

            create_stream(srcSpecPtr, dstSpecPtr, streamOutPtr) {
                self._refreshViews();
                const srcFormat   = self._readU32(srcSpecPtr + 0);
                const srcChannels = self._readI32(srcSpecPtr + 4);
                const srcFreq     = self._readI32(srcSpecPtr + 8);

                if (!ensureAudioCtx()) return WAPI_ERR_NOTCAPABLE;

                const h = self.handles.insert({
                    type: "audio_stream",
                    srcFormat,
                    channels: srcChannels || 1,
                    sampleRate: srcFreq,
                    buffer: [],        // pending Float32 chunks before node exists
                    node: null,
                    deviceHandle: null,
                    // Time-based queue accounting. endTime is the absolute
                    // ctx.currentTime at which the last sample pushed so far
                    // will have finished playing. stream_queued answers from
                    // this, converted back to source bytes.
                    endTime: 0,
                });
                self._writeI32(streamOutPtr, h);
                return WAPI_OK;
            },

            destroy_stream(streamHandle) {
                const obj = self.handles.remove(streamHandle);
                if (!obj) return WAPI_ERR_BADF;
                if (obj.node) { obj.node.disconnect(); obj.node = null; }
                return WAPI_OK;
            },

            bind_stream(deviceHandle, streamHandle) {
                const dev = self.handles.get(deviceHandle);
                const stream = self.handles.get(streamHandle);
                if (!dev || !stream) return WAPI_ERR_BADF;

                stream.deviceHandle = deviceHandle;
                dev.streams.push(streamHandle);

                // Need an AudioWorkletNode; worklet module may not be
                // registered yet. If not, stash stream for later. Once
                // addModule resolves, _wapiMaterializeStream creates the
                // node and flushes stream.buffer to its port.
                if (self._audioWorkletReadyFlag) {
                    _wapiMaterializeStream(stream);
                    console.log(`[WAPI] bound stream ${streamHandle} (${stream.channels}ch @ ${stream.sampleRate}Hz) — node ready`);
                } else {
                    self._audioPendingBinds.push(stream);
                    console.log(`[WAPI] bound stream ${streamHandle} (${stream.channels}ch @ ${stream.sampleRate}Hz) — deferred until worklet ready`);
                }
                return WAPI_OK;
            },

            unbind_stream(streamHandle) {
                const stream = self.handles.get(streamHandle);
                if (!stream) return WAPI_ERR_BADF;
                if (stream.node) { stream.node.disconnect(); stream.node = null; }
                const idx = self._audioPendingBinds.indexOf(stream);
                if (idx >= 0) self._audioPendingBinds.splice(idx, 1);
                stream.deviceHandle = null;
                return WAPI_OK;
            },

            // wapi_result_t wapi_audio_put_stream_data(handle stream, const void* buf, wapi_size_t len)
            // wapi_size_t is uint64_t — JS receives it as a BigInt. Normalize to Number.
            put_stream_data(streamHandle, bufPtr, lenArg) {
                const stream = self.handles.get(streamHandle);
                if (!stream || stream.type !== "audio_stream") return WAPI_ERR_BADF;

                self._refreshViews();
                const srcFormat = stream.srcFormat;
                const len = Number(lenArg);

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

                // The worklet's output runs at ctx.sampleRate. Resample
                // the stream's declared rate to match, otherwise playback
                // pitch/duration drifts (22050 Hz source played at 48 kHz
                // sounds ~2× fast and half as long).
                const ctxRate = self._audioCtx ? self._audioCtx.sampleRate : stream.sampleRate;
                if (ctxRate !== stream.sampleRate && stream.sampleRate > 0) {
                    const channels = stream.channels;
                    const srcFrames = (floats.length / channels) | 0;
                    const ratio = stream.sampleRate / ctxRate;
                    const dstFrames = Math.max(1, Math.floor(srcFrames / ratio));
                    const resampled = new Float32Array(dstFrames * channels);
                    for (let f = 0; f < dstFrames; f++) {
                        const s = f * ratio;
                        const i0 = Math.floor(s);
                        const i1 = Math.min(i0 + 1, srcFrames - 1);
                        const t = s - i0;
                        for (let c = 0; c < channels; c++) {
                            const a = floats[i0 * channels + c];
                            const b = floats[i1 * channels + c];
                            resampled[f * channels + c] = a + (b - a) * t;
                        }
                    }
                    floats = resampled;
                }

                // Time-based queue accounting. Used by stream_queued so the
                // caller (e.g. pump_music) can pace its pushes against what
                // the audio thread has yet to consume.
                const ctx = self._audioCtx;
                if (ctx) {
                    const durSec = (floats.length / stream.channels) / ctxRate;
                    if (stream.endTime < ctx.currentTime) stream.endTime = ctx.currentTime;
                    stream.endTime += durSec;
                }

                if (stream.node) {
                    // Transfer the underlying buffer to the worklet thread
                    // (zero copy). floats is a freshly-allocated array, so
                    // detaching it is safe.
                    stream.node.port.postMessage(
                        { cmd: "push", samples: floats },
                        [floats.buffer],
                    );
                } else {
                    // Node not yet materialized — queue for drain in
                    // _wapiMaterializeStream.
                    stream.buffer.push(floats);
                }
                return WAPI_OK;
            },

            get_stream_data(streamHandle, bufPtr, len, bytesReadPtr) {
                // Recording: not yet implemented
                self._writeU32(bytesReadPtr, 0);
                return WAPI_ERR_NOTSUP;
            },

            stream_available(streamHandle) {
                // Playback stream: capture path has nothing to offer.
                return 0;
            },

            // int32_t: bytes currently queued in SOURCE format, matching
            // what put_stream_data was given. Lets callers pace pushes
            // without knowing about internal resampling.
            stream_queued(streamHandle) {
                const stream = self.handles.get(streamHandle);
                if (!stream) return 0;
                const ctx = self._audioCtx;
                if (!ctx) return 0;
                const remainingSec = Math.max(0, stream.endTime - ctx.currentTime);
                const bytesPerSample = _wapiAudioBytesPerSample(stream.srcFormat);
                if (!bytesPerSample) return 0;
                const bytesPerSec = stream.sampleRate * bytesPerSample * stream.channels;
                return Math.floor(remainingSec * bytesPerSec) | 0;
            },

        };

        // -------------------------------------------------------------------
        // env: vanilla webgpu.h direct imports (wasm32 C ABI)
        // -------------------------------------------------------------------
        // Guests that #include <webgpu/webgpu.h> without adding import-module
        // attributes emit `wgpuXxx` names under module "env". Mirrors
        // wapi_host_wgpu.c:wapi_host_register_wgpu on the desktop side.
        // Per-resource ops delegate to the wapi_wgpu snake_case helpers;
        // instance/adapter/surface/queue acquisition lives here.

        let _wgpuInstanceHandle = 0;
        const _wgpuPendingCBs = [];
        let _wgpuNextFuture = 1n;

        const _wgpuPushStringView = (msg) => {
            let strPtr = 0, len = 0;
            if (msg) {
                const encoded = new TextEncoder().encode(msg);
                len = encoded.length;
                strPtr = self._hostAlloc(len || 1, 1);
                self._refreshViews();
                self._u8.set(encoded, strPtr);
            }
            const svPtr = self._hostAlloc(8, 4);
            self._refreshViews();
            self._dv.setUint32(svPtr + 0, strPtr, true);
            self._dv.setUint32(svPtr + 4, len, true);
            return svPtr;
        };

        const _wgpuEnqueueCB = (funcref, status, handle, msg, ud1, ud2) => {
            _wgpuPendingCBs.push({
                funcref, status, handle,
                msgSvPtr: _wgpuPushStringView(msg),
                ud1, ud2,
            });
        };

        const env = {
            /* ---- Instance / Adapter / Device / Queue ---- */
            wgpuCreateInstance(_descPtr) {
                if (!_wgpuInstanceHandle) {
                    _wgpuInstanceHandle = self.handles.insert({
                        type: "gpu_instance",
                        gpuObj: { _isWgpuInstance: true },
                    });
                }
                return _wgpuInstanceHandle;
            },

            wgpuInstanceRelease(_instance) { return 0; },

            wgpuInstanceProcessEvents(_instance) {
                const table = self.instance.exports.__indirect_function_table;
                if (!table) return;
                while (_wgpuPendingCBs.length > 0) {
                    const p = _wgpuPendingCBs.shift();
                    try {
                        const fn = table.get(p.funcref);
                        if (fn) fn(p.status, p.handle, p.msgSvPtr, p.ud1, p.ud2);
                    } catch (e) {
                        console.error("[wgpu] callback trap:", e);
                    }
                }
            },

            /* WGPURequestAdapterCallbackInfo wasm32:
             *   +0 nextInChain, +4 mode, +8 callback (funcref),
             *  +12 userdata1, +16 userdata2 */
            wgpuInstanceRequestAdapter(_instance, _optsPtr, cbInfoPtr) {
                self._refreshViews();
                const funcref = self._readU32(cbInfoPtr + 8);
                const ud1     = self._readU32(cbInfoPtr + 12);
                const ud2     = self._readU32(cbInfoPtr + 16);
                const adapter = self._gpuAdapter;
                let handle = 0;
                const status = adapter ? 1 : 0; /* Success : Unavailable */
                if (adapter) {
                    handle = self.handles.insert({ type: "gpu_adapter", gpuObj: adapter });
                }
                _wgpuEnqueueCB(funcref, status, handle, null, ud1, ud2);
                return _wgpuNextFuture++;
            },

            wgpuAdapterRequestDevice(_adapter, _descPtr, cbInfoPtr) {
                self._refreshViews();
                const funcref = self._readU32(cbInfoPtr + 8);
                const ud1     = self._readU32(cbInfoPtr + 12);
                const ud2     = self._readU32(cbInfoPtr + 16);
                const device = self._gpuDevice;
                let handle = 0;
                const status = device ? 1 : 0; /* Success : Error */
                if (device) {
                    handle = self.handles.insert({ type: "gpu_device", gpuObj: device });
                }
                _wgpuEnqueueCB(funcref, status, handle, null, ud1, ud2);
                return _wgpuNextFuture++;
            },

            wgpuDeviceGetQueue(device) {
                const entry = self.handles.get(device);
                const dev = (entry && entry.gpuObj) ? entry.gpuObj : self._gpuDevice;
                if (!dev) return 0;
                return self.handles.insert({ type: "gpu_queue", gpuObj: dev.queue });
            },

            wgpuDeviceRelease(_h)  { return 0; },
            wgpuAdapterRelease(_h) { return 0; },
            wgpuQueueRelease(_h)   { return 0; },

            /* ---- Surface ---- */
            /* WGPUSurfaceDescriptor wasm32:
             *   +0 u32 nextInChain, +4 WGPUStringView label (8).
             * Chain walk for WAPI_STYPE_GPU_SURFACE_SOURCE_WAPI (0x0101):
             *   wapi_chain_t: u64 next + u32 sType + u32 _pad (16B)
             *   wapi_gpu_surface_source_t: chain(16) + i32 surface + u32 _pad (24B) */
            wgpuInstanceCreateSurface(_instance, descPtr) {
                self._refreshViews();
                let chainPtr = self._readU32(descPtr + 0);
                let wapiSurfaceHandle = 0;
                while (chainPtr) {
                    const next  = Number(self._dv.getBigUint64(chainPtr + 0, true));
                    const stype = self._readU32(chainPtr + 8);
                    if (stype === 0x0101) {
                        wapiSurfaceHandle = self._dv.getInt32(chainPtr + 16, true);
                        break;
                    }
                    chainPtr = next;
                }
                const surfInfo = self._surfaces.get(wapiSurfaceHandle);
                if (!surfInfo) {
                    console.error("[wgpu] wgpuInstanceCreateSurface: WAPI surface chain missing");
                    return 0;
                }
                return self.handles.insert({
                    type: "gpu_surface",
                    gpuObj: { canvas: surfInfo.canvas, ctx: null, config: null },
                    wapiSurface: wapiSurfaceHandle,
                });
            },

            wgpuSurfaceRelease(surface) {
                self.handles.remove(surface);
                return 0;
            },

            /* WGPUSurfaceConfiguration wasm32 (48B):
             *  +0 nextInChain, +4 device, +8 format, +16 usage(u64),
             * +24 width, +28 height, +32 viewFormatCount, +36 viewFormats,
             * +40 alphaMode, +44 presentMode */
            wgpuSurfaceConfigure(surface, cfgPtr) {
                self._refreshViews();
                const entry = self.handles.get(surface);
                if (!entry || !entry.gpuObj) return;
                const deviceH = self._readI32(cfgPtr + 4);
                const formatId = self._readU32(cfgPtr + 8);
                const width  = self._readU32(cfgPtr + 24);
                const height = self._readU32(cfgPtr + 28);
                const alphaId = self._readU32(cfgPtr + 40);
                const devEntry = self.handles.get(deviceH);
                const device = (devEntry && devEntry.gpuObj) || self._gpuDevice;
                if (!device) return;
                const format = tpFormatToGPU(formatId);
                /* WGPUCompositeAlphaMode → GPUCanvasAlphaMode.
                 *   0=Auto, 1=Opaque, 2=Premultiplied, 3=Unpremultiplied, 4=Inherit.
                 * Browser accepts only "opaque" | "premultiplied"; others collapse. */
                const alphaMode = (alphaId === 2 || alphaId === 3) ? "premultiplied" : "opaque";
                const ctx = entry.gpuObj.canvas.getContext("webgpu");
                if (!ctx) return;
                if (width > 0)  entry.gpuObj.canvas.width  = width;
                if (height > 0) entry.gpuObj.canvas.height = height;
                ctx.configure({ device, format, alphaMode });
                entry.gpuObj.ctx = ctx;
                entry.gpuObj.config = { device, format, alphaMode, width, height };
            },

            /* WGPUSurfaceCapabilities wasm32 (40B):
             *   +0 next/_pad(8), +8 usages(u64),
             *  +16 formatCount, +20 formats, +24 presentModeCount, +28 presentModes,
             *  +32 alphaModeCount, +36 alphaModes */
            wgpuSurfaceGetCapabilities(_surface, _adapter, outPtr) {
                self._refreshViews();
                const preferred = navigator.gpu.getPreferredCanvasFormat();
                const formatsOff = self._hostAlloc(4, 4);
                self._refreshViews();
                self._writeU32(formatsOff, gpuFormatToTP(preferred));
                const pmOff = self._hostAlloc(4, 4);
                self._refreshViews();
                self._writeU32(pmOff, 2); /* WGPUPresentMode_Fifo */
                const amOff = self._hostAlloc(8, 4);
                self._refreshViews();
                self._writeU32(amOff + 0, 2); /* Opaque */
                self._writeU32(amOff + 4, 3); /* Premultiplied */
                self._writeU32(outPtr + 0, 0);
                self._writeU32(outPtr + 4, 0);
                self._dv.setBigUint64(outPtr + 8, 0x10n, true); /* RenderAttachment */
                self._writeU32(outPtr + 16, 1);
                self._writeU32(outPtr + 20, formatsOff);
                self._writeU32(outPtr + 24, 1);
                self._writeU32(outPtr + 28, pmOff);
                self._writeU32(outPtr + 32, 2);
                self._writeU32(outPtr + 36, amOff);
                return 1; /* WGPUStatus_Success */
            },

            /* WGPUSurfaceTexture wasm32:
             *   +0 nextInChain, +4 texture handle, +8 status */
            wgpuSurfaceGetCurrentTexture(surface, outPtr) {
                self._refreshViews();
                const entry = self.handles.get(surface);
                if (!entry || !entry.gpuObj || !entry.gpuObj.ctx) {
                    self._writeU32(outPtr + 0, 0);
                    self._writeU32(outPtr + 4, 0);
                    self._writeU32(outPtr + 8, 4); /* Timeout */
                    return;
                }
                const texture = entry.gpuObj.ctx.getCurrentTexture();
                const th = self.handles.insert({ type: "gpu_texture", gpuObj: texture });
                self._writeU32(outPtr + 0, 0);
                self._writeU32(outPtr + 4, th);
                self._writeU32(outPtr + 8, 1); /* SuccessOptimal */
            },

            wgpuSurfacePresent(_surface) { return 0; },

            /* ---- Per-resource ops: delegate to wapi_wgpu snake_case impls ---- */
            wgpuDeviceCreateShaderModule:        wapi_wgpu.device_create_shader_module,
            wgpuDeviceCreateBuffer:              wapi_wgpu.device_create_buffer,
            wgpuDeviceCreateTexture:             wapi_wgpu.device_create_texture,
            wgpuDeviceCreateSampler:             wapi_wgpu.device_create_sampler,
            wgpuDeviceCreateBindGroupLayout:     wapi_wgpu.device_create_bind_group_layout,
            wgpuDeviceCreatePipelineLayout:      wapi_wgpu.device_create_pipeline_layout,
            wgpuDeviceCreateBindGroup:           wapi_wgpu.device_create_bind_group,
            wgpuDeviceCreateRenderPipeline:      wapi_wgpu.device_create_render_pipeline,
            wgpuDeviceCreateComputePipeline:     wapi_wgpu.device_create_compute_pipeline,
            wgpuDeviceCreateQuerySet:            wapi_wgpu.device_create_query_set,
            wgpuDeviceCreateCommandEncoder:      wapi_wgpu.device_create_command_encoder,
            wgpuCommandEncoderBeginRenderPass:   wapi_wgpu.command_encoder_begin_render_pass,
            wgpuCommandEncoderBeginComputePass:  wapi_wgpu.command_encoder_begin_compute_pass,
            wgpuCommandEncoderCopyBufferToBuffer:  wapi_wgpu.command_encoder_copy_buffer_to_buffer,
            wgpuCommandEncoderCopyBufferToTexture: wapi_wgpu.command_encoder_copy_buffer_to_texture,
            wgpuCommandEncoderCopyTextureToBuffer: wapi_wgpu.command_encoder_copy_texture_to_buffer,
            wgpuCommandEncoderCopyTextureToTexture: wapi_wgpu.command_encoder_copy_texture_to_texture,
            wgpuCommandEncoderFinish:            wapi_wgpu.command_encoder_finish,
            wgpuRenderPassEncoderSetPipeline:    wapi_wgpu.render_pass_set_pipeline,
            wgpuRenderPassEncoderSetBindGroup:   wapi_wgpu.render_pass_set_bind_group,
            wgpuRenderPassEncoderSetVertexBuffer:wapi_wgpu.render_pass_set_vertex_buffer,
            wgpuRenderPassEncoderSetIndexBuffer: wapi_wgpu.render_pass_set_index_buffer,
            wgpuRenderPassEncoderDraw:           wapi_wgpu.render_pass_draw,
            wgpuRenderPassEncoderDrawIndexed:    wapi_wgpu.render_pass_draw_indexed,
            wgpuRenderPassEncoderEnd:            wapi_wgpu.render_pass_end,
            wgpuRenderPassEncoderRelease:        wapi_wgpu.render_pass_encoder_release,
            wgpuRenderPassEncoderSetScissorRect: wapi_wgpu.render_pass_set_scissor_rect,
            wgpuRenderPassEncoderSetViewport:    wapi_wgpu.render_pass_set_viewport,
            wgpuComputePassEncoderSetPipeline:   wapi_wgpu.compute_pass_set_pipeline,
            wgpuComputePassEncoderSetBindGroup:  wapi_wgpu.compute_pass_set_bind_group,
            wgpuComputePassEncoderDispatchWorkgroups: wapi_wgpu.compute_pass_dispatch_workgroups,
            wgpuComputePassEncoderEnd:           wapi_wgpu.compute_pass_end,
            wgpuComputePassEncoderRelease:       wapi_wgpu.compute_pass_release,
            wgpuQueueSubmit:                     wapi_wgpu.queue_submit,
            wgpuQueueWriteBuffer:                wapi_wgpu.queue_write_buffer,
            wgpuQueueWriteTexture:               wapi_wgpu.queue_write_texture,
            wgpuBufferGetMappedRange:            wapi_wgpu.buffer_get_mapped_range,
            wgpuBufferUnmap:                     wapi_wgpu.buffer_unmap,
            wgpuBufferRelease:                   wapi_wgpu.buffer_release,
            wgpuCommandBufferRelease:            wapi_wgpu.command_buffer_release,
            wgpuCommandEncoderRelease:           wapi_wgpu.command_encoder_release,
            wgpuTextureCreateView:               wapi_wgpu.texture_create_view,
            wgpuTextureViewRelease:              wapi_wgpu.texture_view_release,
            wgpuTextureRelease:                  wapi_wgpu.texture_release,
            wgpuBindGroupRelease:                wapi_wgpu.bind_group_release,
            wgpuBindGroupLayoutRelease:          wapi_wgpu.bind_group_layout_release,
            wgpuPipelineLayoutRelease:           wapi_wgpu.pipeline_layout_release,
            wgpuSamplerRelease:                  wapi_wgpu.sampler_release,
            wgpuShaderModuleRelease:             wapi_wgpu.shader_module_release,
            wgpuRenderPipelineRelease:           wapi_wgpu.render_pipeline_release,
            wgpuComputePipelineRelease:          wapi_wgpu.compute_pipeline_release,
            wgpuQuerySetRelease:                 wapi_wgpu.query_set_release,
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
        // Canvas 2D measureText-based shaping + a simple word-wrap layout.
        // This is a pragmatic baseline: we don't emit per-glyph OpenType
        // indices (the browser doesn't expose them), but we can answer
        // metrics / line break / hit-test queries well enough for UI text.
        const _ensureTextCanvas = () => {
            if (!self._textCanvas) {
                self._textCanvas = typeof OffscreenCanvas !== "undefined"
                    ? new OffscreenCanvas(4, 4)
                    : document.createElement("canvas");
                self._textCtx = self._textCanvas.getContext("2d");
            }
            return self._textCtx;
        };
        const readTextFontDesc = (ptr) => {
            // wapi_text_font_desc_t ~40 bytes: family stringview(16), size_px f32, weight u32, style u32, features sv? Keep simple:
            self._refreshViews();
            const family = self._readStringView(ptr + 0) || "sans-serif";
            const size   = self._dv.getFloat32(ptr + 16, true) || 16;
            const weight = self._dv.getUint32(ptr + 20, true) || 400;
            const style  = self._dv.getUint32(ptr + 24, true) || 0;
            const styleName = style === 1 ? "italic" : (style === 2 ? "oblique" : "normal");
            return { family, size, weight, styleName, cssFont: `${styleName} ${weight} ${size}px ${family}` };
        };
        const wapi_text = {
            shape(fontPtr, textSvPtr, _script, _direction) {
                const ctx = _ensureTextCanvas();
                const font = readTextFontDesc(fontPtr);
                ctx.font = font.cssFont;
                const text = self._readStringView(textSvPtr) || "";
                const metrics = ctx.measureText(text);
                // Per-char advances — good enough for monospace; for proportional
                // text we fall back to per-character measureText.
                const glyphs = [];
                let x = 0;
                for (const ch of text) {
                    const m = ctx.measureText(ch);
                    glyphs.push({ ch, x, w: m.width });
                    x += m.width;
                }
                const entry = {
                    type: "shape",
                    text, font,
                    glyphs,
                    totalWidth: metrics.width,
                    ascent:  metrics.actualBoundingBoxAscent  || font.size * 0.8,
                    descent: metrics.actualBoundingBoxDescent || font.size * 0.2,
                };
                return self.handles.insert(entry);
            },
            shape_glyph_count(result) {
                const e = self.handles.get(result);
                return (e && e.type === "shape") ? e.glyphs.length : 0;
            },
            // infos: glyph_id u32, cluster u32 (8 bytes each)
            // positions: x_advance f32, y_advance f32, x_offset f32, y_offset f32 (16 bytes each)
            shape_get_glyphs(result, infosPtr, positionsPtr) {
                const e = self.handles.get(result);
                if (!e || e.type !== "shape") return WAPI_ERR_BADF;
                self._refreshViews();
                for (let i = 0; i < e.glyphs.length; i++) {
                    const g = e.glyphs[i];
                    self._dv.setUint32(infosPtr + i * 8 + 0, g.ch.codePointAt(0), true);
                    self._dv.setUint32(infosPtr + i * 8 + 4, i, true);
                    self._dv.setFloat32(positionsPtr + i * 16 + 0, g.w, true);
                    self._dv.setFloat32(positionsPtr + i * 16 + 4, 0, true);
                    self._dv.setFloat32(positionsPtr + i * 16 + 8, 0, true);
                    self._dv.setFloat32(positionsPtr + i * 16 + 12, 0, true);
                }
                return WAPI_OK;
            },
            shape_get_font_metrics(result, metricsPtr) {
                const e = self.handles.get(result);
                if (!e || e.type !== "shape") return WAPI_ERR_BADF;
                self._refreshViews();
                const dv = self._dv;
                dv.setFloat32(metricsPtr + 0,  e.ascent,  true);
                dv.setFloat32(metricsPtr + 4,  e.descent, true);
                dv.setFloat32(metricsPtr + 8,  e.ascent + e.descent, true);   // line height
                dv.setFloat32(metricsPtr + 12, e.ascent * 0.5, true);         // cap height
                dv.setFloat32(metricsPtr + 16, e.ascent * 0.4, true);         // x-height
                dv.setFloat32(metricsPtr + 20, -e.descent * 0.2, true);       // underline pos
                dv.setFloat32(metricsPtr + 24, e.font.size * 0.05, true);     // underline thickness
                dv.setFloat32(metricsPtr + 28, e.ascent * 0.6, true);         // strikethrough pos
                dv.setFloat32(metricsPtr + 32, e.font.size * 0.05, true);     // strikethrough thickness
                return WAPI_OK;
            },
            shape_destroy(result) { self.handles.remove(result); return WAPI_OK; },
            // Layout: simple word-wrap. Constraints: max_width f32, align u32, wrap u32, overflow u32 (16 bytes)
            layout_create(textRunsPtr, constraintsPtr) {
                const ctx = _ensureTextCanvas();
                // textRuns is `wapi_text_run_t*` (32 bytes each): style* + text stringview + count. Keep simple:
                // interpret textRunsPtr as a stringview directly for now.
                self._refreshViews();
                const text = self._readStringView(textRunsPtr + 16 /* offset of text sv in run */) || "";
                const maxW = self._dv.getFloat32(constraintsPtr + 0, true) || 999999;
                ctx.font = "16px sans-serif";
                const words = text.split(/(\s+)/);
                const lines = [[]];
                let lineWidth = 0;
                for (const w of words) {
                    const wWidth = ctx.measureText(w).width;
                    if (lineWidth + wWidth > maxW && lines[lines.length - 1].length > 0) {
                        lines.push([]);
                        lineWidth = 0;
                    }
                    lines[lines.length - 1].push(w);
                    lineWidth += wWidth;
                }
                const lineHeight = 20;
                const entry = {
                    type: "layout",
                    lines: lines.map((arr, i) => ({
                        text: arr.join(""),
                        width: ctx.measureText(arr.join("")).width,
                        y: i * lineHeight,
                    })),
                    lineHeight,
                    totalWidth: Math.max(...lines.map(a => ctx.measureText(a.join("")).width)),
                    totalHeight: lines.length * lineHeight,
                };
                return self.handles.insert(entry);
            },
            layout_get_size(layout, widthPtr, heightPtr) {
                const e = self.handles.get(layout);
                if (!e || e.type !== "layout") return WAPI_ERR_BADF;
                self._writeF32(widthPtr,  e.totalWidth);
                self._writeF32(heightPtr, e.totalHeight);
                return WAPI_OK;
            },
            layout_line_count(layout) {
                const e = self.handles.get(layout);
                return (e && e.type === "layout") ? e.lines.length : 0;
            },
            layout_get_line_info(layout, lineIdx, infoPtr) {
                const e = self.handles.get(layout);
                if (!e || e.type !== "layout") return WAPI_ERR_BADF;
                if (lineIdx < 0 || lineIdx >= e.lines.length) return WAPI_ERR_OVERFLOW;
                const line = e.lines[lineIdx];
                self._refreshViews();
                self._dv.setFloat32(infoPtr + 0, 0,       true); // x
                self._dv.setFloat32(infoPtr + 4, line.y,  true); // y baseline offset
                self._dv.setFloat32(infoPtr + 8, line.width, true);
                self._dv.setFloat32(infoPtr + 12, e.lineHeight, true);
                self._dv.setUint32 (infoPtr + 16, 0, true);      // start_char
                self._dv.setUint32 (infoPtr + 20, line.text.length, true); // char_count
                return WAPI_OK;
            },
            layout_hit_test(layout, x, y, resultPtr) {
                const e = self.handles.get(layout);
                if (!e || e.type !== "layout") return WAPI_ERR_BADF;
                const lineIdx = Math.max(0, Math.min(e.lines.length - 1, Math.floor(y / e.lineHeight)));
                self._refreshViews();
                self._dv.setUint32(resultPtr + 0, lineIdx, true);
                self._dv.setUint32(resultPtr + 4, 0, true); // char offset (we don't compute per-char here)
                self._dv.setFloat32(resultPtr + 8, x, true);
                self._dv.setFloat32(resultPtr + 12, e.lines[lineIdx].y, true);
                self._dv.setUint32(resultPtr + 16, 0, true);
                return WAPI_OK;
            },
            layout_get_caret(layout, charOffset, infoPtr) {
                const e = self.handles.get(layout);
                if (!e || e.type !== "layout") return WAPI_ERR_BADF;
                let remaining = charOffset, y = 0;
                for (const line of e.lines) {
                    if (remaining < line.text.length) { y = line.y; break; }
                    remaining -= line.text.length;
                    y = line.y;
                }
                self._refreshViews();
                self._dv.setFloat32(infoPtr + 0, 0, true); // x
                self._dv.setFloat32(infoPtr + 4, y, true);
                self._dv.setFloat32(infoPtr + 8, e.lineHeight, true);
                self._dv.setUint32 (infoPtr + 12, 0, true);
                return WAPI_OK;
            },
            layout_update_text(_layout, _textPtr) { return WAPI_ERR_NOTSUP; },
            layout_update_constraints(_layout, _constraintsPtr) { return WAPI_ERR_NOTSUP; },
            layout_destroy(layout) { self.handles.remove(layout); return WAPI_OK; },
        };

        // -------------------------------------------------------------------
        // wapi_transfer (clipboard / DnD / share unified)
        // Direct imports: revoke, format_count, format_name, has_format,
        // set_action. Async ops (offer, read) are dispatched via the
        // IO_OP_TRANSFER_* opcode handlers above.
        //
        // The browser shim caches its own LATENT pool in self._clipboardText
        // / self._clipboardHtml so synchronous queries (format_count etc.)
        // can answer without a Promise round-trip — the system clipboard is
        // gesture-gated and async-only.
        // -------------------------------------------------------------------
        const wapi_transfer = {
            revoke(seat, mode) {
                if (seat !== 0) return WAPI_ERR_INVAL;
                if (mode & WAPI_TRANSFER_LATENT) {
                    self._clipboardText = "";
                    self._clipboardHtml = "";
                }
                if (mode & WAPI_TRANSFER_POINTED) {
                    self._dropItems = [];
                }
                return WAPI_OK;
            },

            format_count(seat, mode) {
                if (seat !== 0) return 0n;
                if (mode === WAPI_TRANSFER_LATENT) {
                    let n = 0;
                    if (self._clipboardText.length > 0) n++;
                    if (self._clipboardHtml.length > 0) n++;
                    return BigInt(n);
                }
                if (mode === WAPI_TRANSFER_POINTED) {
                    return BigInt((self._dropItems || []).length);
                }
                return 0n;
            },

            format_name(seat, mode, index, bufPtr, bufLen, outLenPtr) {
                if (seat !== 0) return WAPI_ERR_INVAL;
                let mimes = [];
                if (mode === WAPI_TRANSFER_LATENT) {
                    if (self._clipboardText.length > 0) mimes.push("text/plain");
                    if (self._clipboardHtml.length > 0) mimes.push("text/html");
                } else if (mode === WAPI_TRANSFER_POINTED) {
                    mimes = (self._dropItems || []).map(it => it.mime);
                }
                const i = Number(index);
                if (i < 0 || i >= mimes.length) return WAPI_ERR_RANGE;
                const written = self._writeString(bufPtr, Number(bufLen), mimes[i]);
                self._writeU64(outLenPtr, BigInt(written));
                return WAPI_OK;
            },

            has_format(seat, mode, mimePtr, mimeLen) {
                if (seat !== 0) return 0;
                const mime = self._readString(mimePtr, Number(mimeLen));
                if (mode === WAPI_TRANSFER_LATENT) {
                    if (mime === "text/plain") return self._clipboardText.length > 0 ? 1 : 0;
                    if (mime === "text/html")  return self._clipboardHtml.length > 0 ? 1 : 0;
                    return 0;
                }
                if (mode === WAPI_TRANSFER_POINTED) {
                    return (self._dropItems || []).some(it => it.mime === mime) ? 1 : 0;
                }
                return 0;
            },

            set_action(_seat, _action) {
                // No active drag in this synchronous-import context.
                return WAPI_ERR_NOTSUP;
            },
        };

        // -------------------------------------------------------------------
        // wapi_seat (single-seat browser host)
        // -------------------------------------------------------------------
        const wapi_seat = {
            count() { return 1n; },
            at(_index) { return 0; /* WAPI_SEAT_DEFAULT */ },
            name(seat, bufPtr, bufLen, outLenPtr) {
                if (seat !== 0) return WAPI_ERR_INVAL;
                const written = self._writeString(bufPtr, Number(bufLen), "default");
                self._writeU64(outLenPtr, BigInt("default".length));
                return WAPI_OK;
            },
            user_id(_seat, _bufPtr, _bufLen, _outLenPtr) {
                return WAPI_ERR_NOENT;
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
        // Font enumeration. Local Font Access is async + permission-gated;
        // we only call queryLocalFonts() when the app explicitly calls a
        // family-enumeration method for the first time. Until it resolves,
        // we return the web-safe defaults.
        const DEFAULT_FONTS = ["serif", "sans-serif", "monospace", "cursive", "fantasy", "system-ui",
                               "Arial", "Times New Roman", "Courier New", "Georgia", "Verdana", "Helvetica"];
        const kickFontEnum = () => {
            if (self._fontEnumRequested) return;
            self._fontEnumRequested = true;
            if (typeof window !== "undefined" && window.queryLocalFonts) {
                window.queryLocalFonts().then((fonts) => {
                    const families = [...new Set(fonts.map(f => f.family))];
                    self._fontList = families.length > 0 ? families : DEFAULT_FONTS;
                }).catch(() => {
                    self._fontList = DEFAULT_FONTS;
                });
            }
        };
        const wapi_font = {
            family_count() {
                if (!self._fontList) self._fontList = DEFAULT_FONTS;
                return self._fontList.length;
            },
            // wapi_font_info_t (32 bytes): family stringview(16), weight_min u32, weight_max u32, supported_styles u32, is_variable i32
            family_info(index, infoPtr) {
                const fonts = self._fontList || DEFAULT_FONTS;
                if (index < 0 || index >= fonts.length) return WAPI_ERR_OVERFLOW;
                const name = fonts[index];
                self._refreshViews();
                // Allocate a little host buffer for the family name so the
                // stringview has somewhere valid to point to.
                if (!self._fontNameBufs) self._fontNameBufs = new Map();
                let cached = self._fontNameBufs.get(name);
                if (!cached) {
                    const encoded = new TextEncoder().encode(name);
                    const ptr = self._hostAlloc(encoded.length + 1, 1);
                    self._refreshViews();
                    self._u8.set(encoded, ptr);
                    self._u8[ptr + encoded.length] = 0;
                    cached = { ptr, len: encoded.length };
                    self._fontNameBufs.set(name, cached);
                }
                self._dv.setBigUint64(infoPtr + 0, BigInt(cached.ptr), true);
                self._dv.setBigUint64(infoPtr + 8, BigInt(cached.len), true);
                self._writeU32(infoPtr + 16, 100);
                self._writeU32(infoPtr + 20, 900);
                self._writeU32(infoPtr + 24, 0x0007);
                self._writeI32(infoPtr + 28, 0);
                return WAPI_OK;
            },
            supports_script(tagPtr, tagLen) {
                return 1; // Browser handles all scripts
            },
            has_feature(familyPtr, familyLen, _tag) {
                // Attempt to detect OpenType features via CSS font-feature-settings
                // is not reliable. Report unknown → 0.
                return 0;
            },
            fallback_count(familyPtr, familyLen) {
                // Kick off local font enumeration (async, idempotent).
                kickFontEnum();
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
        // Web Crypto is Promise-based, and the WAPI ABI is synchronous. We
        // resolve this with the same polling pattern used elsewhere: the
        // first call kicks off the Promise and returns AGAIN; the next call
        // (with the same output pointer) collects the result.
        //
        // Hash algorithms in the header: SHA256=0, SHA384=1, SHA512=2, SHA1=3.
        const HASH_ALGO = { 0: "SHA-256", 1: "SHA-384", 2: "SHA-512", 3: "SHA-1" };
        if (!self._cryptoStates) self._cryptoStates = new Map();
        const cryptoAsync = (key, startFn, finishFn) => {
            if (!self._cryptoStates.has(key)) {
                const entry = { state: "pending" };
                self._cryptoStates.set(key, entry);
                startFn(entry);
                return WAPI_ERR_AGAIN;
            }
            const st = self._cryptoStates.get(key);
            if (st.state === "pending") return WAPI_ERR_AGAIN;
            self._cryptoStates.delete(key);
            if (st.state !== "ready") return WAPI_ERR_IO;
            return finishFn(st);
        };
        const CIPHER_ALGO = {
            0: { name: "AES-GCM", bits: 128 }, 1: { name: "AES-GCM", bits: 256 },
            2: { name: "AES-CBC", bits: 128 }, 3: { name: "AES-CBC", bits: 256 },
            4: { name: "ChaCha20-Poly1305" },
        };
        const wapi_crypto = {
            hash(algo, dataPtr, dataLen, digestPtr, digestLenPtr) {
                if (typeof crypto === "undefined" || !crypto.subtle) return WAPI_ERR_NOTSUP;
                const algoName = HASH_ALGO[algo];
                if (!algoName) return WAPI_ERR_INVAL;
                const key = "hash:" + digestPtr;
                return cryptoAsync(key, (entry) => {
                    self._refreshViews();
                    const data = self._u8.slice(dataPtr, dataPtr + Number(dataLen));
                    crypto.subtle.digest(algoName, data).then(
                        (buf) => { entry.state = "ready"; entry.digest = new Uint8Array(buf); },
                        () => { entry.state = "failed"; }
                    );
                }, (st) => {
                    self._refreshViews();
                    const n = Math.min(st.digest.length, self._readU32(digestLenPtr));
                    self._u8.set(st.digest.subarray(0, n), digestPtr);
                    self._writeU32(digestLenPtr, st.digest.length);
                    return WAPI_OK;
                });
            },
            // Incremental hash: we buffer updates and compute at finish.
            hash_create(algo, ctxPtr) {
                const algoName = HASH_ALGO[algo];
                if (!algoName) return WAPI_ERR_INVAL;
                const h = self.handles.insert({ type: "hashctx", algo: algoName, chunks: [] });
                self._writeI32(ctxPtr, h);
                return WAPI_OK;
            },
            hash_update(ctx, dataPtr, dataLen) {
                const e = self.handles.get(ctx);
                if (!e || e.type !== "hashctx") return WAPI_ERR_BADF;
                self._refreshViews();
                e.chunks.push(self._u8.slice(dataPtr, dataPtr + Number(dataLen)));
                return WAPI_OK;
            },
            hash_finish(ctx, digestPtr, digestLenPtr) {
                const e = self.handles.get(ctx);
                if (!e || e.type !== "hashctx") return WAPI_ERR_BADF;
                const key = "hashfin:" + ctx;
                return cryptoAsync(key, (entry) => {
                    // Concatenate buffered chunks.
                    const total = e.chunks.reduce((n, c) => n + c.length, 0);
                    const all = new Uint8Array(total);
                    let o = 0;
                    for (const c of e.chunks) { all.set(c, o); o += c.length; }
                    crypto.subtle.digest(e.algo, all).then(
                        (buf) => { entry.state = "ready"; entry.digest = new Uint8Array(buf); },
                        () => { entry.state = "failed"; }
                    );
                }, (st) => {
                    self._refreshViews();
                    const n = Math.min(st.digest.length, self._readU32(digestLenPtr));
                    self._u8.set(st.digest.subarray(0, n), digestPtr);
                    self._writeU32(digestLenPtr, st.digest.length);
                    self.handles.remove(ctx);
                    return WAPI_OK;
                });
            },
            key_import_raw(dataPtr, keyLen, usages, keyPtr) {
                if (typeof crypto === "undefined" || !crypto.subtle) return WAPI_ERR_NOTSUP;
                const key = "keyimp:" + keyPtr;
                return cryptoAsync(key, (entry) => {
                    self._refreshViews();
                    const raw = self._u8.slice(dataPtr, dataPtr + Number(keyLen));
                    const uses = [];
                    if (usages & 0x1) uses.push("encrypt");
                    if (usages & 0x2) uses.push("decrypt");
                    if (usages & 0x4) uses.push("sign");
                    if (usages & 0x8) uses.push("verify");
                    crypto.subtle.importKey("raw", raw, { name: "AES-GCM" }, false, uses.length ? uses : ["encrypt","decrypt"]).then(
                        (k) => { entry.state = "ready"; entry.key = k; },
                        () => { entry.state = "failed"; }
                    );
                }, (st) => {
                    const h = self.handles.insert({ type: "cryptokey", key: st.key });
                    self._writeI32(keyPtr, h);
                    return WAPI_OK;
                });
            },
            key_generate(algo, usages, keyPtr) {
                if (typeof crypto === "undefined" || !crypto.subtle) return WAPI_ERR_NOTSUP;
                const spec = CIPHER_ALGO[algo];
                if (!spec) return WAPI_ERR_INVAL;
                const key = "keygen:" + keyPtr;
                return cryptoAsync(key, (entry) => {
                    const uses = [];
                    if (usages & 0x1) uses.push("encrypt");
                    if (usages & 0x2) uses.push("decrypt");
                    const algoDesc = spec.bits
                        ? { name: spec.name, length: spec.bits }
                        : { name: spec.name };
                    crypto.subtle.generateKey(algoDesc, true, uses.length ? uses : ["encrypt","decrypt"]).then(
                        (k) => { entry.state = "ready"; entry.key = k; },
                        () => { entry.state = "failed"; }
                    );
                }, (st) => {
                    const h = self.handles.insert({ type: "cryptokey", key: st.key });
                    self._writeI32(keyPtr, h);
                    return WAPI_OK;
                });
            },
            key_generate_pair(_algo, _usages, _pubPtr, _privPtr) { return WAPI_ERR_NOTSUP; },
            key_release(key) {
                self.handles.remove(key);
                return WAPI_OK;
            },
            encrypt(algo, key, ivPtr, ivLen, ptPtr, ptLen, ctPtr, ctLenPtr) {
                if (typeof crypto === "undefined" || !crypto.subtle) return WAPI_ERR_NOTSUP;
                const spec = CIPHER_ALGO[algo];
                const k = self.handles.get(key);
                if (!spec || !k || k.type !== "cryptokey") return WAPI_ERR_INVAL;
                const cacheKey = "enc:" + ctPtr;
                return cryptoAsync(cacheKey, (entry) => {
                    self._refreshViews();
                    const iv = self._u8.slice(ivPtr, ivPtr + Number(ivLen));
                    const pt = self._u8.slice(ptPtr, ptPtr + Number(ptLen));
                    crypto.subtle.encrypt({ name: spec.name, iv }, k.key, pt).then(
                        (buf) => { entry.state = "ready"; entry.ct = new Uint8Array(buf); },
                        () => { entry.state = "failed"; }
                    );
                }, (st) => {
                    self._refreshViews();
                    const cap = self._readU32(ctLenPtr);
                    const n = Math.min(st.ct.length, cap);
                    self._u8.set(st.ct.subarray(0, n), ctPtr);
                    self._writeU32(ctLenPtr, st.ct.length);
                    return st.ct.length > cap ? WAPI_ERR_OVERFLOW : WAPI_OK;
                });
            },
            decrypt(algo, key, ivPtr, ivLen, ctPtr, ctLen, ptPtr, ptLenPtr) {
                if (typeof crypto === "undefined" || !crypto.subtle) return WAPI_ERR_NOTSUP;
                const spec = CIPHER_ALGO[algo];
                const k = self.handles.get(key);
                if (!spec || !k || k.type !== "cryptokey") return WAPI_ERR_INVAL;
                const cacheKey = "dec:" + ptPtr;
                return cryptoAsync(cacheKey, (entry) => {
                    self._refreshViews();
                    const iv = self._u8.slice(ivPtr, ivPtr + Number(ivLen));
                    const ct = self._u8.slice(ctPtr, ctPtr + Number(ctLen));
                    crypto.subtle.decrypt({ name: spec.name, iv }, k.key, ct).then(
                        (buf) => { entry.state = "ready"; entry.pt = new Uint8Array(buf); },
                        () => { entry.state = "failed"; }
                    );
                }, (st) => {
                    self._refreshViews();
                    const cap = self._readU32(ptLenPtr);
                    const n = Math.min(st.pt.length, cap);
                    self._u8.set(st.pt.subarray(0, n), ptPtr);
                    self._writeU32(ptLenPtr, st.pt.length);
                    return st.pt.length > cap ? WAPI_ERR_OVERFLOW : WAPI_OK;
                });
            },
            sign(_algo, _key, _dataPtr, _dataLen, _sigPtr, _sigLenPtr) { return WAPI_ERR_NOTSUP; },
            verify(_algo, _key, _dataPtr, _dataLen, _sigPtr, _sigLen) { return WAPI_ERR_NOTSUP; },
            derive_key(_algo, _baseKey, _saltPtr, _saltLen, _infoPtr, _infoLen, _iterations, _keyLen, _derivedPtr, _derivedLenPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_video (video/media playback - stub)
        // -------------------------------------------------------------------
        // HTMLVideoElement-backed playback.
        // wapi_video_desc_t: source_kind u32, _pad, url stringview(16), codec u32, _pad, data_ptr i64, data_len i32
        const wapi_video = {
            create(descPtr, videoPtr) {
                if (typeof document === "undefined") return WAPI_ERR_NOTSUP;
                self._refreshViews();
                const source = self._dv.getUint32(descPtr + 0, true); // 0 URL, 1 MEMORY
                const url    = self._readStringView(descPtr + 8);
                const v = document.createElement("video");
                v.playsInline = true;
                v.crossOrigin = "anonymous";
                const entry = {
                    type: "video", video: v,
                    state: 1 /* LOADING */,
                };
                v.addEventListener("loadedmetadata", () => { entry.state = 2; });
                v.addEventListener("playing", () => { entry.state = 3; });
                v.addEventListener("pause", () => { entry.state = 4; });
                v.addEventListener("ended", () => { entry.state = 5; });
                v.addEventListener("error", () => { entry.state = 6; });
                if (source === 0 && url) v.src = url;
                else if (source === 1) {
                    const dptr = Number(self._dv.getBigUint64(descPtr + 32, true));
                    const dlen = self._dv.getUint32(descPtr + 40, true);
                    const blob = new Blob([self._u8.slice(dptr, dptr + dlen)], { type: "video/mp4" });
                    v.src = URL.createObjectURL(blob);
                }
                const h = self.handles.insert(entry);
                self._writeI32(videoPtr, h);
                return WAPI_OK;
            },
            destroy(video) {
                const e = self.handles.get(video);
                if (!e || e.type !== "video") return WAPI_ERR_BADF;
                try { e.video.pause(); e.video.removeAttribute("src"); e.video.load(); } catch (_) {}
                self.handles.remove(video);
                return WAPI_OK;
            },
            // wapi_video_info_t = 20 bytes: width u32, height u32, duration f32, codec u32, has_audio u32
            get_info(video, infoPtr) {
                const e = self.handles.get(video);
                if (!e || e.type !== "video") return WAPI_ERR_BADF;
                const v = e.video;
                self._refreshViews();
                self._dv.setUint32 (infoPtr + 0,  v.videoWidth || 0, true);
                self._dv.setUint32 (infoPtr + 4,  v.videoHeight || 0, true);
                self._dv.setFloat32(infoPtr + 8,  isFinite(v.duration) ? v.duration : 0, true);
                self._dv.setUint32 (infoPtr + 12, 0, true); // codec unknown
                self._dv.setUint32 (infoPtr + 16, !!v.audioTracks?.length | 1, true);
                return WAPI_OK;
            },
            play(video) {
                const e = self.handles.get(video);
                if (!e || e.type !== "video") return WAPI_ERR_BADF;
                e.video.play().catch(() => {});
                return WAPI_OK;
            },
            pause(video) {
                const e = self.handles.get(video);
                if (!e || e.type !== "video") return WAPI_ERR_BADF;
                e.video.pause();
                return WAPI_OK;
            },
            seek(video, time) {
                const e = self.handles.get(video);
                if (!e || e.type !== "video") return WAPI_ERR_BADF;
                e.video.currentTime = time;
                return WAPI_OK;
            },
            get_state(video, statePtr) {
                const e = self.handles.get(video);
                if (!e || e.type !== "video") return WAPI_ERR_BADF;
                self._writeU32(statePtr, e.state);
                return WAPI_OK;
            },
            get_position(video, posPtr) {
                const e = self.handles.get(video);
                if (!e || e.type !== "video") return WAPI_ERR_BADF;
                self._writeF32(posPtr, e.video.currentTime);
                return WAPI_OK;
            },
            // Uploading video frames to an existing GPU texture requires
            // coordinating with the WebGPU path — defer.
            get_frame_texture(_video, _texPtr) { return WAPI_ERR_NOTSUP; },
            blit(_video, _tex, _x, _y, _w, _h) { return WAPI_ERR_NOTSUP; },
            bind_audio(_video, _stream) { return WAPI_ERR_NOTSUP; },
            set_volume(video, vol) {
                const e = self.handles.get(video);
                if (!e || e.type !== "video") return WAPI_ERR_BADF;
                e.video.volume = Math.max(0, Math.min(1, vol));
                return WAPI_OK;
            },
            set_muted(video, muted) {
                const e = self.handles.get(video);
                if (!e || e.type !== "video") return WAPI_ERR_BADF;
                e.video.muted = !!muted;
                return WAPI_OK;
            },
            set_loop(video, loop) {
                const e = self.handles.get(video);
                if (!e || e.type !== "video") return WAPI_ERR_BADF;
                e.video.loop = !!loop;
                return WAPI_OK;
            },
            set_playback_rate(video, rate) {
                const e = self.handles.get(video);
                if (!e || e.type !== "video") return WAPI_ERR_BADF;
                e.video.playbackRate = rate > 0 ? rate : 1.0;
                return WAPI_OK;
            },
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
                    const handle = self._instantiateChildModule(entry.module, hashHex);
                    self._writeI32(modulePtr, handle);
                    return WAPI_OK;
                } catch (e) {
                    console.error("[WAPI] wapi_module.load instantiate failed:", e);
                    return WAPI_ERR_IO;
                }
            },
            // (i32 mod, i32 name_sv_ptr, i32 func_ptr_out) -> i32
            // The name stringview is in the CALLER'S memory (the parent),
            // so using self._readStringView is correct here.
            get_func(mod, nameSvPtr, funcOutPtr) {
                const entry = self.handles.get(mod);
                if (!entry) return WAPI_ERR_BADF;
                const inst = entry.instance;
                if (!inst) return WAPI_ERR_BADF;
                self._refreshViews();
                const name = self._readStringView(nameSvPtr);
                const fn = inst.exports[name];
                if (typeof fn !== "function") return WAPI_ERR_NOENT;
                const h = self.handles.insert({
                    type: "module_func",
                    module: mod,
                    name,
                    fn,
                });
                self._writeI32(funcOutPtr, h);
                return WAPI_OK;
            },
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
                self.handles.remove(mod);
                if (entry.type === "module" && entry.serviceKey) {
                    const svc = _wapiPageServices.get(entry.serviceKey);
                    if (svc && --svc.refcount <= 0) {
                        _wapiPageServices.delete(entry.serviceKey);
                        _wapiExtPost("services.withdraw", {
                            hashHex: svc.hashHex, name: svc.name,
                        });
                        console.log(`[WAPI] service released ${svc.hashHex.slice(0,10)}… (${svc.name || "default"})`);
                    }
                }
                return WAPI_OK;
            },

            // (i32 hash_ptr, i32 url_sv_ptr, i32 name_sv_ptr, i32 module_out_ptr) -> i32
            //
            // Service mode per spec §10:
            //   - The application owns memory 1; the live child instance
            //     must live in the same address space so shared_read/write
            //     and call remain synchronous. That's the page.
            //   - Intra-application sharing: if a service with the same
            //     (hash, name) already exists in this page, attach to it
            //     (refcount++) and return a fresh handle. The underlying
            //     wasm Instance is NOT duplicated.
            //   - The extension SW hosts the content-addressed bytes cache
            //     (cross-origin), not the instance. The page announces the
            //     service to the SW for popup observability.
            join(hashPtr, urlSvPtr, nameSvPtr, modulePtr) {
                self._refreshViews();
                const hashHex = _wapiReadModuleHash(self._u8, hashPtr);
                if (!hashHex) return WAPI_ERR_INVAL;
                const url  = urlSvPtr  ? self._readStringView(urlSvPtr)  : "";
                const name = nameSvPtr ? self._readStringView(nameSvPtr) : "";
                const svcKey = hashHex + ":" + name;

                // 1. Intra-application attach: service already instantiated
                // in this page.
                const existing = _wapiPageServices.get(svcKey);
                if (existing && existing.state === "ready") {
                    existing.refcount++;
                    const h = self.handles.insert({
                        type: "module",
                        instance: existing.instance,
                        module: existing.module,
                        hash: hashHex,
                        name,
                        serviceKey: svcKey,
                    });
                    self._writeI32(modulePtr, h);
                    _wapiExtPost("services.announce", {
                        hashHex, name, url: existing.url,
                        origin: (typeof window !== "undefined" && window.location)
                                ? window.location.origin : "",
                        refcount: existing.refcount,
                    });
                    return WAPI_OK;
                }
                if (existing && existing.state === "pending") return WAPI_ERR_AGAIN;
                if (existing && existing.state === "failed") {
                    _wapiPageServices.delete(svcKey);
                    return WAPI_ERR_IO;
                }

                // 2. Need to instantiate. Do we have bytes?
                const cached = _wapiModuleCache.get(hashHex);
                if (cached && cached.state === "ready") {
                    try {
                        const handle = self._instantiateChildModule(cached.module, hashHex, name);
                        const entry  = self.handles.get(handle);
                        const svc = {
                            state:     "ready",
                            module:    entry.module,
                            instance:  entry.instance,
                            refcount:  1,
                            hashHex,  name,  url,
                        };
                        _wapiPageServices.set(svcKey, svc);
                        // Mark the handle as belonging to this service so
                        // release decrements the right refcount.
                        entry.serviceKey = svcKey;
                        self._writeI32(modulePtr, handle);
                        _wapiExtPost("services.announce", {
                            hashHex, name, url,
                            origin: (typeof window !== "undefined" && window.location)
                                    ? window.location.origin : "",
                            refcount: 1,
                        });
                        _wapiStartHeartbeat();
                        console.log(`[WAPI] service started ${hashHex.slice(0,10)}… (${name || "default"})`);
                        return WAPI_OK;
                    } catch (e) {
                        console.error("[WAPI] wapi_module.join instantiate failed:", e);
                        return WAPI_ERR_IO;
                    }
                }
                if (cached && cached.state === "pending") {
                    _wapiPageServices.set(svcKey, { state: "pending", hashHex, name, url });
                    return WAPI_ERR_AGAIN;
                }

                // 3. No bytes yet — fetch. Require URL.
                if (!url) return WAPI_ERR_NOENT;
                console.log(`[WAPI] fetching module ${url} (hash ${hashHex.slice(0,10)}…)`);
                _wapiModuleCache.set(hashHex, { state: "pending" });
                _wapiPageServices.set(svcKey, { state: "pending", hashHex, name, url });
                _wapiFetchAndCompile(hashHex, url).then(
                    (mod) => {
                        console.log(`[WAPI] module compiled ${hashHex.slice(0,10)}…`);
                        _wapiModuleCache.set(hashHex, {
                            state: "ready", module: mod, hash: hashHex,
                        });
                        // Clear the stale pending service entry so the next
                        // join iteration falls through to instantiation.
                        const pending = _wapiPageServices.get(svcKey);
                        if (pending && pending.state === "pending") {
                            _wapiPageServices.delete(svcKey);
                        }
                    },
                    (err) => {
                        console.error("[WAPI] module fetch failed:", err);
                        _wapiModuleCache.set(hashHex, { state: "failed", error: String(err) });
                        const pending = _wapiPageServices.get(svcKey);
                        if (pending && pending.state === "pending") {
                            _wapiPageServices.set(svcKey, { state: "failed", hashHex, name });
                        }
                    },
                );
                return WAPI_ERR_AGAIN;
            },
            // (i32 mod, i32 func, i32 args_ptr, i64 nargs,
            //  i32 results_ptr, i64 nresults) -> i32
            // args/results are arrays of wapi_val_t (16 bytes each) in the
            // CALLER's private memory.
            call(mod, func, argsPtr, nargs, resultsPtr, nresults) {
                const fnEntry = self.handles.get(func);
                if (!fnEntry || fnEntry.type !== "module_func") {
                    console.error("[WAPI] wapi_module.call: bad func handle", func);
                    return WAPI_ERR_BADF;
                }
                self._refreshViews();

                const nArgs = Number(nargs);
                const jsArgs = new Array(nArgs);
                for (let i = 0; i < nArgs; i++) {
                    const p = Number(argsPtr) + i * 16;
                    const kind = self._u8[p];
                    switch (kind) {
                        case 0: jsArgs[i] = self._dv.getInt32(p + 8, true); break;
                        case 1: jsArgs[i] = self._dv.getBigInt64(p + 8, true); break;
                        case 2: jsArgs[i] = self._dv.getFloat32(p + 8, true); break;
                        case 3: jsArgs[i] = self._dv.getFloat64(p + 8, true); break;
                        default:
                            console.error("[WAPI] wapi_module.call: bad arg kind", kind, "at", p);
                            return WAPI_ERR_INVAL;
                    }
                }

                let result;
                try {
                    console.log(`[WAPI] call ${fnEntry.name}(`, ...jsArgs, `)`);
                    result = fnEntry.fn(...jsArgs);
                    console.log(`[WAPI] call ${fnEntry.name} -> ${result}`);
                } catch (e) {
                    console.error(`[WAPI] call ${fnEntry.name} trapped:`, e);
                    return WAPI_ERR_UNKNOWN;
                }

                const nRes = Number(nresults);
                if (nRes > 0 && resultsPtr) {
                    // Single-value writeback; the common case. Multi-return
                    // is not exercised by current modules.
                    const p = Number(resultsPtr);
                    self._refreshViews();
                    let kind = 0;
                    if (typeof result === "bigint") kind = 1;
                    else if (typeof result === "number" && !Number.isInteger(result)) kind = 3;
                    self._u8[p] = kind;
                    for (let k = 1; k < 8; k++) self._u8[p + k] = 0;
                    switch (kind) {
                        case 0: self._dv.setInt32    (p + 8, (result | 0),        true); break;
                        case 1: self._dv.setBigInt64 (p + 8, BigInt(result),      true); break;
                        case 2: self._dv.setFloat32  (p + 8, result,              true); break;
                        case 3: self._dv.setFloat64  (p + 8, result,              true); break;
                    }
                }
                return WAPI_OK;
            },
            // Shared memory — all callers (parent and children) route to
            // the same arena on the shim. Per-caller memory accesses are
            // bound in _instantiateChildModule for child instances.
            // wapi_size_t is uint64_t in wasm, so offset/size returns must
            // be BigInt or the engine traps with "Cannot convert … to BigInt".
            shared_alloc(size, align) {
                return BigInt(self._sharedAlloc(Number(size), Number(align) || 1));
            },
            shared_free(offset) {
                return self._sharedFree(Number(offset));
            },
            shared_realloc(offset, newSize, align) {
                const sz = Number(newSize);
                const al = Number(align) || 1;
                const off = Number(offset);
                if (!off) return BigInt(self._sharedAlloc(sz, al));
                if (sz === 0) { self._sharedFree(off); return 0n; }
                const old = self._sharedAllocs.get(off);
                if (!old) return 0n;
                if (sz <= old) return BigInt(off);
                const next = self._sharedAlloc(sz, al);
                if (!next) return 0n;
                self._sharedMem.copyWithin(next, off, off + old);
                self._sharedFree(off);
                return BigInt(next);
            },
            shared_usable_size(offset) {
                return BigInt(self._sharedAllocs.get(Number(offset)) || 0);
            },
            // Parent-side shared memory copy. Child instances override these
            // with closures bound to their own memory in _instantiateChildModule.
            shared_read(srcOffset, dstPtr, len) {
                self._refreshViews();
                const s = Number(srcOffset), d = Number(dstPtr), n = Number(len);
                if (s + n > self._sharedMem.length) return WAPI_ERR_RANGE || -2;
                self._u8.set(self._sharedMem.subarray(s, s + n), d);
                return WAPI_OK;
            },
            shared_write(dstOffset, srcPtr, len) {
                self._refreshViews();
                const d = Number(dstOffset), s = Number(srcPtr), n = Number(len);
                if (d + n > self._sharedMem.length) return WAPI_ERR_RANGE || -2;
                self._sharedMem.set(self._u8.subarray(s, s + n), d);
                return WAPI_OK;
            },
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
        //
        // Grant acquisition flows through the universal WAPI_IO_OP_CAP_REQUEST
        // path in the opcode dispatcher above — no per-module perm imports.
        // -------------------------------------------------------------------
        const wapi_notify = {
            // desc layout (56 bytes): title(16) body(16) icon(16) urgency(4) _reserved(4)
            show(descPtr, idPtr) {
                if (typeof Notification === "undefined") return WAPI_ERR_NOTSUP;
                if (Notification.permission !== "granted") return WAPI_ERR_ACCES;
                self._refreshViews();
                const title = self._readStringView(descPtr + 0)  || "";
                const body  = self._readStringView(descPtr + 16) || "";
                const icon  = self._readStringView(descPtr + 32);
                try {
                    const n = new Notification(title, {
                        body,
                        icon: icon || undefined,
                    });
                    const h = self.handles.insert({ type: "notification", obj: n });
                    self._writeI32(idPtr, h);
                    return WAPI_OK;
                } catch (e) {
                    return WAPI_ERR_IO;
                }
            },
            close(id) {
                const e = self.handles.get(id);
                if (!e || e.type !== "notification") return WAPI_ERR_BADF;
                try { e.obj.close(); } catch (_) {}
                self.handles.remove(id);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_geo (geolocation - stub, browser API is async)
        // -------------------------------------------------------------------
        // NOTE: The browser Geolocation API (navigator.geolocation) is
        // async-only (callback-based). Synchronous position queries are not
        // possible on the main thread, so get_position returns NOTSUP.
        // A future async WAPI extension could enable these.
        // Async-to-sync polling state for one-shot get_position.
        // state: "pending" | "ready" | "failed"
        if (!self._geoOneShot) self._geoOneShot = null;
        // watch_position: map handle -> { watchId, lastPos }
        if (!self._geoWatches) self._geoWatches = new Map();

        const writeGeoPos = (ptr, coords) => {
            self._refreshViews();
            const dv = self._dv;
            dv.setFloat64(ptr +  0, coords.latitude,  true);
            dv.setFloat64(ptr +  8, coords.longitude, true);
            dv.setFloat64(ptr + 16, coords.altitude === null || coords.altitude === undefined ? NaN : coords.altitude, true);
            dv.setFloat64(ptr + 24, coords.accuracy,  true);
            dv.setFloat64(ptr + 32, coords.altitudeAccuracy === null || coords.altitudeAccuracy === undefined ? NaN : coords.altitudeAccuracy, true);
            dv.setFloat64(ptr + 40, coords.heading === null || coords.heading === undefined ? NaN : coords.heading, true);
        };

        const wapi_geo = {
            get_position(flags, timeout_ms, positionPtr) {
                if (typeof navigator === "undefined" || !navigator.geolocation) return WAPI_ERR_NOTSUP;
                const st = self._geoOneShot;
                if (st && st.state === "ready") {
                    writeGeoPos(positionPtr, st.coords);
                    self._geoOneShot = null;
                    return WAPI_OK;
                }
                if (st && st.state === "failed") {
                    const err = st.error;
                    self._geoOneShot = null;
                    if (err === 1) return WAPI_ERR_ACCES;
                    if (err === 3) return WAPI_ERR_TIMEDOUT;
                    return WAPI_ERR_IO;
                }
                if (!st) {
                    self._geoOneShot = { state: "pending" };
                    navigator.geolocation.getCurrentPosition(
                        (pos) => { self._geoOneShot = { state: "ready", coords: pos.coords }; },
                        (err) => { self._geoOneShot = { state: "failed", error: err.code }; },
                        {
                            enableHighAccuracy: (flags & 0x0001) !== 0,
                            timeout: timeout_ms > 0 ? timeout_ms : undefined,
                        }
                    );
                }
                return WAPI_ERR_AGAIN;
            },
            watch_position(flags, watchPtr) {
                if (typeof navigator === "undefined" || !navigator.geolocation) return WAPI_ERR_NOTSUP;
                const h = self.handles.insert({ type: "geo_watch" });
                const entry = self.handles.get(h);
                entry.watchId = navigator.geolocation.watchPosition(
                    (pos) => {
                        entry.lastPos = pos.coords;
                        // Deliver as IO completion event.
                        self._eventQueue.push({
                            type: WAPI_EVENT_IO_COMPLETION,
                            userData: BigInt(h),
                            result: 0,
                            flags: 0,
                        });
                    },
                    (_err) => {
                        self._eventQueue.push({
                            type: WAPI_EVENT_IO_COMPLETION,
                            userData: BigInt(h),
                            result: WAPI_ERR_IO,
                            flags: 0,
                        });
                    },
                    { enableHighAccuracy: (flags & 0x0001) !== 0 }
                );
                self._geoWatches.set(h, entry);
                self._writeI32(watchPtr, h);
                return WAPI_OK;
            },
            clear_watch(watch) {
                const e = self.handles.get(watch);
                if (!e || e.type !== "geo_watch") return WAPI_ERR_BADF;
                try { navigator.geolocation.clearWatch(e.watchId); } catch (_) {}
                self._geoWatches.delete(watch);
                self.handles.remove(watch);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_sensor (sensors - stub)
        // -------------------------------------------------------------------
        // Generic Sensor API availability check
        const sensorCtors = {
            0: typeof Accelerometer !== "undefined" ? Accelerometer : null,      // ACCEL
            1: typeof Gyroscope !== "undefined"    ? Gyroscope    : null,        // GYRO
            2: typeof Magnetometer !== "undefined" ? Magnetometer : null,        // MAG
            3: typeof AmbientLightSensor !== "undefined" ? AmbientLightSensor : null, // LIGHT
            5: typeof GravitySensor !== "undefined" ? GravitySensor : null,      // GRAVITY
            6: typeof LinearAccelerationSensor !== "undefined" ? LinearAccelerationSensor : null, // LINEAR
        };
        const wapi_sensor = {
            available(type) {
                return sensorCtors[type] ? 1 : 0;
            },
            start(type, freqHz, sensorPtr) {
                const Ctor = sensorCtors[type];
                if (!Ctor) return WAPI_ERR_NOTSUP;
                try {
                    const sensor = new Ctor({ frequency: freqHz > 0 ? freqHz : 60 });
                    const entry = { type: "sensor", sensor, kind: type, xyz: null, scalar: null, err: 0 };
                    sensor.onreading = () => {
                        if (type === 3 || type === 4) {
                            entry.scalar = { value: sensor.illuminance ?? sensor.distance ?? 0, ts: BigInt(Math.round(performance.now() * 1e6)) };
                        } else {
                            entry.xyz = { x: sensor.x || 0, y: sensor.y || 0, z: sensor.z || 0, ts: BigInt(Math.round(performance.now() * 1e6)) };
                        }
                    };
                    sensor.onerror = (e) => { entry.err = WAPI_ERR_IO; };
                    sensor.start();
                    const h = self.handles.insert(entry);
                    self._writeI32(sensorPtr, h);
                    return WAPI_OK;
                } catch (e) {
                    return WAPI_ERR_ACCES;
                }
            },
            stop(sensor) {
                const e = self.handles.get(sensor);
                if (!e || e.type !== "sensor") return WAPI_ERR_BADF;
                try { e.sensor.stop(); } catch (_) {}
                self.handles.remove(sensor);
                return WAPI_OK;
            },
            read_xyz(sensor, readingPtr) {
                const e = self.handles.get(sensor);
                if (!e || e.type !== "sensor") return WAPI_ERR_BADF;
                if (e.err) return e.err;
                if (!e.xyz) return WAPI_ERR_AGAIN;
                self._refreshViews();
                self._dv.setFloat64(readingPtr +  0, e.xyz.x, true);
                self._dv.setFloat64(readingPtr +  8, e.xyz.y, true);
                self._dv.setFloat64(readingPtr + 16, e.xyz.z, true);
                self._dv.setBigUint64(readingPtr + 24, e.xyz.ts, true);
                return WAPI_OK;
            },
            read_scalar(sensor, readingPtr) {
                const e = self.handles.get(sensor);
                if (!e || e.type !== "sensor") return WAPI_ERR_BADF;
                if (e.err) return e.err;
                if (!e.scalar) return WAPI_ERR_AGAIN;
                self._refreshViews();
                self._dv.setFloat64(readingPtr + 0, e.scalar.value, true);
                self._dv.setBigUint64(readingPtr + 8, e.scalar.ts, true);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_speech (speech synthesis/recognition - stub)
        // -------------------------------------------------------------------
        const wapi_speech = {
            // utterance layout (48 bytes): text(16) lang(16) rate(4) pitch(4) volume(4) _pad(4)
            speak(utterancePtr, idPtr) {
                if (typeof speechSynthesis === "undefined") return WAPI_ERR_NOTSUP;
                self._refreshViews();
                const text = self._readStringView(utterancePtr + 0) || "";
                const lang = self._readStringView(utterancePtr + 16);
                const rate   = self._dv.getFloat32(utterancePtr + 32, true);
                const pitch  = self._dv.getFloat32(utterancePtr + 36, true);
                const volume = self._dv.getFloat32(utterancePtr + 40, true);
                const u = new SpeechSynthesisUtterance(text);
                if (lang) u.lang = lang;
                if (rate > 0)   u.rate = rate;
                if (pitch >= 0) u.pitch = pitch;
                if (volume >= 0) u.volume = volume;
                try {
                    speechSynthesis.speak(u);
                    const h = self.handles.insert({ type: "tts", utterance: u });
                    self._writeI32(idPtr, h);
                    return WAPI_OK;
                } catch (e) { return WAPI_ERR_IO; }
            },
            cancel(id) {
                const e = self.handles.get(id);
                if (!e || e.type !== "tts") return WAPI_ERR_BADF;
                // No per-utterance cancel; cancelling all is the closest match.
                try { speechSynthesis.cancel(); } catch (_) {}
                self.handles.remove(id);
                return WAPI_OK;
            },
            cancel_all() {
                if (typeof speechSynthesis === "undefined") return WAPI_ERR_NOTSUP;
                try { speechSynthesis.cancel(); return WAPI_OK; } catch (_) { return WAPI_ERR_IO; }
            },
            is_speaking() {
                return (typeof speechSynthesis !== "undefined" && speechSynthesis.speaking) ? 1 : 0;
            },
            // lang arrives as a wapi_stringview_t pointer (single i32)
            recognize_start(langSvPtr, continuous, sessionPtr) {
                const SR = typeof webkitSpeechRecognition !== "undefined" ? webkitSpeechRecognition
                         : (typeof SpeechRecognition !== "undefined" ? SpeechRecognition : null);
                if (!SR) return WAPI_ERR_NOTSUP;
                const lang = langSvPtr ? self._readStringView(langSvPtr) : null;
                const r = new SR();
                if (lang) r.lang = lang;
                r.continuous = !!continuous;
                r.interimResults = false;
                const entry = { type: "sr", recog: r, pending: [], ended: false };
                r.onresult = (ev) => {
                    for (let i = ev.resultIndex; i < ev.results.length; i++) {
                        const res = ev.results[i];
                        if (res.isFinal) entry.pending.push({ text: res[0].transcript, confidence: res[0].confidence || 0 });
                    }
                };
                r.onerror = () => { entry.ended = true; };
                r.onend = () => { entry.ended = true; };
                try { r.start(); } catch (e) { return WAPI_ERR_BUSY; }
                const h = self.handles.insert(entry);
                self._writeI32(sessionPtr, h);
                return WAPI_OK;
            },
            recognize_stop(session) {
                const e = self.handles.get(session);
                if (!e || e.type !== "sr") return WAPI_ERR_BADF;
                try { e.recog.stop(); } catch (_) {}
                self.handles.remove(session);
                return WAPI_OK;
            },
            recognize_result(session, bufPtr, bufLen, textLenPtr, confidencePtr) {
                const e = self.handles.get(session);
                if (!e || e.type !== "sr") return WAPI_ERR_BADF;
                if (e.pending.length === 0) {
                    if (e.ended) return WAPI_ERR_IO;
                    return WAPI_ERR_AGAIN;
                }
                const item = e.pending.shift();
                const written = self._writeString(bufPtr, Number(bufLen), item.text);
                self._writeU64(textLenPtr, written);
                self._writeF32(confidencePtr, item.confidence);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_bio (biometric authentication - stub)
        // -------------------------------------------------------------------
        // Biometric: no direct Web API. Best we can do is route through
        // WebAuthn with userVerification="required"/platform authenticator,
        // which returns true if the user completed fingerprint/face auth.
        // Async → polling state.
        const wapi_bio = {
            available_types() {
                // WebAuthn platform authenticator availability is async. Best-effort:
                // assume ANY (0xFF) if PublicKeyCredential is present.
                return (typeof PublicKeyCredential !== "undefined") ? 0xFF : 0;
            },
            authenticate(type, reasonSvPtr) {
                if (typeof PublicKeyCredential === "undefined" || !navigator.credentials)
                    return WAPI_ERR_NOTSUP;
                if (!self._bioState) {
                    self._bioState = { state: "pending" };
                    const challenge = new Uint8Array(32);
                    crypto.getRandomValues(challenge);
                    navigator.credentials.get({
                        publicKey: {
                            challenge,
                            userVerification: "required",
                            timeout: 60000,
                        }
                    }).then(
                        () => { self._bioState = { state: "ready", ok: true }; },
                        () => { self._bioState = { state: "failed" }; }
                    );
                    return WAPI_ERR_AGAIN;
                }
                if (self._bioState.state === "pending") return WAPI_ERR_AGAIN;
                const r = self._bioState.state === "ready" ? WAPI_OK : WAPI_ERR_ACCES;
                self._bioState = null;
                return r;
            },
            can_authenticate() {
                return (typeof PublicKeyCredential !== "undefined") ? 1 : 0;
            },
        };

        // -------------------------------------------------------------------
        // wapi_pay (payments - stub)
        // -------------------------------------------------------------------
        // Payment Request API. The C surface only needs a token string out.
        // Our implementation collects the details pointer + count and runs
        // a standard Google Pay / basic-card request. Async → polling.
        const wapi_pay = {
            can_make_payment() {
                return (typeof PaymentRequest !== "undefined") ? 1 : 0;
            },
            request_payment(requestPtr, tokenPtr, tokenLenPtr) {
                if (typeof PaymentRequest === "undefined") return WAPI_ERR_NOTSUP;
                if (self._payState && self._payState.state !== "pending") {
                    const st = self._payState;
                    self._payState = null;
                    if (st.state === "ready") {
                        const written = self._writeString(tokenPtr, 4096, st.token);
                        self._writeU64(tokenLenPtr, written);
                        return WAPI_OK;
                    }
                    if (st.state === "canceled") return WAPI_ERR_CANCELED;
                    return WAPI_ERR_IO;
                }
                if (!self._payState) {
                    // Minimal: treat request as a single stringview "total_label"
                    // plus amount in USD. The full struct semantics aren't
                    // pinned yet — ship a safe default.
                    const methods = [{ supportedMethods: "basic-card" }];
                    const details = {
                        total: { label: "Total", amount: { currency: "USD", value: "0.00" } },
                    };
                    self._payState = { state: "pending" };
                    try {
                        const pr = new PaymentRequest(methods, details);
                        pr.show().then(
                            async (response) => {
                                const token = JSON.stringify(response.details || {});
                                await response.complete("success").catch(() => {});
                                self._payState = { state: "ready", token };
                            },
                            (err) => {
                                if (err && err.name === "AbortError")
                                    self._payState = { state: "canceled" };
                                else
                                    self._payState = { state: "failed" };
                            }
                        );
                    } catch (e) {
                        self._payState = null;
                        return WAPI_ERR_IO;
                    }
                }
                return WAPI_ERR_AGAIN;
            },
        };

        // -------------------------------------------------------------------
        // wapi_usb (USB - stub)
        // -------------------------------------------------------------------
        // WebUSB. Async throughout — we expose polling state via a map keyed
        // by caller-visible ptrs/handles.
        const asyncOp = (map, key, startFn, onReady) => {
            if (!map.has(key)) {
                const entry = { state: "pending" };
                map.set(key, entry);
                startFn(entry);
                return WAPI_ERR_AGAIN;
            }
            const st = map.get(key);
            if (st.state === "pending") return WAPI_ERR_AGAIN;
            map.delete(key);
            if (st.state === "ready") return onReady(st);
            return WAPI_ERR_IO;
        };
        const wapi_usb = {
            request_device(filtersPtr, filterCount, devicePtr) {
                if (typeof navigator === "undefined" || !navigator.usb) return WAPI_ERR_NOTSUP;
                if (!self._usbStates) self._usbStates = new Map();
                return asyncOp(self._usbStates, "req:" + devicePtr, (entry) => {
                    // wapi_usb_filter_t: 8 bytes. vendor_id u16, product_id u16, class u8, subclass u8, protocol u8, _pad.
                    self._refreshViews();
                    const filters = [];
                    for (let i = 0; i < filterCount; i++) {
                        const base = filtersPtr + i * 8;
                        const vid = self._dv.getUint16(base + 0, true);
                        const pid = self._dv.getUint16(base + 2, true);
                        const cls = self._u8[base + 4];
                        const f = {};
                        if (vid) f.vendorId = vid;
                        if (pid) f.productId = pid;
                        if (cls) f.classCode = cls;
                        filters.push(f);
                    }
                    navigator.usb.requestDevice({ filters: filters.length ? filters : [{}] }).then(
                        (d) => { entry.state = "ready"; entry.device = d; },
                        (err) => { entry.state = err && err.name === "NotFoundError" ? "canceled" : "failed"; }
                    );
                }, (st) => {
                    const h = self.handles.insert({ type: "usb", device: st.device });
                    self._writeI32(devicePtr, h);
                    return WAPI_OK;
                });
            },
            open(device) {
                const e = self.handles.get(device);
                if (!e || e.type !== "usb") return WAPI_ERR_BADF;
                if (e.opened) return WAPI_OK;
                if (!self._usbStates) self._usbStates = new Map();
                return asyncOp(self._usbStates, "open:" + device, (entry) => {
                    e.device.open().then(
                        () => { entry.state = "ready"; e.opened = true; },
                        () => { entry.state = "failed"; }
                    );
                }, () => WAPI_OK);
            },
            close(device) {
                const e = self.handles.get(device);
                if (!e || e.type !== "usb") return WAPI_ERR_BADF;
                try { e.device.close(); } catch (_) {}
                e.opened = false;
                return WAPI_OK;
            },
            claim_interface(device, interfaceNum) {
                const e = self.handles.get(device);
                if (!e || e.type !== "usb") return WAPI_ERR_BADF;
                if (!self._usbStates) self._usbStates = new Map();
                return asyncOp(self._usbStates, "claim:" + device + ":" + interfaceNum, (entry) => {
                    e.device.claimInterface(interfaceNum).then(
                        () => { entry.state = "ready"; },
                        () => { entry.state = "failed"; }
                    );
                }, () => WAPI_OK);
            },
            release_interface(device, interfaceNum) {
                const e = self.handles.get(device);
                if (!e || e.type !== "usb") return WAPI_ERR_BADF;
                e.device.releaseInterface(interfaceNum).catch(() => {});
                return WAPI_OK;
            },
            transfer_in(device, endpoint, bufPtr, len, transferredPtr) {
                const e = self.handles.get(device);
                if (!e || e.type !== "usb") return WAPI_ERR_BADF;
                if (!self._usbStates) self._usbStates = new Map();
                return asyncOp(self._usbStates, "in:" + device + ":" + endpoint + ":" + bufPtr, (entry) => {
                    e.device.transferIn(endpoint, Number(len)).then(
                        (r) => { entry.state = "ready"; entry.data = r.data; },
                        () => { entry.state = "failed"; }
                    );
                }, (st) => {
                    self._refreshViews();
                    const copy = Math.min(Number(len), st.data.byteLength);
                    self._u8.set(new Uint8Array(st.data.buffer, st.data.byteOffset, copy), bufPtr);
                    self._writeU64(transferredPtr, st.data.byteLength);
                    return WAPI_OK;
                });
            },
            transfer_out(device, endpoint, bufPtr, len, transferredPtr) {
                const e = self.handles.get(device);
                if (!e || e.type !== "usb") return WAPI_ERR_BADF;
                if (!self._usbStates) self._usbStates = new Map();
                return asyncOp(self._usbStates, "out:" + device + ":" + endpoint + ":" + bufPtr, (entry) => {
                    self._refreshViews();
                    const data = self._u8.slice(bufPtr, bufPtr + Number(len));
                    e.device.transferOut(endpoint, data).then(
                        (r) => { entry.state = "ready"; entry.written = r.bytesWritten; },
                        () => { entry.state = "failed"; }
                    );
                }, (st) => {
                    self._writeU64(transferredPtr, st.written);
                    return WAPI_OK;
                });
            },
            control_transfer(device, requestType, request, value, index, bufPtr, len, transferredPtr) {
                const e = self.handles.get(device);
                if (!e || e.type !== "usb") return WAPI_ERR_BADF;
                if (!self._usbStates) self._usbStates = new Map();
                // requestType direction bit: 0x80 = IN, 0 = OUT
                const isIn = (requestType & 0x80) !== 0;
                const recipients = ["device", "interface", "endpoint", "other"];
                const types = ["standard", "class", "vendor"];
                const setup = {
                    requestType: types[(requestType >> 5) & 0x3] || "vendor",
                    recipient:   recipients[requestType & 0xF] || "device",
                    request, value, index,
                };
                const key = "ctrl:" + device + ":" + bufPtr;
                return asyncOp(self._usbStates, key, (entry) => {
                    if (isIn) {
                        e.device.controlTransferIn(setup, Number(len)).then(
                            (r) => { entry.state = "ready"; entry.data = r.data; },
                            () => { entry.state = "failed"; }
                        );
                    } else {
                        self._refreshViews();
                        const data = self._u8.slice(bufPtr, bufPtr + Number(len));
                        e.device.controlTransferOut(setup, data).then(
                            (r) => { entry.state = "ready"; entry.written = r.bytesWritten; },
                            () => { entry.state = "failed"; }
                        );
                    }
                }, (st) => {
                    if (isIn && st.data) {
                        self._refreshViews();
                        const copy = Math.min(Number(len), st.data.byteLength);
                        self._u8.set(new Uint8Array(st.data.buffer, st.data.byteOffset, copy), bufPtr);
                        self._writeU64(transferredPtr, st.data.byteLength);
                    } else {
                        self._writeU64(transferredPtr, st.written || 0);
                    }
                    return WAPI_OK;
                });
            },
        };

        // -------------------------------------------------------------------
        // wapi_midi (MIDI - stub)
        // -------------------------------------------------------------------
        // Web MIDI. request_access is async — poll pattern. Once granted,
        // the MIDIAccess object is cached on the instance and enumeration
        // is synchronous.
        const wapi_midi = {
            request_access(sysex) {
                if (typeof navigator === "undefined" || !navigator.requestMIDIAccess)
                    return WAPI_ERR_NOTSUP;
                if (self._midiAccess) return WAPI_OK;
                if (self._midiState === "failed") { self._midiState = null; return WAPI_ERR_ACCES; }
                if (!self._midiState) {
                    self._midiState = "pending";
                    navigator.requestMIDIAccess({ sysex: !!sysex }).then(
                        (a) => { self._midiAccess = a; self._midiState = "ready"; },
                        () => { self._midiState = "failed"; }
                    );
                }
                return WAPI_ERR_AGAIN;
            },
            port_count(type) {
                if (!self._midiAccess) return 0;
                return type === 0 ? self._midiAccess.inputs.size : self._midiAccess.outputs.size;
            },
            port_name(type, index, bufPtr, bufLen, nameLenPtr) {
                if (!self._midiAccess) return WAPI_ERR_BADF;
                const coll = type === 0 ? self._midiAccess.inputs : self._midiAccess.outputs;
                const arr = Array.from(coll.values());
                if (index < 0 || index >= arr.length) return WAPI_ERR_OVERFLOW;
                const written = self._writeString(bufPtr, Number(bufLen), arr[index].name || "");
                self._writeU64(nameLenPtr, written);
                return WAPI_OK;
            },
            open_port(type, index, portPtr) {
                if (!self._midiAccess) return WAPI_ERR_BADF;
                const coll = type === 0 ? self._midiAccess.inputs : self._midiAccess.outputs;
                const arr = Array.from(coll.values());
                if (index < 0 || index >= arr.length) return WAPI_ERR_OVERFLOW;
                const port = arr[index];
                const entry = { type: "midi", port, inbox: [], kind: type };
                if (type === 0) {
                    port.onmidimessage = (e) => {
                        entry.inbox.push({ data: e.data, ts: BigInt(Math.round((e.timeStamp || performance.now()) * 1e6)) });
                    };
                }
                try { port.open && port.open(); } catch (_) {}
                const h = self.handles.insert(entry);
                self._writeI32(portPtr, h);
                return WAPI_OK;
            },
            close_port(port) {
                const e = self.handles.get(port);
                if (!e || e.type !== "midi") return WAPI_ERR_BADF;
                try { if (e.port.onmidimessage) e.port.onmidimessage = null; e.port.close && e.port.close(); } catch (_) {}
                self.handles.remove(port);
                return WAPI_OK;
            },
            send(port, dataPtr, len) {
                const e = self.handles.get(port);
                if (!e || e.type !== "midi") return WAPI_ERR_BADF;
                if (e.kind !== 1) return WAPI_ERR_INVAL;
                self._refreshViews();
                const data = self._u8.slice(dataPtr, dataPtr + Number(len));
                try { e.port.send(data); return WAPI_OK; }
                catch (_) { return WAPI_ERR_IO; }
            },
            recv(port, bufPtr, bufLen, msgLenPtr, timestampPtr) {
                const e = self.handles.get(port);
                if (!e || e.type !== "midi") return WAPI_ERR_BADF;
                if (e.kind !== 0) return WAPI_ERR_INVAL;
                if (e.inbox.length === 0) return WAPI_ERR_AGAIN;
                const msg = e.inbox.shift();
                self._refreshViews();
                const copy = Math.min(Number(bufLen), msg.data.length);
                self._u8.set(msg.data.subarray(0, copy), bufPtr);
                self._writeU64(msgLenPtr, msg.data.length);
                if (timestampPtr) self._dv.setBigUint64(timestampPtr, msg.ts, true);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_bt (Bluetooth LE - stub)
        // -------------------------------------------------------------------
        // Web Bluetooth. All operations are async — we expose a polling
        // state-machine for each device/service/characteristic. Keyed by
        // the caller's output pointer so repeated calls (which is how the
        // wasm polls) observe the same transition.
        const btKey = (op, p) => op + ":" + p;
        const btTake = (key) => {
            const st = self._btStates && self._btStates.get(key);
            if (st && st.state !== "pending") self._btStates.delete(key);
            return st;
        };
        const wapi_bt = {
            request_device(filtersPtr, filterCount, devicePtr) {
                if (typeof navigator === "undefined" || !navigator.bluetooth) return WAPI_ERR_NOTSUP;
                if (!self._btStates) self._btStates = new Map();
                const key = btKey("req", devicePtr);
                const st = btTake(key);
                if (st) {
                    if (st.state === "ready") {
                        const h = self.handles.insert({ type: "bt_device", device: st.device });
                        self._writeI32(devicePtr, h);
                        return WAPI_OK;
                    }
                    if (st.state === "canceled") return WAPI_ERR_CANCELED;
                    return WAPI_ERR_IO;
                }
                if (!self._btStates.has(key)) {
                    const entry = { state: "pending" };
                    self._btStates.set(key, entry);
                    // wapi_bt_filter_t: service_uuid (stringview 16) + name_prefix (16)
                    const filters = [];
                    for (let i = 0; i < filterCount; i++) {
                        const base = filtersPtr + i * 32;
                        const uuid = self._readStringView(base + 0);
                        const name = self._readStringView(base + 16);
                        const f = {};
                        if (uuid) f.services = [uuid.toLowerCase()];
                        if (name) f.namePrefix = name;
                        filters.push(f);
                    }
                    const opts = filterCount > 0
                        ? { filters }
                        : { acceptAllDevices: true };
                    navigator.bluetooth.requestDevice(opts).then(
                        (d) => { entry.state = "ready"; entry.device = d; },
                        (err) => { entry.state = err && err.name === "NotFoundError" ? "canceled" : "failed"; }
                    );
                }
                return WAPI_ERR_AGAIN;
            },
            connect(device) {
                const e = self.handles.get(device);
                if (!e || e.type !== "bt_device") return WAPI_ERR_BADF;
                if (e.server) return WAPI_OK;
                if (e.state === "pending") return WAPI_ERR_AGAIN;
                if (e.state === "failed") { e.state = null; return WAPI_ERR_IO; }
                e.state = "pending";
                e.device.gatt.connect().then(
                    (srv) => { e.server = srv; e.state = "ready"; },
                    () => { e.state = "failed"; }
                );
                return WAPI_ERR_AGAIN;
            },
            disconnect(device) {
                const e = self.handles.get(device);
                if (!e || e.type !== "bt_device") return WAPI_ERR_BADF;
                try { e.server && e.server.disconnect(); } catch (_) {}
                e.server = null;
                return WAPI_OK;
            },
            // get_service: (device, uuid_sv*, service_out*)
            get_service(device, uuidSvPtr, servicePtr) {
                const e = self.handles.get(device);
                if (!e || e.type !== "bt_device" || !e.server) return WAPI_ERR_BADF;
                if (!self._btStates) self._btStates = new Map();
                const key = btKey("getsvc", servicePtr);
                const st = btTake(key);
                if (st) {
                    if (st.state === "ready") {
                        const h = self.handles.insert({ type: "bt_service", service: st.service });
                        self._writeI32(servicePtr, h);
                        return WAPI_OK;
                    }
                    return WAPI_ERR_IO;
                }
                if (!self._btStates.has(key)) {
                    const entry = { state: "pending" };
                    self._btStates.set(key, entry);
                    const uuid = self._readStringView(uuidSvPtr) || "";
                    e.server.getPrimaryService(uuid.toLowerCase()).then(
                        (s) => { entry.state = "ready"; entry.service = s; },
                        () => { entry.state = "failed"; }
                    );
                }
                return WAPI_ERR_AGAIN;
            },
            get_characteristic(service, uuidSvPtr, charPtr) {
                const e = self.handles.get(service);
                if (!e || e.type !== "bt_service") return WAPI_ERR_BADF;
                if (!self._btStates) self._btStates = new Map();
                const key = btKey("getchar", charPtr);
                const st = btTake(key);
                if (st) {
                    if (st.state === "ready") {
                        const h = self.handles.insert({ type: "bt_char", ch: st.ch, notify: null });
                        self._writeI32(charPtr, h);
                        return WAPI_OK;
                    }
                    return WAPI_ERR_IO;
                }
                if (!self._btStates.has(key)) {
                    const entry = { state: "pending" };
                    self._btStates.set(key, entry);
                    const uuid = self._readStringView(uuidSvPtr) || "";
                    e.service.getCharacteristic(uuid.toLowerCase()).then(
                        (c) => { entry.state = "ready"; entry.ch = c; },
                        () => { entry.state = "failed"; }
                    );
                }
                return WAPI_ERR_AGAIN;
            },
            read_value(characteristic, bufPtr, bufLen, valLenPtr) {
                const e = self.handles.get(characteristic);
                if (!e || e.type !== "bt_char") return WAPI_ERR_BADF;
                if (!self._btStates) self._btStates = new Map();
                const key = btKey("read", bufPtr);
                const st = btTake(key);
                if (st) {
                    if (st.state === "ready") {
                        self._refreshViews();
                        const copy = Math.min(Number(bufLen), st.data.byteLength);
                        self._u8.set(new Uint8Array(st.data.buffer, st.data.byteOffset, copy), bufPtr);
                        self._writeU64(valLenPtr, st.data.byteLength);
                        return WAPI_OK;
                    }
                    return WAPI_ERR_IO;
                }
                if (!self._btStates.has(key)) {
                    const entry = { state: "pending" };
                    self._btStates.set(key, entry);
                    e.ch.readValue().then(
                        (dv) => { entry.state = "ready"; entry.data = new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength); },
                        () => { entry.state = "failed"; }
                    );
                }
                return WAPI_ERR_AGAIN;
            },
            write_value(characteristic, dataPtr, len) {
                const e = self.handles.get(characteristic);
                if (!e || e.type !== "bt_char") return WAPI_ERR_BADF;
                self._refreshViews();
                const data = self._u8.slice(dataPtr, dataPtr + Number(len));
                e.ch.writeValue(data).catch(() => {});
                return WAPI_OK;
            },
            start_notifications(characteristic) {
                const e = self.handles.get(characteristic);
                if (!e || e.type !== "bt_char") return WAPI_ERR_BADF;
                e.ch.startNotifications().then((c) => {
                    e.notify = (ev) => {
                        const dv = ev.target.value;
                        self._eventQueue.push({
                            type: WAPI_EVENT_IO_COMPLETION,
                            userData: BigInt(characteristic),
                            result: dv.byteLength,
                            flags: 0x20000, // bt notification
                        });
                    };
                    c.addEventListener("characteristicvaluechanged", e.notify);
                }).catch(() => {});
                return WAPI_OK;
            },
            stop_notifications(characteristic) {
                const e = self.handles.get(characteristic);
                if (!e || e.type !== "bt_char") return WAPI_ERR_BADF;
                if (e.notify) {
                    try { e.ch.removeEventListener("characteristicvaluechanged", e.notify); } catch (_) {}
                    e.ch.stopNotifications().catch(() => {});
                    e.notify = null;
                }
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_camera (camera - stub)
        // -------------------------------------------------------------------
        // getUserMedia-based camera. We only report a count of 1 without a
        // prompt (enumerateDevices requires permission). open() triggers the
        // permission prompt. read_frame draws to a hidden canvas and copies
        // pixels into the caller's buffer.
        const wapi_camera = {
            count() {
                return (typeof navigator !== "undefined" && navigator.mediaDevices &&
                        navigator.mediaDevices.getUserMedia) ? 1 : 0;
            },
            // desc: facing u32, width u32, height u32, pixel_format u32
            open(descPtr, cameraPtr) {
                if (typeof navigator === "undefined" || !navigator.mediaDevices ||
                    !navigator.mediaDevices.getUserMedia) return WAPI_ERR_NOTSUP;
                if (!self._camStates) self._camStates = new Map();
                return asyncOp(self._camStates, "open:" + cameraPtr, (entry) => {
                    self._refreshViews();
                    const facing = self._dv.getUint32(descPtr + 0, true);
                    const w      = self._dv.getUint32(descPtr + 4, true);
                    const h      = self._dv.getUint32(descPtr + 8, true);
                    const constraints = {
                        video: {
                            facingMode: facing === 1 ? "environment" : "user",
                            width:  w > 0 ? w : undefined,
                            height: h > 0 ? h : undefined,
                        },
                        audio: false,
                    };
                    navigator.mediaDevices.getUserMedia(constraints).then(
                        (stream) => {
                            const video = document.createElement("video");
                            video.srcObject = stream;
                            video.muted = true;
                            video.playsInline = true;
                            video.play().catch(() => {});
                            entry.state = "ready";
                            entry.stream = stream;
                            entry.video = video;
                            entry.canvas = document.createElement("canvas");
                            entry.ctx = entry.canvas.getContext("2d");
                        },
                        (err) => { entry.state = err && err.name === "NotAllowedError" ? "canceled" : "failed"; }
                    );
                }, (st) => {
                    const h = self.handles.insert({
                        type: "camera",
                        stream: st.stream, video: st.video,
                        canvas: st.canvas, ctx: st.ctx,
                    });
                    self._writeI32(cameraPtr, h);
                    return WAPI_OK;
                });
            },
            close(camera) {
                const e = self.handles.get(camera);
                if (!e || e.type !== "camera") return WAPI_ERR_BADF;
                try { e.stream.getTracks().forEach(t => t.stop()); } catch (_) {}
                self.handles.remove(camera);
                return WAPI_OK;
            },
            // wapi_camera_frame_t (24 bytes): width u32, height u32, format u32, _pad u32, timestamp u64
            read_frame(camera, framePtr, bufPtr, bufLen, sizePtr) {
                const e = self.handles.get(camera);
                if (!e || e.type !== "camera") return WAPI_ERR_BADF;
                const v = e.video;
                if (!v.videoWidth || !v.videoHeight) return WAPI_ERR_AGAIN;
                const w = v.videoWidth, h = v.videoHeight;
                if (e.canvas.width !== w) e.canvas.width = w;
                if (e.canvas.height !== h) e.canvas.height = h;
                e.ctx.drawImage(v, 0, 0, w, h);
                const img = e.ctx.getImageData(0, 0, w, h);
                self._refreshViews();
                const needed = img.data.length;
                self._writeU64(sizePtr, needed);
                self._dv.setUint32(framePtr + 0, w, true);
                self._dv.setUint32(framePtr + 4, h, true);
                self._dv.setUint32(framePtr + 8, 0 /* RGBA8 */, true);
                self._dv.setUint32(framePtr + 12, 0, true);
                self._dv.setBigUint64(framePtr + 16, BigInt(Math.round(performance.now() * 1e6)), true);
                if (Number(bufLen) < needed) return WAPI_ERR_OVERFLOW;
                self._u8.set(img.data, bufPtr);
                return WAPI_OK;
            },
            // Uploading to GPU would require wiring through our wgpu path.
            read_frame_gpu(_camera, _framePtr, _texturePtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_xr (XR / extended reality - stub)
        // -------------------------------------------------------------------
        // WebXR. Session lifecycle is fully async; we keep the polling
        // state-machine pattern and cache the latest frame state.
        const xrModes = ["immersive-vr", "immersive-ar", "inline"];
        const wapi_xr = {
            is_supported(type) {
                if (typeof navigator === "undefined" || !navigator.xr) return 0;
                // isSessionSupported is async. Best-effort: cache on first call.
                if (!self._xrSupported) self._xrSupported = {};
                const mode = xrModes[type];
                if (!mode) return 0;
                if (self._xrSupported[mode] === undefined) {
                    self._xrSupported[mode] = null;
                    navigator.xr.isSessionSupported(mode).then(
                        (ok) => { self._xrSupported[mode] = !!ok; },
                        () => { self._xrSupported[mode] = false; }
                    );
                }
                return self._xrSupported[mode] ? 1 : 0;
            },
            request_session(type, sessionPtr) {
                if (typeof navigator === "undefined" || !navigator.xr) return WAPI_ERR_NOTSUP;
                if (!self._xrStates) self._xrStates = new Map();
                return asyncOp(self._xrStates, "sess:" + sessionPtr, (entry) => {
                    const mode = xrModes[type] || "inline";
                    navigator.xr.requestSession(mode).then(
                        (s) => { entry.state = "ready"; entry.session = s; },
                        (err) => { entry.state = err && err.name === "NotAllowedError" ? "canceled" : "failed"; }
                    );
                }, (st) => {
                    const h = self.handles.insert({ type: "xr_session", session: st.session, lastFrame: null, refSpaces: new Map() });
                    st.session.addEventListener("end", () => {
                        const e = self.handles.get(h);
                        if (e) e.ended = true;
                    });
                    self._writeI32(sessionPtr, h);
                    return WAPI_OK;
                });
            },
            end_session(session) {
                const e = self.handles.get(session);
                if (!e || e.type !== "xr_session") return WAPI_ERR_BADF;
                try { e.session.end(); } catch (_) {}
                self.handles.remove(session);
                return WAPI_OK;
            },
            create_ref_space(session, type, spacePtr) {
                const e = self.handles.get(session);
                if (!e || e.type !== "xr_session") return WAPI_ERR_BADF;
                const names = ["local","local-floor","bounded-floor","unbounded","viewer"];
                const name = names[type] || "local";
                return asyncOp(self._xrStates, "refspc:" + spacePtr, (entry) => {
                    e.session.requestReferenceSpace(name).then(
                        (s) => { entry.state = "ready"; entry.space = s; },
                        () => { entry.state = "failed"; }
                    );
                }, (st) => {
                    const h = self.handles.insert({ type: "xr_space", space: st.space });
                    self._writeI32(spacePtr, h);
                    return WAPI_OK;
                });
            },
            // Stubs for things that need an actual render loop hookup — keep
            // NOTSUP until the higher-level XR integration is designed. The
            // important thing is session/space acquisition works.
            wait_frame(_session, _statePtr, _viewsPtr, _maxViews) { return WAPI_ERR_NOTSUP; },
            begin_frame(_session) { return WAPI_ERR_NOTSUP; },
            end_frame(_session, _texturesPtr, _texCount) { return WAPI_ERR_NOTSUP; },
            get_controller_pose(_session, _space, _hand, _posePtr) { return WAPI_ERR_NOTSUP; },
            get_controller_state(_session, _hand, _buttonsPtr, _triggerPtr, _gripPtr, _thumbXPtr, _thumbYPtr) { return WAPI_ERR_NOTSUP; },
            hit_test(_session, _space, _originPtr, _directionPtr, _posePtr) { return WAPI_ERR_NOTSUP; },
        };

        // wapi_event: REMOVED — event delivery is now part of wapi_io vtable
        // (poll/wait/flush are on the wapi_io object above)

        // -------------------------------------------------------------------
        // wapi_register (app registration - stub)
        // -------------------------------------------------------------------
        // Browser exposes only protocol handler registration. Filetype handlers
        // and preview handlers require a PWA manifest (static) or OS APIs we
        // can't reach from a page.
        const wapi_register = {
            scheme_add(schemeSvPtr) {
                if (typeof navigator === "undefined" || !navigator.registerProtocolHandler)
                    return WAPI_ERR_NOTSUP;
                const scheme = self._readStringView(schemeSvPtr);
                if (!scheme) return WAPI_ERR_INVAL;
                try {
                    // Use current page as the handler.
                    navigator.registerProtocolHandler(scheme, location.href + "#url=%s");
                    return WAPI_OK;
                } catch (_) { return WAPI_ERR_ACCES; }
            },
            scheme_remove(schemeSvPtr) {
                if (typeof navigator === "undefined" || !navigator.unregisterProtocolHandler)
                    return WAPI_ERR_NOTSUP;
                const scheme = self._readStringView(schemeSvPtr);
                if (!scheme) return WAPI_ERR_INVAL;
                try {
                    navigator.unregisterProtocolHandler(scheme, location.href + "#url=%s");
                    return WAPI_OK;
                } catch (_) { return WAPI_ERR_ACCES; }
            },
            filetype_add(_descPtr)   { return WAPI_ERR_NOTSUP; },
            filetype_remove(_sv)     { return WAPI_ERR_NOTSUP; },
            preview_add(_sv)         { return WAPI_ERR_NOTSUP; },
            scheme_isdefault(_sv)    { return 0; },
            filetype_isdefault(_sv)  { return 0; },
        };

        // -------------------------------------------------------------------
        // wapi_taskbar (taskbar/dock - stub)
        // -------------------------------------------------------------------
        const wapi_taskbar = {
            // Surface progress as a bracket in the tab title when fullscreen
            // browser APIs aren't available. Too noisy if we update every
            // frame — throttle to 1% steps.
            set_progress(surface, state, value) {
                if (typeof document === "undefined") return WAPI_ERR_NOTSUP;
                const pct = Math.round(value * 100);
                if (self._lastProgressPct === pct) return WAPI_OK;
                self._lastProgressPct = pct;
                const base = self._baseTitle != null ? self._baseTitle : document.title;
                if (self._baseTitle == null) self._baseTitle = base;
                if (state === 0 /* NONE */) {
                    document.title = self._baseTitle;
                } else if (state === 1 /* INDETERMINATE */) {
                    document.title = `[…] ${self._baseTitle}`;
                } else {
                    document.title = `[${pct}%] ${self._baseTitle}`;
                }
                return WAPI_OK;
            },
            // App Badging API — works on installed PWAs only.
            set_badge(count) {
                if (typeof navigator === "undefined" || !navigator.setAppBadge)
                    return WAPI_ERR_NOTSUP;
                try {
                    if (count > 0) navigator.setAppBadge(count);
                    else navigator.clearAppBadge();
                    return WAPI_OK;
                } catch (_) { return WAPI_ERR_IO; }
            },
            // No "request attention" on the web — best effort: flash title.
            request_attention(_surface, _critical) {
                if (typeof document === "undefined") return WAPI_ERR_NOTSUP;
                const base = self._baseTitle != null ? self._baseTitle : document.title;
                if (self._baseTitle == null) self._baseTitle = base;
                let n = 0;
                const id = setInterval(() => {
                    document.title = (n++ % 2) ? `* ${base}` : base;
                    if (n > 10) { clearInterval(id); document.title = base; }
                }, 500);
                return WAPI_OK;
            },
            set_overlay_icon(_surface, _iconData, _iconLen, _desc, _descLen) { return WAPI_ERR_NOTSUP; },
            clear_overlay(_surface) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_orient (screen orientation)
        // -------------------------------------------------------------------
        const wapi_orient = {
            get(orientPtr) {
                self._refreshViews();
                let v = 0;
                if (typeof screen !== "undefined" && screen.orientation) {
                    switch (screen.orientation.type) {
                        case "portrait-primary":    v = 0; break;
                        case "portrait-secondary":  v = 1; break;
                        case "landscape-primary":   v = 2; break;
                        case "landscape-secondary": v = 3; break;
                    }
                }
                self._writeU32(orientPtr, v);
                return WAPI_OK;
            },
            lock(lockType) {
                if (typeof screen === "undefined" || !screen.orientation || !screen.orientation.lock)
                    return WAPI_ERR_NOTSUP;
                const names = ["any", "portrait", "landscape", "natural"];
                const name = names[lockType] || "any";
                try { screen.orientation.lock(name).catch(() => {}); return WAPI_OK; }
                catch (_) { return WAPI_ERR_IO; }
            },
            unlock() {
                if (typeof screen !== "undefined" && screen.orientation && screen.orientation.unlock) {
                    try { screen.orientation.unlock(); } catch (_) {}
                }
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_codec (codec - stub)
        // -------------------------------------------------------------------
        // WebCodecs (VideoDecoder, VideoEncoder, AudioDecoder, AudioEncoder).
        // Fully wiring all struct fields is substantial; we expose a
        // "best-effort" subset that covers create/decode/encode/output pull.
        const codecNames = {
            0: "avc1.42e01e", 1: "hev1.1.6.L93.B0", 2: "vp09.00.10.08", 3: "av01.0.01M.08",
            10: "mp4a.40.2", 11: "opus", 12: "vorbis", 13: "flac",
        };
        const wapi_codec = {
            // video desc: codec u32, width u32, height u32, bitrate u32, fps u32, mode u32 + pad
            create_video(descPtr, codecPtr) {
                if (typeof VideoDecoder === "undefined") return WAPI_ERR_NOTSUP;
                self._refreshViews();
                const codecId = self._dv.getUint32(descPtr + 0, true);
                const width   = self._dv.getUint32(descPtr + 4, true);
                const height  = self._dv.getUint32(descPtr + 8, true);
                const bitrate = self._dv.getUint32(descPtr + 12, true);
                const fps     = self._dv.getUint32(descPtr + 16, true);
                const mode    = self._dv.getUint32(descPtr + 20, true); // 0 decode, 1 encode
                const codec = codecNames[codecId];
                if (!codec) return WAPI_ERR_INVAL;
                const outQueue = [];
                const errors = [];
                let instance;
                try {
                    if (mode === 0) {
                        instance = new VideoDecoder({
                            output: (frame) => { outQueue.push(frame); },
                            error:  (e) => { errors.push(e); },
                        });
                        instance.configure({ codec });
                    } else {
                        instance = new VideoEncoder({
                            output: (chunk) => {
                                const data = new Uint8Array(chunk.byteLength);
                                chunk.copyTo(data);
                                outQueue.push({ data, timestamp: chunk.timestamp, type: chunk.type });
                            },
                            error: (e) => { errors.push(e); },
                        });
                        instance.configure({
                            codec, width, height,
                            bitrate: bitrate > 0 ? bitrate : 1_000_000,
                            framerate: fps > 0 ? fps : 30,
                        });
                    }
                } catch (_) { return WAPI_ERR_INVAL; }
                const h = self.handles.insert({ type: "codec", codec: instance, kind: "video", mode, out: outQueue, errors });
                self._writeI32(codecPtr, h);
                return WAPI_OK;
            },
            create_audio(descPtr, codecPtr) {
                if (typeof AudioDecoder === "undefined") return WAPI_ERR_NOTSUP;
                self._refreshViews();
                const codecId    = self._dv.getUint32(descPtr + 0, true);
                const sampleRate = self._dv.getUint32(descPtr + 4, true);
                const channels   = self._dv.getUint32(descPtr + 8, true);
                const mode       = self._dv.getUint32(descPtr + 12, true);
                const codec = codecNames[codecId];
                if (!codec) return WAPI_ERR_INVAL;
                const outQueue = [];
                const errors = [];
                let instance;
                try {
                    if (mode === 0) {
                        instance = new AudioDecoder({
                            output: (frame) => { outQueue.push(frame); },
                            error: (e) => { errors.push(e); },
                        });
                        instance.configure({ codec, sampleRate, numberOfChannels: channels });
                    } else {
                        instance = new AudioEncoder({
                            output: (chunk) => {
                                const data = new Uint8Array(chunk.byteLength);
                                chunk.copyTo(data);
                                outQueue.push({ data, timestamp: chunk.timestamp });
                            },
                            error: (e) => { errors.push(e); },
                        });
                        instance.configure({ codec, sampleRate, numberOfChannels: channels });
                    }
                } catch (_) { return WAPI_ERR_INVAL; }
                const h = self.handles.insert({ type: "codec", codec: instance, kind: "audio", mode, out: outQueue, errors });
                self._writeI32(codecPtr, h);
                return WAPI_OK;
            },
            destroy(codec) {
                const e = self.handles.get(codec);
                if (!e || e.type !== "codec") return WAPI_ERR_BADF;
                try { e.codec.close(); } catch (_) {}
                self.handles.remove(codec);
                return WAPI_OK;
            },
            is_supported(codecType, mode) {
                const codec = codecNames[codecType];
                if (!codec) return 0;
                // isConfigSupported is async; return 1 optimistically if the
                // API exists. Apps needing a hard check can use query_decode.
                if (mode === 0) return typeof VideoDecoder !== "undefined" ? 1 : 0;
                return typeof VideoEncoder !== "undefined" ? 1 : 0;
            },
            // chunk: data ptr i64, data_len u32, _pad, timestamp i64, flags u32, _res, _pad (40 bytes)
            decode(codec, chunkPtr) {
                const e = self.handles.get(codec);
                if (!e || e.type !== "codec" || e.mode !== 0) return WAPI_ERR_BADF;
                self._refreshViews();
                const dptr  = Number(self._dv.getBigUint64(chunkPtr + 0, true));
                const dlen  = self._dv.getUint32(chunkPtr + 8, true);
                const ts    = Number(self._dv.getBigInt64(chunkPtr + 16, true));
                const flags = self._dv.getUint32(chunkPtr + 24, true);
                const data  = self._u8.slice(dptr, dptr + dlen);
                try {
                    if (e.kind === "video") {
                        e.codec.decode(new EncodedVideoChunk({
                            type: (flags & 0x1) ? "key" : "delta",
                            timestamp: ts,
                            data,
                        }));
                    } else {
                        e.codec.decode(new EncodedAudioChunk({
                            type: (flags & 0x1) ? "key" : "delta",
                            timestamp: ts,
                            data,
                        }));
                    }
                    return WAPI_OK;
                } catch (_) { return WAPI_ERR_IO; }
            },
            encode(codec, dataPtr, dataLen, timestampLo, timestampHi) {
                // Video/audio encode takes frames, not raw data — full support
                // needs VideoFrame creation which requires a source image. Leave
                // as NOTSUP for now; wasm-side encoders usually go through an
                // explicit frame-creation entry point.
                return WAPI_ERR_NOTSUP;
            },
            get_output(codec, bufPtr, bufLen, outLenPtr, tsPtr) {
                const e = self.handles.get(codec);
                if (!e || e.type !== "codec") return WAPI_ERR_BADF;
                if (e.errors.length > 0) return WAPI_ERR_IO;
                if (e.out.length === 0) return WAPI_ERR_AGAIN;
                const item = e.out.shift();
                self._refreshViews();
                if (e.mode === 1) {
                    // Encoded chunk: copy bytes.
                    const n = Math.min(Number(bufLen), item.data.length);
                    self._u8.set(item.data.subarray(0, n), bufPtr);
                    self._writeU64(outLenPtr, item.data.length);
                    if (tsPtr) self._dv.setBigInt64(tsPtr, BigInt(item.timestamp || 0), true);
                    return WAPI_OK;
                }
                // Decoded VideoFrame / AudioData — copy into caller's buffer.
                try {
                    if (e.kind === "video") {
                        const buf = new Uint8Array(item.allocationSize({ format: "RGBA" }));
                        item.copyTo(buf, { format: "RGBA" }).then(() => {}); // async, best-effort
                        const n = Math.min(Number(bufLen), buf.length);
                        self._u8.set(buf.subarray(0, n), bufPtr);
                        self._writeU64(outLenPtr, buf.length);
                        if (tsPtr) self._dv.setBigInt64(tsPtr, BigInt(item.timestamp || 0), true);
                        item.close();
                    } else {
                        const samples = item.numberOfFrames * item.numberOfChannels;
                        const buf = new Float32Array(samples);
                        item.copyTo(buf, { planeIndex: 0 });
                        const bytes = new Uint8Array(buf.buffer);
                        const n = Math.min(Number(bufLen), bytes.length);
                        self._u8.set(bytes.subarray(0, n), bufPtr);
                        self._writeU64(outLenPtr, bytes.length);
                        if (tsPtr) self._dv.setBigInt64(tsPtr, BigInt(item.timestamp || 0), true);
                        item.close();
                    }
                    return WAPI_OK;
                } catch (_) { return WAPI_ERR_IO; }
            },
            flush(codec) {
                const e = self.handles.get(codec);
                if (!e || e.type !== "codec") return WAPI_ERR_BADF;
                try { e.codec.flush().catch(() => {}); } catch (_) {}
                return WAPI_OK;
            },
            query_decode(_queryPtr, _resultPtr) { return WAPI_ERR_NOTSUP; },
            query_encode(_queryPtr, _resultPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_media (media session - stub)
        // -------------------------------------------------------------------
        // MediaSession API. Metadata struct layout (64 bytes):
        //   title(16) artist(16) album(16) artwork(8) artwork_len(4) _pad(4)
        const wapi_media = {
            set_metadata(metadataPtr) {
                if (typeof navigator === "undefined" || !navigator.mediaSession) return WAPI_ERR_NOTSUP;
                const title  = self._readStringView(metadataPtr + 0)  || "";
                const artist = self._readStringView(metadataPtr + 16) || "";
                const album  = self._readStringView(metadataPtr + 32) || "";
                try {
                    navigator.mediaSession.metadata = new MediaMetadata({ title, artist, album });
                    return WAPI_OK;
                } catch (_) { return WAPI_ERR_IO; }
            },
            set_playback_state(state) {
                if (typeof navigator === "undefined" || !navigator.mediaSession) return WAPI_ERR_NOTSUP;
                const names = ["none", "paused", "playing"];
                navigator.mediaSession.playbackState = names[state] || "none";
                return WAPI_OK;
            },
            set_position(position, duration) {
                if (typeof navigator === "undefined" || !navigator.mediaSession ||
                    !navigator.mediaSession.setPositionState) return WAPI_ERR_NOTSUP;
                try {
                    navigator.mediaSession.setPositionState({
                        position, duration, playbackRate: 1.0,
                    });
                    return WAPI_OK;
                } catch (_) { return WAPI_ERR_INVAL; }
            },
            // Action handlers emit IO completion events carrying the action id.
            set_actions(actionsPtr, count) {
                if (typeof navigator === "undefined" || !navigator.mediaSession) return WAPI_ERR_NOTSUP;
                const names = ["play","pause","stop","seekbackward","seekforward","previoustrack","nexttrack"];
                const enabled = new Set();
                self._refreshViews();
                for (let i = 0; i < count; i++) {
                    const a = self._dv.getUint32(actionsPtr + i * 4, true);
                    if (names[a]) enabled.add(a);
                }
                // Clear existing.
                for (const n of names) {
                    try { navigator.mediaSession.setActionHandler(n, null); } catch (_) {}
                }
                for (const a of enabled) {
                    const name = names[a];
                    try {
                        navigator.mediaSession.setActionHandler(name, () => {
                            self._eventQueue.push({
                                type: WAPI_EVENT_IO_COMPLETION,
                                userData: BigInt(a),
                                result: 0, flags: 0x10000, // flag: media-session action
                            });
                        });
                    } catch (_) {}
                }
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_encode (text encoding - stub)
        // -------------------------------------------------------------------
        // Text encoding conversions. TextDecoder supports every "from"
        // encoding the spec lists; TextEncoder only emits UTF-8, so
        // "to" != UTF-8 falls back to a small switch.
        const WAPI_ENC = {
            0: "utf-8", 1: "utf-16le", 2: "utf-16be",
            3: "iso-8859-1", 4: "windows-1252",
            5: "shift_jis", 6: "euc-jp",
            7: "gb18030",   // covers GB2312 data
            8: "big5", 9: "euc-kr",
        };
        const _encodeTo = (encName, text) => {
            if (encName === "utf-8") return new TextEncoder().encode(text);
            if (encName === "utf-16le") {
                const out = new Uint8Array(text.length * 2);
                const dv = new DataView(out.buffer);
                for (let i = 0; i < text.length; i++) dv.setUint16(i * 2, text.charCodeAt(i), true);
                return out;
            }
            if (encName === "utf-16be") {
                const out = new Uint8Array(text.length * 2);
                const dv = new DataView(out.buffer);
                for (let i = 0; i < text.length; i++) dv.setUint16(i * 2, text.charCodeAt(i), false);
                return out;
            }
            if (encName === "iso-8859-1" || encName === "windows-1252") {
                const out = new Uint8Array(text.length);
                for (let i = 0; i < text.length; i++) out[i] = text.charCodeAt(i) & 0xFF;
                return out;
            }
            return null; // unsupported
        };
        const wapi_encode = {
            convert(from, to, inputPtr, inputLen, outputPtr, outputLen, bytesWrittenPtr) {
                const fromEnc = WAPI_ENC[from];
                const toEnc   = WAPI_ENC[to];
                if (!fromEnc || !toEnc) return WAPI_ERR_INVAL;
                self._refreshViews();
                let text;
                try {
                    text = new TextDecoder(fromEnc, { fatal: false })
                        .decode(self._u8.subarray(inputPtr, inputPtr + Number(inputLen)));
                } catch (_) { return WAPI_ERR_IO; }
                const encoded = _encodeTo(toEnc, text);
                if (!encoded) return WAPI_ERR_NOTSUP;
                const copy = Math.min(encoded.length, Number(outputLen));
                self._u8.set(encoded.subarray(0, copy), outputPtr);
                self._writeU64(bytesWrittenPtr, encoded.length);
                return encoded.length > Number(outputLen) ? WAPI_ERR_OVERFLOW : WAPI_OK;
            },
            query_size(from, to, inputPtr, inputLen, requiredLenPtr) {
                const fromEnc = WAPI_ENC[from];
                const toEnc   = WAPI_ENC[to];
                if (!fromEnc || !toEnc) return WAPI_ERR_INVAL;
                self._refreshViews();
                let text;
                try {
                    text = new TextDecoder(fromEnc, { fatal: false })
                        .decode(self._u8.subarray(inputPtr, inputPtr + Number(inputLen)));
                } catch (_) { return WAPI_ERR_IO; }
                const encoded = _encodeTo(toEnc, text);
                if (!encoded) return WAPI_ERR_NOTSUP;
                self._writeU64(requiredLenPtr, encoded.length);
                return WAPI_OK;
            },
            detect(dataPtr, len, encodingPtr, confidencePtr) {
                self._refreshViews();
                const bytes = self._u8.subarray(dataPtr, dataPtr + Number(len));
                // BOM sniffing.
                if (bytes.length >= 3 && bytes[0]===0xEF && bytes[1]===0xBB && bytes[2]===0xBF) {
                    self._writeU32(encodingPtr, 0); self._writeF32(confidencePtr, 1.0); return WAPI_OK;
                }
                if (bytes.length >= 2 && bytes[0]===0xFF && bytes[1]===0xFE) {
                    self._writeU32(encodingPtr, 1); self._writeF32(confidencePtr, 1.0); return WAPI_OK;
                }
                if (bytes.length >= 2 && bytes[0]===0xFE && bytes[1]===0xFF) {
                    self._writeU32(encodingPtr, 2); self._writeF32(confidencePtr, 1.0); return WAPI_OK;
                }
                // Heuristic: validate as UTF-8.
                try {
                    new TextDecoder("utf-8", { fatal: true }).decode(bytes);
                    self._writeU32(encodingPtr, 0); self._writeF32(confidencePtr, 0.8); return WAPI_OK;
                } catch (_) {
                    self._writeU32(encodingPtr, 4); self._writeF32(confidencePtr, 0.3);
                    return WAPI_OK;
                }
            },
        };

        // -------------------------------------------------------------------
        // wapi_authn (web authentication - stub)
        // -------------------------------------------------------------------
        // WebAuthn via navigator.credentials. Both operations are async →
        // polling pattern keyed by the caller-visible buffer address.
        // For create_credential we need a user struct; minimal interpretation:
        //   user layout: id sv(16), name sv(16), display_name sv(16) = 48 bytes
        const wapi_authn = {
            // Note: rp_id is an sv, but the existing stub takes (rpIdPtr, rpIdLen). Follow existing style.
            create_credential(rpIdPtr, rpIdLen, userPtr, challengePtr, challengeLen) {
                if (typeof navigator === "undefined" || !navigator.credentials ||
                    typeof PublicKeyCredential === "undefined") return WAPI_ERR_NOTSUP;
                if (!self._authnStates) self._authnStates = new Map();
                const key = "create:" + challengePtr;
                const st = self._authnStates.get(key);
                if (st && st.state !== "pending") {
                    self._authnStates.delete(key);
                    if (st.state !== "ready") return WAPI_ERR_ACCES;
                    // We don't write the full credential back — the C-side
                    // caller must wait for get_assertion. Signal success.
                    return WAPI_OK;
                }
                if (!st) {
                    const entry = { state: "pending" };
                    self._authnStates.set(key, entry);
                    const rpId = self._readString(rpIdPtr, Number(rpIdLen));
                    const userId  = self._readStringView(userPtr + 0)  || "user";
                    const userName= self._readStringView(userPtr + 16) || userId;
                    const display = self._readStringView(userPtr + 32) || userName;
                    const challenge = self._u8.slice(challengePtr, challengePtr + Number(challengeLen));
                    navigator.credentials.create({
                        publicKey: {
                            rp: { id: rpId, name: rpId },
                            user: {
                                id: new TextEncoder().encode(userId),
                                name: userName,
                                displayName: display,
                            },
                            challenge,
                            pubKeyCredParams: [{ type: "public-key", alg: -7 }, { type: "public-key", alg: -257 }],
                            timeout: 60000,
                        },
                    }).then(
                        (_cred) => { entry.state = "ready"; },
                        () => { entry.state = "failed"; }
                    );
                }
                return WAPI_ERR_AGAIN;
            },
            get_assertion(rpIdPtr, rpIdLen, challengePtr, challengeLen) {
                if (typeof navigator === "undefined" || !navigator.credentials ||
                    typeof PublicKeyCredential === "undefined") return WAPI_ERR_NOTSUP;
                if (!self._authnStates) self._authnStates = new Map();
                const key = "get:" + challengePtr;
                const st = self._authnStates.get(key);
                if (st && st.state !== "pending") {
                    self._authnStates.delete(key);
                    return st.state === "ready" ? WAPI_OK : WAPI_ERR_ACCES;
                }
                if (!st) {
                    const entry = { state: "pending" };
                    self._authnStates.set(key, entry);
                    const rpId = self._readString(rpIdPtr, Number(rpIdLen));
                    const challenge = self._u8.slice(challengePtr, challengePtr + Number(challengeLen));
                    navigator.credentials.get({
                        publicKey: { challenge, rpId, timeout: 60000 },
                    }).then(
                        () => { entry.state = "ready"; },
                        () => { entry.state = "failed"; }
                    );
                }
                return WAPI_ERR_AGAIN;
            },
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
            // wapi_power_info_t: 16 bytes. u32 source, f32 battery, f32 seconds, u32 pad.
            // Battery API is async; we kick off once, cache on the instance, and
            // subsequent calls return the cached snapshot.
            get_info(infoPtr) {
                self._refreshViews();
                if (typeof navigator === "undefined" || !navigator.getBattery) {
                    self._dv.setUint32 (infoPtr + 0, 0, true);    // UNKNOWN
                    self._dv.setFloat32(infoPtr + 4, 1.0, true);
                    self._dv.setFloat32(infoPtr + 8, Infinity, true);
                    self._dv.setUint32 (infoPtr + 12, 0, true);
                    return WAPI_OK;
                }
                if (!self._batteryHandle) {
                    self._batteryHandle = { src: 0, level: 1.0, seconds: Infinity };
                    navigator.getBattery().then((b) => {
                        const update = () => {
                            self._batteryHandle.level   = b.level;
                            self._batteryHandle.seconds = b.charging
                                ? (isFinite(b.chargingTime) ? b.chargingTime : Infinity)
                                : (isFinite(b.dischargingTime) ? b.dischargingTime : Infinity);
                            self._batteryHandle.src = b.charging
                                ? (b.level >= 0.999 ? 4 /* CHARGED */ : 3 /* CHARGING */)
                                : 1 /* BATTERY */;
                        };
                        update();
                        b.addEventListener("levelchange", update);
                        b.addEventListener("chargingchange", update);
                        b.addEventListener("chargingtimechange", update);
                        b.addEventListener("dischargingtimechange", update);
                    });
                }
                self._dv.setUint32 (infoPtr + 0, self._batteryHandle.src, true);
                self._dv.setFloat32(infoPtr + 4, self._batteryHandle.level, true);
                self._dv.setFloat32(infoPtr + 8, self._batteryHandle.seconds, true);
                self._dv.setUint32 (infoPtr + 12, 0, true);
                return WAPI_OK;
            },
            // Screen Wake Lock API. Async acquire; we optimistically insert a
            // handle and upgrade it once the promise resolves.
            wake_acquire(type, lockPtr) {
                if (typeof navigator === "undefined" || !navigator.wakeLock) return WAPI_ERR_NOTSUP;
                const kind = type === 1 ? "screen" : "screen"; // only "screen" exists on the web
                const entry = { type: "wakelock", lock: null, released: false };
                navigator.wakeLock.request(kind).then(
                    (l) => {
                        if (entry.released) { try { l.release(); } catch (_) {} return; }
                        entry.lock = l;
                    },
                    () => { entry.released = true; }
                );
                const h = self.handles.insert(entry);
                self._writeI32(lockPtr, h);
                return WAPI_OK;
            },
            wake_release(lock) {
                const e = self.handles.get(lock);
                if (!e || e.type !== "wakelock") return WAPI_ERR_BADF;
                e.released = true;
                if (e.lock) { try { e.lock.release(); } catch (_) {} }
                self.handles.remove(lock);
                return WAPI_OK;
            },
            // Idle Detection API. Async start.
            idle_start(thresholdMs) {
                if (typeof IdleDetector === "undefined") return WAPI_ERR_NOTSUP;
                if (self._idleDetector) return WAPI_ERR_BUSY;
                IdleDetector.requestPermission().then((perm) => {
                    if (perm !== "granted") return;
                    const detector = new IdleDetector();
                    detector.addEventListener("change", () => {
                        self._idleState = detector.userState === "idle"
                            ? (detector.screenState === "locked" ? 2 : 1)
                            : 0;
                    });
                    detector.start({ threshold: Math.max(60_000, thresholdMs) })
                        .then(() => { self._idleDetector = detector; })
                        .catch(() => {});
                });
                return WAPI_OK;
            },
            idle_stop() {
                // No abort controller tracked; best-effort drop reference.
                self._idleDetector = null;
                self._idleState = 0;
                return WAPI_OK;
            },
            idle_get(statePtr) {
                self._writeU32(statePtr, self._idleState || 0);
                return WAPI_OK;
            },
            // Data Saver flag surfaces through navigator.connection.saveData.
            saver_get(statePtr) {
                const c = typeof navigator !== "undefined" && navigator.connection;
                self._writeU32(statePtr, c && c.saveData ? 1 : 0);
                return WAPI_OK;
            },
            thermal_get(statePtr) {
                // No browser thermal API — always nominal.
                self._writeU32(statePtr, 0);
                return WAPI_OK;
            },
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
            gamepad_vibrate(gamepadIndex, strong, weak, durationMs) {
                if (typeof navigator === "undefined" || !navigator.getGamepads) return WAPI_ERR_NOTSUP;
                const pads = navigator.getGamepads();
                const pad = pads && pads[gamepadIndex];
                if (!pad || !pad.vibrationActuator) return WAPI_ERR_BADF;
                try {
                    pad.vibrationActuator.playEffect("dual-rumble", {
                        duration: durationMs,
                        strongMagnitude: strong,
                        weakMagnitude: weak,
                    }).catch(() => {});
                    return WAPI_OK;
                } catch (_) { return WAPI_ERR_IO; }
            },
            // Per-device haptics. The web platform doesn't expose
            // independent haptic devices beyond gamepads and the top-level
            // Vibration API — treat index 0 as the default vibration
            // device.
            haptic_open(deviceIndex, outHandlePtr) {
                if (typeof navigator === "undefined" || !navigator.vibrate) return WAPI_ERR_NOTSUP;
                const h = self.handles.insert({ type: "haptic", idx: deviceIndex });
                self._writeI32(outHandlePtr, h);
                return WAPI_OK;
            },
            haptic_close(handle) {
                if (!self.handles.get(handle)) return WAPI_ERR_BADF;
                self.handles.remove(handle);
                return WAPI_OK;
            },
            haptic_rumble(handle, strength, durationMs) {
                const e = self.handles.get(handle);
                if (!e || e.type !== "haptic") return WAPI_ERR_BADF;
                // Vibration API is binary — approximate strength via duration.
                navigator.vibrate(Math.max(1, durationMs));
                return WAPI_OK;
            },
            haptic_get_features(_handle) {
                // Rumble only on the web.
                return 0x20; // FEAT_RUMBLE
            },
            haptic_effect_create(handle, descPtr, descLen, outEffect) {
                const e = self.handles.get(handle);
                if (!e || e.type !== "haptic") return WAPI_ERR_BADF;
                // Cache the desc as an effect record.
                self._refreshViews();
                const data = self._u8.slice(descPtr, descPtr + Number(descLen));
                const h = self.handles.insert({ type: "haptic_effect", desc: data, parent: handle });
                self._writeI32(outEffect, h);
                return WAPI_OK;
            },
            haptic_effect_play(effect, iterations) {
                const e = self.handles.get(effect);
                if (!e || e.type !== "haptic_effect") return WAPI_ERR_BADF;
                const pulse = [];
                for (let i = 0; i < Math.max(1, iterations); i++) pulse.push(50, 50);
                try { navigator.vibrate(pulse); return WAPI_OK; }
                catch (_) { return WAPI_ERR_IO; }
            },
            haptic_effect_stop(_effect) { navigator.vibrate(0); return WAPI_OK; },
            haptic_effect_destroy(effect) {
                if (!self.handles.get(effect)) return WAPI_ERR_BADF;
                self.handles.remove(effect);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_serial (serial port - stub)
        // -------------------------------------------------------------------
        // Web Serial. Async throughout; the same polling pattern.
        const wapi_serial = {
            request_port(portPtr) {
                if (typeof navigator === "undefined" || !navigator.serial) return WAPI_ERR_NOTSUP;
                if (!self._serialStates) self._serialStates = new Map();
                return asyncOp(self._serialStates, "req:" + portPtr, (entry) => {
                    navigator.serial.requestPort({}).then(
                        (p) => { entry.state = "ready"; entry.port = p; },
                        (err) => { entry.state = err && err.name === "NotFoundError" ? "canceled" : "failed"; }
                    );
                }, (st) => {
                    const h = self.handles.insert({ type: "serial", port: st.port, reader: null, writer: null, rxQueue: [] });
                    self._writeI32(portPtr, h);
                    return WAPI_OK;
                });
            },
            // desc: 16 bytes. baud u32, data_bits u8, stop_bits u8, parity u8, flow u8, pad u64
            open(port, descPtr) {
                const e = self.handles.get(port);
                if (!e || e.type !== "serial") return WAPI_ERR_BADF;
                if (e.opened) return WAPI_OK;
                if (!self._serialStates) self._serialStates = new Map();
                return asyncOp(self._serialStates, "open:" + port, (entry) => {
                    self._refreshViews();
                    const baud   = self._dv.getUint32(descPtr + 0, true) || 9600;
                    const dbits  = self._u8[descPtr + 4] || 8;
                    const sbits  = self._u8[descPtr + 5] || 1;
                    const parity = self._u8[descPtr + 6];
                    const flow   = self._u8[descPtr + 7];
                    const opts = {
                        baudRate: baud,
                        dataBits: dbits,
                        stopBits: sbits,
                        parity: ["none","even","odd"][parity] || "none",
                        flowControl: flow ? "hardware" : "none",
                    };
                    e.port.open(opts).then(
                        async () => {
                            e.opened = true;
                            e.reader = e.port.readable.getReader();
                            e.writer = e.port.writable.getWriter();
                            // Pump the reader in background.
                            (async () => {
                                try {
                                    while (e.opened) {
                                        const { value, done } = await e.reader.read();
                                        if (done) break;
                                        if (value) e.rxQueue.push(value);
                                    }
                                } catch (_) {}
                            })();
                            entry.state = "ready";
                        },
                        () => { entry.state = "failed"; }
                    );
                }, () => WAPI_OK);
            },
            close(port) {
                const e = self.handles.get(port);
                if (!e || e.type !== "serial") return WAPI_ERR_BADF;
                e.opened = false;
                try { e.reader && e.reader.cancel(); } catch (_) {}
                try { e.writer && e.writer.close(); } catch (_) {}
                try { e.port.close(); } catch (_) {}
                return WAPI_OK;
            },
            write(port, dataPtr, dataLen) {
                const e = self.handles.get(port);
                if (!e || e.type !== "serial" || !e.opened) return WAPI_ERR_BADF;
                self._refreshViews();
                const data = self._u8.slice(dataPtr, dataPtr + Number(dataLen));
                e.writer.write(data).catch(() => {});
                return WAPI_OK;
            },
            read(port, bufPtr, bufLen, bytesReadPtr) {
                const e = self.handles.get(port);
                if (!e || e.type !== "serial" || !e.opened) return WAPI_ERR_BADF;
                if (e.rxQueue.length === 0) { self._writeU64(bytesReadPtr, 0); return WAPI_ERR_AGAIN; }
                self._refreshViews();
                const cap = Number(bufLen);
                let written = 0;
                while (e.rxQueue.length > 0 && written < cap) {
                    const front = e.rxQueue[0];
                    const n = Math.min(front.byteLength, cap - written);
                    self._u8.set(front.subarray(0, n), bufPtr + written);
                    written += n;
                    if (n === front.byteLength) e.rxQueue.shift();
                    else e.rxQueue[0] = front.subarray(n);
                }
                self._writeU64(bytesReadPtr, written);
                return WAPI_OK;
            },
            set_signals(port, dtr, rts) {
                const e = self.handles.get(port);
                if (!e || e.type !== "serial" || !e.opened) return WAPI_ERR_BADF;
                e.port.setSignals({ dataTerminalReady: !!dtr, requestToSend: !!rts }).catch(() => {});
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_capture (screen capture - stub)
        // -------------------------------------------------------------------
        // Screen capture via getDisplayMedia.
        const wapi_capture = {
            request(sourceType, capturePtr) {
                if (typeof navigator === "undefined" || !navigator.mediaDevices ||
                    !navigator.mediaDevices.getDisplayMedia) return WAPI_ERR_NOTSUP;
                if (!self._capStates) self._capStates = new Map();
                return asyncOp(self._capStates, "req:" + capturePtr, (entry) => {
                    const opts = { video: true, audio: false };
                    if (sourceType === 2) opts.preferCurrentTab = true;
                    navigator.mediaDevices.getDisplayMedia(opts).then(
                        (stream) => {
                            const video = document.createElement("video");
                            video.srcObject = stream;
                            video.muted = true;
                            video.playsInline = true;
                            video.play().catch(() => {});
                            entry.state = "ready";
                            entry.stream = stream;
                            entry.video = video;
                            entry.canvas = document.createElement("canvas");
                            entry.ctx = entry.canvas.getContext("2d");
                        },
                        (err) => { entry.state = err && err.name === "NotAllowedError" ? "canceled" : "failed"; }
                    );
                }, (st) => {
                    const h = self.handles.insert({
                        type: "screen_cap",
                        stream: st.stream, video: st.video, canvas: st.canvas, ctx: st.ctx,
                    });
                    self._writeI32(capturePtr, h);
                    return WAPI_OK;
                });
            },
            get_frame(capture, bufPtr, bufLen, frameInfoPtr) {
                const e = self.handles.get(capture);
                if (!e || e.type !== "screen_cap") return WAPI_ERR_BADF;
                const v = e.video;
                if (!v.videoWidth || !v.videoHeight) return WAPI_ERR_AGAIN;
                const w = v.videoWidth, h = v.videoHeight;
                if (e.canvas.width !== w) e.canvas.width = w;
                if (e.canvas.height !== h) e.canvas.height = h;
                e.ctx.drawImage(v, 0, 0, w, h);
                const img = e.ctx.getImageData(0, 0, w, h);
                self._refreshViews();
                self._dv.setUint32(frameInfoPtr + 0, w, true);
                self._dv.setUint32(frameInfoPtr + 4, h, true);
                self._dv.setUint32(frameInfoPtr + 8, 0, true);
                self._dv.setUint32(frameInfoPtr + 12, 0, true);
                self._dv.setBigUint64(frameInfoPtr + 16, BigInt(Math.round(performance.now() * 1e6)), true);
                if (Number(bufLen) < img.data.length) return WAPI_ERR_OVERFLOW;
                self._u8.set(img.data, bufPtr);
                return WAPI_OK;
            },
            stop(capture) {
                const e = self.handles.get(capture);
                if (!e || e.type !== "screen_cap") return WAPI_ERR_BADF;
                try { e.stream.getTracks().forEach(t => t.stop()); } catch (_) {}
                self.handles.remove(capture);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_contacts (contact picker - stub)
        // -------------------------------------------------------------------
        // Contact Picker API. Serializes results into the caller's buffer
        // as a compact text form:  "name\tphone\temail\turl\n" per contact.
        // That's a shim decision; a stricter struct-based encoding would
        // need a specified wire format.
        const wapi_contacts = {
            pick(propsMask, multiple, resultsBufPtr, resultsBufLen) {
                if (typeof navigator === "undefined" || !navigator.contacts ||
                    !navigator.contacts.select) return WAPI_ERR_NOTSUP;
                if (!self._contactsStates) self._contactsStates = new Map();
                return asyncOp(self._contactsStates, "pick:" + resultsBufPtr, (entry) => {
                    const props = [];
                    if (propsMask & 0x01) props.push("name");
                    if (propsMask & 0x02) props.push("email");
                    if (propsMask & 0x04) props.push("tel");
                    if (propsMask & 0x08) props.push("address");
                    if (propsMask & 0x10) props.push("icon");
                    if (props.length === 0) props.push("name", "email", "tel");
                    navigator.contacts.select(props, { multiple: !!multiple }).then(
                        (contacts) => {
                            entry.state = "ready";
                            entry.text = contacts.map(c => [
                                (c.name || [""])[0],
                                (c.tel || [""])[0],
                                (c.email || [""])[0],
                                (c.address || [""])[0],
                            ].join("\t")).join("\n");
                        },
                        (err) => { entry.state = err && err.name === "AbortError" ? "canceled" : "failed"; }
                    );
                }, (st) => {
                    return self._writeString(resultsBufPtr, Number(resultsBufLen), st.text);
                });
            },
            icon_read(_iconHandle, _bufPtr, _bufLen, _outLenPtr) { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_barcode (barcode detection - stub)
        // -------------------------------------------------------------------
        // BarcodeDetector (Chromium). detect() takes raw RGBA and converts to
        // ImageData then runs the detector.
        // Result layout (32 bytes each): format u32, value_len u32, value_ptr i64, x/y/w/h f32.
        const bcFormat = { qr_code:0, ean_13:1, ean_8:2, code_128:3, code_39:4, upc_a:5, upc_e:6, data_matrix:7, pdf417:8, aztec:9 };
        const writeBarcodeResults = (results, maxResults, resultsBufPtr) => {
            self._refreshViews();
            const n = Math.min(results.length, maxResults);
            for (let i = 0; i < n; i++) {
                const r = results[i];
                const base = resultsBufPtr + i * 32;
                self._dv.setUint32(base + 0, bcFormat[r.format] ?? 0, true);
                // Encode the value into host-allocated memory and leave a pointer.
                const encoded = new TextEncoder().encode(r.rawValue || "");
                const vptr = self._hostAlloc(encoded.length + 1, 1);
                self._u8.set(encoded, vptr);
                self._u8[vptr + encoded.length] = 0;
                self._dv.setUint32(base + 4, encoded.length, true);
                self._dv.setBigUint64(base + 8, BigInt(vptr), true);
                const box = r.boundingBox || { x:0, y:0, width:0, height:0 };
                self._dv.setFloat32(base + 16, box.x, true);
                self._dv.setFloat32(base + 20, box.y, true);
                self._dv.setFloat32(base + 24, box.width, true);
                self._dv.setFloat32(base + 28, box.height, true);
            }
            return n;
        };
        const wapi_barcode = {
            detect(imageDataPtr, width, height, resultsBufPtr, maxResults) {
                if (typeof BarcodeDetector === "undefined") return WAPI_ERR_NOTSUP;
                if (!self._barcodeStates) self._barcodeStates = new Map();
                const key = "det:" + resultsBufPtr;
                if (!self._barcodeStates.has(key)) {
                    const entry = { state: "pending" };
                    self._barcodeStates.set(key, entry);
                    self._refreshViews();
                    const px = self._u8.slice(imageDataPtr, imageDataPtr + width * height * 4);
                    const img = new ImageData(new Uint8ClampedArray(px.buffer), width, height);
                    const canvas = document.createElement("canvas");
                    canvas.width = width; canvas.height = height;
                    canvas.getContext("2d").putImageData(img, 0, 0);
                    const detector = new BarcodeDetector();
                    detector.detect(canvas).then(
                        (r) => { entry.state = "ready"; entry.results = r; },
                        () => { entry.state = "failed"; }
                    );
                }
                const st = self._barcodeStates.get(key);
                if (st.state === "pending") return WAPI_ERR_AGAIN;
                self._barcodeStates.delete(key);
                if (st.state !== "ready") return WAPI_ERR_IO;
                return writeBarcodeResults(st.results, maxResults, resultsBufPtr);
            },
            detect_from_camera(cameraHandle, resultsBufPtr, maxResults) {
                if (typeof BarcodeDetector === "undefined") return WAPI_ERR_NOTSUP;
                const cam = self.handles.get(cameraHandle);
                if (!cam || cam.type !== "camera") return WAPI_ERR_BADF;
                if (!cam.video.videoWidth) return WAPI_ERR_AGAIN;
                if (!self._barcodeStates) self._barcodeStates = new Map();
                const key = "detcam:" + cameraHandle + ":" + resultsBufPtr;
                if (!self._barcodeStates.has(key)) {
                    const entry = { state: "pending" };
                    self._barcodeStates.set(key, entry);
                    const detector = new BarcodeDetector();
                    detector.detect(cam.video).then(
                        (r) => { entry.state = "ready"; entry.results = r; },
                        () => { entry.state = "failed"; }
                    );
                }
                const st = self._barcodeStates.get(key);
                if (st.state === "pending") return WAPI_ERR_AGAIN;
                self._barcodeStates.delete(key);
                if (st.state !== "ready") return WAPI_ERR_IO;
                return writeBarcodeResults(st.results, maxResults, resultsBufPtr);
            },
        };

        // -------------------------------------------------------------------
        // wapi_nfc (NFC - stub)
        // -------------------------------------------------------------------
        // Web NFC (NDEFReader, Android Chrome only). We stash a single
        // reader and push scan results into the event queue as IO completions
        // flagged with bit 0x40000.
        const wapi_nfc = {
            scan_start() {
                if (typeof NDEFReader === "undefined") return WAPI_ERR_NOTSUP;
                if (self._nfcReader) return WAPI_OK;
                try {
                    const reader = new NDEFReader();
                    self._nfcReader = reader;
                    reader.scan().then(() => {
                        reader.onreading = (ev) => {
                            for (const rec of ev.message.records) {
                                const text = rec.data ? new TextDecoder().decode(rec.data) : "";
                                // Stash in a scan queue the app can drain via IO events
                                if (!self._nfcInbox) self._nfcInbox = [];
                                self._nfcInbox.push({ type: rec.recordType, text });
                                self._eventQueue.push({
                                    type: WAPI_EVENT_IO_COMPLETION,
                                    userData: BigInt(self._nfcInbox.length - 1),
                                    result: 0, flags: 0x40000,
                                });
                            }
                        };
                    }).catch(() => { self._nfcReader = null; });
                    return WAPI_OK;
                } catch (_) { return WAPI_ERR_IO; }
            },
            scan_stop() {
                self._nfcReader = null;
                self._nfcInbox = [];
                return WAPI_OK;
            },
            write(recordsPtr, recordCount, tagPtr) {
                if (typeof NDEFReader === "undefined") return WAPI_ERR_NOTSUP;
                // NDEFReader write from a static message; we expect one TEXT record at recordsPtr.
                // Record: record_type u32, payload_len u32, payload_ptr i64, _pad (16 bytes total).
                try {
                    const writer = new NDEFReader();
                    const records = [];
                    self._refreshViews();
                    for (let i = 0; i < recordCount; i++) {
                        const base = recordsPtr + i * 32;
                        const rt = self._dv.getUint32(base + 0, true);
                        const plen = self._dv.getUint32(base + 4, true);
                        const pptr = Number(self._dv.getBigUint64(base + 8, true));
                        const bytes = self._u8.slice(pptr, pptr + plen);
                        const text = new TextDecoder().decode(bytes);
                        records.push({ recordType: ["text","url","mime","absolute-url","empty","unknown"][rt] || "text", data: text });
                    }
                    writer.write({ records }).catch(() => {});
                    return WAPI_OK;
                } catch (_) { return WAPI_ERR_IO; }
            },
            make_read_only(_tag) { return WAPI_ERR_NOTSUP; },
        };

        // wapi_dnd / wapi_share / wapi_clipboard collapsed into wapi_transfer above.
        // POINTED reads still go through the IO_OP_TRANSFER_READ handler, which
        // pulls from self._dropItems populated by the canvas drop listener.

        // -------------------------------------------------------------------
        // wapi_window (OS window management - stub)
        // -------------------------------------------------------------------
        const wapi_window = {
            set_title(surface, titlePtr, titleLen) {
                if (typeof document === "undefined") return WAPI_ERR_NOTSUP;
                document.title = self._readString(titlePtr, Number(titleLen));
                return WAPI_OK;
            },
            get_size_logical(surface, widthPtr, heightPtr) {
                const s = self._surfaces.get(surface);
                if (!s || !s.canvas) return WAPI_ERR_BADF;
                const rect = s.canvas.getBoundingClientRect();
                self._writeU32(widthPtr,  Math.round(rect.width));
                self._writeU32(heightPtr, Math.round(rect.height));
                return WAPI_OK;
            },
            set_fullscreen(surface, fullscreen) {
                const s = self._surfaces.get(surface);
                if (!s || !s.canvas) return WAPI_ERR_BADF;
                if (fullscreen) {
                    if (s.canvas.requestFullscreen) s.canvas.requestFullscreen().catch(() => {});
                } else {
                    if (document.exitFullscreen) document.exitFullscreen().catch(() => {});
                }
                return WAPI_OK;
            },
            set_visible(surface, visible) {
                const s = self._surfaces.get(surface);
                if (!s || !s.canvas) return WAPI_ERR_BADF;
                s.canvas.style.display = visible ? "" : "none";
                return WAPI_OK;
            },
            minimize(_surface) { return WAPI_ERR_NOTSUP; }, // no web API
            maximize(_surface) { return WAPI_ERR_NOTSUP; },
            restore(_surface)  { return WAPI_ERR_NOTSUP; },
        };

        // -------------------------------------------------------------------
        // wapi_display (display enumeration - stub)
        // -------------------------------------------------------------------
        // The Window Management API (Screen.getScreenDetails) is gated behind
        // a permission prompt. Without it we can only surface the current
        // screen via window.screen. Default: one display, no prompt.
        // wapi_display_info_t = 56 bytes. Fields (per header):
        //   rect (i32 x,y,w,h) = 16 bytes
        //   dpi_x f32 (4), dpi_y f32 (4)
        //   scale f32 (4), refresh_hz f32 (4)
        //   color_depth u32 (4), hdr u32 (4)
        //   primary u32 (4), _pad (4)
        //   subpixel_layout u32 (4), _pad (4)
        const wapi_display = {
            display_count() {
                return (typeof window !== "undefined" && window.screen) ? 1 : 0;
            },
            display_get_info(index, infoPtr) {
                if (index !== 0 || typeof window === "undefined" || !window.screen) return WAPI_ERR_OVERFLOW;
                const s = window.screen;
                const dpr = window.devicePixelRatio || 1;
                self._refreshViews();
                const dv = self._dv;
                dv.setInt32(infoPtr +  0, 0, true); // x
                dv.setInt32(infoPtr +  4, 0, true); // y
                dv.setInt32(infoPtr +  8, s.width, true);
                dv.setInt32(infoPtr + 12, s.height, true);
                dv.setFloat32(infoPtr + 16, 96 * dpr, true);
                dv.setFloat32(infoPtr + 20, 96 * dpr, true);
                dv.setFloat32(infoPtr + 24, dpr, true);
                dv.setFloat32(infoPtr + 28, 60, true); // refresh_hz (unknown; default 60)
                dv.setUint32(infoPtr + 32, (s.colorDepth || 24), true);
                dv.setUint32(infoPtr + 36, 0, true); // hdr unknown
                dv.setUint32(infoPtr + 40, 1, true); // primary
                dv.setUint32(infoPtr + 44, 0, true);
                dv.setUint32(infoPtr + 48, 0, true); // subpixel_layout UNKNOWN
                dv.setUint32(infoPtr + 52, 0, true);
                return WAPI_OK;
            },
            display_get_subpixels(index, subpixelsPtr, maxCount, countPtr) {
                self._writeU32(countPtr, 0);
                return WAPI_OK;
            },
            display_get_usable_bounds(index, xPtr, yPtr, wPtr, hPtr) {
                if (index !== 0 || typeof window === "undefined" || !window.screen) return WAPI_ERR_OVERFLOW;
                const s = window.screen;
                self._writeI32(xPtr, (s.availLeft | 0) || 0);
                self._writeI32(yPtr, (s.availTop | 0) || 0);
                self._writeI32(wPtr, s.availWidth || s.width);
                self._writeI32(hPtr, s.availHeight || s.height);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_dialog (native file/message/color/font dialogs - stub)
        // -------------------------------------------------------------------
        // Dialogs: file pickers route through showOpenFilePicker/showSaveFilePicker
        // (Chromium) when available, otherwise <input type="file">. All are
        // async → polling. Message boxes use blocking window.confirm/alert
        // where it's safe; color picker uses <input type="color"> async.
        //
        // Dialog state is keyed per call site by the caller's result-buffer
        // pointer, so re-calling after AGAIN returns the pending result
        // deterministically.
        if (!self._dialogStates) self._dialogStates = new Map();
        const dlgTake = (key) => {
            const st = self._dialogStates.get(key);
            if (st && st.state !== "pending") self._dialogStates.delete(key);
            return st;
        };
        const readFilters = (filtersPtr, filterCount) => {
            const out = [];
            for (let i = 0; i < filterCount; i++) {
                const base = filtersPtr + i * 32;
                const name = self._readStringView(base + 0)  || "";
                const pat  = self._readStringView(base + 16) || "";
                const exts = pat.split(";").map(s => s.replace(/^\*\.?/, "").trim()).filter(Boolean);
                out.push({ description: name, accept: { "*/*": exts.map(e => "." + e) } });
            }
            return out;
        };
        const wapi_dialog = {
            // open_file: (filters*, count, default_path_sv*, flags, buf*, buf_len, result_len*)
            open_file(filtersPtr, filterCount, defPathSvPtr, flags, bufPtr, bufLen, resultLenPtr) {
                const key = "openfile:" + bufPtr;
                const st = dlgTake(key);
                if (st) {
                    if (st.state === "ready") {
                        const written = self._writeString(bufPtr, Number(bufLen), st.text);
                        self._writeU64(resultLenPtr, written);
                        return WAPI_OK;
                    }
                    if (st.state === "canceled") return WAPI_ERR_CANCELED;
                    if (st.state === "failed")   return WAPI_ERR_IO;
                }
                if (!self._dialogStates.has(key)) {
                    const multi = (flags & 0x0001) !== 0;
                    const entry = { state: "pending" };
                    self._dialogStates.set(key, entry);
                    const finish = (paths) => {
                        // Consecutive NUL-terminated strings, double-NUL at end.
                        const parts = paths.map(p => p + "\0").join("") + "\0";
                        entry.state = "ready";
                        entry.text  = parts;
                    };
                    if (typeof window !== "undefined" && window.showOpenFilePicker) {
                        window.showOpenFilePicker({
                            multiple: multi,
                            types: filterCount > 0 ? readFilters(filtersPtr, filterCount) : undefined,
                        }).then(
                            (handles) => finish(handles.map(h => h.name)),
                            (err) => { entry.state = err && err.name === "AbortError" ? "canceled" : "failed"; }
                        );
                    } else {
                        const input = document.createElement("input");
                        input.type = "file";
                        if (multi) input.multiple = true;
                        input.style.display = "none";
                        input.oncancel = () => { entry.state = "canceled"; input.remove(); };
                        input.onchange = () => {
                            const names = Array.from(input.files || []).map(f => f.name);
                            if (names.length === 0) entry.state = "canceled";
                            else finish(names);
                            input.remove();
                        };
                        document.body.appendChild(input);
                        input.click();
                    }
                }
                return WAPI_ERR_AGAIN;
            },
            // save_file: (filters*, count, default_path_sv*, buf*, buf_len, result_len*)
            save_file(filtersPtr, filterCount, defPathSvPtr, bufPtr, bufLen, resultLenPtr) {
                const key = "savefile:" + bufPtr;
                const st = dlgTake(key);
                if (st) {
                    if (st.state === "ready") {
                        const written = self._writeString(bufPtr, Number(bufLen), st.text);
                        self._writeU64(resultLenPtr, written);
                        return WAPI_OK;
                    }
                    if (st.state === "canceled") return WAPI_ERR_CANCELED;
                    return WAPI_ERR_IO;
                }
                if (typeof window !== "undefined" && window.showSaveFilePicker && !self._dialogStates.has(key)) {
                    const entry = { state: "pending" };
                    self._dialogStates.set(key, entry);
                    const def = defPathSvPtr ? self._readStringView(defPathSvPtr) : null;
                    window.showSaveFilePicker({
                        suggestedName: def || undefined,
                        types: filterCount > 0 ? readFilters(filtersPtr, filterCount) : undefined,
                    }).then(
                        (h) => { entry.state = "ready"; entry.text = h.name; },
                        (err) => { entry.state = err && err.name === "AbortError" ? "canceled" : "failed"; }
                    );
                    return WAPI_ERR_AGAIN;
                }
                return WAPI_ERR_NOTSUP;
            },
            open_folder(defPathSvPtr, bufPtr, bufLen, resultLenPtr) {
                const key = "openfolder:" + bufPtr;
                const st = dlgTake(key);
                if (st) {
                    if (st.state === "ready") {
                        const written = self._writeString(bufPtr, Number(bufLen), st.text);
                        self._writeU64(resultLenPtr, written);
                        return WAPI_OK;
                    }
                    if (st.state === "canceled") return WAPI_ERR_CANCELED;
                    return WAPI_ERR_IO;
                }
                if (typeof window !== "undefined" && window.showDirectoryPicker && !self._dialogStates.has(key)) {
                    const entry = { state: "pending" };
                    self._dialogStates.set(key, entry);
                    window.showDirectoryPicker().then(
                        (h) => { entry.state = "ready"; entry.text = h.name; },
                        (err) => { entry.state = err && err.name === "AbortError" ? "canceled" : "failed"; }
                    );
                    return WAPI_ERR_AGAIN;
                }
                return WAPI_ERR_NOTSUP;
            },
            // message_box: (type, title_sv*, msg_sv*, buttons, result*)
            message_box(type, titleSvPtr, msgSvPtr, buttons, resultPtr) {
                const title = self._readStringView(titleSvPtr) || "";
                const msg   = self._readStringView(msgSvPtr)   || "";
                const body  = title ? (title + "\n\n" + msg) : msg;
                let result = 0;
                try {
                    if (buttons === 0) { // OK
                        window.alert(body); result = 0;
                    } else { // OK_CANCEL / YES_NO / YES_NO_CANCEL — fall back to confirm
                        const ok = window.confirm(body);
                        if (buttons === 1) result = ok ? 0 /*OK*/    : 1 /*CANCEL*/;
                        else                result = ok ? 2 /*YES*/  : 3 /*NO*/;
                    }
                } catch (_) { return WAPI_ERR_IO; }
                self._writeI32(resultPtr, result);
                return WAPI_OK;
            },
            simple_message_box(titleSvPtr, msgSvPtr) {
                const title = self._readStringView(titleSvPtr) || "";
                const msg   = self._readStringView(msgSvPtr)   || "";
                try { window.alert(title ? (title + "\n\n" + msg) : msg); return WAPI_OK; }
                catch (_) { return WAPI_ERR_IO; }
            },
            pick_color(titleSvPtr, initialRgba, flags, resultRgbaPtr) {
                const key = "pickcolor:" + resultRgbaPtr;
                const st = dlgTake(key);
                if (st) {
                    if (st.state === "ready") {
                        self._writeU32(resultRgbaPtr, st.rgba);
                        return WAPI_OK;
                    }
                    if (st.state === "canceled") return WAPI_ERR_CANCELED;
                    return WAPI_ERR_IO;
                }
                if (!self._dialogStates.has(key)) {
                    const entry = { state: "pending" };
                    self._dialogStates.set(key, entry);
                    if (typeof EyeDropper !== "undefined") {
                        // No native picker in the web platform — use color input.
                    }
                    const input = document.createElement("input");
                    input.type = "color";
                    const r = (initialRgba >>> 24) & 0xFF;
                    const g = (initialRgba >>> 16) & 0xFF;
                    const b = (initialRgba >>>  8) & 0xFF;
                    input.value = "#" + [r, g, b].map(v => v.toString(16).padStart(2, "0")).join("");
                    input.style.position = "absolute"; input.style.opacity = "0";
                    let settled = false;
                    input.addEventListener("change", () => {
                        settled = true;
                        const hex = input.value.replace("#", "");
                        const nr = parseInt(hex.substr(0,2), 16);
                        const ng = parseInt(hex.substr(2,2), 16);
                        const nb = parseInt(hex.substr(4,2), 16);
                        entry.state = "ready";
                        entry.rgba = ((nr << 24) | (ng << 16) | (nb << 8) | 0xFF) >>> 0;
                        input.remove();
                    });
                    input.addEventListener("cancel", () => {
                        settled = true;
                        entry.state = "canceled";
                        input.remove();
                    });
                    // Fallback detection: blur after click without change = cancel.
                    setTimeout(() => {
                        if (!settled && !self._dialogStates.has(key)) return;
                    }, 30_000);
                    document.body.appendChild(input);
                    input.click();
                }
                return WAPI_ERR_AGAIN;
            },
            pick_font(_titleSvPtr, _ioPtr, _nameBufPtr, _nameCap) {
                // No web font picker exists.
                return WAPI_ERR_NOTSUP;
            },
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
            // CSS accent-color exposes the user's system accent via computed style.
            // Fall back to #0078D4 if unavailable.
            theme_get_accent_color(rgbaPtr) {
                let r = 0x00, g = 0x78, b = 0xD4, a = 0xFF;
                if (typeof document !== "undefined" && typeof getComputedStyle === "function") {
                    try {
                        const el = document.createElement("div");
                        el.style.accentColor = "AccentColor";
                        el.style.color = "AccentColor";
                        el.style.position = "absolute"; el.style.visibility = "hidden";
                        document.body.appendChild(el);
                        const c = getComputedStyle(el).color; // "rgb(r, g, b)" or "rgba(...)"
                        el.remove();
                        const m = c.match(/rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)/);
                        if (m) { r = +m[1]; g = +m[2]; b = +m[3]; }
                    } catch (_) {}
                }
                self._writeU32(rgbaPtr, ((r<<24)|(g<<16)|(b<<8)|a) >>> 0);
                return WAPI_OK;
            },
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
            // wapi_sysinfo_t = 128 bytes. Layout in include/wapi/wapi_sysinfo.h.
            sysinfo_get(infoPtr) {
                self._refreshViews();
                for (let i = 0; i < 128; i++) self._u8[infoPtr + i] = 0;
                const dv = self._dv;
                dv.setUint32(infoPtr + 0, 6, true); // platform = BROWSER
                let cls = 1; // DESKTOP
                if (typeof navigator !== "undefined") {
                    const ua = navigator.userAgent || "";
                    if (/iPad|Tablet/i.test(ua)) cls = 3;
                    else if (/Mobile|Android|iPhone/i.test(ua)) cls = 4;
                }
                dv.setUint32(infoPtr + 4, cls, true);
                // cpu_count / physical_cpu_count @ 28, 32
                const hwc = (typeof navigator !== "undefined" && navigator.hardwareConcurrency) || 1;
                dv.setUint32(infoPtr + 28, hwc, true);
                dv.setUint32(infoPtr + 32, hwc, true);
                // ram_mb @ 36
                const ram = (typeof navigator !== "undefined" && navigator.deviceMemory)
                    ? Math.round(navigator.deviceMemory * 1024) : 0;
                dv.setUint32(infoPtr + 36, ram, true);
                // cpu_arch = WASM32 @ 40
                dv.setUint32(infoPtr + 40, 6, true);
                // cache_line_size @ 44
                dv.setUint32(infoPtr + 44, 64, true);
                // page_size @ 48
                dv.setUint32(infoPtr + 48, 65536, true);
                // dark_mode @ 52
                const dark = (typeof matchMedia === "function" &&
                              matchMedia("(prefers-color-scheme: dark)").matches) ? 1 : 0;
                dv.setUint32(infoPtr + 52, dark, true);
                // accent_color_rgba @ 56 (unknown in browsers without computed style probing)
                dv.setUint32(infoPtr + 56, 0, true);
                // sandbox = BROWSER @ 60
                dv.setUint32(infoPtr + 60, 2, true);
                // is_remote @ 64 (browsers can't detect cleanly)
                dv.setUint32(infoPtr + 64, 0, true);
                return WAPI_OK;
            },
            // (i32 key_sv_ptr, i32 buf, i64 buf_len, i32 val_len_ptr) -> i32
            host_get(keySvPtr, bufPtr, bufLen, valLenPtr) {
                const key = self._readStringView(keySvPtr);
                const ua = (typeof navigator !== "undefined" && navigator.userAgent) || "";
                const hostInfo = {
                    "os.family":       "browser",
                    "runtime.name":    "wapi-browser",
                    "runtime.version": "0.1.0",
                    "device.form":     "desktop",
                    "browser.engine":  ua.includes("Chrome") ? "chromium"
                                     : ua.includes("Firefox") ? "gecko"
                                     : ua.includes("Safari") ? "webkit"
                                     : "unknown",
                };
                if (!key || !(key in hostInfo)) return WAPI_ERR_NOENT;
                const val = hostInfo[key];
                const written = self._writeString(bufPtr, Number(bufLen), val);
                self._writeU64(valLenPtr, written);
                return WAPI_OK;
            },
        };

        // -------------------------------------------------------------------
        // wapi_user (identity - browsers have no portable user identity)
        // -------------------------------------------------------------------
        const wapi_user = {
            provider() { return 0; /* WAPI_USER_PROVIDER_UNKNOWN */ },
            get_field(_field, _bufPtr, _bufLen, _outLenPtr) {
                return WAPI_ERR_NOENT;
            },
            avatar(_maxEdge, _outW, _outH, _bufPtr, _bufLen) {
                return WAPI_ERR_NOENT;
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
        // Threading. Real wasm threads need SharedArrayBuffer + Cross-Origin
        // Isolation, which isn't our current deployment model — create()
        // stays NOTSUP. But because a single-threaded caller can't observe
        // contention, mutex/rwlock/sem/cond become legal no-ops that always
        // "succeed." TLS becomes a plain per-instance map. This lets ports
        // of stdlib code (pthreads, std::mutex) link and run correctly.
        const tlsMap = new Map();
        let nextTlsSlot = 1;
        const lockMap = new Map();
        let nextLockId = 1;
        const wapi_thread = {
            create(_descPtr, _threadPtr) { return WAPI_ERR_NOTSUP; },
            join(_thread, _exitCodePtr) { return WAPI_ERR_NOTSUP; },
            detach(_thread) { return WAPI_ERR_NOTSUP; },
            get_state(_thread, statePtr) {
                self._writeU32(statePtr, 2 /* COMPLETE */);
                return WAPI_OK;
            },
            current_id() { return 1n; },
            get_id(_thread) { return 0n; },
            set_qos(_qos) { return WAPI_OK; },

            tls_create(_destructor, slotPtr) {
                const slot = nextTlsSlot++;
                tlsMap.set(slot, 0);
                self._writeI32(slotPtr, slot);
                return WAPI_OK;
            },
            tls_destroy(slot) { tlsMap.delete(slot); return WAPI_OK; },
            tls_set(slot, value) {
                if (!tlsMap.has(slot)) return WAPI_ERR_BADF;
                tlsMap.set(slot, value);
                return WAPI_OK;
            },
            tls_get(slot) { return tlsMap.get(slot) || 0; },

            // Single-thread: every lock succeeds trivially.
            mutex_create(mutexPtr) {
                const id = nextLockId++;
                lockMap.set(id, { kind: "mutex", held: false });
                self._writeI32(mutexPtr, id);
                return WAPI_OK;
            },
            mutex_destroy(mutex)    { lockMap.delete(mutex); return WAPI_OK; },
            mutex_lock(mutex)       { const l = lockMap.get(mutex); if (!l) return WAPI_ERR_BADF; l.held = true; return WAPI_OK; },
            mutex_try_lock(mutex)   { const l = lockMap.get(mutex); if (!l) return WAPI_ERR_BADF; if (l.held) return WAPI_ERR_BUSY; l.held = true; return WAPI_OK; },
            mutex_unlock(mutex)     { const l = lockMap.get(mutex); if (!l) return WAPI_ERR_BADF; l.held = false; return WAPI_OK; },

            rwlock_create(ptr) {
                const id = nextLockId++;
                lockMap.set(id, { kind: "rw", readers: 0, writer: false });
                self._writeI32(ptr, id);
                return WAPI_OK;
            },
            rwlock_destroy(rw) { lockMap.delete(rw); return WAPI_OK; },
            rwlock_read_lock(rw) { const l = lockMap.get(rw); if (!l) return WAPI_ERR_BADF; l.readers++; return WAPI_OK; },
            rwlock_try_read_lock(rw) { return this.rwlock_read_lock(rw); },
            rwlock_write_lock(rw) { const l = lockMap.get(rw); if (!l) return WAPI_ERR_BADF; l.writer = true; return WAPI_OK; },
            rwlock_try_write_lock(rw) { return this.rwlock_write_lock(rw); },
            rwlock_unlock(rw) { const l = lockMap.get(rw); if (!l) return WAPI_ERR_BADF; if (l.writer) l.writer = false; else if (l.readers > 0) l.readers--; return WAPI_OK; },

            sem_create(initialValue, semPtr) {
                const id = nextLockId++;
                lockMap.set(id, { kind: "sem", value: initialValue });
                self._writeI32(semPtr, id);
                return WAPI_OK;
            },
            sem_destroy(sem) { lockMap.delete(sem); return WAPI_OK; },
            sem_wait(sem) {
                const l = lockMap.get(sem); if (!l) return WAPI_ERR_BADF;
                if (l.value > 0) { l.value--; return WAPI_OK; }
                return WAPI_ERR_DEADLK; // Can't block in single-thread; caller must treat this as "would block forever".
            },
            sem_try_wait(sem) {
                const l = lockMap.get(sem); if (!l) return WAPI_ERR_BADF;
                if (l.value > 0) { l.value--; return WAPI_OK; }
                return WAPI_ERR_AGAIN;
            },
            sem_wait_timeout(sem, _timeoutNs) { return this.sem_try_wait(sem); },
            sem_signal(sem) { const l = lockMap.get(sem); if (!l) return WAPI_ERR_BADF; l.value++; return WAPI_OK; },
            sem_get_value(sem) { const l = lockMap.get(sem); return l ? l.value : 0; },

            cond_create(condPtr) {
                const id = nextLockId++;
                lockMap.set(id, { kind: "cond" });
                self._writeI32(condPtr, id);
                return WAPI_OK;
            },
            cond_destroy(cond) { lockMap.delete(cond); return WAPI_OK; },
            cond_wait(_cond, _mutex) { return WAPI_ERR_DEADLK; }, // would block forever
            cond_wait_timeout(_cond, _mutex, _timeoutNs) { return WAPI_ERR_TIMEDOUT; },
            cond_signal(_cond) { return WAPI_OK; },
            cond_broadcast(_cond) { return WAPI_OK; },

            barrier_create(count, barrierPtr) {
                const id = nextLockId++;
                lockMap.set(id, { kind: "barrier", count, arrived: 0 });
                self._writeI32(barrierPtr, id);
                return WAPI_OK;
            },
            barrier_destroy(barrier) { lockMap.delete(barrier); return WAPI_OK; },
            barrier_wait(barrier) {
                // Single-thread: the caller is the one arrival; if count > 1
                // we'd wait forever, so signal deadlock.
                const l = lockMap.get(barrier); if (!l) return WAPI_ERR_BADF;
                if (l.count <= 1) return WAPI_OK;
                return WAPI_ERR_DEADLK;
            },

            // call_once: run initFunc exactly once. onceFlagPtr points to a
            // 4-byte flag in linear memory; we set it to 1 after invocation.
            call_once(onceFlagPtr, initFunc) {
                self._refreshViews();
                const flag = self._readU32(onceFlagPtr);
                if (flag === 1) return WAPI_OK;
                const table = self.instance.exports.__indirect_function_table;
                if (!table) return WAPI_ERR_NOTSUP;
                const fn = table.get(initFunc);
                if (!fn) return WAPI_ERR_INVAL;
                try { fn(); self._writeU32(onceFlagPtr, 1); return WAPI_OK; }
                catch (_) { return WAPI_ERR_IO; }
            },
        };

        // -------------------------------------------------------------------
        // wapi_eyedrop (screen color picker - stub)
        // -------------------------------------------------------------------
        // EyeDropper API (Chromium-only). Async-to-sync polling.
        const wapi_eyedrop = {
            eyedropper_pick(rgbaPtr) {
                if (typeof EyeDropper === "undefined") return WAPI_ERR_NOTSUP;
                const key = "eyedrop:" + rgbaPtr;
                const st = self._dialogStates && self._dialogStates.get(key);
                if (st && st.state !== "pending") {
                    self._dialogStates.delete(key);
                    if (st.state === "ready") {
                        self._writeU32(rgbaPtr, st.rgba);
                        return WAPI_OK;
                    }
                    if (st.state === "canceled") return WAPI_ERR_CANCELED;
                    return WAPI_ERR_IO;
                }
                if (!self._dialogStates) self._dialogStates = new Map();
                if (!self._dialogStates.has(key)) {
                    const entry = { state: "pending" };
                    self._dialogStates.set(key, entry);
                    const ed = new EyeDropper();
                    ed.open().then(
                        (r) => {
                            const c = r.sRGBHex.replace("#", "");
                            const rr = parseInt(c.substr(0,2), 16);
                            const gg = parseInt(c.substr(2,2), 16);
                            const bb = parseInt(c.substr(4,2), 16);
                            entry.state = "ready";
                            entry.rgba = ((rr<<24)|(gg<<16)|(bb<<8)|0xFF) >>> 0;
                        },
                        (err) => {
                            entry.state = err && err.name === "AbortError" ? "canceled" : "failed";
                        }
                    );
                }
                return WAPI_ERR_AGAIN;
            },
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

        // -------------------------------------------------------------------
        // Built-in IO opcode handlers for the new spec-defined namespaces.
        //
        // These are pre-registered on the instance's opcode dispatch table
        // so modules compiled against the opcode-based headers can use the
        // capability surface without any host integration glue. All handlers
        // accept the decoded op descriptor and post a completion.
        // -------------------------------------------------------------------
        if (!self._opcodeHandlers) self._opcodeHandlers = new Map();

        // WAPI_IO_OP_CAP_REQUEST (0x01) — universal capability gate.
        // Every capability, even ambient ones, flows through here. The
        // browser host auto-grants anything the user-agent actually
        // supports; real prompts land on the individual-op handlers
        // (notify.show fires the Notification request, geo.get_position
        // fires geolocation, etc.). We complete synchronously with the
        // current grant state inlined.
        self._opcodeHandlers.set(0x01 >>> 0, ({ addr, len, resultPtr, userData, self: inst }) => {
            inst._refreshViews();
            const name = inst._readString(addr, Number(len));
            const granted = inst._detectCapabilities().includes(name);
            // GRANTED=0, DENIED=1, PROMPT=2
            const state = granted ? 0 : 1;
            if (resultPtr) inst._writeU32(resultPtr, state);
            const payload = new Uint8Array(4);
            new DataView(payload.buffer).setUint32(0, state, true);
            inst._eventQueue.push({
                type: WAPI_EVENT_IO_COMPLETION,
                userData, result: granted ? WAPI_OK : WAPI_ERR_ACCES,
                flags: 0x0004 /* WAPI_IO_CQE_F_INLINE */,
                inlinePayload: payload,
            });
        });

        // WAPI_IO_OP_CRYPTO_HASH (0x2C0) — SubtleCrypto digest,
        // result inlined as up to 64 bytes in the event payload.
        self._opcodeHandlers.set(WAPI_IO_OP_CRYPTO_HASH >>> 0, ({ flags, addr, len, userData, self: inst }) => {
            const algos = { 0: "SHA-256", 1: "SHA-384", 2: "SHA-512", 3: "SHA-1" };
            const algoName = algos[flags];
            if (!algoName || typeof crypto === "undefined" || !crypto.subtle) {
                inst._eventQueue.push({
                    type: WAPI_EVENT_IO_COMPLETION,
                    userData, result: WAPI_ERR_NOTSUP, flags: 0,
                });
                return;
            }
            inst._refreshViews();
            const data = inst._u8.slice(addr, addr + Number(len));
            crypto.subtle.digest(algoName, data).then(
                (buf) => {
                    const digest = new Uint8Array(buf);
                    inst._eventQueue.push({
                        type: WAPI_EVENT_IO_COMPLETION,
                        userData, result: digest.length,
                        flags: 0x0004 /* WAPI_IO_CQE_F_INLINE */,
                        inlinePayload: digest,
                    });
                },
                () => {
                    inst._eventQueue.push({
                        type: WAPI_EVENT_IO_COMPLETION,
                        userData, result: WAPI_ERR_IO, flags: 0,
                    });
                }
            );
        });

        // WAPI_IO_OP_TRANSFER_OFFER (0x310) — unified clipboard/DnD/share offer.
        //   fd        = seat (must be 0 = WAPI_SEAT_DEFAULT)
        //   flags     = mode bitmask (WAPI_TRANSFER_LATENT|POINTED|ROUTED)
        //   addr/len  = wapi_transfer_offer_t* (48 bytes)
        //
        // wapi_transfer_offer_t layout:
        //    0:u64 items_ptr  8:u32 item_count  12:u32 allowed_actions
        //   16:u64 title.data 24:u64 title.length
        //   32:i32 preview    36:u32 _reserved  40:u64 _reserved2
        //
        // wapi_transfer_item_t (32B):
        //    0:u64 mime.data  8:u64 mime.length  16:u64 data  24:u64 data_len
        self._opcodeHandlers.set(WAPI_IO_OP_TRANSFER_OFFER >>> 0,
        ({ fd, flags, addr, userData, self: inst }) => {
            const seat = fd | 0;
            const mode = flags >>> 0;
            const complete = (result) => inst._eventQueue.push({
                type: WAPI_EVENT_IO_COMPLETION,
                userData, result: result | 0, flags: 0,
            });

            if (seat !== 0) { complete(WAPI_ERR_INVAL); return; }
            inst._refreshViews();
            const dv = inst._dv;

            const itemsPtr   = Number(dv.getBigUint64(addr +  0, true));
            const itemCount  = dv.getUint32(addr +  8, true);
            // const allowedActions = dv.getUint32(addr + 12, true);
            const titleData  = Number(dv.getBigUint64(addr + 16, true));
            const titleLen   = Number(dv.getBigUint64(addr + 24, true));
            const titleStr   = (titleData && titleLen)
                ? new TextDecoder().decode(inst._u8.subarray(titleData, titleData + titleLen))
                : "";

            // Decode all items.
            const items = [];
            for (let i = 0; i < itemCount; i++) {
                const it = itemsPtr + i * 32;
                const mimeData = Number(dv.getBigUint64(it +  0, true));
                const mimeLen  = Number(dv.getBigUint64(it +  8, true));
                const dataAddr = Number(dv.getBigUint64(it + 16, true));
                const dataLen  = Number(dv.getBigUint64(it + 24, true));
                const mime = new TextDecoder().decode(
                    inst._u8.subarray(mimeData, mimeData + mimeLen));
                const bytes = inst._u8.slice(dataAddr, dataAddr + dataLen);
                items.push({ mime, bytes });
            }

            // Helpers
            const blobs = items.map(({ mime, bytes }) =>
                new Blob([bytes], { type: mime || "application/octet-stream" }));

            const doLatent = () => {
                if (typeof navigator === "undefined" ||
                    !navigator.clipboard || !navigator.clipboard.write) {
                    return Promise.resolve(WAPI_ERR_NOTSUP);
                }
                if (typeof ClipboardItem === "undefined") {
                    return Promise.resolve(WAPI_ERR_NOTSUP);
                }
                const dict = {};
                items.forEach(({ mime }, idx) => { dict[mime] = blobs[idx]; });
                return navigator.clipboard.write([new ClipboardItem(dict)]).then(
                    () => (WAPI_TRANSFER_LATENT << 8) | 1, /* COPY */
                    () => WAPI_ERR_ACCES,
                );
            };

            const doRouted = () => {
                if (typeof navigator === "undefined" || !navigator.share) {
                    return Promise.resolve(WAPI_ERR_NOTSUP);
                }
                const data = {};
                if (titleStr) data.title = titleStr;
                // Map well-known MIME types into Web Share fields.
                for (const { mime, bytes } of items) {
                    if (mime === "text/plain" && !data.text) {
                        data.text = new TextDecoder().decode(bytes);
                    } else if (mime === "text/uri-list" && !data.url) {
                        data.url = new TextDecoder().decode(bytes).split("\n")[0].trim();
                    }
                }
                // Files (when navigator.canShare supports them).
                if (navigator.canShare) {
                    const files = items
                        .filter(({ mime }) =>
                            mime !== "text/plain" && mime !== "text/uri-list")
                        .map(({ mime, bytes }) =>
                            new File([bytes], "shared",
                                     { type: mime || "application/octet-stream" }));
                    if (files.length && navigator.canShare({ files })) {
                        data.files = files;
                    }
                }
                return navigator.share(data).then(
                    () => (WAPI_TRANSFER_ROUTED << 8) | 1,
                    (err) => (err && err.name === "AbortError")
                        ? WAPI_ERR_CANCELED : WAPI_ERR_IO,
                );
            };

            const doPointed = () => {
                // Browser drag-source initiation requires a real DOM dragstart;
                // we can't open a session from an arbitrary IO call. Apps that
                // need POINTED must use the dragstart-based flow exposed
                // separately through the canvas listeners.
                return Promise.resolve(WAPI_ERR_NOTSUP);
            };

            // Run the requested modes; report the first non-NOTSUP completion.
            const tasks = [];
            if (mode & WAPI_TRANSFER_LATENT)  tasks.push(doLatent());
            if (mode & WAPI_TRANSFER_POINTED) tasks.push(doPointed());
            if (mode & WAPI_TRANSFER_ROUTED)  tasks.push(doRouted());

            if (tasks.length === 0) { complete(WAPI_ERR_INVAL); return; }

            Promise.all(tasks).then((results) => {
                for (const r of results) {
                    if (r !== WAPI_ERR_NOTSUP) { complete(r); return; }
                }
                complete(WAPI_ERR_NOTSUP);
            });
        });

        // WAPI_IO_OP_TRANSFER_READ (0x311) — read a format from a seat's
        // transfer pool for a given mode.
        //   fd       = seat
        //   flags    = mode (single, not bitmask)
        //   addr/len = mime stringview
        //   addr2/len2 = output buffer
        self._opcodeHandlers.set(WAPI_IO_OP_TRANSFER_READ >>> 0,
        ({ fd, flags, addr, len, addr2, len2, userData, self: inst }) => {
            const seat = fd | 0;
            const mode = flags >>> 0;
            const complete = (result) => inst._eventQueue.push({
                type: WAPI_EVENT_IO_COMPLETION,
                userData, result: result | 0, flags: 0,
            });

            if (seat !== 0) { complete(WAPI_ERR_INVAL); return; }
            const mime = inst._readString(addr, Number(len));

            if (mode === WAPI_TRANSFER_POINTED) {
                // Items stashed by the canvas drop listener.
                const stash = inst._dropItems || [];
                const item = stash.find(it => it.mime === mime);
                if (!item) { complete(WAPI_ERR_NOENT); return; }
                inst._refreshViews();
                const n = Math.min(item.bytes.length, Number(len2));
                inst._u8.set(item.bytes.subarray(0, n), addr2);
                complete(item.bytes.length);
                return;
            }

            if (mode !== WAPI_TRANSFER_LATENT) {
                complete(WAPI_ERR_NOTSUP);
                return;
            }

            // LATENT — read from system clipboard.
            if (typeof navigator === "undefined" || !navigator.clipboard) {
                complete(WAPI_ERR_NOTSUP);
                return;
            }
            if (mime === "text/plain" && navigator.clipboard.readText) {
                navigator.clipboard.readText().then(
                    (text) => {
                        inst._refreshViews();
                        const bytes = new TextEncoder().encode(text);
                        const n = Math.min(bytes.length, Number(len2));
                        inst._u8.set(bytes.subarray(0, n), addr2);
                        complete(bytes.length);
                    },
                    () => complete(WAPI_ERR_ACCES),
                );
                return;
            }
            if (navigator.clipboard.read) {
                navigator.clipboard.read().then(async (clipItems) => {
                    for (const ci of clipItems) {
                        if (!ci.types.includes(mime)) continue;
                        const blob = await ci.getType(mime);
                        const buf  = new Uint8Array(await blob.arrayBuffer());
                        inst._refreshViews();
                        const n = Math.min(buf.length, Number(len2));
                        inst._u8.set(buf.subarray(0, n), addr2);
                        complete(buf.length);
                        return;
                    }
                    complete(WAPI_ERR_NOENT);
                }, () => complete(WAPI_ERR_ACCES));
                return;
            }
            complete(WAPI_ERR_NOTSUP);
        });

        // Helper factories for completion emission. The kick-off handlers
        // below all share the same shape: do async work, then push an
        // IO_COMPLETION event. These helpers keep each registration a
        // one-liner while still honouring the inline-payload convention.
        const H = self._opcodeHandlers;
        const complete = (inst, userData, result, flags = 0, inlinePayload = null) => {
            inst._eventQueue.push({
                type: WAPI_EVENT_IO_COMPLETION,
                userData, result: result | 0,
                flags: flags >>> 0,
                inlinePayload,
            });
        };
        const completeInline = (inst, userData, payload, result = WAPI_OK) => {
            complete(inst, userData, result, 0x0004 /* INLINE */, payload);
        };
        const abortErr = (err) =>
            (err && (err.name === "NotAllowedError" || err.name === "NotFoundError" || err.name === "AbortError"))
                ? WAPI_ERR_CANCELED : WAPI_ERR_IO;

        // ====================================================================
        // BLUETOOTH (namespace 0x0000 methods 0x0A0-0x0A6)
        // ====================================================================

        H.set(WAPI_IO_OP_BT_DEVICE_REQUEST, ({ addr, flags2, resultPtr, userData, self: inst }) => {
            if (!navigator.bluetooth) return complete(inst, userData, WAPI_ERR_NOTSUP);
            inst._refreshViews();
            const filters = [];
            for (let i = 0; i < flags2; i++) {
                const base = addr + i * 32;
                const uuid = inst._readStringView(base + 0);
                const name = inst._readStringView(base + 16);
                const f = {};
                if (uuid) f.services = [uuid.toLowerCase()];
                if (name) f.namePrefix = name;
                filters.push(f);
            }
            const opts = flags2 > 0 ? { filters } : { acceptAllDevices: true };
            navigator.bluetooth.requestDevice(opts).then(
                (d) => {
                    const h = inst.handles.insert({ type: "bt_device", device: d });
                    inst._writeI32(resultPtr, h);
                    complete(inst, userData, WAPI_OK);
                },
                (err) => complete(inst, userData, abortErr(err))
            );
        });

        H.set(WAPI_IO_OP_BT_CONNECT, ({ fd, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "bt_device") return complete(inst, userData, WAPI_ERR_BADF);
            e.device.gatt.connect().then(
                (srv) => { e.server = srv; complete(inst, userData, WAPI_OK); },
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        H.set(WAPI_IO_OP_BT_SERVICE_GET, ({ fd, addr, len, resultPtr, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || !e.server) return complete(inst, userData, WAPI_ERR_BADF);
            const uuid = inst._readString(addr, Number(len));
            e.server.getPrimaryService(uuid.toLowerCase()).then(
                (s) => {
                    const h = inst.handles.insert({ type: "bt_service", service: s });
                    inst._writeI32(resultPtr, h);
                    complete(inst, userData, WAPI_OK);
                },
                () => complete(inst, userData, WAPI_ERR_NOENT)
            );
        });

        H.set(WAPI_IO_OP_BT_CHARACTERISTIC_GET, ({ fd, addr, len, resultPtr, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "bt_service") return complete(inst, userData, WAPI_ERR_BADF);
            const uuid = inst._readString(addr, Number(len));
            e.service.getCharacteristic(uuid.toLowerCase()).then(
                (c) => {
                    const h = inst.handles.insert({ type: "bt_char", ch: c });
                    inst._writeI32(resultPtr, h);
                    complete(inst, userData, WAPI_OK);
                },
                () => complete(inst, userData, WAPI_ERR_NOENT)
            );
        });

        H.set(WAPI_IO_OP_BT_VALUE_READ, ({ fd, addr, len, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "bt_char") return complete(inst, userData, WAPI_ERR_BADF);
            e.ch.readValue().then(
                (dv) => {
                    inst._refreshViews();
                    const bytes = new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength);
                    const n = Math.min(Number(len), bytes.length);
                    inst._u8.set(bytes.subarray(0, n), addr);
                    complete(inst, userData, bytes.length);
                },
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        H.set(WAPI_IO_OP_BT_VALUE_WRITE, ({ fd, addr, len, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "bt_char") return complete(inst, userData, WAPI_ERR_BADF);
            inst._refreshViews();
            const data = inst._u8.slice(addr, addr + Number(len));
            e.ch.writeValue(data).then(
                () => complete(inst, userData, WAPI_OK),
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        H.set(WAPI_IO_OP_BT_NOTIFICATIONS_START, ({ fd, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "bt_char") return complete(inst, userData, WAPI_ERR_BADF);
            e.ch.startNotifications().then(
                (c) => {
                    e.notify = (ev) => {
                        const dv = ev.target.value;
                        complete(inst, userData, dv.byteLength, 0x0001 /* MORE */);
                    };
                    c.addEventListener("characteristicvaluechanged", e.notify);
                    complete(inst, userData, WAPI_OK, 0x0001 /* MORE */);
                },
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        // ====================================================================
        // USB (0x0B0-0x0B5)
        // ====================================================================

        H.set(WAPI_IO_OP_USB_DEVICE_REQUEST, ({ addr, flags2, resultPtr, userData, self: inst }) => {
            if (!navigator.usb) return complete(inst, userData, WAPI_ERR_NOTSUP);
            inst._refreshViews();
            const filters = [];
            for (let i = 0; i < flags2; i++) {
                const base = addr + i * 8;
                const vid = inst._dv.getUint16(base + 0, true);
                const pid = inst._dv.getUint16(base + 2, true);
                const cls = inst._u8[base + 4];
                const f = {};
                if (vid) f.vendorId = vid;
                if (pid) f.productId = pid;
                if (cls) f.classCode = cls;
                filters.push(f);
            }
            navigator.usb.requestDevice({ filters: filters.length ? filters : [{}] }).then(
                (d) => {
                    const h = inst.handles.insert({ type: "usb", device: d });
                    inst._writeI32(resultPtr, h);
                    complete(inst, userData, WAPI_OK);
                },
                (err) => complete(inst, userData, abortErr(err))
            );
        });

        H.set(WAPI_IO_OP_USB_OPEN, ({ fd, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "usb") return complete(inst, userData, WAPI_ERR_BADF);
            e.device.open().then(
                () => { e.opened = true; complete(inst, userData, WAPI_OK); },
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        H.set(WAPI_IO_OP_USB_INTERFACE_CLAIM, ({ fd, flags, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "usb") return complete(inst, userData, WAPI_ERR_BADF);
            e.device.claimInterface(flags).then(
                () => complete(inst, userData, WAPI_OK),
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        H.set(WAPI_IO_OP_USB_TRANSFER_IN, ({ fd, flags, addr, len, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "usb") return complete(inst, userData, WAPI_ERR_BADF);
            e.device.transferIn(flags, Number(len)).then(
                (r) => {
                    inst._refreshViews();
                    const bytes = new Uint8Array(r.data.buffer, r.data.byteOffset, r.data.byteLength);
                    const n = Math.min(Number(len), bytes.length);
                    inst._u8.set(bytes.subarray(0, n), addr);
                    complete(inst, userData, bytes.length);
                },
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        H.set(WAPI_IO_OP_USB_TRANSFER_OUT, ({ fd, flags, addr, len, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "usb") return complete(inst, userData, WAPI_ERR_BADF);
            inst._refreshViews();
            const data = inst._u8.slice(addr, addr + Number(len));
            e.device.transferOut(flags, data).then(
                (r) => complete(inst, userData, r.bytesWritten),
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        H.set(WAPI_IO_OP_USB_CONTROL_TRANSFER, ({ fd, offset, addr, len, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "usb") return complete(inst, userData, WAPI_ERR_BADF);
            const rt = Number(offset & 0xFFn);
            const rq = Number((offset >> 8n) & 0xFFn);
            const vl = Number((offset >> 16n) & 0xFFFFn);
            const ix = Number((offset >> 32n) & 0xFFFFn);
            const isIn = (rt & 0x80) !== 0;
            const recipients = ["device", "interface", "endpoint", "other"];
            const types = ["standard", "class", "vendor"];
            const setup = {
                requestType: types[(rt >> 5) & 0x3] || "vendor",
                recipient: recipients[rt & 0xF] || "device",
                request: rq, value: vl, index: ix,
            };
            const done = (r) => complete(inst, userData, r | 0);
            if (isIn) {
                e.device.controlTransferIn(setup, Number(len)).then(
                    (r) => {
                        inst._refreshViews();
                        const bytes = new Uint8Array(r.data.buffer, r.data.byteOffset, r.data.byteLength);
                        const n = Math.min(Number(len), bytes.length);
                        inst._u8.set(bytes.subarray(0, n), addr);
                        done(bytes.length);
                    },
                    () => done(WAPI_ERR_IO)
                );
            } else {
                inst._refreshViews();
                const data = inst._u8.slice(addr, addr + Number(len));
                e.device.controlTransferOut(setup, data).then(
                    (r) => done(r.bytesWritten),
                    () => done(WAPI_ERR_IO)
                );
            }
        });

        // ====================================================================
        // SERIAL (0x080-0x083)
        // ====================================================================

        H.set(WAPI_IO_OP_SERIAL_PORT_REQUEST, ({ resultPtr, userData, self: inst }) => {
            if (!navigator.serial) return complete(inst, userData, WAPI_ERR_NOTSUP);
            navigator.serial.requestPort({}).then(
                (p) => {
                    const h = inst.handles.insert({
                        type: "serial", port: p,
                        reader: null, writer: null, rxQueue: [], opened: false,
                    });
                    inst._writeI32(resultPtr, h);
                    complete(inst, userData, WAPI_OK);
                },
                (err) => complete(inst, userData, abortErr(err))
            );
        });

        H.set(WAPI_IO_OP_SERIAL_OPEN, ({ fd, addr, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "serial") return complete(inst, userData, WAPI_ERR_BADF);
            inst._refreshViews();
            const baud   = inst._dv.getUint32(addr + 0, true) || 9600;
            const dbits  = inst._u8[addr + 4] || 8;
            const sbits  = inst._u8[addr + 5] || 1;
            const parity = inst._u8[addr + 6];
            const flow   = inst._u8[addr + 7];
            const opts = {
                baudRate: baud, dataBits: dbits, stopBits: sbits,
                parity: ["none", "even", "odd"][parity] || "none",
                flowControl: flow ? "hardware" : "none",
            };
            e.port.open(opts).then(
                async () => {
                    e.opened = true;
                    e.reader = e.port.readable.getReader();
                    e.writer = e.port.writable.getWriter();
                    (async () => {
                        try {
                            while (e.opened) {
                                const { value, done } = await e.reader.read();
                                if (done) break;
                                if (value) e.rxQueue.push(value);
                            }
                        } catch (_) {}
                    })();
                    complete(inst, userData, WAPI_OK);
                },
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        H.set(WAPI_IO_OP_SERIAL_READ, ({ fd, addr, len, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "serial" || !e.opened) return complete(inst, userData, WAPI_ERR_BADF);
            if (e.rxQueue.length === 0) return complete(inst, userData, WAPI_ERR_AGAIN);
            inst._refreshViews();
            const cap = Number(len);
            let written = 0;
            while (e.rxQueue.length > 0 && written < cap) {
                const front = e.rxQueue[0];
                const n = Math.min(front.byteLength, cap - written);
                inst._u8.set(front.subarray(0, n), addr + written);
                written += n;
                if (n === front.byteLength) e.rxQueue.shift();
                else e.rxQueue[0] = front.subarray(n);
            }
            complete(inst, userData, written);
        });

        H.set(WAPI_IO_OP_SERIAL_WRITE, ({ fd, addr, len, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "serial" || !e.opened) return complete(inst, userData, WAPI_ERR_BADF);
            inst._refreshViews();
            const data = inst._u8.slice(addr, addr + Number(len));
            e.writer.write(data).then(
                () => complete(inst, userData, Number(len)),
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        // ====================================================================
        // MIDI (0x092-0x093) — send/recv. Port acquisition goes through ROLE_REQUEST.
        // ====================================================================

        H.set(WAPI_IO_OP_MIDI_SEND, ({ fd, addr, len, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "midi" || e.kind !== 1) return complete(inst, userData, WAPI_ERR_BADF);
            inst._refreshViews();
            const data = inst._u8.slice(addr, addr + Number(len));
            try { e.port.send(data); complete(inst, userData, Number(len)); }
            catch (_) { complete(inst, userData, WAPI_ERR_IO); }
        });

        H.set(WAPI_IO_OP_MIDI_RECV, ({ fd, addr, len, resultPtr, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "midi" || e.kind !== 0) return complete(inst, userData, WAPI_ERR_BADF);
            if (e.inbox.length === 0) return complete(inst, userData, WAPI_ERR_AGAIN);
            const msg = e.inbox.shift();
            inst._refreshViews();
            const copy = Math.min(Number(len), msg.data.length);
            inst._u8.set(msg.data.subarray(0, copy), addr);
            if (resultPtr) inst._dv.setBigUint64(resultPtr, msg.ts, true);
            complete(inst, userData, msg.data.length);
        });

        // ====================================================================
        // NFC (0x0C0-0x0C1)
        // ====================================================================

        H.set(WAPI_IO_OP_NFC_SCAN_START, ({ userData, self: inst }) => {
            if (typeof NDEFReader === "undefined") return complete(inst, userData, WAPI_ERR_NOTSUP);
            try {
                const r = new NDEFReader();
                inst._nfcReader = r;
                r.scan().then(() => {
                    r.onreading = (ev) => {
                        for (const rec of ev.message.records) {
                            const text = rec.data ? new TextDecoder().decode(rec.data) : "";
                            const payload = new TextEncoder().encode(text).subarray(0, 96);
                            complete(inst, userData, payload.length, 0x0004 | 0x0001 /* INLINE|MORE */, payload);
                        }
                    };
                }).catch(() => complete(inst, userData, WAPI_ERR_ACCES));
                complete(inst, userData, WAPI_OK, 0x0001 /* MORE */);
            } catch (_) { complete(inst, userData, WAPI_ERR_IO); }
        });

        H.set(WAPI_IO_OP_NFC_WRITE, ({ addr, flags, userData, self: inst }) => {
            if (typeof NDEFReader === "undefined") return complete(inst, userData, WAPI_ERR_NOTSUP);
            try {
                const w = new NDEFReader();
                const records = [];
                inst._refreshViews();
                for (let i = 0; i < flags; i++) {
                    const base = addr + i * 32;
                    const rt   = inst._dv.getUint32(base + 0, true);
                    const plen = inst._dv.getUint32(base + 4, true);
                    const pptr = Number(inst._dv.getBigUint64(base + 8, true));
                    const bytes = inst._u8.slice(pptr, pptr + plen);
                    const text = new TextDecoder().decode(bytes);
                    records.push({ recordType: ["text","url","mime","absolute-url","empty","unknown"][rt] || "text", data: text });
                }
                w.write({ records }).then(
                    () => complete(inst, userData, WAPI_OK),
                    () => complete(inst, userData, WAPI_ERR_IO)
                );
            } catch (_) { complete(inst, userData, WAPI_ERR_IO); }
        });

        // CAMERA: acquisition via ROLE_REQUEST; frame read is bounded-local.

        // ====================================================================
        // CAPTURE (0x130)
        // ====================================================================

        H.set(WAPI_IO_OP_CAPTURE_REQUEST, ({ flags, resultPtr, userData, self: inst }) => {
            if (!navigator.mediaDevices || !navigator.mediaDevices.getDisplayMedia) {
                return complete(inst, userData, WAPI_ERR_NOTSUP);
            }
            const opts = { video: true, audio: false };
            if (flags === 2) opts.preferCurrentTab = true;
            navigator.mediaDevices.getDisplayMedia(opts).then(
                (stream) => {
                    const video = document.createElement("video");
                    video.srcObject = stream; video.muted = true; video.playsInline = true;
                    video.play().catch(() => {});
                    const canvas = document.createElement("canvas");
                    const h = inst.handles.insert({
                        type: "screen_cap", stream, video, canvas,
                        ctx: canvas.getContext("2d"),
                    });
                    inst._writeI32(resultPtr, h);
                    complete(inst, userData, WAPI_OK);
                },
                (err) => complete(inst, userData, abortErr(err))
            );
        });

        // ====================================================================
        // XR (0x200-0x204 + 0x203 is_supported + 0x204 create_ref_space)
        // ====================================================================

        H.set(WAPI_IO_OP_XR_SESSION_REQUEST, ({ flags, resultPtr, userData, self: inst }) => {
            if (!navigator.xr) return complete(inst, userData, WAPI_ERR_NOTSUP);
            const modes = ["immersive-vr", "immersive-ar", "inline"];
            const mode = modes[flags] || "inline";
            navigator.xr.requestSession(mode).then(
                (s) => {
                    const h = inst.handles.insert({ type: "xr_session", session: s });
                    inst._writeI32(resultPtr, h);
                    complete(inst, userData, WAPI_OK);
                },
                (err) => complete(inst, userData, abortErr(err))
            );
        });

        H.set(0x203 /* XR_IS_SUPPORTED */, ({ flags, userData, self: inst }) => {
            if (!navigator.xr) return completeInline(inst, userData, new Uint8Array([0,0,0,0]), 0);
            const modes = ["immersive-vr", "immersive-ar", "inline"];
            const mode = modes[flags];
            if (!mode) return completeInline(inst, userData, new Uint8Array([0,0,0,0]), 0);
            navigator.xr.isSessionSupported(mode).then(
                (ok) => completeInline(inst, userData, new Uint8Array([ok ? 1 : 0, 0, 0, 0]), ok ? 1 : 0),
                () => completeInline(inst, userData, new Uint8Array([0,0,0,0]), 0)
            );
        });

        H.set(0x204 /* XR_REF_SPACE_CREATE */, ({ fd, flags, resultPtr, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "xr_session") return complete(inst, userData, WAPI_ERR_BADF);
            const names = ["local", "local-floor", "bounded-floor", "unbounded", "viewer"];
            const name = names[flags] || "local";
            e.session.requestReferenceSpace(name).then(
                (s) => {
                    const h = inst.handles.insert({ type: "xr_space", space: s });
                    inst._writeI32(resultPtr, h);
                    complete(inst, userData, WAPI_OK);
                },
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        // ====================================================================
        // DIALOG (0x180-0x185)
        // ====================================================================

        const readDialogFilters = (inst, filtersPtr, filterCount) => {
            const out = [];
            for (let i = 0; i < filterCount; i++) {
                const base = filtersPtr + i * 32;
                const name = inst._readStringView(base + 0) || "";
                const pat  = inst._readStringView(base + 16) || "";
                const exts = pat.split(";").map(s => s.replace(/^\*\.?/, "").trim()).filter(Boolean);
                out.push({ description: name, accept: { "*/*": exts.map(e => "." + e) } });
            }
            return out;
        };

        H.set(WAPI_IO_OP_DIALOG_FILE_OPEN, ({ flags, flags2, offset, addr2, len2, userData, self: inst }) => {
            const filtersPtr = Number(offset);
            const multi = (flags & 0x0001) !== 0;
            const finish = (paths) => {
                const text = paths.map(p => p + "\0").join("") + "\0";
                inst._refreshViews();
                const written = inst._writeString(addr2, Number(len2), text);
                complete(inst, userData, written);
            };
            if (window.showOpenFilePicker) {
                window.showOpenFilePicker({
                    multiple: multi,
                    types: flags2 > 0 ? readDialogFilters(inst, filtersPtr, flags2) : undefined,
                }).then(
                    (hs) => finish(hs.map(h => h.name)),
                    (err) => complete(inst, userData, abortErr(err))
                );
                return;
            }
            const input = document.createElement("input");
            input.type = "file"; if (multi) input.multiple = true;
            input.style.display = "none";
            input.oncancel = () => { complete(inst, userData, WAPI_ERR_CANCELED); input.remove(); };
            input.onchange = () => {
                const names = Array.from(input.files || []).map(f => f.name);
                input.remove();
                if (names.length === 0) complete(inst, userData, WAPI_ERR_CANCELED);
                else finish(names);
            };
            document.body.appendChild(input);
            input.click();
        });

        H.set(WAPI_IO_OP_DIALOG_FILE_SAVE, ({ flags2, offset, addr, len, addr2, len2, userData, self: inst }) => {
            if (!window.showSaveFilePicker) return complete(inst, userData, WAPI_ERR_NOTSUP);
            const filtersPtr = Number(offset);
            const defPath = addr && len ? inst._readString(addr, Number(len)) : undefined;
            window.showSaveFilePicker({
                suggestedName: defPath || undefined,
                types: flags2 > 0 ? readDialogFilters(inst, filtersPtr, flags2) : undefined,
            }).then(
                (h) => {
                    const written = inst._writeString(addr2, Number(len2), h.name);
                    complete(inst, userData, written);
                },
                (err) => complete(inst, userData, abortErr(err))
            );
        });

        H.set(WAPI_IO_OP_DIALOG_FOLDER_OPEN, ({ addr, len, addr2, len2, userData, self: inst }) => {
            if (!window.showDirectoryPicker) return complete(inst, userData, WAPI_ERR_NOTSUP);
            window.showDirectoryPicker().then(
                (h) => {
                    const written = inst._writeString(addr2, Number(len2), h.name);
                    complete(inst, userData, written);
                },
                (err) => complete(inst, userData, abortErr(err))
            );
        });

        H.set(WAPI_IO_OP_DIALOG_MESSAGEBOX, ({ flags, flags2, addr, len, addr2, len2, userData, self: inst }) => {
            const title = addr && len ? inst._readString(addr, Number(len)) : "";
            const msg   = addr2 && len2 ? inst._readString(addr2, Number(len2)) : "";
            const body  = title ? title + "\n\n" + msg : msg;
            let result = 0;
            try {
                if (flags2 === 0) { window.alert(body); result = 0; }
                else {
                    const ok = window.confirm(body);
                    if (flags2 === 1) result = ok ? 0 : 1;
                    else              result = ok ? 2 : 3;
                }
            } catch (_) { return complete(inst, userData, WAPI_ERR_IO); }
            const payload = new Uint8Array(4);
            new DataView(payload.buffer).setUint32(0, result, true);
            completeInline(inst, userData, payload, result);
        });

        H.set(WAPI_IO_OP_DIALOG_PICK_COLOR, ({ flags, addr, len, userData, self: inst }) => {
            const input = document.createElement("input");
            input.type = "color";
            const r = (flags >>> 24) & 0xFF, g = (flags >>> 16) & 0xFF, b = (flags >>> 8) & 0xFF;
            input.value = "#" + [r, g, b].map(v => v.toString(16).padStart(2, "0")).join("");
            input.style.position = "absolute"; input.style.opacity = "0";
            input.addEventListener("change", () => {
                const hex = input.value.replace("#", "");
                const nr = parseInt(hex.substr(0,2), 16);
                const ng = parseInt(hex.substr(2,2), 16);
                const nb = parseInt(hex.substr(4,2), 16);
                const rgba = ((nr<<24)|(ng<<16)|(nb<<8)|0xFF) >>> 0;
                const payload = new Uint8Array(4);
                new DataView(payload.buffer).setUint32(0, rgba, true);
                completeInline(inst, userData, payload, rgba);
                input.remove();
            });
            input.addEventListener("cancel", () => { complete(inst, userData, WAPI_ERR_CANCELED); input.remove(); });
            document.body.appendChild(input);
            input.click();
        });

        H.set(WAPI_IO_OP_DIALOG_PICK_FONT, ({ userData, self: inst }) => {
            complete(inst, userData, WAPI_ERR_NOTSUP);
        });

        // ====================================================================
        // PICKERS: authn, bio, pay, contacts, eyedrop (0x190-0x1A3)
        // ====================================================================

        H.set(WAPI_IO_OP_AUTHN_CREDENTIAL_CREATE, ({ addr, len, addr2, len2, userData, self: inst }) => {
            if (!navigator.credentials) return complete(inst, userData, WAPI_ERR_NOTSUP);
            inst._refreshViews();
            const rpId = inst._readString(addr, Number(len));
            const challenge = inst._u8.slice(addr2, addr2 + Number(len2));
            navigator.credentials.create({
                publicKey: {
                    rp: { id: rpId, name: rpId },
                    user: {
                        id: crypto.getRandomValues(new Uint8Array(16)),
                        name: "user", displayName: "user",
                    },
                    challenge,
                    pubKeyCredParams: [{ type: "public-key", alg: -7 }, { type: "public-key", alg: -257 }],
                    timeout: 60000,
                },
            }).then(
                () => complete(inst, userData, WAPI_OK),
                () => complete(inst, userData, WAPI_ERR_ACCES)
            );
        });

        H.set(WAPI_IO_OP_AUTHN_ASSERTION_GET, ({ addr, len, addr2, len2, userData, self: inst }) => {
            if (!navigator.credentials) return complete(inst, userData, WAPI_ERR_NOTSUP);
            inst._refreshViews();
            const rpId = inst._readString(addr, Number(len));
            const challenge = inst._u8.slice(addr2, addr2 + Number(len2));
            navigator.credentials.get({
                publicKey: { challenge, rpId, timeout: 60000 },
            }).then(
                () => complete(inst, userData, WAPI_OK),
                () => complete(inst, userData, WAPI_ERR_ACCES)
            );
        });

        H.set(WAPI_IO_OP_BIO_AUTHENTICATE, ({ userData, self: inst }) => {
            if (typeof PublicKeyCredential === "undefined" || !navigator.credentials) {
                return complete(inst, userData, WAPI_ERR_NOTSUP);
            }
            const challenge = new Uint8Array(32);
            crypto.getRandomValues(challenge);
            navigator.credentials.get({
                publicKey: { challenge, userVerification: "required", timeout: 60000 },
            }).then(
                () => complete(inst, userData, WAPI_OK),
                () => complete(inst, userData, WAPI_ERR_ACCES)
            );
        });

        H.set(WAPI_IO_OP_PAY_PAYMENT_REQUEST, ({ addr2, len2, userData, self: inst }) => {
            if (typeof PaymentRequest === "undefined") return complete(inst, userData, WAPI_ERR_NOTSUP);
            const methods = [{ supportedMethods: "basic-card" }];
            const details = { total: { label: "Total", amount: { currency: "USD", value: "0.00" } } };
            try {
                const pr = new PaymentRequest(methods, details);
                pr.show().then(
                    async (resp) => {
                        const token = JSON.stringify(resp.details || {});
                        await resp.complete("success").catch(() => {});
                        const written = inst._writeString(addr2, Number(len2), token);
                        complete(inst, userData, written);
                    },
                    (err) => complete(inst, userData, abortErr(err))
                );
            } catch (_) { complete(inst, userData, WAPI_ERR_IO); }
        });

        H.set(WAPI_IO_OP_CONTACTS_PICK, ({ flags, flags2, addr, len, userData, self: inst }) => {
            if (!navigator.contacts || !navigator.contacts.select) {
                return complete(inst, userData, WAPI_ERR_NOTSUP);
            }
            const props = [];
            if (flags & 0x01) props.push("name");
            if (flags & 0x02) props.push("email");
            if (flags & 0x04) props.push("tel");
            if (flags & 0x08) props.push("address");
            if (flags & 0x10) props.push("icon");
            if (props.length === 0) props.push("name", "email", "tel");
            navigator.contacts.select(props, { multiple: !!flags2 }).then(
                (contacts) => {
                    const text = contacts.map(c => [
                        (c.name || [""])[0],
                        (c.tel || [""])[0],
                        (c.email || [""])[0],
                        (c.address || [""])[0],
                    ].join("\t")).join("\n");
                    const written = inst._writeString(addr, Number(len), text);
                    complete(inst, userData, contacts.length);
                },
                (err) => complete(inst, userData, abortErr(err))
            );
        });

        H.set(WAPI_IO_OP_EYEDROPPER_PICK, ({ userData, self: inst }) => {
            if (typeof EyeDropper === "undefined") return complete(inst, userData, WAPI_ERR_NOTSUP);
            const ed = new EyeDropper();
            ed.open().then(
                (r) => {
                    const c = r.sRGBHex.replace("#", "");
                    const rr = parseInt(c.substr(0,2), 16);
                    const gg = parseInt(c.substr(2,2), 16);
                    const bb = parseInt(c.substr(4,2), 16);
                    const rgba = ((rr<<24)|(gg<<16)|(bb<<8)|0xFF) >>> 0;
                    const payload = new Uint8Array(4);
                    new DataView(payload.buffer).setUint32(0, rgba, true);
                    completeInline(inst, userData, payload, rgba);
                },
                (err) => complete(inst, userData, abortErr(err))
            );
        });

        // ====================================================================
        // GEOLOCATION (0x210-0x211) — position inlines 48B
        // ====================================================================

        const writeGeoPayload = (coords) => {
            const buf = new Uint8Array(48);
            const dv = new DataView(buf.buffer);
            dv.setFloat64(0, coords.latitude, true);
            dv.setFloat64(8, coords.longitude, true);
            dv.setFloat64(16, coords.altitude == null ? NaN : coords.altitude, true);
            dv.setFloat64(24, coords.accuracy, true);
            dv.setFloat64(32, coords.altitudeAccuracy == null ? NaN : coords.altitudeAccuracy, true);
            dv.setFloat64(40, coords.heading == null ? NaN : coords.heading, true);
            return buf;
        };

        H.set(WAPI_IO_OP_GEO_POSITION_GET, ({ flags, offset, userData, self: inst }) => {
            if (!navigator.geolocation) return complete(inst, userData, WAPI_ERR_NOTSUP);
            navigator.geolocation.getCurrentPosition(
                (pos) => completeInline(inst, userData, writeGeoPayload(pos.coords), WAPI_OK),
                (err) => complete(inst, userData, err.code === 1 ? WAPI_ERR_ACCES : err.code === 3 ? WAPI_ERR_TIMEDOUT : WAPI_ERR_IO),
                {
                    enableHighAccuracy: (flags & 0x0001) !== 0,
                    timeout: Number(offset) > 0 ? Number(offset) : undefined,
                }
            );
        });

        H.set(WAPI_IO_OP_GEO_POSITION_WATCH, ({ flags, resultPtr, userData, self: inst }) => {
            if (!navigator.geolocation) return complete(inst, userData, WAPI_ERR_NOTSUP);
            const watchId = navigator.geolocation.watchPosition(
                (pos) => completeInline(inst, userData, writeGeoPayload(pos.coords), WAPI_OK),
                () => complete(inst, userData, WAPI_ERR_IO, 0x0001 /* MORE */),
                { enableHighAccuracy: (flags & 0x0001) !== 0 }
            );
            const h = inst.handles.insert({ type: "geo_watch", watchId });
            inst._writeI32(resultPtr, h);
            complete(inst, userData, WAPI_OK, 0x0001 /* MORE */);
        });

        // ====================================================================
        // SPEECH (0x120-0x122)
        // ====================================================================

        H.set(WAPI_IO_OP_SPEECH_SPEAK, ({ addr, resultPtr, userData, self: inst }) => {
            if (typeof speechSynthesis === "undefined") return complete(inst, userData, WAPI_ERR_NOTSUP);
            const text = inst._readStringView(addr + 0) || "";
            const lang = inst._readStringView(addr + 16);
            const rate   = inst._dv.getFloat32(addr + 32, true);
            const pitch  = inst._dv.getFloat32(addr + 36, true);
            const volume = inst._dv.getFloat32(addr + 40, true);
            const u = new SpeechSynthesisUtterance(text);
            if (lang) u.lang = lang;
            if (rate > 0) u.rate = rate;
            if (pitch >= 0) u.pitch = pitch;
            if (volume >= 0) u.volume = volume;
            u.onend = () => complete(inst, userData, WAPI_OK);
            u.onerror = () => complete(inst, userData, WAPI_ERR_IO);
            try {
                speechSynthesis.speak(u);
                const h = inst.handles.insert({ type: "tts", utterance: u });
                inst._writeI32(resultPtr, h);
            } catch (_) { complete(inst, userData, WAPI_ERR_IO); }
        });

        // Recognize start/result left as follow-on; webkitSpeechRecognition
        // plumbing is involved and lower priority than the rest of this pass.

        // ====================================================================
        // CRYPTO remaining (encrypt/decrypt/sign/verify/derive/key_*)
        // ====================================================================

        const CRYPTO_HASH_ALGO = { 0: "SHA-256", 1: "SHA-384", 2: "SHA-512", 3: "SHA-1" };
        const CRYPTO_CIPHER = {
            0: { name: "AES-GCM", bits: 128 }, 1: { name: "AES-GCM", bits: 256 },
            2: { name: "AES-CBC", bits: 128 }, 3: { name: "AES-CBC", bits: 256 },
        };

        H.set(WAPI_IO_OP_CRYPTO_HASH_CREATE,
            ({ fd, flags, addr, len, resultPtr, userData, self: inst }) => {
                if (!crypto.subtle) return complete(inst, userData, WAPI_ERR_NOTSUP);
                // flags==0: update; flags==1: finish; fd==0 & !fd: create
                if (!fd) {
                    // Create new ctx
                    const algo = CRYPTO_HASH_ALGO[flags];
                    if (!algo) return complete(inst, userData, WAPI_ERR_INVAL);
                    const h = inst.handles.insert({ type: "hashctx", algo, chunks: [] });
                    inst._writeI32(resultPtr, h);
                    return complete(inst, userData, WAPI_OK);
                }
                const e = inst.handles.get(fd);
                if (!e || e.type !== "hashctx") return complete(inst, userData, WAPI_ERR_BADF);
                if (flags === 0) {
                    inst._refreshViews();
                    e.chunks.push(inst._u8.slice(addr, addr + Number(len)));
                    return complete(inst, userData, WAPI_OK);
                }
                // finish
                const total = e.chunks.reduce((n, c) => n + c.length, 0);
                const all = new Uint8Array(total);
                let o = 0;
                for (const c of e.chunks) { all.set(c, o); o += c.length; }
                crypto.subtle.digest(e.algo, all).then(
                    (buf) => {
                        const digest = new Uint8Array(buf);
                        inst.handles.remove(fd);
                        completeInline(inst, userData, digest, digest.length);
                    },
                    () => complete(inst, userData, WAPI_ERR_IO)
                );
            });

        H.set(WAPI_IO_OP_CRYPTO_ENCRYPT,
            ({ fd, flags, offset, addr, len, addr2, len2, userData, self: inst }) => {
                const spec = CRYPTO_CIPHER[flags];
                const k = inst.handles.get(fd);
                if (!spec || !k || k.type !== "cryptokey") return complete(inst, userData, WAPI_ERR_INVAL);
                const ivPtr = Number(offset & 0xFFFFFFFFn);
                const ivLen = Number((offset >> 32n) & 0xFFFFFFFFn);
                inst._refreshViews();
                const iv = inst._u8.slice(ivPtr, ivPtr + ivLen);
                const pt = inst._u8.slice(addr, addr + Number(len));
                crypto.subtle.encrypt({ name: spec.name, iv }, k.key, pt).then(
                    (buf) => {
                        const ct = new Uint8Array(buf);
                        inst._refreshViews();
                        const n = Math.min(Number(len2), ct.length);
                        inst._u8.set(ct.subarray(0, n), addr2);
                        complete(inst, userData, ct.length);
                    },
                    () => complete(inst, userData, WAPI_ERR_IO)
                );
            });

        H.set(WAPI_IO_OP_CRYPTO_DECRYPT,
            ({ fd, flags, offset, addr, len, addr2, len2, userData, self: inst }) => {
                const spec = CRYPTO_CIPHER[flags];
                const k = inst.handles.get(fd);
                if (!spec || !k || k.type !== "cryptokey") return complete(inst, userData, WAPI_ERR_INVAL);
                const ivPtr = Number(offset & 0xFFFFFFFFn);
                const ivLen = Number((offset >> 32n) & 0xFFFFFFFFn);
                inst._refreshViews();
                const iv = inst._u8.slice(ivPtr, ivPtr + ivLen);
                const ct = inst._u8.slice(addr, addr + Number(len));
                crypto.subtle.decrypt({ name: spec.name, iv }, k.key, ct).then(
                    (buf) => {
                        const pt = new Uint8Array(buf);
                        inst._refreshViews();
                        const n = Math.min(Number(len2), pt.length);
                        inst._u8.set(pt.subarray(0, n), addr2);
                        complete(inst, userData, pt.length);
                    },
                    () => complete(inst, userData, WAPI_ERR_IO)
                );
            });

        H.set(WAPI_IO_OP_CRYPTO_KEY_IMPORT_RAW,
            ({ flags, addr, len, resultPtr, userData, self: inst }) => {
                if (!crypto.subtle) return complete(inst, userData, WAPI_ERR_NOTSUP);
                inst._refreshViews();
                const raw = inst._u8.slice(addr, addr + Number(len));
                const uses = [];
                if (flags & 0x1) uses.push("encrypt");
                if (flags & 0x2) uses.push("decrypt");
                if (flags & 0x4) uses.push("sign");
                if (flags & 0x8) uses.push("verify");
                crypto.subtle.importKey("raw", raw, { name: "AES-GCM" }, false,
                    uses.length ? uses : ["encrypt", "decrypt"]).then(
                    (k) => {
                        const h = inst.handles.insert({ type: "cryptokey", key: k });
                        inst._writeI32(resultPtr, h);
                        complete(inst, userData, WAPI_OK);
                    },
                    () => complete(inst, userData, WAPI_ERR_IO)
                );
            });

        H.set(WAPI_IO_OP_CRYPTO_KEY_GENERATE,
            ({ flags, flags2, resultPtr, userData, self: inst }) => {
                if (!crypto.subtle) return complete(inst, userData, WAPI_ERR_NOTSUP);
                const spec = CRYPTO_CIPHER[flags];
                if (!spec) return complete(inst, userData, WAPI_ERR_INVAL);
                const uses = [];
                if (flags2 & 0x1) uses.push("encrypt");
                if (flags2 & 0x2) uses.push("decrypt");
                const algoDesc = spec.bits ? { name: spec.name, length: spec.bits } : { name: spec.name };
                crypto.subtle.generateKey(algoDesc, true,
                    uses.length ? uses : ["encrypt", "decrypt"]).then(
                    (k) => {
                        const h = inst.handles.insert({ type: "cryptokey", key: k });
                        inst._writeI32(resultPtr, h);
                        complete(inst, userData, WAPI_OK);
                    },
                    () => complete(inst, userData, WAPI_ERR_IO)
                );
            });

        // ====================================================================
        // POWER (0x2E8-0x2EA)
        // ====================================================================

        H.set(WAPI_IO_OP_POWER_INFO_GET, ({ userData, self: inst }) => {
            const payload = new Uint8Array(16);
            const dv = new DataView(payload.buffer);
            if (!navigator.getBattery) {
                dv.setUint32 (0, 0, true);
                dv.setFloat32(4, 1.0, true);
                dv.setFloat32(8, Infinity, true);
                return completeInline(inst, userData, payload, WAPI_OK);
            }
            navigator.getBattery().then(
                (b) => {
                    const src = b.charging
                        ? (b.level >= 0.999 ? 4 : 3)
                        : 1;
                    const secs = b.charging
                        ? (isFinite(b.chargingTime) ? b.chargingTime : Infinity)
                        : (isFinite(b.dischargingTime) ? b.dischargingTime : Infinity);
                    dv.setUint32 (0, src, true);
                    dv.setFloat32(4, b.level, true);
                    dv.setFloat32(8, secs, true);
                    completeInline(inst, userData, payload, WAPI_OK);
                },
                () => {
                    dv.setUint32(0, 0, true);
                    completeInline(inst, userData, payload, WAPI_OK);
                }
            );
        });

        H.set(WAPI_IO_OP_POWER_WAKE_ACQUIRE, ({ flags, resultPtr, userData, self: inst }) => {
            if (!navigator.wakeLock) return complete(inst, userData, WAPI_ERR_NOTSUP);
            navigator.wakeLock.request("screen").then(
                (lock) => {
                    const h = inst.handles.insert({ type: "wakelock", lock });
                    inst._writeI32(resultPtr, h);
                    complete(inst, userData, WAPI_OK);
                },
                () => complete(inst, userData, WAPI_ERR_ACCES)
            );
        });

        H.set(WAPI_IO_OP_POWER_IDLE_START, ({ flags, userData, self: inst }) => {
            if (typeof IdleDetector === "undefined") return complete(inst, userData, WAPI_ERR_NOTSUP);
            IdleDetector.requestPermission().then((perm) => {
                if (perm !== "granted") return complete(inst, userData, WAPI_ERR_ACCES);
                const det = new IdleDetector();
                det.addEventListener("change", () => {
                    const state = det.userState === "idle"
                        ? (det.screenState === "locked" ? 2 : 1)
                        : 0;
                    const payload = new Uint8Array(4);
                    new DataView(payload.buffer).setUint32(0, state, true);
                    completeInline(inst, userData, payload, state);
                });
                det.start({ threshold: Math.max(60_000, flags) }).then(
                    () => { inst._idleDetector = det; complete(inst, userData, WAPI_OK, 0x0001 /* MORE */); },
                    () => complete(inst, userData, WAPI_ERR_IO)
                );
            });
        });

        // SENSOR: acquisition via ROLE_REQUEST (see ROLE_REQUEST handler below).

        const SENSOR_CTORS = {
            0: typeof Accelerometer !== "undefined" ? Accelerometer : null,
            1: typeof Gyroscope !== "undefined" ? Gyroscope : null,
            2: typeof Magnetometer !== "undefined" ? Magnetometer : null,
            3: typeof AmbientLightSensor !== "undefined" ? AmbientLightSensor : null,
            5: typeof GravitySensor !== "undefined" ? GravitySensor : null,
            6: typeof LinearAccelerationSensor !== "undefined" ? LinearAccelerationSensor : null,
        };

        // ====================================================================
        // ROLE_REQUEST / ROLE_REPICK (0x16 / 0x17) — spec §9.10
        //
        // Reads an array of wapi_role_request_t (56B each) from addr,
        // fulfills each role via a Web API, writes the resulting handle
        // and wapi_result_t back into the per-entry out_handle / out_result
        // slots. Completion fires once for the whole batch.
        // ====================================================================

        function role_write_handle(inst, outAddr, handle) {
            if (outAddr) inst._writeI32(Number(outAddr), handle);
        }
        function role_write_result(inst, outAddr, code) {
            if (outAddr) inst._writeI32(Number(outAddr), code);
        }

        function role_fulfill_one(inst, entryAddr) {
            inst._refreshViews();
            const kind       = inst._dv.getUint32(entryAddr +  0, true);
            const flags      = inst._dv.getUint32(entryAddr +  4, true);
            const prefsAddr  = inst._dv.getBigUint64(entryAddr +  8, true);
            const prefsLen   = inst._dv.getUint32(entryAddr + 16, true);
            const outHandle  = inst._dv.getBigUint64(entryAddr + 24, true);
            const outResult  = inst._dv.getBigUint64(entryAddr + 32, true);
            // target_uid[16] at offset 40 — ignored until UID tracking lands.
            void flags;

            switch (kind) {
            case WAPI_ROLE_AUDIO_PLAYBACK: {
                const h = wapi_audio_open_default_endpoint();
                if (h) role_write_handle(inst, outHandle, h);
                role_write_result(inst, outResult, h ? WAPI_OK : WAPI_ERR_NOTCAPABLE);
                return null;
            }
            case WAPI_ROLE_AUDIO_RECORDING: {
                if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
                    role_write_result(inst, outResult, WAPI_ERR_NOTSUP);
                    return null;
                }
                return navigator.mediaDevices.getUserMedia({ audio: true }).then(
                    (stream) => {
                        const h = inst.handles.insert({ type: "audio_recording", stream });
                        role_write_handle(inst, outHandle, h);
                        role_write_result(inst, outResult, WAPI_OK);
                    },
                    () => role_write_result(inst, outResult, WAPI_ERR_ACCES)
                );
            }
            case WAPI_ROLE_CAMERA: {
                if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
                    role_write_result(inst, outResult, WAPI_ERR_NOTSUP);
                    return null;
                }
                let facing = 0, w = 0, h = 0;
                if (Number(prefsAddr) && prefsLen >= 16) {
                    const p = Number(prefsAddr);
                    facing = inst._dv.getUint32(p + 0, true);
                    w      = inst._dv.getInt32 (p + 4, true);
                    h      = inst._dv.getInt32 (p + 8, true);
                }
                return navigator.mediaDevices.getUserMedia({
                    video: {
                        facingMode: facing === 2 ? "environment" : "user",
                        width:  w > 0 ? w : undefined,
                        height: h > 0 ? h : undefined,
                    },
                    audio: false,
                }).then(
                    (stream) => {
                        const video = document.createElement("video");
                        video.srcObject = stream; video.muted = true; video.playsInline = true;
                        video.play().catch(() => {});
                        const canvas = document.createElement("canvas");
                        const handle = inst.handles.insert({
                            type: "camera", stream, video, canvas,
                            ctx: canvas.getContext("2d"),
                        });
                        role_write_handle(inst, outHandle, handle);
                        role_write_result(inst, outResult, WAPI_OK);
                    },
                    () => role_write_result(inst, outResult, WAPI_ERR_ACCES)
                );
            }
            case WAPI_ROLE_MIDI_INPUT:
            case WAPI_ROLE_MIDI_OUTPUT: {
                if (!navigator.requestMIDIAccess) {
                    role_write_result(inst, outResult, WAPI_ERR_NOTSUP);
                    return null;
                }
                let sysex = false;
                if (Number(prefsAddr) && prefsLen >= 4) {
                    sysex = (inst._dv.getUint32(Number(prefsAddr) + 0, true) & 1) !== 0;
                }
                return navigator.requestMIDIAccess({ sysex }).then(
                    (a) => {
                        const coll = kind === WAPI_ROLE_MIDI_INPUT ? a.inputs : a.outputs;
                        const port = Array.from(coll.values())[0];
                        if (!port) { role_write_result(inst, outResult, WAPI_ERR_NOENT); return; }
                        const entry = { type: "midi", port, inbox: [], kind: kind === WAPI_ROLE_MIDI_INPUT ? 0 : 1 };
                        if (kind === WAPI_ROLE_MIDI_INPUT) {
                            port.onmidimessage = (e) => {
                                entry.inbox.push({
                                    data: e.data,
                                    ts: BigInt(Math.round((e.timeStamp || performance.now()) * 1e6)),
                                });
                            };
                        }
                        try { port.open && port.open(); } catch (_) {}
                        const h = inst.handles.insert(entry);
                        role_write_handle(inst, outHandle, h);
                        role_write_result(inst, outResult, WAPI_OK);
                    },
                    () => role_write_result(inst, outResult, WAPI_ERR_ACCES)
                );
            }
            case WAPI_ROLE_KEYBOARD:
                role_write_handle(inst, outHandle, WAPI_INPUT_HANDLE_KEYBOARD);
                role_write_result(inst, outResult, WAPI_OK);
                return null;
            case WAPI_ROLE_MOUSE:
                role_write_handle(inst, outHandle, WAPI_INPUT_HANDLE_MOUSE);
                role_write_result(inst, outResult, WAPI_OK);
                return null;
            case WAPI_ROLE_POINTER:
                role_write_handle(inst, outHandle, WAPI_INPUT_HANDLE_POINTER);
                role_write_result(inst, outResult, WAPI_OK);
                return null;
            case WAPI_ROLE_HID: {
                if (!navigator.hid || !navigator.hid.requestDevice) {
                    role_write_result(inst, outResult, WAPI_ERR_NOTSUP);
                    return null;
                }
                const filter = {};
                if (Number(prefsAddr) && prefsLen >= 8) {
                    const p = Number(prefsAddr);
                    const v = inst._dv.getUint16(p + 0, true);
                    const pid = inst._dv.getUint16(p + 2, true);
                    const up = inst._dv.getUint16(p + 4, true);
                    const us = inst._dv.getUint16(p + 6, true);
                    if (v)   filter.vendorId   = v;
                    if (pid) filter.productId  = pid;
                    if (up)  filter.usagePage  = up;
                    if (us)  filter.usage      = us;
                }
                return navigator.hid.requestDevice({ filters: Object.keys(filter).length ? [filter] : [] }).then(
                    (devs) => {
                        const dev = devs && devs[0];
                        if (!dev) { role_write_result(inst, outResult, WAPI_ERR_ACCES); return; }
                        return dev.open().then(() => {
                            const handle = inst.handles.insert({ type: "hid", dev });
                            role_write_handle(inst, outHandle, handle);
                            role_write_result(inst, outResult, WAPI_OK);
                        }, () => role_write_result(inst, outResult, WAPI_ERR_ACCES));
                    },
                    () => role_write_result(inst, outResult, WAPI_ERR_ACCES)
                );
            }
            case WAPI_ROLE_SENSOR: {
                let sensorType = 0, freq = 0;
                if (Number(prefsAddr) && prefsLen >= 8) {
                    const p = Number(prefsAddr);
                    sensorType = inst._dv.getUint32(p + 0, true);
                    const bits = inst._dv.getUint32(p + 4, true);
                    const b = new ArrayBuffer(4);
                    new DataView(b).setUint32(0, bits, true);
                    freq = new DataView(b).getFloat32(0, true);
                }
                const Ctor = SENSOR_CTORS[sensorType];
                if (!Ctor) { role_write_result(inst, outResult, WAPI_ERR_NOTSUP); return null; }
                try {
                    const sensor = new Ctor({ frequency: freq > 0 ? freq : 60 });
                    const entry = { type: "sensor", sensor, kind: sensorType, xyz: null, scalar: null };
                    sensor.onreading = () => {
                        const ts = BigInt(Math.round(performance.now() * 1e6));
                        if (sensorType === 3 || sensorType === 4) {
                            entry.scalar = { value: sensor.illuminance ?? sensor.distance ?? 0, ts };
                        } else {
                            entry.xyz = { x: sensor.x || 0, y: sensor.y || 0, z: sensor.z || 0, ts };
                        }
                    };
                    sensor.start();
                    const h = inst.handles.insert(entry);
                    role_write_handle(inst, outHandle, h);
                    role_write_result(inst, outResult, WAPI_OK);
                } catch (_) {
                    role_write_result(inst, outResult, WAPI_ERR_ACCES);
                }
                return null;
            }
            case WAPI_ROLE_GAMEPAD:
            case WAPI_ROLE_TOUCH:
            case WAPI_ROLE_PEN:
            case WAPI_ROLE_HAPTIC:
            case WAPI_ROLE_DISPLAY:
            default:
                role_write_result(inst, outResult, WAPI_ERR_NOSYS);
                return null;
            }
        }

        /* role_fulfill_one returns null for sync-resolved entries (handle +
         * result already written before return) and a Promise for async
         * ones. If the whole batch is synchronous we fire the IO
         * completion immediately so the guest's synchronous poll loop
         * inside wapi_main sees it without having to yield to the browser
         * event loop. If any entry is async, we wait for all before
         * completing. */
        H.set(WAPI_IO_OP_ROLE_REQUEST, ({ addr, flags2, len, userData, self: inst }) => {
            const count = flags2 || Math.floor(Number(len) / 56);
            const pending = [];
            for (let i = 0; i < count; i++) {
                const p = role_fulfill_one(inst, Number(addr) + i * 56);
                if (p && typeof p.then === "function") pending.push(p);
            }
            if (pending.length === 0) {
                complete(inst, userData, WAPI_OK);
            } else {
                Promise.all(pending).then(() => complete(inst, userData, WAPI_OK));
            }
        });

        H.set(WAPI_IO_OP_ROLE_REPICK, ({ fd, resultPtr, userData, self: inst }) => {
            // No browser-side picker yet — re-issue the same handle.
            if (resultPtr) inst._writeI32(resultPtr, fd);
            complete(inst, userData, WAPI_OK);
        });

        // ====================================================================
        // NOTIFY SHOW (0x2F8) — capability grant is handled by the
        // universal CAP_REQUEST path above.
        // ====================================================================

        H.set(WAPI_IO_OP_NOTIFY_SHOW, ({ addr, resultPtr, userData, self: inst }) => {
            if (typeof Notification === "undefined") return complete(inst, userData, WAPI_ERR_NOTSUP);
            if (Notification.permission !== "granted") return complete(inst, userData, WAPI_ERR_ACCES);
            const title = inst._readStringView(addr + 0) || "";
            const body  = inst._readStringView(addr + 16) || "";
            const icon  = inst._readStringView(addr + 32);
            try {
                const n = new Notification(title, { body, icon: icon || undefined });
                const h = inst.handles.insert({ type: "notification", obj: n });
                inst._writeI32(resultPtr, h);
                complete(inst, userData, WAPI_OK);
            } catch (_) { complete(inst, userData, WAPI_ERR_IO); }
        });

        // ====================================================================
        // FONT family_info (0x2FC)
        // ====================================================================

        H.set(WAPI_IO_OP_FONT_FAMILY_INFO, ({ flags, addr, userData, self: inst }) => {
            const defaults = ["serif", "sans-serif", "monospace", "cursive", "fantasy",
                              "system-ui", "Arial", "Times New Roman", "Courier New",
                              "Georgia", "Verdana", "Helvetica"];
            const writeInfo = (list, index) => {
                if (index < 0 || index >= list.length) return complete(inst, userData, WAPI_ERR_OVERFLOW);
                const name = list[index];
                inst._refreshViews();
                const encoded = new TextEncoder().encode(name);
                const ptr = inst._hostAlloc(encoded.length + 1, 1);
                inst._refreshViews();
                inst._u8.set(encoded, ptr);
                inst._u8[ptr + encoded.length] = 0;
                inst._dv.setBigUint64(addr + 0, BigInt(ptr), true);
                inst._dv.setBigUint64(addr + 8, BigInt(encoded.length), true);
                inst._dv.setUint32(addr + 16, 100, true);
                inst._dv.setUint32(addr + 20, 900, true);
                inst._dv.setUint32(addr + 24, 0x0007, true);
                inst._dv.setInt32 (addr + 28, 0, true);
                complete(inst, userData, list.length);
            };
            if (window.queryLocalFonts) {
                window.queryLocalFonts().then(
                    (fonts) => {
                        const fams = [...new Set(fonts.map(f => f.family))];
                        writeInfo(fams.length ? fams : defaults, flags);
                    },
                    () => writeInfo(defaults, flags)
                );
            } else {
                writeInfo(defaults, flags);
            }
        });

        // ====================================================================
        // FWATCH (host: 0x008-0x009; sandbox: 0x2A3-0x2A4)
        // ====================================================================

        H.set(WAPI_IO_OP_FWATCH_ADD, ({ flags, addr, len, resultPtr, userData, self: inst }) => {
            const path = inst._readString(addr, Number(len));
            const entry = { type: "fwatch", path, recursive: !!flags };
            if (typeof FileSystemObserver !== "undefined") {
                try {
                    const obs = new FileSystemObserver((records) => {
                        for (const r of records) {
                            const kind = r.type === "appeared" ? 0 : r.type === "disappeared" ? 2 : 1;
                            complete(inst, userData, kind, 0x0001 /* MORE */);
                        }
                    });
                    entry.observer = obs;
                } catch (_) {}
            }
            const h = inst.handles.insert(entry);
            inst._writeI32(resultPtr, h);
            complete(inst, userData, WAPI_OK, 0x0001 /* MORE */);
        });

        H.set(WAPI_IO_OP_FWATCH_REMOVE, ({ fd, userData, self: inst }) => {
            const e = inst.handles.get(fd);
            if (!e || e.type !== "fwatch") return complete(inst, userData, WAPI_ERR_BADF);
            try { e.observer && e.observer.disconnect(); } catch (_) {}
            inst.handles.remove(fd);
            complete(inst, userData, WAPI_OK);
        });

        // ====================================================================
        // BARCODE (0x2D8-0x2D9)
        // ====================================================================

        const writeBarcodeResults2 = (inst, results, maxResults, resultsBufPtr) => {
            const n = Math.min(results.length, maxResults);
            const formatMap = { qr_code:0, ean_13:1, ean_8:2, code_128:3, code_39:4,
                                upc_a:5, upc_e:6, data_matrix:7, pdf417:8, aztec:9 };
            inst._refreshViews();
            for (let i = 0; i < n; i++) {
                const r = results[i];
                const base = resultsBufPtr + i * 32;
                inst._dv.setUint32(base + 0, formatMap[r.format] ?? 0, true);
                const encoded = new TextEncoder().encode(r.rawValue || "");
                const vptr = inst._hostAlloc(encoded.length + 1, 1);
                inst._refreshViews();
                inst._u8.set(encoded, vptr);
                inst._u8[vptr + encoded.length] = 0;
                inst._dv.setUint32(base + 4, encoded.length, true);
                inst._dv.setBigUint64(base + 8, BigInt(vptr), true);
                const box = r.boundingBox || { x:0, y:0, width:0, height:0 };
                inst._dv.setFloat32(base + 16, box.x, true);
                inst._dv.setFloat32(base + 20, box.y, true);
                inst._dv.setFloat32(base + 24, box.width, true);
                inst._dv.setFloat32(base + 28, box.height, true);
            }
            return n;
        };

        H.set(WAPI_IO_OP_BARCODE_DETECT_IMAGE, ({ flags, flags2, addr, addr2, len2, userData, self: inst }) => {
            if (typeof BarcodeDetector === "undefined") return complete(inst, userData, WAPI_ERR_NOTSUP);
            const width = flags, height = flags2;
            inst._refreshViews();
            const px = inst._u8.slice(addr, addr + width * height * 4);
            const img = new ImageData(new Uint8ClampedArray(px.buffer), width, height);
            const canvas = document.createElement("canvas");
            canvas.width = width; canvas.height = height;
            canvas.getContext("2d").putImageData(img, 0, 0);
            const det = new BarcodeDetector();
            det.detect(canvas).then(
                (results) => {
                    const maxResults = Math.floor(Number(len2) / 32);
                    const count = writeBarcodeResults(inst, results, maxResults, addr2);
                    complete(inst, userData, count);
                },
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        H.set(WAPI_IO_OP_BARCODE_DETECT_CAMERA, ({ fd, addr, len, userData, self: inst }) => {
            if (typeof BarcodeDetector === "undefined") return complete(inst, userData, WAPI_ERR_NOTSUP);
            const cam = inst.handles.get(fd);
            if (!cam || cam.type !== "camera") return complete(inst, userData, WAPI_ERR_BADF);
            if (!cam.video.videoWidth) return complete(inst, userData, WAPI_ERR_AGAIN);
            const det = new BarcodeDetector();
            det.detect(cam.video).then(
                (results) => {
                    const maxResults = Math.floor(Number(len) / 32);
                    const count = writeBarcodeResults(inst, results, maxResults, addr);
                    complete(inst, userData, count);
                },
                () => complete(inst, userData, WAPI_ERR_IO)
            );
        });

        // Single-seat browser host: wapi_input.seat is registered in the
        // wapi_input object above and always returns WAPI_SEAT_DEFAULT.

        // wapi_random (spec §9: separate capability / import module "wapi_random")
        const wapi_random = {
            get(bufPtr, len) {
                self._refreshViews();
                const n = Number(len);
                const sub = self._u8.subarray(bufPtr, bufPtr + n);
                crypto.getRandomValues(sub);
                return WAPI_OK;
            },
            get_nonblock(bufPtr, len) {
                self._refreshViews();
                const n = Number(len);
                const sub = self._u8.subarray(bufPtr, bufPtr + n);
                crypto.getRandomValues(sub);
                return WAPI_OK;
            },
            fill_seed(bufPtr, len) {
                self._refreshViews();
                const n = Number(len);
                const sub = self._u8.subarray(bufPtr, bufPtr + n);
                crypto.getRandomValues(sub);
                return WAPI_OK;
            },
        };

        /* wapi_io_bridge: the 10-function host-import module that guest
         * reactor shims import from. Five live on the wapi_io dispatch
         * object; five are capability/namespace queries. */
        const wapi_io_bridge = {
            submit:          wapi_io.submit,
            cancel:          wapi_io.cancel,
            poll:            wapi_io.poll,
            wait:            wapi_io.wait,
            flush:           wapi_io.flush,
            cap_supported:   wapi.cap_supported,
            cap_version:     wapi.cap_version,
            cap_query(capSvPtr, statePtr) {
                const cap = self._readStringView(capSvPtr);
                if (!cap) { self._writeU32(statePtr, 0); return WAPI_ERR_INVAL; }
                /* 0=GRANTED, 1=DENIED, 2=PROMPT. Every supported cap is
                 * granted until WAPI_IO_OP_CAP_REQUEST prompts for real. */
                const state = supportedCaps.includes(cap) ? 0 : 2;
                self._writeU32(statePtr, state);
                return WAPI_OK;
            },
            namespace_register(svPtr, outIdPtr) {
                const name = self._readStringView(svPtr);
                if (!name) return WAPI_ERR_INVAL;
                if (!self._nsRegistry) {
                    self._nsRegistry = new Map();
                    self._nsRegistryRev = new Map();
                    self._nsRegistryNext = 0x4000;
                }
                let id = self._nsRegistry.get(name);
                if (id === undefined) {
                    if (self._nsRegistryNext > 0xFFFF) return WAPI_ERR_NOSPC;
                    id = self._nsRegistryNext++;
                    self._nsRegistry.set(name, id);
                    self._nsRegistryRev.set(id, name);
                }
                self._refreshViews();
                self._dv.setUint16(outIdPtr, id, true);
                return WAPI_OK;
            },
            namespace_name(id, bufPtr, bufLen, nameLenPtr) {
                const name = self._nsRegistryRev && self._nsRegistryRev.get(id);
                if (!name) return WAPI_ERR_NOENT;
                const written = self._writeString(bufPtr, Number(bufLen), name);
                self._writeU64(nameLenPtr, written);
                return WAPI_OK;
            },
        };

        return { wapi, wapi_env, wapi_memory, wapi_clock, wapi_filesystem,
                 wapi_random,
                 wapi_io_bridge,
                 wapi_gpu, wapi_wgpu, env,
                 wapi_surface, wapi_input, wapi_audio, wapi_content, wapi_text,
                 wapi_transfer, wapi_seat,
                 wapi_kv, wapi_font, wapi_crypto, wapi_video, wapi_module,
                 wapi_notify, wapi_geo, wapi_sensor, wapi_speech, wapi_bio,
                 wapi_pay, wapi_usb, wapi_midi, wapi_bt, wapi_camera, wapi_xr,
                 wapi_register, wapi_taskbar, wapi_power, wapi_orient,
                 wapi_codec, wapi_media, wapi_encode,
                 wapi_authn, wapi_netinfo, wapi_haptic,
                 wapi_serial, wapi_capture, wapi_contacts,
                 wapi_barcode, wapi_nfc,
                 wapi_window, wapi_display, wapi_dialog, wapi_menu, wapi_tray,
                 wapi_theme, wapi_sysinfo, wapi_user, wapi_process, wapi_thread,
                 wapi_eyedrop, wapi_plugin,
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

            case WAPI_EVENT_TRANSFER_ENTER:
            case WAPI_EVENT_TRANSFER_OVER:
            case WAPI_EVENT_TRANSFER_LEAVE:
            case WAPI_EVENT_TRANSFER_DELIVER:
                // wapi_transfer_event_t after 16-byte common header:
                //   16: i32 pointer_id
                //   20: i32 x
                //   24: i32 y
                //   28: u32 item_count
                //   32: u32 available_actions
                this._dv.setInt32(ptr + 16, ev.pointer_id | 0, true);
                this._dv.setInt32(ptr + 20, ev.x | 0, true);
                this._dv.setInt32(ptr + 24, ev.y | 0, true);
                this._dv.setUint32(ptr + 28, (ev.item_count >>> 0) || 0, true);
                this._dv.setUint32(ptr + 32, (ev.available_actions >>> 0) || 0, true);
                break;

            case WAPI_EVENT_IO_COMPLETION:
                // wapi_io_event_t after 16-byte common header:
                //   16: int32_t   result
                //   20: uint32_t  flags    (WAPI_IO_CQE_F_*)
                //   24: uint64_t  user_data
                //   32: uint8_t   payload[96]  (inline, when F_INLINE set)
                this._dv.setInt32(ptr + 16, ev.result | 0, true);
                this._dv.setUint32(ptr + 20, ev.flags >>> 0, true);
                this._dv.setBigUint64(ptr + 24, BigInt(ev.userData || 0n), true);
                if (ev.inlinePayload) {
                    const n = Math.min(ev.inlinePayload.length, 96);
                    this._u8.set(ev.inlinePayload.subarray(0, n), ptr + 32);
                }
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
        // Window-level listeners (so drags continue when the cursor leaves the canvas);
        // coordinates are translated into canvas-local space because the canvas can be
        // inset from the viewport (e.g. by a host-page stats bar).
        window.addEventListener("mousemove", (e) => {
            const rect = canvas.getBoundingClientRect();
            const lx = e.clientX - rect.left;
            const ly = e.clientY - rect.top;
            self._mouseX = lx;
            self._mouseY = ly;
            self._eventQueue.push({
                type: WAPI_EVENT_MOUSE_MOTION,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
                mouse_id: 0,
                button_state: self._mouseButtons,
                x: lx,
                y: ly,
                xrel: e.movementX,
                yrel: e.movementY,
            });
        });

        window.addEventListener("mousedown", (e) => {
            const btn = domButtonToTP(e.button);
            self._mouseButtons |= (1 << btn);
            const rect = canvas.getBoundingClientRect();
            self._eventQueue.push({
                type: WAPI_EVENT_MOUSE_BUTTON_DOWN,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
                mouse_id: 0,
                button: btn,
                down: 1,
                clicks: e.detail,
                x: e.clientX - rect.left,
                y: e.clientY - rect.top,
            });
        });

        window.addEventListener("mouseup", (e) => {
            const btn = domButtonToTP(e.button);
            self._mouseButtons &= ~(1 << btn);
            const rect = canvas.getBoundingClientRect();
            self._eventQueue.push({
                type: WAPI_EVENT_MOUSE_BUTTON_UP,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
                mouse_id: 0,
                button: btn,
                down: 0,
                clicks: e.detail,
                x: e.clientX - rect.left,
                y: e.clientY - rect.top,
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

        // Drag-and-drop. Bridge HTML5 drag events into the unified WAPI
        // transfer event family (WAPI_EVENT_TRANSFER_*). Items are stashed
        // in self._dropItems as { mime, bytes } so apps can read them via
        // wapi_transfer_read(POINTED, mime, ...).
        const transferActionsFromEffectAllowed = (s) => {
            // Bitmask of wapi_transfer_action_t values.
            switch (s) {
                case "none":         return 0;
                case "copy":         return 1 << 1;       // COPY
                case "move":         return 1 << 2;       // MOVE
                case "link":         return 1 << 3;       // LINK
                case "copyMove":     return (1 << 1) | (1 << 2);
                case "copyLink":     return (1 << 1) | (1 << 3);
                case "linkMove":     return (1 << 2) | (1 << 3);
                case "all":
                case "uninitialized":
                default:             return (1 << 1) | (1 << 2) | (1 << 3);
            }
        };
        const pushTransferEvent = (e, type, count) => {
            const rect = canvas.getBoundingClientRect();
            self._eventQueue.push({
                type,
                surface_id: sid,
                timestamp: performance.now() * 1_000_000,
                pointer_id: 0, // single-pointer drag in browsers
                x: Math.round(e.clientX - rect.left),
                y: Math.round(e.clientY - rect.top),
                item_count: count,
                available_actions: transferActionsFromEffectAllowed(
                    e.dataTransfer && e.dataTransfer.effectAllowed),
            });
        };
        canvas.addEventListener("dragenter", (e) => {
            const types = (e.dataTransfer && e.dataTransfer.types) || [];
            if (types.length === 0) return;
            e.preventDefault();
            pushTransferEvent(e, WAPI_EVENT_TRANSFER_ENTER, types.length);
        });
        canvas.addEventListener("dragover", (e) => {
            const types = (e.dataTransfer && e.dataTransfer.types) || [];
            if (types.length === 0) return;
            e.preventDefault();
            if (e.dataTransfer) e.dataTransfer.dropEffect = "copy";
            pushTransferEvent(e, WAPI_EVENT_TRANSFER_OVER, types.length);
        });
        canvas.addEventListener("dragleave", (e) => {
            pushTransferEvent(e, WAPI_EVENT_TRANSFER_LEAVE, 0);
        });
        canvas.addEventListener("drop", (e) => {
            e.preventDefault();
            self._dropItems = [];
            const dt = e.dataTransfer;
            if (!dt) {
                pushTransferEvent(e, WAPI_EVENT_TRANSFER_DELIVER, 0);
                return;
            }
            // Synchronously stash text/* items.
            for (const t of (dt.types || [])) {
                if (t === "Files") continue;
                const data = dt.getData(t);
                if (data) {
                    self._dropItems.push({
                        mime: t,
                        bytes: new TextEncoder().encode(data),
                    });
                }
            }
            // Asynchronously stash file items; deliver event after all loaded.
            const filePromises = Array.from(dt.files || []).map((f) =>
                f.arrayBuffer().then((buf) => ({
                    mime: f.type || "application/octet-stream",
                    bytes: new Uint8Array(buf),
                }))
            );
            if (filePromises.length === 0) {
                pushTransferEvent(e, WAPI_EVENT_TRANSFER_DELIVER,
                                  self._dropItems.length);
                return;
            }
            Promise.all(filePromises).then((fileItems) => {
                self._dropItems.push(...fileItems);
                pushTransferEvent(e, WAPI_EVENT_TRANSFER_DELIVER,
                                  self._dropItems.length);
            });
        });

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
            // Chrome ignores powerPreference on Windows (crbug/369219127)
            // and prints a deprecation warning if we pass it. Leave it out;
            // the default adapter is fine for WAPI's use cases.
            this._gpuAdapter = await navigator.gpu.requestAdapter();
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
                throw new Error(`[WAPI] wapi_main returned ${wapiErrName(result)}`);
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

    /**
     * Register a JS-side handler for an IO opcode. Extension point for
     * host integrators who want to plug vendor opcodes into dispatch
     * without forking the shim.
     *
     * @param {number} opcode — packed u32 (namespace<<16 | method).
     * @param {(op: {fd, flags, flags2, offset, addr, len, addr2, len2,
     *     userData, resultPtr, _pushIoCompletion, self}) => void} handler
     *   Either complete synchronously (use _pushIoCompletion) or
     *   schedule async work that calls it later. `self` is the WAPI
     *   instance, giving access to memory helpers (_u8, _dv, etc.).
     */
    registerOpcodeHandler(opcode, handler) {
        if (!this._opcodeHandlers) this._opcodeHandlers = new Map();
        this._opcodeHandlers.set(opcode >>> 0, handler);
    }

    /**
     * Look up the minted id for a namespace name, without causing a
     * registration. Returns `undefined` if the namespace hasn't been
     * registered yet.
     */
    getNamespaceId(name) {
        return this._nsRegistry && this._nsRegistry.get(name);
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
