#ifndef NM_UTIL_H
#define NM_UTIL_H
#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>

#include <NickelHook.h>

// strtrim trims ASCII whitespace in-place (i.e. don't give it a string literal)
// from the left/right of the string.
__attribute__((unused)) static inline char *strtrim(char *s) {
    if (!s) return NULL;
    char *a = s, *b = s + strlen(s);
    for (; a < b && isspace((unsigned char)(*a)); a++);
    for (; b > a && isspace((unsigned char)(*(b-1))); b--);
    *b = '\0';
    return a;
}

// NM_LOG writes a log message.
#define NM_LOG(fmt, ...) nh_log(fmt " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)

// Error handling (thread-safe):

// nm_err returns the current error message and clears the error state. If there
// isn't any error set, NULL is returned. The returned string is only valid on
// the current thread until nm_err_set is called.
const char *nm_err();

// nm_err_peek is like nm_err, but doesn't clear the error state.
const char *nm_err_peek();

// nm_err_set sets the current error message to the specified format string. If
// fmt is NULL, the error is cleared. It is safe to use the return value of
// nm_err as an argument. If fmt was not NULL, true is returned.
bool nm_err_set(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// NM_ERR_SET set is like nm_err_set, but also includes information about the
// current file/line. To set it to NULL, use nm_err_set directly.
#define NM_ERR_SET(fmt, ...) \
    nm_err_set((fmt " (%s:%d)"), ##__VA_ARGS__, __FILE__, __LINE__);

// NM_ERR_RET is like NM_ERR_SET, but also returns the specified value.
#define NM_ERR_RET(ret, fmt, ...) do { \
    NM_ERR_SET(fmt, ##__VA_ARGS__);    \
    return (ret);                      \
} while (0)

// NM_CHECK checks a condition and calls nm_err_set then returns the specified
// value if the condition is false. Otherwise, nothing happens.
#define NM_CHECK(ret, cond, fmt, ...) do {                             \
    if (!(cond)) {                                                     \
        nm_err_set((fmt " (check failed: %s)"), ##__VA_ARGS__, #cond); \
        return (ret);                                                  \
    }                                                                  \
} while (0)

#ifdef __cplusplus
}
#endif
#endif
