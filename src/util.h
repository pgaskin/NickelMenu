#ifndef NM_UTIL_H
#define NM_UTIL_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // asprintf
#endif

#include <stdio.h>
#include <syslog.h>

// A bunch of useful macros to simplify error handling and logging.

// NM_LOG writes a log message.
#define NM_LOG(fmt, ...) syslog(LOG_DEBUG, "(NickelMenu) " fmt " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)

// NM_RETURN returns ret, and if ret is NM_ERR_RET and err_out is not NULL, it
// writes the formatted error message to *err_out as a malloc'd string. The
// arguments may or may not be evaluated more than once.
#define NM_RETURN(ret, fmt, ...) ({                                                                    \
    __typeof__(ret) _ret = (ret);                                                                      \
    if (err_out) {                                                                                     \
        if (_ret == NM_ERR_RET) asprintf(err_out, fmt " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__);  \
        else *err_out = NULL;                                                                          \
    }                                                                                                  \
    return _ret;                                                                                       \
})

// NM_ASSERT is like assert, but it writes the formatted error message to
// err_out as a malloc'd string, and returns NM_ERR_RET. Cond will always be
// evaluated exactly once. The other arguments may or may not be evaluated one
// or more times.
#define NM_ASSERT(cond, fmt, ...) ({                                                         \
    if (!(cond)) NM_RETURN(NM_ERR_RET, fmt " (assertion failed: %s)", ##__VA_ARGS__, #cond); \
})

// NM_RETURN_ERR is the same as NM_RETURN(NM_ERR_RET, fmt, ...).
#define NM_RETURN_ERR(fmt, ...) NM_RETURN(NM_ERR_RET, fmt, ##__VA_ARGS__)

// NM_RETURN_OK is the same as NM_RETURN(ret, "").
#define NM_RETURN_OK(ret) NM_RETURN(ret, "")

#ifdef __cplusplus
}
#endif
#endif
