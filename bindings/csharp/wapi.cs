// ────────────────────────────────────────────────────────────────────────────
// WAPI — C# bindings (single file, two layers)
//
//   Layer 1 — Raw ABI
//     Byte-for-byte bindings that mirror wapi.h. Names faithful to the spec
//     tokens but re-cased to PascalCase per C# convention (e.g. the C
//     WAPI_RESULT_ERR_NOENT constant lives as WapiResult.ErrNoEnt; the C
//     wapi_surface_desc_t struct is WapiSurfaceDesc with PascalCase fields).
//     Structs laid out by Pack/FieldOffset; P/Invokes on `static class Wapi`.
//     Use this layer when cross-referencing the spec or when you need
//     precise control over pointers, layouts, and result codes.
//
//   Layer 2 — Idiomatic C#
//     Thin wrappers: [Flags] enums, string overloads, tuple returns, and
//     Panic on failure (spec-compliant abort via wapi_panic_report).
//     Subsystem classes (Surface, Window, Gpu, Input, Transfer, Seat, Audio,
//     Clock, Io) mirror the filenames under include/wapi/. The async I/O
//     pump lives here too — it's inherently idiomatic (keyed completion
//     dispatch).
//
//   Distribution
//     Shipped as a loose .cs from the canonical WAPI repo at
//     WAPI/bindings/csharp/wapi.cs. Consumer csprojs pick it up via the
//     default **/*.cs glob or an explicit <Compile Include>. No csproj
//     in the submodule — consumers own packaging.
//
//   Wasm32 notes
//     Targets wasm32-wasi. `wapi_size_t` is uint64 on the unified ABI, even
//     on wasm32 — pointer fields in wire-format structs are uint64 (must be
//     zero-extended from linear-memory pointers by hand). `wapi_handle_t` is
//     int32. `wapi_result_t` is int32, 0 = OK, negative = error.
//
//     The one exception is WapiIo: it's a *module-owned* vtable assembled
//     inside the caller's linear memory, so its function-pointer fields are
//     native-sized (4 bytes on wasm32), not unified-ABI uint64.
//
//     WebGPU is NOT WAPI — WAPI hands you a device via an opaque handle.
//     Full WebGPU bindings live in a separate file.
// ────────────────────────────────────────────────────────────────────────────

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace WAPI;

// ════════════════════════════════════════════════════════════════════════════
//  LAYER 1 — RAW ABI
// ════════════════════════════════════════════════════════════════════════════

// ── Result codes ────────────────────────────────────────────────────────────

public static class WapiResult
{
    public const int Ok             =  0;
    public const int ErrNoEnt       = -1;
    public const int ErrAcces       = -2;
    public const int ErrBadf        = -3;
    public const int ErrNoMem       = -4;
    public const int ErrInval       = -5;
    public const int ErrNotSup      = -6;
    public const int ErrOverflow    = -7;
    public const int ErrIo          = -8;
    public const int ErrBusy        = -9;
    public const int ErrExist       = -10;
    public const int ErrIsDir       = -11;
    public const int ErrNotDir      = -12;
    public const int ErrNotEmpty    = -13;
    public const int ErrPerm        = -14;
    public const int ErrCanceled    = -15;
    public const int ErrTimedOut    = -16;
    public const int ErrRange       = -17;
    public const int ErrAgain       = -18;
    public const int ErrNotCapable  = -19;
    public const int ErrUnknown     = -20;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool Failed(int result) { return result < 0; }

    public static string Name(int code)
    {
        return code switch
        {
            Ok             => "OK",
            ErrNoEnt       => "NOENT",
            ErrAcces       => "ACCES",
            ErrBadf        => "BADF",
            ErrNoMem       => "NOMEM",
            ErrInval       => "INVAL",
            ErrNotSup      => "NOTSUP",
            ErrOverflow    => "OVERFLOW",
            ErrIo          => "IO",
            ErrBusy        => "BUSY",
            ErrExist       => "EXIST",
            ErrIsDir       => "ISDIR",
            ErrNotDir      => "NOTDIR",
            ErrNotEmpty    => "NOTEMPTY",
            ErrPerm        => "PERM",
            ErrCanceled    => "CANCELED",
            ErrTimedOut    => "TIMEDOUT",
            ErrRange       => "RANGE",
            ErrAgain       => "AGAIN",
            ErrNotCapable  => "NOTCAPABLE",
            ErrUnknown     => "UNKNOWN",
            _              => "?",
        };
    }
}

