#define _GNU_SOURCE // asprintf

#include "action.h"
#include "util.h"

NM_ACTION_(dbg_syslog) {
    #define NM_ERR_RET NULL
    NM_LOG("dbgsyslog: %s", arg);
    NM_RETURN_OK(nm_action_result_silent());
    #undef NM_ERR_RET
}

NM_ACTION_(dbg_error) {
    #define NM_ERR_RET NULL
    NM_RETURN_ERR("%s", arg);
    #undef NM_ERR_RET
}

NM_ACTION_(dbg_msg) {
    #define NM_ERR_RET NULL
    NM_RETURN_OK(nm_action_result_msg("%s", arg));
    #undef NM_ERR_RET
}

NM_ACTION_(dbg_toast) {
    #define NM_ERR_RET NULL
    NM_RETURN_OK(nm_action_result_toast("%s", arg));
    #undef NM_ERR_RET
}

NM_ACTION_(kfmon) {
    #define NM_ERR_RET NULL
    NM_RETURN_ERR("not implemented yet (arg=%s)", arg); // TODO
    #undef NM_ERR_RET
}
