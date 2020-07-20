#define _GNU_SOURCE // vasprintf
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "action.h"

nm_action_result_t *nm_action_result_silent() {
    nm_action_result_t *res = calloc(1, sizeof(nm_action_result_t));
    res->type = NM_ACTION_RESULT_TYPE_SILENT;
    return res;
}

#define _nm_action_result_fmt(_fn, _typ)                                 \
    nm_action_result_t *nm_action_result_##_fn(const char *fmt, ...) {   \
        nm_action_result_t *res = calloc(1, sizeof(nm_action_result_t)); \
        res->type = _typ;                                                \
        va_list v;                                                       \
        va_start(v, fmt);                                                \
        if (vasprintf(&res->msg, fmt, v) == -1)                          \
            res->msg = strdup("error");                                  \
        va_end(v);                                                       \
        return res;                                                      \
    }

_nm_action_result_fmt(msg,    NM_ACTION_RESULT_TYPE_MSG);
_nm_action_result_fmt(toast,  NM_ACTION_RESULT_TYPE_TOAST);

void nm_action_result_free(nm_action_result_t *res) {
    if (!res)
        return;
    if (res->msg)
        free(res->msg);
    free(res);
}
