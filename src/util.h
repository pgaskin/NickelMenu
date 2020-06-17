#ifndef NM_UTIL_H
#define NM_UTIL_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // asprintf
#endif

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

// Fallback version tag
#ifndef NM_VERSION
#define NM_VERSION "dev"
#endif

#ifndef NM_LOG_NAME
#define NM_LOG_NAME "NickelMenu"
#endif

// Symbol visibility
#define NM_PUBLIC  __attribute__((visibility("default")))
#define NM_PRIVATE __attribute__((visibility("hidden")))

// strtrim trims ASCII whitespace in-place (i.e. don't give it a string literal)
// from the left/right of the string.
inline char *strtrim(char *s) {
    if (!s) return NULL;
    char *a = s, *b = s + strlen(s);
    for (; a < b && isspace((unsigned char)(*a)); a++);
    for (; b > a && isspace((unsigned char)(*(b-1))); b--);
    *b = '\0';
    return a;
}

// A bunch of useful macros to simplify error handling and logging.

// NM_LOG writes a log message.
#define NM_LOG(fmt, ...) syslog(LOG_DEBUG, "(" NM_LOG_NAME ") " fmt " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)

// NM_RETURN returns ret, and if ret is NM_ERR_RET and err_out is not NULL, it
// writes the formatted error message to *err_out as a malloc'd string. The
// arguments may or may not be evaluated more than once.
#define NM_RETURN(ret, fmt, ...) _NM_RETURN(0, ret, fmt, ##__VA_ARGS__)
#define _NM_RETURN(noerr, ret, fmt, ...) ({                                                                     \
    __typeof__(ret) _ret = (ret);                                                                               \
    if (err_out) {                                                                                              \
        if (!noerr && _ret == NM_ERR_RET) asprintf(err_out, fmt " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__); \
        else *err_out = NULL;                                                                                   \
    }                                                                                                           \
    return _ret;                                                                                                \
})

// NM_ASSERT is like assert, but it writes the formatted error message to
// err_out as a malloc'd string, and returns NM_ERR_RET. Cond will always be
// evaluated exactly once. The other arguments may or may not be evaluated one
// or more times.
#define NM_ASSERT(cond, fmt, ...) ({                                                             \
    if (!(cond)) _NM_RETURN(0, NM_ERR_RET, fmt " (assertion failed: %s)", ##__VA_ARGS__, #cond); \
})

// NM_RETURN_ERR is the same as NM_RETURN(NM_ERR_RET, fmt, ...).
#define NM_RETURN_ERR(fmt, ...) _NM_RETURN(0, NM_ERR_RET, fmt, ##__VA_ARGS__)

// NM_RETURN_OK is the same as NM_RETURN(ret, "").
#define NM_RETURN_OK(ret) _NM_RETURN(1, ret, "")

#ifdef __cplusplus
}
#endif
#endif
