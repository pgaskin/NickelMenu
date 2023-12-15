#ifndef NM_ACTION_H
#define NM_ACTION_H
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NM_ACTION_RESULT_TYPE_SILENT = 0,
    NM_ACTION_RESULT_TYPE_MSG    = 1,
    NM_ACTION_RESULT_TYPE_TOAST  = 2,
    NM_ACTION_RESULT_TYPE_SKIP   = 3, // for use by skip only
} nm_action_result_type_t;

typedef struct {
    nm_action_result_type_t type;
    char *msg;
    int skip; // for use by skip only
} nm_action_result_t;

// nm_action_fn_t represents an action. On success, a nm_action_result_t is
// returned and needs to be freed with nm_action_result_free. Otherwise, NULL is
// returned and nm_err is set.
typedef nm_action_result_t *(*nm_action_fn_t)(const char *arg);

nm_action_result_t *nm_action_result_silent();
nm_action_result_t *nm_action_result_msg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
nm_action_result_t *nm_action_result_toast(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void nm_action_result_free(nm_action_result_t *res);

#define NM_ACTION(name) nm_action_##name

#ifdef __cplusplus
#define NM_ACTION_(name) extern "C" nm_action_result_t *NM_ACTION(name)(const char *arg)
#else
#define NM_ACTION_(name) nm_action_result_t *NM_ACTION(name)(const char *arg)
#endif

#define NM_ACTIONS        \
    X(cmd_spawn)          \
    X(cmd_output)         \
    X(dbg_syslog)         \
    X(dbg_error)          \
    X(dbg_msg)            \
    X(dbg_toast)          \
    X(kfmon)              \
    X(kfmon_id)           \
    X(nickel_setting)     \
    X(nickel_extras)      \
    X(nickel_browser)     \
    X(nickel_misc)        \
    X(nickel_open)        \
    X(nickel_wifi)        \
    X(nickel_bluetooth)   \
    X(nickel_orientation) \
    X(power)              \
    X(skip)

#define X(name) NM_ACTION_(name);
NM_ACTIONS
#undef X

#ifdef __cplusplus
}
#endif
#endif
