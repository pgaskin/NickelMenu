#define _GNU_SOURCE // asprintf
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "action.h"
#include "config.h"
#include "generator.h"
#include "menu.h"
#include "util.h"

struct nm_config_file_t {
    char             *path;
    struct timespec  mtime;
    nm_config_file_t *next;
};

nm_config_file_t *nm_config_files(char **err_out) {
    #define NM_ERR_RET NULL

    nm_config_file_t *cfs = NULL, *cfc = NULL;

    struct dirent **nl;
    int n = scandir(NM_CONFIG_DIR, &nl, NULL, alphasort);
    NM_ASSERT(n != -1, "could not scan config dir: %s", strerror(errno));

    for (int i = 0; i < n; i++) {
        struct dirent *de = nl[i];
        
        char *fn;
        NM_ASSERT(asprintf(&fn, "%s/%s", NM_CONFIG_DIR, de->d_name) != -1, "could not build full path for config file");

        struct stat statbuf;
        NM_ASSERT(!stat(fn, &statbuf), "could not stat %s", fn);

        // skip it if it isn't a file
        if (de->d_type != DT_REG && !S_ISREG(statbuf.st_mode)) {
            NM_LOG("config: skipping %s because not a regular file", fn);
            free(fn);
            continue;
        }

        // skip special files, including:
        // - dotfiles
        // - vim: .*~, .*.s?? (unix only, usually swp or swo), *.swp, *.swo
        // - gedit: .*~
        // - emacs: .*~, #*#
        // - kate: .*.kate-swp
        // - macOS: .DS_Store*, .Spotlight-V*, ._*
        // - Windows: [Tt]humbs.db, desktop.ini
        char *bn = de->d_name;
        char *ex = strrchr(bn, '.');
        ex = (ex && ex != bn && bn[0] != '.') ? ex : NULL;
        char lc = bn[strlen(bn)-1];
        if ((bn[0] == '.') ||
            (lc == '~') ||
            (bn[0] == '#' && lc == '#') ||
            (ex && (!strcmp(ex, ".swo") || !strcmp(ex, ".swp"))) ||
            (!strcmp(&bn[1], "humbs.db") && tolower(bn[0]) == 't') ||
            (!strcmp(bn, "desktop.ini"))) {
            NM_LOG("config: skipping %s because it's a special file", fn);
            free(fn);
            continue;
        }

        if (cfc) {
            cfc->next = calloc(1, sizeof(nm_config_file_t));
            cfc = cfc->next;
        } else {
            cfs = calloc(1, sizeof(nm_config_file_t));
            cfc = cfs;
        }

        cfc->path = fn;
        cfc->mtime = statbuf.st_mtim;

        free(de);
    }

    free(nl);

    NM_RETURN_OK(cfs);
    #undef NM_ERR_RET
}

bool nm_config_files_update(nm_config_file_t **files, char **err_out) {
    #define NM_ERR_RET false
    NM_ASSERT(files, "files pointer must not be null");

    nm_config_file_t *nfiles = nm_config_files(err_out);
    if (*err_out)
        return NM_ERR_RET;

    if (!*files) {
        *files = nfiles;
        NM_RETURN_OK(true);
    }

    bool ch = false;
    nm_config_file_t *op = *files;
    nm_config_file_t *np = nfiles;
    
    while (op && np) {
        if (strcmp(op->path, np->path) || op->mtime.tv_sec != np->mtime.tv_sec || op->mtime.tv_nsec != np->mtime.tv_nsec) {
            ch = true;
            break;
        }
        op = op->next;
        np = np->next;
    }

    if (ch || op || np) {
        nm_config_files_free(*files);
        *files = nfiles;
        NM_RETURN_OK(true);
    } else {
        nm_config_files_free(nfiles);
        NM_RETURN_OK(false);
    }

    #undef NM_ERR_RET
}

void nm_config_files_free(nm_config_file_t *files) {
    while (files) {
        nm_config_file_t *tmp = files->next;
        free(files);
        files = tmp;
    }
}