// ── Core structs ────────────────────────────────────────────────────────────

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct WapiStringView
{
    public ulong Data;    // linear-memory address
    public ulong Length;  // byte count, or NullTerminated for null-terminated

    public const ulong NullTerminated = 0xFFFFFFFFFFFFFFFF;
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct WapiChainedStruct
{
    public ulong Next;    // address of next chained struct, 0 = end
    public uint SType;    // WapiSType.* tag
    public uint Pad;
}

// ── Surface ─────────────────────────────────────────────────────────────────

public static class WapiSurfaceFlags
{
    public const ulong HighDpi     = 0x0001;
    public const ulong Transparent = 0x0002;
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct WapiSurfaceDesc
{
    public ulong NextInChain;  // address of chained struct (e.g. WapiWindowConfig)
    public int Width;
    public int Height;
    public ulong Flags;
}

// ── Window ──────────────────────────────────────────────────────────────────

public static class WapiSType
{
    public const uint WindowConfig = 0x0001;
}

public static class WapiWindowFlags
{
    public const uint Resizable    = 0x0001;
    public const uint Borderless   = 0x0002;
    public const uint Fullscreen   = 0x0004;
    public const uint Hidden       = 0x0008;
    public const uint AlwaysOnTop  = 0x0010;
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct WapiWindowConfig
{
    public WapiChainedStruct Chain;  // SType = WapiSType.WindowConfig
    public WapiStringView Title;
    public uint WindowFlags;
    public uint Pad;
}

// ── GPU ─────────────────────────────────────────────────────────────────────

public static class WapiGpuPowerPreference
{
    public const uint Default   = 0;
    public const uint LowPower  = 1;
    public const uint HighPerf  = 2;
}

public static class WapiGpuPresentMode
{
    public const uint Fifo         = 0;
    public const uint FifoRelaxed  = 1;
    public const uint Immediate    = 2;
    public const uint Mailbox      = 3;
}

public static class WapiGpuTextureFormat
{
    public const uint Rgba8Unorm      = 0x0016;
    public const uint Rgba8UnormSrgb  = 0x0017;
    public const uint Bgra8Unorm      = 0x001B;
    public const uint Bgra8UnormSrgb  = 0x001C;
    public const uint Rgba16Float     = 0x0028;
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct WapiGpuDeviceDesc
{
    public ulong NextInChain;
    public uint PowerPreference;
    public uint RequiredFeaturesCount;
    public ulong RequiredFeatures;  // pointer to uint32[]
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct WapiGpuSurfaceConfig
{
    public ulong NextInChain;
    public int Surface;     // wapi_handle_t
    public int Device;      // wapi_handle_t
    public uint Format;     // WapiGpuTextureFormat.*
    public uint PresentMode;
    public uint Usage;      // WGPUTextureUsage flags
    public uint Pad;
}

// ── Events ──────────────────────────────────────────────────────────────────

public static class WapiEventType
{
    // Lifecycle
    public const uint Quit              = 0x0100;
    // Surface
    public const uint SurfaceResized    = 0x0200;
    public const uint SurfaceDpiChanged = 0x020A;
    // Window
    public const uint WindowClose       = 0x0210;
    public const uint WindowFocusGained = 0x0211;
    public const uint WindowFocusLost   = 0x0212;
    public const uint WindowShown       = 0x0213;
    public const uint WindowHidden      = 0x0214;
    public const uint WindowMinimized   = 0x0215;
    public const uint WindowMaximized   = 0x0216;
    public const uint WindowRestored    = 0x0217;
    public const uint WindowMoved       = 0x0218;
    // Keyboard
    public const uint KeyDown           = 0x0300;
    public const uint KeyUp             = 0x0301;
    public const uint TextInput         = 0x0302;
    public const uint TextEditing       = 0x0303;
    // Mouse
    public const uint MouseMotion       = 0x0400;
    public const uint MouseButtonDown   = 0x0401;
    public const uint MouseButtonUp     = 0x0402;
    public const uint MouseWheel        = 0x0403;
    // Touch
    public const uint TouchDown         = 0x0700;
    public const uint TouchUp           = 0x0701;
    public const uint TouchMotion       = 0x0702;
    // Pointer (unified mouse/touch/pen)
    public const uint PointerDown       = 0x0900;
    public const uint PointerUp         = 0x0901;
    public const uint PointerMotion     = 0x0902;
    public const uint PointerCancel     = 0x0903;
    public const uint PointerEnter      = 0x0904;
    public const uint PointerLeave      = 0x0905;
    // Device / role lifecycle
    public const uint DeviceAdded       = 0x0500;
    public const uint DeviceRemoved     = 0x0501;
    public const uint RoleRerouted      = 0x0502;
    public const uint RoleRevoked       = 0x0503;
    // IO Completion
    public const uint IoCompletion      = 0x2000;
}

[StructLayout(LayoutKind.Explicit, Size = 128)]
public unsafe struct WapiEvent
{
    // Common header (16 bytes)
    [FieldOffset(0)]  public uint Type;
    [FieldOffset(4)]  public uint SurfaceId;
    [FieldOffset(8)]  public ulong Timestamp;

    // Keyboard event overlay
    [FieldOffset(16)] public int KeyboardHandle;
    [FieldOffset(20)] public ushort Scancode;
    [FieldOffset(22)] public ushort Keycode;
    [FieldOffset(24)] public ushort KeyMod;
    [FieldOffset(26)] public byte KeyDown;
    [FieldOffset(27)] public byte KeyRepeat;

    // Text input overlay
    [FieldOffset(16)] public fixed byte TextBytes[32];

    // Mouse motion overlay
    [FieldOffset(16)] public int MouseHandle;
    [FieldOffset(20)] public uint MouseButtonState;
    [FieldOffset(24)] public float MouseX;
    [FieldOffset(28)] public float MouseY;
    [FieldOffset(32)] public float MouseXRel;
    [FieldOffset(36)] public float MouseYRel;

    // Mouse button overlay
    [FieldOffset(20)] public byte MouseButton;
    [FieldOffset(21)] public byte MouseButtonDown;
    [FieldOffset(22)] public byte MouseClicks;
    [FieldOffset(24)] public float MouseButtonX;
    [FieldOffset(28)] public float MouseButtonY;

    // Mouse wheel overlay
    [FieldOffset(24)] public float WheelX;
    [FieldOffset(28)] public float WheelY;

    // Surface event overlay
    [FieldOffset(16)] public int SurfaceData1;  // width or x
    [FieldOffset(20)] public int SurfaceData2;  // height or y

    // Pointer event overlay (unified mouse/touch/pen — 72 bytes)
    [FieldOffset(16)] public int PointerId;         // 0=mouse, 1+=touch fingers, <0=pen
    [FieldOffset(20)] public byte PointerType;      // WapiPointerType
    [FieldOffset(21)] public byte PointerButton;    // button that changed
    [FieldOffset(22)] public byte PointerButtons;   // bitmask of currently pressed buttons
    [FieldOffset(24)] public float PointerX;        // surface-pixel X
    [FieldOffset(28)] public float PointerY;        // surface-pixel Y
    [FieldOffset(32)] public float PointerDx;       // relative motion X
    [FieldOffset(36)] public float PointerDy;       // relative motion Y
    [FieldOffset(40)] public float PointerPressure; // 0..1
    [FieldOffset(44)] public float PointerTiltX;    // -90..90 degrees
    [FieldOffset(48)] public float PointerTiltY;    // -90..90 degrees
    [FieldOffset(52)] public float PointerTwist;    // 0..360 degrees
    [FieldOffset(56)] public float PointerWidth;    // contact width CSS px (1.0 for mouse/pen)
    [FieldOffset(60)] public float PointerHeight;   // contact height CSS px (1.0 for mouse/pen)

    // I/O completion event overlay — mirrors wapi_io_event_t in wapi.h.
    [FieldOffset(16)] public int IoResult;          // bytes transferred / new fd / negative error
    [FieldOffset(20)] public uint IoFlags;          // WAPI_IO_CQE_F_* flags
    [FieldOffset(24)] public ulong IoUserData;      // echoed from WapiIoOp.UserData

    // Device / role lifecycle overlay — mirrors wapi_device_event_t (40 bytes).
    // Fires for DEVICE_ADDED, DEVICE_REMOVED, ROLE_REROUTED, ROLE_REVOKED.
    [FieldOffset(16)] public uint DeviceRoleKind;   // wapi_role_kind_t
    [FieldOffset(20)] public int DeviceHandle;      // endpoint handle, or WAPI_HANDLE_INVALID
    [FieldOffset(24)] public ulong DeviceUid0;      // first 8 bytes of uid[16]
    [FieldOffset(32)] public ulong DeviceUid1;      // last 8 bytes of uid[16]
}

// ── Input constants ─────────────────────────────────────────────────────────

public static class WapiMouseButton
{
    public const byte Left   = 1;
    public const byte Middle = 2;
    public const byte Right  = 3;
    public const byte X1     = 4;
    public const byte X2     = 5;
}

public static class WapiPointerType
{
    public const byte Mouse = 0;
    public const byte Touch = 1;
    public const byte Pen   = 2;
}

public static class WapiCursorType
{
    public const int Default     = 0;
    public const int Pointer     = 1;
    public const int Text        = 2;
    public const int Crosshair   = 3;
    public const int Move        = 4;
    public const int ResizeNS    = 5;
    public const int ResizeEW    = 6;
    public const int ResizeNWSE  = 7;
    public const int ResizeNESW  = 8;
    public const int NotAllowed  = 9;
    public const int Wait        = 10;
    public const int Grab        = 11;
    public const int Grabbing    = 12;
    public const int None        = 13;
}

// ── Keyboard modifier flags ────────────────────────────────────────────────

public static class WapiKMod
{
    public const ushort LShift = 0x0001;
    public const ushort RShift = 0x0002;
    public const ushort Shift  = 0x0003;
    public const ushort LCtrl  = 0x0040;
    public const ushort RCtrl  = 0x0080;
    public const ushort Ctrl   = 0x00C0;
    public const ushort LAlt   = 0x0100;
    public const ushort RAlt   = 0x0200;
    public const ushort Alt    = 0x0300;
    public const ushort LGui   = 0x0400;
    public const ushort RGui   = 0x0800;
    public const ushort Gui    = 0x0C00;
    public const ushort Num    = 0x1000;
    public const ushort Caps   = 0x2000;
    public const ushort Action = 0x4000;
}

// ── Audio ──────────────────────────────────────────────────────────────────

public static class WapiAudioFormat
{
    public const uint U8  = 0x0008;
    public const uint S16 = 0x8010;
    public const uint S32 = 0x8020;
    public const uint F32 = 0x8120;
}

[StructLayout(LayoutKind.Sequential)]
public struct WapiAudioSpec
{
    public uint Format;
    public int Channels;
    public int Freq;
}

public static class WapiAudioForm
{
    public const uint Unknown    = 0;
    public const uint Speakers   = 1;
    public const uint Headphones = 2;
    public const uint Headset    = 3;
    public const uint LineOut    = 4;
    public const uint BuiltIn    = 5;
}

[StructLayout(LayoutKind.Sequential, Pack = 4, Size = 32)]
public unsafe struct WapiAudioEndpointInfo
{
    public WapiAudioSpec NativeSpec;
    public uint Form;
    public fixed byte Uid[16];
}

// ── Role system (device access, spec §9.10) ─────────────────────────────────

public static class WapiRoleKind
{
    public const uint AudioPlayback  = 0x01;
    public const uint AudioRecording = 0x02;
    public const uint Camera         = 0x03;
    public const uint MidiInput      = 0x04;
    public const uint MidiOutput     = 0x05;
    public const uint Keyboard       = 0x06;
    public const uint Mouse          = 0x07;
    public const uint Gamepad        = 0x08;
    public const uint Haptic         = 0x09;
    public const uint Sensor         = 0x0A;
    public const uint Display        = 0x0B;
    public const uint Hid            = 0x0C;
    public const uint Touch          = 0x0D;
    public const uint Pen            = 0x0E;
    public const uint Pointer        = 0x0F;
}

public static class WapiRoleFlags
{
    public const uint None          = 0;
    public const uint Optional      = 1u << 0;
    public const uint FollowDefault = 1u << 1;
    public const uint PinSpecific   = 1u << 2;
    public const uint All           = 1u << 3;
    public const uint WaitForDevice = 1u << 4;
}

// wapi_role_request_t: 56 bytes, 8-byte aligned.
[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 56)]
public unsafe struct WapiRoleRequest
{
    public uint Kind;
    public uint Flags;
    public ulong PrefsAddr;
    public uint PrefsLen;
    public uint _Pad;
    public ulong OutHandle;
    public ulong OutResult;
    public fixed byte TargetUid[16];
}

// ── HID prefs + endpoint info (for WAPI_ROLE_HID) ───────────────────────────

public static class WapiHidTransport
{
    public const uint Unknown = 0;
    public const uint Usb     = 1;
    public const uint Bt      = 2;
    public const uint Ble     = 3;
    public const uint I2c     = 4;
}

[StructLayout(LayoutKind.Sequential, Pack = 2, Size = 8)]
public struct WapiHidPrefs
{
    public ushort VendorId;
    public ushort ProductId;
    public ushort UsagePage;
    public ushort Usage;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 32)]
public unsafe struct WapiHidEndpointInfo
{
    public ushort VendorId;
    public ushort ProductId;
    public ushort UsagePage;
    public ushort Usage;
    public uint Transport;
    public uint ReportDescriptorLen;
    public fixed byte Uid[16];
}

// ── Camera / MIDI / Sensor / Haptic prefs + endpoint info ───────────────────

[StructLayout(LayoutKind.Sequential, Pack = 4, Size = 16)]
public struct WapiCameraDesc
{
    public uint Facing;
    public int Width;
    public int Height;
    public int Fps;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 40)]
public unsafe struct WapiCameraEndpointInfo
{
    public int Width;
    public int Height;
    public int Fps;
    public uint Facing;
    public uint NativeFormat;
    public uint _Pad;
    public fixed byte Uid[16];
}

[StructLayout(LayoutKind.Sequential, Pack = 4, Size = 4)]
public struct WapiMidiPrefs
{
    public uint Flags; // 1 = sysex
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 24)]
public unsafe struct WapiMidiEndpointInfo
{
    public uint ManufacturerId;
    public uint Flags;
    public fixed byte Uid[16];
}

[StructLayout(LayoutKind.Sequential, Pack = 4, Size = 8)]
public struct WapiSensorPrefs
{
    public uint Type;
    public uint FreqHzBits; // f32 raw bits, 0 = default
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 24)]
public unsafe struct WapiSensorEndpointInfo
{
    public uint Type;
    public uint NativeFreqBits;
    public fixed byte Uid[16];
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 24)]
public unsafe struct WapiHapticEndpointInfo
{
    public uint Features;
    public uint _Pad;
    public fixed byte Uid[16];
}

