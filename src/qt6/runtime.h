#pragma once
#include "hook.h"
namespace nm {
struct Paths {
    const char *active;
    const char *disabled;
    const char *active_dir;
    const char *disabled_dir;
    const char *log;
    const char *process;
};
// All strings must have static lifetime. Call once, during plugin loading.
void start(const Spec &, const Paths &);
}
