#ifndef NMI_MENU_H
#define NMI_MENU_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef enum {
    NMI_MENU_LOCATION_MAIN_MENU = 1,
    NMI_MENU_LOCATION_READER_MENU = 2,
} nmi_menu_location_t;

typedef struct {
    nmi_menu_location_t loc;
    const char *lbl;
    const char *arg;
    int (*execute)(const char *arg, char **out_err); // can block, must return 0 on success, nonzero with out_err set to the malloc'd error message on error
} nmi_menu_entry_t;

// nmi_menu_hook hooks a dlopen'd libnickel handle to add the specified menus,
// and returns 0 on success, or 1 with err_out (if provided) set to the malloc'd
// error message on error. The provided configuration and all pointers it
// references must remain valid for the lifetime of the program (i.e. not stack
// allocated). It MUST NOT be called more than once.
int nmi_menu_hook(void *libnickel, nmi_menu_entry_t *entries, size_t entries_n, char **err_out);

#ifdef __cplusplus
}
#endif
#endif