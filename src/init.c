#define _GNU_SOURCE // program_invocation_short_name
#include <link.h>
#include <dlfcn.h>
#include <errno.h> // program_invocation_short_name
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "action.h"
#include "config.h"
#include "failsafe.h"
#include "menu.h"
#include "util.h"

__attribute__((constructor)) void nm_init() {
    // for if it's been loaded with LD_PRELOAD rather than as a Qt plugin
    if (strcmp(program_invocation_short_name, "nickel"))
        if (!(getenv("LIBNM_FORCE") && !strcmp(getenv("LIBNM_FORCE"), "true")))
            return;

    char *err;

    NM_LOG("init: creating failsafe");
    nm_failsafe_t *fs;
    if (!(fs = nm_failsafe_create(&err)) && err) {
        NM_LOG("error: could not create failsafe: %s, stopping", err);
        free(err);
        goto stop;
    }

    NM_LOG("init: checking for uninstall flag");
    if (!access("/mnt/onboard/.adds/nm/uninstall", F_OK)) {
        NM_LOG("init: flag found, uninstalling");
        nm_failsafe_uninstall(fs);
        unlink("/mnt/onboard/.adds/nm/uninstall");
        goto stop;
    }

    NM_LOG("init: parsing config");
    size_t items_n;
    nm_menu_item_t **items;
    nm_config_t *cfg;
    if (!(cfg = nm_config_parse(&err)) && err) {
        NM_LOG("error: could not parse config: %s, creating error item in main menu instead", err);

        items_n  = 1;
        items    = calloc(items_n, sizeof(nm_menu_item_t*));
        items[0] = calloc(1, sizeof(nm_menu_item_t));
        items[0]->loc = NM_MENU_LOCATION_MAIN_MENU;
        items[0]->lbl = strdup("Config Error");
        items[0]->action = calloc(1, sizeof(nm_menu_action_t));
        items[0]->action->arg = strdup(err);
        items[0]->action->act = NM_ACTION(dbg_msg);

        free(err);
    } else if (!(items = nm_config_get_menu(cfg, &items_n))) {
        NM_LOG("error: could not allocate memory, stopping");
        goto stop_fs;
    }

    NM_LOG("init: opening libnickel");
    void *libnickel = dlopen("libnickel.so.1.0.0", RTLD_LAZY|RTLD_NODELETE);
    if (!libnickel) {
        NM_LOG("error: could not dlopen libnickel, stopping");
        goto stop_fs;
    }

    NM_LOG("init: hooking libnickel");
    if (nm_menu_hook(libnickel, items, items_n, &err) && err) {
        NM_LOG("error: could not hook libnickel: %s, stopping", err);
        free(err);
        goto stop_fs;
    }

stop_fs:
    NM_LOG("init: destroying failsafe");
    nm_failsafe_destroy(fs, 20);

stop:
    NM_LOG("init: done");
    return;
}
