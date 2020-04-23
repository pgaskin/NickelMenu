#define _GNU_SOURCE // asprintf
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "action_c.h"
#include "action_cc.h"
#include "config.h"
#include "menu.h"
#include "util.h"

#ifndef NMI_CONFIG_DIR
#define NMI_CONFIG_DIR "/mnt/onboard/.adds/nmi"
#endif

typedef enum {
    NMI_CONFIG_TYPE_MENU_ITEM = 1,
} nmi_config_type_t;

struct nmi_config_t {
    nmi_config_type_t type;
    union {
        nmi_menu_item_t *menu_item;
    } value;
    nmi_config_t *next;
};

// strtrim trims ASCII whitespace in-place (i.e. don't give it a string literal)
// from the left/right of the string.
static char *strtrim(char *s){
    if (!s) return NULL;
    char *a = s, *b = s + strlen(s);
    for (; a < b && isspace((unsigned char)(*a)); a++);
    for (; b > a && isspace((unsigned char)(*(b-1))); b--);
    *b = '\0';
    return a;
}

static void nmi_config_push_menu_item(nmi_config_t **cfg, nmi_menu_item_t *it) {
    nmi_config_t *tmp = calloc(1, sizeof(nmi_config_t));
    tmp->type = NMI_CONFIG_TYPE_MENU_ITEM;
    tmp->value.menu_item = it;
    tmp->next = *cfg;
    *cfg = tmp;
}

