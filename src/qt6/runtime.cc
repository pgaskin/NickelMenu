#include "runtime.h"
#include "navigation.h"
#include "config.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace nm {
namespace {
Hook hook{};
const char *log_path = nullptr;
unsigned entered = 0;
bool log(const char *message) {
    if (!log_path) return false;
    int fd = open(log_path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0644);
    if (fd < 0) return false;
    size_t done = 0, length = strlen(message);
    while (done < length) {
        ssize_t n = write(fd, message + done, length - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        done += static_cast<size_t>(n);
    }
    const bool okay = fsync(fd) == 0 && done == length;
    close(fd);
    return okay;
}
void report(const char *message) { log(message); log("\n"); }

// ARM C++ ABI C1 constructors return `this`, unlike the generic Itanium ABI.
// Preserve that return value and both opaque arguments. Only after successful
// restoration and original return do we install public QWidget-based UI work.
// No private fields are accessed; Nickel's exceptions are not caught.
void *wrapper(void *self, void *parent) {
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    using Constructor = void *(*)(void *, void *);
    const auto original = reinterpret_cast<Constructor>(hook.original);
    const bool first = __atomic_exchange_n(&entered, 1, __ATOMIC_ACQ_REL) == 0;
    bool restored = false;
    if (first) {
        log("ENTER MainNavView C1; forwarding unchanged opaque arguments.\n");
        const char *error = restore(hook);
        if (error) report(error);
        else {
            restored = true;
            log("RESTORED original GOT destination and read-only page protection.\n");
        }
    }
    void *result = original(self, parent); // exactly once, even if restoration fails
    if (first) {
        log("RETURN MainNavView C1; original constructor returned normally.\n");
        if (restored && result == self) monitor_menu(self, report);
        else log("REFUSED: no UI insertion after failed restoration or unexpected return.\n");
    }
    return result;
}
}

void start(const Spec &spec, const Paths &paths) {
    log_path = paths.log;
    char executable[512];
    const ssize_t n = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (n < 0 || static_cast<size_t>(n) == sizeof(executable) - 1) {
        log("REFUSED: cannot identify loading process; NO hook installed.\n");
        return;
    }
    executable[n] = 0;
    log("PROCESS "); report(executable);
    if (strcmp(executable, paths.process) != 0) {
        log("REFUSED: not the expected Nickel process; NO hook installed.\n");
        return;
    }
    struct stat st{};
    if (lstat(NM_CONFIG_DIR "/uninstall", &st) == 0) {
        log("DISABLED by configuration uninstall flag; NO hook installed.\n");
        return;
    }
    // The image plugin loader is free to release its explicit handle after
    // discovery. Keep our restored-hook callback and event filter resident for
    // the life of this Nickel process while leaving the library active on disk
    // for subsequent boots.
    void *self = dlopen(paths.active, RTLD_NOW | RTLD_NOLOAD | RTLD_NODELETE);
    if (!self) {
        log("REFUSED: persistent residency pin failed; NO hook installed.\n");
        return;
    }
    dlclose(self); // RTLD_NODELETE retains the mapping until process exit
    if (!log("PINNED persistent backend for this process; active library retained on disk.\n")) return;
    const char *error = prepare(spec, reinterpret_cast<uintptr_t>(&wrapper), hook);
    if (error) { report(error); return; }
    char details[256];
    snprintf(details, sizeof(details),
             "VALIDATED Thumb destination=%p symbol-offset=%p GOT=%p slot-offset=%p protection=read-only build=%s\n",
             reinterpret_cast<void *>(hook.original), reinterpret_cast<void *>(hook.symbol_value),
             static_cast<void *>(hook.slot), reinterpret_cast<void *>(hook.slot_offset), hook.build_id);
    if (!log(details)) return;
    // Do not add constructors/destructors or dynamic initialization to this
    // state: this function runs in the DSO's ELF initialization sequence.
    error = install(hook);
    if (error) report(error);
    else log("INSTALLED one-shot MainNavView C1 hook; later views use public Qt monitoring.\n");
}
}
