# WAPI Specification

**Version 1.0.0**

**Status: Draft**

---

## Table of Contents

1. [Overview](#1-overview)
2. [Calling Convention](#2-calling-convention)
3. [Handle System](#3-handle-system)
4. [Error Handling](#4-error-handling)
5. [Struct Layout Rules](#5-struct-layout-rules)
6. [Import Namespaces](#6-import-namespaces)
7. [Async I/O Model](#7-async-io-model)
8. [Module Entry Points](#8-module-entry-points)
9. [Capabilities](#9-capabilities)
10. [Module Linking](#10-module-linking)
11. [Browser Integration](#11-browser-integration)
---

## 1. Overview

The WAPI defines a capability-based application binary interface that turns WebAssembly into a universal application platform. A single compiled `.wasm` binary runs unmodified on every host: native desktop, mobile, browser tab, server, edge function, or embedded device.

Where WASI and the Component Model aim for language-agnostic composability through WIT interface descriptions and a Canonical ABI that lifts and lowers complex types, the WAPI optimizes for a different point in the design space: **minimal indirection between application and host**. The ABI is a C-style calling convention document. Wasm modules import functions from well-known namespaces; the host implements them. The interface is specified entirely through C header files with explicit struct layouts and Wasm-level function signatures. This trades the Component Model's rich type system and automatic bindings generation for a thinner contract that any host can implement with no toolchain beyond a C compiler.

**Core thesis:** WebAssembly provides a universal ISA for CPU compute. The WAPI provides everything else an application needs (I/O, windowing, GPU, audio, networking) through a single stable surface that every host agrees to implement.

### 1.1 Goals

- **One binary, one ABI, every platform.** A compiled `.wasm` file runs on any conforming host without recompilation, relinking, or adaptation layers.
- **Zero abstraction tax.** The ABI maps directly to Wasm value types. No serialization, no intermediate representations, no code generation steps between the application and the host.
- **Capability-based security by default.** Modules start with zero access. Every resource is a host-validated handle. There is no ambient authority.
- **Additive complexity.** A CLI tool imports only `wapi_env` and `wapi_filesystem`. A game imports the full graphical stack. The module declares what it needs; the host provides exactly that.
- **Self-contained capabilities.** Each capability header stands alone. Vocabulary shared across capabilities (font style, pixel format, etc.) is redeclared per namespace rather than factored into a shared header, so no capability has a compile-time or link-time dependency on another. See §6.1. The cost is a little syntactic duplication; the benefit is that there is no cross-capability dependency graph to track, validate, or version.

### 1.2 Non-Goals

- **Runtime identification as the primary mechanism.** Modules should detect features via capabilities, not branch on platform strings. However, `wapi_sysinfo_host_get` provides an escape hatch for the ~10% of cases where platform knowledge is genuinely needed (workarounds, analytics, platform-appropriate UI). Prefer capability queries; use host info as a last resort.
- **Backward-compatible evolution of individual function signatures.** Functions are versioned at the module level. When a function signature must change, a new version of the capability module is introduced.
- **Replacing webgpu.h.** GPU operations use the standard `webgpu.h` API from the webgpu-native project. The WAPI provides only the bridge between its handle/surface model and the WebGPU API.

---

## 2. Calling Convention

### 2.1 Value Types

All functions at the ABI boundary use only WebAssembly value types:

| Wasm Type | Size    | C Equivalent     | Usage                                    |
|-----------|---------|------------------|------------------------------------------|
| `i32`     | 4 bytes | `int32_t`        | Handles, result codes, enums, booleans, flags |
| `i64`     | 8 bytes | `int64_t`        | Pointers, sizes, timestamps, file offsets, user data |
| `f32`     | 4 bytes | `float`          | Coordinates, scale factors, audio levels |
| `f64`     | 8 bytes | `double`         | (reserved for future use)                |

No other types cross the ABI boundary. Structs, strings, and arrays are passed by pointer (`i64`).

### 2.2 Pointer Representation

**Design principle: one ABI for both wasm32 and wasm64.** All pointers and sizes at the Wasm function boundary are `i64`. Wasm32 modules zero-extend their 32-bit addresses and lengths into `i64` parameters. All address/pointer/size fields in ABI structs are `uint64_t`. This costs a few extra bytes and an extend instruction per pointer in wasm32 but eliminates the need for two ABI profiles, two sets of function signatures, two sets of struct layouts, and two code paths in every host implementation.

Pointers reference byte offsets into the module's linear memory. The host reads and writes linear memory directly to exchange structured data. This applies to:

- Address fields in `wapi_io_op_t` (`addr`, `addr2`, `result_ptr`)
- Pointer fields in `wapi_string_view_t` (`data`)
- Chain pointers in `wapi_chained_struct_t` (`next`)
- Array element counts and byte lengths that may span the full address space
- Any other struct field that holds a linear memory address

Size and count fields that may need to span the full address space (array element counts, byte lengths) are `uint64_t`, matching the address field convention. Wasm32 modules zero-extend their 32-bit values. Fields that are inherently bounded (handles (`int32_t`), enums, flags) remain their natural width.

**No truncation risk:** pointers always reference the module's own linear memory. A wasm32 module's memory is bounded at 4 GB, so every valid address fits in the lower 32 bits of an `i64`. The host never writes an address exceeding 32 bits into a wasm32 module's structs because no such address exists in that module's memory.

Because the import signatures are identical (`i64` for all pointers and sizes), **cross-width module linking** works for runtime (isolated) modules with no adaptation. A wasm64 application can load a wasm32 library (or vice versa) in isolated mode; the module-to-module boundary is handle-based (function handles, buffer mappings, Wasm value types), none of which depend on pointer width.

**Build-time shared linear memory** requires matching pointer width. Two modules linked into the same binary must both be wasm32 or both be wasm64. This is enforced at link time.

### 2.3 Return Codes

All fallible functions return `wapi_result_t` (an `i32`):

- `0` (`WAPI_OK`): success.
- Negative values: error codes (see [Section 4: Error Handling](#4-error-handling)).
- Positive values: used by some functions to return counts (e.g., `io->submit()` returns the number of operations submitted).

### 2.4 Output Values

Functions that produce output values receive a **caller-provided pointer** as a parameter. The host writes the result to the pointed-to location in linear memory. For example:

```
wapi_result_t wapi_filesystem_read(wapi_handle_t fd, void* buf, uint64_t len, uint64_t* bytes_read);
```

At the Wasm level, this is:

```
(func $wapi_filesystem_read (import "wapi_filesystem" "read") (param i32 i64 i64 i64) (result i32))
```

The first parameter (`fd`) is an `i32` handle. The remaining parameters are `i64`: `buf` (pointer), `len` (size), and `bytes_read` (pointer). On success, the host writes a `uint64_t` value to the `bytes_read` address in linear memory.

### 2.5 Strings

Strings are passed as `(pointer, length)` pairs, two `i64` parameters at the Wasm level. All strings crossing the host boundary are UTF-8 encoded. There is no null-terminator requirement when an explicit length is provided. Modules are free to use any internal string encoding (UTF-16, Latin-1, etc.); the UTF-8 requirement applies only to strings passed through WAPI host imports and exports.

The C-level type is `wapi_string_view_t`:

```c
typedef struct wapi_string_view_t {
    uint64_t    data;    /* Offset 0: linear memory address of UTF-8 bytes */
    uint64_t    length;  /* Offset 8: byte count                           */
} wapi_string_view_t;
```

**Layout:** 16 bytes, alignment 8. Both fields are `uint64_t` so the layout is identical for wasm32 and wasm64. Wasm32 modules zero-extend their 32-bit values.

**Sentinel value:** When `length` equals `WAPI_STRLEN` (`UINT64_MAX`), the string is null-terminated and the host reads until the first `0x00` byte.

When strings appear as function parameters (not inside a struct), they are split into two separate Wasm parameters: `(i64 ptr, i64 len)`. Wasm32 modules zero-extend both values.

### 2.6 Structs

Structs are passed **by pointer**. The host reads and writes struct fields directly from the module's linear memory. All struct layouts are pinned at the byte level (see [Section 5: Struct Layout Rules](#5-struct-layout-rules)).

At the Wasm level, a struct pointer is a single `i64` parameter.

### 2.7 Arrays

Arrays are passed as `(pointer, count)` pairs — two `i64` parameters at the Wasm level. The pointer references the first element; the count specifies the number of elements. In struct layouts, both fields are `uint64_t` following the unified layout principle (see [Section 2.2](#22-pointer-representation)). Wasm32 modules zero-extend their 32-bit values.

### 2.8 Naming Conventions

Every identifier at the ABI surface — functions, types, structs, fields, enum values, macros — follows three spelling rules. The goal is a surface that sorts into hierarchical groups and can be navigated with `grep`.

Casing is fixed: functions, types, and fields are `lower_snake_case`; macros and enum values are `UPPER_SNAKE_CASE`; type names end in `_t`. These rules apply on top of that casing and govern how segments are chosen and ordered.

**Rule 1: Read left to right, most general to most specific.**

Each underscore-separated segment is one step down the hierarchy. The leftmost segment names the module (`wapi`, `WAPI_`), the next names a subsystem (`INPUT`, `GAMEPAD`, `GPU`), and each subsequent segment narrows the meaning further.

```
WAPI_GAMEPAD_AXIS_TRIGGER_LEFT
 └──┘ └─────┘ └──┘ └─────┘ └──┘
  │      │     │      │     └─ which instance
  │      │     │      └─────── axis sub-kind
  │      │     └────────────── property kind
  │      └──────────────────── device
  └─────────────────────────── module
```

Sorting by name groups related identifiers: every `WAPI_GAMEPAD_*` constant lives in one block, every `..._AXIS_*` in a sub-block, and so on. The header reads top-down as an outline.

**Rule 2: Underscores separate levels of hierarchy, not words.**

A concept that takes multiple English words to name remains one segment — the words are joined without a separator. Underscores are reserved for crossing a hierarchy boundary; they are not generic word delimiters.

| Identifier                            | Why it is one segment                       |
|---------------------------------------|---------------------------------------------|
| `WAPI_CURSOR_NOTALLOWED`              | "not allowed" names one cursor kind         |
| `WAPI_PEN_AXIS_TANGENTIALPRESSURE`    | "tangential pressure" is one axis           |
| `wapi_input_start_textinput`          | "text input" is one mode                    |
| `wapi_keyboard_get_modstate`          | "mod state" is one query                    |

A segment earns its own underscore only when more than one sibling exists at that level. If a word has no siblings, it belongs joined to its parent.

**Rule 3: Category before instance.**

When several instances share a category, the category is the earlier segment and the instance is the later one — never the reverse. This is a corollary of Rule 1 (categories are broader than instances) and the practical payoff of Rule 1 (grouping under `grep`).

| Prefer                              | Avoid                               |
|-------------------------------------|-------------------------------------|
| `WAPI_GAMEPAD_AXIS_TRIGGER_LEFT`    | `WAPI_GAMEPAD_AXIS_LEFT_TRIGGER`    |
| `WAPI_GAMEPAD_AXIS_TRIGGER_RIGHT`   | `WAPI_GAMEPAD_AXIS_RIGHT_TRIGGER`   |
| `WAPI_GAMEPAD_AXIS_STICK_LEFT_X`    | `WAPI_GAMEPAD_AXIS_LEFTX`           |
| `WAPI_GAMEPAD_BUTTON_DPAD_UP`       | `WAPI_GAMEPAD_BUTTON_UP_DPAD`       |
| `WAPI_GAMEPAD_BUTTON_SHOULDER_LEFT` | `WAPI_GAMEPAD_BUTTON_LSHOULDER`     |

After Rule 3, `grep TRIGGER_` finds every trigger, `grep STICK_` finds every stick, and the enum values for a gamepad sort into sensible blocks instead of interleaving instances of different kinds.

**Applying the rules.**

When choosing a name, walk down the hierarchy and emit one segment per level, joining multi-word concepts within a level:

1. Start at the module (`wapi` / `WAPI`).
2. Add the subsystem (device, capability, operation domain).
3. Add the property kind if there are multiple kinds at that level.
4. Add the specific instance last.
5. If a step has only one occupant, join its word(s) to the next segment — do not emit a lone underscore.

When a name already exists and feels wrong, check the three rules in order. Rule 1 catches reversed hierarchies, Rule 2 catches spurious underscores between words of one concept, and Rule 3 catches instance-before-category mistakes.

### 2.9 Type Suffixes

Every struct and enum that crosses the ABI uses one of three suffixes. The suffix tells the reader what the data is *for* and whether it can be cached. Do not introduce `config`, `settings`, `options`, `params`, `descriptor`, `properties`, or similar variants — the three suffixes below cover every case.

| Suffix     | Form        | Direction          | Lifetime                                           | Example                                  |
|------------|-------------|--------------------|----------------------------------------------------|------------------------------------------|
| `_desc_t`  | struct      | caller → platform  | Describes caller intent. Passed to `*_create`, `*_open`, `*_configure`. | `wapi_surface_desc_t`, `wapi_gpu_device_desc_t` |
| `_info_t`  | struct      | platform → caller  | Static identity/capability. Filled by the platform when the resource is opened and does not change at runtime. Safe to cache. | `wapi_gamepad_info_t`, `wapi_display_info_t`    |
| `_state_t` | struct      | platform → caller  | Live runtime snapshot. Changes frame-to-frame. Must be re-read. | `wapi_finger_state_t`, `wapi_xr_frame_state_t`  |
| `_state_t` | **enum**    | value type         | Enumeration of discrete state values. The struct/enum distinction resolves the overload. | `wapi_power_idle_state_t`, `wapi_cap_state_t`  |

**Method naming follows from the suffix:**

- `wapi_X_open(const wapi_X_desc_t* desc, ...)` — create, takes `_desc_t`.
- `wapi_X_configure(handle, const wapi_X_desc_t* desc)` — reconfigure an existing object. Still takes `_desc_t` — the struct describes the new configuration; there is no `_config_t`.
- `wapi_X_get_info(handle, wapi_X_info_t* out)` — one-time static query.
- `wapi_X_get_state(handle, wapi_X_state_t* out)` — live snapshot query, expected to be called every frame.

**Info vs state — the cache rule.** If a reader sees `_info_t` they may read it once when they open the handle and store it. If they see `_state_t` they must re-read it whenever they need a current value. Choose the suffix based on this behavior, not on the word that reads most naturally. If the data ever changes at runtime — even occasionally, even on plug/unplug — it is state, not info.

**When in doubt, ask:**

1. Is the struct filled by the caller or by the platform? Caller → `_desc_t`. Platform → `_info_t` or `_state_t`.
2. If platform-filled, is the value stable for the lifetime of the handle? Stable → `_info_t`. Volatile → `_state_t`.
3. If it is an enum of possible states (not a struct), it is `_state_t`.

---

## 3. Handle System

### 3.1 Handle Type

All host-managed resources are referenced by `wapi_handle_t`, defined as `int32_t`. Handles are opaque tokens. The module must not interpret their numeric value or perform arithmetic on them.

```c
typedef int32_t wapi_handle_t;
```

### 3.2 Invalid Handle

The value `0` is reserved as the invalid/null handle:

```c
#define WAPI_HANDLE_INVALID ((wapi_handle_t)0)
```

Functions that return handles return `WAPI_HANDLE_INVALID` on failure. Functions that accept handles reject `WAPI_HANDLE_INVALID` with `WAPI_ERR_BADF`.

### 3.3 Pre-granted Handles

The host pre-grants a small set of handles before the module's entry point is called:

| Handle | Value | Description                    |
|--------|-------|--------------------------------|
| `WAPI_STDIN`  | `1` | Standard input stream          |
| `WAPI_STDOUT` | `2` | Standard output stream         |
| `WAPI_STDERR` | `3` | Standard error stream          |

Pre-opened filesystem directories begin at handle value `4` (`WAPI_FS_PREOPEN_BASE`).

### 3.4 Handle Lifecycle

1. **Grant:** The host creates a handle and returns it to the module (e.g., `wapi_filesystem_open` writes a new handle to an output pointer).
2. **Use:** The module passes the handle to subsequent API calls. The host validates it on every call.
3. **Release:** The module explicitly closes or destroys the handle (e.g., `wapi_filesystem_close`, `wapi_surface_destroy`). After release, the handle value is invalid and must not be reused by the module.

### 3.5 Handle Validation

The host MUST validate every handle on every call. If a module passes a handle that:
- equals `WAPI_HANDLE_INVALID`,
- was never granted,
- has already been released, or
- is of the wrong type for the function,

the host MUST return `WAPI_ERR_BADF` and MUST NOT perform the requested operation.

---

## 4. Error Handling

### 4.1 Errors Are Values

Errors are `i32` return codes. There are no exceptions, no traps (for ABI errors), and no out-of-band error channels. Every fallible function returns `wapi_result_t`.

### 4.2 Checking Results

```c
#define WAPI_FAILED(result)    ((result) < 0)
#define WAPI_SUCCEEDED(result) ((result) >= 0)
```

### 4.3 Error Code Table

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

### 4.4 Error Messages

The host maintains a per-thread human-readable error message for the most recent error. The module can retrieve it via:

```c
wapi_result_t wapi_env_get_error(char* buf, uint64_t buf_len, uint64_t* msg_len);
```

The message is valid until the next WAPI API call on the same thread.

### 4.5 Language Bindings

Language bindings are expected to wrap `wapi_result_t` into native error types:
- **Rust:** `Result<T, WapiError>`
- **Zig:** `error.WapiError` via error unions
- **Go:** `error` interface
- **C++:** optional exception wrappers or `std::expected`

The ABI itself never throws, never traps, and never uses exceptions. Error handling at the boundary is always explicit return codes.

---

## 5. Struct Layout Rules

### 5.1 Byte Order

All multi-byte values in structs are **little-endian**. This matches the WebAssembly memory model, which defines little-endian byte order for all loads and stores.

### 5.2 Alignment

Each field is aligned to its **natural alignment**:

| Field Type  | Size    | Alignment |
|-------------|---------|-----------|
| `uint8_t`   | 1 byte  | 1 byte    |
| `uint16_t`  | 2 bytes | 2 bytes   |
| `int32_t` / `uint32_t` / `float` | 4 bytes | 4 bytes |
| `int64_t` / `uint64_t` / `double` | 8 bytes | 8 bytes |

Struct alignment equals the alignment of its most-aligned field.

### 5.3 Explicit Padding

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

### 5.4 Reserved Fields for Extension

Structs include `_reserved` fields that must be set to zero by the module. Future ABI versions may assign meaning to these fields. Hosts MUST ignore reserved fields that are zero. This provides forward compatibility without changing struct sizes.

### 5.5 Forward Compatibility via `nextInChain`

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

### 5.6 Compile-Time Verification

All struct sizes AND field offsets are verified at compile time using `_Static_assert` with both `sizeof` and `offsetof`. Size-only assertions catch total-size drift but not internal layout bugs (swapped fields, wrong-sized padding that silently introduces implicit padding elsewhere). The `offsetof` assertions catch these.

Every ABI struct must have:
1. An `offsetof` assertion for **every field** (including `_pad` / `_reserved` fields).
2. A `sizeof` assertion for the total struct size.
3. An `_Alignof` assertion for the struct's alignment.

```c
/* Example: wapi_io_op_t (80 bytes, align 8) */
_Static_assert(offsetof(wapi_io_op_t, opcode)     ==  0, "");
_Static_assert(offsetof(wapi_io_op_t, flags)      ==  4, "");
_Static_assert(offsetof(wapi_io_op_t, fd)         ==  8, "");
_Static_assert(offsetof(wapi_io_op_t, flags2)     == 12, "");
_Static_assert(offsetof(wapi_io_op_t, offset)     == 16, "");
_Static_assert(offsetof(wapi_io_op_t, addr)       == 24, "");
_Static_assert(offsetof(wapi_io_op_t, len)        == 32, "");
_Static_assert(offsetof(wapi_io_op_t, addr2)      == 40, "");
_Static_assert(offsetof(wapi_io_op_t, len2)       == 48, "");
_Static_assert(offsetof(wapi_io_op_t, user_data)  == 56, "");
_Static_assert(offsetof(wapi_io_op_t, result_ptr) == 64, "");
_Static_assert(offsetof(wapi_io_op_t, reserved)   == 72, "");
_Static_assert(sizeof(wapi_io_op_t) == 80, "wapi_io_op_t must be 80 bytes");
_Static_assert(_Alignof(wapi_io_op_t) == 8, "wapi_io_op_t must be 8-byte aligned");

```

Address fields in ABI structs are `uint64_t` regardless of pointer width, so the layout is identical for wasm32 and wasm64 (see [Section 2.2](#22-pointer-representation)). Structs that embed C pointers (like `wapi_string_view_t`, `wapi_chained_struct_t`) use `uint64_t` for the address component, making their layout platform-independent. Assertions verify on all platforms.

The complete set of assertions is in `wapi.h` Part 7 and in each capability module header.

---

## 6. Import Namespaces

Each capability module defines a Wasm import namespace. A module's `.wasm` binary imports functions from only the namespaces it uses. The host must provide implementations for all imported functions. The C header for each module is `<module>.h` (e.g., `wapi_gpu` → `wapi_gpu.h`).

The `wapi` namespace provides the two vtable acquisition imports (`io_get`, `allocator_get`) and panic reporting. I/O and memory allocation are accessed through the returned vtables, not as separate import namespaces. Typed API namespaces (`wapi_gpu`, `wapi_audio`, etc.) are host imports operating on handles.

| Module               | Description                                    |
|----------------------|------------------------------------------------|
| `wapi`               | Vtable acquisition (`io_get`, `allocator_get`), panic reporting |
| `wapi_env`           | Arguments, environment variables, random bytes, locale, timezone, exit |
| `wapi_clock`         | Monotonic and wall clocks, performance counter |
| `wapi_filesystem`    | Capability-based filesystem                    |
| `wapi_network`       | QUIC/WebTransport networking                   |
| `wapi_gpu`           | WebGPU bridge (device, surface, proc table)    |
| `wapi_surface`       | Render targets (on-screen and offscreen)       |
| `wapi_window`        | OS window management (title, fullscreen, etc.) |
| `wapi_display`       | Display enumeration, geometry, sub-pixels      |
| `wapi_input`         | Input events (keyboard, mouse, touch, gamepad) |
| `wapi_audio`         | Audio playback and recording                   |
| `wapi_audioplugin`   | Audio plugin hosting (VST-style)               |
| `wapi_content`       | Host-rendered text, images, media              |
| `wapi_transfer`      | Unified data transfer (clipboard / DnD / share) |
| `wapi_seat`          | Seat enumeration (multi-user / multi-pointer)   |
| `wapi_font`          | Font system queries and enumeration            |
| `wapi_video`         | Video/media playback                           |
| `wapi_geolocation`   | GPS / location services                        |
| `wapi_notifications` | System notifications                           |
| `wapi_sensors`       | Accelerometer, gyroscope, compass              |
| `wapi_speech`        | Speech recognition and synthesis               |
| `wapi_crypto`        | Hardware-accelerated cryptography              |
| `wapi_biometric`     | Fingerprint, face recognition                  |
| `wapi_kvstorage`     | Persistent key-value storage                   |
| `wapi_payments`      | In-app purchases / payment processing          |
| `wapi_usb`           | USB device access                              |
| `wapi_midi`          | MIDI device access                             |
| `wapi_bluetooth`     | Bluetooth device access                        |
| `wapi_camera`        | Camera capture                                 |
| `wapi_xr`            | VR/AR (WebXR, OpenXR)                          |
| `wapi_module`        | Runtime module loading, cross-module calls     |
| `wapi_thread`        | Threads, TLS, and synchronization primitives   |
| `wapi_process`       | Process spawning and management                |
| `wapi_dialog`        | File/save/message dialogs                      |
| `wapi_sysinfo`       | System information queries                     |
| `wapi_eyedrop`       | Screen color picker                            |
| `wapi_contacts`      | Contact picker and icon access                 |

### 6.0 Unified Transfer (clipboard / DnD / share)

`wapi_transfer` collapses what other platforms split across three APIs (clipboard, drag-and-drop, share-sheet) into one verb — `offer` — with three **delivery modes** that capture the only thing that actually differs: which user gesture routes the offer to a target, and how long the offer persists.

| Mode | Gesture | Offer lifetime | Target |
|---|---|---|---|
| `WAPI_TRANSFER_LATENT` | paste | until replaced | any future reader on the seat |
| `WAPI_TRANSFER_POINTED` | drop on a surface | drag session | revealed at drop position |
| `WAPI_TRANSFER_ROUTED` | pick from system sheet | until pick or cancel | revealed by user pick |

`mode` is a **bitmask**: a single `wapi_transfer_offer` call can advertise the same payload in multiple delivery modes simultaneously. A "share this" affordance that *also* fills the clipboard *and* allows dragging is one call with `mode = LATENT|POINTED|ROUTED`.

The target side is symmetric across modes. `wapi_transfer_format_count`, `wapi_transfer_has_format`, and `wapi_transfer_read` each take `(seat, mode)` — `LATENT` reads the clipboard pool, `POINTED` reads the active drag session (valid between `WAPI_EVENT_TRANSFER_ENTER` and `WAPI_EVENT_TRANSFER_LEAVE`). `ROUTED` has no target-side API; the recipient is another app picked by the OS sheet.

POINTED-only in-flight UX (cursor hover, drop-effect feedback) is delivered through `WAPI_EVENT_TRANSFER_*` events carrying `wapi_transfer_event_t`. The drag-side app responds with `wapi_transfer_set_action` during `WAPI_EVENT_TRANSFER_OVER`.

### 6.0.1 Seats

A **seat** is one user's bundle of input devices, display(s), audio, clipboard, and active drag state — the same concept as Wayland's `wl_seat` or Linux logind's seat assignment. On single-user systems there is exactly one seat (`WAPI_SEAT_DEFAULT`). On multi-user-on-one-machine configurations (Linux multi-seat, kiosks, shared-display collaboration tools) each user is a distinct seat.

WAPI models seats as a property of the **device**, not of every event. `wapi_pointer_event_t`, key events, and pen events all already carry a `pointer_id` / `device_id`; `wapi_device_seat(device) → wapi_seat_t` recovers the owning seat on demand. Apps that don't care about seats (the common case) ignore it and pass `WAPI_SEAT_DEFAULT` to outbound transfer ops. Apps that do (multi-user kiosks) attribute events to seats by looking up the device and address per-seat clipboards / share sheets explicitly.

`wapi_seat_count` / `wapi_seat_at` / `wapi_seat_name` enumerate seats. Enumeration is freely available — no capability gate — because it only names what the OS already gave the process. Per-seat *operations* go through the per-capability grants (`wapi.transfer`, etc.).

### 6.1 Namespace Isolation

Each capability module is a **self-contained unit**. A capability's C header declares every type it needs, even when another capability declares a type with the same shape. Headers do not `#include` each other except for the universal `wapi.h`, and no capability has a compile-time or link-time dependency on another.

**Concrete rule:** when two capabilities need the same vocabulary (font weight, font style, pixel format, etc.), each redeclares its own namespaced enum. The values are kept identical so a plain `uint32_t` field round-trips between them, but the *types* stay distinct.

Example: `wapi_font_style_t`, `wapi_text_font_style_t`, and `wapi_dialog_font_style_t` all exist, all use `NORMAL=0 / ITALIC=1 / OBLIQUE=2`. A module that reads a style from the font picker and passes it into text shaping does so through raw `uint32_t` round-tripping — there is no shared C type binding the two capabilities together.

**Why:**
- A host can implement `wapi_text` without ever touching `wapi_font`. No transitive ABI surface.
- Each header compiles in isolation; there is no cross-capability build graph to manage.
- There is nothing to track in a cross-capability dependency system, because there are no cross-capability dependencies.
- A module can link against exactly the capabilities it needs without pulling in vocabulary from others.

**Cost:** syntactic duplication of small enums. This is deliberate. Duplicating a 5-line enum is cheaper than shipping a dependency graph.

**What's shared anyway:**
- `wapi.h` — the universal header with calling conventions, result codes, handle system, and vtables. Every capability header includes it.
- Wire values (integer constants) within a logical family (font style, pixel format, key codes) are kept consistent across capabilities so that round-tripping works. Consistency is maintained by convention and review, not by a shared C type.

### 6.2 Import Annotation

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
(import "wapi" "io_get" (func ...))
(import "wapi_gpu" "request_device" (func ...))
```

---

## 7. Async I/O Model

### 7.1 Design

The WAPI uses a **submit + unified event queue** async I/O model inspired by Linux's `io_uring`. All asynchronous operations (file reads, network sends, audio buffer fills, timer waits) are submitted through the `wapi_io_t` vtable. Completions arrive as events in the same unified event queue as input, window, and lifecycle events.

There is no `async`/`await`. There are no colored functions. There are no new types for futures or promises. The module submits operation descriptors via the vtable's `submit` function, does other work, and then polls or waits for completion events via `poll` / `wait`.

This eliminates the function coloring problem for library composition. Because the I/O primitive is always the same (submit descriptors, poll for completions), there is no split between "sync" and "async" libraries. Every library uses the same interface regardless of how it structures its internal control flow. A library that accepts `const wapi_io_t*` works identically whether backed by the real host, a logging wrapper, a mock, or a throttler.

### 7.2 The `wapi_io_t` Vtable

All I/O, event handling, and capability queries go through the `wapi_io_t` vtable:

```c
typedef struct wapi_io_t {
    void*         impl;
    int32_t       (*submit)(void* impl, const wapi_io_op_t* ops, wapi_size_t count);
    wapi_result_t (*cancel)(void* impl, uint64_t user_data);
    int32_t       (*poll)(void* impl, wapi_event_t* event);
    int32_t       (*wait)(void* impl, wapi_event_t* event, int32_t timeout_ms);
    void          (*flush)(void* impl, uint32_t event_type);
    wapi_bool_t   (*cap_supported)(void* impl, wapi_string_view_t name);
    wapi_result_t (*cap_version)(void* impl, wapi_string_view_t name, wapi_version_t* ver);
    wapi_result_t (*cap_query)(void* impl, wapi_string_view_t cap, wapi_cap_state_t* state);
} wapi_io_t;
```

A module obtains its vtable in one of two ways:

**Module-owned (from the host):** Call `wapi_io_get()`. Returns the host-determined vtable for the calling module. This is host-controlled and cannot be influenced by any parent — safe for shared instances. For top-level apps, the host returns a fully-capable vtable. For child modules, the host returns a sandbox-only vtable (see Section 7.8).

**Per-call (from caller):** Accept `const wapi_io_t*` as a function parameter. The caller passes its own vtable (or a wrapped/filtered version). This is how a module gets capabilities beyond its sandbox.

```c
/* Module-owned — sandbox only for children */
const wapi_io_t* io = wapi_io_get();

/* Per-call — caller provides real filesystem access */
void my_library_func(const wapi_io_t* io, const wapi_allocator_t* alloc, ...);
```

The caller can wrap the vtable for logging, throttling, or filtering:

```c
static int32_t logging_submit(void* impl, const wapi_io_op_t* ops, wapi_size_t count) {
    log_state_t* state = (log_state_t*)impl;
    log("submitting %d ops", count);
    return state->inner->submit(state->inner->impl, ops, count);
}
```

**Vtable scope and module boundaries.** Within a single Wasm module (build-time linked libraries), vtable function pointers are ordinary `call_indirect` targets — passing and wrapping `const wapi_io_t*` between functions works directly because all code shares the same linear memory and function table. Across runtime-linked module boundaries, vtable pointers cannot be passed directly: each module has its own isolated linear memory and function table, so a function pointer valid in one module is meaningless in another. The host mediates cross-module vtable passing via proxy vtables (see [Section 10.7](#107-io-for-runtime-modules)).

### 7.3 Operation Descriptor (`wapi_io_op_t`)

Each operation is described by a fixed-size **80-byte descriptor**. Address and size fields are `uint64_t` so the layout is identical for wasm32 and wasm64 (wasm32 modules zero-extend their 32-bit values):

```
Byte Layout (80 bytes, alignment 8):

Offset  Size  Type      Field        Description
------  ----  --------  -----------  -----------------------------------
 0       4    uint32_t  opcode       Operation type (wapi_io_opcode_t)
 4       4    uint32_t  flags        Operation flags
 8       4    int32_t   fd           Handle / file descriptor
12       4    uint32_t  flags2       Additional operation-specific flags
16       8    uint64_t  offset       File offset or timeout (nanoseconds)
24       8    uint64_t  addr         Pointer to buffer
32       8    uint64_t  len          Buffer length
40       8    uint64_t  addr2        Second pointer (path, etc.)
48       8    uint64_t  len2         Second length
56       8    uint64_t  user_data    Opaque, echoed in completion event
64       8    uint64_t  result_ptr   Pointer for output values
72       8    uint8_t[] reserved     Reserved (must be zero)
```

### 7.4 Operation Codes

Selected opcodes (full enum in `wapi.h`). All values are method-ids in namespace `0x0000`.

| Opcode                            | Value  | Cap                | Description                                  |
|-----------------------------------|--------|--------------------|----------------------------------------------|
| `WAPI_IO_OP_NOP`                  | `0x00` | (none)             | No operation (fence)                         |
| `WAPI_IO_OP_CAP_REQUEST`          | `0x01` | (universal)        | Request grant for a capability by name       |
| `WAPI_IO_OP_READ`                 | `0x02` | `wapi.filesystem`  | Read from fd into buffer                     |
| `WAPI_IO_OP_WRITE`                | `0x03` | `wapi.filesystem`  | Write buffer to fd                           |
| `WAPI_IO_OP_OPEN`                 | `0x04` | `wapi.filesystem`  | Open a host-fs file                          |
| `WAPI_IO_OP_CLOSE`                | `0x05` | `wapi.filesystem`  | Close a handle                               |
| `WAPI_IO_OP_STAT`                 | `0x06` | `wapi.filesystem`  | Stat a file                                  |
| `WAPI_IO_OP_LOG`                  | `0x07` | `wapi.log`         | Log message (fire-and-forget)                |
| `WAPI_IO_OP_FWATCH_ADD`           | `0x08` | `wapi.filesystem`  | Watch a host-fs path                         |
| `WAPI_IO_OP_FWATCH_REMOVE`        | `0x09` | `wapi.filesystem`  | Stop a host-fs watch                         |
| `WAPI_IO_OP_CONNECT`              | `0x0A` | `wapi.network`     | Open a connection                            |
| `WAPI_IO_OP_ACCEPT`               | `0x0B` | `wapi.network`     | Accept a connection                          |
| `WAPI_IO_OP_SEND`                 | `0x0C` | `wapi.network`     | Send data                                    |
| `WAPI_IO_OP_RECV`                 | `0x0D` | `wapi.network`     | Receive data                                 |
| `WAPI_IO_OP_NETWORK_LISTEN`       | `0x0E` | `wapi.network`     | Listen for connections                       |
| `WAPI_IO_OP_NETWORK_CHANNEL_OPEN` | `0x0F` | `wapi.network`     | Open a channel on a multiplexed conn         |
| `WAPI_IO_OP_NETWORK_CHANNEL_ACCEPT`| `0x10` | `wapi.network`    | Accept an inbound channel                    |
| `WAPI_IO_OP_NETWORK_RESOLVE`      | `0x11` | `wapi.network`     | DNS resolve                                  |
| `WAPI_IO_OP_TIMEOUT`              | `0x14` | `wapi.clock`       | Wait for a duration                          |
| `WAPI_IO_OP_TIMEOUT_ABS`          | `0x15` | `wapi.clock`       | Wait until an absolute time                  |
| `WAPI_IO_OP_ROLE_REQUEST`         | `0x16` | (per-kind)         | Request endpoint(s) for role(s) (see §9.10)  |
| `WAPI_IO_OP_ROLE_REPICK`          | `0x17` | (per-kind)         | Re-pick the endpoint behind a role handle    |
| `WAPI_IO_OP_AUDIO_WRITE`          | `0x1E` | `wapi.audio`       | Submit audio samples for playback            |
| `WAPI_IO_OP_AUDIO_READ`           | `0x1F` | `wapi.audio`       | Read captured audio samples                  |
| `WAPI_IO_OP_SANDBOX_OPEN`         |`0x2A0` | `wapi.sandbox`     | Open a sandbox-fs file (module-scoped)       |
| `WAPI_IO_OP_SANDBOX_FWATCH_ADD`   |`0x2A3` | `wapi.sandbox`     | Watch a sandbox-fs path                      |
| `WAPI_IO_OP_CACHE_OPEN`           |`0x2B0` | `wapi.cache`       | Open a cache-fs file (instance-scoped)       |

**`WAPI_IO_OP_LOG` field mapping:**

| Field       | Usage                                                    |
|-------------|----------------------------------------------------------|
| `flags`     | Log level: `WAPI_LOG_DEBUG` (0), `WAPI_LOG_INFO` (1), `WAPI_LOG_WARN` (2), `WAPI_LOG_ERROR` (3) |
| `addr/len`  | UTF-8 message text                                       |
| `addr2/len2`| Optional tag/category (e.g. "render", "audio"); 0/0 if none |

Log is fire-and-forget — the host processes it immediately (writes to stderr, forwards to console, etc.). No completion event is pushed. `user_data` is ignored.

### 7.5 Completion Events (`wapi_io_event_t`)

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

### 7.6 Submit and Cancel

Operations are submitted through the `wapi_io_t` vtable:

```c
const wapi_io_t* io = wapi_io_get();

/* Submit a batch of operations. Returns count submitted, or negative error.
 * Completions arrive as WAPI_EVENT_IO_COMPLETION events. */
int32_t count = io->submit(io->impl, ops, n);

/* Cancel a pending operation by user_data. */
wapi_result_t result = io->cancel(io->impl, user_data);
```

There is no separate I/O completion queue. All completions flow through the unified event queue via `io->poll()` and `io->wait()`, alongside input, window, and lifecycle events.

### 7.7 Usage Patterns

The same API supports fundamentally different application architectures:

- **Game loop:** Submit asset loads, render a frame, poll events at the end of the frame — I/O completions and input arrive in the same queue.
- **CLI tool:** Submit a file read, call `io->wait()` to block until the I/O completion event arrives.
- **Audio processor:** Poll events every audio callback cycle for completed buffer fills.
- **Server:** Submit accepts and receives, wait for completion events in a loop.

### 7.8 Sandboxed Filesystems

Every module has access to two private filesystems through its module-owned `wapi_io_t` (from `wapi_io_get()`). Neither requires any permission.

**Transient (per-instance):** Dies when the module instance is unloaded. Each instance gets its own. Use cases: temp files, scratch buffers, intermediate results.

| Opcode                    | Value   | Description                     |
|---------------------------|---------|---------------------------------|
| `WAPI_IO_OP_SANDBOX_OPEN`  | `0x2A0` | Open/create file in transient sandbox |
| `WAPI_IO_OP_SANDBOX_STAT`  | `0x2A1` | Stat a file in transient sandbox |
| `WAPI_IO_OP_SANDBOX_DELETE` | `0x2A2` | Delete a file in transient sandbox |

**Persistent (per-module, shared by content hash):** Survives across instances and app restarts. All instances of the same module (same content hash) share this filesystem. The host manages concurrency (read-many, write-serialized). Use cases: lookup tables, precomputed indices, caches.

| Opcode                   | Value   | Description                      |
|--------------------------|---------|----------------------------------|
| `WAPI_IO_OP_CACHE_OPEN`   | `0x2B0` | Open/create file in persistent cache |
| `WAPI_IO_OP_CACHE_STAT`   | `0x2B1` | Stat a file in persistent cache  |
| `WAPI_IO_OP_CACHE_DELETE`  | `0x2B2` | Delete a file in persistent cache |

Both use relative paths (e.g., `"tables.bin"`, `"index/trigrams.dat"`). Read and write on returned file descriptors use standard `WAPI_IO_OP_READ` / `WAPI_IO_OP_WRITE`.

```c
/* Example: shared library loading tables from persistent cache */
const wapi_io_t* io = wapi_io_get();

wapi_io_op_t op = {0};
op.opcode = WAPI_IO_OP_CACHE_OPEN;
op.addr   = (uint64_t)(uintptr_t)"tables.bin";
op.len    = 10;
op.flags  = WAPI_OPEN_READ;
io->submit(io->impl, &op, 1);
/* ... wait for completion, read from returned fd ... */
```

---

## 8. Module Entry Points

### 8.1 `wapi_main`

```wat
(func (export "wapi_main") (result i32))
```

**C signature:**

```c
WAPI_EXPORT(wapi_main) wapi_result_t wapi_main(void);
```

Called by the host after module instantiation and memory initialization. The module obtains its vtables and queries capabilities:

```c
wapi_result_t wapi_main(void) {
    const wapi_io_t* io = wapi_io_get();
    const wapi_allocator_t* alloc = wapi_allocator_get();

    if (wapi_cap_supported(io, WAPI_STR(WAPI_CAP_GPU))) {
        /* Request GPU device, create surfaces, etc. */
    }

    my_library_init(io, alloc);
    return WAPI_OK;
}
```

Every module — top-level or child — is written identically:

1. Call `wapi_io_get()` and `wapi_allocator_get()` for module-owned vtables.
2. Query capabilities via `wapi_cap_supported()` (wrapping `io->cap_supported()`).
3. For headless applications: perform work and return `WAPI_OK` to exit.
4. For graphical applications: create surfaces, request GPU device, and return `WAPI_OK` to begin the frame loop.
5. Pass vtables explicitly to libraries that need I/O or allocation.

**Panic reporting:** When a module encounters an unrecoverable error, it calls the `wapi_panic_report` host import to record the message, then traps. The import is NOT noreturn — it records and returns; the module then hits `__builtin_trap()` (`unreachable` in wasm). The host knows which module is calling and can route the message to stderr, console, or the parent's handler.

When a parent module calls a child via `wapi_module_call` and the child traps, the runtime catches the trap and returns an error to the parent (not a trap). The child module is faulted and should be released.

```c
static inline _Noreturn void wapi_panic(const char* msg, uint64_t msg_len) {
    wapi_panic_report(msg, msg_len);
    __builtin_trap();
}
```

Returns `WAPI_OK` on success, or a negative error code to abort the module.

### 8.2 `wapi_frame`

```wat
(func (export "wapi_frame") (param i64) (result i32))
```

**C signature:**

```c
WAPI_EXPORT(wapi_frame) wapi_result_t wapi_frame(wapi_timestamp_t timestamp);
```

Called each frame for graphical applications. The host calls this at the display's refresh rate (typically 60 Hz, 120 Hz, or 144 Hz). The `timestamp` parameter is the current monotonic time in nanoseconds.

The host waits for `wapi_frame` to return before scheduling the next call. If a frame overruns the refresh interval, the host skips the missed deadline(s) and calls again at the next vsync. The module computes elapsed time by differencing consecutive timestamps (i.e. `dt = current_timestamp - previous_timestamp`) and uses that delta to advance its simulation.

The module should:

1. Poll events via `io->poll()` (input, I/O completions, lifecycle).
2. Update application state.
3. Render via WebGPU.
4. Return `WAPI_OK` to continue, or `WAPI_ERR_CANCELED` to request exit.

If the module does not export `wapi_frame`, the host does not enter a frame loop. This is the expected behavior for headless applications.

---

## 9. Capabilities

### 9.1 Unified Capability Model

All capabilities — foundational (memory, filesystem, clock) or specialized (geolocation, camera, bluetooth) — use a single, unified lifecycle: **check support → query state → request → use**. There is no distinction between "core" and "extension" capabilities, and there are no "free" capabilities that bypass the grant flow.

Every capability, including the ones that look ambient, routes grant acquisition through the same `WAPI_IO_OP_CAP_REQUEST` opcode with its capability name. Whether that translates to a user-visible prompt, an auto-grant, or an auto-deny is a platform decision. On browser hosts `wapi.clock` auto-grants silently; on an enterprise-managed VM `wapi.network` might auto-deny; on a phone `wapi.geolocation` shows the system prompt. The module's code is identical across all three because the flow is one rubric, not per-capability specialization.

Capability names are **dot-separated strings** following a hierarchical namespace:

```
wapi.<module>          Spec-defined capabilities
vendor.<name>.*      Vendor-specific capabilities
```

The `WAPI_IO_OP_CAP_REQUEST` opcode is the single grant-request surface. Do not introduce capability-specific grant opcodes — every caller, for every capability, submits `WAPI_IO_OP_CAP_REQUEST` with its capability name and lets the platform decide.

### 9.2 Capability Check and Request Flow

Capability queries go through the `wapi_io_t` vtable's `cap_supported`, `cap_version`, and `cap_query` function pointers, wrapped by inline helpers `wapi_cap_supported` / `wapi_cap_version` / `wapi_cap_query` declared in `wapi.h`. Using a capability involves up to three steps, depending on whether the capability requires user grant:

**Step 1 — Check support.** The module calls `wapi_cap_supported()` to determine whether the host provides a capability at all. If the host does not support it, the capability is permanently unavailable — there is nothing to request.

**Step 2 — Query grant state.** For capabilities that require user consent (geolocation, camera, notifications, etc.), the module calls `wapi_cap_query()` to check the current grant state without triggering a prompt. The result is one of:

| State              | Value | Meaning                                                  |
|--------------------|-------|----------------------------------------------------------|
| `WAPI_CAP_GRANTED` |   0   | Capability granted — the module may use it               |
| `WAPI_CAP_DENIED`  |   1   | Capability denied — do not prompt again                  |
| `WAPI_CAP_PROMPT`  |   2   | Not yet requested — prompting is possible                |

**Step 3 — Request grant.** If the state is `WAPI_CAP_PROMPT`, the module calls `wapi_cap_request()` (which submits a `WAPI_IO_OP_CAP_REQUEST` via `io->submit()`). This is an async I/O operation — the host may show a platform-native permission dialog. The completion event writes the resulting `wapi_cap_state_t` to `result_ptr`.

```
WAPI_IO_OP_CAP_REQUEST (0x01):
  addr/len   = capability name string (e.g., "wapi.geolocation")
  result_ptr = pointer to wapi_cap_state_t (written on completion)
```

Not all capabilities require an explicit user grant. Foundational capabilities like memory, clock, and I/O auto-grant immediately after `wapi_cap_supported()` returns true. Capabilities that access sensitive resources (location, camera, microphone, contacts, notifications) typically require a grant. The host decides which capabilities are gated — the module should always check.

**Complete example:**

```c
const wapi_io_t* io = wapi_io_get();

/* 1. Check if the host supports geolocation at all */
if (!wapi_cap_supported(io, WAPI_STR(WAPI_CAP_GEOLOCATION))) {
    /* Not available on this host — fall back or skip */
    return;
}

/* 2. Query current grant state (no prompt) */
wapi_cap_state_t state;
wapi_cap_query(io, WAPI_STR(WAPI_CAP_GEOLOCATION), &state);

if (state == WAPI_CAP_DENIED) {
    /* User previously denied — respect the decision */
    return;
}

if (state == WAPI_CAP_PROMPT) {
    /* 3. Request grant (async — triggers platform prompt) */
    wapi_cap_request(io, WAPI_STR(WAPI_CAP_GEOLOCATION),
                     &state, MY_CAP_REQUEST_TAG);
    /* Handle the completion event in the poll loop */
    return;
}

/* state == WAPI_CAP_GRANTED — use the capability */
```

### 9.3 Capability Query API

Capability queries are function pointers on the `wapi_io_t` vtable, wrapped by inline helpers in `wapi.h`. String parameters use `wapi_string_view_t` (a 16-byte struct with `uint64_t data` and `uint64_t length`).

```c
const wapi_io_t* io = wapi_io_get();

/* Check if a capability is supported by name. Returns WAPI_TRUE/WAPI_FALSE. */
wapi_cap_supported(io, name);

/* Get the version of a supported capability. */
wapi_cap_version(io, name, &version);

/* Query grant state for a capability (no prompt). */
wapi_cap_query(io, capability, &state);

/* Submit a grant request (async). */
wapi_cap_request(io, capability, &state, user_data);
```

### 9.4 Spec-Defined Capability Names

The following capability names are defined by the spec. Each has a corresponding `WAPI_CAP_*` define in `wapi.h`. Hosts report support for each independently.

| Capability Name          | Header Define              | Description                                    |
|--------------------------|----------------------------|------------------------------------------------|
| `wapi.env`               | `WAPI_CAP_ENV`             | Arguments, environment variables, random bytes, exit |
| `wapi.clock`             | `WAPI_CAP_CLOCK`           | Monotonic and wall clocks, performance counter |
| `wapi.filesystem`        | `WAPI_CAP_FILESYSTEM`      | Capability-based filesystem                    |
| `wapi.network`           | `WAPI_CAP_NETWORK`         | QUIC/WebTransport networking                   |
| `wapi.gpu`               | `WAPI_CAP_GPU`             | WebGPU bridge (device, surface, proc table)    |
| `wapi.surface`           | `WAPI_CAP_SURFACE`         | Render targets (on-screen and offscreen)       |
| `wapi.window`            | `WAPI_CAP_WINDOW`          | OS window management (title, fullscreen, etc.) |
| `wapi.display`           | `WAPI_CAP_DISPLAY`         | Display enumeration, geometry, sub-pixels      |
| `wapi.input`             | `WAPI_CAP_INPUT`           | Input events (keyboard, mouse, touch, gamepad) |
| `wapi.audio`             | `WAPI_CAP_AUDIO`           | Audio playback and recording                   |
| `wapi.audioplugin`       | `WAPI_CAP_AUDIO_PLUGIN`    | Audio plugin hosting (VST-style)               |
| `wapi.content`           | `WAPI_CAP_CONTENT`         | Host-rendered text, images, media              |
| `wapi.transfer`          | `WAPI_CAP_TRANSFER`        | Unified clipboard / DnD / share                |
| `wapi.seat`              | `WAPI_CAP_SEAT`            | Seat enumeration (no permission required)      |
| `wapi.font`              | `WAPI_CAP_FONT`            | Font system queries and enumeration            |
| `wapi.video`             | `WAPI_CAP_VIDEO`           | Video/media playback                           |
| `wapi.geolocation`       | `WAPI_CAP_GEOLOCATION`     | GPS / location services                        |
| `wapi.notifications`     | `WAPI_CAP_NOTIFICATIONS`   | System notifications                           |
| `wapi.sensors`           | `WAPI_CAP_SENSORS`         | Accelerometer, gyroscope, compass              |
| `wapi.speech`            | `WAPI_CAP_SPEECH`          | Speech recognition and synthesis               |
| `wapi.crypto`            | `WAPI_CAP_CRYPTO`          | Hardware-accelerated cryptography              |
| `wapi.biometric`         | `WAPI_CAP_BIOMETRIC`       | Fingerprint, face recognition                  |
| `wapi.kvstorage`         | `WAPI_CAP_KV_STORAGE`      | Persistent key-value storage                   |
| `wapi.payments`          | `WAPI_CAP_PAYMENTS`        | In-app purchases / payment processing          |
| `wapi.usb`               | `WAPI_CAP_USB`             | USB device access                              |
| `wapi.midi`              | `WAPI_CAP_MIDI`            | MIDI device access                             |
| `wapi.bluetooth`         | `WAPI_CAP_BLUETOOTH`       | Bluetooth device access                        |
| `wapi.camera`            | `WAPI_CAP_CAMERA`          | Camera capture                                 |
| `wapi.xr`               | `WAPI_CAP_XR`              | VR/AR (WebXR, OpenXR)                          |
| `wapi.module`            | `WAPI_CAP_MODULE`          | Runtime module loading, cross-module calls     |
| `wapi.thread`            | `WAPI_CAP_THREAD`          | Threads, TLS, and synchronization primitives   |
| `wapi.process`           | `WAPI_CAP_PROCESS`         | Process spawning and management                |
| `wapi.dialog`            | `WAPI_CAP_DIALOG`          | File/save/message dialogs                      |
| `wapi.sysinfo`           | `WAPI_CAP_SYSINFO`         | System information queries                     |
| `wapi.eyedrop`           | `WAPI_CAP_EYEDROP`         | Screen color picker                            |
| `wapi.contacts`          | `WAPI_CAP_CONTACTS`        | Contact picker and icon access                 |
| `wapi.http`              | `WAPI_CAP_HTTP`            | One-shot HTTP requests (`WAPI_IO_OP_HTTP_FETCH`) |
| `wapi.compression`       | `WAPI_CAP_COMPRESSION`     | Compression / decompression (`WAPI_IO_OP_COMPRESS_PROCESS`) |
| `wapi.user`              | `WAPI_CAP_USER`            | Current-user identity (login / display / email / UPN / id / avatar) |

### 9.5 Vendor Capabilities

Vendors may define custom capabilities using the `vendor.<vendor_name>.*` namespace. For example:

- `vendor.nintendo.joycon` Nintendo Joy-Con haptics
- `vendor.apple.pencil` Apple Pencil force/tilt data
- `vendor.valve.steamdeck` Steam Deck-specific features

Vendor capabilities use the same `wapi_cap_supported()` / `wapi_cap_version()` query mechanism. Hosts MUST ignore vendor capability queries they do not recognize (returning false from `cap_supported`).

**Constraints on vendor capabilities:**

- Vendor capabilities MUST NOT duplicate functionality that has a spec-defined equivalent. If `wapi.gpu` provides a feature, a vendor MUST NOT redefine it under `vendor.<name>.gpu`.
- Modules MAY require vendor capabilities (e.g., a Joy-Con haptics demo that only makes sense on Nintendo hardware). There is no requirement for fallback paths. A module that requires a capability simply will not run on hosts that lack it. The capability query mechanism ensures the module can detect this at startup and report a clear error.
- Frequently-used vendor capabilities SHOULD be considered for promotion to spec-defined capabilities in future versions.

### 9.6 Capability Versioning

Each capability reports its own version via `wapi_cap_version()`. This allows the module to detect the specific feature level within a capability:

```c
const wapi_io_t* io = wapi_io_get();
wapi_version_t v;
if (wapi_cap_supported(io, WAPI_STR(WAPI_CAP_GEOLOCATION))) {
    wapi_cap_version(io, WAPI_STR(WAPI_CAP_GEOLOCATION), &v);
    /* v.major, v.minor, v.patch */
}
```

### 9.7 Opcode Encoding and Namespace Registration

IO opcodes passed to `wapi_io_t::submit` are packed `(namespace_id:u16 << 16) | method_id:u16`. The encoding is a strict partition of the 32-bit opcode space:

| Namespace range    | Assignment                                               |
|--------------------|----------------------------------------------------------|
| `0x0000`           | Every first-party spec-defined capability — file, network, timers, audio, crypto, transfer, barcode, power, sensor, notify, font, sandbox, cache, etc. Low 16 bits are the method-id; the spec carves the method-id space into per-capability sub-ranges (see `wapi.h`). All ops gate through the same grant lifecycle (§9.4); namespace `0x0000` is an opcode-layout artifact, **not** a privilege tier. |
| `0x0001 – 0xFFFF`  | Vendor extensions. Dynamically registered per session via `wapi_io_t::namespace_register`. Not assigned by the spec; minted by the host on demand. Modules agree on **names** (DNS-style, `"com.acme.ml"`), never on numeric ids. |

Unknown opcode is a **runtime error, not a trap**: `submit` accepts any opcode; completion fires with `result = WAPI_ERR_NOSYS` and `flags |= WAPI_IO_CQE_F_NOSYS`. This lets a host register handlers lazily and lets vendor opcodes submitted on a host that doesn't understand them produce diagnosable completions instead of crashing.

**Vendor registration.** Two new methods on `wapi_io_t`:

```c
wapi_result_t (*namespace_register)(void* impl, wapi_stringview_t name,
                                    uint16_t* out_id);
wapi_result_t (*namespace_name)(void* impl, uint16_t id,
                                char* buf, wapi_size_t buf_len,
                                wapi_size_t* name_len);
```

Modules call `namespace_register` at init with a DNS-style name (`"com.acme.ml"`), cache the returned id, and compose opcodes with `WAPI_NS(id, method)`. The id is stable for the lifetime of the vtable impl: registering the same name twice returns the same id. Two modules loaded against the same host vtable and registering the same name receive the same id — the host's registry is the single source of truth.

**Modules never hardcode vendor namespace ids.** They agree on **names**. A vendor publishes a namespace-name plus per-method contract (method number, op-descriptor field semantics, result shape), the same way the core spec documents namespace `0x0000`. No central coordination, no allocation policy, no registry operator — the DNS name is the identifier, and two companies owning the same name is as impossible as two companies owning `acme.com`.

**Sandbox isolation.** Sandbox vtables (§7.8) MUST return `WAPI_ERR_NOTCAPABLE` from `namespace_register` and `namespace_name`. A child module gains vendor-opcode access only via a caller-supplied wrapped vtable its parent explicitly hands in (§10.7).

### 9.8 Inline Result Payload

The I/O completion event `wapi_io_event_t` reserves 96 bytes after the correlation fields for **inline result payload**. When a host sets `WAPI_IO_CQE_F_INLINE` in `flags`, the full operation result has been written to `payload[0..95]` — the module does not need to pre-allocate an output buffer or dereference `result_ptr` for that op.

Inlineability is decided per-opcode at spec time. Results that fit (up to 96 bytes) inline; larger results (HTTP bodies, file reads) still flow through `result_ptr`.

Known inline payload layouts:

| Opcode                                | `payload` bytes                                     |
|---------------------------------------|-----------------------------------------------------|
| `WAPI_IO_OP_CAP_REQUEST`              | `0..3` = `wapi_cap_state_t` (u32)                   |
| `WAPI_IO_OP_EYEDROPPER_PICK`          | `0..3` = RGBA u32                                   |
| `WAPI_IO_OP_DIALOG_PICK_COLOR`        | `0..3` = RGBA u32                                   |
| `WAPI_IO_OP_DIALOG_MESSAGEBOX`        | `0..3` = button id (u32)                            |
| `WAPI_IO_OP_GEO_POSITION_GET`         | `0..47` = `wapi_geo_position_t`                     |
| `WAPI_IO_OP_CRYPTO_HASH`              | `0..(digest_len-1)` = digest bytes; `result` = len  |
| `WAPI_IO_OP_CRYPTO_SIGN`              | up to 64B = signature; `result` = len               |
| `WAPI_IO_OP_CRYPTO_VERIFY`            | `result` = 1 valid / 0 invalid; no payload          |
| `WAPI_IO_OP_XR_HIT_TEST`              | `0..27` = `wapi_xr_pose_t`                          |
| `WAPI_IO_OP_POWER_INFO_GET`           | `0..15` = `wapi_power_info_t`                       |

### 9.9 Presets

Presets are convenience arrays of capability name strings that give developers a stable target. A host claims conformance to a preset by supporting all capabilities in the array. Presets are not exclusive; a host may support additional capabilities beyond its preset.

**Preset Definitions:**

```c
static const char* const WAPI_PRESET_EMBEDDED[] = {
    "wapi.env", "wapi.clock", NULL
};

static const char* const WAPI_PRESET_HEADLESS[] = {
    "wapi.env", "wapi.clock", "wapi.filesystem", "wapi.network",
    "wapi.sysinfo", "wapi.crypto",
    "wapi.thread", "wapi.process", "wapi.module", NULL
};

static const char* const WAPI_PRESET_COMPUTE[] = {
    "wapi.env", "wapi.clock", "wapi.filesystem", "wapi.network",
    "wapi.sysinfo", "wapi.crypto",
    "wapi.gpu", "wapi.thread", "wapi.process",
    "wapi.module", NULL
};

static const char* const WAPI_PRESET_AUDIO[] = {
    "wapi.env", "wapi.clock", "wapi.filesystem",
    "wapi.audio", "wapi.thread", "wapi.module", NULL
};

static const char* const WAPI_PRESET_GRAPHICAL[] = {
    "wapi.env", "wapi.clock", "wapi.filesystem", "wapi.network",
    "wapi.sysinfo", "wapi.crypto",
    "wapi.gpu", "wapi.surface", "wapi.window", "wapi.display",
    "wapi.input", "wapi.audio", "wapi.content", "wapi.transfer",
    "wapi.font", "wapi.dialog",
    "wapi.thread", "wapi.process", "wapi.module", NULL
};

static const char* const WAPI_PRESET_MOBILE[] = {
    "wapi.env", "wapi.clock", "wapi.filesystem", "wapi.network",
    "wapi.sysinfo", "wapi.crypto",
    "wapi.gpu", "wapi.surface", "wapi.window", "wapi.display",
    "wapi.input", "wapi.audio", "wapi.content", "wapi.transfer",
    "wapi.font",
    "wapi.thread",
    "wapi.geolocation", "wapi.camera", "wapi.notifications",
    "wapi.sensors", "wapi.biometric", "wapi.module", NULL
};
```

**Preset summary:**

| Preset       | Includes                                      | Use Cases                                  |
|--------------|-----------------------------------------------|--------------------------------------------|
| **Embedded** | env, clock                                    | Microcontrollers, RTOS, bare-metal firmware |
| **Headless** | env, clock, filesystem, network, sysinfo, crypto, thread, sync, process, module | Servers, CLI tools, edge functions |
| **Compute**  | Headless + gpu                                | ML inference, video transcoding, simulations |
| **Audio**    | env, clock, filesystem, audio, thread, sync, module | VST plugins, audio processing, voice |
| **Graphical**| Headless + gpu, surface, window, display, input, audio, content, transfer, font, dialog | Apps, games, creative tools |
| **Mobile**   | Headless + gpu, surface, window, display, input, audio, content, transfer, font, geolocation, camera, notifications, sensors, biometric | Mobile apps |

**Checking preset conformance:**

The `wapi_preset_supported` inline helper iterates the preset array and checks each capability via the I/O vtable:

```c
static inline wapi_bool_t wapi_preset_supported(const wapi_io_t* io,
                                                 const char* const* preset) {
    for (int i = 0; preset[i] != NULL; i++) {
        if (!wapi_cap_supported(io, WAPI_STR(preset[i]))) return 0;
    }
    return 1;
}
```

Usage:

```c
const wapi_io_t* io = wapi_io_get();
if (wapi_preset_supported(io, WAPI_PRESET_GRAPHICAL)) {
    /* Host supports the full graphical stack */
}
```

### 9.10 Role System

Capabilities grant the *kind* of access an app is allowed ("this app may use wapi.audio"). **Roles** bind that access to a specific device instance — an *endpoint*. Opening any device across the platform (audio playback, audio recording, camera, midi, keyboard, mouse, gamepad, haptic, sensor, display, hid, touch, pen, pointer) is a role request.

Apps **do not enumerate devices before grant**. The runtime is the enumerator and the user-facing picker; the app describes roles it needs, the runtime resolves them to specific endpoints (via policy or prompt), and the app receives bound handles. Post-grant enumeration is available via `WAPI_ROLE_ALL` for apps that need every granted endpoint of a kind.

#### 9.10.1 Role Kinds

```c
typedef enum wapi_role_kind_t {
    WAPI_ROLE_AUDIO_PLAYBACK  = 0x01,
    WAPI_ROLE_AUDIO_RECORDING = 0x02,
    WAPI_ROLE_CAMERA          = 0x03,
    WAPI_ROLE_MIDI_INPUT      = 0x04,
    WAPI_ROLE_MIDI_OUTPUT     = 0x05,
    WAPI_ROLE_KEYBOARD        = 0x06,
    WAPI_ROLE_MOUSE           = 0x07,
    WAPI_ROLE_GAMEPAD         = 0x08,
    WAPI_ROLE_HAPTIC          = 0x09,
    WAPI_ROLE_SENSOR          = 0x0A,
    WAPI_ROLE_DISPLAY         = 0x0B,
    WAPI_ROLE_HID             = 0x0C,
    WAPI_ROLE_TOUCH           = 0x0D,
    WAPI_ROLE_PEN             = 0x0E,
    WAPI_ROLE_POINTER         = 0x0F, /* any pointer-shaped endpoint */
} wapi_role_kind_t;
```

`wapi_role_kind_t` is the central registry of device kinds. Adding a kind is an explicit `wapi.h` edit, not an organic module extension. `wapi_input.h` uses this enum directly — there is no separate `wapi_device_type_t`.

**A single physical device may fulfill multiple role kinds simultaneously, sharing one UID.** A mouse surfaces `HID` (OS permitting) + `MOUSE` + `POINTER`. A DualSense surfaces `HID` (OS permitting) + `GAMEPAD` + `POINTER` (via its touchpad) + `HAPTIC` + `AUDIO_PLAYBACK` + `AUDIO_RECORDING`. The runtime decides which roles a device can fulfill based on hardware capability and platform policy; apps request the role they want and receive an endpoint handle scoped to that role. Cross-kind correlation (e.g. "this POINTER is my gamepad's touchpad") uses UID comparison or `target_uid` targeting.

`WAPI_ROLE_POINTER` specifically covers **any endpoint that produces pointer-shaped (x, y, button) events** — mouse, touchscreen, pen digitizer, gamepad touchpad, trackball-over-HID, etc. Multi-player apps that want per-player pointers request `POINTER` with `target_uid` set to each player's gamepad UID.

#### 9.10.2 Request Flow

```
App                         Runtime                      User
 |                             |                           |
 |--- ROLE_REQUEST([roles])--->|                           |
 |                             |-- policy check            |
 |                             |-- ambient-grant silently  |
 |                             |-- prompt if sensitive --->|
 |                             |<---------- picks devices--|
 |<-- handles (1 per role) ----|                           |
 |                             |                           |
 |--- query endpoint metadata  |                           |
 |--- use (push/pull/send/...) |                           |
 |                             |                           |
 |--- ROLE_REPICK(handle) ---->|                           |
 |                             |-- show picker ----------->|
 |                             |<------ user picks --------|
 |<-- rebound or new handle ---|                           |
```

Two opcodes on the `wapi_io_t` vtable:

- `WAPI_IO_OP_ROLE_REQUEST` (0x16): `addr/len` = array of `wapi_role_request_t`, `flags2` = count. Runtime may prompt once for the whole batch.
- `WAPI_IO_OP_ROLE_REPICK` (0x17): `fd` = existing endpoint handle, `result_ptr` = `wapi_handle_t*` for the rebound handle.

Inline helpers in `wapi.h`: `wapi_role_request()`, `wapi_role_repick()`.

#### 9.10.3 Role Request Descriptor

```c
typedef struct wapi_role_request_t {
    uint32_t kind;           /* wapi_role_kind_t */
    uint32_t flags;          /* wapi_role_flags_t bitmask */
    uint64_t prefs_addr;     /* kind-specific prefs struct */
    uint32_t prefs_len;
    uint32_t _pad;
    uint64_t out_handle;     /* wapi_handle_t* or handle array for ALL */
    uint64_t out_result;     /* wapi_result_t* — per-role outcome, optional */
    uint8_t  target_uid[16]; /* all-zero = runtime picks */
} wapi_role_request_t; /* 56 bytes */
```

**Prefs are opaque bytes at the wapi.h level.** Each kind's module header owns the typed prefs layout (`wapi_audio_spec_t`, `wapi_camera_desc_t`, `wapi_hid_prefs_t`, `wapi_sensor_prefs_t`, `wapi_midi_prefs_t`). `wapi.h` never references module-specific types; modules never reference each other. Cross-kind correlation is done purely via UID comparison.

**Per-role outcome**: each request writes its own `wapi_result_t` to `out_result` (if non-null). The top-level `io->submit` return reports only dispatch status, not grant outcomes. A batched prompt may grant some roles and deny others.

#### 9.10.4 Flags

| Flag                         | Meaning                                                                |
|------------------------------|------------------------------------------------------------------------|
| `WAPI_ROLE_OPTIONAL`         | UX hint: runtime may de-emphasize or default-deny this row             |
| `WAPI_ROLE_FOLLOW_DEFAULT`   | Runtime reroutes on OS default-device change (fires `ROLE_REROUTED`)   |
| `WAPI_ROLE_PIN_SPECIFIC`     | Handle revoked if target device disappears (fires `ROLE_REVOKED`)      |
| `WAPI_ROLE_ALL`              | Bind every granted endpoint of this kind (post-grant enumeration)      |
| `WAPI_ROLE_WAIT_FOR_DEVICE`  | Park until a matching device appears instead of failing `NOT_FOUND`    |

#### 9.10.5 Ambient-Grant Policy

Runtime policy decides which role kinds auto-grant vs prompt. Default policy under a desktop / gaming runtime:

| Kind                          | Default policy                                           |
|-------------------------------|----------------------------------------------------------|
| `KEYBOARD`, `MOUSE`, `POINTER`| Ambient-grant (app can't function without them)          |
| `DISPLAY`                     | Ambient-grant (informational; read via `wapi_display.h`) |
| `AUDIO_PLAYBACK` (default)    | Ambient-grant for the system default only                |
| `AUDIO_PLAYBACK` (specific)   | Prompt                                                   |
| `GAMEPAD`                     | Ambient-grant under gaming runtime; prompt otherwise     |
| `AUDIO_RECORDING`, `CAMERA`   | Always prompt                                            |
| `MIDI_*`                      | Prompt (sysex gated separately via prefs flag)           |
| `HID`                         | Always prompt (filter-driven picker)                     |
| `SENSOR`                      | Prompt, optionally frequency-gated                       |
| `TOUCH`, `PEN`                | Ambient-grant where hardware exists                      |
| `HAPTIC`                      | Inherits from the parent device's grant                  |

Runtime is authoritative; policy is configurable per deployment. The app influences only prefs and flags.

#### 9.10.6 UID, Cross-Kind Correlation, and Reconnect

The 16-byte UID is the only cross-module identifier. The runtime derives it from `(vendor_id, product_id, serial)` when a serial is available, otherwise from `(vendor, product, bus_position)`. Same physical device → same UID across reconnects and across app runs.

- **Headset detection**: `audio_playback_endpoint.uid == audio_recording_endpoint.uid` → playback and mic are the same physical device.
- **Per-player binding**: after acquiring a gamepad handle and reading its UID via `endpoint_info`, request the matching haptic and audio playback endpoints with `target_uid` set.
- **Reconnect**: `PIN_SPECIFIC` handles revoke on physical removal (`ROLE_REVOKED`). The runtime remembers the previously-granted UID; when `DEVICE_ADDED` fires with the same UID, a follow-up `ROLE_REQUEST(kind, target_uid=...)` binds without re-prompting.

#### 9.10.7 Endpoint Metadata

Each module header declares a kind-specific `wapi_<kind>_endpoint_info_t` struct and a query function that takes a granted handle and fills the struct plus an optional display-name buffer. The name is privacy-gated — the runtime may return a generic label ("Microphone", "Speakers") until the app has earned the real label. Serials on HID endpoints have a separate query (`wapi_hid_serial`) gated the same way.

#### 9.10.8 Vendor-Specific Hardware Apps

Apps built for specific hardware (iCUE, G Hub, Playdate Mirror, Stream Deck console) use the role system plus module-manifest hints:

- Filter-based requests: `ROLE_REQUEST(HID, prefs=wapi_hid_prefs_t{vendor_id=..., product_id=...})` drives a filtered picker.
- One physical device, multiple endpoints: a vendor keyboard exposing standard-HID keyboard + vendor-usage HID shows up as one device with two endpoints sharing a UID. Granting the vendor endpoint does not intercept standard typing events.
- `WAPI_ROLE_WAIT_FOR_DEVICE`: park the request until the hardware appears — vendor apps commonly launch before the device is plugged in.
- Manifest-declared pairing is a **runtime policy hook**, not an ABI construct. A vendor app's module manifest may declare the vendor/product it owns; the runtime may auto-grant matching HID requests without a picker. Other runtimes may ignore the hint.
- Lower-level transport (USB bulk/control, raw serial) stays on `WAPI_IO_OP_USB_DEVICE_REQUEST` / `WAPI_IO_OP_SERIAL_PORT_REQUEST`. Role system covers HID; those cover transports.

#### 9.10.9 Non-Goal: No "Player" Abstraction

WAPI exposes input as a flat pool of role handles. It does not model "player," "input set," or "seat-as-player." Different games pair devices differently (kb+mouse as a unit, gamepad alone, gamepad+headset via UID, shared keyboard demuxed by keymap). Any `wapi_player_t` abstraction would either restrict those cases or be a pass-through over handles. Consumers build player slots on top of handles; `target_uid` is the primitive that lets them bind related devices into a slot.

Same handle may be routed to multiple app-side consumers (classic shared-keyboard co-op: arrow keys → P1, WASD → P2 on the same keyboard handle). WAPI delivers per-handle event streams; app-side demultiplexing is out of scope.

---

## 10. Module Linking

Runtime module linking is a capability (`wapi.module`). Query availability via `wapi_cap_supported(io, WAPI_STR("wapi.module"))` before using any `wapi_module_*` imports. Not all hosts support runtime linking — minimal or embedded runtimes may omit it.

### 10.1 Hybrid Memory Model

Each module has two memories:

- **Memory 0 (private):** The module's own linear memory. Stack, globals, internal state. Fully isolated — no other module can access it. Allocated via the module's `wapi_allocator_t` (from `wapi_allocator_get()`).
- **Memory 1 (shared):** A single shared memory owned by the application. All loaded child modules share the same memory 1 instance. The application always has full access to all of shared memory — it owns it. Child modules can only access regions they allocated themselves or were explicitly lent via `wapi_module_lend`.

**Build-time linking** (one binary): Libraries are compiled into a single `.wasm` module. They share memory 0 naturally. Pass pointers, call functions directly. Pass `wapi_allocator_t` and `wapi_io_t` vtables as explicit function parameters (Zig-style). No WAPI mechanism needed.

**Runtime linking** (separate modules): Each module has its own memory 0. Data exchange uses shared memory (memory 1) with the borrow system for zero-copy access, or explicit copies via `wapi_module_copy_in` for simple cases. Two access paths for shared memory:
- **Multi-memory:** Import memory 1, load/store directly (zero copy).
- **Host-call:** `wapi_module_shared_read` / `wapi_module_shared_write` (one copy, portable C).

The dependency injection pattern is the same in both modes. A library accepts `wapi_allocator_t` for memory and `wapi_io_t` for I/O. Every module also gets its own module-owned vtables from `wapi_io_get()` / `wapi_allocator_get()` — these are host-controlled, parent-proof, and provide sandbox-only I/O for child modules. For capabilities beyond the sandbox, callers pass their own vtables explicitly.

### 10.2 Build-Time Linking

Build-time linked libraries are simply part of the same Wasm binary. They share linear memory and can call each other's functions directly. The vtable model (`wapi_allocator_t`, `wapi_io_t`, `wapi_panic_handler_t`) is universal — the same types used for module-owned vtables from the host are also passed explicitly between functions for dependency injection, following Zig's allocator pattern:

```c
// A library function that accepts an explicit allocator
image_t* decode_png(const uint8_t* data, size_t len, const wapi_allocator_t* alloc);

// A library function that accepts an explicit I/O interface
void http_fetch(const char* url, const wapi_io_t* io, callback_t cb);

// A library that needs both
void download_and_decode(const char* url, const wapi_io_t* io, const wapi_allocator_t* alloc);
```

This is a convention, not an ABI requirement. Libraries choose their own parameter patterns. The key principle is: if a function does I/O, it takes a `wapi_io_t*`. If it allocates, it takes a `wapi_allocator_t*`. The caller always controls the callee's capabilities.

### 10.3 Runtime Module Identity

Runtime modules are identified by the SHA-256 hash of their Wasm binary. The hash IS the identity. Name and version are human-readable metadata, not the linking key.

```c
wapi_module_hash_t hash = { /* SHA-256 bytes */ };
wapi_handle_t module;
wapi_module_load(&hash, WAPI_STR("https://registry.example.com/image-decoder-1.2.0.wasm"), &module);
```

The URL is a fetch hint, not an identity. Two calls with different URLs but the same hash produce the same module.

### 10.4 Cross-Module Calling

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
const wapi_allocator_t* alloc = wapi_allocator_get();
void* pixels = wapi_mem_alloc(alloc, result_len, 16);
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

### 10.5 Shared Memory Ownership

The application owns shared memory (memory 1) and always has full access to every offset. Child modules have restricted access:

- A child can access regions it **allocated itself** (via `wapi_module_shared_alloc`).
- A child can access regions **explicitly lent** to it (via `wapi_module_lend`).
- All other shared memory is inaccessible to the child.

The application can read, write, free, and lend **any** region — regardless of who allocated it. A child can only free and lend its own allocations. Freeing a region with active borrows fails (`WAPI_ERR_BUSY`) — reclaim all borrows first.

### 10.6 Borrow System

The borrow system controls child access to shared memory regions the child did not allocate. The application does not need borrows — it always has full access.

The application can lend any region (including regions allocated by a child). A child can only lend regions it allocated itself.

Borrow rules follow reader-writer lock semantics per region:
- `WAPI_LEND_READ`: borrower gets read-only access; lender retains read. Multiple concurrent READ borrows to different modules are allowed.
- `WAPI_LEND_WRITE`: borrower gets exclusive read-write; lender loses access until reclaim. No concurrent borrows on the same region.

This enables concurrent module execution on different regions: module B reads region R1 while module C writes region R3. The developer controls granularity through allocation pattern — smaller allocations mean finer-grained concurrency.

**Child output pattern:** The child allocates in shared memory, writes its result, and returns the offset as a return value. The application reads it directly — no borrow or ownership transfer needed. The application can then lend this data to another module for pipeline processing (A→B→C) without any copies.

### 10.7 I/O for Runtime Modules

Every runtime module gets its own module-owned vtables from `wapi_io_get()` and `wapi_allocator_get()`. These are host-controlled and cannot be influenced by any parent module — critical for shared instances where multiple parents use the same child.

**Module-owned I/O is sandboxed.** A child module's `wapi_io_get()` returns a vtable that can only access the module's transient and persistent filesystems (see Section 7.8). It cannot access the real filesystem, network, or any permission-gated capability.

**Per-call capabilities are explicit.** For capabilities beyond the sandbox, the caller grants its own `wapi_io_t` (or a wrapped version) to the child. The conceptual pattern is the same in both build-time and runtime linking — dependency injection — but the mechanism differs because of address space isolation.

In **build-time linking**, all code shares a single linear memory and function table. Passing `const wapi_io_t*` works directly — the pointer is valid and the function pointer fields are callable via `call_indirect`:

```c
/* Build-time: direct vtable passing within the same module */
image_t* load_image(const wapi_io_t* io, const wapi_allocator_t* alloc,
                    const char* path, size_t path_len);
```

In **runtime linking**, vtable pointers cannot cross module boundaries. Each module has its own linear memory and function table — a function pointer valid in the caller's function table is meaningless in the child's. Instead, the caller creates a **host-mediated proxy** in the child's address space using `wapi_module_io_proxy`:

```c
/* Create a proxy wapi_io_t in the child's linear memory.
   The proxy's function pointers are host imports in the child's
   function table that forward all calls back to the caller's io. */
uint32_t child_io_ptr;
wapi_module_io_proxy(module, io, &child_io_ptr);

/* Pass the child-local pointer as a normal argument */
wapi_val_t args[] = {
    { .kind = WAPI_VAL_I32, .of.i32 = child_io_ptr },
    { .kind = WAPI_VAL_I32, .of.i32 = path_off },
    { .kind = WAPI_VAL_I32, .of.i32 = path_len },
};
wapi_module_call(module, func, args, 3, &result, 1);

/* Destroy the proxy when the child no longer needs it */
wapi_module_io_proxy_destroy(module, child_io_ptr);
```

From the child's perspective, the proxy is an ordinary `const wapi_io_t*`. The child calls `io->submit(...)`, `io->poll(...)`, etc. via `call_indirect` — exactly as it would with a vtable from `wapi_io_get()` or one passed by a build-time linked caller. The host transparently marshals data between the child's and caller's linear memories when forwarding calls.

The same proxy mechanism applies to `wapi_allocator_t` via `wapi_module_allocator_proxy` / `wapi_module_allocator_proxy_destroy`.

The caller controls what the callee can do by choosing what to proxy — the real host I/O, a restricted wrapper, a logging layer, or a mock.

**Security properties:**

1. `wapi_io_get()` is host-controlled and parent-proof. A parent cannot change what a shared child's `wapi_io_get()` returns.
2. Module-owned I/O is sandboxed. No network, no permission-gated capabilities.
3. Per-call vtables are explicit. A module only gets capabilities beyond its sandbox when a caller passes them.
4. Typed API handles are validated per-call by the host.

### 10.8 Semver for ABI Contracts

Module versions follow **semantic versioning**:

- **Major version** change: breaking ABI change. The runtime must not substitute a different major version.
- **Minor version** change: backward-compatible additions. The runtime may substitute a higher minor version within the same major version.
- **Patch version** change: bug fix. The runtime may substitute a higher patch version.

### 10.9 Module Cache

The runtime maintains a cache of modules keyed by content hash. Analogous to the browser's HTTP cache or a Nix store. Modules can be pre-fetched (`wapi_module_prefetch`) for background download.

---

## 11. Browser Integration

### 11.1 Architecture

In the browser, a JavaScript shim implements the WAPI against Web APIs. The shim is a standard JavaScript module that:

1. Instantiates the `.wasm` binary via `WebAssembly.instantiate`.
2. Provides import implementations that delegate to Web APIs.
3. Manages the frame loop via `requestAnimationFrame`.

### 11.2 API Mapping

| WAPI Module         | Web API                                                      |
|-------------------|--------------------------------------------------------------|
| `wapi` (vtables)    | `wapi_io_t`/`wapi_allocator_t` backed by microtask queue + linear memory |
| `wapi_env`          | `location.search`, Web Crypto `getRandomValues`              |
| `wapi_clock`        | `performance.now()`, `Date.now()`                            |
| `wapi_filesystem`           | Origin Private File System (OPFS)                            |
| `wapi_network`          | `fetch`, `WebSocket`, `WebTransport`                         |
| `wapi_gpu`          | `navigator.gpu` (WebGPU)                                     |
| `wapi_surface`      | `<canvas>` element, fullscreen API                           |
| `wapi_input`        | DOM events (`keydown`, `mousemove`, `pointerdown`, `gamepadconnected`) |
| `wapi_audio`        | Web Audio API (`AudioContext`, `AudioWorklet`)               |
| `wapi_content`      | DOM overlay elements, Canvas 2D, `OffscreenCanvas`           |
| `wapi_transfer`     | Clipboard API + DataTransfer + Web Share API                 |
| `wapi_seat`         | (browser is single-seat: `count==1`, `name=="default"`)      |
| `wapi_font`         | CSS Font Loading API, `document.fonts`                       |
| `wapi_video`        | `<video>` element, Media Source Extensions                   |
| `wapi_geolocation`  | Geolocation API (`navigator.geolocation`)                    |
| `wapi_notifications`| Notifications API (`Notification`)                           |
| `wapi_sensors`      | Generic Sensor API (`Accelerometer`, `Gyroscope`)            |
| `wapi_camera`       | `getUserMedia`, MediaStream API                              |

### 11.3 Surface Model in Browser

Each browser tab renders a single fullscreen `<canvas>` element. The `wapi_surface_create` call creates or reconfigures this canvas. Requests for multiple surfaces are either mapped to a single canvas (with host-managed compositing) or rejected based on browser policy.

Surface descriptors like `width`, `height`, and `flags` are best-effort. The browser may ignore size requests and always use the viewport dimensions. The module should respond to `WAPI_SURFACE_EVENT_RESIZED` events rather than assuming the requested size was honored.

### 11.4 Coexistence with Traditional Web Content

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
| `wapi_hid_prefs_t`           |     8       |    2      |
| `wapi_sensor_prefs_t`        |     8       |    4      |
| `wapi_midi_prefs_t`          |     4       |    4      |
| `wapi_camera_desc_t`         |    16       |    4      |
| `wapi_audio_endpoint_info_t` |    32       |    4      |
| `wapi_hid_endpoint_info_t`   |    32       |    4      |
| `wapi_camera_endpoint_info_t`|    40       |    4      |
| `wapi_midi_endpoint_info_t`  |    24       |    4      |
| `wapi_sensor_endpoint_info_t`|    24       |    4      |
| `wapi_haptic_endpoint_info_t`|    24       |    4      |
| `wapi_role_request_t`        |    56       |    8      |
| `wapi_window_config_t`       |    40       |    8      |
| `wapi_network_listen_desc_t`     |    20       |    4      |
| `wapi_io_t`                  |    36       |    4      |
| `wapi_dirent_t`              |    24       |    8      |
| `wapi_network_connect_desc_t`    |    24       |    4      |
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
