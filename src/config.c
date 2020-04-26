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

#include "action.h"
#include "config.h"
#include "menu.h"
#include "util.h"

#ifndef NM_CONFIG_DIR
#define NM_CONFIG_DIR "/mnt/onboard/.adds/nm"
#endif

#ifndef NM_CONFIG_MAX_MENU_ITEMS_PER_MENU
#define NM_CONFIG_MAX_MENU_ITEMS_PER_MENU 50
#endif

typedef enum {
    NM_CONFIG_TYPE_MENU_ITEM = 1,
} nm_config_type_t;

struct nm_config_t {
    nm_config_type_t type;
    union {
        nm_menu_item_t *menu_item;
    } value;
    nm_config_t *next;
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

static void nm_config_push_menu_item(nm_config_t **cfg, nm_menu_item_t *it) {
    nm_config_t *tmp = calloc(1, sizeof(nm_config_t));
    tmp->type = NM_CONFIG_TYPE_MENU_ITEM;
    tmp->value.menu_item = it;
    tmp->next = *cfg;
    *cfg = tmp;
}

static void nm_config_push_action(nm_menu_action_t **cur, nm_menu_action_t *act) {
    if (*cur)
        (*cur)->next = act;
    *cur = act;
}

nm_config_t *nm_config_parse(char **err_out) {
    #define NM_ERR_RET NULL
    NM_LOG("config: reading config dir %s", NM_CONFIG_DIR);

    // set up the linked list
    nm_config_t *cfg = NULL;

    // open the config dir
    DIR *cfgdir;
    NM_ASSERT((cfgdir = opendir(NM_CONFIG_DIR)), "could not open config dir: %s", strerror(errno));

    // loop over the dirents of the config dir
    struct dirent *dirent; errno = 0;
    while ((dirent = readdir(cfgdir))) {
        char *fn;
        NM_ASSERT(asprintf(&fn, "%s/%s", NM_CONFIG_DIR, dirent->d_name) != -1, "could not build full path for config file");

        // skip it if it isn't a file
        bool reg = dirent->d_type == DT_REG;
        if (dirent->d_type == DT_UNKNOWN) {
            struct stat statbuf;
            NM_ASSERT(!stat(fn, &statbuf), "could not stat %s", fn);
            reg = S_ISREG(statbuf.st_mode);
        }
        if (!reg) {
            NM_LOG("config: skipping %s because not a regular file", fn);
            continue;
        }

        // open the config file
        NM_LOG("config: reading config file %s", fn);
        FILE *cfgfile;
        NM_ASSERT((cfgfile = fopen(fn, "r")), "could not open file: %s", strerror(errno));

        #define RETERR(fmt, ...) do {           \
            fclose(cfgfile);                    \
            free(line);                         \
            closedir(cfgdir);                   \
            NM_RETURN_ERR(fmt, ##__VA_ARGS__); \
        } while (0)

        // parse each line
        char *line;
        int line_n = 0;
        ssize_t line_sz;
        size_t line_bufsz = 0;

        nm_menu_item_t *it = NULL;
        nm_menu_action_t *cur_act = NULL;
        while ((line_sz = getline(&line, &line_bufsz, cfgfile)) != -1) {
            line_n++;

            // empty line or comment
            char *cur = strtrim(line);
            if (!*cur || *cur == '#')
                continue;

            // field 1: type
            char *c_typ = strtrim(strsep(&cur, ":"));
            if (!strcmp(c_typ, "menu_item")) {
                if (it) nm_config_push_menu_item(&cfg, it);
                // type: menu_item
                it = calloc(1, sizeof(nm_menu_item_t));
                cur_act = NULL;
                // type: menu_item - field 2: location
                char *c_loc = strtrim(strsep(&cur, ":"));
                if (!c_loc) RETERR("file %s: line %d: field 2: expected location, got end of line", fn, line_n);
                else if (!strcmp(c_loc, "main"))   it->loc = NM_MENU_LOCATION_MAIN_MENU;
                else if (!strcmp(c_loc, "reader")) it->loc = NM_MENU_LOCATION_READER_MENU;
                else RETERR("file %s: line %d: field 2: unknown location '%s'", fn, line_n, c_loc);

                // type: menu_item - field 3: label
                char *c_lbl = strtrim(strsep(&cur, ":"));
                if (!c_lbl) RETERR("file %s: line %d: field 3: expected label, got end of line", fn, line_n);
                else it->lbl = strdup(c_lbl);

                nm_menu_action_t *action = calloc(1, sizeof(nm_menu_action_t));
                // type: menu_item - field 4: action
                char *c_act = strtrim(strsep(&cur, ":"));
                if (!c_act) RETERR("file %s: line %d: field 4: expected action, got end of line", fn, line_n);
                #define X(name) else if (!strcmp(c_act, #name)) action->act = NM_ACTION(name);
                NM_ACTIONS
                #undef X
                else RETERR("file %s: line %d: field 4: unknown action '%s'", fn, line_n, c_act);

                // type: menu_item - field 5: argument
                char *c_arg = strtrim(cur);
                if (!c_arg) RETERR("file %s: line %d: field 5: expected argument, got end of line\n", fn, line_n);
                else action->arg = strdup(c_arg);
                nm_config_push_action(&cur_act, action);
                it->action = cur_act;
            } else if (!strcmp(c_typ, "chain")) {
                if (!it) RETERR("file %s: line %d: unexpected chain, no menu_item to link to", fn, line_n);
                nm_menu_action_t *action = calloc(1, sizeof(nm_menu_action_t));

                // type: chain - field 2: action
                char *c_act = strtrim(strsep(&cur, ":"));
                if (!c_act) RETERR("file %s: line %d: field 2: expected action, got end of line", fn, line_n);
                #define X(name) else if (!strcmp(c_act, #name)) action->act = NM_ACTION(name);
                NM_ACTIONS
                #undef X
                else RETERR("file %s: line %d: field 2: unknown action '%s'", fn, line_n, c_act);

                // type: chain - field 3: argument
                char *c_arg = strtrim(cur);
                if (!c_arg) RETERR("file %s: line %d: field 3: expected argument, got end of line\n", fn, line_n);
                else action->arg = strdup(c_arg);
                nm_config_push_action(&cur_act, action);
            } else RETERR("file %s: line %d: field 1: unknown type '%s'", fn, line_n, c_typ);
        }
        // Push the last menu item onto the config
        if (it) nm_config_push_menu_item(&cfg, it);
        it = NULL;
        cur_act = NULL;
        #undef RETERR

        fclose(cfgfile);
        free(line);
    }
    NM_ASSERT(!errno, "could not read config dir: %s", strerror(errno));

    // close the config dir
    closedir(cfgdir);

    // add a default entry if none were found
    if (!cfg) {
        nm_menu_item_t *it = calloc(1, sizeof(nm_menu_item_t));
        nm_menu_action_t *action = calloc(1, sizeof(nm_menu_action_t));
        it->loc = NM_MENU_LOCATION_MAIN_MENU;
        it->lbl = strdup("NickelMenu");
        action->arg = strdup("See KOBOeReader/.add/nm/doc for instructions on how to customize this menu.");
        action->act = NM_ACTION(dbg_toast);
        nm_config_push_menu_item(&cfg, it);
    }

    size_t mm = 0, rm = 0;
    for (nm_config_t *cur = cfg; cur; cur = cur->next) {
        if (cur->type == NM_CONFIG_TYPE_MENU_ITEM) {
            for (nm_menu_action_t *cur_act = cur->value.menu_item->action; cur_act; cur_act = cur_act->next)
                NM_LOG("cfg(NM_CONFIG_TYPE_MENU_ITEM) : %d:%s:%p:%s", cur->value.menu_item->loc, cur->value.menu_item->lbl, cur_act->act, cur_act->arg);
            switch (cur->value.menu_item->loc) {
                case NM_MENU_LOCATION_MAIN_MENU:   mm++; break;
                case NM_MENU_LOCATION_READER_MENU: rm++; break;
            }
        }
    }
    NM_ASSERT(mm <= NM_CONFIG_MAX_MENU_ITEMS_PER_MENU, "too many menu items in main menu (> %d)", NM_CONFIG_MAX_MENU_ITEMS_PER_MENU);
    NM_ASSERT(rm <= NM_CONFIG_MAX_MENU_ITEMS_PER_MENU, "too many menu items in reader menu (> %d)", NM_CONFIG_MAX_MENU_ITEMS_PER_MENU);

    // return the head of the list
    NM_RETURN_OK(cfg);
    #undef NM_ERR_RET
}

nm_menu_item_t **nm_config_get_menu(nm_config_t *cfg, size_t *n_out) {
    *n_out = 0;
    for (nm_config_t *cur = cfg; cur; cur = cur->next)
        if (cur->type == NM_CONFIG_TYPE_MENU_ITEM)
            (*n_out)++;

    nm_menu_item_t **it = calloc(*n_out, sizeof(nm_menu_item_t*));
    if (!it)
        return NULL;

    nm_menu_item_t **tmp = &it[*n_out - 1];
    for (nm_config_t *cur = cfg; cur; cur = cur->next)
        if (cur->type == NM_CONFIG_TYPE_MENU_ITEM)
           *(tmp--) = cur->value.menu_item;

    return it;
}

void nm_config_free(nm_config_t *cfg) {
    while (cfg) {
        nm_config_t *n = cfg->next;

        if (cfg->type == NM_CONFIG_TYPE_MENU_ITEM) {
            free(cfg->value.menu_item->lbl);
            if (cfg->value.menu_item->action) {
                nm_menu_action_t *cur = cfg->value.menu_item->action;
                nm_menu_action_t *tmp;
                while (cur) {
                    tmp = cur;
                    cur = cur->next;
                    free(tmp->arg);
                    free(tmp);
                }
            }
            free(cfg->value.menu_item);
        }
        free(cfg);

        cfg = n;
    }
};
