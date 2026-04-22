/**
 * WAPI Desktop Runtime — Process Control (wapi_process.h)
 *
 * Spawns subprocesses via CreateProcessW and exposes stdin/stdout/
 * stderr pipes as separate WAPI handles. The parent side of each
 * pipe is non-inheritable; the child side is inheritable and passed
 * via STARTUPINFOW.hStd*. Pipes and the process handle are closed on
 * wapi_process_destroy (process keeps running detached).
 */

#include "wapi_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/* wapi_process_io_t */
#define WAPI_PROC_IO_INHERITED 0
#define WAPI_PROC_IO_PIPE      1
#define WAPI_PROC_IO_NULL      2

/* Build a wide-char command line from argv. argv[0] is the program.
 * Each arg is quoted + backslash-escaped per CommandLineToArgvW
 * round-trip semantics. Caller frees the returned buffer. Returns
 * NULL on allocation failure. */
static WCHAR* build_cmdline(const wapi_stringview_t* argv, uint32_t argc) {
    if (argc == 0) return NULL;
    size_t cap = 0;
    for (uint32_t i = 0; i < argc; i++) cap += (size_t)argv[i].length * 2 + 4;
    cap += 1;
    WCHAR* buf = (WCHAR*)malloc(cap * sizeof(WCHAR));
    if (!buf) return NULL;
    size_t pos = 0;
    for (uint32_t i = 0; i < argc; i++) {
        if (i > 0) buf[pos++] = L' ';
        buf[pos++] = L'"';
        const char* s = (const char*)wapi_wasm_ptr((uint32_t)argv[i].data, (uint32_t)argv[i].length);
        if (!s) { free(buf); return NULL; }
        int wlen = MultiByteToWideChar(CP_UTF8, 0, s, (int)argv[i].length, NULL, 0);
        if (wlen < 0) wlen = 0;
        int n = MultiByteToWideChar(CP_UTF8, 0, s, (int)argv[i].length,
                                    buf + pos, (int)(cap - pos));
        if (n < 0) n = 0;
        /* Escape internal quotes + trailing backslashes. Simple pass:
         * re-scan what we just wrote, doubling backslashes before '"'. */
        for (int k = 0; k < n; k++) {
            if (buf[pos + k] == L'"') {
                /* Insert a backslash in front; shift right. */
                if (pos + n + 1 >= cap) break;
                memmove(&buf[pos + k + 1], &buf[pos + k],
                        (size_t)(n - k) * sizeof(WCHAR));
                buf[pos + k] = L'\\';
                n++;
                k++;
            }
        }
        pos += (size_t)n;
        buf[pos++] = L'"';
    }
    buf[pos] = 0;
    return buf;
}

/* Build the Unicode environment block from envp ("KEY=VALUE" entries)
 * terminated by a double-NUL. Returns NULL to signal "inherit the
 * parent's environment" (envp==NULL or envc==0). */
static WCHAR* build_envblock(const wapi_stringview_t* envp, uint32_t envc) {
    if (!envp || envc == 0) return NULL;
    size_t wcap = 1; /* trailing NUL */
    for (uint32_t i = 0; i < envc; i++) wcap += (size_t)envp[i].length + 1;
    WCHAR* buf = (WCHAR*)malloc(wcap * sizeof(WCHAR));
    if (!buf) return NULL;
    size_t pos = 0;
    for (uint32_t i = 0; i < envc; i++) {
        const char* s = (const char*)wapi_wasm_ptr((uint32_t)envp[i].data, (uint32_t)envp[i].length);
        if (!s) { free(buf); return NULL; }
        int n = MultiByteToWideChar(CP_UTF8, 0, s, (int)envp[i].length,
                                    buf + pos, (int)(wcap - pos - 2));
        if (n < 0) n = 0;
        pos += (size_t)n;
        buf[pos++] = 0; /* entry terminator */
    }
    buf[pos] = 0;        /* block terminator */
    return buf;
}

/* Allocate a pipe WAPI handle owning the parent-side HANDLE. */
static int32_t alloc_pipe_handle(HANDLE h, bool readable) {
    int32_t hid = wapi_handle_alloc(WAPI_HTYPE_PIPE);
    if (hid <= 0) { CloseHandle(h); return 0; }
    g_rt.handles[hid].data.pipe.h        = h;
    g_rt.handles[hid].data.pipe.readable = readable;
    return hid;
}

