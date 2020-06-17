#ifndef NM_DLHOOK_H
#define NM_DLHOOK_H
#ifdef __cplusplus
extern "C" {
#endif

#include "util.h"

// nm_dlhook takes a lib handle from dlopen and redirects the specified symbol
// to another, returning a pointer to the original one. Only calls from within
// that library itself are affected (because it replaces that library's GOT). If
// an error occurs, NULL is returned and if err is a valid pointer, it is set to
// a malloc'd string describing it. This function requires glibc and Linux. It
// should work on any architecture, and it should be resilient to most errors.
NM_PRIVATE void *nm_dlhook(void *handle, const char *symname, void *target, char **err_out);

#ifdef __cplusplus
}
#endif
#endif