// ── IO ─────────────────────────────────────────────────────────────────────
//
// The wapi_io_t vtable is built by the shim as wasm function-table trampolines.
// Retrieved once at startup via Wapi.IoGet(); ops are submitted by indirectly
// calling the delegate* unmanaged fields. Unlike the wire-format structs,
// WapiIo's pointer fields are *native-sized* (4 bytes on wasm32) because this
// vtable lives in the caller module's linear memory, not in cross-module wire
// format.

[StructLayout(LayoutKind.Sequential, Pack = 4)]
public unsafe struct WapiIo
{
    public void* Impl;
    public delegate* unmanaged<void*, WapiIoOp*, ulong, int> Submit;
    public delegate* unmanaged<void*, ulong, int> Cancel;
    public delegate* unmanaged<void*, WapiEvent*, int> Poll;
    public delegate* unmanaged<void*, WapiEvent*, int, int> Wait;
    public delegate* unmanaged<void*, uint, void> Flush;
    public delegate* unmanaged<void*, WapiStringView*, int> CapSupported;
    public delegate* unmanaged<void*, WapiStringView*, uint*, int> CapVersion;
    public delegate* unmanaged<void*, WapiStringView*, uint*, int> CapQuery;
}

// wapi_io_op_t: 80 bytes, 8-byte aligned. Laid out in wapi.h.
[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 80)]
public struct WapiIoOp
{
    public uint Opcode;
    public uint Flags;
    public int Fd;
    public uint Flags2;
    public ulong Offset;
    public ulong Addr;
    public ulong Len;
    public ulong Addr2;
    public ulong Len2;
    public ulong UserData;
    public ulong ResultPtr;
    public ulong Reserved;
}

// Canonical capability names per wapi.h. Callers pass the UTF-8 literal
// directly to Wapi.CapSupported — kept here only as documentation
// of the spec-defined set.
//
//   "wapi.gpu"         "wapi.surface"     "wapi.window"    "wapi.input"
//   "wapi.audio"       "wapi.clipboard"   "wapi.font"      "wapi.clock"
//   "wapi.http"        "wapi.compression"

