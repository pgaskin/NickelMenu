#ifndef NM_ACTION_C_H
#define NM_ACTION_C_H
#ifdef __cplusplus
extern "C" {
#endif

int nm_action_dbgsyslog(const char *arg, char **err_out);
int nm_action_dbgerror(const char *arg, char **err_out);
int nm_action_kfmon(const char *arg, char **err_out);

#ifdef __cplusplus
}
#endif
#endif
