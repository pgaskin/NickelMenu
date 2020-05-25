#ifndef NM_GENERATOR_H
#define NM_GENERATOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "menu.h"

// nm_generator_fn_t generates menu items. It must return a malloc'd array of
// pointers to malloc'd nm_menu_item_t's, and write the number of items to
// out_sz. The menu item locations must not be set. On error, err_out must be
// set if provided, NULL must be returned, and sz_out is undefined. If no
// entries are generated, NULL must be returned with sz_out set to 0. All
// strings should also be malloc'd.
typedef nm_menu_item_t **(*nm_generator_fn_t)(const char *arg, size_t *sz_out, char **err_out);

typedef struct {
    char *desc; // only used for making the errors more meaningful (it is the title)
    char *arg;
    nm_menu_location_t loc;
    nm_generator_fn_t generate; // should be as quick as possible with a short timeout, as it will block startup
} nm_generator_t;

// nm_generator_do runs a generator and returns the generated items, if any, or
// an item which shows the error returned by the generator.
nm_menu_item_t **nm_generator_do(nm_generator_t *gen, size_t *sz_out);

#define NM_GENERATOR(name) nm_generator_##name

#ifdef __cplusplus
#define NM_GENERATOR_(name) extern "C" nm_menu_item_t **NM_GENERATOR(name)(const char *arg, size_t *sz_out, char **err_out)
#else
#define NM_GENERATOR_(name) nm_menu_item_t **NM_GENERATOR(name)(const char *arg, size_t *sz_out, char **err_out)
#endif

#define NM_GENERATORS \
    X(_test)

#define X(name) NM_GENERATOR_(name);
NM_GENERATORS
#undef X

#ifdef __cplusplus
}
#endif
#endif
