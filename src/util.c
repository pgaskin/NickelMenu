#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static __thread bool nm_err_state                       = false;
static __thread char nm_err_buf[2048]                   = {0};
static __thread char nm_err_buf_tmp[sizeof(nm_err_buf)] = {0}; // in case the format string overlaps

const char *nm_err() {
    if (nm_err_state) {
        nm_err_state = false;
        return nm_err_buf;
    }
    return NULL;
}

const char *nm_err_peek() {
    if (nm_err_state)
        return nm_err_buf;
    return NULL;
}

bool nm_err_set(const char *fmt, ...) {
    va_list a;
    if ((nm_err_state = !!fmt)) {
        va_start(a, fmt);
        int r = vsnprintf(nm_err_buf_tmp, sizeof(nm_err_buf_tmp), fmt, a);
        if (r < 0)
            r = snprintf(nm_err_buf_tmp, sizeof(nm_err_buf_tmp), "error applying format to error string '%s'", fmt);
        if (r >= (int)(sizeof(nm_err_buf_tmp))) {
            nm_err_buf_tmp[sizeof(nm_err_buf_tmp) - 2] = '.';
            nm_err_buf_tmp[sizeof(nm_err_buf_tmp) - 3] = '.';
            nm_err_buf_tmp[sizeof(nm_err_buf_tmp) - 4] = '.';
        }
        memcpy(nm_err_buf, nm_err_buf_tmp, sizeof(nm_err_buf));
        va_end(a);
    }
    return nm_err_state;
}
