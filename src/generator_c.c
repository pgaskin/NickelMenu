#define _GNU_SOURCE // asprintf
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "generator.h"
#include "menu.h"
#include "kfmon.h"
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

NM_GENERATOR_(kfmon) {
    #define NM_ERR_RET NULL

    // Default with no arg or an empty arg is to request a gui-listing
    char *kfmon_cmd = NULL;
    if (!arg || !*arg || strcmp(arg, "gui") == 0) {
        kfmon_cmd = strdupa("gui-list");
    } else if (strcmp(arg, "all") == 0) {
        kfmon_cmd = strdupa("list");
    } else {
        NM_RETURN_ERR("invalid argument '%s': if specified, must be either gui or all", arg);
    }

    // We'll want to retrieve our watch list in there.
    kfmon_watch_list_t list = { 0 };
    int status = nm_kfmon_list_request(kfmon_cmd, &list);

    // If there was an error, handle it now.
    if (status != KFMON_IPC_OK) {
        return nm_kfmon_error_handler(status, err_out);
    }

    // Handle an empty listing safely
    if (list.count == 0) {
        *sz_out = 0;
        NM_RETURN_OK(NULL);
    }

    // And now we can start populating an array of nm_menu_item_t :)
    *sz_out = list.count;
    nm_menu_item_t **items = calloc(list.count, sizeof(nm_menu_item_t*));

    // Walk the list
    NM_LOG("List has %zu nodes", list.count);
    NM_LOG("Head is at %p", list.head);
    NM_LOG("Tail is at %p", list.tail);
    size_t i = 0;
    for (kfmon_watch_node_t* node = list.head; node != NULL; node = node->next) {
        NM_LOG("Dumping node at %p", node);
        NM_LOG("idx: %hhu // filename: %s // label: %s", node->watch.idx, node->watch.filename, node->watch.label);
        NM_LOG("Populating item %zu", i);
        nm_menu_item_t *item = items[i++] = calloc(1, sizeof(nm_menu_item_t));
        item->action = calloc(1, sizeof(nm_menu_action_t));
        item->lbl = strdup(node->watch.label);
        item->action->act = NM_ACTION(kfmon);
        item->action->arg = strdup(node->watch.filename);
        item->action->on_failure = true;
        item->action->on_success = true;
    }

    // Destroy the list now that we've dumped it into an array of nm_menu_item_t
    kfmon_teardown_list(&list);

    NM_RETURN_OK(items);

    #undef NM_ERR_RET
}
