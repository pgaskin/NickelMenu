#define _GNU_SOURCE // asprintf

#include "action_c.h"
#include "util.h"

int nmi_action_dbgsyslog(const char *arg, char **err_out) {
    #define NMI_ERR_RET 1
    NMI_LOG("dbgsyslog: %s", arg);
    NMI_RETURN_OK(0);
    #undef NMI_ERR_RET
}

int nmi_action_dbgerror(const char *arg, char **err_out) {
    #define NMI_ERR_RET 1
    NMI_RETURN_ERR("%s", arg);
    #undef NMI_ERR_RET
}

int nmi_action_kfmon(const char *arg, char **err_out) {
    #define NMI_ERR_RET 1
    NMI_RETURN_ERR("not implemented yet (arg=%s)", arg); // TODO
    #undef NMI_ERR_RET
}
