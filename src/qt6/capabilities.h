#pragma once
#include "action.h"
#include <functional>

namespace nm6 {
// Start the synthetic USB cycle. An immediate error has no callback; after a
// successful start, completion runs once on the GUI thread after unplugging.
// Direct action calls may omit it; the asynchronous executor waits for it so
// subsequent chain steps cannot race the temporary USB view/database closure.
nm_action_result_t *rescan_books_full(std::function<void(bool)> completion = {});
}
