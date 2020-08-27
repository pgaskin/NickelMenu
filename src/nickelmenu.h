#ifndef NM_H
#define NM_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "action.h"

typedef enum {
    NM_MENU_LOCATION_MAIN_MENU    = 1,
    NM_MENU_LOCATION_READER_MENU  = 2,
    NM_MENU_LOCATION_BROWSER_MENU = 3,
    NM_MENU_LOCATION_LIBRARY_MENU = 4,
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

#ifdef __cplusplus
}
#endif
#endif
