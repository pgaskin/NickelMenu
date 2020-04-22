#define _GNU_SOURCE // program_invocation_short_name
#include <link.h>
#include <dlfcn.h>
#include <errno.h> // program_invocation_short_name
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "failsafe.h"
#include "menu.h"
#include "util.h"

__attribute__((constructor)) void nmi_init() {
    // for if it's been loaded with LD_PRELOAD rather than as a Qt plugin
    if (strcmp(program_invocation_short_name, "nickel"))
        if (!(getenv("LIBNMI_FORCE") && !strcmp(getenv("LIBNMI_FORCE"), "true")))
            return;

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
