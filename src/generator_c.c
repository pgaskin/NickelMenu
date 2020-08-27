#define _GNU_SOURCE // asprintf
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "generator.h"
#include "kfmon.h"
#include "nickelmenu.h"
#include "util.h"

NM_GENERATOR_(_test) {
    if (time_in_out->tv_sec || time_in_out->tv_nsec) {
        nm_err_set(NULL);
        return NULL; // updates not supported (or needed, for that matter)
    }

    char *tmp;
    long n = strtol(arg, &tmp, 10);
    NM_CHECK(NULL, *arg && !*tmp && n >= 0 && n <= 10, "invalid count '%s': must be an integer from 1-10", arg);

    if (n == 0) {
        *sz_out = 0;
        nm_err_set(NULL);
        return NULL;
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

    clock_gettime(CLOCK_REALTIME, time_in_out); // note: any nonzero value would work, but this generator is for testing and as an example

    *sz_out = n;
    nm_err_set(NULL);
    return items;
}

NM_GENERATOR_(_test_time) {
    if (arg && *arg)
        NM_ERR_RET(NULL, "_test_time does not accept any arguments");

    // note: this used as an example and for testing

    NM_LOG("_test_time: checking for updates");

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm lt;
    localtime_r(&ts.tv_sec, &lt);

    if (time_in_out->tv_sec && ts.tv_sec - time_in_out->tv_sec < 10) {
        NM_LOG("_test_time: last update is nonzero and last update time is < 10s, skipping");
        nm_err_set(NULL);
        return NULL;
    }

    NM_LOG("_test_time: updating");

    // note: you'd usually do the slower logic here

    nm_menu_item_t **items = calloc(1, sizeof(nm_menu_item_t*));
    items[0] = calloc(1, sizeof(nm_menu_item_t));
    asprintf(&items[0]->lbl, "%d:%02d:%02d", lt.tm_hour, lt.tm_min, lt.tm_sec);
    items[0]->action = calloc(1, sizeof(nm_menu_action_t));
    items[0]->action->act = NM_ACTION(dbg_msg);
    items[0]->action->arg = strdup("It worked!");
    items[0]->action->on_failure = true;
    items[0]->action->on_success = true;

    time_in_out->tv_sec = ts.tv_sec;

    *sz_out = 1;
    nm_err_set(NULL);
    return items;
}

NM_GENERATOR_(kfmon) {
    struct stat sb;
    if (stat(KFMON_IPC_SOCKET, &sb))
        NM_ERR_RET(NULL, "error checking '%s': stat: %m", KFMON_IPC_SOCKET);

    if (time_in_out->tv_sec == sb.st_mtim.tv_sec && time_in_out->tv_nsec == sb.st_mtim.tv_nsec) {
        nm_err_set(NULL);
        return NULL;
    }

    // Default with no arg or an empty arg is to request a gui-listing
    const char *kfmon_cmd = NULL;
    if (!arg || !*arg || !strcmp(arg, "gui")) {
        kfmon_cmd = "gui-list";
    } else if (!strcmp(arg, "all")) {
        kfmon_cmd = "list";
    } else {
        NM_ERR_RET(NULL, "invalid argument '%s': if specified, must be either gui or all", arg);
    }

    // We'll want to retrieve our watch list in there.
    kfmon_watch_list_t list = { 0 };
    int status = nm_kfmon_list_request(kfmon_cmd, &list);

    // If there was an error, handle it now.
    if (nm_kfmon_error_handler(status))
        return NULL; // the error will be passed on

    // Handle an empty listing safely
    if (list.count == 0) {
        *sz_out = 0;
        nm_err_set(NULL);
        return NULL;
    }

    // And now we can start populating an array of nm_menu_item_t :)
    *sz_out = list.count;
    nm_menu_item_t **items = calloc(list.count, sizeof(nm_menu_item_t*));

    // Walk the list to populate the items array
    size_t i = 0;
    for (kfmon_watch_node_t *node = list.head; node != NULL; node = node->next) {
        items[i] = calloc(1, sizeof(nm_menu_item_t));
        items[i]->action = calloc(1, sizeof(nm_menu_action_t));
        items[i]->lbl = strdup(node->watch.label);
        items[i]->action->act = NM_ACTION(kfmon);
        items[i]->action->arg = strdup(node->watch.filename);
        items[i]->action->on_failure = true;
        items[i]->action->on_success = true;
        i++;
    }

    // Destroy the list now that we've dumped it into an array of nm_menu_item_t
    kfmon_teardown_list(&list);

    *time_in_out = sb.st_mtim;
    nm_err_set(NULL);
    return items;
}
