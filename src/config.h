#ifndef NM_CONFIG_H
#define NM_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "menu.h"
#include "visibility.h"

#ifndef NM_CONFIG_DIR
#define NM_CONFIG_DIR "/mnt/onboard/.adds/nm"
#endif

#ifndef NM_CONFIG_MAX_MENU_ITEMS_PER_MENU
#define NM_CONFIG_MAX_MENU_ITEMS_PER_MENU 50
#endif

typedef struct nm_config_t nm_config_t;

// nm_config_parse parses the configuration files in /mnt/onboard/.adds/nm.
// An error is returned if there are syntax errors, file access errors, or
// invalid action names for menu_item.
NM_PRIVATE nm_config_t *nm_config_parse(char **err_out);

// nm_config_generate runs all generators synchronously and sequentially. Any
// previously generated items are automatically removed.
NM_PRIVATE void nm_config_generate(nm_config_t *cfg);

// nm_config_get_menu gets a malloc'd array of pointers to the menu items
// defined in the config. These pointers will be valid until nm_config_free is
// called.
NM_PRIVATE nm_menu_item_t **nm_config_get_menu(nm_config_t *cfg, size_t *n_out);

// nm_config_free frees all allocated memory.
NM_PRIVATE void nm_config_free(nm_config_t *cfg);

#ifdef __cplusplus
}
#endif
#endif
