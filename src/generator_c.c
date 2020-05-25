#define _GNU_SOURCE // asprintf
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "generator.h"
#include "menu.h"
#include "util.h"

NM_GENERATOR_(_test) {
    #define NM_ERR_RET NULL

    char *tmp;
    long n = strtol(arg, &tmp, 10);
    NM_ASSERT(*arg && !*tmp && n >= 0 && n <= 10, "invalid count '%s': must be an integer from 1-10", arg);

    if (n == 0) {
        *sz_out = 0;
        NM_RETURN_OK(NULL);
    }

    nm_menu_item_t **items = calloc(n, sizeof(nm_menu_item_t*));
    for (size_t i = 0; i < (size_t)(n); i++) {
        items[i] = calloc(1, sizeof(nm_menu_item_t));
        items[i]->action = calloc(1, sizeof(nm_menu_action_t));
        asprintf(&items[i]->lbl, "Generated %zu", i+1);
        items[i]->action->act = NM_ACTION(dbg_msg);
        items[i]->action->arg = strdup("Pressed");
        items[i]->action->on_failure = true;
        items[i]->action->on_success = true;
    }

    *sz_out = n;
    NM_RETURN_OK(items);

    #undef NM_ERR_RET
}
