#ifndef NM_ACTION_H
#define NM_ACTION_H
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NM_ACTION_RESULT_TYPE_SILENT = 0,
    NM_ACTION_RESULT_TYPE_MSG    = 1,
    NM_ACTION_RESULT_TYPE_TOAST  = 2,
} nm_action_result_type_t;

typedef struct {
    nm_action_result_type_t type;
    char *msg;
} nm_action_result_t;

typedef nm_action_result_t *(*nm_action_fn_t)(const char *arg, char **err);

nm_action_result_t *nm_action_result_silent();
nm_action_result_t *nm_action_result_msg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
nm_action_result_t *nm_action_result_toast(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void nm_action_result_free(nm_action_result_t *res);

#define NM_ACTION(name) nm_action_##name

#ifdef __cplusplus
#define NM_ACTION_(name) extern "C" nm_action_result_t *NM_ACTION(name)(const char *arg, char **err_out)
#else
#define NM_ACTION_(name) nm_action_result_t *NM_ACTION(name)(const char *arg, char **err_out)
#endif

#define NM_ACTIONS    \
    X(dbg_syslog)     \
    X(dbg_error)      \
    X(dbg_msg)        \
    X(dbg_toast)      \
    X(kfmon)          \
    X(nickel_setting) \
    X(nickel_extras)  \
    X(nickel_misc)    \
    X(cmd_spawn)      \
    X(cmd_output)

#define X(name) NM_ACTION_(name);
NM_ACTIONS
#undef X

#ifdef __cplusplus
}
#endif
#endif