#include <alloca.h>
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "action_cc.h"
#include "util.h"

typedef void Device;
typedef void Settings;

extern "C" int nmi_action_nickelsetting(const char *arg, char **err_out) {
    #define NMI_ERR_RET 1

    Device *(*Device_getCurrentDevice)();
    reinterpret_cast<void*&>(Device_getCurrentDevice) = dlsym(RTLD_DEFAULT, "_ZN6Device16getCurrentDeviceEv");
    NMI_ASSERT(Device_getCurrentDevice, "could not dlsym Device::getCurrentDevice");

    void *(*Settings_Settings)(Settings*, Device*, bool);
    reinterpret_cast<void*&>(Settings_Settings) = dlsym(RTLD_DEFAULT, "_ZN8SettingsC2ERK6Deviceb");
    NMI_ASSERT(Device_getCurrentDevice, "could not dlsym Settings constructor");

    void *(*Settings_SettingsD)(Settings*);
    reinterpret_cast<void*&>(Settings_SettingsD) = dlsym(RTLD_DEFAULT, "_ZN8SettingsD2Ev");
    NMI_ASSERT(Settings_SettingsD, "could not dlsym Settings destructor");

    Device *dev = Device_getCurrentDevice();
    NMI_ASSERT(dev, "could not get shared nickel device pointer");

    Settings *settings = alloca(128); // way larger than it is, but better to be safe
    Settings_Settings(settings, dev, false);

    // to cast the generic Settings into its subclass FeatureSettings, the
    // vtable pointer at the beginning needs to be replaced with the target
    // vtable address plus 8 (for the header) (and if we don't do this, the
    // virtual functions such as sectionName won't get resolved properly) (it
    // it also needs to be restored after each function call, as that call may
    // change it to something itself).

    // warning: this is highly unportable, unsafe, and implementation
    // defined, but to make it safer, we can ensure the original pointer is
    // as we expect.

    // note: to figure this out yourself, do the pointer math in the
    // disassembly, note that it assigns the address of the target of an
    // address plus 8 (for the vtable header), then look up the address in
    // reverse to find that it refers to a GOT relocation for a vtable symbol.

    #define vtable_ptr(x) *reinterpret_cast<void**&>(settings)
    #define vtable_target(x) reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(x)+8)

    void *Settings_vtable = dlsym(RTLD_DEFAULT, "_ZTV8Settings");
    NMI_ASSERT(Settings_vtable, "could not dlsym the vtable for Settings");
    NMI_ASSERT(vtable_ptr(settings) == vtable_target(Settings_vtable), "unexpected vtable layout (expected class to start with a pointer to 8 bytes into the vtable)");

    if (!strcmp(arg, "invert")) {
        void *FeatureSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV15FeatureSettings");
        NMI_ASSERT(FeatureSettings_vtable, "could not dlsym the vtable for FeatureSettings");
        vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

        bool (*FeatureSettings_invertScreen)(Settings*);
        reinterpret_cast<void*&>(FeatureSettings_invertScreen) = dlsym(RTLD_DEFAULT, "_ZN15FeatureSettings12invertScreenEv");
        NMI_ASSERT(FeatureSettings_invertScreen, "could not dlsym FeatureSettings::invertScreen");

        bool (*FeatureSettings_setInvertScreen)(Settings*, bool);
        reinterpret_cast<void*&>(FeatureSettings_setInvertScreen) = dlsym(RTLD_DEFAULT, "_ZN15FeatureSettings15setInvertScreenEb");
        NMI_ASSERT(FeatureSettings_setInvertScreen, "could not dlsym FeatureSettings::setInvertScreen");

        bool v = FeatureSettings_invertScreen(settings);
        NMI_LOG("invertScreen = %d", v);
        vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

        FeatureSettings_setInvertScreen(settings, !v);
        vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

        NMI_ASSERT(FeatureSettings_invertScreen(settings) == !v, "failed to set setting");
        vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);
    } else {
        // TODO: more settings
        Settings_SettingsD(settings);
        NMI_RETURN_ERR("unknown setting name");
    }

    #undef vtable_ptr
    #undef vtable_target

    Settings_SettingsD(settings);

    NMI_RETURN_OK(0);
    #undef NMI_ERR_RET
}
