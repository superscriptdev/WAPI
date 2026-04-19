/**
 * WAPI Desktop Runtime - Seat
 *
 * Single-seat host: count == 1, the only seat is WAPI_SEAT_DEFAULT (0),
 * its name is the OS user. Also registers wapi_input.device_seat which
 * always returns WAPI_SEAT_DEFAULT for any valid device on a single-seat
 * machine.
 */

#include "wapi_host.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <lmcons.h>
#else
#  include <unistd.h>
#  include <pwd.h>
#  include <sys/types.h>
#endif

/* Wasm signature: () -> i64 */
static wasm_trap_t* host_seat_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I64(1);
    return NULL;
}

/* Wasm signature: (i64) -> i32 */
static wasm_trap_t* host_seat_at(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint64_t index = WAPI_ARG_U64(0);
    if (index != 0) {
        WAPI_RET_I32(0);  /* WAPI_SEAT_DEFAULT for index 0, invalid otherwise */
        return NULL;
    }
    WAPI_RET_I32(0);  /* WAPI_SEAT_DEFAULT */
    return NULL;
}

static void get_user_name(char* out, size_t out_size) {
#ifdef _WIN32
    DWORD n = (DWORD)out_size;
    if (!GetUserNameA(out, &n)) {
        if (out_size > 0) out[0] = '\0';
    }
#else
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_name) {
        strncpy(out, pw->pw_name, out_size - 1);
        out[out_size - 1] = '\0';
    } else if (out_size > 0) {
        out[0] = '\0';
    }
#endif
}

/* Wasm signature: (i32, i32, i64, i32) -> i32
 * (seat, buf, buf_len, out_len) */
static wasm_trap_t* host_seat_name(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  seat       = WAPI_ARG_I32(0);
    uint32_t buf_ptr    = WAPI_ARG_U32(1);
    uint64_t buf_len    = WAPI_ARG_U64(2);
    uint32_t out_len_ptr = WAPI_ARG_U32(3);

    if (seat != 0) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    char user[256];
    get_user_name(user, sizeof(user));
    size_t name_len = strlen(user);

    uint64_t copy_len = name_len;
    if (copy_len > buf_len) copy_len = buf_len;

    if (copy_len > 0) {
        void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)copy_len);
        if (!buf) {
            WAPI_RET_I32(WAPI_ERR_INVAL);
            return NULL;
        }
        memcpy(buf, user, copy_len);
    }
    wapi_wasm_write_u64(out_len_ptr, name_len);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* user_id: (i32 seat, i32 buf, i64 buf_len, i32 out_len) -> i32
 * On a single-seat host, seat 0 maps to the current user's id; any other
 * seat returns WAPI_ERR_NOENT. Multi-seat Linux attribution is a follow-up. */
static wasm_trap_t* host_seat_user_id(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  seat        = WAPI_ARG_I32(0);
    uint32_t buf_ptr     = WAPI_ARG_U32(1);
    uint64_t buf_len     = WAPI_ARG_U64(2);
    uint32_t out_len_ptr = WAPI_ARG_U32(3);

    if (seat != 0) {
        WAPI_RET_I32(WAPI_ERR_NOENT);
        return NULL;
    }

    char id[256];
    int n = wapi_host_user_current_id(id, sizeof(id));
    if (n < 0) {
        WAPI_RET_I32(WAPI_ERR_NOENT);
        return NULL;
    }

    wapi_wasm_write_u64(out_len_ptr, (uint64_t)n);
    uint64_t copy_len = (uint64_t)n;
    if (copy_len > buf_len) copy_len = buf_len;
    if (copy_len > 0) {
        void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)copy_len);
        if (!buf) {
            WAPI_RET_I32(WAPI_ERR_INVAL);
            return NULL;
        }
        memcpy(buf, id, copy_len);
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* wapi_input.device_seat: (i32 device) -> i32
 * Always returns WAPI_SEAT_DEFAULT on this single-seat host. */
static wasm_trap_t* host_input_device_seat(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0);  /* WAPI_SEAT_DEFAULT */
    return NULL;
}

void wapi_host_register_seat(wasmtime_linker_t* linker) {
    /* count: () -> i64 */
    wapi_linker_define(linker, "wapi_seat", "count", host_seat_count,
        0, NULL,
        1, (wasm_valkind_t[]){WASM_I64});

    /* at: (i64) -> i32 */
    wapi_linker_define(linker, "wapi_seat", "at", host_seat_at,
        1, (wasm_valkind_t[]){WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});

    /* name: (i32, i32, i64, i32) -> i32 */
    wapi_linker_define(linker, "wapi_seat", "name", host_seat_name,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    /* user_id: (i32, i32, i64, i32) -> i32 */
    wapi_linker_define(linker, "wapi_seat", "user_id", host_seat_user_id,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    /* wapi_input.device_seat: (i32) -> i32 */
    WAPI_DEFINE_1_1(linker, "wapi_input", "device_seat", host_input_device_seat);
}
