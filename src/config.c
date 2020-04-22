#define _GNU_SOURCE
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "menu.h"
#include "util.h"
#include "subsys_c.h"
#include "subsys_cc.h"

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

nmi_config_t *nmi_config_parse(char **err_out) {
    #define NMI_ERR_RET NULL

    NMI_RETURN_ERR("not implemented");
    #undef NMI_ERR_RET
}

nmi_menu_item_t **nmi_config_get_menu(nmi_config_t *cfg, size_t *n_out) {
    *n_out = 0;
    for (nmi_config_t *cur = cfg; cur; cur = cfg->next)
        if (cur->type == NMI_CONFIG_TYPE_MENU_ITEM)
            (*n_out)++;

    nmi_menu_item_t **it = calloc(*n_out, sizeof(nmi_menu_item_t*));
    if (!it)
        return NULL;

    nmi_menu_item_t **tmp = it;
    for (nmi_config_t *cur = cfg; cur; cur = cfg->next)
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
