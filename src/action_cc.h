#ifndef NM_ACTION_CC_H
#define NM_ACTION_CC_H
#ifdef __cplusplus
extern "C" {
#endif

int nm_action_nickelsetting(const char *arg, char **err_out);
int nm_action_nickelextras(const char *arg, char **err_out);
int nm_action_nickelmisc(const char *arg, char **err_out);

#ifdef __cplusplus
}
#endif
#endif
