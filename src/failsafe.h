#ifndef NMI_FAILSAFE_H
#define NMI_FAILSAFE_H
#ifdef __cplusplus
extern "C" {
#endif

// nmi_failsafe_t is a failsafe mechanism for injected shared libraries. It
// works by moving it to a temporary file (so it won't get loaded the next time)
// and dlopening itself (to prevent it from being unloaded if it is dlclose'd by
// whatever dlopen'd it). When it is disarmed, the library is moved back to its
// original location.
typedef struct nmi_failsafe_t nmi_failsafe_t;

// nmi_failsafe_create allocates and arms a failsafe mechanism for the currently
// dlopen'd or LD_PRELOAD'd library. 
nmi_failsafe_t *nmi_failsafe_create(char **err_out);

// nmi_failsafe_destroy starts a pthread which disarms and frees the failsafe
// after a delay. The nmi_failsafe_t must not be used afterwards.
void nmi_failsafe_destroy(nmi_failsafe_t *fs, int delay);

// nmi_failsafe_uninstall uninstalls the lib. The nmi_failsafe_t must not be
// used afterwards.
void nmi_failsafe_uninstall(nmi_failsafe_t *fs);

#ifdef __cplusplus
}
#endif
#endif
