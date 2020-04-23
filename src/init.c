#define _GNU_SOURCE // program_invocation_short_name
#include <link.h>
#include <dlfcn.h>
#include <errno.h> // program_invocation_short_name
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "action_c.h"
#include "action_cc.h"
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

    NMI_LOG("init: checking for uninstall flag");
    if (!access("/mnt/onboard/.adds/nmi/uninstall", F_OK)) {
        NMI_LOG("init: flag found, uninstalling");
        nmi_failsafe_uninstall(fs);
        unlink("/mnt/onboard/.adds/nmi/uninstall");
        goto stop;
    }

    NMI_LOG("init: parsing config");
    size_t items_n;
    nmi_menu_item_t **items;
    nmi_config_t *cfg;
    if (!(cfg = nmi_config_parse(&err)) && err) {
        NMI_LOG("error: could not parse config: %s, creating error item in main menu instead", err);

        items_n  = 1;
        items    = calloc(items_n, sizeof(nmi_menu_item_t*));
        items[0] = calloc(1, sizeof(nmi_menu_item_t));

        items[0]->loc = NMI_MENU_LOCATION_MAIN_MENU;
        items[0]->lbl = strdup("Config Error");
        items[0]->arg = strdup(err);
        items[0]->act = nmi_action_dbgerror;

        free(err);
    } else if (!(items = nmi_config_get_menu(cfg, &items_n))) {
        NMI_LOG("error: could not allocate memory, stopping");
        goto stop_fs;
    }

    NMI_LOG("init: opening libnickel");
    void *libnickel = dlopen("libnickel.so.1.0.0", RTLD_LAZY|RTLD_NODELETE);
    if (!libnickel) {
        NMI_LOG("error: could not dlopen libnickel, stopping");
        goto stop_fs;
    }

    NMI_LOG("init: hooking libnickel");
    if (nmi_menu_hook(libnickel, items, items_n, &err) && err) {
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
