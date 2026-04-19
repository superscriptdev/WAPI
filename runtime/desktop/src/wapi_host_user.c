/**
 * WAPI Desktop Runtime - User Identity
 *
 * Current-user login / display / email / upn / id + provider + avatar.
 * Avatar currently returns WAPI_ERR_NOENT on all platforms; decode path
 * is a follow-up per the initial landing scope.
 */

#include "wapi_host.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <lmcons.h>
#  include <sddl.h>
#else
#  include <unistd.h>
#  include <pwd.h>
#  include <sys/types.h>
#endif

#include <string.h>
#include <stdio.h>

/* wapi_user_field_t values (see include/wapi/wapi_user.h) */
#define WAPI_USER_FIELD_LOGIN_V    0
#define WAPI_USER_FIELD_DISPLAY_V  1
#define WAPI_USER_FIELD_EMAIL_V    2
#define WAPI_USER_FIELD_UPN_V      3
#define WAPI_USER_FIELD_ID_V       4

/* wapi_user_provider_t values */
#define WAPI_USER_PROVIDER_LOCAL_V 1

static int get_login(char* out, size_t out_size) {
#ifdef _WIN32
    DWORD n = (DWORD)out_size;
    if (!GetUserNameA(out, &n)) return -1;
    return (int)(n > 0 ? n - 1 : 0);
#else
    struct passwd* pw = getpwuid(getuid());
    if (!pw || !pw->pw_name) return -1;
    size_t n = strlen(pw->pw_name);
    if (n >= out_size) n = out_size - 1;
    memcpy(out, pw->pw_name, n);
    out[n] = '\0';
    return (int)n;
#endif
}

static int get_display(char* out, size_t out_size) {
#ifdef _WIN32
    /* GetUserNameExA(NameDisplay) would be ideal but pulls in secur32.lib;
     * fall back to login for the initial landing. */
    return get_login(out, out_size);
#else
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_gecos && pw->pw_gecos[0]) {
        const char* g = pw->pw_gecos;
        const char* comma = strchr(g, ',');
        size_t n = comma ? (size_t)(comma - g) : strlen(g);
        if (n >= out_size) n = out_size - 1;
        if (n > 0) {
            memcpy(out, g, n);
            out[n] = '\0';
            return (int)n;
        }
    }
    return get_login(out, out_size);
#endif
}

static int get_upn(char* out, size_t out_size) {
    (void)out; (void)out_size;
    /* Needs secur32.lib on Windows / Address Book on macOS / AccountsService
     * on Linux. Deferred to follow-up; return NOENT. */
    return -1;
}

static int get_user_id_string(char* out, size_t out_size) {
#ifdef _WIN32
    char login[UNLEN + 1];
    DWORD ln = sizeof(login);
    if (!GetUserNameA(login, &ln)) return -1;

    BYTE sid_buf[SECURITY_MAX_SID_SIZE];
    DWORD sid_len = sizeof(sid_buf);
    char dom[256];
    DWORD dom_len = sizeof(dom);
    SID_NAME_USE use;
    if (!LookupAccountNameA(NULL, login, (PSID)sid_buf, &sid_len,
                            dom, &dom_len, &use)) {
        return -1;
    }
    LPSTR sid_str = NULL;
    if (!ConvertSidToStringSidA((PSID)sid_buf, &sid_str)) return -1;
    size_t n = strlen(sid_str);
    if (n >= out_size) n = out_size - 1;
    memcpy(out, sid_str, n);
    out[n] = '\0';
    LocalFree(sid_str);
    return (int)n;
#else
    int n = snprintf(out, out_size, "%u", (unsigned)getuid());
    if (n < 0 || (size_t)n >= out_size) return -1;
    return n;
#endif
}

/* Exposed for wapi_host_seat.c so seat.user_id(WAPI_SEAT_DEFAULT)
 * returns the same bytes as user.get_field(ID). */
int wapi_host_user_current_id(char* out, size_t out_size) {
    return get_user_id_string(out, out_size);
}

/* provider: () -> i32 */
static wasm_trap_t* host_user_provider(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    /* AAD / domain / Apple ID detection is a follow-up. */
    WAPI_RET_I32(WAPI_USER_PROVIDER_LOCAL_V);
    return NULL;
}

/* get_field: (i32 field, i32 buf, i64 buf_len, i32 out_len_ptr) -> i32 */
static wasm_trap_t* host_user_get_field(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t field       = WAPI_ARG_U32(0);
    uint32_t buf_ptr     = WAPI_ARG_U32(1);
    uint64_t buf_len     = WAPI_ARG_U64(2);
    uint32_t out_len_ptr = WAPI_ARG_U32(3);

    char val[512];
    int n = -1;
    switch (field) {
        case WAPI_USER_FIELD_LOGIN_V:   n = get_login(val, sizeof(val));   break;
        case WAPI_USER_FIELD_DISPLAY_V: n = get_display(val, sizeof(val)); break;
        case WAPI_USER_FIELD_EMAIL_V:   n = -1;                            break;
        case WAPI_USER_FIELD_UPN_V:     n = get_upn(val, sizeof(val));     break;
        case WAPI_USER_FIELD_ID_V:      n = get_user_id_string(val, sizeof(val)); break;
        default:
            WAPI_RET_I32(WAPI_ERR_INVAL);
            return NULL;
    }
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
        memcpy(buf, val, copy_len);
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* avatar: (i32 max_edge, i32 out_w, i32 out_h, i32 buf, i64 buf_len) -> i32 */
static wasm_trap_t* host_user_avatar(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOENT);
    return NULL;
}

void wapi_host_register_user(wasmtime_linker_t* linker) {
    /* provider: () -> i32 */
    WAPI_DEFINE_0_1(linker, "wapi_user", "provider", host_user_provider);

    /* get_field: (i32, i32, i64, i32) -> i32 */
    wapi_linker_define(linker, "wapi_user", "get_field", host_user_get_field,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    /* avatar: (i32, i32, i32, i32, i64) -> i32 */
    wapi_linker_define(linker, "wapi_user", "avatar", host_user_avatar,
        5, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I32, WASM_I32, WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});
}
