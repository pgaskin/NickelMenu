#include "NickelHook.h"
#include <cstdarg>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

extern "C" void nh_log(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsyslog(LOG_DEBUG, format, args);
    va_end(args);
    // Bound persistent debug output. Core logs can include config arguments;
    // users should review logs before sharing them.
    int fd = open("/mnt/onboard/.adds/nm-qt6/core.log", O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0644);
    if (fd < 0) return;
    struct stat st{};
    if (fstat(fd, &st) == 0 && st.st_size < 1024 * 1024) {
        char line[2048];
        va_start(args, format);
        vsnprintf(line, sizeof(line), format, args);
        va_end(args);
        dprintf(fd, "%s\n", line);
    }
    close(fd);
}
