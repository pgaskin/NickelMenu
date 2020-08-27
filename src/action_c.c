#include <limits.h>

#include "action.h"
#include "kfmon.h"
#include "util.h"

NM_ACTION_(dbg_syslog) {
    NM_LOG("dbgsyslog: %s", arg);
    return nm_action_result_silent();
}

NM_ACTION_(dbg_error) {
    NM_ERR_SET("%s", arg);
    return NULL;
}

NM_ACTION_(dbg_msg) {
    return nm_action_result_msg("%s", arg);
}

NM_ACTION_(dbg_toast) {
    return nm_action_result_toast("%s", arg);
}

NM_ACTION_(skip) {
    char *tmp;
    long n = strtol(arg, &tmp, 10);
    NM_CHECK(NULL, *arg && !*tmp && n != 0 && n >= -1 && n < INT_MAX, "invalid count '%s': must be a nonzero integer or -1", arg);

    nm_action_result_t *res = calloc(1, sizeof(nm_action_result_t));
    res->type = NM_ACTION_RESULT_TYPE_SKIP;
    res->skip = (int)(n);
    return res;
}

NM_ACTION_(kfmon_id) {
    // Start by watch ID (simpler, but IDs may not be stable across a single power cycle, given severe KFMon config shuffling)
    int status = nm_kfmon_simple_request("start", arg);
    return nm_kfmon_return_handler(status);
}

NM_ACTION_(kfmon) {
    // Trigger a watch, given its trigger basename. Stable runtime lookup done by KFMon.
    int status = nm_kfmon_simple_request("trigger", arg);

    // Fixup INVALID_ID to INVALID_NAME for slightly clearer feedback (see e8b2588 for details).
    if (status == KFMON_IPC_ERR_INVALID_ID)
        status = KFMON_IPC_ERR_INVALID_NAME;

    return nm_kfmon_return_handler(status);
}
