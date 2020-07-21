#ifndef NM_CONFIG_H
#define NM_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "menu.h"

#ifndef NM_CONFIG_DIR
#define NM_CONFIG_DIR "/mnt/onboard/.adds/nm"
#endif

#ifndef NM_CONFIG_MAX_MENU_ITEMS_PER_MENU
#define NM_CONFIG_MAX_MENU_ITEMS_PER_MENU 50
#endif

typedef struct nm_config_t nm_config_t;

typedef struct nm_config_file_t nm_config_file_t;

// nm_config_parse lists the configuration files in /mnt/onboard/.adds/nm. If
// there are errors reading the dir, NULL is returned and nm_err is set.
nm_config_file_t *nm_config_files();

// nm_config_files_update checks if the configuration files are up to date and
// updates them. If the files are already up-to-date, 1 is returned. If the
// files were updated, 0 is returned. If an error occurs, the pointer is left
// untouched and -1 is returned with nm_err set. Warning: if the files have
// changed, the pointer passed to files will become invalid (it gets replaced).
// If *files is NULL, it is equivalent to doing `*files = nm_config_files()`.
int nm_config_files_update(nm_config_file_t **files);

// nm_config_files_free frees the list of configuration files.
void nm_config_files_free(nm_config_file_t *files);

// nm_config_parse parses the configuration files. If there are syntax errors,
// file access errors, or invalid action names for menu_item, then NULL is
// returned and nm_err is set. On success, the config is returned.
nm_config_t *nm_config_parse(nm_config_file_t *files);

// nm_config_generate runs all generators synchronously and sequentially. Any
// previously generated items are automatically removed if updates are required.
// If the config was modified, true is returned.
bool nm_config_generate(nm_config_t *cfg, bool force_update);

// nm_config_get_menu gets a malloc'd array of pointers to the menu items
// defined in the config. These pointers will be valid until nm_config_free is
// called.
nm_menu_item_t **nm_config_get_menu(nm_config_t *cfg, size_t *n_out);

// nm_config_free frees all allocated memory.
void nm_config_free(nm_config_t *cfg);

#ifdef __cplusplus
}
#endif
#endif