// ── Native WAPI imports ────────────────────────────────────────────────────

public static unsafe class Wapi
{
    // ── Core ──
    //
    // Spec drift: wapi.h says `cap_supported` and `io_get` are NOT host
    // imports — they're supposed to live on a module-owned wapi_io_t vtable
    // obtained via a C reactor shim that builds the vtable from a smaller
    // `wapi_io_bridge.{submit,poll,wait,cancel,flush}` import set. Reason:
    // wasm function pointers are indices into the caller module's own function
    // table, so the host cannot hand a module a struct of function pointers —
    // the vtable must be constructed *inside* the wasm module with local
    // function references. See wapi.h:1283-1296.
    //
    // Our browser shim (wapi_shim.js) currently provides both as direct host
    // imports for convenience, and we bind them that way here. Works today but
    // should migrate to a C reactor shim (alongside wapi_reactor.c) that
    // builds the vtable from bridge primitives. Don't add more direct-import
    // bindings for functions the spec places on the vtable.

    [WasmImportLinkage]
    [DllImport("wapi", EntryPoint = "cap_supported")]
    public static extern int CapSupported(WapiStringView* name);

    [WasmImportLinkage]
    [DllImport("wapi", EntryPoint = "allocator_get")]
    public static extern void* AllocatorGet();

    [WasmImportLinkage]
    [DllImport("wapi", EntryPoint = "panic_report")]
    public static extern void PanicReport(byte* msg, ulong msgLen);

    // ── Surface ──

    [WasmImportLinkage]
    [DllImport("wapi_surface", EntryPoint = "create")]
    public static extern int SurfaceCreate(WapiSurfaceDesc* desc, int* surfaceOut);

    [WasmImportLinkage]
    [DllImport("wapi_surface", EntryPoint = "destroy")]
    public static extern int SurfaceDestroy(int surface);

    [WasmImportLinkage]
    [DllImport("wapi_surface", EntryPoint = "get_size")]
    public static extern int SurfaceGetSize(int surface, int* width, int* height);

    [WasmImportLinkage]
    [DllImport("wapi_surface", EntryPoint = "get_dpi_scale")]
    public static extern int SurfaceGetDpiScale(int surface, float* scale);

    [WasmImportLinkage]
    [DllImport("wapi_surface", EntryPoint = "request_size")]
    public static extern int SurfaceRequestSize(int surface, int width, int height);

    // ── Window ──

    [WasmImportLinkage]
    [DllImport("wapi_window", EntryPoint = "set_title")]
    public static extern int WindowSetTitle(int surface, WapiStringView title);

    [WasmImportLinkage]
    [DllImport("wapi_window", EntryPoint = "get_size_logical")]
    public static extern int WindowGetSizeLogical(int surface, int* width, int* height);

    [WasmImportLinkage]
    [DllImport("wapi_window", EntryPoint = "set_fullscreen")]
    public static extern int WindowSetFullscreen(int surface, int fullscreen);

    [WasmImportLinkage]
    [DllImport("wapi_window", EntryPoint = "set_visible")]
    public static extern int WindowSetVisible(int surface, int visible);

    [WasmImportLinkage]
    [DllImport("wapi_window", EntryPoint = "minimize")]
    public static extern int WindowMinimize(int surface);

    [WasmImportLinkage]
    [DllImport("wapi_window", EntryPoint = "maximize")]
    public static extern int WindowMaximize(int surface);

    [WasmImportLinkage]
    [DllImport("wapi_window", EntryPoint = "restore")]
    public static extern int WindowRestore(int surface);

    // ── GPU ──

    [WasmImportLinkage]
    [DllImport("wapi_gpu", EntryPoint = "request_device")]
    public static extern int GpuRequestDevice(WapiGpuDeviceDesc* desc, int* deviceOut);

    [WasmImportLinkage]
    [DllImport("wapi_gpu", EntryPoint = "get_queue")]
    public static extern int GpuGetQueue(int device, int* queueOut);

    [WasmImportLinkage]
    [DllImport("wapi_gpu", EntryPoint = "release_device")]
    public static extern int GpuReleaseDevice(int device);

    [WasmImportLinkage]
    [DllImport("wapi_gpu", EntryPoint = "configure_surface")]
    public static extern int GpuConfigureSurface(WapiGpuSurfaceConfig* config);

    [WasmImportLinkage]
    [DllImport("wapi_gpu", EntryPoint = "surface_get_current_texture")]
    public static extern int GpuSurfaceGetCurrentTexture(int surface, int* textureOut, int* viewOut);

    [WasmImportLinkage]
    [DllImport("wapi_gpu", EntryPoint = "surface_present")]
    public static extern int GpuSurfacePresent(int surface);

    [WasmImportLinkage]
    [DllImport("wapi_gpu", EntryPoint = "surface_preferred_format")]
    public static extern int GpuSurfacePreferredFormat(int surface, uint* formatOut);

    // ── Input ──

    [WasmImportLinkage]
    [DllImport("wapi_input", EntryPoint = "mouse_set_cursor")]
    public static extern int MouseSetCursor(int handle, int cursorType);

    [WasmImportLinkage]
    [DllImport("wapi_input", EntryPoint = "start_textinput")]
    public static extern void InputStartTextInput(int surface);

    [WasmImportLinkage]
    [DllImport("wapi_input", EntryPoint = "stop_textinput")]
    public static extern void InputStopTextInput(int surface);

    // ── Transfer (clipboard / DnD / share unified) ──

    [WasmImportLinkage]
    [DllImport("wapi_transfer", EntryPoint = "revoke")]
    public static extern int TransferRevoke(int seat, uint mode);

    [WasmImportLinkage]
    [DllImport("wapi_transfer", EntryPoint = "format_count")]
    public static extern ulong TransferFormatCount(int seat, uint mode);

    [WasmImportLinkage]
    [DllImport("wapi_transfer", EntryPoint = "format_name")]
    public static extern int TransferFormatName(int seat, uint mode, ulong index,
                                                void* buf, ulong bufLen, ulong* outLen);

    [WasmImportLinkage]
    [DllImport("wapi_transfer", EntryPoint = "has_format")]
    public static extern int TransferHasFormat(int seat, uint mode,
                                               byte* mimeData, ulong mimeLen);

    [WasmImportLinkage]
    [DllImport("wapi_transfer", EntryPoint = "set_action")]
    public static extern int TransferSetAction(int seat, int action);

    // ── Seat ──

    [WasmImportLinkage]
    [DllImport("wapi_seat", EntryPoint = "count")]
    public static extern ulong SeatCount();

    [WasmImportLinkage]
    [DllImport("wapi_seat", EntryPoint = "at")]
    public static extern int SeatAt(ulong index);

    [WasmImportLinkage]
    [DllImport("wapi_seat", EntryPoint = "name")]
    public static extern int SeatName(int seat, void* buf, ulong bufLen, ulong* outLen);

    [WasmImportLinkage]
    [DllImport("wapi_input", EntryPoint = "device_seat")]
    public static extern int InputDeviceSeat(int device);

