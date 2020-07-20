#ifndef NM_INIT_H
#define NM_INIT_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "menu.h"

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
