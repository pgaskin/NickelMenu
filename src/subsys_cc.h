#ifndef NMI_SUBSYS_CC_H
#define NMI_SUBSYS_CC_H
#ifdef __cplusplus
extern "C" {
#endif

int nmi_subsys_dbgsyslog(const char *arg, char **err_out);
int nmi_subsys_dbgerror(const char *arg, char **err_out);

#ifdef __cplusplus
}
#endif
#endif
