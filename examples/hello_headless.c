/**
 * WAPI - Hello Headless Example
 *
 * A minimal headless application that demonstrates the Headless preset:
 * filesystem access, clocks, environment, and async I/O.
 *
 * Compile with:
 *   clang --target=wasm32 -O2 -nostdlib \
 *     -I../include -o hello_headless.wasm hello_headless.c
 */

#include <wapi/wapi.h>

/* Module entry point */
WAPI_EXPORT(wapi_main)
wapi_result_t wapi_main(const wapi_context_t* ctx) {
    (void)ctx;
    /* --------------------------------------------------------
     * 1. Check capabilities
     * -------------------------------------------------------- */
    if (!wapi_preset_supported(WAPI_PRESET_HEADLESS)) {
        /* Host doesn't provide the headless preset */
        return WAPI_ERR_NOTCAPABLE;
    }

    /* --------------------------------------------------------
     * 2. Print command-line arguments to stdout
     * -------------------------------------------------------- */
    int32_t argc = wapi_env_args_count();
    char arg_buf[256];
    wapi_size_t arg_len;

    const char header[] = "Arguments:\n";
    wapi_size_t written;
    wapi_fs_write(WAPI_STDOUT, header, sizeof(header) - 1, &written);

    for (int32_t i = 0; i < argc; i++) {
        if (WAPI_SUCCEEDED(wapi_env_args_get(i, arg_buf, sizeof(arg_buf), &arg_len))) {
            wapi_fs_write(WAPI_STDOUT, "  ", 2, &written);
            wapi_fs_write(WAPI_STDOUT, arg_buf, arg_len, &written);
            wapi_fs_write(WAPI_STDOUT, "\n", 1, &written);
        }
    }

    /* --------------------------------------------------------
     * 3. Read the current time
     * -------------------------------------------------------- */
    wapi_timestamp_t now;
    if (WAPI_SUCCEEDED(wapi_clock_time_get(WAPI_CLOCK_REALTIME, &now))) {
        /* Convert nanoseconds to seconds for display */
        uint64_t seconds = now / 1000000000ULL;
        char time_msg[] = "Current time: ";
        wapi_fs_write(WAPI_STDOUT, time_msg, sizeof(time_msg) - 1, &written);

        /* Simple integer-to-string (no libc available) */
        char num_buf[20];
        int pos = 19;
        num_buf[pos] = '\0';
        if (seconds == 0) {
            num_buf[--pos] = '0';
        } else {
            while (seconds > 0 && pos > 0) {
                num_buf[--pos] = '0' + (seconds % 10);
                seconds /= 10;
            }
        }
        wapi_fs_write(WAPI_STDOUT, &num_buf[pos], 19 - pos, &written);
        wapi_fs_write(WAPI_STDOUT, "s since epoch\n", 14, &written);
    }

    /* --------------------------------------------------------
     * 4. Generate random bytes
     * -------------------------------------------------------- */
    uint8_t random_bytes[16];
    if (WAPI_SUCCEEDED(wapi_env_random_get(random_bytes, sizeof(random_bytes)))) {
        const char hex[] = "0123456789abcdef";
        char hex_str[33];
        for (int i = 0; i < 16; i++) {
            hex_str[i * 2]     = hex[random_bytes[i] >> 4];
            hex_str[i * 2 + 1] = hex[random_bytes[i] & 0x0F];
        }
        hex_str[32] = '\0';

        const char rand_msg[] = "Random bytes: ";
        wapi_fs_write(WAPI_STDOUT, rand_msg, sizeof(rand_msg) - 1, &written);
        wapi_fs_write(WAPI_STDOUT, hex_str, 32, &written);
        wapi_fs_write(WAPI_STDOUT, "\n", 1, &written);
    }

    /* --------------------------------------------------------
     * 5. List pre-opened directories
     * -------------------------------------------------------- */
    int32_t preopen_count = wapi_fs_preopen_count();
    const char dir_msg[] = "Pre-opened directories:\n";
    wapi_fs_write(WAPI_STDOUT, dir_msg, sizeof(dir_msg) - 1, &written);

    char path_buf[256];
    wapi_size_t path_len;
    for (int32_t i = 0; i < preopen_count; i++) {
        if (WAPI_SUCCEEDED(wapi_fs_preopen_path(i, path_buf, sizeof(path_buf), &path_len))) {
            wapi_fs_write(WAPI_STDOUT, "  ", 2, &written);
            wapi_fs_write(WAPI_STDOUT, path_buf, path_len, &written);
            wapi_fs_write(WAPI_STDOUT, "\n", 1, &written);
        }
    }

    /* --------------------------------------------------------
     * 6. Write a file using async I/O
     * -------------------------------------------------------- */
    if (preopen_count > 0) {
        wapi_handle_t dir = wapi_fs_preopen_handle(0);
        wapi_handle_t fd;
        const char filename[] = "hello.txt";
        const char content[] = "Hello from the WAPI!\n";

        wapi_result_t res = wapi_fs_open(dir, filename, sizeof(filename) - 1,
                                      WAPI_FS_OFLAG_CREATE, 0, &fd);
        if (WAPI_SUCCEEDED(res)) {
            wapi_fs_write(fd, content, sizeof(content) - 1, &written);
            wapi_fs_close(fd);

            const char file_msg[] = "Wrote hello.txt\n";
            wapi_fs_write(WAPI_STDOUT, file_msg, sizeof(file_msg) - 1, &written);
        }
    }

    const char done[] = "Done.\n";
    wapi_fs_write(WAPI_STDOUT, done, sizeof(done) - 1, &written);

    return WAPI_OK;
}