    // ── Audio ──

    [WasmImportLinkage]
    [DllImport("wapi_audio", EntryPoint = "open_device")]
    public static extern int AudioOpenDevice(int deviceId, WapiAudioSpec* spec, int* deviceOut);

    [WasmImportLinkage]
    [DllImport("wapi_audio", EntryPoint = "close_device")]
    public static extern int AudioCloseDevice(int device);

    [WasmImportLinkage]
    [DllImport("wapi_audio", EntryPoint = "resume_device")]
    public static extern int AudioResumeDevice(int device);

    [WasmImportLinkage]
    [DllImport("wapi_audio", EntryPoint = "pause_device")]
    public static extern int AudioPauseDevice(int device);

    [WasmImportLinkage]
    [DllImport("wapi_audio", EntryPoint = "create_stream")]
    public static extern int AudioCreateStream(WapiAudioSpec* srcSpec, WapiAudioSpec* dstSpec, int* streamOut);

    [WasmImportLinkage]
    [DllImport("wapi_audio", EntryPoint = "destroy_stream")]
    public static extern int AudioDestroyStream(int stream);

    [WasmImportLinkage]
    [DllImport("wapi_audio", EntryPoint = "bind_stream")]
    public static extern int AudioBindStream(int device, int stream);

    [WasmImportLinkage]
    [DllImport("wapi_audio", EntryPoint = "put_stream_data")]
    public static extern int AudioPutStreamData(int stream, void* buf, ulong len);

    [WasmImportLinkage]
    [DllImport("wapi_audio", EntryPoint = "get_stream_data")]
    public static extern int AudioGetStreamData(int stream, void* buf, ulong len, ulong* bytesRead);

    // ── Clock ──

    [WasmImportLinkage]
    [DllImport("wapi_clock", EntryPoint = "time_get")]
    public static extern int ClockTimeGet(int clockId, ulong* timeOut);

    [WasmImportLinkage]
    [DllImport("wapi_clock", EntryPoint = "resolution")]
    public static extern int ClockResolution(int clockId, ulong* resolutionOut);

    [WasmImportLinkage]
    [DllImport("wapi_clock", EntryPoint = "perf_counter")]
    public static extern ulong ClockPerfCounter();

    [WasmImportLinkage]
    [DllImport("wapi_clock", EntryPoint = "perf_frequency")]
    public static extern ulong ClockPerfFrequency();

    [WasmImportLinkage]
    [DllImport("wapi_clock", EntryPoint = "yield")]
    public static extern void ClockYield();

    [WasmImportLinkage]
    [DllImport("wapi_clock", EntryPoint = "sleep")]
    public static extern void ClockSleep(ulong durationNs);

    // ── Async I/O ──
    //
    // The wapi_io_t vtable is host-built and retrieved once via io_get.
    // All async ops go through the function pointers on WapiIo.

    [WasmImportLinkage]
    [DllImport("wapi", EntryPoint = "io_get")]
    public static extern WapiIo* IoGet();
}


// ════════════════════════════════════════════════════════════════════════════
//  LAYER 2 — IDIOMATIC
// ════════════════════════════════════════════════════════════════════════════

// ── Panic ──────────────────────────────────────────────────────────────────
//
// Idiomatic-layer failure path: format a message, forward it to the
// spec-compliant wapi_panic_report, then spin (the host kills the module).
// Never throws — exceptions aren't used as control flow.

