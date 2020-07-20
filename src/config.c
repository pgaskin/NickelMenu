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


// nm_config_files_filter skips special files, including:
// - dotfiles
// - vim: .*~, .*.s?? (unix only, usually swp or swo), *.swp, *.swo
// - gedit: .*~
// - emacs: .*~, #*#
// - kate: .*.kate-swp
// - macOS: .DS_Store*, .Spotlight-V*, ._*
// - Windows: [Tt]humbs.db, desktop.ini
static int nm_config_files_filter(const struct dirent *de) {
    const char *bn = de->d_name;
    char *ex = strrchr(bn, '.');
    ex = (ex && ex != bn && bn[0] != '.') ? ex : NULL;
    char lc = bn[strlen(bn)-1];
    if ((bn[0] == '.') ||
        (lc == '~') ||
        (bn[0] == '#' && lc == '#') ||
        (ex && (!strcmp(ex, ".swo") || !strcmp(ex, ".swp"))) ||
        (!strcmp(&bn[1], "humbs.db") && tolower(bn[0]) == 't') ||
        (!strcmp(bn, "desktop.ini"))) {
        NM_LOG("config: skipping %s/%s because it's a special file", NM_CONFIG_DIR, de->d_name);
        return 0;
    }
    return 1;
}

nm_config_file_t *nm_config_files() {
    nm_config_file_t *cfs = NULL, *cfc = NULL;

    struct dirent **nl;
    int n = scandir(NM_CONFIG_DIR, &nl, nm_config_files_filter, alphasort);
    NM_CHECK(NULL, n != -1, "could not scan config dir: %s", strerror(errno));

    for (int i = 0; i < n; i++) {
        struct dirent *de = nl[i];

        char *fn;
        if (asprintf(&fn, "%s/%s", NM_CONFIG_DIR, de->d_name) == -1)
            fn = NULL;

        struct stat statbuf;
        if (!fn || stat(fn, &statbuf)) {
            while (cfs) {
                nm_config_file_t *tmp = cfs->next;
                free(cfs);
                cfs = tmp;
            }
            if (!fn)
                NM_ERR_RET(NULL, "could not build full path for config file");
            free(fn);
            NM_ERR_RET(NULL, "could not stat %s/%s", NM_CONFIG_DIR, de->d_name);
        }

        // skip it if it isn't a file
        if (de->d_type != DT_REG && !S_ISREG(statbuf.st_mode)) {
            NM_LOG("config: skipping %s because not a regular file", fn);
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

    return cfs;
}

int nm_config_files_update(nm_config_file_t **files) {
    NM_CHECK(false, files, "files pointer must not be null");

    nm_config_file_t *nfiles = nm_config_files();
    if (nm_err_peek())
        return -1; // the error is passed on

    if (!*files) {
        *files = nfiles;
        return 0;
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
        return 0;
    } else {
        nm_config_files_free(nfiles);
        return 1;
    }
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

static nm_config_parse__append__ret_t nm_config_parse__append_item(nm_config_parse__state_t *restrict state, nm_menu_item_t *const restrict it); // note: action pointer will be ignored (add it with append_action)
static nm_config_parse__append__ret_t nm_config_parse__append_action(nm_config_parse__state_t *restrict state, nm_menu_action_t *const restrict act); // note: next pointer will be ignored (add another one by calling this again)
static nm_config_parse__append__ret_t nm_config_parse__append_generator(nm_config_parse__state_t *restrict state, nm_generator_t *const restrict gn);

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

// note: line must point to the part after the config line type, and will be
//       modified. if the config line type doesn't match, everything will be
//       left as-is and false will be returned with nm_err cleared. if an error
//       occurs, nm_err will be set and true will be returned (since stuff
//       may be modified). otherwise, true is returned with nm_err cleared (the
//       parsed output will have strings pointing directly into the line).

static bool nm_config_parse__lineend_action(int field, char **line, bool p_on_success, bool p_on_failure, nm_menu_action_t *act_out);
static bool nm_config_parse__line_item(const char *type, char **line, nm_menu_item_t *it_out, nm_menu_action_t *action_out);
static bool nm_config_parse__line_chain(const char *type, char **line, nm_menu_action_t *act_out);
static bool nm_config_parse__line_generator(const char *type, char **line, nm_generator_t *gn_out);

nm_config_t *nm_config_parse(nm_config_file_t *files) {
    const char *err = NULL;

    FILE   *cfgfile    = NULL;
    char   *line       = NULL;
    size_t  line_bufsz = 0;
    int     line_n;
    ssize_t line_sz;

    nm_config_parse__append__ret_t ret;
    nm_config_parse__state_t state = {0};

    nm_menu_item_t   tmp_it;
    nm_menu_action_t tmp_act;
    nm_generator_t   tmp_gn;

    #define RETERR(fmt, ...) do {                  \
        if (cfgfile)                               \
            fclose(cfgfile);                       \
        free(line);                                \
        nm_config_free(state.cfg_c);               \
        NM_ERR_RET(NULL, fmt, ##__VA_ARGS__);      \
    } while (0)

    for (nm_config_file_t *cf = files; cf; cf = cf->next) {
        NM_LOG("config: reading config file %s", cf->path);

        line_n  = 0;
        cfgfile = fopen(cf->path, "r");

        if (!cfgfile)
            RETERR("could not open file: %s", strerror(errno));

        while ((line_sz = getline(&line, &line_bufsz, cfgfile)) != -1) {
            line_n++;

            char *cur = strtrim(line);
            if (!*cur || *cur == '#')
                continue; // empty line or comment

            char *s_typ = strtrim(strsep(&cur, ":"));

            if (nm_config_parse__line_item(s_typ, &cur, &tmp_it, &tmp_act)) {
                if ((err = nm_err()))
                    RETERR("file %s: line %d: parse menu_item: %s", cf->path, line_n, err);
                if ((ret = nm_config_parse__append_item(&state, &tmp_it)))
                    RETERR("file %s: line %d: error appending item to config: %s", cf->path, line_n, nm_config_parse__strerror(ret));
                if ((ret = nm_config_parse__append_action(&state, &tmp_act)))
                    RETERR("file %s: line %d: error appending action to config: %s", cf->path, line_n, nm_config_parse__strerror(ret));
                continue;
            }

            if (nm_config_parse__line_chain(s_typ, &cur, &tmp_act)) {
                if ((err = nm_err()))
                    RETERR("file %s: line %d: parse chain: %s", cf->path, line_n, err);
                if ((ret = nm_config_parse__append_action(&state, &tmp_act)))
                    RETERR("file %s: line %d: error appending action to config: %s", cf->path, line_n, nm_config_parse__strerror(ret));
                continue;
            }

            if (nm_config_parse__line_generator(s_typ, &cur, &tmp_gn)) {
                if ((err = nm_err()))
                    RETERR("file %s: line %d: parse generator: %s", cf->path, line_n, err);
                if ((ret = nm_config_parse__append_generator(&state, &tmp_gn)))
                    RETERR("file %s: line %d: error appending generator to config: %s", cf->path, line_n, nm_config_parse__strerror(ret));
                continue;
            }

            RETERR("file %s: line %d: field 1: unknown type '%s'", cf->path, line_n, s_typ);
        }

        // reset the current per-file state
        state.cfg_it_c     = NULL;
        state.cfg_it_act_s = NULL;
        state.cfg_it_act_c = NULL;
        state.cfg_gn_c     = NULL;

        fclose(cfgfile);
        cfgfile = NULL;
    }

    if (!state.cfg_c) {
        if ((ret = nm_config_parse__append_item(&state, &(nm_menu_item_t){
            .loc    = NM_MENU_LOCATION_MAIN_MENU,
            .lbl    = "NickelMenu",
            .action = NULL,
        }))) RETERR("error appending default item to empty config: %s", nm_config_parse__strerror(ret));

        if ((ret = nm_config_parse__append_action(&state, &(nm_menu_action_t){
            .act        = NM_ACTION(dbg_toast),
            .on_failure = true,
            .on_success = true,
            .arg        = "See .adds/nm/doc for instructions on how to customize this menu.",
            .next       = NULL,
        }))) RETERR("error appending default action to empty config: %s", nm_config_parse__strerror(ret));
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

    if (mm > NM_CONFIG_MAX_MENU_ITEMS_PER_MENU)
        RETERR("too many menu items in main menu (> %d)", NM_CONFIG_MAX_MENU_ITEMS_PER_MENU);
    if (rm > NM_CONFIG_MAX_MENU_ITEMS_PER_MENU)
        RETERR("too many menu items in reader menu (> %d)", NM_CONFIG_MAX_MENU_ITEMS_PER_MENU);

    return state.cfg_s;
}

static bool nm_config_parse__line_item(const char *type, char **line, nm_menu_item_t *it_out, nm_menu_action_t *action_out) {
    if (strcmp(type, "menu_item")) {
        nm_err_set(NULL);
        return false;
    }

    *it_out = (nm_menu_item_t){0};

    char *s_loc = strtrim(strsep(line, ":"));
    if      (!s_loc)                   NM_ERR_RET(NULL, "field 2: expected location, got end of line");
    else if (!strcmp(s_loc, "main"))   it_out->loc = NM_MENU_LOCATION_MAIN_MENU;
    else if (!strcmp(s_loc, "reader")) it_out->loc = NM_MENU_LOCATION_READER_MENU;
    else NM_ERR_RET(NULL, "field 2: unknown location '%s'", s_loc);

    char *p_lbl = strtrim(strsep(line, ":"));
    if (!p_lbl) NM_ERR_RET(NULL, "field 3: expected label, got end of line");
    it_out->lbl = p_lbl;

    return nm_config_parse__lineend_action(4, line, true, true, action_out);
}

static bool nm_config_parse__line_chain(const char *type, char **line, nm_menu_action_t *act_out) {
    if (strncmp(type, "chain_", 5)) {
        nm_err_set(NULL);
        return false;
    }

    bool p_on_success, p_on_failure;
    if (!strcmp(type, "chain_success")) {
        p_on_success = true;
        p_on_failure = false;
    } else if (!strcmp(type, "chain_always")) {
        p_on_success = true;
        p_on_failure = true;
    } else if (!strcmp(type, "chain_failure")) {
        p_on_success = false;
        p_on_failure = true;
    } else {
        nm_err_set(NULL);
        return false;
    }

    return nm_config_parse__lineend_action(2, line, p_on_success, p_on_failure, act_out);
}

static bool nm_config_parse__line_generator(const char *type, char **line, nm_generator_t *gn_out) {
    if (strcmp(type, "generator")) {
        nm_err_set(NULL);
        return false;
    }

    *gn_out = (nm_generator_t){0};

    char *s_loc = strtrim(strsep(line, ":"));
    if      (!s_loc)                   NM_ERR_RET(NULL, "field 2: expected location, got end of line");
    else if (!strcmp(s_loc, "main"))   gn_out->loc = NM_MENU_LOCATION_MAIN_MENU;
    else if (!strcmp(s_loc, "reader")) gn_out->loc = NM_MENU_LOCATION_READER_MENU;
    else NM_ERR_RET(NULL, "field 2: unknown location '%s'", s_loc);

    char *s_generate = strtrim(strsep(line, ":"));
    if (!s_generate) NM_ERR_RET(NULL, "field 3: expected generator, got end of line");
    #define X(name) \
    else if (!strcmp(s_generate, #name)) gn_out->generate = NM_GENERATOR(name);
    NM_GENERATORS
    #undef X
    else NM_ERR_RET(NULL, "field 3: unknown generator '%s'", s_generate);

    char *p_arg = strtrim(*line); // note: optional
    if (p_arg) gn_out->arg = p_arg;

    gn_out->desc = s_generate;

    nm_err_set(NULL);
    return true;
}

static bool nm_config_parse__lineend_action(int field, char **line, bool p_on_success, bool p_on_failure, nm_menu_action_t *act_out) {
    *act_out = (nm_menu_action_t){0};

    char *s_act = strtrim(strsep(line, ":"));
    if (!s_act) NM_ERR_RET(true, "field %d: expected action, got end of line", field);
    #define X(name) \
    else if (!strcmp(s_act, #name)) act_out->act = NM_ACTION(name);
    NM_ACTIONS
    #undef X
    else NM_ERR_RET(true, "field %d: unknown action '%s'", field, s_act);

    // type: menu_item - field 5: argument
    char *p_arg = strtrim(*line);
    if (!p_arg) NM_ERR_RET(true, "field %d: expected argument, got end of line\n", field+1);
    act_out->arg = p_arg;

    act_out->on_success = p_on_success;
    act_out->on_failure = p_on_failure;

    nm_err_set(NULL);
    return true;
}

static nm_config_parse__append__ret_t nm_config_parse__append_item(nm_config_parse__state_t *restrict state, nm_menu_item_t *const restrict it) {
    nm_config_t    *cfg_n    = calloc(1, sizeof(nm_config_t));
    nm_menu_item_t *cfg_it_n = calloc(1, sizeof(nm_menu_item_t));

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

    if (!cfg_it_n->lbl) {
        free(cfg_n);
        free(cfg_it_n);
        return NM_CONFIG_PARSE__APPEND__RET_ALLOC_ERROR;
    }

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

static nm_config_parse__append__ret_t nm_config_parse__append_action(nm_config_parse__state_t *restrict state, nm_menu_action_t *const restrict act) {
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

    if (!cfg_it_act_n->arg) {
        free(cfg_it_act_n);
        return NM_CONFIG_PARSE__APPEND__RET_ALLOC_ERROR;
    }

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
        free(cfg_n);
        free(cfg_gn_n);
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

bool nm_config_generate(nm_config_t *cfg, bool force_update) {
    bool changed = false;

    NM_LOG("config: running generators");
    for (nm_config_t *cur = cfg; cur; cur = cur->next) {
        if (cur->type == NM_CONFIG_TYPE_GENERATOR) {
            NM_LOG("config: running generator %s:%s", cur->value.generator->desc, cur->value.generator->arg);

            if (force_update)
                cur->value.generator->time = (struct timespec){0, 0};

            size_t sz;
            nm_menu_item_t **items = nm_generator_do(cur->value.generator, &sz);
            if (!items) {
                NM_LOG("config: ... no new items generated");
                if (force_update)
                    NM_LOG("config: ... possible bug: no items were generated even with force_update");
                continue;
            }

            NM_LOG("config: ... %zu items generated, removing previously generated items and replacing with new ones", sz);

            changed = true;

            // remove all generated items immediately after the generator
            for (nm_config_t *t_prev = cur, *t_cur = cur->next; t_cur && t_cur->generated; t_cur = t_cur->next) {
                t_prev->next = t_cur->next; // skip the generated item
                t_cur->next = NULL; // so we only free that item
                nm_config_free(t_cur);
                t_cur = t_prev; // continue with the new current item
            }

            // add the new ones
            for (ssize_t i = sz-1; i >= 0; i--) {
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

    return changed;
}

nm_menu_item_t **nm_config_get_menu(nm_config_t *cfg, size_t *n_out) {
    *n_out = 0;
    for (nm_config_t *cur = cfg; cur; cur = cur->next)
        if (cur->type == NM_CONFIG_TYPE_MENU_ITEM)
            (*n_out)++;

    nm_menu_item_t **it = calloc(*n_out, sizeof(nm_menu_item_t*));
    if (!it)
        return NULL;

    nm_menu_item_t **tmp = it;
    for (nm_config_t *cur = cfg; cur; cur = cur->next)
        if (cur->type == NM_CONFIG_TYPE_MENU_ITEM)
           *(tmp++) = cur->value.menu_item;

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