typedef enum {
    NM_CONFIG_TYPE_MENU_ITEM = 1,
    NM_CONFIG_TYPE_GENERATOR = 2,
} nm_config_type_t;

struct nm_config_t {
    nm_config_type_t type;
    bool generated;
    union {
        nm_menu_item_t *menu_item;
        nm_generator_t *generator;
    } value;
    nm_config_t *next;
};

// nm_config_parse__state_t contains the current state of the config parser. It
// should be initialized to zero. The nm_config_parse__append__* functions will
// deep-copy the item to append (to malloc'd memory). Each call will always
// leave the state consistent, even on error (i.e. it will always be safe to
// nm_config_free cfg_s).
typedef struct nm_config_parse__state_t {
    nm_config_t *cfg_s; // config (first)
    nm_config_t *cfg_c; // config (current)

    nm_menu_item_t   *cfg_it_c;     // menu item (current)
    nm_menu_action_t *cfg_it_act_s; // menu action (first)
    nm_menu_action_t *cfg_it_act_c; // menu action (current)

    nm_generator_t *cfg_gn_c; // generator (current)
} nm_config_parse__state_t;

typedef enum nm_config_parse__append__ret_t {
    NM_CONFIG_PARSE__APPEND__RET_OK = 0,
    NM_CONFIG_PARSE__APPEND__RET_ALLOC_ERROR = 1,
    NM_CONFIG_PARSE__APPEND__RET_ACTION_MUST_BE_AFTER_ITEM = 2,
} nm_config_parse__append__ret_t;

static const char* nm_config_parse__strerror(nm_config_parse__append__ret_t ret) {
    switch (ret) {
    case NM_CONFIG_PARSE__APPEND__RET_OK:
        return NULL;
    case NM_CONFIG_PARSE__APPEND__RET_ALLOC_ERROR:
        return "error allocating memory";
    case NM_CONFIG_PARSE__APPEND__RET_ACTION_MUST_BE_AFTER_ITEM:
        return "unexpected chain, must be directly after a menu item or another chain";
    default:
        return "unknown error";
    }
}

// note: action will be ignored (add it with append_action)
static nm_config_parse__append__ret_t nm_config_parse__append_item(nm_config_parse__state_t *restrict state, nm_menu_item_t *const restrict it) {
    nm_config_t      *cfg_n        = calloc(1, sizeof(nm_config_t));
    nm_menu_item_t   *cfg_it_n     = calloc(1, sizeof(nm_menu_item_t));

    if (!cfg_n || !cfg_it_n) {
        free(cfg_n);
        free(cfg_it_n);
        return NM_CONFIG_PARSE__APPEND__RET_ALLOC_ERROR;
    }

    *cfg_n = (nm_config_t){
        .type      = NM_CONFIG_TYPE_MENU_ITEM,
        .generated = false,
        .value     = { .menu_item = cfg_it_n },
        .next      = NULL,
    };

    *cfg_it_n = (nm_menu_item_t){
        .loc    = it->loc,
        .lbl    = strdup(it->lbl ? it->lbl : ""),
        .action = NULL,
    };

    if (!cfg_it_n->lbl)
        return NM_CONFIG_PARSE__APPEND__RET_ALLOC_ERROR;

    if (state->cfg_c)
        state->cfg_c->next = cfg_n;
    else
        state->cfg_s = cfg_n;

    state->cfg_c    = cfg_n;
    state->cfg_it_c = cfg_it_n;

    state->cfg_it_act_s = NULL;
    state->cfg_it_act_c = NULL;
    state->cfg_gn_c     = NULL;

    return NM_CONFIG_PARSE__APPEND__RET_OK;
}