internal static unsafe class WapiPanic
{
    [DoesNotReturn]
    public static void Report(string context, int code)
    {
        string message = context + " failed: code " + code + " (" + WapiResult.Name(code) + ")";
        int byteCount = Encoding.UTF8.GetByteCount(message);
        byte* bytes = stackalloc byte[byteCount == 0 ? 1 : byteCount];
        if (byteCount > 0)
            Encoding.UTF8.GetBytes(message, new Span<byte>(bytes, byteCount));
        Wapi.PanicReport(bytes, (ulong)byteCount);
        while (true) { }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void OnFailure(int result, string context)
    {
        if (WapiResult.Failed(result))
            Report(context, result);
    }
}

// ── Flags & enums ──────────────────────────────────────────────────────────

[Flags]
public enum SurfaceFlags : ulong
{
    None         = 0,
    HighDpi      = WapiSurfaceFlags.HighDpi,
    Transparent  = WapiSurfaceFlags.Transparent,
}

[Flags]
public enum WindowFlags : uint
{
    None         = 0,
    Resizable    = WapiWindowFlags.Resizable,
    Borderless   = WapiWindowFlags.Borderless,
    Fullscreen   = WapiWindowFlags.Fullscreen,
    Hidden       = WapiWindowFlags.Hidden,
    AlwaysOnTop  = WapiWindowFlags.AlwaysOnTop,
}

[Flags]
public enum KeyMod : ushort
{
    None    = 0,
    LShift  = WapiKMod.LShift,
    RShift  = WapiKMod.RShift,
    Shift   = WapiKMod.Shift,
    LCtrl   = WapiKMod.LCtrl,
    RCtrl   = WapiKMod.RCtrl,
    Ctrl    = WapiKMod.Ctrl,
    LAlt    = WapiKMod.LAlt,
    RAlt    = WapiKMod.RAlt,
    Alt     = WapiKMod.Alt,
    LGui    = WapiKMod.LGui,
    RGui    = WapiKMod.RGui,
    Gui     = WapiKMod.Gui,
    Num     = WapiKMod.Num,
    Caps    = WapiKMod.Caps,
    Action  = WapiKMod.Action,
}

public enum CursorType
{
    Default     = WapiCursorType.Default,
    Pointer     = WapiCursorType.Pointer,
    Text        = WapiCursorType.Text,
    Crosshair   = WapiCursorType.Crosshair,
    Move        = WapiCursorType.Move,
    ResizeNS    = WapiCursorType.ResizeNS,
    ResizeEW    = WapiCursorType.ResizeEW,
    ResizeNWSE  = WapiCursorType.ResizeNWSE,
    ResizeNESW  = WapiCursorType.ResizeNESW,
    NotAllowed  = WapiCursorType.NotAllowed,
    Wait        = WapiCursorType.Wait,
    Grab        = WapiCursorType.Grab,
    Grabbing    = WapiCursorType.Grabbing,
    None        = WapiCursorType.None,
}

public enum MouseButton : byte
{
    Left    = WapiMouseButton.Left,
    Middle  = WapiMouseButton.Middle,
    Right   = WapiMouseButton.Right,
    X1      = WapiMouseButton.X1,
    X2      = WapiMouseButton.X2,
}

public enum PointerKind : byte
{
    Mouse   = WapiPointerType.Mouse,
    Touch   = WapiPointerType.Touch,
    Pen     = WapiPointerType.Pen,
}

public enum PowerPreference : uint
{
    Default          = WapiGpuPowerPreference.Default,
    LowPower         = WapiGpuPowerPreference.LowPower,
    HighPerformance  = WapiGpuPowerPreference.HighPerf,
}

public enum PresentMode : uint
{
    Fifo         = WapiGpuPresentMode.Fifo,
    FifoRelaxed  = WapiGpuPresentMode.FifoRelaxed,
    Immediate    = WapiGpuPresentMode.Immediate,
    Mailbox      = WapiGpuPresentMode.Mailbox,
}

public enum GpuTextureFormat : uint
{
    Rgba8Unorm      = WapiGpuTextureFormat.Rgba8Unorm,
    Rgba8UnormSrgb  = WapiGpuTextureFormat.Rgba8UnormSrgb,
    Bgra8Unorm      = WapiGpuTextureFormat.Bgra8Unorm,
    Bgra8UnormSrgb  = WapiGpuTextureFormat.Bgra8UnormSrgb,
    Rgba16Float     = WapiGpuTextureFormat.Rgba16Float,
}

public enum EventKind : uint
{
    Quit              = WapiEventType.Quit,
    SurfaceResized    = WapiEventType.SurfaceResized,
    SurfaceDpiChanged = WapiEventType.SurfaceDpiChanged,
    WindowClose       = WapiEventType.WindowClose,
    WindowFocusGained = WapiEventType.WindowFocusGained,
    WindowFocusLost   = WapiEventType.WindowFocusLost,
    WindowShown       = WapiEventType.WindowShown,
    WindowHidden      = WapiEventType.WindowHidden,
    WindowMinimized   = WapiEventType.WindowMinimized,
    WindowMaximized   = WapiEventType.WindowMaximized,
    WindowRestored    = WapiEventType.WindowRestored,
    WindowMoved       = WapiEventType.WindowMoved,
    KeyDown           = WapiEventType.KeyDown,
    KeyUp             = WapiEventType.KeyUp,
    TextInput         = WapiEventType.TextInput,
    TextEditing       = WapiEventType.TextEditing,
    MouseMotion       = WapiEventType.MouseMotion,
    MouseButtonDown   = WapiEventType.MouseButtonDown,
    MouseButtonUp     = WapiEventType.MouseButtonUp,
    MouseWheel        = WapiEventType.MouseWheel,
    TouchDown         = WapiEventType.TouchDown,
    TouchUp           = WapiEventType.TouchUp,
    TouchMotion       = WapiEventType.TouchMotion,
    PointerDown       = WapiEventType.PointerDown,
    PointerUp         = WapiEventType.PointerUp,
    PointerMotion     = WapiEventType.PointerMotion,
    PointerCancel     = WapiEventType.PointerCancel,
    PointerEnter      = WapiEventType.PointerEnter,
    PointerLeave      = WapiEventType.PointerLeave,
    IoCompletion      = WapiEventType.IoCompletion,
}

public enum AudioFormatKind : uint
{
    U8   = WapiAudioFormat.U8,
    S16  = WapiAudioFormat.S16,
    S32  = WapiAudioFormat.S32,
    F32  = WapiAudioFormat.F32,
}

// ── Window config Args struct ─────────────────────────────────────────────

[StructLayout(LayoutKind.Sequential)]
public struct WindowArgs
{
    public string Title;
    public WindowFlags Flags;
}

// ── Subsystem façades ─────────────────────────────────────────────────────
//
// One static class per wapi_*.h header. These wrap the raw P/Invokes with
// C# conveniences (strings, enums, tuple returns) and Panic on failure.
// Callers who want to inspect codes should use layer 1 directly.

public static unsafe class Surface
{
    public static int Create(int width, int height, SurfaceFlags flags, in WindowArgs window)
    {
        int titleBytes = Encoding.UTF8.GetByteCount(window.Title ?? "");
        byte* titlePtr = stackalloc byte[titleBytes == 0 ? 1 : titleBytes];
        if (titleBytes > 0)
            Encoding.UTF8.GetBytes(window.Title, new Span<byte>(titlePtr, titleBytes));

        WapiWindowConfig wc = new WapiWindowConfig
        {
            Chain = new WapiChainedStruct { Next = 0, SType = WapiSType.WindowConfig },
            Title = new WapiStringView { Data = (ulong)(nuint)titlePtr, Length = (ulong)titleBytes },
            WindowFlags = (uint)window.Flags,
        };
        WapiSurfaceDesc desc = new WapiSurfaceDesc
        {
            NextInChain = (ulong)(nuint)(&wc),
            Width = width,
            Height = height,
            Flags = (ulong)flags,
        };
        int surface;
        WapiPanic.OnFailure(Wapi.SurfaceCreate(&desc, &surface), "SurfaceCreate");
        return surface;
    }

    public static int Create(int width, int height, SurfaceFlags flags = SurfaceFlags.None)
    {
        WapiSurfaceDesc desc = new WapiSurfaceDesc
        {
            NextInChain = 0,
            Width = width,
            Height = height,
            Flags = (ulong)flags,
        };
        int surface;
        WapiPanic.OnFailure(Wapi.SurfaceCreate(&desc, &surface), "SurfaceCreate");
        return surface;
    }

    public static void Destroy(int surface)
    {
        WapiPanic.OnFailure(Wapi.SurfaceDestroy(surface), "SurfaceDestroy");
    }

    public static (int Width, int Height) GetSize(int surface)
    {
        int w, h;
        WapiPanic.OnFailure(Wapi.SurfaceGetSize(surface, &w, &h), "SurfaceGetSize");
        return (w, h);
    }

    public static float GetDpiScale(int surface)
    {
        float scale;
        WapiPanic.OnFailure(Wapi.SurfaceGetDpiScale(surface, &scale), "SurfaceGetDpiScale");
        return scale;
    }

    public static void RequestSize(int surface, int width, int height)
    {
        WapiPanic.OnFailure(Wapi.SurfaceRequestSize(surface, width, height), "SurfaceRequestSize");
    }
}

public static unsafe class Window
{
    public static void SetTitle(int surface, string title)
    {
        int n = Encoding.UTF8.GetByteCount(title ?? "");
        byte* p = stackalloc byte[n == 0 ? 1 : n];
        if (n > 0) Encoding.UTF8.GetBytes(title, new Span<byte>(p, n));
        WapiStringView sv = new WapiStringView { Data = (ulong)(nuint)p, Length = (ulong)n };
        WapiPanic.OnFailure(Wapi.WindowSetTitle(surface, sv), "WindowSetTitle");
    }

    public static (int Width, int Height) GetSizeLogical(int surface)
    {
        int w, h;
        WapiPanic.OnFailure(Wapi.WindowGetSizeLogical(surface, &w, &h), "WindowGetSizeLogical");
        return (w, h);
    }

    public static void SetFullscreen(int surface, bool on)
    {
        WapiPanic.OnFailure(Wapi.WindowSetFullscreen(surface, on ? 1 : 0), "WindowSetFullscreen");
    }

    public static void SetVisible(int surface, bool visible)
    {
        WapiPanic.OnFailure(Wapi.WindowSetVisible(surface, visible ? 1 : 0), "WindowSetVisible");
    }

    public static void Minimize(int surface)
    {
        WapiPanic.OnFailure(Wapi.WindowMinimize(surface), "WindowMinimize");
    }

    public static void Maximize(int surface)
    {
        WapiPanic.OnFailure(Wapi.WindowMaximize(surface), "WindowMaximize");
    }

