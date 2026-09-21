#include "plugin.h"
#include "runtime.h"

__attribute__((constructor)) static void start_probe() {
    static constexpr nm::Spec spec{
        "/usr/local/Kobo/libnickel.so.1.0.0",
        "5bfd31d75408ae3826eb1e9b58b90156d495fd28",
        "_ZN11MainNavViewC1EP7QWidget", 0x01603ab5, 0x02ba2e2c};
    static constexpr nm::Paths paths{
        "/usr/local/Kobo/imageformats/libnmqt6.so",
        "/usr/local/Kobo/nm-qt6/libnmqt6.disabled",
        "/usr/local/Kobo/imageformats", "/usr/local/Kobo/nm-qt6",
        "/mnt/onboard/.adds/nm-qt6/probe.log", "/usr/local/Kobo/nickel"};
    nm::start(spec, paths);
}
