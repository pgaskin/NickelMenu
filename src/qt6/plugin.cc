#include "plugin.h"
#include "runtime.h"

__attribute__((constructor)) static void start_backend() {
    static constexpr nm::Spec spec{
        "/usr/local/Kobo/libnickel.so.1.0.0",
        nullptr, "_ZN11MainNavViewC1EP7QWidget", 0, 0};
    static constexpr nm::Paths paths{
        "/usr/local/Kobo/imageformats/libnmqt6.so",
        "/mnt/onboard/.adds/nm-qt6/runtime.log", "/usr/local/Kobo/nickel"};
    nm::start(spec, paths);
}