    public static void Restore(int surface)
    {
        WapiPanic.OnFailure(Wapi.WindowRestore(surface), "WindowRestore");
    }
}

public static unsafe class Gpu
{
    public static int RequestDevice(PowerPreference preference = PowerPreference.Default)
    {
        WapiGpuDeviceDesc desc = new WapiGpuDeviceDesc
        {
            NextInChain = 0,
            PowerPreference = (uint)preference,
            RequiredFeaturesCount = 0,
            RequiredFeatures = 0,
        };
        int device;
        WapiPanic.OnFailure(Wapi.GpuRequestDevice(&desc, &device), "GpuRequestDevice");
        return device;
    }

    public static int GetQueue(int device)
    {
        int queue;
        WapiPanic.OnFailure(Wapi.GpuGetQueue(device, &queue), "GpuGetQueue");
        return queue;
    }

    public static void ReleaseDevice(int device)
    {
        WapiPanic.OnFailure(Wapi.GpuReleaseDevice(device), "GpuReleaseDevice");
    }

    public static void ConfigureSurface(int surface, int device, GpuTextureFormat format,
                                        PresentMode presentMode, uint usage)
    {
        WapiGpuSurfaceConfig cfg = new WapiGpuSurfaceConfig
        {
            NextInChain = 0,
            Surface = surface,
            Device = device,
            Format = (uint)format,
            PresentMode = (uint)presentMode,
            Usage = usage,
        };
        WapiPanic.OnFailure(Wapi.GpuConfigureSurface(&cfg), "GpuConfigureSurface");
    }

    public static (int Texture, int View) SurfaceGetCurrentTexture(int surface)
    {
        int tex, view;
        WapiPanic.OnFailure(Wapi.GpuSurfaceGetCurrentTexture(surface, &tex, &view), "GpuSurfaceGetCurrentTexture");
        return (tex, view);
    }

    public static void SurfacePresent(int surface)
    {
        WapiPanic.OnFailure(Wapi.GpuSurfacePresent(surface), "GpuSurfacePresent");
    }

    public static GpuTextureFormat SurfacePreferredFormat(int surface)
    {
        uint format;
        WapiPanic.OnFailure(Wapi.GpuSurfacePreferredFormat(surface, &format), "GpuSurfacePreferredFormat");
        return (GpuTextureFormat)format;
    }
}

public static unsafe class Input
{
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void SetCursor(int handle, CursorType cursor)
    {
        Wapi.MouseSetCursor(handle, (int)cursor);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void StartTextInput(int surface) { Wapi.InputStartTextInput(surface); }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void StopTextInput(int surface)  { Wapi.InputStopTextInput(surface);  }
}

// Mode bitmask for Transfer.Offer / Transfer.Read / etc.
public static class TransferMode
{
    public const uint Latent  = 0x01; // clipboard
    public const uint Pointed = 0x02; // drag
    public const uint Routed  = 0x04; // share
}

public enum TransferAction : int
{
    None = 0,
    Copy = 1,
    Move = 2,
    Link = 3,
}

[StructLayout(LayoutKind.Sequential)]
public struct WapiTransferItem
{
    public WapiStringView Mime;       // 16
    public ulong          Data;       // 8 — wasm linear-memory address
    public ulong          DataLen;    // 8
}

[StructLayout(LayoutKind.Sequential)]
public struct WapiTransferOffer
{
    public ulong          Items;            // 8 — pointer to WapiTransferItem[]
    public uint           ItemCount;        // 4
    public uint           AllowedActions;   // 4 — bitmask of TransferAction
    public WapiStringView Title;            // 16
    public int            Preview;          // 4 — surface handle (0 = none)
    public uint           Reserved0;        // 4
    public ulong          Reserved1;        // 8
}

[StructLayout(LayoutKind.Sequential)]
public struct WapiTransferEvent
{
    public uint  Type;
    public uint  SurfaceId;
    public ulong Timestamp;
    public int   PointerId;
    public int   X;
    public int   Y;
    public uint  ItemCount;
    public uint  AvailableActions;
    public uint  Pad;
}

public static unsafe class Transfer
{
    public const int SeatDefault = 0;


    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int Revoke(int seat, uint mode)
        => Wapi.TransferRevoke(seat, mode);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong FormatCount(int seat, uint mode)
        => Wapi.TransferFormatCount(seat, mode);

    public static int FormatName(int seat, uint mode, ulong index,
                                 void* buf, ulong bufLen, out ulong outLen)
    {
        ulong n = 0;
        int res = Wapi.TransferFormatName(seat, mode, index, buf, bufLen, &n);
        outLen = n;
        return res;
    }