// note: next will be ignored (add another one by calling this again)
static int nm_config_parse__append_action(nm_config_parse__state_t *restrict state, nm_menu_action_t *const restrict act) {
    if (!state->cfg_c || !state->cfg_it_c || state->cfg_gn_c)
        return NM_CONFIG_PARSE__APPEND__RET_ACTION_MUST_BE_AFTER_ITEM;

    nm_menu_action_t *cfg_it_act_n = calloc(1, sizeof(nm_menu_action_t));

    if (!cfg_it_act_n)
        return NM_CONFIG_PARSE__APPEND__RET_ALLOC_ERROR;

    *cfg_it_act_n = (nm_menu_action_t){
        .act        = act->act,
        .on_failure = act->on_failure,
        .on_success = act->on_success,
        .arg        = strdup(act->arg ? act->arg : ""),
        .next       = NULL,
    };

    if (!cfg_it_act_n->arg)
        return NM_CONFIG_PARSE__APPEND__RET_ALLOC_ERROR;

    if (!state->cfg_it_c->action)
        state->cfg_it_c->action = cfg_it_act_n;

    if (state->cfg_it_act_c)
        state->cfg_it_act_c->next = cfg_it_act_n;
    else
        state->cfg_it_act_s = cfg_it_act_n;

    state->cfg_it_act_c = cfg_it_act_n;

    return NM_CONFIG_PARSE__APPEND__RET_OK;
}

static nm_config_parse__append__ret_t nm_config_parse__append_generator(nm_config_parse__state_t *restrict state, nm_generator_t *const restrict gn) {
    nm_config_t    *cfg_n    = calloc(1, sizeof(nm_config_t));
    nm_generator_t *cfg_gn_n = calloc(1, sizeof(nm_generator_t));

    if (!cfg_n || !cfg_gn_n) {
        free(cfg_n);
        free(cfg_gn_n);
        return NM_CONFIG_PARSE__APPEND__RET_ALLOC_ERROR;
    }

    *cfg_n = (nm_config_t){
        .type      = NM_CONFIG_TYPE_GENERATOR,
        .generated = false,
        .value     = { .generator = cfg_gn_n },
        .next      = NULL,
    };

    *cfg_gn_n = (nm_generator_t){
        .desc     = strdup(gn->desc ? gn->desc : ""),
        .loc      = gn->loc,
        .arg      = strdup(gn->arg ? gn->arg : ""),
        .generate = gn->generate,
    };

    if (!cfg_gn_n->desc || !cfg_gn_n->arg) {
        free(cfg_gn_n->desc);
        free(cfg_gn_n->arg);
        return NM_CONFIG_PARSE__APPEND__RET_ALLOC_ERROR;
    }

    if (state->cfg_c)
        state->cfg_c->next = cfg_n;
    else
        state->cfg_s = cfg_n;

    state->cfg_c    = cfg_n;
    state->cfg_gn_c = cfg_gn_n;

    state->cfg_it_c     = NULL;
    state->cfg_it_act_s = NULL;
    state->cfg_it_act_c = NULL;

    return NM_CONFIG_PARSE__APPEND__RET_OK;
}

