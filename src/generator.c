#define _GNU_SOURCE // asprintf
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>

#include "action.h"
#include "generator.h"
#include "nickelmenu.h"
#include "util.h"

nm_menu_item_t **nm_generator_do(nm_generator_t *gen, size_t *sz_out) {
    NM_LOG("generator: running generator (%s) (%s) (%d) (%p)", gen->desc, gen->arg, gen->loc, gen->generate);

    struct timespec old = gen->time;
    size_t sz = (size_t)(-1); // this should always be set by generate upon success, but we'll initialize it just in case
    nm_menu_item_t **items = gen->generate(gen->arg, &gen->time, &sz);

    if (items && old.tv_sec == gen->time.tv_sec && old.tv_nsec == gen->time.tv_nsec)
        NM_LOG("generator: bug: new items were returned, but time wasn't changed");

    const char *err = nm_err();

    if (!old.tv_sec && !old.tv_nsec && !err && !items)
        NM_LOG("generator: warning: no existing items (time == 0), but no new items or error were returned");

    if (err) {
        if (items)
            NM_LOG("generator: bug: items should be null on error");

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
    }

    if (!err && !items && (old.tv_sec != gen->time.tv_sec || old.tv_nsec != gen->time.tv_nsec))
        NM_LOG("generator: bug: the time should have been updated if new items were returned");

    if (items) {
        if (sz == (size_t)(-1))
            NM_LOG("generator: bug: size should have been set by generate, but wasn't");
        if (!sz)
            NM_LOG("generator: bug: items should be null when size is 0");

        for (size_t i = 0; i < sz; i++) {
            if (items[i]->loc)
                NM_LOG("generator: bug: generator should not set the menu item location, as it will be overridden");

            items[i]->loc = gen->loc;
        }
    }

    *sz_out = sz;
    return items;
}
