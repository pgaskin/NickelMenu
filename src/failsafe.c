#define _GNU_SOURCE // asprintf, Dl_info, dladdr
#include <dlfcn.h>
#include <link.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "failsafe.h"
#include "util.h"

struct nm_failsafe_t {
    char *orig;
    char *tmp;
    int  delay;
};

nm_failsafe_t *nm_failsafe_create() {
    NM_LOG("failsafe: allocating memory");
    nm_failsafe_t *fs;
    NM_CHECK(NULL, (fs = calloc(1, sizeof(nm_failsafe_t))), "could not allocate memory");

    NM_LOG("failsafe: finding filenames");
    Dl_info info;
    NM_CHECK(NULL, dladdr(nm_failsafe_create, &info), "could not find own path");
    NM_CHECK(NULL, info.dli_fname, "dladdr did not return a filename");
    char *d = strrchr(info.dli_fname, '.');
    NM_CHECK(NULL, !(d && !strcmp(d, ".failsafe")), "lib was loaded from the failsafe for some reason");
    NM_CHECK(NULL, (fs->orig = realpath(info.dli_fname, NULL)), "could not resolve %s", info.dli_fname);
    NM_CHECK(NULL, asprintf(&fs->tmp, "%s.failsafe", fs->orig) != -1, "could not generate temp filename");

    NM_LOG("failsafe: ensuring own lib remains in memory even if it is dlclosed after being loaded with a dlopen");
    NM_CHECK(NULL, dlopen(fs->orig, RTLD_LAZY|RTLD_NODELETE), "could not dlopen self");

    NM_LOG("failsafe: renaming %s to %s", fs->orig, fs->tmp);
    NM_CHECK(NULL, !rename(fs->orig, fs->tmp), "could not rename lib");

    return fs;
}

static void *_nm_failsafe_destroy(void* _fs) {
    nm_failsafe_t *fs = (nm_failsafe_t*)(_fs);

    NM_LOG("failsafe: restoring after %d seconds", fs->delay);
    sleep(fs->delay);

    NM_LOG("failsafe: renaming %s to %s", fs->tmp, fs->orig);
    if (rename(fs->tmp, fs->orig))
        NM_LOG("error: could not rename lib");

    NM_LOG("failsafe: freeing memory");
    free(fs->orig);
    free(fs->tmp);
    free(fs);

    return NULL;
}

void nm_failsafe_destroy(nm_failsafe_t *fs, int delay) {
    fs->delay = delay;

    NM_LOG("failsafe: scheduling restore");

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t t;
    pthread_create(&t, &attr, _nm_failsafe_destroy, fs);
    const char name[16] = "nm_failsafe";
    pthread_setname_np(t, name);
    pthread_attr_destroy(&attr);
}

void nm_failsafe_uninstall(nm_failsafe_t *fs) {
    NM_LOG("failsafe: deleting %s", fs->tmp);
    unlink(fs->tmp);
}