nm_config_t *nm_config_parse(nm_config_file_t *files, char **err_out) {
    #define NM_ERR_RET NULL
    NM_LOG("config: reading config dir %s", NM_CONFIG_DIR);

    nm_config_parse__state_t state = {0};

    for (nm_config_file_t *cf = files; cf; cf = cf->next) {
        NM_LOG("config: reading config file %s", cf->path);

        FILE *cfgfile = fopen(cf->path, "r");
        NM_ASSERT(cfgfile, "could not open file: %s", strerror(errno));

        // note: nm_config_free will handle freeing any added items, actions,
        //       and generators too.
        #define RETERR(fmt, ...) do {          \
            fclose(cfgfile);                   \
            free(line);                        \
            nm_config_free(state.cfg_c);       \
            NM_RETURN_ERR(fmt, ##__VA_ARGS__); \
        } while (0)

        // parse each line
        char *line;
        int line_n = 0;
        ssize_t line_sz;
        size_t line_bufsz = 0;

        while ((line_sz = getline(&line, &line_bufsz, cfgfile)) != -1) {
            line_n++;

            // empty line or comment
            char *cur = strtrim(line);
            if (!*cur || *cur == '#')
                continue;

            // field 1: type
            char *s_typ = strtrim(strsep(&cur, ":"));
            if (!strcmp(s_typ, "menu_item")) {
                // type: menu_item

                // type: menu_item - field 2: location
                nm_menu_location_t p_loc;
                char *s_loc = strtrim(strsep(&cur, ":"));
                if (!s_loc) {
                    RETERR("file %s: line %d: field 2: expected location, got end of line", cf->path, line_n);
                } else if (!strcmp(s_loc, "main")) {
                    p_loc = NM_MENU_LOCATION_MAIN_MENU;
                } else if (!strcmp(s_loc, "reader")) {
                    p_loc = NM_MENU_LOCATION_READER_MENU;
                } else RETERR("file %s: line %d: field 2: unknown location '%s'", cf->path, line_n, s_loc);

                // type: menu_item - field 3: label
                char *p_lbl = strtrim(strsep(&cur, ":"));
                if (!p_lbl)
                    RETERR("file %s: line %d: field 3: expected label, got end of line", cf->path, line_n);

                // type: menu_item - field 4: action
                nm_action_fn_t p_act;
                char *s_act = strtrim(strsep(&cur, ":"));
                if (!s_act) {
                    RETERR("file %s: line %d: field 4: expected action, got end of line", cf->path, line_n);
                    #define X(name)                 \
                } else if (!strcmp(s_act, #name)) { \
                    p_act = NM_ACTION(name);
                    NM_ACTIONS
                    #undef X
                } else RETERR("file %s: line %d: field 4: unknown action '%s'", cf->path, line_n, s_act);

                // type: menu_item - field 5: argument
                char *p_arg = strtrim(cur);
                if (!p_arg)
                    RETERR("file %s: line %d: field 5: expected argument, got end of line\n", cf->path, line_n);

                nm_config_parse__append__ret_t ret;

                if ((ret = nm_config_parse__append_item(&state, &(nm_menu_item_t){
                    .loc    = p_loc,
                    .lbl    = p_lbl,
                    .action = NULL,
                }))) RETERR("file %s: line %d: error appending item to config: %s", cf->path, line_n, nm_config_parse__strerror(ret));

                if ((ret = nm_config_parse__append_action(&state, &(nm_menu_action_t){
                    .act        = p_act,
                    .on_failure = true,
                    .on_success = true,
                    .arg        = p_arg,
                    .next       = NULL,
                }))) RETERR("file %s: line %d: error appending action to config: %s", cf->path, line_n, nm_config_parse__strerror(ret));

            } else if (!strncmp(s_typ, "chain", 5)) {
                // type: chain

                bool p_on_failure, p_on_success;
                if (!strcmp(s_typ, "chain_success")) {
                    p_on_failure = false;
                    p_on_success = true;
                } else if (!strcmp(s_typ, "chain_always")) {
                    p_on_failure = true;
                    p_on_success = true;
                } else if (!strcmp(s_typ, "chain_failure")) {
                    p_on_failure = true;
                    p_on_success = false;
                } else RETERR("file %s: line %d: field 1: unknown type '%s'", cf->path, line_n, s_typ);

                // type: chain - field 2: action
                nm_action_fn_t p_act;
                char *s_act = strtrim(strsep(&cur, ":"));
                if (!s_act) {
                    RETERR("file %s: line %d: field 2: expected action, got end of line", cf->path, line_n);
                    #define X(name)                 \
                } else if (!strcmp(s_act, #name)) { \
                    p_act = NM_ACTION(name);
                    NM_ACTIONS
                    #undef X
                } else RETERR("file %s: line %d: field 2: unknown action '%s'", cf->path, line_n, s_act);

                // type: chain - field 3: argument
                char *p_arg = strtrim(cur);
                if (!p_arg)
                    RETERR("file %s: line %d: field 3: expected argument, got end of line\n", cf->path, line_n);

                nm_config_parse__append__ret_t ret;

                if ((ret = nm_config_parse__append_action(&state, &(nm_menu_action_t){
                    .act        = p_act,
                    .on_failure = p_on_failure,
                    .on_success = p_on_success,
                    .arg        = p_arg,
                    .next       = NULL,
                }))) RETERR("file %s: line %d: error appending action to config: %s", cf->path, line_n, nm_config_parse__strerror(ret));

            } else if (!strcmp(s_typ, "generator")) {
                // type: generator

                // type: generator - field 2: location
                nm_menu_location_t p_loc;
                char *s_loc = strtrim(strsep(&cur, ":"));
                if (!s_loc) {
                    RETERR("file %s: line %d: field 2: expected location, got end of line", cf->path, line_n);
                } else if (!strcmp(s_loc, "main")) {
                    p_loc = NM_MENU_LOCATION_MAIN_MENU;
                } else if (!strcmp(s_loc, "reader")) {
                    p_loc = NM_MENU_LOCATION_READER_MENU;
                } else RETERR("file %s: line %d: field 2: unknown location '%s'", cf->path, line_n, s_loc);

                // type: generator - field 3: generator
                nm_generator_fn_t p_generate;
                char *s_generate = strtrim(strsep(&cur, ":"));
                if (!s_generate) {
                    RETERR("file %s: line %d: field 3: expected generator, got end of line", cf->path, line_n);
                    #define X(name)                 \
                } else if (!strcmp(s_generate, #name)) { \
                    p_generate = NM_GENERATOR(name);
                    NM_GENERATORS
                    #undef X
                } else RETERR("file %s: line %d: field 3: unknown generator '%s'", cf->path, line_n, s_generate);

                // type: generator - field 4: argument (optional)
                char *p_arg = strtrim(cur);

                nm_config_parse__append__ret_t ret;

                if ((ret = nm_config_parse__append_generator(&state, &(nm_generator_t){
                    .desc     = s_generate,
                    .loc      = p_loc,
                    .arg      = p_arg,
                    .generate = p_generate,
                }))) RETERR("file %s: line %d: error appending generator to config: %s", cf->path, line_n, nm_config_parse__strerror(ret));

            } else RETERR("file %s: line %d: field 1: unknown type '%s'", cf->path, line_n, s_typ);
        }

        fclose(cfgfile);
        free(line);
    }

    // add a default entry if none were found
    if (!state.cfg_c) {
        nm_config_parse__append__ret_t ret;

        if ((ret = nm_config_parse__append_item(&state, &(nm_menu_item_t){
            .loc    = NM_MENU_LOCATION_MAIN_MENU,
            .lbl    = "NickelMenu",
            .action = NULL,
        }))) NM_RETURN_ERR("error appending default item to empty config: %s", nm_config_parse__strerror(ret));

        if ((ret = nm_config_parse__append_action(&state, &(nm_menu_action_t){
            .act        = NM_ACTION(dbg_toast),
            .on_failure = true,
            .on_success = true,
            .arg        = "See .adds/nm/doc for instructions on how to customize this menu.",
            .next       = NULL,
        }))) NM_RETURN_ERR("error appending default action to empty config: %s", nm_config_parse__strerror(ret));
    }

    size_t mm = 0, rm = 0;
    for (nm_config_t *cur = state.cfg_s; cur; cur = cur->next) {
        switch (cur->type) {
        case NM_CONFIG_TYPE_MENU_ITEM:
            NM_LOG("cfg(NM_CONFIG_TYPE_MENU_ITEM) : %d:%s", cur->value.menu_item->loc, cur->value.menu_item->lbl);
            for (nm_menu_action_t *cur_act = cur->value.menu_item->action; cur_act; cur_act = cur_act->next)
                NM_LOG("...cfg(NM_CONFIG_TYPE_MENU_ITEM) (%s%s%s) : %p:%s", cur_act->on_success ? "on_success" : "", (cur_act->on_success && cur_act->on_failure) ? ", " : "", cur_act->on_failure ? "on_failure" : "", cur_act->act, cur_act->arg);
            switch (cur->value.menu_item->loc) {
                case NM_MENU_LOCATION_MAIN_MENU:   mm++; break;
                case NM_MENU_LOCATION_READER_MENU: rm++; break;
            }
            break;
        case NM_CONFIG_TYPE_GENERATOR:
            NM_LOG("cfg(NM_CONFIG_TYPE_GENERATOR) : %d:%s(%p):%s", cur->value.generator->loc, cur->value.generator->desc, cur->value.generator->generate, cur->value.generator->arg);
            break;
        }
    }
    NM_ASSERT(mm <= NM_CONFIG_MAX_MENU_ITEMS_PER_MENU, "too many menu items in main menu (> %d)", NM_CONFIG_MAX_MENU_ITEMS_PER_MENU);
    NM_ASSERT(rm <= NM_CONFIG_MAX_MENU_ITEMS_PER_MENU, "too many menu items in reader menu (> %d)", NM_CONFIG_MAX_MENU_ITEMS_PER_MENU);

    NM_RETURN_OK(state.cfg_s);
    #undef NM_ERR_RET
}

void nm_config_generate(nm_config_t *cfg) {
    NM_LOG("config: removing any previously generated items");
    for (nm_config_t *prev = NULL, *cur = cfg; cur; cur = cur->next) {
        if (prev && cur->generated) { // we can't get rid of the first item and it won't be generated anyways (plus doing it this way simplifies the loop)
            prev->next = cur->next; // skip the generated item
            cur->next = NULL; // so we only free that item
            nm_config_free(cur);
            cur = prev; // continue with the new current item
        } else {
            prev = cur; // the item was kept, so it's now the previous one
        }
    }

    NM_LOG("config: running generators");
    for (nm_config_t *cur = cfg; cur; cur = cur->next) {
        if (cur->type == NM_CONFIG_TYPE_GENERATOR) {
            NM_LOG("config: running generator %s:%s", cur->value.generator->desc, cur->value.generator->arg);

            size_t sz;
            nm_menu_item_t **items = nm_generator_do(cur->value.generator, &sz);
            if (!items) {
                NM_LOG("config: ... no items generated");
                continue;
            }

            NM_LOG("config: ... %zu items generated", sz);
            for (size_t i = 0; i < sz; i++) {
                nm_config_t *tmp = calloc(1, sizeof(nm_config_t));
                tmp->type = NM_CONFIG_TYPE_MENU_ITEM;
                tmp->value.menu_item = items[i];
                tmp->generated = true;
                tmp->next = cur->next;
                cur->next = tmp;
            }
            free(items);
        }
    }

    NM_LOG("config: generated items");
    for (nm_config_t *cur = cfg; cur; cur = cur->next) {
        if (cur->generated) {
            switch (cur->type) {
            case NM_CONFIG_TYPE_MENU_ITEM:
                NM_LOG("cfg(NM_CONFIG_TYPE_MENU_ITEM) : %d:%s", cur->value.menu_item->loc, cur->value.menu_item->lbl);
                for (nm_menu_action_t *cur_act = cur->value.menu_item->action; cur_act; cur_act = cur_act->next)
                    NM_LOG("...cfg(NM_CONFIG_TYPE_MENU_ITEM) (%s%s%s) : %p:%s", cur_act->on_success ? "on_success" : "", (cur_act->on_success && cur_act->on_failure) ? ", " : "", cur_act->on_failure ? "on_failure" : "", cur_act->act, cur_act->arg);
                break;
            case NM_CONFIG_TYPE_GENERATOR:
                NM_LOG("cfg(NM_CONFIG_TYPE_GENERATOR) : %d:%s(%p):%s", cur->value.generator->loc, cur->value.generator->desc, cur->value.generator->generate, cur->value.generator->arg);
                break;
            }
        }
    }
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

        switch (cfg->type) {
        case NM_CONFIG_TYPE_MENU_ITEM:
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
            break;
        case NM_CONFIG_TYPE_GENERATOR:
            free(cfg->value.generator->arg);
            free(cfg->value.generator->desc);
            free(cfg->value.generator);
            break;
        }
        free(cfg);

        cfg = n;
    }
}
