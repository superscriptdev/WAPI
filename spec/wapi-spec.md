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
11. [Shared Module Linking](#11-shared-module-linking)
12. [Browser Integration](#12-browser-integration)
13. [Complete Function Reference](#13-complete-function-reference)

---

## 1. Overview

The WAPI defines a capability-based application binary interface that turns WebAssembly into a universal application platform. A single compiled `.wasm` binary runs unmodified on every host: native desktop, mobile, browser tab, server, edge function, or embedded device.

The ABI is a **C-style calling convention document**. Wasm modules import functions from well-known namespaces; the host implements them. There is no WIT, no IDL, no Canonical ABI with lifting and lowering. The interface is specified entirely through C header files with explicit struct layouts and Wasm-level function signatures.

**Core thesis:** WebAssembly provides a universal ISA for CPU compute. The WAPI provides everything else an application needs -- I/O, windowing, GPU, audio, networking -- through a single stable surface that every host agrees to implement.

### 1.1 Goals

- **One binary, one ABI, every platform.** A compiled `.wasm` file runs on any conforming host without recompilation, relinking, or adaptation layers.
- **Zero abstraction tax.** The ABI maps directly to Wasm value types. No serialization, no intermediate representations, no code generation steps between the application and the host.
- **Capability-based security by default.** Modules start with zero access. Every resource is a host-validated handle. There is no ambient authority.
- **Additive complexity.** A CLI tool imports only `wapi_env` and `wapi_fs`. A game imports the full graphical stack. The module declares what it needs; the host provides exactly that.

### 1.2 Non-Goals

- **Runtime identification.** The ABI provides no function to query what operating system, browser, or device the module is running on. Modules detect features, not platforms.
- **Backward-compatible evolution of individual function signatures.** Functions are versioned at the module level. When a function signature must change, a new version of the capability module is introduced.
- **Replacing webgpu.h.** GPU operations use the standard `webgpu.h` API from the webgpu-native project. The WAPI provides only the bridge between its handle/surface model and the WebGPU API.

---

## 2. Design Principles

### 2.1 Capabilities All the Way Down

Every external resource -- file, network connection, GPU device, audio stream, clipboard, window -- is represented as an opaque `i32` handle. The host validates every handle on every call. Invalid handles produce `WAPI_ERR_BADF`.

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

Modules declare imports by namespace and version. The runtime resolves shared modules from a cache, deduplicating common dependencies (e.g., libc, image decoders, math libraries). This is described in detail in [Section 11: Shared Module Linking](#11-shared-module-linking).

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

### 3.3 Return Codes

All fallible functions return `wapi_result_t` (an `i32`):

- `0` (`WAPI_OK`): success.
- Negative values: error codes (see [Section 5: Error Handling](#5-error-handling)).
- Positive values: used by some functions to return counts (e.g., `ctx->io->submit` returns the number of operations submitted).

### 3.4 Output Values

Functions that produce output values receive a **caller-provided pointer** as a parameter. The host writes the result to the pointed-to location in linear memory. For example:

```
wapi_result_t wapi_fs_read(wapi_handle_t fd, void* buf, wapi_size_t len, wapi_size_t* bytes_read);
```

At the Wasm level, this is:

```
(func $wapi_fs_read (import "wapi_fs" "read") (param i32 i32 i32 i32) (result i32))
```

The fourth parameter (`bytes_read`) is an `i32` pointer. On success, the host writes a `uint32_t` value to that address in linear memory.

### 3.5 Strings

Strings are passed as `(pointer, length)` pairs -- two `i32` parameters at the Wasm level. All strings are UTF-8 encoded. There is no null-terminator requirement when an explicit length is provided.

The C-level type is `wapi_string_view_t`:

```c
typedef struct wapi_string_view_t {
    const char* data;    /* Offset 0: pointer to UTF-8 bytes (i32) */
    wapi_size_t   length;  /* Offset 4: byte count (i32)             */
} wapi_string_view_t;
```

**Layout:** 8 bytes, alignment 4.

**Sentinel value:** When `length` equals `WAPI_STRLEN` (`UINT32_MAX`), the string is null-terminated and the host reads until the first `0x00` byte.

When strings appear as function parameters (not inside a struct), they are split into two separate Wasm parameters: `(i32 ptr, i32 len)`.

### 3.6 Structs

Structs are passed **by pointer**. The host reads and writes struct fields directly from the module's linear memory. All struct layouts are pinned at the byte level (see [Section 6: Struct Layout Rules](#6-struct-layout-rules)).

At the Wasm level, a struct pointer is a single `i32` parameter.

### 3.7 Arrays

Arrays are passed as `(pointer, count)` pairs: two `i32` parameters. The pointer references the first element; the count specifies the number of elements.

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
wapi_result_t wapi_env_get_error(char* buf, wapi_size_t buf_len, wapi_size_t* msg_len);
```

The message is valid until the next WAPI API call on the same thread.

### 5.5 Language Bindings

Language bindings are expected to wrap `wapi_result_t` into native error types:
- **Rust:** `Result<T, TpError>`
- **Zig:** `error.TpError` via error unions
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
    struct wapi_chained_struct_t* next;   /* Offset 0, 4 bytes (pointer) */
    wapi_stype_t                  sType;  /* Offset 4, 4 bytes (struct type tag) */
} wapi_chained_struct_t;                  /* Total: 8 bytes, align 4 */
```

Descriptor structs that support chaining have `wapi_chained_struct_t* nextInChain` as their first field. Extension structs embed `wapi_chained_struct_t` as their first field with an `sType` tag identifying the extension type.

The host walks the chain, processing known `sType` values and ignoring unknown ones. This allows new extensions to be added without changing existing struct layouts.

**Defined `sType` values:**

| Constant                          | Value    | Description                     |
|-----------------------------------|----------|---------------------------------|
| `WAPI_STYPE_INVALID`               | `0x0000` | Invalid / no type               |
| `WAPI_STYPE_SURFACE_DESC_FULLSCREEN` | `0x0001` | Fullscreen surface extension  |
| `WAPI_STYPE_SURFACE_DESC_RESIZABLE`  | `0x0002` | Resizable surface extension   |
| `WAPI_STYPE_GPU_SURFACE_CONFIG`      | `0x0100` | GPU surface configuration     |
| `WAPI_STYPE_AUDIO_SPATIAL_CONFIG`    | `0x0200` | Spatial audio configuration   |
| `WAPI_STYPE_TEXT_STYLE_INLINE`       | `0x0300` | Inline text style extension   |

### 6.6 Compile-Time Verification

All struct sizes are verified at compile time using `_Static_assert`:

```c
_Static_assert(sizeof(wapi_io_op_t) == 64, "wapi_io_op_t must be 64 bytes");
_Static_assert(sizeof(wapi_io_event_t) == 32, "wapi_io_event_t must be 32 bytes");
_Static_assert(sizeof(wapi_allocator_t) == 16, "wapi_allocator_t must be 16 bytes");
_Static_assert(sizeof(wapi_io_t) == 24, "wapi_io_t must be 24 bytes");
_Static_assert(sizeof(wapi_panic_handler_t) == 8, "wapi_panic_handler_t must be 8 bytes");
_Static_assert(sizeof(wapi_context_t) == 20, "wapi_context_t must be 20 bytes");
_Static_assert(sizeof(wapi_filestat_t) == 56, "wapi_filestat_t must be 56 bytes");
_Static_assert(sizeof(wapi_dirent_t) == 24, "wapi_dirent_t must be 24 bytes");
_Static_assert(sizeof(wapi_audio_spec_t) == 12, "wapi_audio_spec_t must be 12 bytes");
_Static_assert(sizeof(wapi_event_t) == 128, "wapi_event_t must be 128 bytes");
```

---

## 7. Import Namespaces

Each capability module defines a Wasm import namespace. A module's `.wasm` binary imports functions from only the namespaces it uses. The host must provide implementations for all imported functions.

| Import Module      | C Header               | Description                                    |
|--------------------|------------------------|------------------------------------------------|
| `wapi`               | `wapi_capability.h`      | Capability queries, ABI version                |
| `wapi_env`           | `wapi_env.h`             | Arguments, environment variables, random, exit |
| `wapi_memory`        | `wapi_memory.h`          | Host-provided memory allocation                |
| `wapi_io`            | `wapi_io.h`              | I/O vtable (submit/cancel/poll/wait/flush)     |
| `wapi_clock`         | `wapi_clock.h`           | Monotonic and wall clocks, performance counter |
| `wapi_fs`            | `wapi_fs.h`              | Capability-based filesystem                    |
| `wapi_net`           | `wapi_net.h`             | QUIC/WebTransport networking                   |
| `wapi_gpu`           | `wapi_gpu.h`             | WebGPU bridge (device, surface, proc table)    |
| `wapi_surface`       | `wapi_surface.h`         | Windowing / display surfaces                   |
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

The WAPI uses a **submit + unified event queue** async I/O model inspired by Linux's `io_uring`. All asynchronous operations -- file reads, network sends, audio buffer fills, timer waits -- are submitted through a function table (vtable). Completions arrive as events in the same unified event queue as input, window, and lifecycle events.

There is no `async`/`await`. There are no colored functions. There are no new types for futures or promises. The module submits operation descriptors via `ctx->io->submit()`, does other work, and then polls or waits for completion events via `ctx->io->poll()` / `ctx->io->wait()`.

### 8.2 I/O Function Table (`wapi_io_t`)

Defined in `wapi_context.h`. I/O is accessed through a function table with an opaque context pointer:

```c
typedef struct wapi_io_t {
    void*         impl;     // Opaque context pointer
    int32_t       (*submit)(void* impl, const wapi_io_op_t* ops, wapi_size_t count);
    wapi_result_t (*cancel)(void* impl, uint64_t user_data);
    int32_t       (*poll)(void* impl, wapi_event_t* event);
    int32_t       (*wait)(void* impl, wapi_event_t* event, int32_t timeout_ms);
    void          (*flush)(void* impl, uint32_t event_type);
} wapi_io_t;  // 24 bytes, align 4
```

The host provides a default `wapi_io_t` in the `wapi_context_t` passed to `wapi_main`. All events -- I/O completions, input, lifecycle -- are delivered through `poll`/`wait`. Parent modules can wrap the vtable to add logging, throttling, sandboxing, or mocking:

```c
static int32_t logging_submit(void* impl, const wapi_io_op_t* ops, wapi_size_t count) {
    log_state_t* state = (log_state_t*)impl;
    log("submitting %d ops", count);
    return state->inner->submit(state->inner->impl, ops, count);
}
```

### 8.3 Operation Descriptor (`wapi_io_op_t`)

Each operation is described by a fixed-size **64-byte descriptor**:

```
Byte Layout (64 bytes, alignment 8):

Offset  Size  Type      Field        Description
------  ----  --------  -----------  -----------------------------------
 0       4    uint32_t  opcode       Operation type (wapi_io_opcode_t)
 4       4    uint32_t  flags        Operation flags
 8       4    int32_t   fd           Handle / file descriptor
12       4    uint32_t  _pad0        Padding (must be zero)
16       8    uint64_t  offset       File offset or timeout (nanoseconds)
24       4    uint32_t  addr         Pointer to buffer (i32)
28       4    uint32_t  len          Buffer length
32       4    uint32_t  addr2        Second pointer (path, etc.)
36       4    uint32_t  len2         Second length
40       8    uint64_t  user_data    Opaque, echoed in completion event
48       4    uint32_t  result_ptr   Pointer for output values
52       4    uint32_t  flags2       Additional operation-specific flags
56       8    uint8_t[] reserved     Reserved (must be zero)
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

Operations are submitted through the I/O vtable provided in the context:

```c
/* Submit a batch of operations. Returns count submitted, or negative error.
 * Completions arrive as WAPI_EVENT_IO_COMPLETION events. */
int32_t count = ctx->io->submit(ctx->io->impl, ops, n);

/* Cancel a pending operation by user_data. */
wapi_result_t result = ctx->io->cancel(ctx->io->impl, user_data);
```

There are no separate `poll` or `wait` functions for I/O. All completions flow through the unified event queue via `ctx->io->poll()` and `ctx->io->wait()`.

### 8.7 Usage Patterns

The same API supports fundamentally different application architectures:

- **Game loop:** Submit asset loads, render a frame, poll events at the end of the frame — I/O completions and input arrive in the same queue.
- **CLI tool:** Submit a file read, call `ctx->io->wait()` to block until the I/O completion event arrives.
- **Audio processor:** Poll events every audio callback cycle for completed buffer fills.
- **Server:** Submit accepts and receives, wait for completion events in a loop.

---

## 9. Module Entry Points

### 9.1 `wapi_main`

```wat
(func (export "wapi_main") (param i32) (result i32))
```

**C signature:**

```c
WAPI_EXPORT(wapi_main) wapi_result_t wapi_main(const wapi_context_t* ctx);
```

Called by the host after module instantiation and memory initialization. The host writes a `wapi_context_t` into linear memory and passes its pointer as the argument:

```c
typedef struct wapi_panic_handler_t {
    void* impl;                          // Offset 0  (opaque implementation context)
    void  (*fn)(void* impl, const char* msg, wapi_size_t msg_len);  // Offset 4
} wapi_panic_handler_t;  // 8 bytes, align 4

typedef struct wapi_context_t {
    const wapi_allocator_t*      allocator;   // Offset 0  (host-provided allocator vtable)
    const wapi_io_t*             io;          // Offset 4  (host-provided I/O vtable)
    const wapi_panic_handler_t*  panic;       // Offset 8  (panic handler, NULL = default)
    wapi_handle_t                gpu_device;  // Offset 12 (0 if no GPU)
    uint32_t                     flags;       // Offset 16 (WAPI_CTX_FLAG_*)
} wapi_context_t;  // 20 bytes, align 4
```

The module should:

1. Store the context pointer for use throughout its lifetime.
2. Query capabilities via `wapi_capability_supported` or enumerate with `wapi_capability_count` / `wapi_capability_name`.
3. Use `ctx->allocator` for memory allocation, `ctx->io` for async I/O submission, and `ctx->panic` for unrecoverable error reporting.
4. For headless applications: perform work and return `WAPI_OK` to exit.
5. For graphical applications: create surfaces, configure GPU, and return `WAPI_OK` to begin the frame loop.

The same `wapi_context_t` struct is used when passing context from an application to a shared module via `wapi_module_init`. A parent module may wrap the allocator, I/O vtable, or panic handler to control how children allocate memory, perform I/O, and report unrecoverable errors.

**Panic handler:** The `panic` field points to a `wapi_panic_handler_t` (or NULL for the runtime default). When a module encounters an unrecoverable error, it calls the panic handler to record the message, then traps. The handler is NOT noreturn — it records and returns; the module then hits `__builtin_trap()` (`unreachable` in wasm). The runtime catches the trap and has the recorded message available.

When a parent module calls a child via `wapi_module_get_func` and the child traps, the runtime catches the trap and returns an error to the parent (not a trap). The child module is faulted and should be released. Because the parent provides the panic handler via the context, the panic message routes to the correct caller — even when the module runtime is shared between multiple applications.

```c
static inline _Noreturn void wapi_panic(const wapi_context_t* ctx,
                                        const char* msg, wapi_size_t msg_len) {
    if (ctx->panic) {
        ctx->panic->fn(ctx->panic->impl, msg, msg_len);
    }
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

1. Poll events via `ctx->io->poll()` (input, I/O completions, lifecycle).
2. Update application state.
3. Render via WebGPU.
4. Return `WAPI_OK` to continue, or `WAPI_ERR_CANCELED` to request exit.

If the module does not export `wapi_frame`, the host does not enter a frame loop. This is the expected behavior for headless applications.

---

## 10. Capabilities

### 10.1 Unified Capability Model

All capabilities -- whether foundational (memory, filesystem) or specialized (geolocation, camera) -- use a single, unified query mechanism. There is no distinction between "core" and "extension" capabilities. This follows the **Vulkan model**: query at startup by string name, use only if the host reports support. Each capability is independently versioned.

Capability names are **dot-separated strings** following a hierarchical namespace:

```
wapi.<module>          Spec-defined capabilities
vendor.<name>.*      Vendor-specific capabilities
```

### 10.2 Capability Query API

```c
/* Check if a capability is supported by name. Returns boolean. */
wapi_bool_t wapi_capability_supported(const char* name, wapi_size_t name_len);

/* Get the version of a capability. Writes wapi_version_t to output pointer. */
wapi_result_t wapi_capability_version(const char* name, wapi_size_t name_len, wapi_version_t* version);

/* Get the total count of supported capabilities. */
int32_t wapi_capability_count(void);

/* Get the name of a capability by index. Writes name string to buffer. */
wapi_result_t wapi_capability_name(int32_t index, char* buf, wapi_size_t buf_len, wapi_size_t* name_len);

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
| `wapi.io`                | `wapi_io`            | I/O vtable (submit/cancel/poll/wait/flush)     |
| `wapi.clock`             | `wapi_clock`         | Monotonic and wall clocks, performance counter |
| `wapi.fs`                | `wapi_fs`            | Capability-based filesystem                    |
| `wapi.net`               | `wapi_net`           | QUIC/WebTransport networking                   |
| `wapi.gpu`               | `wapi_gpu`           | WebGPU bridge (device, surface, proc table)    |
| `wapi.surface`           | `wapi_surface`       | Windowing / display surfaces                   |
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

### 10.4 Vendor Capabilities

Vendors may define custom capabilities using the `vendor.<vendor_name>.*` namespace. For example:

- `vendor.nintendo.joycon` -- Nintendo Joy-Con haptics
- `vendor.apple.pencil` -- Apple Pencil force/tilt data
- `vendor.valve.steamdeck` -- Steam Deck-specific features

Vendor capabilities use the same `wapi_capability_supported` / `wapi_capability_version` query mechanism. Hosts MUST ignore vendor capability queries they do not recognize (returning false from `wapi_capability_supported`).

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
    "wapi.gpu", "wapi.surface", "wapi.input", "wapi.audio", "wapi.content",
    "wapi.clipboard", "wapi.font",
    NULL
};

static const char* WAPI_PRESET_MOBILE[] = {
    "wapi.env", "wapi.memory", "wapi.io", "wapi.clock", "wapi.fs", "wapi.net",
    "wapi.gpu", "wapi.surface", "wapi.input", "wapi.audio", "wapi.content",
    "wapi.clipboard", "wapi.font", "wapi.geolocation", "wapi.sensors",
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
| **Graphical**| Headless + gpu, surface, input, audio, content, clipboard, font | Apps, games, creative tools |
| **Mobile**   | Graphical + geolocation, sensors, notifications, biometric, camera, share | Mobile apps |

**Checking preset conformance:**

The `wapi_preset_supported` inline helper iterates the preset array and checks each capability:

```c
static inline wapi_bool_t wapi_preset_supported(const char** preset) {
    for (int i = 0; preset[i] != NULL; i++) {
        const char* name = preset[i];
        wapi_size_t len = 0;
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

## 11. Shared Module Linking

### 11.1 Motivation

Bundling every dependency into a single `.wasm` binary leads to duplication. If ten applications each bundle their own libc, image decoder, and math library, the same code is loaded ten times. Shared module linking allows the runtime to deduplicate common dependencies.

### 11.2 Import Declaration

Modules declare shared imports by namespace and version in their Wasm custom sections:

```
Custom section: "wapi_shared_imports"
Format: JSON array of {namespace, version} objects

[
  {"namespace": "libc", "version": "1.0.0"},
  {"namespace": "libpng", "version": "1.6.40"}
]
```

### 11.3 Runtime Resolution

The host runtime resolves shared imports:

1. Looks up the namespace and version in its module cache.
2. If a compatible version is cached, links it to the importing module.
3. If not cached, fetches the module (from a registry, local store, or network) and caches it.
4. Modules are instantiated once and shared across all importers.

### 11.4 Semver for ABI Contracts

Shared module versions follow **semantic versioning**:

- **Major version** change: breaking ABI change. The runtime must not substitute a different major version.
- **Minor version** change: backward-compatible additions. The runtime may substitute a higher minor version within the same major version.
- **Patch version** change: bug fix. The runtime may substitute a higher patch version.

### 11.5 Shared Linear Memory

Shared modules may operate on the importing module's linear memory. The runtime enforces borrowing rules:

- Only one module may have mutable access to a memory region at a time.
- Multiple modules may have read-only access to the same memory region concurrently.
- The host tracks active borrows and traps on violations.

This model follows Rust's borrowing semantics at the module boundary, enforced by the runtime rather than the compiler.

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

The WAPI does not replace HTML, CSS, or JavaScript. Traditional websites continue to work unchanged. The ABI provides an alternative application model for software that benefits from a single portable binary -- games, creative tools, productivity applications, simulations.

A web page may embed a WAPI module alongside traditional web content using an `<iframe>` or a dedicated container element.

---

## 13. Complete Function Reference

This section lists every function in each import module with its Wasm-level signature. Parameter types use Wasm notation: `i32`, `i64`, `f32`, `f64`. Return types follow the `->` arrow. Functions that return no value are marked `-> ()`.

---

### 13.1 Module: `wapi` (Capability Queries)

| Wasm Import Name         | Wasm Signature                  | C Function                          | Description |
|--------------------------|---------------------------------|-------------------------------------|-------------|
| `capability_supported`   | `(i32, i32) -> i32`             | `wapi_capability_supported`           | Query whether a capability is supported by name. Params: name_ptr, name_len. Returns boolean. |
| `capability_version`     | `(i32, i32, i32) -> i32`        | `wapi_capability_version`             | Get the version of a capability by name. Params: name_ptr, name_len, out_version_ptr. |
| `capability_count`       | `() -> i32`                     | `wapi_capability_count`               | Get count of all supported capabilities. |
| `capability_name`        | `(i32, i32, i32, i32) -> i32`   | `wapi_capability_name`                | Get capability name by index. Params: index, buf_ptr, buf_len, out_name_len_ptr. |
| `abi_version`            | `(i32) -> i32`                  | `wapi_abi_version`                    | Get the WAPI version. Writes `wapi_version_t` to pointer. |

---

### 13.2 Module: `wapi_env` (Environment and Process)

| Wasm Import Name    | Wasm Signature                          | C Function               | Description |
|---------------------|-----------------------------------------|--------------------------|-------------|
| `args_count`        | `() -> i32`                             | `wapi_env_args_count`      | Get number of command-line arguments. |
| `args_get`          | `(i32, i32, i32, i32) -> i32`           | `wapi_env_args_get`        | Get argument by index. Params: index, buf_ptr, buf_len, out_len_ptr. |
| `environ_count`     | `() -> i32`                             | `wapi_env_environ_count`   | Get number of environment variables. |
| `environ_get`       | `(i32, i32, i32, i32) -> i32`           | `wapi_env_environ_get`     | Get env var by index as "KEY=VALUE". Params: index, buf_ptr, buf_len, out_len_ptr. |
| `getenv`            | `(i32, i32, i32, i32, i32) -> i32`      | `wapi_env_getenv`          | Look up env var by name. Params: name_ptr, name_len, buf_ptr, buf_len, out_len_ptr. |
| `random_get`        | `(i32, i32) -> i32`                     | `wapi_env_random_get`      | Fill buffer with cryptographic random bytes. Params: buf_ptr, len. |
| `exit`              | `(i32) -> ()`                           | `wapi_env_exit`            | Exit the process. Does not return. |
| `get_error`         | `(i32, i32, i32) -> i32`               | `wapi_env_get_error`       | Get human-readable error message. Params: buf_ptr, buf_len, out_len_ptr. |

---

### 13.3 Module: `wapi_memory` (Allocation)

| Wasm Import Name | Wasm Signature            | C Function           | Description |
|------------------|---------------------------|----------------------|-------------|
| `alloc`          | `(i32, i32) -> i32`       | `wapi_mem_alloc`       | Allocate memory. Params: size, alignment. Returns pointer or 0. |
| `free`           | `(i32) -> ()`             | `wapi_mem_free`        | Free allocated memory. Param: pointer. |
| `realloc`        | `(i32, i32, i32) -> i32`  | `wapi_mem_realloc`     | Resize allocation. Params: pointer, new_size, alignment. Returns pointer or 0. |
| `usable_size`    | `(i32) -> i32`            | `wapi_mem_usable_size` | Query usable size of allocation. Param: pointer. Returns byte count. |

---

### 13.4 Module: `wapi_io` (Async I/O)

I/O operations are submitted through the `wapi_io_t` vtable provided in `wapi_context_t`, not through direct host imports. The host registers all five vtable-backing functions as imports:

| Wasm Import Name | Wasm Signature                  | C Function              | Description |
|------------------|---------------------------------|-------------------------|-------------|
| `submit`         | `(i32, i32, i32) -> i32`        | (vtable backing)          | Submit operations. Params: impl_ptr, ops_ptr, count. Returns submitted count. |
| `cancel`         | `(i32, i64) -> i32`             | (vtable backing)          | Cancel a pending operation. Params: impl_ptr, user_data. |
| `poll`           | `(i32, i32) -> i32`             | (vtable backing)          | Non-blocking poll. Params: impl_ptr, event_ptr. Returns 1 if event, 0 if empty. |
| `wait`           | `(i32, i32, i32) -> i32`        | (vtable backing)          | Blocking wait. Params: impl_ptr, event_ptr, timeout_ms. Returns 1 if event, 0 on timeout. |
| `flush`          | `(i32, i32) -> ()`              | (vtable backing)          | Discard pending events. Params: impl_ptr, event_type (0 = all). |

All events -- I/O completions, input, lifecycle, device changes -- are delivered through `poll`/`wait`. There is no separate event import namespace.

---

### 13.5 Module: `wapi_clock` (Time)

| Wasm Import Name  | Wasm Signature         | C Function                | Description |
|-------------------|------------------------|---------------------------|-------------|
| `time_get`        | `(i32, i32) -> i32`    | `wapi_clock_time_get`       | Get current time. Params: clock_id, out_timestamp_ptr. |
| `resolution`      | `(i32, i32) -> i32`    | `wapi_clock_resolution`     | Get clock resolution. Params: clock_id, out_resolution_ptr. |
| `perf_counter`    | `() -> i64`            | `wapi_clock_perf_counter`   | Get high-resolution performance counter value. |
| `perf_frequency`  | `() -> i64`            | `wapi_clock_perf_frequency` | Get performance counter frequency in Hz. |
| `yield`           | `() -> ()`             | `wapi_yield`                | Yield the current execution slice. |
| `sleep`           | `(i64) -> ()`          | `wapi_sleep`                | Sleep for a duration in nanoseconds. |

---

### 13.6 Module: `wapi_fs` (Filesystem)

| Wasm Import Name  | Wasm Signature                              | C Function             | Description |
|-------------------|---------------------------------------------|------------------------|-------------|
| `preopen_count`   | `() -> i32`                                 | `wapi_fs_preopen_count`  | Get number of pre-opened directories. |
| `preopen_path`    | `(i32, i32, i32, i32) -> i32`               | `wapi_fs_preopen_path`   | Get path of pre-opened directory. Params: index, buf_ptr, buf_len, out_len_ptr. |
| `preopen_handle`  | `(i32) -> i32`                              | `wapi_fs_preopen_handle` | Get handle of pre-opened directory. Param: index. Returns handle. |
| `open`            | `(i32, i32, i32, i32, i32, i32) -> i32`     | `wapi_fs_open`           | Open a file. Params: dir_fd, path_ptr, path_len, oflags, fdflags, out_fd_ptr. |
| `read`            | `(i32, i32, i32, i32) -> i32`               | `wapi_fs_read`           | Read from file. Params: fd, buf_ptr, len, out_bytes_read_ptr. |
| `write`           | `(i32, i32, i32, i32) -> i32`               | `wapi_fs_write`          | Write to file. Params: fd, buf_ptr, len, out_bytes_written_ptr. |
| `pread`           | `(i32, i32, i32, i64, i32) -> i32`          | `wapi_fs_pread`          | Positional read. Params: fd, buf_ptr, len, offset, out_bytes_read_ptr. |
| `pwrite`          | `(i32, i32, i32, i64, i32) -> i32`          | `wapi_fs_pwrite`         | Positional write. Params: fd, buf_ptr, len, offset, out_bytes_written_ptr. |
| `seek`            | `(i32, i64, i32, i32) -> i32`               | `wapi_fs_seek`           | Seek within file. Params: fd, offset, whence, out_new_offset_ptr. |
| `close`           | `(i32) -> i32`                              | `wapi_fs_close`          | Close a file descriptor. Param: fd. |
| `sync`            | `(i32) -> i32`                              | `wapi_fs_sync`           | Sync file data to storage. Param: fd. |
| `stat`            | `(i32, i32) -> i32`                         | `wapi_fs_stat`           | Get file status. Params: fd, out_stat_ptr. |
| `path_stat`       | `(i32, i32, i32, i32) -> i32`               | `wapi_fs_path_stat`      | Get file status by path. Params: dir_fd, path_ptr, path_len, out_stat_ptr. |
| `set_size`        | `(i32, i64) -> i32`                         | `wapi_fs_set_size`       | Set file size. Params: fd, size. |
| `mkdir`           | `(i32, i32, i32) -> i32`                    | `wapi_fs_mkdir`          | Create directory. Params: dir_fd, path_ptr, path_len. |
| `rmdir`           | `(i32, i32, i32) -> i32`                    | `wapi_fs_rmdir`          | Remove directory. Params: dir_fd, path_ptr, path_len. |
| `unlink`          | `(i32, i32, i32) -> i32`                    | `wapi_fs_unlink`         | Remove file. Params: dir_fd, path_ptr, path_len. |
| `rename`          | `(i32, i32, i32, i32, i32, i32) -> i32`     | `wapi_fs_rename`         | Rename file/directory. Params: old_dir_fd, old_path_ptr, old_path_len, new_dir_fd, new_path_ptr, new_path_len. |
| `readdir`         | `(i32, i32, i32, i64, i32) -> i32`          | `wapi_fs_readdir`        | Read directory entries. Params: fd, buf_ptr, buf_len, cookie, out_used_ptr. |

---

### 13.7 Module: `wapi_net` (Networking)

| Wasm Import Name   | Wasm Signature                   | C Function                | Description |
|--------------------|----------------------------------|---------------------------|-------------|
| `connect`          | `(i32, i32) -> i32`              | `wapi_net_connect`          | Open a connection. Params: desc_ptr, out_conn_ptr. |
| `listen`           | `(i32, i32) -> i32`              | `wapi_net_listen`           | Start listening. Params: desc_ptr, out_listener_ptr. |
| `accept`           | `(i32, i32) -> i32`              | `wapi_net_accept`           | Accept a connection. Params: listener, out_conn_ptr. |
| `close`            | `(i32) -> i32`                   | `wapi_net_close`            | Close a connection, listener, or stream. Param: handle. |
| `stream_open`      | `(i32, i32, i32) -> i32`         | `wapi_net_stream_open`      | Open a QUIC stream. Params: conn, type, out_stream_ptr. |
| `stream_accept`    | `(i32, i32) -> i32`              | `wapi_net_stream_accept`    | Accept a QUIC stream. Params: conn, out_stream_ptr. |
| `send`             | `(i32, i32, i32, i32) -> i32`    | `wapi_net_send`             | Send data. Params: handle, buf_ptr, len, out_bytes_sent_ptr. |
| `recv`             | `(i32, i32, i32, i32) -> i32`    | `wapi_net_recv`             | Receive data. Params: handle, buf_ptr, len, out_bytes_recv_ptr. |
| `send_datagram`    | `(i32, i32, i32) -> i32`         | `wapi_net_send_datagram`    | Send unreliable datagram (QUIC). Params: conn, buf_ptr, len. |
| `recv_datagram`    | `(i32, i32, i32, i32) -> i32`    | `wapi_net_recv_datagram`    | Receive a datagram. Params: conn, buf_ptr, len, out_recv_len_ptr. |
| `resolve`          | `(i32, i32, i32, i32, i32) -> i32` | `wapi_net_resolve`        | DNS resolution. Params: host_ptr, host_len, addrs_buf_ptr, buf_len, out_count_ptr. |

---

### 13.8 Module: `wapi_gpu` (WebGPU Bridge)

| Wasm Import Name              | Wasm Signature               | C Function                              | Description |
|-------------------------------|------------------------------|-----------------------------------------|-------------|
| `request_device`              | `(i32, i32) -> i32`          | `wapi_gpu_request_device`                 | Request a GPU device. Params: desc_ptr, out_device_ptr. |
| `get_queue`                   | `(i32, i32) -> i32`          | `wapi_gpu_get_queue`                      | Get default queue. Params: device, out_queue_ptr. |
| `release_device`              | `(i32) -> i32`               | `wapi_gpu_release_device`                 | Release a GPU device. Param: device. |
| `configure_surface`           | `(i32) -> i32`               | `wapi_gpu_configure_surface`              | Configure a surface for GPU rendering. Param: config_ptr. |
| `surface_get_current_texture` | `(i32, i32, i32) -> i32`     | `wapi_gpu_surface_get_current_texture`    | Get current texture for rendering. Params: surface, out_texture_ptr, out_view_ptr. |
| `surface_present`             | `(i32) -> i32`               | `wapi_gpu_surface_present`                | Present surface texture. Param: surface. |
| `surface_preferred_format`    | `(i32, i32) -> i32`          | `wapi_gpu_surface_preferred_format`       | Query preferred texture format. Params: surface, out_format_ptr. |
| `get_proc_address`            | `(i32, i32) -> i32`          | `wapi_gpu_get_proc_address`               | Get a WebGPU function pointer by name. Params: name_ptr, name_len. Returns function pointer. |

The `get_proc_address` function provides access to the full `webgpu.h` function table (~150 functions). The module retrieves individual function pointers by name (e.g., `"wgpuDeviceCreateBuffer"`) and calls them directly. This matches the `WGPUProcTable` / `wgpuGetProcAddress` pattern from `webgpu.h`.

---

### 13.9 Module: `wapi_surface` (Windowing)

| Wasm Import Name    | Wasm Signature                       | C Function                     | Description |
|---------------------|--------------------------------------|--------------------------------|-------------|
| `create`            | `(i32, i32) -> i32`                  | `wapi_surface_create`            | Create a display surface. Params: desc_ptr, out_surface_ptr. |
| `destroy`           | `(i32) -> i32`                       | `wapi_surface_destroy`           | Destroy a surface. Param: surface. |
| `get_size`          | `(i32, i32, i32) -> i32`             | `wapi_surface_get_size`          | Get pixel size. Params: surface, out_width_ptr, out_height_ptr. |
| `get_size_logical`  | `(i32, i32, i32) -> i32`             | `wapi_surface_get_size_logical`  | Get logical size. Params: surface, out_width_ptr, out_height_ptr. |
| `get_dpi_scale`     | `(i32, i32) -> i32`                  | `wapi_surface_get_dpi_scale`     | Get DPI scale. Params: surface, out_scale_ptr. |
| `request_size`      | `(i32, i32, i32) -> i32`             | `wapi_surface_request_size`      | Request a size change. Params: surface, width, height. |
| `set_title`         | `(i32, i32, i32) -> i32`             | `wapi_surface_set_title`         | Set window title. Params: surface, title_ptr, title_len. |
| `set_fullscreen`    | `(i32, i32) -> i32`                  | `wapi_surface_set_fullscreen`    | Set fullscreen mode. Params: surface, fullscreen_bool. |
| `set_visible`       | `(i32, i32) -> i32`                  | `wapi_surface_set_visible`       | Show or hide surface. Params: surface, visible_bool. |
| `minimize`          | `(i32) -> i32`                       | `wapi_surface_minimize`          | Minimize surface. Param: surface. |
| `maximize`          | `(i32) -> i32`                       | `wapi_surface_maximize`          | Maximize surface. Param: surface. |
| `restore`           | `(i32) -> i32`                       | `wapi_surface_restore`           | Restore surface from min/max. Param: surface. |
| `set_cursor`        | `(i32, i32) -> i32`                  | `wapi_surface_set_cursor`        | Set mouse cursor style. Params: surface, cursor_type. |

**Cursor type constants:**

| Constant                | Value |
|-------------------------|-------|
| `WAPI_CURSOR_DEFAULT`     |   0   |
| `WAPI_CURSOR_POINTER`     |   1   |
| `WAPI_CURSOR_TEXT`        |   2   |
| `WAPI_CURSOR_CROSSHAIR`   |   3   |
| `WAPI_CURSOR_MOVE`        |   4   |
| `WAPI_CURSOR_RESIZE_NS`   |   5   |
| `WAPI_CURSOR_RESIZE_EW`   |   6   |
| `WAPI_CURSOR_RESIZE_NWSE` |   7   |
| `WAPI_CURSOR_RESIZE_NESW` |   8   |
| `WAPI_CURSOR_NOT_ALLOWED` |   9   |
| `WAPI_CURSOR_WAIT`        |  10   |
| `WAPI_CURSOR_GRAB`        |  11   |
| `WAPI_CURSOR_GRABBING`    |  12   |
| `WAPI_CURSOR_NONE`        |  13   |

---

### 13.10 Module: `wapi_input` (Input Events)

| Wasm Import Name       | Wasm Signature              | C Function                     | Description |
|------------------------|-----------------------------|--------------------------------|-------------|
| `poll_event`           | `(i32) -> i32`              | `wapi_input_poll_event`          | Poll for next event (non-blocking). Param: out_event_ptr. Returns boolean. |
| `wait_event`           | `(i32, i32) -> i32`         | `wapi_input_wait_event`          | Wait for event. Params: out_event_ptr, timeout_ms. Returns boolean. |
| `flush_events`         | `(i32) -> ()`               | `wapi_input_flush_events`        | Flush events by type. Param: event_type (0 = all). |
| `key_pressed`          | `(i32) -> i32`              | `wapi_input_key_pressed`         | Check if key is pressed. Param: scancode. Returns boolean. |
| `get_mod_state`        | `() -> i32`                 | `wapi_input_get_mod_state`       | Get modifier key state. Returns WAPI_KMOD_* flags. |
| `mouse_position`       | `(i32, i32, i32) -> i32`    | `wapi_input_mouse_position`      | Get mouse position. Params: surface, out_x_ptr, out_y_ptr. |
| `mouse_button_state`   | `() -> i32`                 | `wapi_input_mouse_button_state`  | Get mouse button state bitmask. |
| `set_relative_mouse`   | `(i32, i32) -> i32`         | `wapi_input_set_relative_mouse`  | Enable/disable relative mouse mode. Params: surface, enabled. |
| `start_text_input`     | `(i32) -> ()`               | `wapi_input_start_text_input`    | Start text input / IME. Param: surface. |
| `stop_text_input`      | `(i32) -> ()`               | `wapi_input_stop_text_input`     | Stop text input / IME. Param: surface. |

**Event union:** All events are delivered as 128-byte `wapi_event_t` unions. The first `uint32_t` field identifies the event type. All events share a 16-byte common header:

```
Byte Layout of wapi_event_common_t (16 bytes):

Offset  Size  Type      Field       Description
------  ----  --------  ----------  -----------------------------------
 0       4    uint32_t  type        Event type (wapi_event_type_t)
 4       4    uint32_t  surface_id  Surface handle
 8       8    uint64_t  timestamp   Monotonic time in nanoseconds
```

**Event type ranges:**

| Range           | Category               |
|-----------------|------------------------|
| `0x100-0x1FF`   | Application lifecycle  |
| `0x200-0x2FF`   | Surface events         |
| `0x300-0x3FF`   | Keyboard events        |
| `0x400-0x4FF`   | Mouse events           |
| `0x650-0x6FF`   | Gamepad events         |
| `0x700-0x7FF`   | Touch events           |
| `0x800-0x8FF`   | Pen/stylus events      |
| `0x1000-0x10FF` | Drop events            |
| `0x8000-0xFFFF` | User-defined events    |

---

### 13.11 Module: `wapi_audio` (Audio)

| Wasm Import Name          | Wasm Signature                    | C Function                       | Description |
|---------------------------|-----------------------------------|----------------------------------|-------------|
| `open_device`             | `(i32, i32, i32) -> i32`          | `wapi_audio_open_device`           | Open audio device. Params: device_id, spec_ptr, out_device_ptr. |
| `close_device`            | `(i32) -> i32`                    | `wapi_audio_close_device`          | Close audio device. Param: device. |
| `resume_device`           | `(i32) -> i32`                    | `wapi_audio_resume_device`         | Resume (unpause) device. Param: device. |
| `pause_device`            | `(i32) -> i32`                    | `wapi_audio_pause_device`          | Pause device. Param: device. |
| `create_stream`           | `(i32, i32, i32) -> i32`          | `wapi_audio_create_stream`         | Create audio stream. Params: src_spec_ptr, dst_spec_ptr, out_stream_ptr. |
| `destroy_stream`          | `(i32) -> i32`                    | `wapi_audio_destroy_stream`        | Destroy audio stream. Param: stream. |
| `bind_stream`             | `(i32, i32) -> i32`               | `wapi_audio_bind_stream`           | Bind stream to device. Params: device, stream. |
| `unbind_stream`           | `(i32) -> i32`                    | `wapi_audio_unbind_stream`         | Unbind stream from device. Param: stream. |
| `put_stream_data`         | `(i32, i32, i32) -> i32`          | `wapi_audio_put_stream_data`       | Push playback data. Params: stream, buf_ptr, len. |
| `get_stream_data`         | `(i32, i32, i32, i32) -> i32`     | `wapi_audio_get_stream_data`       | Pull recorded data. Params: stream, buf_ptr, len, out_bytes_read_ptr. |
| `stream_available`        | `(i32) -> i32`                    | `wapi_audio_stream_available`      | Query available bytes in stream. Param: stream. |
| `stream_queued`           | `(i32) -> i32`                    | `wapi_audio_stream_queued`         | Query queued bytes in stream. Param: stream. |
| `open_device_stream`      | `(i32, i32, i32, i32) -> i32`     | `wapi_audio_open_device_stream`    | Open device + create + bind stream. Params: device_id, spec_ptr, out_device_ptr, out_stream_ptr. |
| `playback_device_count`   | `() -> i32`                       | `wapi_audio_playback_device_count` | Get number of playback devices. |
| `recording_device_count`  | `() -> i32`                       | `wapi_audio_recording_device_count`| Get number of recording devices. |
| `device_name`             | `(i32, i32, i32, i32) -> i32`     | `wapi_audio_device_name`           | Get device name. Params: device_id, buf_ptr, buf_len, out_name_len_ptr. |

**Audio format constants:**

| Constant         | Value    | Description                     |
|------------------|----------|---------------------------------|
| `WAPI_AUDIO_U8`   | `0x0008` | Unsigned 8-bit samples          |
| `WAPI_AUDIO_S16`  | `0x8010` | Signed 16-bit little-endian     |
| `WAPI_AUDIO_S32`  | `0x8020` | Signed 32-bit little-endian     |
| `WAPI_AUDIO_F32`  | `0x8120` | 32-bit float little-endian      |

**Audio spec layout (`wapi_audio_spec_t`, 12 bytes, alignment 4):**

```
Offset  Size  Type      Field     Description
------  ----  --------  --------  -----------------------------------
 0       4    uint32_t  format    Audio format (wapi_audio_format_t)
 4       4    int32_t   channels  Channel count (1=mono, 2=stereo)
 8       4    int32_t   freq      Sample rate in Hz
```

---

### 13.12 Module: `wapi_content` (Host-Rendered Content)

| Wasm Import Name       | Wasm Signature                            | C Function                      | Description |
|------------------------|-------------------------------------------|---------------------------------|-------------|
| `create_text`          | `(i32) -> i32`                            | `wapi_content_create_text`        | Create text content. Param: desc_ptr. Returns content handle. |
| `create_image`         | `(i32) -> i32`                            | `wapi_content_create_image`       | Create image content. Param: desc_ptr. Returns content handle. |
| `destroy`              | `(i32) -> i32`                            | `wapi_content_destroy`            | Destroy content element. Param: content. |
| `measure`              | `(i32, i32, i32) -> i32`                  | `wapi_content_measure`            | Measure content dimensions. Params: content, constraints_ptr, out_result_ptr. |
| `render_to_texture`    | `(i32, i32, i32, i32, i32, i32) -> i32`   | `wapi_content_render_to_texture`  | Render content to GPU texture. Params: content, gpu_texture, x, y, width, height. |
| `update_text`          | `(i32, i32) -> i32`                       | `wapi_content_update_text`        | Update existing text content. Params: content, desc_ptr. |
| `set_a11y`             | `(i32, i32, i32, i32) -> i32`             | `wapi_content_set_a11y`           | Set accessibility role and label. Params: content, role, label_ptr, label_len. |
| `get_line_info`        | `(i32, i32, i32) -> i32`                  | `wapi_content_get_line_info`      | Get info about a text line. Params: content, line_index, out_line_info_ptr. |
| `hit_test`             | `(i32, f32, f32, i32) -> i32`             | `wapi_content_hit_test`           | Hit-test a point against text content. Params: content, x, y, out_result_ptr. |
| `get_caret_info`       | `(i32, i32, i32) -> i32`                  | `wapi_content_get_caret_info`     | Get caret geometry at a text offset. Params: content, char_offset, out_caret_info_ptr. |
| `get_load_status`      | `(i32) -> i32`                            | `wapi_content_get_load_status`    | Get load/decode status of content. Param: content. Returns status enum. |

**Content structs:**

`wapi_text_line_info_t` (28 bytes, alignment 4):

```
Offset  Size  Type      Field            Description
------  ----  --------  ---------------  -----------------------------------
 0       4    int32_t   char_offset      Byte offset of line start in text
 4       4    int32_t   char_length      Byte length of line
 8       4    float     origin_x         X origin of line in content coords
12       4    float     origin_y         Y origin of line (baseline)
16       4    float     width            Typographic width of line
20       4    float     ascent           Ascent above baseline
24       4    float     descent          Descent below baseline
```

`wapi_hit_test_result_t` (16 bytes, alignment 4):

```
Offset  Size  Type      Field            Description
------  ----  --------  ---------------  -----------------------------------
 0       4    int32_t   char_offset      Byte offset of nearest character
 4       4    int32_t   line_index       Line index containing the hit
 8       4    uint32_t  is_trailing      1 if hit is on trailing edge of char
12       4    uint32_t  is_inside        1 if point is inside text bounds
```

`wapi_caret_info_t` (24 bytes, alignment 4):

```
Offset  Size  Type      Field            Description
------  ----  --------  ---------------  -----------------------------------
 0       4    float     x                Caret X position
 4       4    float     y                Caret top Y position
 8       4    float     height           Caret height
12       4    int32_t   line_index       Line containing the caret
16       4    uint32_t  is_rtl           1 if caret is in RTL context
20       4    uint32_t  _reserved        Reserved (must be zero)
```

`wapi_image_extended_t` (32 bytes, alignment 4):

```
Offset  Size  Type      Field            Description
------  ----  --------  ---------------  -----------------------------------
 0       4    uint32_t  width            Image width in pixels
 4       4    uint32_t  height           Image height in pixels
 8       4    uint32_t  format           Pixel format (wapi_image_format_t)
12       4    uint32_t  flags            Image flags
16       4    uint32_t  data_ptr         Pointer to pixel data (i32)
20       4    uint32_t  data_len         Pixel data byte length
24       4    float     scale            Display scale factor (1.0 = 1x)
28       4    uint32_t  _reserved        Reserved (must be zero)
```

**Content load status constants:**

| Constant                    | Value | Description                  |
|-----------------------------|-------|------------------------------|
| `WAPI_CONTENT_STATUS_EMPTY`   |   0   | No content loaded            |
| `WAPI_CONTENT_STATUS_LOADING` |   1   | Content is loading/decoding  |
| `WAPI_CONTENT_STATUS_READY`   |   2   | Content is ready for use     |
| `WAPI_CONTENT_STATUS_ERROR`   |   3   | Load/decode failed           |

---

### 13.13 Module: `wapi_clipboard` (Clipboard)

| Wasm Import Name | Wasm Signature                  | C Function              | Description |
|------------------|---------------------------------|-------------------------|-------------|
| `has_format`     | `(i32) -> i32`                  | `wapi_clipboard_has_format` | Check if clipboard has format. Param: format. Returns boolean. |
| `read`           | `(i32, i32, i32, i32) -> i32`   | `wapi_clipboard_read`     | Read from clipboard. Params: format, buf_ptr, buf_len, out_bytes_written_ptr. |
| `write`          | `(i32, i32, i32) -> i32`        | `wapi_clipboard_write`    | Write to clipboard. Params: format, data_ptr, len. |
| `clear`          | `() -> i32`                     | `wapi_clipboard_clear`    | Clear all clipboard contents. |

**Clipboard format constants:**

| Constant              | Value | Description       |
|-----------------------|-------|-------------------|
| `WAPI_CLIPBOARD_TEXT`   |   0   | UTF-8 text        |
| `WAPI_CLIPBOARD_HTML`   |   1   | HTML text         |
| `WAPI_CLIPBOARD_IMAGE`  |   2   | Image data (PNG)  |

---

### 13.14 Module: `wapi_font` (Font System)

| Wasm Import Name       | Wasm Signature                          | C Function                    | Description |
|------------------------|-----------------------------------------|-------------------------------|-------------|
| `family_count`         | `() -> i32`                             | `wapi_font_family_count`        | Get number of available font families. |
| `family_name`          | `(i32, i32, i32, i32) -> i32`           | `wapi_font_family_name`         | Get family name by index. Params: index, buf_ptr, buf_len, out_name_len_ptr. |
| `match_family`         | `(i32, i32, i32, i32, i32) -> i32`      | `wapi_font_match_family`        | Match a font by family, weight, style. Params: family_ptr, family_len, weight, style, out_font_ptr. |
| `get_metrics`          | `(i32, f32, i32) -> i32`               | `wapi_font_get_metrics`         | Get font metrics at a given size. Params: font, size, out_metrics_ptr. |
| `destroy`              | `(i32) -> i32`                          | `wapi_font_destroy`             | Release a matched font handle. Param: font. |
| `has_glyph`            | `(i32, i32) -> i32`                     | `wapi_font_has_glyph`           | Check if font contains a codepoint. Params: font, codepoint. Returns boolean. |

**Font weight constants:**

| Constant              | Value |
|-----------------------|-------|
| `WAPI_FONT_WEIGHT_THIN`       | 100 |
| `WAPI_FONT_WEIGHT_LIGHT`      | 300 |
| `WAPI_FONT_WEIGHT_REGULAR`    | 400 |
| `WAPI_FONT_WEIGHT_MEDIUM`     | 500 |
| `WAPI_FONT_WEIGHT_BOLD`       | 700 |
| `WAPI_FONT_WEIGHT_BLACK`      | 900 |

**Font style constants:**

| Constant              | Value |
|-----------------------|-------|
| `WAPI_FONT_STYLE_NORMAL`  | 0 |
| `WAPI_FONT_STYLE_ITALIC`  | 1 |
| `WAPI_FONT_STYLE_OBLIQUE` | 2 |

---

### 13.15 Module: `wapi_video` (Video/Media Playback)

| Wasm Import Name       | Wasm Signature                          | C Function                    | Description |
|------------------------|-----------------------------------------|-------------------------------|-------------|
| `create`               | `(i32, i32) -> i32`                     | `wapi_video_create`             | Create a video player. Params: desc_ptr, out_player_ptr. |
| `destroy`              | `(i32) -> i32`                          | `wapi_video_destroy`            | Destroy a video player. Param: player. |
| `play`                 | `(i32) -> i32`                          | `wapi_video_play`               | Start playback. Param: player. |
| `pause`                | `(i32) -> i32`                          | `wapi_video_pause`              | Pause playback. Param: player. |
| `seek`                 | `(i32, f64) -> i32`                     | `wapi_video_seek`               | Seek to time in seconds. Params: player, time_seconds. |
| `get_position`         | `(i32, i32) -> i32`                     | `wapi_video_get_position`       | Get current playback position. Params: player, out_time_ptr (f64). |
| `get_duration`         | `(i32, i32) -> i32`                     | `wapi_video_get_duration`       | Get total duration. Params: player, out_duration_ptr (f64). |
| `get_status`           | `(i32) -> i32`                          | `wapi_video_get_status`         | Get playback status. Param: player. Returns status enum. |
| `get_frame_texture`    | `(i32, i32) -> i32`                     | `wapi_video_get_frame_texture`  | Get current frame as GPU texture. Params: player, out_texture_ptr. |
| `set_volume`           | `(i32, f32) -> i32`                     | `wapi_video_set_volume`         | Set playback volume (0.0-1.0). Params: player, volume. |
| `set_loop`             | `(i32, i32) -> i32`                     | `wapi_video_set_loop`           | Enable/disable looping. Params: player, loop_bool. |

**Video status constants:**

| Constant                   | Value | Description              |
|----------------------------|-------|--------------------------|
| `WAPI_VIDEO_STATUS_IDLE`     |   0   | No media loaded          |
| `WAPI_VIDEO_STATUS_LOADING`  |   1   | Media is loading         |
| `WAPI_VIDEO_STATUS_PLAYING`  |   2   | Actively playing         |
| `WAPI_VIDEO_STATUS_PAUSED`   |   3   | Paused                   |
| `WAPI_VIDEO_STATUS_ENDED`    |   4   | Playback reached the end |
| `WAPI_VIDEO_STATUS_ERROR`    |   5   | Playback error           |

---

## Appendix A: Version History

| Version | Date       | Description          |
|---------|------------|----------------------|
| 1.0.0   | 2026-04-02 | Initial specification |

## Appendix B: Module Exports Summary

A conforming WAPI module MUST export:

| Export Name | Wasm Signature     | Required | Description                         |
|-------------|--------------------|----------|-------------------------------------|
| `wapi_main`   | `(i32) -> i32`     | Yes      | Module entry point (receives context ptr) |
| `wapi_frame`  | `(i64) -> i32`     | No       | Per-frame callback (graphical apps) |
| `memory`    | `(memory ...)`     | Yes      | Linear memory (for host access)     |

## Appendix C: Struct Size Summary

| Struct                     | Size (bytes) | Alignment |
|----------------------------|-------------|-----------|
| `wapi_string_view_t`         |     8       |    4      |
| `wapi_chained_struct_t`      |     8       |    4      |
| `wapi_version_t`             |     8       |    2      |
| `wapi_audio_spec_t`          |    12       |    4      |
| `wapi_layout_constraints_t`  |     8       |    4      |
| `wapi_layout_result_t`       |    16       |    4      |
| `wapi_text_run_t`            |    16       |    4      |
| `wapi_io_t`                  |    24       |    4      |
| `wapi_allocator_t`           |    16       |    4      |
| `wapi_io_event_t`            |    32       |    4      |
| `wapi_panic_handler_t`       |     8       |    4      |
| `wapi_context_t`             |    20       |    4      |
| `wapi_net_listen_desc_t`     |    20       |    4      |
| `wapi_dirent_t`              |    24       |    8      |
| `wapi_net_connect_desc_t`    |    24       |    4      |
| `wapi_surface_desc_t`        |    24       |    4      |
| `wapi_gpu_surface_config_t`  |    24       |    4      |
| `wapi_text_style_t`          |    40       |    4      |
| `wapi_filestat_t`            |    56       |    8      |
| `wapi_io_op_t`               |    64       |    8      |
| `wapi_event_t`               |   128       |    8      |

---

*End of specification.*
