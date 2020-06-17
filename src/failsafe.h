#ifndef NM_FAILSAFE_H
#define NM_FAILSAFE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "util.h"

// nm_failsafe_t is a failsafe mechanism for injected shared libraries. It
// works by moving it to a temporary file (so it won't get loaded the next time)
// and dlopening itself (to prevent it from being unloaded if it is dlclose'd by
// whatever dlopen'd it). When it is disarmed, the library is moved back to its
// original location.
typedef struct nm_failsafe_t nm_failsafe_t;

// nm_failsafe_create allocates and arms a failsafe mechanism for the currently
// dlopen'd or LD_PRELOAD'd library.
NM_PRIVATE nm_failsafe_t *nm_failsafe_create(char **err_out);

// nm_failsafe_destroy starts a pthread which disarms and frees the failsafe
// after a delay. The nm_failsafe_t must not be used afterwards.
NM_PRIVATE void nm_failsafe_destroy(nm_failsafe_t *fs, int delay);

// nm_failsafe_uninstall uninstalls the lib. The nm_failsafe_t must not be
// used afterwards.
NM_PRIVATE void nm_failsafe_uninstall(nm_failsafe_t *fs);

#ifdef __cplusplus
}
#endif
#endif
