#ifndef NM_MENU_H
#define NM_MENU_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "action.h"

typedef enum {
    NM_MENU_LOCATION_MAIN_MENU   = 1,
    NM_MENU_LOCATION_READER_MENU = 2,
} nm_menu_location_t;

typedef struct nm_menu_action_t {
    char *arg;
    nm_action_fn_t act; // can block, must return 0 on success, nonzero with out_err set to the malloc'd error message on error
    struct nm_menu_action_t *next;
} nm_menu_action_t;

typedef struct {
    nm_menu_location_t loc;
    char *lbl;
    nm_menu_action_t *action;
} nm_menu_item_t;

// nm_menu_hook hooks a dlopen'd libnickel handle to add the specified menus,
// and returns 0 on success, or 1 with err_out (if provided) set to the malloc'd
// error message on error. The provided configuration and all pointers it
// references must remain valid for the lifetime of the program (i.e. not stack
// allocated). It MUST NOT be called more than once.
int nm_menu_hook(void *libnickel, nm_menu_item_t **items, size_t items_n, char **err_out);

#ifdef __cplusplus
}
#endif
#endif