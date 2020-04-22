#define _GNU_SOURCE // asprintf, Dl_info, dladdr
#include <link.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "failsafe.h"
#include "util.h"

struct nmi_failsafe_t {
    char *orig;
    char *tmp;
    int  delay;
};

nmi_failsafe_t *nmi_failsafe_create(char **err_out) {
    #define NMI_ERR_RET NULL

    NMI_LOG("failsafe: allocating memory");
    nmi_failsafe_t *fs;
    NMI_ASSERT((fs = calloc(1, sizeof(nmi_failsafe_t))), "could not allocate memory");

    NMI_LOG("failsafe: finding filenames");
    Dl_info info;
    NMI_ASSERT(dladdr(nmi_failsafe_create, &info), "could not find own path");
    NMI_ASSERT(info.dli_fname, "dladdr did not return a filename");
    NMI_ASSERT((fs->orig = realpath(info.dli_fname, NULL)), "could not resolve %s", info.dli_fname);
    NMI_ASSERT(asprintf(&fs->tmp, "%s.failsafe", fs->orig) != -1, "could not generate temp filename");

    NMI_LOG("failsafe: renaming %s to %s", fs->orig, fs->tmp);
    NMI_ASSERT(!rename(fs->orig, fs->tmp), "could not rename lib");

    NMI_LOG("failsafe: ensuring own lib remains in memory even if it is dlclosed after being loaded with a dlopen");
    NMI_ASSERT(!dlopen(fs->orig, RTLD_LAZY|RTLD_NODELETE), "could not dlopen self");

    NMI_RETURN_OK(fs);
    #undef NMI_ERR_RET
}

static void *_nmi_failsafe_destroy(void* _fs) {
    nmi_failsafe_t *fs = (nmi_failsafe_t*)(_fs);

    NMI_LOG("failsafe: restoring after %d seconds", fs->delay);
    sleep(fs->delay);

    NMI_LOG("failsafe: renaming %s to %s", fs->tmp, fs->orig);
    if (!rename(fs->tmp, fs->orig))
        NMI_LOG("error: could not rename lib");

    NMI_LOG("failsafe: freeing memory");
    free(fs->orig);
    free(fs->tmp);
    free(fs);

    return NULL;
}

void nmi_failsafe_destroy(nmi_failsafe_t *fs, int delay) {
    fs->delay = delay;

    NMI_LOG("failsafe: starting restore thread");
    pthread_t t;
    pthread_create(&t, NULL, _nmi_failsafe_destroy, fs);
}