nmi_config_t *nmi_config_parse(char **err_out) {
    #define NMI_ERR_RET NULL
    NMI_LOG("config: reading config dir %s", NMI_CONFIG_DIR);

    // set up the linked list
    nmi_config_t *cfg = NULL;

    // open the config dir
    DIR *cfgdir;
    NMI_ASSERT((cfgdir = opendir(NMI_CONFIG_DIR)), "could not open config dir: %s", strerror(errno));

    // loop over the dirents of the config dir
    struct dirent *dirent; errno = 0;
    while ((dirent = readdir(cfgdir))) {
        char *fn;
        NMI_ASSERT(asprintf(&fn, "%s/%s", NMI_CONFIG_DIR, dirent->d_name) != -1, "could not build full path for config file");

        // skip it if it isn't a file
        bool reg = dirent->d_type == DT_REG;
        if (dirent->d_type == DT_UNKNOWN) {
            struct stat statbuf;
            NMI_ASSERT(!stat(fn, &statbuf), "could not stat %s", fn);
            reg = S_ISREG(statbuf.st_mode);
        }
        if (!reg) {
            NMI_LOG("config: skipping %s because not a regular file", fn);
            continue;
        }

        // open the config file
        NMI_LOG("config: reading config file %s", fn);
        FILE *cfgfile;
        NMI_ASSERT((cfgfile = fopen(fn, "r")), "could not open file: %s", strerror(errno));

        #define RETERR(fmt, ...) do {           \
            fclose(cfgfile);                    \
            free(line);                         \
            closedir(cfgdir);                   \
            NMI_RETURN_ERR(fmt, ##__VA_ARGS__); \
        } while (0)

        // parse each line
        char *line;
        int line_n;
        ssize_t line_sz;
        size_t line_bufsz = 0;
        while ((line_sz = getline(&line, &line_bufsz, cfgfile)) != -1) {
            line_n++;

            // empty line or comment
            char *cur = strtrim(line);
            if (!*cur || *cur == '#')
                continue;

            // field 1: type
            char *c_typ = strtrim(strsep(&cur, ":"));
            if (!strcmp(c_typ, "menu_item")) {
                // type: menu_item
                nmi_menu_item_t *it = calloc(1, sizeof(nmi_menu_item_t));

                // type: menu_item - field 2: location
                char *c_loc = strtrim(strsep(&cur, ":"));
                if (!c_loc) RETERR("file %s: line %d: field 2: expected location, got end of line", fn, line_n);
                else if (!strcmp(c_loc, "main"))   it->loc = NMI_MENU_LOCATION_MAIN_MENU;
                else if (!strcmp(c_loc, "reader")) it->loc = NMI_MENU_LOCATION_READER_MENU;
                else RETERR("file %s: line %d: field 2: unknown location '%s'", fn, line_n, c_loc);

                // type: menu_item - field 3: label
                char *c_lbl = strtrim(strsep(&cur, ":"));
                if (!c_lbl) RETERR("file %s: line %d: field 3: expected label, got end of line", fn, line_n);
                else it->lbl = strdup(c_lbl);

                // type: menu_item - field 4: action
                char *c_act = strtrim(strsep(&cur, ":"));
                if (!c_act) RETERR("file %s: line %d: field 4: expected action, got end of line", fn, line_n);
                else if (!strcmp(c_act, "dbg_syslog"))     it->act = nmi_action_dbgsyslog;
                else if (!strcmp(c_act, "dbg_error"))      it->act = nmi_action_dbgerror;
                else if (!strcmp(c_act, "kfmon"))          it->act = nmi_action_kfmon;
                else if (!strcmp(c_act, "nickel_setting")) it->act = nmi_action_nickelsetting;
                else if (!strcmp(c_act, "nickel_extras"))  it->act = nmi_action_nickelextras;
                else if (!strcmp(c_act, "nickel_misc"))    it->act = nmi_action_nickelmisc;
                else RETERR("file %s: line %d: field 4: unknown action '%s'", fn, line_n, c_act);

                // type: menu_item - field 5: argument
                char *c_arg = strtrim(cur);
                if (!c_arg) RETERR("file %s: line %d: field 5: expected argument, got end of line\n", fn, line_n);
                else it->arg = strdup(c_arg);

                nmi_config_push_menu_item(&cfg, it);
            } else RETERR("file %s: line %d: field 1: unknown type '%s'", fn, line_n, c_typ);
        }

        #undef RETERR

        fclose(cfgfile);
        free(line);
    }
    NMI_ASSERT(!errno, "could not read config dir: %s", strerror(errno));

    // close the config dir
    closedir(cfgdir);

    // add a default entry if none were found
    if (!cfg) {
        nmi_menu_item_t *it = calloc(1, sizeof(nmi_menu_item_t));
        it->loc = NMI_MENU_LOCATION_MAIN_MENU;
        it->lbl = strdup("nickel-menu-inject");
        it->arg = strdup("See KOBOeReader/.add/nmi/doc for instructions on how to customize this menu.");
        it->act = nmi_action_dbgerror;
        nmi_config_push_menu_item(&cfg, it);
    }

    for (nmi_config_t *cur = cfg; cur; cur = cur->next)
        if (cur->type == NMI_CONFIG_TYPE_MENU_ITEM)
            NMI_LOG("cfg(NMI_CONFIG_TYPE_MENU_ITEM) : %d:%s:%p:%s", cur->value.menu_item->loc, cur->value.menu_item->lbl, cur->value.menu_item->act, cur->value.menu_item->arg);

    // return the head of the list
    NMI_RETURN_OK(cfg);
    #undef NMI_ERR_RET
}

nmi_menu_item_t **nmi_config_get_menu(nmi_config_t *cfg, size_t *n_out) {
    *n_out = 0;
    for (nmi_config_t *cur = cfg; cur; cur = cur->next)
        if (cur->type == NMI_CONFIG_TYPE_MENU_ITEM)
            (*n_out)++;

    nmi_menu_item_t **it = calloc(*n_out, sizeof(nmi_menu_item_t*));
    if (!it)
        return NULL;

    nmi_menu_item_t **tmp = it;
    for (nmi_config_t *cur = cfg; cur; cur = cur->next)
        if (cur->type == NMI_CONFIG_TYPE_MENU_ITEM)
           *(tmp++) = cur->value.menu_item;

    return it;
}

void nmi_config_free(nmi_config_t *cfg) {
    while (cfg) {
        nmi_config_t *n = cfg->next;

        if (cfg->type == NMI_CONFIG_TYPE_MENU_ITEM) {
            free(cfg->value.menu_item->lbl);
            free(cfg->value.menu_item->arg);
            free(cfg->value.menu_item);
        }
        free(cfg);

        cfg = n;
    }
};