/* Create a pipe pair; write the parent-side to *parent_out and the
 * child-side (inheritable) to *child_out. `parent_reads` picks the
 * parent-side direction. Returns false on failure. */
static bool make_pipe(HANDLE* parent_out, HANDLE* child_out, bool parent_reads) {
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE read_end = NULL, write_end = NULL;
    if (!CreatePipe(&read_end, &write_end, &sa, 0)) return false;
    HANDLE parent = parent_reads ? read_end : write_end;
    HANDLE child  = parent_reads ? write_end : read_end;
    /* Strip inheritance from the parent side. */
    HANDLE np = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), parent,
                         GetCurrentProcess(), &np,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
        CloseHandle(read_end); CloseHandle(write_end);
        return false;
    }
    CloseHandle(parent);
    *parent_out = np;
    *child_out  = child;
    return true;
}

/* Read a wapi_process_desc_t (56B) out of guest memory into host form.
 * Returns false on bad pointers. Caller owns the pointer arrays. */
typedef struct desc_read_t {
    wapi_stringview_t* argv;
    uint32_t           argc;
    wapi_stringview_t* envp;
    uint32_t           envc;
    wapi_stringview_t  cwd;
    uint32_t           stdin_mode;
    uint32_t           stdout_mode;
    uint32_t           stderr_mode;
} desc_read_t;

static bool read_desc(uint32_t desc_ptr, desc_read_t* out) {
    /* wapi_process_desc_t (56B):
     *   +0  i32 argv_ptr         (wasm32 pointer to wapi_stringview_t[])
     *   +4  u32 argc
     *   +8  i32 envp_ptr
     *   +12 u32 envc
     *   +16 stringview cwd (16B)
     *   +32 u32 stdin_mode
     *   +36 u32 stdout_mode
     *   +40 u32 stderr_mode
     *   +44 u32 _pad
     *   +48 u64 flags
     */
    uint8_t* src = (uint8_t*)wapi_wasm_ptr(desc_ptr, 56);
    if (!src) return false;
    uint32_t argv_gp, argc, envp_gp, envc;
    memcpy(&argv_gp, src +  0, 4);
    memcpy(&argc,    src +  4, 4);
    memcpy(&envp_gp, src +  8, 4);
    memcpy(&envc,    src + 12, 4);
    uint64_t cwd_data, cwd_len;
    memcpy(&cwd_data, src + 16, 8);
    memcpy(&cwd_len,  src + 24, 8);
    memcpy(&out->stdin_mode,  src + 32, 4);
    memcpy(&out->stdout_mode, src + 36, 4);
    memcpy(&out->stderr_mode, src + 40, 4);

    out->argc = argc;
    out->envc = envc;
    out->cwd.data   = cwd_data;
    out->cwd.length = cwd_len;

    if (argc == 0) return false;
    /* Each stringview is 16B: u64 data + u64 length. */
    if ((uint64_t)argc * 16ull > 0xFFFFFFFFull) return false;
    out->argv = (wapi_stringview_t*)wapi_wasm_ptr(argv_gp, argc * 16u);
    if (!out->argv) return false;

    if (envc > 0) {
        out->envp = (wapi_stringview_t*)wapi_wasm_ptr(envp_gp, envc * 16u);
        if (!out->envp) return false;
    } else {
        out->envp = NULL;
    }
    return true;
}

/* ============================================================
 * wapi_process imports
 * ============================================================ */

