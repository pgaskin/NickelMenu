#ifndef NM_CONFIG_H
#define NM_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "action.h"

typedef enum {
    NM_MENU_LOCATION_MAIN_MENU    = 1,
    NM_MENU_LOCATION_READER_MENU  = 2,
    NM_MENU_LOCATION_BROWSER_MENU = 3,
} nm_menu_location_t;

typedef struct nm_menu_action_t {
    char *arg;
    bool on_success;
    bool on_failure;
    nm_action_fn_t act; // can block, must return zero on success, nonzero with nm_err set on error
    struct nm_menu_action_t *next;
} nm_menu_action_t;

typedef struct {
    nm_menu_location_t loc;
    char *lbl;
    nm_menu_action_t *action;
} nm_menu_item_t;

#if !(defined(NM_CONFIG_DIR) && defined(NM_CONFIG_DIR_DISP))
#error "NM_CONFIG_DIR not set (it should be done by the Makefile)"
#endif

#ifndef NM_CONFIG_MAX_MENU_ITEMS_PER_MENU
#define NM_CONFIG_MAX_MENU_ITEMS_PER_MENU 50
#endif

typedef struct nm_config_t nm_config_t;

typedef struct nm_config_file_t nm_config_file_t;

// nm_config_parse lists the configuration files in NM_CONFIG_DIR. If there are
// errors reading the dir, NULL is returned and nm_err is set.
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

// nm_global_config_update updates and regenerates the config if needed. If the
// menu items changed (i.e. the old items aren't valid anymore), the revision
// will be incremented and returned (even if there was an error). On error,
// nm_err is set, and otherwise, it is cleared.
int nm_global_config_update();

// nm_global_config_items returns an array of pointers with the current menu
// items (the pointer and the items it points to will remain valid until the
// next time nm_global_config_update is called). The number of items is stored
// in the variable pointed to by n_out. If an error ocurred during the last time
// nm_global_config_update was called, it is returned as a "Config Error" menu
// item. If nm_global_config_update has never been called successfully before,
// NULL is returned and n_out is set to 0.
nm_menu_item_t **nm_global_config_items(size_t *n_out);

#ifdef __cplusplus
}
#endif
#endif
