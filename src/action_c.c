#define _GNU_SOURCE // asprintf
#include <limits.h>

#include "action.h"
#include "kfmon.h"
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

NM_ACTION_(skip) {
    #define NM_ERR_RET NULL

    char *tmp;
    long n = strtol(arg, &tmp, 10);
    NM_ASSERT(*arg && !*tmp && n != 0 && n >= -1 && n < INT_MAX, "invalid count '%s': must be a nonzero integer or -1", arg);

    nm_action_result_t *res = calloc(1, sizeof(nm_action_result_t));
    res->type = NM_ACTION_RESULT_TYPE_SKIP;
    res->skip = (int)(n);
    NM_RETURN_OK(res);

    #undef NM_ERR_RET
}

NM_ACTION_(kfmon_id) {
    // Start by watch ID (simpler, but IDs may not be stable across a single power cycle, given severe KFMon config shuffling)
    int status = nm_kfmon_simple_request("start", arg);
    return nm_kfmon_return_handler(status, err_out);
}

NM_ACTION_(kfmon) {
    // Trigger a watch, given its trigger basename. Stable runtime lookup done by KFMon.
    int status = nm_kfmon_simple_request("trigger", arg);

    // Fixup INVALID_ID to INVALID_NAME for slightly clearer feedback (see e8b2588 for details).
    if (status == KFMON_IPC_ERR_INVALID_ID) {
        status = KFMON_IPC_ERR_INVALID_NAME;
    }

    return nm_kfmon_return_handler(status, err_out);
}
