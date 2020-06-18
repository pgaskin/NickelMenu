#ifndef NM_MENU_H
#define NM_MENU_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "action.h"

typedef enum {
    NM_MENU_LOCATION_MAIN_MENU   = 1,
    NM_MENU_LOCATION_READER_MENU = 2,
} nm_menu_location_t;

typedef struct nm_menu_action_t {
    char *arg;
    bool on_success;
    bool on_failure;
    nm_action_fn_t act; // can block, must return 0 on success, nonzero with out_err set to the malloc'd error message on error
    struct nm_menu_action_t *next;
} nm_menu_action_t;

typedef struct {
    nm_menu_location_t loc;
    char *lbl;
    nm_menu_action_t *action;
} nm_menu_item_t;

// nm_menu_hook hooks a dlopen'd libnickel handle. It MUST NOT be called more
// than once.
int nm_menu_hook(void *libnickel, char **err_out);

#ifdef __cplusplus
}
#endif
#endif