#ifndef NMI_ACTION_CC_H
#define NMI_ACTION_CC_H
#ifdef __cplusplus
extern "C" {
#endif

int nmi_action_nickelsetting(const char *arg, char **err_out);
int nmi_action_nickelextras(const char *arg, char **err_out);
int nmi_action_nickelmisc(const char *arg, char **err_out);

#ifdef __cplusplus
}
#endif
#endif
