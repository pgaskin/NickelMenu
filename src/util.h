#ifndef NMI_UTIL_H
#define NMI_UTIL_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // asprintf
#endif

#include <stdio.h>
#include <syslog.h>

// A bunch of useful macros to simplify error handling and logging.

// NMI_LOG writes a log message.
#define NMI_LOG(fmt, ...) syslog(LOG_DEBUG, "(nickel-menu-inject) " fmt " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)

// NMI_RETURN returns ret, and if ret is NMI_ERR_RET and err_out is not NULL, it
// writes the formatted error message to *err_out as a malloc'd string. The
// arguments may or may not be evaluated more than once.
#define NMI_RETURN(ret, fmt, ...) do {                                                                 \
    typeof(ret) _ret = (ret);                                                                          \
    if (err_out) {                                                                                     \
        if (_ret == NMI_ERR_RET) asprintf(err_out, fmt " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__); \
        else *err_out = NULL;                                                                          \
    }                                                                                                  \
    return _ret;                                                                                       \
} while (0)

// NMI_ASSERT is like assert, but it writes the formatted error message to
// err_out as a malloc'd string, and returns NMI_ERR_RET. Cond will always be
// evaluated exactly once. The other arguments may or may not be evaluated one
// or more times.
#define NMI_ASSERT(cond, fmt, ...) do {                                                        \
    if (!(cond)) NMI_RETURN(NMI_ERR_RET, fmt " (assertion failed: %s)", ##__VA_ARGS__, #cond); \
} while (0)

// NMI_RETURN_ERR is the same as NMI_RETURN(NMI_ERR_RET, fmt, ...).
#define NMI_RETURN_ERR(fmt, ...) NMI_RETURN(NMI_ERR_RET, fmt, ##__VA_ARGS__)

// NMI_RETURN_OK is the same as NMI_RETURN(ret, "").
#define NMI_RETURN_OK(ret) NMI_RETURN(ret, "")

#ifdef __cplusplus
}
#endif
#ifdef __cplusplus

#include <dlfcn.h>

// NMI_SYM loads a symbol from the global scope.
#define NMI_SYM(var, sym) reinterpret_cast<void*&>(var) = dlsym(RTLD_DEFAULT, sym)

#endif
#endif
