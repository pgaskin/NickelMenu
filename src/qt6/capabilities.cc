#include "action.h"
#include "util.h"
#include <cerrno>
#include <cstdio>
#include <cstring>

// Do not silently call a surviving symbol with an unverified private ABI.
#define UNAVAILABLE(name) NM_ACTION_(name) { \
    (void)arg; \
    NM_ERR_RET(nullptr, #name " is unavailable in this experimental Qt 6 build; see the compatibility matrix"); \
}
UNAVAILABLE(nickel_setting)
UNAVAILABLE(nickel_extras)
UNAVAILABLE(nickel_browser)
UNAVAILABLE(nickel_open)
UNAVAILABLE(nickel_wifi)
UNAVAILABLE(nickel_bluetooth)
UNAVAILABLE(nickel_orientation)
UNAVAILABLE(nickel_screenshot)
UNAVAILABLE(power)
UNAVAILABLE(kfmon)
UNAVAILABLE(kfmon_id)

NM_ACTION_(nickel_misc) {
    if (strcmp(arg, "force_usb_connection") != 0)
        NM_ERR_RET(nullptr, "nickel_misc action '%s' is unavailable in this experimental Qt 6 build", arg);

    // This is the same non-private interface used by the Qt 5 backend. Keep it
    // on the worker path: opening the hardware-status FIFO can block.
    FILE *status = fopen("/tmp/nickel-hardware-status", "w");
    NM_CHECK(nullptr, status, "could not open nickel hardware status pipe: %m");
    if (fputs("usb plug add", status) < 0) {
        const int error = errno;
        fclose(status);
        NM_ERR_RET(nullptr, "could not write USB event to nickel hardware status pipe: %s", strerror(error));
    }
    if (fclose(status) != 0)
        NM_ERR_RET(nullptr, "could not close nickel hardware status pipe: %m");
    return nm_action_result_silent();
}

NM_ACTION_(uninstall) {
    (void)arg;
    return nm_action_result_msg("This persistent test build cannot safely reboot Nickel from this action. "
        "Use its matching removal update.tar, or create .adds/nm/uninstall and restart to disable loading. "
        "Your configuration will be retained.");
}
