/**
 * WAPI SDL Runtime - Seat
 *
 * Single-seat host. count == 1, the only seat is WAPI_SEAT_DEFAULT (0).
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

static int64_t host_seat_count(wasm_exec_env_t env) {
    (void)env;
    return 1;
}

static int32_t host_seat_at(wasm_exec_env_t env, uint64_t index) {
    (void)env; (void)index;
    return 0;  /* WAPI_SEAT_DEFAULT */
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

static int32_t host_seat_name(wasm_exec_env_t env, int32_t seat,
                              uint32_t buf_ptr, uint64_t buf_len,
                              uint32_t out_len_ptr) {
    (void)env;
    if (seat != 0) return WAPI_ERR_INVAL;

    char user[256];
    get_user_name(user, sizeof(user));
    size_t name_len = strlen(user);

    uint64_t copy = name_len;
    if (copy > buf_len) copy = buf_len;
    if (copy > 0) {
        void* buf = wapi_wasm_ptr(buf_ptr, (uint32_t)copy);
        if (!buf) return WAPI_ERR_INVAL;
        memcpy(buf, user, (size_t)copy);
    }
    wapi_wasm_write_u64(out_len_ptr, name_len);
    return WAPI_OK;
}

static int32_t host_input_device_seat(wasm_exec_env_t env, int32_t device) {
    (void)env; (void)device;
    return 0;  /* WAPI_SEAT_DEFAULT */
}

/* WAMR sig codes: i=i32, I=i64 */
static NativeSymbol g_seat_symbols[] = {
    { "count", (void*)host_seat_count, "()I",     NULL },
    { "at",    (void*)host_seat_at,    "(I)i",    NULL },
    { "name",  (void*)host_seat_name,  "(iiIi)i", NULL },
};

static NativeSymbol g_input_seat_symbols[] = {
    { "device_seat", (void*)host_input_device_seat, "(i)i", NULL },
};

wapi_cap_registration_t wapi_host_seat_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_seat",
        .symbols = g_seat_symbols,
        .count = (uint32_t)(sizeof(g_seat_symbols) / sizeof(g_seat_symbols[0])),
    };
    return r;
}

wapi_cap_registration_t wapi_host_input_seat_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_input",
        .symbols = g_input_seat_symbols,
        .count = (uint32_t)(sizeof(g_input_seat_symbols) / sizeof(g_input_seat_symbols[0])),
    };
    return r;
}
