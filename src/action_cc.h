#ifndef NM_ACTION_CC_H
#define NM_ACTION_CC_H
#ifdef __cplusplus
extern "C" {
#endif

// This file contains actions which need access to Qt classes, but is still
// mostly C code.

int nm_action_nickelsetting(const char *arg, char **err_out);
int nm_action_nickelextras(const char *arg, char **err_out);
int nm_action_nickelmisc(const char *arg, char **err_out);
int nm_action_cmdspawn(const char *arg, char **err_out);
int nm_action_cmdoutput(const char *arg, char **err_out);

#ifdef __cplusplus
}
#endif
#endif
