#define _GNU_SOURCE
#include <link.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>

#include "config.h"
#include "failsafe.h"
#include "menu.h"
#include "util.h"

__attribute__((constructor)) void nmi_init() {
    char *err;

    NMI_LOG("init: creating failsafe");
    nmi_failsafe_t *fs;
    if (!(fs = nmi_failsafe_create(&err)) && err) {
        NMI_LOG("error: could not create failsafe: %s, stopping", err);
        free(err);
        goto stop;
    }

    // TODO: handle uninstall

    NMI_LOG("init: parsing config");
    size_t entries_n;
    nmi_menu_entry_t *entries;
    if (!(entries = nmi_config_parse(&entries_n, &err)) && err) {
        NMI_LOG("error: could not parse config: %s, stopping", err);
        free(err);
        goto stop_fs;
    }

    NMI_LOG("init: opening libnickel");
    void *libnickel = dlopen("libnickel.so.1.0.0", RTLD_LAZY|RTLD_NODELETE);
    if (!libnickel) {
        NMI_LOG("error: could not dlopen libnickel, stopping");
        goto stop_fs;
    }

    NMI_LOG("init: hooking libnickel");
    if (nmi_menu_hook(libnickel, entries, entries_n, &err) && err) {
        NMI_LOG("error: could not hook libnickel: %s, stopping", err);
        free(err);
        goto stop_fs;
    }

stop_fs:
    NMI_LOG("init: destroying failsafe");
    nmi_failsafe_destroy(fs, 20);

stop:
    NMI_LOG("init: done");
    return;
}
