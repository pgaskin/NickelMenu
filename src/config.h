#ifndef NMI_CONFIG_H
#define NMI_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "menu.h"

typedef struct nmi_config_t nmi_config_t;

// nmi_config_parse parses the configuration files in /mnt/onboard/.adds/nmi.
// An error is returned if there are syntax errors, file access errors, or
// invalid action names for menu_item.
nmi_config_t *nmi_config_parse(char **err_out);

// nmi_config_get_menu gets a malloc'd array of pointers to the menu items
// defined in the config. These pointers will be valid until nmi_config_free is
// called.
nmi_menu_item_t **nmi_config_get_menu(nmi_config_t *cfg, size_t *n_out);

// nmi_config_free frees all allocated memory.
void nmi_config_free(nmi_config_t *cfg);

#ifdef __cplusplus
}
#endif
#endif
