#ifndef NM_GENERATOR_H
#define NM_GENERATOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <time.h>
#include "nickelmenu.h"

// nm_generator_fn_t generates menu items. It must return a malloc'd array of
// pointers to malloc'd nm_menu_item_t's, and write the number of items to
// out_sz. The menu item locations must not be set. On error, nm_err must be
// set, NULL must be returned, and sz_out is undefined. If no entries are
// generated, NULL must be returned with sz_out set to 0. All strings should
// also be malloc'd. On success, nm_err must be cleared.
//
// time_in_out will not be NULL, and contains zero or the last modification time
// for the generator. If it is zero, the generator should generate the items as
// usual and update the time. If it is nonzero, the generator should return NULL
// without making changes if the time is up to date (the check should be as
// quick as possible), and if not, it should update the items and update the
// time to match. If the generator does not have a way of checking for updates
// quickly, it should only update the item and set the time to a nonzero value
// if the time is zero, and return NULL if the time is nonzero. Note that this
// time doesn't have to account for different arguments or multiple instances,
// as changes in those will always cause the time to be set to zero.
typedef nm_menu_item_t **(*nm_generator_fn_t)(const char *arg, struct timespec *time_in_out, size_t *sz_out);

typedef struct {
    char *desc; // only used for making the errors more meaningful (it is the title)
    char *arg;
    nm_menu_location_t loc;
    nm_generator_fn_t generate; // should be as quick as possible with a short timeout, as it will block startup
    struct timespec time;
} nm_generator_t;

// nm_generator_do runs a generator and returns the generated items, if any, or
// an item which shows the error returned by the generator. If NULL is returned,
// no items needed to be updated (set time to zero to force an update) (sz_out
// is undefined).
nm_menu_item_t **nm_generator_do(nm_generator_t *gen, size_t *sz_out);

#define NM_GENERATOR(name) nm_generator_##name

#ifdef __cplusplus
#define NM_GENERATOR_(name) extern "C" nm_menu_item_t **NM_GENERATOR(name)(const char *arg, struct timespec *time_in_out, size_t *sz_out)
#else
#define NM_GENERATOR_(name) nm_menu_item_t **NM_GENERATOR(name)(const char *arg, struct timespec *time_in_out, size_t *sz_out)
#endif

#define NM_GENERATORS \
    X(_test)          \
    X(_test_time)     \
    X(kfmon)

#define X(name) NM_GENERATOR_(name);
NM_GENERATORS
#undef X

#ifdef __cplusplus
}
#endif
#endif