    public static bool HasFormat(int seat, uint mode, ReadOnlySpan<byte> mimeUtf8)
    {
        fixed (byte* p = mimeUtf8)
        {
            return Wapi.TransferHasFormat(seat, mode, p, (ulong)mimeUtf8.Length) != 0;
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int SetAction(int seat, TransferAction action)
        => Wapi.TransferSetAction(seat, (int)action);
}

public static unsafe class Seat
{
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong Count() => Wapi.SeatCount();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int At(ulong index) => Wapi.SeatAt(index);

    public static int Name(int seat, void* buf, ulong bufLen, out ulong outLen)
    {
        ulong n = 0;
        int res = Wapi.SeatName(seat, buf, bufLen, &n);
        outLen = n;
        return res;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int FromDevice(int device) => Wapi.InputDeviceSeat(device);
}

public static unsafe class Audio
{
    public static int OpenDevice(int deviceId, in WapiAudioSpec spec)
    {
        fixed (WapiAudioSpec* p = &spec)
        {
            int dev;
            WapiPanic.OnFailure(Wapi.AudioOpenDevice(deviceId, p, &dev), "AudioOpenDevice");
            return dev;
        }
    }

    public static void CloseDevice(int device)  { WapiPanic.OnFailure(Wapi.AudioCloseDevice(device),  "AudioCloseDevice");  }
    public static void ResumeDevice(int device) { WapiPanic.OnFailure(Wapi.AudioResumeDevice(device), "AudioResumeDevice"); }
    public static void PauseDevice(int device)  { WapiPanic.OnFailure(Wapi.AudioPauseDevice(device),  "AudioPauseDevice");  }

    public static int CreateStream(in WapiAudioSpec src, in WapiAudioSpec dst)
    {
        fixed (WapiAudioSpec* sp = &src)
        fixed (WapiAudioSpec* dp = &dst)
        {
            int stream;
            WapiPanic.OnFailure(Wapi.AudioCreateStream(sp, dp, &stream), "AudioCreateStream");
            return stream;
        }
    }

    public static void DestroyStream(int stream) { WapiPanic.OnFailure(Wapi.AudioDestroyStream(stream), "AudioDestroyStream"); }
    public static void BindStream(int device, int stream) { WapiPanic.OnFailure(Wapi.AudioBindStream(device, stream), "AudioBindStream"); }

    public static void PutStreamData(int stream, ReadOnlySpan<byte> data)
    {
        fixed (byte* p = data)
            WapiPanic.OnFailure(Wapi.AudioPutStreamData(stream, p, (ulong)data.Length), "AudioPutStreamData");
    }

    public static int GetStreamData(int stream, Span<byte> dst)
    {
        fixed (byte* p = dst)
        {
            ulong bytesRead = 0;
            WapiPanic.OnFailure(Wapi.AudioGetStreamData(stream, p, (ulong)dst.Length, &bytesRead), "AudioGetStreamData");
            return (int)bytesRead;
        }
    }
}

public static unsafe class Clock
{
    public static ulong TimeGet(int clockId)
    {
        ulong t;
        WapiPanic.OnFailure(Wapi.ClockTimeGet(clockId, &t), "ClockTimeGet");
        return t;
    }

    public static ulong Resolution(int clockId)
    {
        ulong r;
        WapiPanic.OnFailure(Wapi.ClockResolution(clockId, &r), "ClockResolution");
        return r;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong PerfCounter()   { return Wapi.ClockPerfCounter(); }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong PerfFrequency() { return Wapi.ClockPerfFrequency(); }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void Yield()          { Wapi.ClockYield(); }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void Sleep(ulong durationNs) { Wapi.ClockSleep(durationNs); }
}

public static unsafe class Core
{
    public static bool CapSupported(ReadOnlySpan<byte> nameUtf8)
    {
        fixed (byte* p = nameUtf8)
        {
            WapiStringView sv = new WapiStringView
            {
                Data = (ulong)(nuint)p,
                Length = (ulong)nameUtf8.Length,
            };
            return Wapi.CapSupported(&sv) != 0;
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void* AllocatorGet() { return Wapi.AllocatorGet(); }

    public static void PanicReport(ReadOnlySpan<byte> msgUtf8)
    {
        fixed (byte* p = msgUtf8)
            Wapi.PanicReport(p, (ulong)msgUtf8.Length);
    }
}

// ── Async I/O ──────────────────────────────────────────────────────────────
//
// Front-end for the module-owned wapi_io_t vtable. Submits async ops and
// dispatches completions to per-op callbacks keyed by a user_data token.
// Completions arrive through the unified event queue — consumers pump the
// event loop, route WAPI IO completion events through DispatchCompletion,
// and the token lookup fires the original callback.
//
// Dictionary<ulong, Completion> is used here rather than an arena-backed
// equivalent because these bindings ship in the vendor-neutral WAPI repo and
// must not depend on any consumer's allocator. Submission is not a hot path
// (one per async HTTP fetch / compress op), so the BCL cost is acceptable.

public static unsafe class Io
{
    // Opcodes — keep in sync with wapi.h.
    public const uint OpCapRequest      = 0x01;
    public const uint OpRoleRequest     = 0x16;
    public const uint OpRoleRepick      = 0x17;
    public const uint OpHttpFetch       = 0x060;
    public const uint OpCompressProcess = 0x140;

    // Compression algos (Flags field on OpCompressProcess).
    public const uint CompressGzip        = 0;
    public const uint CompressDeflate     = 1;
    public const uint CompressDeflateRaw  = 2;

    // Compression modes (Flags2 field).
    public const uint CompressModeCompress   = 0;
    public const uint CompressModeDecompress = 1;

    public delegate void Completion(int result, uint flags);

    // Mutable state grouped per the "nested Internal static class" convention.
    // Consumers should not access these directly; Initialize/Submit/
    // DispatchCompletion/TakeCompletionCount are the intended API.
    public static class Internal
    {
        public static WapiIo* VtableCache;
        public static Dictionary<ulong, Completion> Pending;
        public static ulong NextToken;
        public static int CompletionsThisFrame;
    }

    /// <summary>
    /// Retrieve and cache the wapi_io_t vtable. Call once at startup, after
    /// the module has loaded and before the first Submit.
    /// </summary>
    public static void Initialize()
    {
        Internal.VtableCache = Wapi.IoGet();
        Internal.Pending = new Dictionary<ulong, Completion>();
        Internal.NextToken = 1;
        Internal.CompletionsThisFrame = 0;
    }

    public static WapiIo* GetVtable() { return Internal.VtableCache; }

    /// <summary>
    /// Submit a pre-filled wapi_io_op_t. A user_data token is assigned
    /// automatically; the callback fires when the completion drains from
    /// the event queue.
    /// </summary>
    public static void Submit(WapiIoOp* op, Completion onComplete)
    {
        ulong token = Internal.NextToken++;
        Internal.Pending[token] = onComplete;
        op->UserData = token;
        WapiIo* v = Internal.VtableCache;
        v->Submit(v->Impl, op, 1);
    }

    public static void FillHttpFetch(WapiIoOp* op, byte* urlPtr, int urlLen,
                                     byte* dstPtr, int dstCap, uint method = 0)
    {
        *op = default;
        op->Opcode = OpHttpFetch;
        op->Flags  = method;
        op->Addr   = (ulong)(nuint)urlPtr;
        op->Len    = (ulong)urlLen;
        op->Addr2  = (ulong)(nuint)dstPtr;
        op->Len2   = (ulong)dstCap;
    }

    public static void FillCompress(WapiIoOp* op, byte* inPtr, int inLen,
                                    byte* outPtr, int outCap,
                                    uint algo, uint mode)
    {
        *op = default;
        op->Opcode = OpCompressProcess;
        op->Flags  = algo;
        op->Flags2 = mode;
        op->Addr   = (ulong)(nuint)inPtr;
        op->Len    = (ulong)inLen;
        op->Addr2  = (ulong)(nuint)outPtr;
        op->Len2   = (ulong)outCap;
    }

    /// <summary>
    /// Submit a batched role request. Each WapiRoleRequest carries its own
    /// OutHandle / OutResult pointers; dispatch results land per-entry.
    /// </summary>
    public static void FillRoleRequest(WapiIoOp* op, WapiRoleRequest* reqs, int count)
    {
        *op = default;
        op->Opcode = OpRoleRequest;
        op->Addr   = (ulong)(nuint)reqs;
        op->Len    = (ulong)(count * sizeof(WapiRoleRequest));
        op->Flags2 = (uint)count;
    }

    /// <summary>
    /// Ask the runtime to re-pick the endpoint behind an existing role handle.
    /// </summary>
    public static void FillRoleRepick(WapiIoOp* op, int endpointHandle, int* outHandle)
    {
        *op = default;
        op->Opcode    = OpRoleRepick;
        op->Fd        = endpointHandle;
        op->ResultPtr = (ulong)(nuint)outHandle;
    }

    /// <summary>
    /// Called by the event pump when a WAPI IO completion event is drained
    /// from the unified event queue. Looks up the callback by user_data
    /// token and invokes it.
    /// </summary>
    public static void DispatchCompletion(WapiEvent* evt)
    {
        int result  = evt->IoResult;
        uint flags  = evt->IoFlags;
        ulong token = evt->IoUserData;

        if (Internal.Pending.TryGetValue(token, out Completion cb))
        {
            Internal.Pending.Remove(token);
            cb(result, flags);
            Internal.CompletionsThisFrame++;
        }
    }

    /// <summary>
    /// Returns and resets the count of completions dispatched since the last
    /// call. Useful for scheduling fresh passes when work has landed.
    /// </summary>
    public static int TakeCompletionCount()
    {
        int n = Internal.CompletionsThisFrame;
        Internal.CompletionsThisFrame = 0;
        return n;
    }
}
