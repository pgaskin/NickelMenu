#ifndef NM_H
#define NM_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "action.h"

#define NM_MENU_LOCATION(name) NM_MENU_LOCATION_##name

#define NM_MENU_LOCATIONS \
    X(main)               \
    X(reader)             \
    X(browser)            \
    X(library)            \
    X(selection)          \
    X(selection_search)

typedef enum {
    NM_MENU_LOCATION_NONE = 0, // to allow it to be checked with if
    #define X(name) \
    NM_MENU_LOCATION(name),
    NM_MENU_LOCATIONS
    #undef X
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