static wasm_trap_t* h_process_create(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t desc_ptr = WAPI_ARG_U32(0);
    uint32_t out_ptr  = WAPI_ARG_U32(1);

    desc_read_t d;
    if (!read_desc(desc_ptr, &d)) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    WCHAR* cmdline = build_cmdline(d.argv, d.argc);
    if (!cmdline) { WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }
    WCHAR* envblk = build_envblock(d.envp, d.envc);

    WCHAR* wcwd = NULL;
    if (d.cwd.length > 0) {
        const char* s = (const char*)wapi_wasm_ptr((uint32_t)d.cwd.data, (uint32_t)d.cwd.length);
        if (s) {
            int n = MultiByteToWideChar(CP_UTF8, 0, s, (int)d.cwd.length, NULL, 0);
            if (n > 0) {
                wcwd = (WCHAR*)malloc((size_t)(n + 1) * sizeof(WCHAR));
                if (wcwd) {
                    MultiByteToWideChar(CP_UTF8, 0, s, (int)d.cwd.length, wcwd, n);
                    wcwd[n] = 0;
                }
            }
        }
    }

    /* Pipe set-up: for each PIPE-mode stream, create one pair; for
     * INHERITED/NULL we pass the parent's handle or NUL:. The child-
     * side handles are closed in the parent after CreateProcess
     * succeeds; the parent-side ones live in WAPI_HTYPE_PIPE
     * handles. */
    HANDLE child_stdin = INVALID_HANDLE_VALUE;
    HANDLE child_stdout = INVALID_HANDLE_VALUE;
    HANDLE child_stderr = INVALID_HANDLE_VALUE;
    HANDLE parent_stdin = NULL;   /* writable */
    HANDLE parent_stdout = NULL;  /* readable */
    HANDLE parent_stderr = NULL;  /* readable */
    bool need_inherit = false;

    /* NUL device shared by NULL-mode stream setup. */
    HANDLE nul_rd = INVALID_HANDLE_VALUE;
    HANDLE nul_wr = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };

    if (d.stdin_mode == WAPI_PROC_IO_PIPE) {
        if (!make_pipe(&parent_stdin, &child_stdin, /*parent_reads=*/false)) goto fail;
        need_inherit = true;
    } else if (d.stdin_mode == WAPI_PROC_IO_NULL) {
        nul_rd = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             &sa, OPEN_EXISTING, 0, NULL);
        if (nul_rd == INVALID_HANDLE_VALUE) goto fail;
        child_stdin = nul_rd;
        need_inherit = true;
    } else {
        child_stdin = GetStdHandle(STD_INPUT_HANDLE);
    }

    if (d.stdout_mode == WAPI_PROC_IO_PIPE) {
        if (!make_pipe(&parent_stdout, &child_stdout, /*parent_reads=*/true)) goto fail;
        need_inherit = true;
    } else if (d.stdout_mode == WAPI_PROC_IO_NULL) {
        nul_wr = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             &sa, OPEN_EXISTING, 0, NULL);
        if (nul_wr == INVALID_HANDLE_VALUE) goto fail;
        child_stdout = nul_wr;
        need_inherit = true;
    } else {
        child_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    if (d.stderr_mode == WAPI_PROC_IO_PIPE) {
        if (!make_pipe(&parent_stderr, &child_stderr, /*parent_reads=*/true)) goto fail;
        need_inherit = true;
    } else if (d.stderr_mode == WAPI_PROC_IO_NULL) {
        /* Reuse nul_wr if we already opened one for stdout. */
        HANDLE h = (nul_wr != INVALID_HANDLE_VALUE) ? nul_wr
                 : CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               &sa, OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) goto fail;
        child_stderr = h;
        need_inherit = true;
    } else {
        child_stderr = GetStdHandle(STD_ERROR_HANDLE);
    }

    STARTUPINFOW si = {0};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdInput  = child_stdin;
    si.hStdOutput = child_stdout;
    si.hStdError  = child_stderr;

    PROCESS_INFORMATION pi = {0};
    DWORD flags = CREATE_UNICODE_ENVIRONMENT;
    BOOL ok = CreateProcessW(NULL, cmdline, NULL, NULL,
                             need_inherit ? TRUE : FALSE,
                             flags, envblk, wcwd, &si, &pi);

    /* Close the child-side pipe ends — the child has inherited them;
     * keeping them open in the parent prevents EOF detection. */
    if (d.stdin_mode  == WAPI_PROC_IO_PIPE) CloseHandle(child_stdin);
    if (d.stdout_mode == WAPI_PROC_IO_PIPE) CloseHandle(child_stdout);
    if (d.stderr_mode == WAPI_PROC_IO_PIPE) CloseHandle(child_stderr);

    free(cmdline);
    free(envblk);
    free(wcwd);

    if (!ok) {
        if (parent_stdin)  CloseHandle(parent_stdin);
        if (parent_stdout) CloseHandle(parent_stdout);
        if (parent_stderr) CloseHandle(parent_stderr);
        if (nul_rd != INVALID_HANDLE_VALUE) CloseHandle(nul_rd);
        /* nul_wr may equal child_stderr; already closed if stdout used it. */
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    int32_t proc_h = wapi_handle_alloc(WAPI_HTYPE_PROCESS);
    if (proc_h <= 0) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (parent_stdin)  CloseHandle(parent_stdin);
        if (parent_stdout) CloseHandle(parent_stdout);
        if (parent_stderr) CloseHandle(parent_stderr);
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }
    wapi_process_t* p = &g_rt.handles[proc_h].data.process;
    p->h_process = pi.hProcess;
    p->pid       = (uint32_t)pi.dwProcessId;
    p->stdin_pipe  = parent_stdin  ? alloc_pipe_handle(parent_stdin,  false) : 0;
    p->stdout_pipe = parent_stdout ? alloc_pipe_handle(parent_stdout, true)  : 0;
    p->stderr_pipe = parent_stderr ? alloc_pipe_handle(parent_stderr, true)  : 0;
    CloseHandle(pi.hThread);

    wapi_wasm_write_i32(out_ptr, proc_h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;

fail:
    free(cmdline);
    free(envblk);
    free(wcwd);
    if (parent_stdin)  CloseHandle(parent_stdin);
    if (parent_stdout) CloseHandle(parent_stdout);
    if (parent_stderr) CloseHandle(parent_stderr);
    if (nul_rd != INVALID_HANDLE_VALUE) CloseHandle(nul_rd);
    if (nul_wr != INVALID_HANDLE_VALUE) CloseHandle(nul_wr);
    if (d.stdin_mode  == WAPI_PROC_IO_PIPE && child_stdin  != INVALID_HANDLE_VALUE) CloseHandle(child_stdin);
    if (d.stdout_mode == WAPI_PROC_IO_PIPE && child_stdout != INVALID_HANDLE_VALUE) CloseHandle(child_stdout);
    if (d.stderr_mode == WAPI_PROC_IO_PIPE && child_stderr != INVALID_HANDLE_VALUE) CloseHandle(child_stderr);
    WAPI_RET_I32(WAPI_ERR_IO);
    return NULL;
}

