#ifndef NH_H
#define NH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#ifndef NH_VERSION
#define NH_VERSION "dev"
#endif

#define NickelHook(...)                    \
    __attribute__((visibility("default"))) \
    struct nh NickelHook = (struct nh){    \
        __VA_ARGS__                        \
    };                                     \

#define nh_symptr(sym) *(void**)(&sym)
#define nh_symoutptr(sym) (void**)(&sym)

// nh represents a Nickel mod.
struct nh {
    int (*init)(); // optional, return nonzero value on error (it will restore hooks)
    struct nh_info  *info;
    struct nh_hook  *hook;  // pointer to the first element, NULL terminated
    struct nh_dlsym *dlsym; // pointer to the first element, NULL terminated
};

// nh_info contains information about the mod.
struct nh_info {
    const char *name; // must be unique

    // optional options
    const char *desc;            // default: none - human-readable description
    const char *uninstall_flag;  // default: none - path to flag which triggers an uninstall and deletes itself if it exists
    const char *uninstall_xflag; // default: none - path to flag which triggers an uninstall if it is deleted
    int        failsafe_delay;   // default: 0    - delay in seconds before disarming failsafe

    // TODO: maybe a mechanism to only load the latest version?
};

// nh_hook hooks a symbol for a specific library.
struct nh_hook {
    const char *sym;     // the symbol to hook
    const char *sym_new; // the symbol to replace it with
    const char *lib;     // the library to hook the symbol for
    void       **out;    // the variable to store the original symbol in, use nh_symoutptr for convenience

    // optional options
    const char *desc;    // default: none  - human-readable description
    bool       optional; // default: false - prevents a failure to hook from being treated as a fatal error
};

// nh_dlsym loads a symbol.
struct nh_dlsym {
    const char *name; // the name of the symbol to resolve with RTLD_DEFAULT (note that libnickel is guaranteed to be loaded at this point)
    void       **out; // the variable to store the resolved symbol in, use nh_symoutptr for convenience

    // optional options
    const char *desc;    // default: none  - human-readable description
    bool       optional; // default: false - prevents a failure to resolve the sym from being treated as a fatal error, and sets it to NULL instead
};

// nh_log logs a message with a prefix based on the mod name, and should be used
// for all logging. Messages larger than 256 bytes will be silently  truncated.
__attribute__((visibility("default"))) void nh_log(const char *fmt, ...) __attribute((format(printf, 1, 2)));

// nh_dump_log dumps the syslog to a file on the user storage in the format
// `/mnt/onboard/.kobo/{NickelHook.info->name ?: "NickelHook"}_YYYY-MM-DD_HH-MM-SS.log`.
// This is intended for debugging or for dumping the log after a failure. It
// will be automatically called by NickelHook if initialization fails.
__attribute__((visibility("default"))) void nh_dump_log();

#ifdef __cplusplus
}
#endif

#endif
