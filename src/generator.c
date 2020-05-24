#define _GNU_SOURCE // asprintf
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#include "action.h"
#include "generator.h"
#include "menu.h"
#include "util.h"

nm_menu_item_t **nm_generator_do(nm_generator_t *gen, size_t *sz_out) {
    NM_LOG("generator: running generator (%s) (%s) (%d) (%p)", gen->desc, gen->arg, gen->loc, gen->generate);

    char *err;
    size_t sz;
    nm_menu_item_t **items = gen->generate(gen->arg, &sz, &err);

    if (err) {
        if (items)
            NM_LOG("generator: warning: items should be null on error");

        NM_LOG("generator: generator error (%s) (%s), replacing with error item: %s", gen->desc, gen->arg, err);
        sz = 1;
        items = calloc(sz, sizeof(nm_menu_item_t*));
        items[0] = calloc(1, sizeof(nm_menu_item_t));
        // loc will be set below
        items[0]->lbl = strdup("Generator error");
        items[0]->action = calloc(1, sizeof(nm_menu_action_t));
        items[0]->action->act = NM_ACTION(dbg_msg);
        asprintf(&items[0]->action->arg, "%s: %s", gen->desc, err);
        items[0]->action->on_failure = true;
        items[0]->action->on_success = true;
        free(err);
    }

    if (items) {
        if (!sz)
            NM_LOG("generator: warning: items should be null when size is 0");

        for (size_t i = 0; i < sz; i++) {
            if (items[i]->loc)
                NM_LOG("generator: warning: generator should not set the menu item location, as it will be overridden");

            items[i]->loc = gen->loc;
        }
    }

    *sz_out = sz;
    return items;
}