static wapi_process_t* process_from_handle(int32_t h) {
    if (!wapi_handle_valid(h, WAPI_HTYPE_PROCESS)) return NULL;
    return &g_rt.handles[h].data.process;
}

#define GET_PIPE_FIELD(FIELD) \
    wapi_process_t* p = process_from_handle(h); \
    if (!p) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; } \
    int32_t ph = p->FIELD; \
    if (ph <= 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; } \
    wapi_wasm_write_i32(out_ptr, ph); \
    WAPI_RET_I32(WAPI_OK); \
    return NULL;

static wasm_trap_t* h_process_get_stdin(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{ (void)env; (void)caller; (void)nargs; (void)nresults;
  int32_t h = WAPI_ARG_I32(0); uint32_t out_ptr = WAPI_ARG_U32(1); GET_PIPE_FIELD(stdin_pipe) }

static wasm_trap_t* h_process_get_stdout(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{ (void)env; (void)caller; (void)nargs; (void)nresults;
  int32_t h = WAPI_ARG_I32(0); uint32_t out_ptr = WAPI_ARG_U32(1); GET_PIPE_FIELD(stdout_pipe) }

static wasm_trap_t* h_process_get_stderr(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{ (void)env; (void)caller; (void)nargs; (void)nresults;
  int32_t h = WAPI_ARG_I32(0); uint32_t out_ptr = WAPI_ARG_U32(1); GET_PIPE_FIELD(stderr_pipe) }

static wasm_trap_t* h_pipe_write(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h       = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint64_t len     = WAPI_ARG_U64(2);
    uint32_t out_ptr = WAPI_ARG_U32(3);
    if (!wapi_handle_valid(h, WAPI_HTYPE_PIPE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_pipe_t* pp = &g_rt.handles[h].data.pipe;
    if (pp->readable) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    const void* src = len ? wapi_wasm_ptr(buf_ptr, (uint32_t)len) : NULL;
    if (len > 0 && !src) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    DWORD wrote = 0;
    BOOL ok = WriteFile(pp->h, src, (DWORD)len, &wrote, NULL);
    if (out_ptr) wapi_wasm_write_u64(out_ptr, (uint64_t)wrote);
    if (!ok) { WAPI_RET_I32(WAPI_ERR_PIPE); return NULL; }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_pipe_read(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h       = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint64_t len     = WAPI_ARG_U64(2);
    uint32_t out_ptr = WAPI_ARG_U32(3);
    if (!wapi_handle_valid(h, WAPI_HTYPE_PIPE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_pipe_t* pp = &g_rt.handles[h].data.pipe;
    if (!pp->readable) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    void* dst = len ? wapi_wasm_ptr(buf_ptr, (uint32_t)len) : NULL;
    if (len > 0 && !dst) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    DWORD got = 0;
    BOOL ok = ReadFile(pp->h, dst, (DWORD)len, &got, NULL);
    if (!ok && GetLastError() == ERROR_BROKEN_PIPE) {
        /* Child closed its write end — report 0 = EOF. */
        got = 0; ok = TRUE;
    }
    if (out_ptr) wapi_wasm_write_u64(out_ptr, (uint64_t)got);
    if (!ok) { WAPI_RET_I32(WAPI_ERR_PIPE); return NULL; }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_pipe_close(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(h, WAPI_HTYPE_PIPE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_pipe_t* pp = &g_rt.handles[h].data.pipe;
    if (pp->h) CloseHandle(pp->h);
    /* Clear stale references in any owning process record. */
    for (int i = 1; i < WAPI_MAX_HANDLES; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_PROCESS) {
            wapi_process_t* p = &g_rt.handles[i].data.process;
            if (p->stdin_pipe  == h) p->stdin_pipe  = 0;
            if (p->stdout_pipe == h) p->stdout_pipe = 0;
            if (p->stderr_pipe == h) p->stderr_pipe = 0;
        }
    }
    wapi_handle_free(h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_process_wait(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h         = WAPI_ARG_I32(0);
    int32_t  block     = WAPI_ARG_I32(1);
    uint32_t code_ptr  = WAPI_ARG_U32(2);
    wapi_process_t* p = process_from_handle(h);
    if (!p) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    DWORD ms = block ? INFINITE : 0;
    DWORD rc = WaitForSingleObject((HANDLE)p->h_process, ms);
    if (rc == WAIT_TIMEOUT) { WAPI_RET_I32(WAPI_ERR_AGAIN); return NULL; }
    if (rc != WAIT_OBJECT_0) { WAPI_RET_I32(WAPI_ERR_IO); return NULL; }
    DWORD exit_code = 0;
    GetExitCodeProcess((HANDLE)p->h_process, &exit_code);
    if (code_ptr) wapi_wasm_write_i32(code_ptr, (int32_t)exit_code);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_process_kill(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    wapi_process_t* p = process_from_handle(h);
    if (!p) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    WAPI_RET_I32(TerminateProcess((HANDLE)p->h_process, 1) ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

static wasm_trap_t* h_process_destroy(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    wapi_process_t* p = process_from_handle(h);
    if (!p) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    if (p->stdin_pipe > 0  && wapi_handle_valid(p->stdin_pipe,  WAPI_HTYPE_PIPE)) {
        if (g_rt.handles[p->stdin_pipe].data.pipe.h)  CloseHandle(g_rt.handles[p->stdin_pipe].data.pipe.h);
        wapi_handle_free(p->stdin_pipe);
    }
    if (p->stdout_pipe > 0 && wapi_handle_valid(p->stdout_pipe, WAPI_HTYPE_PIPE)) {
        if (g_rt.handles[p->stdout_pipe].data.pipe.h) CloseHandle(g_rt.handles[p->stdout_pipe].data.pipe.h);
        wapi_handle_free(p->stdout_pipe);
    }
    if (p->stderr_pipe > 0 && wapi_handle_valid(p->stderr_pipe, WAPI_HTYPE_PIPE)) {
        if (g_rt.handles[p->stderr_pipe].data.pipe.h) CloseHandle(g_rt.handles[p->stderr_pipe].data.pipe.h);
        wapi_handle_free(p->stderr_pipe);
    }
    if (p->h_process) CloseHandle((HANDLE)p->h_process);
    wapi_handle_free(h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

void wapi_host_register_process(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_process", "create",     h_process_create);
    WAPI_DEFINE_2_1(linker, "wapi_process", "get_stdin",  h_process_get_stdin);
    WAPI_DEFINE_2_1(linker, "wapi_process", "get_stdout", h_process_get_stdout);
    WAPI_DEFINE_2_1(linker, "wapi_process", "get_stderr", h_process_get_stderr);
    wapi_linker_define(linker, "wapi_process", "pipe_write", h_pipe_write,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, "wapi_process", "pipe_read", h_pipe_read,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, "wapi_process", "pipe_close", h_pipe_close);
    WAPI_DEFINE_3_1(linker, "wapi_process", "wait",       h_process_wait);
    WAPI_DEFINE_1_1(linker, "wapi_process", "kill",       h_process_kill);
    WAPI_DEFINE_1_1(linker, "wapi_process", "destroy",    h_process_destroy);
}
