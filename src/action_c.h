#ifndef NMI_ACTION_C_H
#define NMI_ACTION_C_H
#ifdef __cplusplus
extern "C" {
#endif

int nmi_action_dbgsyslog(const char *arg, char **err_out);
int nmi_action_dbgerror(const char *arg, char **err_out);
int nmi_action_kfmon(const char *arg, char **err_out);

#ifdef __cplusplus
}
#endif
#endif
