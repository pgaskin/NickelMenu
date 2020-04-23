#define _GNU_SOURCE // asprintf

#include "action_c.h"
#include "util.h"

int nm_action_dbgsyslog(const char *arg, char **err_out) {
    #define NM_ERR_RET 1
    NM_LOG("dbgsyslog: %s", arg);
    NM_RETURN_OK(0);
    #undef NM_ERR_RET
}

int nm_action_dbgerror(const char *arg, char **err_out) {
    #define NM_ERR_RET 1
    NM_RETURN_ERR("%s", arg);
    #undef NM_ERR_RET
}

int nm_action_kfmon(const char *arg, char **err_out) {
    #define NM_ERR_RET 1
    NM_RETURN_ERR("not implemented yet (arg=%s)", arg); // TODO
    #undef NM_ERR_RET
}
