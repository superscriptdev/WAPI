# WAPI Specification

**Version 1.0.0**

**Status: Draft**

---

## Table of Contents

1. [Overview](#1-overview)
2. [Design Principles](#2-design-principles)
3. [Calling Convention](#3-calling-convention)
4. [Handle System](#4-handle-system)
5. [Error Handling](#5-error-handling)
6. [Struct Layout Rules](#6-struct-layout-rules)
7. [Import Namespaces](#7-import-namespaces)
8. [Async I/O Model](#8-async-io-model)
9. [Module Entry Points](#9-module-entry-points)
10. [Presets](#10-presets)
11. [Module Linking](#11-module-linking)
12. [Browser Integration](#12-browser-integration)
---

## 1. Overview

The WAPI defines a capability-based application binary interface that turns WebAssembly into a universal application platform. A single compiled `.wasm` binary runs unmodified on every host: native desktop, mobile, browser tab, server, edge function, or embedded device.

Where WASI and the Component Model aim for language-agnostic composability through WIT interface descriptions and a Canonical ABI that lifts and lowers complex types, the WAPI optimizes for a different point in the design space: **minimal indirection between application and host**. The ABI is a C-style calling convention document. Wasm modules import functions from well-known namespaces; the host implements them. The interface is specified entirely through C header files with explicit struct layouts and Wasm-level function signatures. This trades the Component Model's rich type system and automatic bindings generation for a thinner contract that any host can implement with no toolchain beyond a C compiler.

**Core thesis:** WebAssembly provides a universal ISA for CPU compute. The WAPI provides everything else an application needs (I/O, windowing, GPU, audio, networking) through a single stable surface that every host agrees to implement.

### 1.1 Goals

- **One binary, one ABI, every platform.** A compiled `.wasm` file runs on any conforming host without recompilation, relinking, or adaptation layers.
- **Zero abstraction tax.** The ABI maps directly to Wasm value types. No serialization, no intermediate representations, no code generation steps between the application and the host.
- **Capability-based security by default.** Modules start with zero access. Every resource is a host-validated handle. There is no ambient authority.
- **Additive complexity.** A CLI tool imports only `wapi_env` and `wapi_fs`. A game imports the full graphical stack. The module declares what it needs; the host provides exactly that.

### 1.2 Non-Goals

- **Runtime identification as the primary mechanism.** Modules should detect features via capabilities, not branch on platform strings. However, `wapi_env_host_get` provides an escape hatch for the ~10% of cases where platform knowledge is genuinely needed (workarounds, analytics, platform-appropriate UI). Prefer capability queries; use host info as a last resort.
- **Backward-compatible evolution of individual function signatures.** Functions are versioned at the module level. When a function signature must change, a new version of the capability module is introduced.
- **Replacing webgpu.h.** GPU operations use the standard `webgpu.h` API from the webgpu-native project. The WAPI provides only the bridge between its handle/surface model and the WebGPU API.

---

## 2. Design Principles

### 2.1 Capabilities All the Way Down

Every external resource (file, network connection, GPU device, audio stream, clipboard, window) is represented as an opaque `i32` handle. The host validates every handle on every call. Invalid handles produce `WAPI_ERR_BADF`.

Modules cannot fabricate handles. Handles are **granted** by the host in response to explicit requests (open a file, create a surface, request a GPU device). The default state is zero access.

This model applies uniformly:
- Filesystem access requires pre-opened directory handles granted by the host.
- Network access requires connecting to an explicit URL; the host may deny the connection based on policy.
- GPU access requires requesting a device; the host may return `WAPI_ERR_NOTCAPABLE`.
- Clipboard access requires the clipboard capability; the host may gate it on user gesture.

### 2.2 Abstracting Over Compute

The WAPI separates compute into two domains:

- **CPU compute** is handled by the WebAssembly ISA itself. The module's compiled code runs in the Wasm virtual machine. No additional abstraction is needed.
- **GPU compute** is handled by `webgpu.h`. The WAPI bridges its surface model to WebGPU but does not redefine the GPU API. Modules include `webgpu.h` directly and call standard `wgpu*` functions.

This separation means the ABI itself contains no shader languages, no pipeline state descriptions, and no buffer binding tables. All of that lives in `webgpu.h`, maintained independently by the webgpu-native project.

### 2.3 Shared Dependencies, Not Bundled Duplicates

Modules can be loaded at runtime by content hash, with the runtime caching and deduplicating common dependencies (e.g., image decoders, math libraries). Build-time linking bundles dependencies into a single binary. Runtime linking keeps modules isolated with host-mediated data transfer. This is described in detail in [Section 11: Module Linking](#11-module-linking).

---

## 3. Calling Convention

### 3.1 Value Types

All functions at the ABI boundary use only WebAssembly value types:

| Wasm Type | Size    | C Equivalent     | Usage                                    |
|-----------|---------|------------------|------------------------------------------|
| `i32`     | 4 bytes | `int32_t`        | Handles, result codes, pointers, sizes, enums, booleans, flags |
| `i64`     | 8 bytes | `int64_t`        | Timestamps, file offsets, user data      |
| `f32`     | 4 bytes | `float`          | Coordinates, scale factors, audio levels |
| `f64`     | 8 bytes | `double`         | (reserved for future use)                |

No other types cross the ABI boundary. Structs, strings, and arrays are passed by pointer (an `i32` in wasm32).

### 3.2 Pointer Representation

All pointers are `i32` values in wasm32. They reference byte offsets into the module's linear memory. The host reads and writes the module's linear memory directly to exchange structured data.

### 3.2.1 Memory64 (wasm64)

**Design principle: one struct layout for both wasm32 and wasm64.** All address/pointer fields in ABI structs are `uint64_t`, regardless of pointer width. Wasm32 modules zero-extend their 32-bit addresses into 64-bit fields. This costs a few extra bytes per struct in wasm32 but eliminates the need for two ABI profiles, two sets of struct layouts, and two code paths in every host implementation.

This applies to:
- Address fields in `wapi_io_op_t` (`addr`, `addr2`, `result_ptr`)
- Pointer fields in `wapi_string_view_t` (`data`)
- Chain pointers in `wapi_chained_struct_t` (`next`)
- Array element counts and byte lengths that may span the full address space
- Any other struct field that holds a linear memory address

Size and count fields that may need to span the full address space (array element counts, byte lengths) are `uint64_t`, matching the address field convention. Wasm32 modules zero-extend their 32-bit values. Fields that are inherently bounded (handles (`int32_t`), enums, flags) remain their natural width.

**Cross-width module linking** is supported for runtime (isolated) modules. The module-to-module boundary is handle-based: function handles (`i32`), buffer mappings, and Wasm value types. None of these depend on pointer width. The host adapts import signatures to each module's memory type independently. A wasm64 application can load a wasm32 library (or vice versa) in isolated mode with no special handling.

**Build-time shared linear memory** requires matching pointer width. Two modules linked into the same binary must both be wasm32 or both be wasm64. This is enforced at link time.

### 3.3 Return Codes

All fallible functions return `wapi_result_t` (an `i32`):

- `0` (`WAPI_OK`): success.
- Negative values: error codes (see [Section 5: Error Handling](#5-error-handling)).
- Positive values: used by some functions to return counts (e.g., `wapi_io_submit` returns the number of operations submitted).

### 3.4 Output Values

Functions that produce output values receive a **caller-provided pointer** as a parameter. The host writes the result to the pointed-to location in linear memory. For example:

```
wapi_result_t wapi_fs_read(wapi_handle_t fd, void* buf, uint64_t len, uint64_t* bytes_read);
```

At the Wasm level, this is:

```
(func $wapi_fs_read (import "wapi_fs" "read") (param i32 i32 i32 i32) (result i32))
```

The fourth parameter (`bytes_read`) is an `i32` pointer. On success, the host writes a `uint32_t` value to that address in linear memory.

### 3.5 Strings

Strings are passed as `(pointer, length)` pairs, two `i32` parameters at the Wasm level. All strings crossing the host boundary are UTF-8 encoded. There is no null-terminator requirement when an explicit length is provided. Modules are free to use any internal string encoding (UTF-16, Latin-1, etc.); the UTF-8 requirement applies only to strings passed through WAPI host imports and exports.

The C-level type is `wapi_string_view_t`:

```c
typedef struct wapi_string_view_t {
    uint64_t    data;    /* Offset 0: linear memory address of UTF-8 bytes */
    uint64_t    length;  /* Offset 8: byte count                           */
} wapi_string_view_t;
```

**Layout:** 16 bytes, alignment 8. Both fields are `uint64_t` so the layout is identical for wasm32 and wasm64. Wasm32 modules zero-extend their 32-bit values.

**Sentinel value:** When `length` equals `WAPI_STRLEN` (`UINT64_MAX`), the string is null-terminated and the host reads until the first `0x00` byte.

When strings appear as function parameters (not inside a struct), they are split into two separate Wasm parameters: `(i32 ptr, i32 len)`.

### 3.6 Structs

Structs are passed **by pointer**. The host reads and writes struct fields directly from the module's linear memory. All struct layouts are pinned at the byte level (see [Section 6: Struct Layout Rules](#6-struct-layout-rules)).

At the Wasm level, a struct pointer is a single address-width parameter. The host adapts the import signature to the module's memory type (see [Section 3.2.1](#321-memory64-wasm64)).

### 3.7 Arrays

Arrays are passed as `(pointer, count)` pairs. The pointer references the first element; the count specifies the number of elements. In struct layouts, both fields are `uint64_t` following the unified layout principle (see [Section 3.2.1](#321-memory64-wasm64)). Wasm32 modules zero-extend their 32-bit values.

---

## 4. Handle System

### 4.1 Handle Type

All host-managed resources are referenced by `wapi_handle_t`, defined as `int32_t`. Handles are opaque tokens. The module must not interpret their numeric value or perform arithmetic on them.

```c
typedef int32_t wapi_handle_t;
```

### 4.2 Invalid Handle

The value `0` is reserved as the invalid/null handle:

```c
#define WAPI_HANDLE_INVALID ((wapi_handle_t)0)
```

Functions that return handles return `WAPI_HANDLE_INVALID` on failure. Functions that accept handles reject `WAPI_HANDLE_INVALID` with `WAPI_ERR_BADF`.

### 4.3 Pre-granted Handles

The host pre-grants a small set of handles before the module's entry point is called:

| Handle | Value | Description                    |
|--------|-------|--------------------------------|
| `WAPI_STDIN`  | `1` | Standard input stream          |
| `WAPI_STDOUT` | `2` | Standard output stream         |
| `WAPI_STDERR` | `3` | Standard error stream          |

Pre-opened filesystem directories begin at handle value `4` (`WAPI_FS_PREOPEN_BASE`).

### 4.4 Handle Lifecycle

1. **Grant:** The host creates a handle and returns it to the module (e.g., `wapi_fs_open` writes a new handle to an output pointer).
2. **Use:** The module passes the handle to subsequent API calls. The host validates it on every call.
3. **Release:** The module explicitly closes or destroys the handle (e.g., `wapi_fs_close`, `wapi_surface_destroy`). After release, the handle value is invalid and must not be reused by the module.

### 4.5 Handle Validation

The host MUST validate every handle on every call. If a module passes a handle that:
- equals `WAPI_HANDLE_INVALID`,
- was never granted,
- has already been released, or
- is of the wrong type for the function,

the host MUST return `WAPI_ERR_BADF` and MUST NOT perform the requested operation.

---

## 5. Error Handling

### 5.1 Errors Are Values

Errors are `i32` return codes. There are no exceptions, no traps (for ABI errors), and no out-of-band error channels. Every fallible function returns `wapi_result_t`.

### 5.2 Checking Results

```c
#define WAPI_FAILED(result)    ((result) < 0)
#define WAPI_SUCCEEDED(result) ((result) >= 0)
```

### 5.3 Error Code Table

| Constant               | Value | Description                           |
|------------------------|-------|---------------------------------------|
| `WAPI_OK`                |   0   | Success                               |
| `WAPI_ERR_UNKNOWN`       |  -1   | Unspecified error                     |
| `WAPI_ERR_INVAL`         |  -2   | Invalid argument                      |
| `WAPI_ERR_BADF`          |  -3   | Bad handle / descriptor               |
| `WAPI_ERR_ACCES`         |  -4   | Permission denied                     |
| `WAPI_ERR_NOENT`         |  -5   | No such file or directory             |
| `WAPI_ERR_EXIST`         |  -6   | Already exists                        |
| `WAPI_ERR_NOTDIR`        |  -7   | Not a directory                       |
| `WAPI_ERR_ISDIR`         |  -8   | Is a directory                        |
| `WAPI_ERR_NOSPC`         |  -9   | No space left                         |
| `WAPI_ERR_NOMEM`         | -10   | Out of memory                         |
| `WAPI_ERR_NAMETOOLONG`   | -11   | Name too long                         |
| `WAPI_ERR_NOTEMPTY`      | -12   | Directory not empty                   |
| `WAPI_ERR_IO`            | -13   | I/O error                             |
| `WAPI_ERR_AGAIN`         | -14   | Resource temporarily unavailable      |
| `WAPI_ERR_BUSY`          | -15   | Resource busy                         |
| `WAPI_ERR_TIMEDOUT`      | -16   | Operation timed out                   |
| `WAPI_ERR_CONNREFUSED`   | -17   | Connection refused                    |
| `WAPI_ERR_CONNRESET`     | -18   | Connection reset                      |
| `WAPI_ERR_CONNABORTED`   | -19   | Connection aborted                    |
| `WAPI_ERR_NETUNREACH`    | -20   | Network unreachable                   |
| `WAPI_ERR_HOSTUNREACH`   | -21   | Host unreachable                      |
| `WAPI_ERR_ADDRINUSE`     | -22   | Address in use                        |
| `WAPI_ERR_PIPE`          | -23   | Broken pipe                           |
| `WAPI_ERR_NOTCAPABLE`    | -24   | Capability not granted                |
| `WAPI_ERR_NOTSUP`        | -25   | Operation not supported               |
| `WAPI_ERR_OVERFLOW`      | -26   | Value too large                       |
| `WAPI_ERR_CANCELED`      | -27   | Operation canceled                    |
| `WAPI_ERR_FBIG`          | -28   | File too large                        |
| `WAPI_ERR_ROFS`          | -29   | Read-only filesystem                  |
| `WAPI_ERR_RANGE`         | -30   | Result out of range                   |
| `WAPI_ERR_DEADLK`        | -31   | Deadlock would occur                  |
| `WAPI_ERR_NOSYS`         | -32   | Function not implemented              |

### 5.4 Error Messages

The host maintains a per-thread human-readable error message for the most recent error. The module can retrieve it via:

```c
wapi_result_t wapi_env_get_error(char* buf, uint64_t buf_len, uint64_t* msg_len);
```

The message is valid until the next WAPI API call on the same thread.

### 5.5 Language Bindings

Language bindings are expected to wrap `wapi_result_t` into native error types:
- **Rust:** `Result<T, WapiError>`
- **Zig:** `error.WapiError` via error unions
- **Go:** `error` interface
- **C++:** optional exception wrappers or `std::expected`

The ABI itself never throws, never traps, and never uses exceptions. Error handling at the boundary is always explicit return codes.

---

## 6. Struct Layout Rules

### 6.1 Byte Order

All multi-byte values in structs are **little-endian**. This matches the WebAssembly memory model, which defines little-endian byte order for all loads and stores.

### 6.2 Alignment

Each field is aligned to its **natural alignment**:

| Field Type  | Size    | Alignment |
|-------------|---------|-----------|
| `uint8_t`   | 1 byte  | 1 byte    |
| `uint16_t`  | 2 bytes | 2 bytes   |
| `int32_t` / `uint32_t` / `float` / pointer | 4 bytes | 4 bytes |
| `int64_t` / `uint64_t` / `double` | 8 bytes | 8 bytes |

Struct alignment equals the alignment of its most-aligned field.

### 6.3 Explicit Padding

All padding is **explicit**. Struct definitions include named `_pad` or `_reserved` fields to account for every byte. There are no implementation-defined gaps. This ensures that any conforming compiler targeting wasm32 produces identical struct layouts.

Example:

```c
typedef struct wapi_filestat_t {
    uint64_t    dev;        /* Offset  0, 8 bytes */
    uint64_t    ino;        /* Offset  8, 8 bytes */
    uint32_t    filetype;   /* Offset 16, 4 bytes */
    uint32_t    _pad0;      /* Offset 20, 4 bytes (explicit padding) */
    uint64_t    nlink;      /* Offset 24, 8 bytes */
    uint64_t    size;       /* Offset 32, 8 bytes */
    uint64_t    atim;       /* Offset 40, 8 bytes */
    uint64_t    mtim;       /* Offset 48, 8 bytes */
} wapi_filestat_t;            /* Total: 56 bytes, align 8 */
```

### 6.4 Reserved Fields for Extension

Structs include `_reserved` fields that must be set to zero by the module. Future ABI versions may assign meaning to these fields. Hosts MUST ignore reserved fields that are zero. This provides forward compatibility without changing struct sizes.

### 6.5 Forward Compatibility via `nextInChain`

Descriptor structs that may need future extension use the **chained struct** pattern from `webgpu.h`:

```c
typedef struct wapi_chained_struct_t {
    uint64_t      next;    /* Offset 0, 8 bytes (linear memory address, or 0) */
    wapi_stype_t  sType;   /* Offset 8, 4 bytes (struct type tag) */
    uint32_t      _pad;    /* Offset 12, 4 bytes */
} wapi_chained_struct_t;   /* Total: 16 bytes, align 8 */
```

Descriptor structs that support chaining have `wapi_chained_struct_t* nextInChain` as their first field. Extension structs embed `wapi_chained_struct_t` as their first field with an `sType` tag identifying the extension type.

The host walks the chain, processing known `sType` values and ignoring unknown ones. This allows new extensions to be added without changing existing struct layouts.

**Defined `sType` values:**

| Constant                          | Value    | Description                     |
|-----------------------------------|----------|---------------------------------|
| `WAPI_STYPE_INVALID`               | `0x0000` | Invalid / no type               |
| `WAPI_STYPE_WINDOW_CONFIG`             | `0x0001` | Window configuration (chained onto surface desc) |
| `WAPI_STYPE_SURFACE_DESC_FULLSCREEN` | `0x0002` | Fullscreen surface extension  |
| `WAPI_STYPE_SURFACE_DESC_RESIZABLE`  | `0x0003` | Resizable surface extension   |
| `WAPI_STYPE_GPU_SURFACE_CONFIG`      | `0x0100` | GPU surface configuration     |
| `WAPI_STYPE_AUDIO_SPATIAL_CONFIG`    | `0x0200` | Spatial audio configuration   |
| `WAPI_STYPE_TEXT_STYLE_INLINE`       | `0x0300` | Inline text style extension   |

### 6.6 Compile-Time Verification

All struct sizes AND field offsets are verified at compile time using `_Static_assert` with both `sizeof` and `offsetof`. Size-only assertions catch total-size drift but not internal layout bugs (swapped fields, wrong-sized padding that silently introduces implicit padding elsewhere). The `offsetof` assertions catch these.

Every ABI struct must have:
1. An `offsetof` assertion for **every field** (including `_pad` / `_reserved` fields).
2. A `sizeof` assertion for the total struct size.

```c
/* Example: wapi_io_op_t (80 bytes, align 8) */
_Static_assert(offsetof(wapi_io_op_t, opcode)     ==  0, "");
_Static_assert(offsetof(wapi_io_op_t, flags)      ==  4, "");
_Static_assert(offsetof(wapi_io_op_t, fd)         ==  8, "");
_Static_assert(offsetof(wapi_io_op_t, _pad0)      == 12, "");
_Static_assert(offsetof(wapi_io_op_t, offset)     == 16, "");
_Static_assert(offsetof(wapi_io_op_t, addr)       == 24, "");
_Static_assert(offsetof(wapi_io_op_t, len)        == 32, "");
_Static_assert(offsetof(wapi_io_op_t, _pad1)      == 36, "");
_Static_assert(offsetof(wapi_io_op_t, addr2)      == 40, "");
_Static_assert(offsetof(wapi_io_op_t, len2)       == 48, "");
_Static_assert(offsetof(wapi_io_op_t, flags2)     == 52, "");
_Static_assert(offsetof(wapi_io_op_t, user_data)  == 56, "");
_Static_assert(offsetof(wapi_io_op_t, result_ptr) == 64, "");
_Static_assert(offsetof(wapi_io_op_t, reserved)   == 72, "");
_Static_assert(sizeof(wapi_io_op_t) == 80, "wapi_io_op_t must be 80 bytes");

```

Address fields in ABI structs are `uint64_t` regardless of pointer width, so the layout is identical for wasm32 and wasm64 (see [Section 3.2.1](#321-memory64-wasm64)). Structs that embed C pointers (like `wapi_string_view_t`, `wapi_chained_struct_t`) use `uint64_t` for the address component, making their layout platform-independent. Assertions verify on all platforms.

The complete set of assertions is in `wapi.h` Part 7 and in each capability module header.

---

## 7. Import Namespaces

Each capability module defines a Wasm import namespace. A module's `.wasm` binary imports functions from only the namespaces it uses. The host must provide implementations for all imported functions.

| Import Module      | C Header               | Description                                    |
|--------------------|------------------------|------------------------------------------------|
| `wapi`               | `wapi_capability.h`      | Capability queries, ABI version                |
| `wapi_env`           | `wapi_env.h`             | Arguments, environment variables, random, exit |
| `wapi_memory`        | `wapi_memory.h`          | Host-provided memory allocation                |
| `wapi_io`            | `wapi_io.h`              | Async I/O (submit/cancel/poll/wait/flush)      |
| `wapi_clock`         | `wapi_clock.h`           | Monotonic and wall clocks, performance counter |
| `wapi_fs`            | `wapi_fs.h`              | Capability-based filesystem                    |
| `wapi_net`           | `wapi_net.h`             | QUIC/WebTransport networking                   |
| `wapi_gpu`           | `wapi_gpu.h`             | WebGPU bridge (device, surface, proc table)    |
| `wapi_surface`       | `wapi_surface.h`         | Render targets (on-screen and offscreen)       |
| `wapi_window`        | `wapi_window.h`          | OS window management (title, fullscreen, etc.) |
| `wapi_display`       | `wapi_display.h`         | Display enumeration, geometry, sub-pixels      |
| `wapi_input`         | `wapi_input.h`           | Input events (keyboard, mouse, touch, gamepad) |
| `wapi_audio`         | `wapi_audio.h`           | Audio playback and recording                   |
| `wapi_content`       | `wapi_content.h`         | Host-rendered text, images, media              |
| `wapi_clipboard`     | `wapi_clipboard.h`       | System clipboard access                        |
| `wapi_font`          | `wapi_font.h`            | Font system queries and enumeration            |
| `wapi_video`         | `wapi_video.h`           | Video/media playback                           |
| `wapi_geolocation`   | `wapi_geolocation.h`     | GPS / location services                        |
| `wapi_notifications` | `wapi_notifications.h`   | System notifications                           |
| `wapi_sensors`       | `wapi_sensors.h`         | Accelerometer, gyroscope, compass              |
| `wapi_speech`        | `wapi_speech.h`          | Speech recognition and synthesis               |
| `wapi_crypto`        | `wapi_crypto.h`          | Hardware-accelerated cryptography              |
| `wapi_biometric`     | `wapi_biometric.h`       | Fingerprint, face recognition                  |
| `wapi_share`         | `wapi_share.h`           | System share sheet                             |
| `wapi_kv_storage`    | `wapi_kv_storage.h`      | Persistent key-value storage                   |
| `wapi_payments`      | `wapi_payments.h`        | In-app purchases / payment processing          |
| `wapi_usb`           | `wapi_usb.h`             | USB device access                              |
| `wapi_midi`          | `wapi_midi.h`            | MIDI device access                             |
| `wapi_bluetooth`     | `wapi_bluetooth.h`       | Bluetooth device access                        |
| `wapi_camera`        | `wapi_camera.h`          | Camera capture                                 |
| `wapi_xr`            | `wapi_xr.h`              | VR/AR (WebXR, OpenXR)                          |
| `wapi_module`        | `wapi_module.h`          | Runtime module loading, cross-module calls     |

### 7.1 Import Annotation

When compiling to Wasm, functions are annotated with `import_module` and `import_name` attributes:

```c
#ifdef __wasm__
#define WAPI_IMPORT(module, name) \
    __attribute__((import_module(#module), import_name(#name)))
#else
#define WAPI_IMPORT(module, name)
#endif
```

This produces Wasm imports of the form:

```wat
(import "wapi_fs" "open" (func ...))
(import "wapi_io" "poll" (func ...))
```

---

## 8. Async I/O Model

### 8.1 Design

The WAPI uses a **submit + unified event queue** async I/O model inspired by Linux's `io_uring`. All asynchronous operations (file reads, network sends, audio buffer fills, timer waits) are submitted through host imports. Completions arrive as events in the same unified event queue as input, window, and lifecycle events.

There is no `async`/`await`. There are no colored functions. There are no new types for futures or promises. The module submits operation descriptors via `wapi_io_submit()`, does other work, and then polls or waits for completion events via `wapi_io_poll()` / `wapi_io_wait()`.

This eliminates the function coloring problem for library composition. Because the I/O primitive is always the same (submit descriptors, poll for completions), there is no split between "sync" and "async" libraries. Every library uses the same interface regardless of how it structures its internal control flow. A library that batches many operations before polling and a library that submits one at a time compose freely because neither imposes a calling convention on the other. The `wapi_io_t` vtable (see below) extends this to build-time linked libraries: a library that accepts `wapi_io_t*` works identically whether backed by the real host, a logging wrapper, a mock, or a throttler.

### 8.2 I/O Host Imports

I/O is accessed through direct host imports in the `wapi_io` namespace:

```c
WAPI_IMPORT(wapi_io, submit)
int32_t wapi_io_submit(const wapi_io_op_t* ops, uint64_t count);

WAPI_IMPORT(wapi_io, cancel)
wapi_result_t wapi_io_cancel(uint64_t user_data);

WAPI_IMPORT(wapi_io, poll)
int32_t wapi_io_poll(wapi_event_t* event);

WAPI_IMPORT(wapi_io, wait)
int32_t wapi_io_wait(wapi_event_t* event, int32_t timeout_ms);

WAPI_IMPORT(wapi_io, flush)
void wapi_io_flush(uint32_t event_type);
```

All events (I/O completions, input, lifecycle) are delivered through `poll`/`wait`. The host knows which module is calling and routes accordingly.

I/O access follows the same dependency injection pattern as memory allocation. WAPI defines a `wapi_io_t` vtable that libraries and modules accept as an explicit parameter. The caller controls what I/O the callee can do by choosing what to pass. This applies uniformly to both build-time and runtime composition (see [Section 11: Module Linking](#11-module-linking)):

```c
static int32_t logging_submit(void* impl, const wapi_io_op_t* ops, uint64_t count) {
    log_state_t* state = (log_state_t*)impl;
    log("submitting %d ops", count);
    return state->inner->submit(state->inner->impl, ops, count);
}
```

### 8.3 Operation Descriptor (`wapi_io_op_t`)

Each operation is described by a fixed-size **88-byte descriptor**. Address and size fields are `uint64_t` so the layout is identical for wasm32 and wasm64 (wasm32 modules zero-extend their 32-bit values):

```
Byte Layout (88 bytes, alignment 8):

Offset  Size  Type      Field        Description
------  ----  --------  -----------  -----------------------------------
 0       4    uint32_t  opcode       Operation type (wapi_io_opcode_t)
 4       4    uint32_t  flags        Operation flags
 8       4    int32_t   fd           Handle / file descriptor
12       4    uint32_t  _pad0        Padding (must be zero)
16       8    uint64_t  offset       File offset or timeout (nanoseconds)
24       8    uint64_t  addr         Pointer to buffer
32       8    uint64_t  len          Buffer length
40       8    uint64_t  addr2        Second pointer (path, etc.)
48       8    uint64_t  len2         Second length
56       4    uint32_t  flags2       Additional operation-specific flags
60       4    uint32_t  _pad1        Padding (must be zero)
64       8    uint64_t  user_data    Opaque, echoed in completion event
72       8    uint64_t  result_ptr   Pointer for output values
80       8    uint8_t[] reserved     Reserved (must be zero)
```

### 8.4 Operation Codes

| Opcode                  | Value | Category    | Description                         |
|-------------------------|-------|-------------|-------------------------------------|
| `WAPI_IO_OP_NOP`          |   0   | Control     | No operation (fence)                |
| `WAPI_IO_OP_READ`         |   1   | Filesystem  | Read from fd into buffer            |
| `WAPI_IO_OP_WRITE`        |   2   | Filesystem  | Write buffer to fd                  |
| `WAPI_IO_OP_OPEN`         |   3   | Filesystem  | Open a file                         |
| `WAPI_IO_OP_CLOSE`        |   4   | Filesystem  | Close a handle                      |
| `WAPI_IO_OP_STAT`         |   5   | Filesystem  | Stat a file                         |
| `WAPI_IO_OP_LOG`          |   6   | Core        | Log message (fire-and-forget)       |
| `WAPI_IO_OP_CONNECT`      |  10   | Network     | Open a connection                   |
| `WAPI_IO_OP_ACCEPT`       |  11   | Network     | Accept a connection                 |
| `WAPI_IO_OP_SEND`         |  12   | Network     | Send data                           |
| `WAPI_IO_OP_RECV`         |  13   | Network     | Receive data                        |
| `WAPI_IO_OP_TIMEOUT`      |  20   | Timer       | Wait for a duration                 |
| `WAPI_IO_OP_TIMEOUT_ABS`  |  21   | Timer       | Wait until an absolute time         |
| `WAPI_IO_OP_AUDIO_WRITE`  |  30   | Audio       | Submit audio samples for playback   |
| `WAPI_IO_OP_AUDIO_READ`   |  31   | Audio       | Read captured audio samples         |

**`WAPI_IO_OP_LOG` field mapping:**

| Field       | Usage                                                    |
|-------------|----------------------------------------------------------|
| `flags`     | Log level: `WAPI_LOG_DEBUG` (0), `WAPI_LOG_INFO` (1), `WAPI_LOG_WARN` (2), `WAPI_LOG_ERROR` (3) |
| `addr/len`  | UTF-8 message text                                       |
| `addr2/len2`| Optional tag/category (e.g. "render", "audio"); 0/0 if none |

Log is fire-and-forget — the host processes it immediately (writes to stderr, forwards to console, etc.). No completion event is pushed. `user_data` is ignored.

### 8.5 Completion Events (`wapi_io_event_t`)

I/O completions arrive as `WAPI_EVENT_IO_COMPLETION` events in the unified event queue. The event struct occupies 32 bytes within the 128-byte `wapi_event_t` union:

```
Byte Layout (32 bytes within wapi_event_t):

Offset  Size  Type      Field        Description
------  ----  --------  ----------   -----------------------------------
 0       4    uint32_t  type         WAPI_EVENT_IO_COMPLETION (0x2000)
 4       4    uint32_t  surface_id   Always 0 for I/O events
 8       8    uint64_t  timestamp    When the completion was posted
16       4    int32_t   result       Bytes transferred or negative error
20       4    uint32_t  flags        Completion flags
24       8    uint64_t  user_data    Echoed from the submitted operation
```

**Completion flags:**

| Flag                  | Value    | Description                               |
|-----------------------|----------|-------------------------------------------|
| `WAPI_IO_CQE_F_MORE`   | `0x0001` | More completions coming for this operation |
| `WAPI_IO_CQE_F_OVERFLOW`| `0x0002`| Completion queue overflowed               |

### 8.6 Submit and Cancel

Operations are submitted through the `wapi_io` host imports:

```c
/* Submit a batch of operations. Returns count submitted, or negative error.
 * Completions arrive as WAPI_EVENT_IO_COMPLETION events. */
int32_t count = wapi_io_submit(ops, n);

/* Cancel a pending operation by user_data. */
wapi_result_t result = wapi_io_cancel(user_data);
```

There is no separate I/O completion queue. All completions flow through the unified event queue via `wapi_io_poll()` and `wapi_io_wait()`, alongside input, window, and lifecycle events.

### 8.7 Usage Patterns

The same API supports fundamentally different application architectures:

- **Game loop:** Submit asset loads, render a frame, poll events at the end of the frame — I/O completions and input arrive in the same queue.
- **CLI tool:** Submit a file read, call `wapi_io_wait()` to block until the I/O completion event arrives.
- **Audio processor:** Poll events every audio callback cycle for completed buffer fills.
- **Server:** Submit accepts and receives, wait for completion events in a loop.

---

## 9. Module Entry Points

### 9.1 `wapi_main`

```wat
(func (export "wapi_main") (result i32))
```

**C signature:**

```c
WAPI_EXPORT(wapi_main) wapi_result_t wapi_main(void);
```

Called by the host after module instantiation and memory initialization. There is no context struct. The module starts with zero state and obtains everything through host imports:

- **Capabilities** via `wapi_capability_supported` or enumerate with `wapi_capability_count` / `wapi_capability_name`.
- **I/O** via host imports directly: `wapi_io_submit()`, `wapi_io_poll()`, `wapi_io_wait()`.
- **Allocation** via host imports (`wapi_memory.alloc` etc.) or module-internal allocators.
- **GPU** via `wapi_gpu_request_device()`.
- **Panic** via the `wapi_panic_report` host import.

This separation supports two worlds of module composition:

- **Build-time linked libraries** (shared memory, one binary) take allocator/I/O vtables as explicit function parameters, like Zig's allocator pattern.
- **Runtime isolated modules** receive allocator/I/O vtables via function handle calls from the parent. The parent controls what the child can do by choosing what to pass.

The module should:

1. Query capabilities via `wapi_capability_supported` or enumerate with `wapi_capability_count` / `wapi_capability_name`.
2. Use `wapi_io_*` imports for I/O, `wapi_memory.*` imports for allocation.
3. For headless applications: perform work and return `WAPI_OK` to exit.
4. For graphical applications: create surfaces, request GPU device, and return `WAPI_OK` to begin the frame loop.

**Panic reporting:** When a module encounters an unrecoverable error, it calls the `wapi_panic_report` host import to record the message, then traps. The import is NOT noreturn — it records and returns; the module then hits `__builtin_trap()` (`unreachable` in wasm). The host knows which module is calling and can route the message to stderr, console, or the parent's handler.

When a parent module calls a child via `wapi_module_call` and the child traps, the runtime catches the trap and returns an error to the parent (not a trap). The child module is faulted and should be released.

```c
static inline _Noreturn void wapi_panic(const char* msg, uint64_t msg_len) {
    wapi_panic_report(msg, msg_len);
    __builtin_trap();
}
```

Returns `WAPI_OK` on success, or a negative error code to abort the module.

### 9.2 `wapi_frame`

```wat
(func (export "wapi_frame") (param i64) (result i32))
```

**C signature:**

```c
WAPI_EXPORT(wapi_frame) wapi_result_t wapi_frame(wapi_timestamp_t timestamp);
```

Called each frame for graphical applications. The host calls this at the display's refresh rate (typically 60 Hz, 120 Hz, or 144 Hz). The `timestamp` parameter is the current monotonic time in nanoseconds.

The module should:

1. Poll events via `wapi_io_poll()` (input, I/O completions, lifecycle).
2. Update application state.
3. Render via WebGPU.
4. Return `WAPI_OK` to continue, or `WAPI_ERR_CANCELED` to request exit.

If the module does not export `wapi_frame`, the host does not enter a frame loop. This is the expected behavior for headless applications.

---

## 10. Capabilities

### 10.1 Unified Capability Model

All capabilities, whether foundational (memory, filesystem) or specialized (geolocation, camera), use a single, unified query mechanism. There is no distinction between "core" and "extension" capabilities. This follows the **Vulkan model**: query at startup by string name, use only if the host reports support. Each capability is independently versioned.

Capability names are **dot-separated strings** following a hierarchical namespace:

```
wapi.<module>          Spec-defined capabilities
vendor.<name>.*      Vendor-specific capabilities
```

### 10.2 Capability Query API

```c
/* Check if a capability is supported by name. Returns boolean. */
wapi_bool_t wapi_capability_supported(const char* name, uint64_t name_len);

/* Get the version of a capability. Writes wapi_version_t to output pointer. */
wapi_result_t wapi_capability_version(const char* name, uint64_t name_len, wapi_version_t* version);

/* Get the total count of supported capabilities. */
int32_t wapi_capability_count(void);

/* Get the name of a capability by index. Writes name string to buffer. */
wapi_result_t wapi_capability_name(int32_t index, char* buf, uint64_t buf_len, uint64_t* name_len);

/* Get the ABI version. Writes wapi_version_t to output pointer. */
wapi_result_t wapi_abi_version(wapi_version_t* version);
```

At the Wasm level:

```wat
(func $wapi_capability_supported (import "wapi" "capability_supported") (param i32 i32) (result i32))
(func $wapi_capability_version   (import "wapi" "capability_version")   (param i32 i32 i32) (result i32))
(func $wapi_capability_count     (import "wapi" "capability_count")     (result i32))
(func $wapi_capability_name      (import "wapi" "capability_name")      (param i32 i32 i32 i32) (result i32))
(func $wapi_abi_version          (import "wapi" "abi_version")          (param i32) (result i32))
```

### 10.3 Spec-Defined Capability Names

The following capability names are defined by the spec. Hosts report support for each independently:

| Capability Name        | Import Module      | Description                                    |
|------------------------|--------------------|------------------------------------------------|
| `wapi.env`               | `wapi_env`           | Arguments, environment variables, random, exit |
| `wapi.memory`            | `wapi_memory`        | Host-provided memory allocation                |
| `wapi.io`                | `wapi_io`            | Async I/O (submit/cancel/poll/wait/flush)      |
| `wapi.clock`             | `wapi_clock`         | Monotonic and wall clocks, performance counter |
| `wapi.fs`                | `wapi_fs`            | Capability-based filesystem                    |
| `wapi.net`               | `wapi_net`           | QUIC/WebTransport networking                   |
| `wapi.gpu`               | `wapi_gpu`           | WebGPU bridge (device, surface, proc table)    |
| `wapi.surface`           | `wapi_surface`       | Render targets (on-screen and offscreen)       |
| `wapi.window`            | `wapi_window`        | OS window management (title, fullscreen, etc.) |
| `wapi.display`           | `wapi_display`       | Display enumeration, geometry, sub-pixels      |
| `wapi.input`             | `wapi_input`         | Input events (keyboard, mouse, touch, gamepad) |
| `wapi.audio`             | `wapi_audio`         | Audio playback and recording                   |
| `wapi.content`           | `wapi_content`       | Host-rendered text, images, media              |
| `wapi.clipboard`         | `wapi_clipboard`     | System clipboard access                        |
| `wapi.font`              | `wapi_font`          | Font system queries and enumeration            |
| `wapi.video`             | `wapi_video`         | Video/media playback                           |
| `wapi.geolocation`       | `wapi_geolocation`   | GPS / location services                        |
| `wapi.notifications`     | `wapi_notifications` | System notifications                           |
| `wapi.sensors`           | `wapi_sensors`       | Accelerometer, gyroscope, compass              |
| `wapi.speech`            | `wapi_speech`        | Speech recognition and synthesis               |
| `wapi.crypto`            | `wapi_crypto`        | Hardware-accelerated cryptography              |
| `wapi.biometric`         | `wapi_biometric`     | Fingerprint, face recognition                  |
| `wapi.share`             | `wapi_share`         | System share sheet                             |
| `wapi.kv_storage`        | `wapi_kv_storage`    | Persistent key-value storage                   |
| `wapi.payments`          | `wapi_payments`      | In-app purchases / payment processing          |
| `wapi.usb`               | `wapi_usb`           | USB device access                              |
| `wapi.midi`              | `wapi_midi`          | MIDI device access                             |
| `wapi.bluetooth`         | `wapi_bluetooth`     | Bluetooth device access                        |
| `wapi.camera`            | `wapi_camera`        | Camera capture                                 |
| `wapi.xr`               | `wapi_xr`            | VR/AR (WebXR, OpenXR)                          |
| `wapi.module`            | `wapi_module`        | Runtime module loading, cross-module calls     |

### 10.4 Vendor Capabilities

Vendors may define custom capabilities using the `vendor.<vendor_name>.*` namespace. For example:

- `vendor.nintendo.joycon` Nintendo Joy-Con haptics
- `vendor.apple.pencil` Apple Pencil force/tilt data
- `vendor.valve.steamdeck` Steam Deck-specific features

Vendor capabilities use the same `wapi_capability_supported` / `wapi_capability_version` query mechanism. Hosts MUST ignore vendor capability queries they do not recognize (returning false from `wapi_capability_supported`).

**Constraints on vendor capabilities:**

- Vendor capabilities MUST NOT duplicate functionality that has a spec-defined equivalent. If `wapi.gpu` provides a feature, a vendor MUST NOT redefine it under `vendor.<name>.gpu`.
- Modules MAY require vendor capabilities (e.g., a Joy-Con haptics demo that only makes sense on Nintendo hardware). There is no requirement for fallback paths. A module that requires a capability simply will not run on hosts that lack it. The capability query mechanism ensures the module can detect this at startup and report a clear error.
- Frequently-used vendor capabilities SHOULD be considered for promotion to spec-defined capabilities in future versions.

### 10.5 Capability Versioning

Each capability reports its own version via `wapi_capability_version`. This allows the module to detect the specific feature level within a capability:

```c
wapi_version_t v;
if (wapi_capability_supported("wapi.geolocation", 16)) {
    wapi_capability_version("wapi.geolocation", 16, &v);
    /* v.major, v.minor, v.patch */
}
```

### 10.6 Presets

Presets are convenience arrays of capability name strings that give developers a stable target. A host claims conformance to a preset by supporting all capabilities in the array. Presets are not exclusive; a host may support additional capabilities beyond its preset.

**Preset Definitions:**

```c
static const char* WAPI_PRESET_HEADLESS[] = {
    "wapi.env", "wapi.memory", "wapi.io", "wapi.clock", "wapi.fs", "wapi.net",
    NULL
};

static const char* WAPI_PRESET_COMPUTE[] = {
    "wapi.env", "wapi.memory", "wapi.io", "wapi.clock", "wapi.fs", "wapi.net",
    "wapi.gpu",
    NULL
};

static const char* WAPI_PRESET_AUDIO[] = {
    "wapi.env", "wapi.memory", "wapi.io", "wapi.clock", "wapi.fs", "wapi.net",
    "wapi.audio",
    NULL
};

static const char* WAPI_PRESET_GRAPHICAL[] = {
    "wapi.env", "wapi.memory", "wapi.io", "wapi.clock", "wapi.fs", "wapi.net",
    "wapi.gpu", "wapi.surface", "wapi.window", "wapi.display", "wapi.input",
    "wapi.audio", "wapi.content", "wapi.clipboard", "wapi.font",
    NULL
};

static const char* WAPI_PRESET_MOBILE[] = {
    "wapi.env", "wapi.memory", "wapi.io", "wapi.clock", "wapi.fs", "wapi.net",
    "wapi.gpu", "wapi.surface", "wapi.window", "wapi.display", "wapi.input",
    "wapi.audio", "wapi.content", "wapi.clipboard", "wapi.font",
    "wapi.geolocation", "wapi.sensors",
    "wapi.notifications", "wapi.biometric", "wapi.camera", "wapi.share",
    NULL
};
```

**Preset summary:**

| Preset       | Includes                                      | Use Cases                                  |
|--------------|-----------------------------------------------|--------------------------------------------|
| **Headless** | env, memory, io, clock, fs, net               | Servers, CLI tools, edge functions, IoT    |
| **Compute**  | Headless + gpu                                | ML inference, video transcoding, simulations |
| **Audio**    | Headless + audio                              | VST plugins, audio processing, voice       |
| **Graphical**| Headless + gpu, surface, window, display, input, audio, content, clipboard, font | Apps, games, creative tools |
| **Mobile**   | Graphical + geolocation, sensors, notifications, biometric, camera, share | Mobile apps |

**Checking preset conformance:**

The `wapi_preset_supported` inline helper iterates the preset array and checks each capability:

```c
static inline wapi_bool_t wapi_preset_supported(const char** preset) {
    for (int i = 0; preset[i] != NULL; i++) {
        const char* name = preset[i];
        uint64_t len = 0;
        while (name[len]) len++;
        if (!wapi_capability_supported(name, len)) {
            return 0;
        }
    }
    return 1;
}
```

Usage:

```c
if (wapi_preset_supported(WAPI_PRESET_GRAPHICAL)) {
    /* Host supports the full graphical stack */
}
```

---

## 11. Module Linking

### 11.1 Hybrid Memory Model

Each module has two memories:

- **Memory 0 (private):** The module's own linear memory. Stack, globals, internal state. Fully isolated — no other module can access it. Allocated via `wapi_mem_alloc`.
- **Memory 1 (shared):** A single shared memory owned by the application. All loaded child modules share the same memory 1 instance. The application always has full access to all of shared memory — it owns it. Child modules can only access regions they allocated themselves or were explicitly lent via `wapi_module_lend`.

**Build-time linking** (one binary): Libraries are compiled into a single `.wasm` module. They share memory 0 naturally. Pass pointers, call functions directly. Pass allocator and I/O vtables as explicit function parameters (Zig-style). No WAPI mechanism needed.

**Runtime linking** (separate modules): Each module has its own memory 0. Data exchange uses shared memory (memory 1) with the borrow system for zero-copy access, or explicit copies via `wapi_module_copy_in` for simple cases. Two access paths for shared memory:
- **Multi-memory:** Import memory 1, load/store directly (zero copy).
- **Host-call:** `wapi_module_shared_read` / `wapi_module_shared_write` (one copy, portable C).

The dependency injection pattern is the same in both modes. A library accepts `wapi_allocator_t` for memory and `wapi_io_t` for I/O. In build-time linking these are pointers into shared memory. In runtime linking the parent provides function handles that the child calls. A child module starts with zero I/O access and only gets what the parent explicitly passes.

### 11.2 Build-Time Linking

Build-time linked libraries are simply part of the same Wasm binary. They share linear memory and can call each other's functions directly. WAPI defines vtable types (`wapi_allocator_t`, `wapi_io_t`, `wapi_panic_handler_t`) that libraries accept as explicit parameters for dependency injection, following Zig's allocator pattern:

```c
// A library function that accepts an explicit allocator
image_t* decode_png(const uint8_t* data, size_t len, const wapi_allocator_t* alloc);

// A library function that accepts an explicit I/O interface
void http_fetch(const char* url, const wapi_io_t* io, callback_t cb);

// A library that needs both
void download_and_decode(const char* url, const wapi_io_t* io, const wapi_allocator_t* alloc);
```

This is a convention, not an ABI requirement. Libraries choose their own parameter patterns. The key principle is: if a function does I/O, it takes a `wapi_io_t*`. If it allocates, it takes a `wapi_allocator_t*`. The caller always controls the callee's capabilities.

### 11.3 Runtime Module Identity

Runtime modules are identified by the SHA-256 hash of their Wasm binary. The hash IS the identity. Name and version are human-readable metadata, not the linking key.

```c
wapi_module_hash_t hash = { /* SHA-256 bytes */ };
wapi_handle_t module;
wapi_module_load(&hash, WAPI_STR("https://registry.example.com/image-decoder-1.2.0.wasm"), &module);
```

The URL is a fetch hint, not an identity. Two calls with different URLs but the same hash produce the same module.

### 11.4 Cross-Module Calling

To call functions on a runtime module, the caller uses `wapi_module_call`. Data is exchanged via shared memory with borrows (zero-copy) or via explicit copies:

**Input via shared memory (zero-copy):**

```c
// 1. Allocate in shared memory and write data
uint64_t off = wapi_module_shared_alloc(png_len, 4);
wapi_module_shared_write(off, png_data, png_len);

// 2. Lend to child module
wapi_handle_t borrow;
wapi_module_lend(module, off, WAPI_LEND_READ, &borrow);

// 3. Call the function
wapi_handle_t func;
wapi_module_get_func(module, WAPI_STR("decode"), &func);
wapi_val_t args[] = {
    { .kind = WAPI_VAL_I32, .of.i32 = off },
    { .kind = WAPI_VAL_I32, .of.i32 = png_len },
};
wapi_val_t result;
wapi_module_call(module, func, args, 2, &result, 1);

// 4. Reclaim borrow and free shared memory
wapi_module_reclaim(borrow);
wapi_module_shared_free(off);
```

**Child output (child allocates, app reads directly):**

```c
// Child's decode function allocates output in shared memory and
// returns the offset. The application reads it directly — no
// borrow needed, because the application owns shared memory.
uint64_t result_off = result.of.i32;
uint64_t result_len = wapi_module_shared_usable_size(result_off);

// Read the decoded pixels into private memory
void* pixels = wapi_mem_alloc(result_len, 16);
wapi_module_shared_read(result_off, pixels, result_len);

// Free the child's shared allocation when done
wapi_module_shared_free(result_off);
```

**Explicit copy (simple cases):**

```c
// 1. Copy data into child's private memory
uint32_t child_ptr;
wapi_module_copy_in(module, config_data, config_len, &child_ptr);

// 2. Call the function
wapi_val_t args[] = {
    { .kind = WAPI_VAL_I32, .of.i32 = child_ptr },
    { .kind = WAPI_VAL_I32, .of.i32 = config_len },
};
wapi_module_call(module, func, args, 2, &result, 1);
```

### 11.5 Shared Memory Ownership

The application owns shared memory (memory 1) and always has full access to every offset. Child modules have restricted access:

- A child can access regions it **allocated itself** (via `wapi_module_shared_alloc`).
- A child can access regions **explicitly lent** to it (via `wapi_module_lend`).
- All other shared memory is inaccessible to the child.

The application can read, write, free, and lend **any** region — regardless of who allocated it. A child can only free and lend its own allocations. Freeing a region with active borrows fails (`WAPI_ERR_BUSY`) — reclaim all borrows first.

### 11.6 Borrow System

The borrow system controls child access to shared memory regions the child did not allocate. The application does not need borrows — it always has full access.

The application can lend any region (including regions allocated by a child). A child can only lend regions it allocated itself.

Borrow rules follow reader-writer lock semantics per region:
- `WAPI_LEND_READ`: borrower gets read-only access; lender retains read. Multiple concurrent READ borrows to different modules are allowed.
- `WAPI_LEND_WRITE`: borrower gets exclusive read-write; lender loses access until reclaim. No concurrent borrows on the same region.

This enables concurrent module execution on different regions: module B reads region R1 while module C writes region R3. The developer controls granularity through allocation pattern — smaller allocations mean finer-grained concurrency.

**Child output pattern:** The child allocates in shared memory, writes its result, and returns the offset as a return value. The application reads it directly — no borrow or ownership transfer needed. The application can then lend this data to another module for pipeline processing (A→B→C) without any copies.

### 11.7 I/O for Runtime Modules

Runtime modules do not get implicit `wapi_io` host imports. By default, a child module has zero I/O access. The parent grants I/O by passing a `wapi_io_t` vtable (as function handles) when calling into the child:

```c
// Parent creates function handles for the I/O vtable
wapi_handle_t io_submit_fn, io_poll_fn, io_wait_fn;
wapi_module_export_func(parent, WAPI_STR("my_submit_wrapper"), &io_submit_fn);

// Pass the I/O handles as arguments when calling the child
wapi_val_t args[] = {
    { .kind = WAPI_VAL_I32, .of.i32 = io_submit_fn },
    { .kind = WAPI_VAL_I32, .of.i32 = io_poll_fn },
    // ... data args
};
wapi_module_call(module, func, args, count, &result, 1);
```

The child uses whatever I/O interface the parent provides. The parent can pass the real host I/O, a restricted wrapper, a logging layer, or a mock. This is the same pattern as `wapi_allocator_t` and `wapi_io_t` in build-time linking, applied across module boundaries via function handles instead of pointers.

### 11.8 Semver for ABI Contracts

Module versions follow **semantic versioning**:

- **Major version** change: breaking ABI change. The runtime must not substitute a different major version.
- **Minor version** change: backward-compatible additions. The runtime may substitute a higher minor version within the same major version.
- **Patch version** change: bug fix. The runtime may substitute a higher patch version.

### 11.9 Module Cache

The runtime maintains a cache of modules keyed by content hash. Analogous to the browser's HTTP cache or a Nix store. Modules can be pre-fetched (`wapi_module_prefetch`) for background download.

---

## 12. Browser Integration

### 12.1 Architecture

In the browser, a JavaScript shim implements the WAPI against Web APIs. The shim is a standard JavaScript module that:

1. Instantiates the `.wasm` binary via `WebAssembly.instantiate`.
2. Provides import implementations that delegate to Web APIs.
3. Manages the frame loop via `requestAnimationFrame`.

### 12.2 API Mapping

| WAPI Module         | Web API                                                      |
|-------------------|--------------------------------------------------------------|
| `wapi_env`          | `location.search`, Web Crypto `getRandomValues`              |
| `wapi_memory`       | Direct linear memory management                              |
| `wapi_io`           | Internal microtask queue                                     |
| `wapi_clock`        | `performance.now()`, `Date.now()`                            |
| `wapi_fs`           | Origin Private File System (OPFS)                            |
| `wapi_net`          | `fetch`, `WebSocket`, `WebTransport`                         |
| `wapi_gpu`          | `navigator.gpu` (WebGPU)                                     |
| `wapi_surface`      | `<canvas>` element, fullscreen API                           |
| `wapi_input`        | DOM events (`keydown`, `mousemove`, `pointerdown`, `gamepadconnected`) |
| `wapi_audio`        | Web Audio API (`AudioContext`, `AudioWorklet`)               |
| `wapi_content`      | DOM overlay elements, Canvas 2D, `OffscreenCanvas`           |
| `wapi_clipboard`    | Clipboard API (`navigator.clipboard`)                        |
| `wapi_font`         | CSS Font Loading API, `document.fonts`                       |
| `wapi_video`        | `<video>` element, Media Source Extensions                   |
| `wapi_geolocation`  | Geolocation API (`navigator.geolocation`)                    |
| `wapi_notifications`| Notifications API (`Notification`)                           |
| `wapi_sensors`      | Generic Sensor API (`Accelerometer`, `Gyroscope`)            |
| `wapi_camera`       | `getUserMedia`, MediaStream API                              |

### 12.3 Surface Model in Browser

Each browser tab renders a single fullscreen `<canvas>` element. The `wapi_surface_create` call creates or reconfigures this canvas. Requests for multiple surfaces are either mapped to a single canvas (with host-managed compositing) or rejected based on browser policy.

Surface descriptors like `width`, `height`, and `flags` are best-effort. The browser may ignore size requests and always use the viewport dimensions. The module should respond to `WAPI_SURFACE_EVENT_RESIZED` events rather than assuming the requested size was honored.

### 12.4 Coexistence with Traditional Web Content

The WAPI does not replace HTML, CSS, or JavaScript. Traditional websites continue to work unchanged. The ABI provides an alternative application model for software that benefits from a single portable binary: games, creative tools, productivity applications, simulations.

A web page may embed a WAPI module alongside traditional web content using an `<iframe>` or a dedicated container element.

---

## Appendix A: Version History

| Version | Date       | Description          |
|---------|------------|----------------------|
| 1.0.0   | 2026-04-02 | Initial specification |

## Appendix B: Module Exports Summary

A conforming WAPI module MUST export:

| Export Name | Wasm Signature     | Required | Description                         |
|-------------|--------------------|----------|-------------------------------------|
| `wapi_main`   | `() -> i32`        | Yes      | Module entry point |
| `wapi_frame`  | `(i64) -> i32`     | No       | Per-frame callback (graphical apps) |
| `memory`    | `(memory ...)`     | Yes      | Linear memory (for host access)     |

## Appendix C: Struct Size Summary

| Struct                     | Size (bytes) | Alignment |
|----------------------------|-------------|-----------|
| `wapi_subpixel_t`             |     4       |    1      |
| `wapi_string_view_t`         |    16       |    8      |
| `wapi_chained_struct_t`      |    16       |    8      |
| `wapi_version_t`             |     8       |    2      |
| `wapi_layout_constraints_t`  |     8       |    4      |
| `wapi_panic_handler_t`       |     8       |    4      |
| `wapi_audio_spec_t`          |    12       |    4      |
| `wapi_allocator_t`           |    16       |    4      |
| `wapi_layout_result_t`       |    16       |    4      |
| `wapi_text_run_t`            |    16       |    4      |
| `wapi_val_t`                 |    16       |    8      |
| `wapi_hit_test_result_t`     |    16       |    4      |
| `wapi_window_config_t`       |    40       |    8      |
| `wapi_net_listen_desc_t`     |    20       |    4      |
| `wapi_io_t`                  |    24       |    4      |
| `wapi_dirent_t`              |    24       |    8      |
| `wapi_net_connect_desc_t`    |    24       |    4      |
| `wapi_surface_desc_t`        |    24       |    4      |
| `wapi_gpu_surface_config_t`  |    24       |    4      |
| `wapi_caret_info_t`          |    24       |    4      |
| `wapi_text_line_info_t`      |    28       |    4      |
| `wapi_io_event_t`            |    32       |    4      |
| `wapi_image_extended_t`      |    32       |    4      |
| `wapi_module_hash_t`         |    32       |    1      |
| `wapi_text_style_t`          |    40       |    4      |
| `wapi_display_info_t`        |    48       |    4      |
| `wapi_filestat_t`            |    56       |    8      |
| `wapi_io_op_t`               |    80       |    8      |
| `wapi_event_t`               |   128       |    8      |

---

*End of specification.*
