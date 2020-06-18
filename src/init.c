#define _GNU_SOURCE // program_invocation_short_name
#include <dlfcn.h>
#include <errno.h> // program_invocation_short_name
#include <link.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "action.h"
#include "config.h"
#include "failsafe.h"
#include "init.h"
#include "menu.h"
#include "util.h"

__attribute__((constructor)) void nm_init() {
    // for if it's been loaded with LD_PRELOAD rather than as a Qt plugin
    if (strcmp(program_invocation_short_name, "nickel"))
        if (!(getenv("LIBNM_FORCE") && !strcmp(getenv("LIBNM_FORCE"), "true")))
            return;

    char *err;

    NM_LOG("version: " NM_VERSION);
    #ifdef NM_UNINSTALL_CONFIGDIR
    NM_LOG("feature: NM_UNINSTALL_CONFIGDIR: true");
    #else
    NM_LOG("feature: NM_UNINSTALL_CONFIGDIR: false");
    #endif
    NM_LOG("config dir: %s", NM_CONFIG_DIR);

    NM_LOG("init: creating failsafe");

    nm_failsafe_t *fs;
    if (!(fs = nm_failsafe_create(&err)) && err) {
        NM_LOG("error: could not create failsafe: %s, stopping", err);
        free(err);
        goto stop;
    }

    NM_LOG("init: checking for uninstall flag");

    if (!access(NM_CONFIG_DIR "/uninstall", F_OK)) {
        NM_LOG("init: flag found, uninstalling");
        nm_failsafe_uninstall(fs);
        unlink(NM_CONFIG_DIR "/uninstall");
        goto stop;
    }

    #ifdef NM_UNINSTALL_CONFIGDIR
    NM_LOG("init: NM_UNINSTALL_CONFIGDIR: checking if config dir exists");

    if (access(NM_CONFIG_DIR, F_OK) && errno == ENOENT) {
        NM_LOG("init: config dir does not exist, uninstalling");
        nm_failsafe_uninstall(fs);
        goto stop;
    }
    #endif

    NM_LOG("init: updating config");

    bool upd = nm_global_config_update(&err);
    if (err) {
        NM_LOG("init: error parsing config, will show a menu item with the error: %s", err);
    }

    size_t ntmp = -1;
    if (!upd) {
        NM_LOG("init: no config file changes detected for initial config update (it should always return an error or update), stopping (this is a bug; err should have been returned instead)");
        return;
    } else if (!nm_global_config_items(&ntmp)) {
        NM_LOG("init: warning: no menu items returned by nm_global_config_items, ignoring for now (this is a bug; it should always have a menu item whether the default, an error, or the actual config)");
    } else if (ntmp == -1) {
        NM_LOG("init: warning: no size returned by nm_global_config_items, ignoring for now (this is a bug)");
    } else if (!ntmp) {
        NM_LOG("init: warning: size returned by nm_global_config_items is 0, ignoring for now (this is a bug; it should always have a menu item whether the default, an error, or the actual config)");
    }

    NM_LOG("init: opening libnickel");

    void *libnickel = dlopen("libnickel.so.1.0.0", RTLD_LAZY|RTLD_NODELETE);
    if (!libnickel) {
        NM_LOG("error: could not dlopen libnickel, stopping");
        goto stop_fs;
    }

    NM_LOG("init: hooking libnickel");

    if (nm_menu_hook(libnickel, &err) && err) {
        NM_LOG("error: could not hook libnickel: %s, stopping", err);
        free(err);
        goto stop_fs;
    }

stop_fs:
    NM_LOG("init: destroying failsafe");
    nm_failsafe_destroy(fs, 5);

stop:
    NM_LOG("init: done");
    return;
}

// note: not thread safe
static nm_config_file_t  *nm_global_menu_config_files = NULL; // updated in-place by nm_global_config_update
static      nm_config_t  *nm_global_menu_config       = NULL; // updated by nm_global_config_update, replaced by nm_global_config_replace, NULL on error
static   nm_menu_item_t **nm_global_menu_config_items = NULL; // updated by nm_global_config_replace to an error message or the items from nm_global_menu_config
static           size_t  nm_global_menu_config_n      = 0;    // ^

nm_menu_item_t **nm_global_config_items(size_t *n_out) {
    if (n_out)
        *n_out = nm_global_menu_config_n;
    return nm_global_menu_config_items;
}

static void nm_global_config_replace(nm_config_t *cfg, const char *err) {
    if (nm_global_menu_config_n)
        nm_global_menu_config_n = 0;

    if (nm_global_menu_config_items) {
        free(nm_global_menu_config_items);
        nm_global_menu_config_items = NULL;
    }

    if (nm_global_menu_config) {
        nm_config_free(nm_global_menu_config);
        nm_global_menu_config = NULL;
    }

    if (err && cfg)
        nm_config_free(cfg);

    // this isn't strictly necessary, but we should always try to reparse it
    // every time just in case the error was temporary
    if (err && nm_global_menu_config_files) {
        nm_config_files_free(nm_global_menu_config_files);
        nm_global_menu_config_files = NULL;
    }

    if (err) {
        nm_global_menu_config_n        = 1;
        nm_global_menu_config_items    = calloc(nm_global_menu_config_n, sizeof(nm_menu_item_t*));
        nm_global_menu_config_items[0] = calloc(1, sizeof(nm_menu_item_t));
        nm_global_menu_config_items[0]->loc = NM_MENU_LOCATION_MAIN_MENU;
        nm_global_menu_config_items[0]->lbl = strdup("Config Error");
        nm_global_menu_config_items[0]->action = calloc(1, sizeof(nm_menu_action_t));
        nm_global_menu_config_items[0]->action->arg = strdup(err);
        nm_global_menu_config_items[0]->action->act = NM_ACTION(dbg_msg);
        nm_global_menu_config_items[0]->action->on_failure = true;
        nm_global_menu_config_items[0]->action->on_success = true;
        return;
    }

    nm_global_menu_config_items = nm_config_get_menu(cfg, &nm_global_menu_config_n);
    if (!nm_global_menu_config_items) 
        NM_LOG("could not allocate memory");
}

bool nm_global_config_update(char **err_out) {
    #define NM_ERR_RET true
    char *err;

    NM_LOG("global: scanning for config files");
    bool updated = nm_config_files_update(&nm_global_menu_config_files, &err);
    if (err) {
        NM_LOG("... error: %s", err);
        NM_LOG("global: freeing old config and replacing with error item");
        nm_global_config_replace(NULL, err);
        NM_RETURN_ERR("scan for config files: %s", err);
    }

    NM_LOG("global:%s changes detected", updated ? "" : " no");
    if (!updated)
        NM_RETURN_OK(false);
    
    NM_LOG("global: parsing new config");
    nm_config_t *cfg = nm_config_parse(nm_global_menu_config_files, &err);
    if (err) {
        NM_LOG("... error: %s", err);
        NM_LOG("global: freeing old config and replacing with error item");
        nm_global_config_replace(NULL, err);
        NM_RETURN_ERR("parse config files: %s", err);
    }

    NM_LOG("global: running generators");
    nm_config_generate(cfg);

    NM_LOG("global: freeing old config and replacing with new one");
    nm_global_config_replace(cfg, NULL);

    NM_LOG("global: done swapping config");
    NM_RETURN_OK(true);
    #undef NM_ERR_RET
}
