#include "nickel.h"
#include "util.h"
#include <QCoreApplication>
#include <QThread>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <cstdint>
#include <cstdio>

namespace nm6 {
namespace {
struct Build {
    uintptr_t base;
    bool audited = false;
};
int inspect_build(dl_phdr_info *info, size_t, void *opaque) {
    auto &build = *static_cast<Build *>(opaque);
    if (info->dlpi_addr != build.base) return 0;
    for (size_t i = 0; i < info->dlpi_phnum; ++i) {
        const auto &note = info->dlpi_phdr[i];
        if (note.p_type != PT_NOTE) continue;
        bool mapped = false;
        for (size_t j = 0; j < info->dlpi_phnum; ++j) {
            const auto &load = info->dlpi_phdr[j];
            if (load.p_type == PT_LOAD && (load.p_flags & PF_R) &&
                note.p_vaddr >= load.p_vaddr && note.p_vaddr - load.p_vaddr <= load.p_memsz &&
                note.p_memsz <= load.p_memsz - (note.p_vaddr - load.p_vaddr)) mapped = true;
        }
        if (!mapped) continue;
        const auto *bytes = reinterpret_cast<const unsigned char *>(build.base + note.p_vaddr);
        size_t offset = 0;
        while (note.p_memsz - offset >= sizeof(ElfW(Nhdr))) {
            ElfW(Nhdr) header;
            memcpy(&header, bytes + offset, sizeof(header));
            offset += sizeof(header);
            const size_t names = (size_t(header.n_namesz) + 3) & ~size_t(3);
            const size_t desc = (size_t(header.n_descsz) + 3) & ~size_t(3);
            if (names < header.n_namesz || desc < header.n_descsz ||
                names > note.p_memsz - offset || desc > note.p_memsz - offset - names) break;
            if (header.n_type == NT_GNU_BUILD_ID && header.n_namesz == 4 &&
                header.n_descsz == 20 && !memcmp(bytes + offset, "GNU\0", 4)) {
                char id[41]{};
                for (size_t k = 0; k < 20; ++k)
                    snprintf(id + k * 2, 3, "%02x", bytes[offset + names + k]);
                build.audited = !strcmp(id, "5bfd31d75408ae3826eb1e9b58b90156d495fd28") ||
                                !strcmp(id, "eaa30496411474df4019643cf7ba21efbdc25b52");
                NM_LOG("Qt6 private action ABI build=%s audited=%d", id, build.audited);
                return 1;
            }
            offset += names + desc;
        }
    }
    return 1;
}
}

Nickel::Nickel() {
    auto *app = QCoreApplication::instance();
    if (!app || QThread::currentThread() != app->thread()) {
        NM_ERR_SET("Nickel actions require the GUI thread");
        return;
    }
    void *handle = dlopen("libnickel.so.1.0.0", RTLD_NOW | RTLD_NOLOAD);
    if (!handle) {
        NM_ERR_SET("Nickel is not loaded");
        return;
    }
    link_map *map = nullptr;
    Build build{};
    if (dlinfo(handle, RTLD_DI_LINKMAP, &map) == 0 && map) {
        build.base = static_cast<uintptr_t>(map->l_addr);
        dl_iterate_phdr(inspect_build, &build);
    }
    if (sizeof(void *) != 4 || !build.audited) {
        dlclose(handle);
        NM_ERR_SET("private Nickel actions require an audited ARM32 firmware (5.18.270971 or 6.0.274403)");
        return;
    }
    handle_ = handle;
    base_ = reinterpret_cast<void *>(build.base);
}
Nickel::~Nickel() { if (handle_) dlclose(handle_); }
void *Nickel::symbol(const char *name, bool function) const {
    if (!handle_) return nullptr;
    void *address = dlsym(handle_, name);
    Dl_info info{};
    if (!address || !dladdr(address, &info) || info.dli_fbase != base_ ||
        (function && !(reinterpret_cast<uintptr_t>(address) & 1))) {
        NM_ERR_SET("this firmware does not provide the required Nickel interface %s", name);
        return nullptr;
    }
    return address;
}

NickelSettings::NickelSettings(const Nickel &nickel, const char *section) {
    auto device = nickel.resolve<void *(*)()>("_ZN6Device16getCurrentDeviceEv");
    auto construct = nickel.resolve<void (*)(void *, const void *, bool)>("_ZN8SettingsC1ERK6Deviceb");
    auto destroy = nickel.resolve<void (*)(void *)>("_ZN8SettingsD1Ev");
    get_ = nickel.resolve<decltype(get_)>("_ZN8Settings10getSettingERK7QStringRK8QVariant");
    set_ = nickel.resolve<decltype(set_)>("_ZN8Settings11saveSettingERK7QStringRK8QVariantb");
    auto *base = nickel.vtable("_ZTV8Settings");
    auto *derived = nickel.vtable(section);
    if (!device || !construct || !destroy || !get_ || !set_ || !base || !derived) return;
    auto *current = device();
    if (!current) { NM_ERR_SET("could not get Nickel's current device"); return; }
    base_vtable_ = static_cast<unsigned char *>(base) + 2 * sizeof(void *);
    section_vtable_ = static_cast<unsigned char *>(derived) + 2 * sizeof(void *);
    construct(storage_, current, false);
    destroy_ = destroy;
    void *actual;
    memcpy(&actual, storage_, sizeof(actual));
    if (actual != base_vtable_) { NM_ERR_SET("unexpected Nickel Settings vtable"); return; }
    // The audited subclasses have no additional fields. Nickel's own inline
    // construction calls Settings C2 then installs this same address point.
    ready_ = true;
}
NickelSettings::~NickelSettings() {
    if (!destroy_) return;
    memcpy(storage_, &base_vtable_, sizeof(base_vtable_));
    destroy_(storage_);
}
void *NickelSettings::object() {
    memcpy(storage_, &section_vtable_, sizeof(section_vtable_));
    return storage_;
}
QVariant NickelSettings::get(const QString &key, const QVariant &fallback) {
    return get_(object(), key, fallback);
}
void NickelSettings::set(const QString &key, const QVariant &value) {
    set_(object(), key, value, false);
}
}
