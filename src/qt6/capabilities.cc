#include "action.h"
#include "util.h"

// Do not silently call a surviving symbol with an unverified private ABI.
#define UNAVAILABLE(name) NM_ACTION_(name) { \
    (void)arg; \
    NM_ERR_RET(nullptr, #name " is unavailable in this experimental Qt 6 build; see the compatibility matrix"); \
}
UNAVAILABLE(nickel_setting)
UNAVAILABLE(nickel_extras)
UNAVAILABLE(nickel_browser)
UNAVAILABLE(nickel_misc)
UNAVAILABLE(nickel_open)
UNAVAILABLE(nickel_wifi)
UNAVAILABLE(nickel_bluetooth)
UNAVAILABLE(nickel_orientation)
UNAVAILABLE(nickel_screenshot)
UNAVAILABLE(power)
UNAVAILABLE(kfmon)
UNAVAILABLE(kfmon_id)

NM_ACTION_(uninstall) {
    (void)arg;
    return nm_action_result_msg("This test build already disabled itself on disk. "
        "Restart to stop it; use its matching removal update.tar to remove the disabled library. "
        "Your configuration will be retained.");
}
