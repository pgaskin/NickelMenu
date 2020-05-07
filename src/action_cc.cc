#include <QApplication>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QWidget>

#include <initializer_list>

#include <alloca.h>
#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "action.h"
#include "util.h"

// A note about Nickel dlsyms:
//
// In general, we try to use Nickel's functionality over a Qt or system
// version provided that the symbol has existed for multiple versions, is
// unlikely to change (as judged by @geek1011), is not deeply tied to other
// parts of Nickel, and is unlikely to cause issues if it fails to run properly.
//
// Other times, we generally try to use system functionality through C library
// calls. This should be used if the action could easily be performed externally
// from Nickel with the exact same effect. Just keep in mind that NickelMenu
// runs as part of Nickel. Note that for this, you generally should put it into
// action_c.c instead of this file.
//
// In some cases, we might use Qt. This should only be done if it makes things
// significantly simpler, is the alternative to doing something which may
// interfere with Nickel's usage of Qt (i.e. exec), and will not interfere with
// Nickel itself. One example of this is running processes.
//
// Although we could write more C++-like code here, we're generally trying to
// reduce dependency on compiler-specific behaviour (e.g. with vtables) and
// external libraries. This is so we are able to gracefully catch and show the
// greatest number of errors, and so we are the most cross-Nickel-version
// compatible as possible.

typedef void Device;
typedef void Settings;
typedef void PlugWorkflowManager;
typedef void BrowserWorkflowManager;
typedef void N3SettingsExtrasController;
typedef void N3PowerWorkflowManager;
typedef void FrontLight;

NM_ACTION_(nickel_setting) {
    #define NM_ERR_RET nullptr

    Device *(*Device_getCurrentDevice)();
    reinterpret_cast<void*&>(Device_getCurrentDevice) = dlsym(RTLD_DEFAULT, "_ZN6Device16getCurrentDeviceEv");
    NM_ASSERT(Device_getCurrentDevice, "could not dlsym Device::getCurrentDevice");

    void *(*Settings_Settings)(Settings*, Device*, bool);
    void *(*Settings_SettingsLegacy)(Settings*, Device*);
    reinterpret_cast<void*&>(Settings_Settings) = dlsym(RTLD_DEFAULT, "_ZN8SettingsC2ERK6Deviceb");
    reinterpret_cast<void*&>(Settings_SettingsLegacy) = dlsym(RTLD_DEFAULT, "_ZN8SettingsC2ERK6Device");
    NM_ASSERT(Settings_Settings || Settings_SettingsLegacy, "could not dlsym Settings constructor (new and/or old)");

    void *(*Settings_SettingsD)(Settings*);
    reinterpret_cast<void*&>(Settings_SettingsD) = dlsym(RTLD_DEFAULT, "_ZN8SettingsD2Ev");
    NM_ASSERT(Settings_SettingsD, "could not dlsym Settings destructor");

    // some settings don't have symbols in a usable form, and some are inlined, so we may need to set them directly
    QVariant (*Settings_getSetting)(Settings*, QString const&, QVariant const&); // the last param is the default, also note that this requires a subclass of Settings
    reinterpret_cast<void*&>(Settings_getSetting) = dlsym(RTLD_DEFAULT, "_ZN8Settings10getSettingERK7QStringRK8QVariant");
    NM_ASSERT(Settings_getSetting, "could not dlsym Settings::getSetting");

    // ditto
    void *(*Settings_saveSetting)(Settings*, QString const&, QVariant const&, bool); // the last param is whether to do a full disk sync immediately (rather than waiting for the kernel to do it)
    reinterpret_cast<void*&>(Settings_saveSetting) = dlsym(RTLD_DEFAULT, "_ZN8Settings11saveSettingERK7QStringRK8QVariantb");
    NM_ASSERT(Settings_saveSetting, "could not dlsym Settings::saveSetting");

    Device *dev = Device_getCurrentDevice();
    NM_ASSERT(dev, "could not get shared nickel device pointer");

    Settings *settings = alloca(128); // way larger than it is, but better to be safe
    if (Settings_Settings)
        Settings_Settings(settings, dev, false);
    else if (Settings_SettingsLegacy)
        Settings_SettingsLegacy(settings, dev);

    // to cast the generic Settings into its subclass FeatureSettings, the
    // vtable pointer at the beginning needs to be replaced with the target
    // vtable address plus 8 (for the header) (and if we don't do this, the
    // virtual functions such as sectionName won't get resolved properly) (it
    // also needs to be restored after each function call, as that call may
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
    NM_ASSERT(Settings_vtable, "could not dlsym the vtable for Settings");
    NM_ASSERT(vtable_ptr(settings) == vtable_target(Settings_vtable), "unexpected vtable layout (expected class to start with a pointer to 8 bytes into the vtable)");

    bool v = false;

    if (!strcmp(arg, "invert") || !strcmp(arg, "screenshots")) {
        void *FeatureSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV15FeatureSettings");
        NM_ASSERT(FeatureSettings_vtable, "could not dlsym the vtable for FeatureSettings");
        vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

        if (!strcmp(arg, "invert")) {
            bool (*FeatureSettings_invertScreen)(Settings*);
            reinterpret_cast<void*&>(FeatureSettings_invertScreen) = dlsym(RTLD_DEFAULT, "_ZN15FeatureSettings12invertScreenEv");
            NM_ASSERT(FeatureSettings_invertScreen, "could not dlsym FeatureSettings::invertScreen");

            bool (*FeatureSettings_setInvertScreen)(Settings*, bool);
            reinterpret_cast<void*&>(FeatureSettings_setInvertScreen) = dlsym(RTLD_DEFAULT, "_ZN15FeatureSettings15setInvertScreenEb");
            NM_ASSERT(FeatureSettings_setInvertScreen, "could not dlsym FeatureSettings::setInvertScreen");

            v = FeatureSettings_invertScreen(settings);
            vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

            FeatureSettings_setInvertScreen(settings, !v);
            vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

            NM_ASSERT(FeatureSettings_invertScreen(settings) == !v, "failed to set setting");
            vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

            QWidget *w = QApplication::topLevelAt(25, 25);
            NM_LOG("updating top-level window %p after invert", w);
            if (w)
                w->update(); // TODO: figure out how to make it update _after_ the menu item redraws itself
        } else if (!strcmp(arg, "screenshots")) {
            QVariant v1 = Settings_getSetting(settings, QStringLiteral("Screenshots"), QVariant(false));
            vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

            v = v1.toBool();

            Settings_saveSetting(settings, QStringLiteral("Screenshots"), QVariant(!v), false);
            vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

            QVariant v2 = Settings_getSetting(settings, QStringLiteral("Screenshots"), QVariant(false));
            vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);
        }
    } else if (!strcmp(arg, "lockscreen")) {
        void *PowerSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV13PowerSettings");
        NM_ASSERT(PowerSettings_vtable, "could not dlsym the vtable for PowerSettings");
        vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

        if (!strcmp(arg, "lockscreen")) {
            bool (*PowerSettings__getUnlockEnabled)(Settings*);
            reinterpret_cast<void*&>(PowerSettings__getUnlockEnabled) = dlsym(RTLD_DEFAULT, "_ZN13PowerSettings16getUnlockEnabledEv");
            NM_ASSERT(PowerSettings__getUnlockEnabled, "could not dlsym PowerSettings::getUnlockEnabled");

            bool (*PowerSettings__setUnlockEnabled)(Settings*, bool);
            reinterpret_cast<void*&>(PowerSettings__setUnlockEnabled) = dlsym(RTLD_DEFAULT, "_ZN13PowerSettings16setUnlockEnabledEb");
            NM_ASSERT(PowerSettings__setUnlockEnabled, "could not dlsym PowerSettings::setUnlockEnabled");

            v = PowerSettings__getUnlockEnabled(settings);
            vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

            PowerSettings__setUnlockEnabled(settings, !v);
            vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

            NM_ASSERT(PowerSettings__getUnlockEnabled(settings) == !v, "failed to set setting");
            vtable_ptr(settings) = vtable_target(PowerSettings_vtable);
        }
    }  else if (!strcmp(arg, "force_wifi")) {
        void *PowerSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV11DevSettings");
        NM_ASSERT(PowerSettings_vtable, "could not dlsym the vtable for DevSettings");
        vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

        if (!strcmp(arg, "force_wifi")) {
            QVariant v1 = Settings_getSetting(settings, QStringLiteral("ForceWifiOn"), QVariant(false));
            vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

            v = v1.toBool();

            Settings_saveSetting(settings, QStringLiteral("ForceWifiOn"), QVariant(!v), false);
            vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

            QVariant v2 = Settings_getSetting(settings, QStringLiteral("ForceWifiOn"), QVariant(false));
            vtable_ptr(settings) = vtable_target(PowerSettings_vtable);
        }
    } else {
        // TODO: more settings
        Settings_SettingsD(settings);
        NM_RETURN_ERR("unknown setting name '%s'", arg);
    }

    #undef vtable_ptr
    #undef vtable_target

    Settings_SettingsD(settings);

    NM_RETURN_OK(strcmp(arg, "invert") // invert is obvious
        ? nm_action_result_toast("%s %s", v ? "disabled" : "enabled", arg)
        : nm_action_result_silent());
    #undef NM_ERR_RET
}

NM_ACTION_(frontlight) {
    // DO NOT USE THIS ACTION! SEE https://github.com/geek1011/NickelMenu/issues/3#issuecomment-625440790.
    // (it segfaults intermittently some time after changing it, due to unknown
    // reasons which are most likely to do with the hackery around the QObject
    // memory management here)

    #define NM_ERR_RET nullptr

    char *tmp = strdup(arg);

    char *value = tmp;
    char *thingy = strtrim(strsep(&value, ":"));
    NM_ASSERT(*value, "no value in frontlight argument '%s'", arg);
    value = strtrim(value);

    // note: this is probably the most fragile and complex out of all the actions

    void *(*FrontLight_FrontLight)(QObject*);
    reinterpret_cast<void*&>(FrontLight_FrontLight) = dlsym(RTLD_DEFAULT, "_ZN10FrontLightC2EP7QObject");
    NM_ASSERT(FrontLight_FrontLight, "could not dlsym FrontLight constructor");

    void *(*FrontLight_FrontLightD)(FrontLight*);
    reinterpret_cast<void*&>(FrontLight_FrontLightD) = dlsym(RTLD_DEFAULT, "_ZN10FrontLightD1Ev"); // note: we don't use D0, as that will delete the object itself too
    NM_ASSERT(FrontLight_FrontLightD, "could not dlsym FrontLight destructor");

    Device *(*Device_getCurrentDevice)();
    reinterpret_cast<void*&>(Device_getCurrentDevice) = dlsym(RTLD_DEFAULT, "_ZN6Device16getCurrentDeviceEv");
    NM_ASSERT(Device_getCurrentDevice, "could not dlsym Device::getCurrentDevice");

    void *(*Settings_Settings)(Settings*, Device*, bool);
    void *(*Settings_SettingsLegacy)(Settings*, Device*);
    reinterpret_cast<void*&>(Settings_Settings) = dlsym(RTLD_DEFAULT, "_ZN8SettingsC2ERK6Deviceb");
    reinterpret_cast<void*&>(Settings_SettingsLegacy) = dlsym(RTLD_DEFAULT, "_ZN8SettingsC2ERK6Device");
    NM_ASSERT(Settings_Settings || Settings_SettingsLegacy, "could not dlsym Settings constructor (new and/or old)");

    void *(*Settings_SettingsD)(Settings*);
    reinterpret_cast<void*&>(Settings_SettingsD) = dlsym(RTLD_DEFAULT, "_ZN8SettingsD2Ev");
    NM_ASSERT(Settings_SettingsD, "could not dlsym Settings destructor");

    // I don't like the feel of this much, but it's the only way, so we'll have
    // to live with the possible segfaults. We need a custom QObject for the
    // FrontLight constructor, and it replaces the vtable with the FrontLight
    // one, so it doesn't actually matter what it is. Since we are doing funny
    // stuff (for lack of a better word) with this, we need to dlsym the QObject
    // constructor so *this* compiler doesn't try to treat it like a real
    // object.

    QObject *parent = new QObject(nullptr); // we can't just use another object directly, as FrontLight will replace the vtable
    parent->moveToThread(QApplication::instance()->thread()); // the QObject doesn't have a thread to begin with
    NM_ASSERT(parent->thread(), "our parent QObject doesn't have a thread for FrontLight to attach signals to");
    FrontLight *fl = FrontLight_FrontLight(parent); // this will replace the vtable and attach some signals
    NM_ASSERT(fl, "frontlight didn't return anything"); // it's more likely to just segfault in the constructor

    Device *dev = Device_getCurrentDevice();
    NM_ASSERT(dev, "could not get shared nickel device pointer");

    Settings *settings = alloca(128); // way larger than it is, but better to be safe
    if (Settings_Settings)
        Settings_Settings(settings, dev, false);
    else if (Settings_SettingsLegacy)
        Settings_SettingsLegacy(settings, dev);

    #define vtable_ptr(x) *reinterpret_cast<void**&>(settings)
    #define vtable_target(x) reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(x)+8)

    void *PowerSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV13PowerSettings");
    NM_ASSERT(PowerSettings_vtable, "could not dlsym the vtable for PowerSettings");
    vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

    if (!strcmp(thingy, "level")) {
        char *end;
        long pct = strtol(value, &end, 10);
        NM_ASSERT(*thingy && !*end && pct >= 0 && pct <= 100, "level (%%) invalid or out of range 0-100: '%s'", value);

        bool (*Device__supportsLight)(Device*);
        reinterpret_cast<void*&>(Device__supportsLight) = dlsym(RTLD_DEFAULT, "_ZNK6Device13supportsLightEv");
        if (Device__supportsLight)
            NM_ASSERT(Device__supportsLight(dev), "device does not have a light");

        void (*FrontLight__setBrightness)(FrontLight*, int, int); // the first int is percent (it must not be 0, or it will segfault), the second int is animation duration
        reinterpret_cast<void*&>(FrontLight__setBrightness) = dlsym(RTLD_DEFAULT, "_ZNK10FrontLight13setBrightnessEii");
        NM_ASSERT(FrontLight__setBrightness, "could not dlsym FrontLight::setBrightness");

        void (*PowerSettings__setFrontLightState)(Settings*, bool);
        reinterpret_cast<void*&>(PowerSettings__setFrontLightState) = dlsym(RTLD_DEFAULT, "_ZN13PowerSettings18setFrontLightStateEb");
        NM_ASSERT(PowerSettings__setFrontLightState, "could not dlsym PowerSettings::setFrontLightState");

        void (*PowerSettings__setFrontLightLevel)(Settings*, int);
        reinterpret_cast<void*&>(PowerSettings__setFrontLightLevel) = dlsym(RTLD_DEFAULT, "_ZN13PowerSettings18setFrontLightLevelEi");
        NM_ASSERT(PowerSettings__setFrontLightLevel, "could not dlsym PowerSettings::setFrontLightLevel");

        bool (*Device__hasAmbientLightSensor)(Device*);
        reinterpret_cast<void*&>(Device__hasAmbientLightSensor) = dlsym(RTLD_DEFAULT, "_ZNK6Device21hasAmbientLightSensorEv");
        if (Device__hasAmbientLightSensor && Device__hasAmbientLightSensor(dev)) {
            void (*PowerSettings__setAmbientLightSensorEnabled)(Settings*, bool);
            reinterpret_cast<void*&>(PowerSettings__setAmbientLightSensorEnabled) = dlsym(RTLD_DEFAULT, "_ZN13PowerSettings28setAmbientLightSensorEnabledEb");
            NM_ASSERT(PowerSettings__setAmbientLightSensorEnabled, "could not dlsym PowerSettings::setAmbientLightSensorEnabled");

            PowerSettings__setAmbientLightSensorEnabled(settings, false); // this won't update the toggle in the frontlight dialog, but that's just a cosmetic issue
            vtable_ptr(settings) = vtable_target(PowerSettings_vtable);
        }

        FrontLight__setBrightness(fl, (int)(pct), 0);

        PowerSettings__setFrontLightState(settings, pct != 0);
        vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

        if (pct) {
            PowerSettings__setFrontLightLevel(settings, (int)(pct));
            vtable_ptr(settings) = vtable_target(PowerSettings_vtable);
        }
    } else if (!strcmp(thingy, "temperature")) {
        char *end;
        long temp = strtol(value, &end, 10);
        NM_ASSERT(*thingy && !*end , "temperature (K) invalid: '%s'", value);

        bool (*Device__hasNaturalLight)(Device*);
        reinterpret_cast<void*&>(Device__hasNaturalLight) = dlsym(RTLD_DEFAULT, "_ZNK6Device15hasNaturalLightEv");
        if (Device__hasNaturalLight)
            NM_ASSERT(Device__hasNaturalLight(dev), "device does not support ComfortLight");

        int (*FrontLight__minTemperature)(FrontLight*);
        reinterpret_cast<void*&>(FrontLight__minTemperature) = dlsym(RTLD_DEFAULT, "_ZNK10FrontLight14minTemperatureEv");
        NM_ASSERT(FrontLight__minTemperature, "could not dlsym FrontLight::minTemperature");

        int (*FrontLight__maxTemperature)(FrontLight*);
        reinterpret_cast<void*&>(FrontLight__maxTemperature) = dlsym(RTLD_DEFAULT, "_ZNK10FrontLight14maxTemperatureEv");
        NM_ASSERT(FrontLight__maxTemperature, "could not dlsym FrontLight::maxTemperature");

        int min = FrontLight__minTemperature(fl);
        int max = FrontLight__maxTemperature(fl);
        NM_LOG("temp: min=%d max=%d", min, max);
        NM_ASSERT(temp == 0 || (temp >= min && temp <= max), "temperature (K) out of device range %d-%d (or 0 for auto): '%s'", min, max, value);

        // note: this one MUST not be called with a temperature out of range, or it corrupts something and eventually segfaults within a minute or two
        void (*FrontLight__setTemperature)(FrontLight*, int, bool, int); // the first int is percent, the second int is animation duration, I don't have the slightest idea what the bool is for, but it seems to segfault after a delay if it's false
        reinterpret_cast<void*&>(FrontLight__setTemperature) = dlsym(RTLD_DEFAULT, "_ZN10FrontLight14setTemperatureEibi");
        NM_ASSERT(FrontLight__setTemperature, "could not dlsym FrontLight::setTemperature");

        void (*PowerSettings__setAutoColorEnabled)(Settings*, bool);
        reinterpret_cast<void*&>(PowerSettings__setAutoColorEnabled) = dlsym(RTLD_DEFAULT, "_ZN13PowerSettings19setAutoColorEnabledEb");
        NM_ASSERT(PowerSettings__setAutoColorEnabled, "could not dlsym PowerSettings::setAutoColorEnabled");

        void (*PowerSettings__setColorSetting)(Settings*, int);
        reinterpret_cast<void*&>(PowerSettings__setColorSetting) = dlsym(RTLD_DEFAULT, "_ZN13PowerSettings15setColorSettingEi");
        NM_ASSERT(PowerSettings__setColorSetting, "could not dlsym PowerSettings::setColorSetting");

        if (temp)
            FrontLight__setTemperature(fl, temp, true, 0);

        PowerSettings__setAutoColorEnabled(settings, temp == 0); // this won't update the toggle in the frontlight dialog, but that's just a cosmetic issue
        vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

        if (temp) {
            PowerSettings__setColorSetting(settings, (int)(temp));
            vtable_ptr(settings) = vtable_target(PowerSettings_vtable);
        }
    } else {
        Settings_SettingsD(settings);
        NM_RETURN_ERR("unknown frontlight action '%s'", arg);
    }

    #undef vtable_ptr
    #undef vtable_target

    // see https://github.com/geek1011/NickelMenu/issues/3#issuecomment-625376864
    NM_LOG("there is an intentional memory leak here (but it is small enough to be neglegible) of FrontLight objects, as we can't destroy them until after their QTimer is done, which is past the lifetime of the action, and we can't do it from a different thread without violating Qt's memory model");
    //FrontLight_FrontLightD(fl);

    Settings_SettingsD(settings);
    free(tmp);

    NM_RETURN_OK(nm_action_result_silent());
    #undef NM_ERR_RET
}

NM_ACTION_(nickel_extras) {
    #define NM_ERR_RET nullptr

    if (!strcmp(arg, "web_browser")) {
        void (*N3SettingsExtrasController_N3SettingsExtrasController)(N3SettingsExtrasController*);
        reinterpret_cast<void*&>(N3SettingsExtrasController_N3SettingsExtrasController) = dlsym(RTLD_DEFAULT, "_ZN26N3SettingsExtrasControllerC2Ev");
        NM_ASSERT(N3SettingsExtrasController_N3SettingsExtrasController, "could not dlsym N3SettingsExtrasController constructor");

        void (*N3SettingsExtrasController_N3SettingsExtrasControllerD)(N3SettingsExtrasController*);
        reinterpret_cast<void*&>(N3SettingsExtrasController_N3SettingsExtrasControllerD) = dlsym(RTLD_DEFAULT, "_ZN26N3SettingsExtrasControllerD1Ev");
        NM_ASSERT(N3SettingsExtrasController_N3SettingsExtrasControllerD, "could not dlsym N3SettingsExtrasController destructor");

        void (*N3SettingsExtrasController_openBrowser)(N3SettingsExtrasController*);
        reinterpret_cast<void*&>(N3SettingsExtrasController_openBrowser) = dlsym(RTLD_DEFAULT, "_ZN26N3SettingsExtrasController11openBrowserEv");
        NM_ASSERT(N3SettingsExtrasController_openBrowser, "could not dlsym BrowserWorkflowManager::openBrowser");

        N3SettingsExtrasController *nse = alloca(128); // as of 4.20.14622, it's actually 48 bytes, but we're going to stay on the safe side
        N3SettingsExtrasController_N3SettingsExtrasController(nse);
        N3SettingsExtrasController_openBrowser(nse);
        N3SettingsExtrasController_N3SettingsExtrasControllerD(nse);

        // the QObject is only used to pass events to it, but there's something I'm missing here which leads to it segfaulting after connecting to WiFi if it isn't already connected
        /*void (*BrowserWorkflowManager_BrowserWorkflowManager)(BrowserWorkflowManager*, QObject*);
        reinterpret_cast<void*&>(BrowserWorkflowManager_BrowserWorkflowManager) = dlsym(RTLD_DEFAULT, "_ZN22BrowserWorkflowManagerC1EP7QObject");
        NM_ASSERT(BrowserWorkflowManager_BrowserWorkflowManager, "could not dlsym BrowserWorkflowManager constructor");

        void (*BrowserWorkflowManager_openBrowser)(BrowserWorkflowManager*, bool, QUrl const&, QString const&); // the bool is whether to open it as a modal, the string is CSS to inject
        reinterpret_cast<void*&>(BrowserWorkflowManager_openBrowser) = dlsym(RTLD_DEFAULT, "_ZN22BrowserWorkflowManager11openBrowserEbRK4QUrlRK7QString");
        NM_ASSERT(BrowserWorkflowManager_openBrowser, "could not dlsym BrowserWorkflowManager::openBrowser");

        BrowserWorkflowManager *bwm = alloca(128); // as of 4.20.14622, it's actually 20 bytes, but we're going to stay on the safe side
        BrowserWorkflowManager_BrowserWorkflowManager(bwm, new QObject());
        BrowserWorkflowManager_openBrowser(bwm, false, QUrl(), QStringLiteral("")); // if !QUrl::isValid(), it loads the homepage*/
        NM_RETURN_OK(nm_action_result_silent());
    }

    const char* mimetype;
    if (strchr(arg, '/'))                   mimetype = arg;
    else if (!strcmp(arg, "unblock_it"))    mimetype = "application/x-games-RushHour";
    else if (!strcmp(arg, "sketch_pad"))    mimetype = "application/x-games-Scribble";
    else if (!strcmp(arg, "solitaire"))     mimetype = "application/x-games-Solitaire";
    else if (!strcmp(arg, "sudoku"))        mimetype = "application/x-games-Sudoku";
    else if (!strcmp(arg, "word_scramble")) mimetype = "application/x-games-Boggle";
    else NM_RETURN_ERR("unknown beta feature name or plugin mimetype '%s'", arg);

    void (*ExtrasPluginLoader_loadPlugin)(const char*);
    reinterpret_cast<void*&>(ExtrasPluginLoader_loadPlugin) = dlsym(RTLD_DEFAULT, "_ZN18ExtrasPluginLoader10loadPluginEPKc");
    NM_ASSERT(ExtrasPluginLoader_loadPlugin, "could not dlsym ExtrasPluginLoader::loadPlugin");
    ExtrasPluginLoader_loadPlugin(mimetype);

    NM_RETURN_OK(nm_action_result_silent());
    #undef NM_ERR_RET
}

NM_ACTION_(nickel_misc) {
    #define NM_ERR_RET nullptr
    if (!strcmp(arg, "rescan_books")) {
        PlugWorkflowManager *(*PlugWorkflowManager_sharedInstance)();
        reinterpret_cast<void*&>(PlugWorkflowManager_sharedInstance) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager14sharedInstanceEv");
        NM_ASSERT(PlugWorkflowManager_sharedInstance, "could not dlsym PlugWorkflowManager::sharedInstance");

        void (*PlugWorkflowManager_sync)(PlugWorkflowManager*);
        reinterpret_cast<void*&>(PlugWorkflowManager_sync) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager4syncEv");
        NM_ASSERT(PlugWorkflowManager_sync, "could not dlsym PlugWorkflowManager::sync");

        PlugWorkflowManager *wf = PlugWorkflowManager_sharedInstance();
        NM_ASSERT(wf, "could not get shared PlugWorkflowManager pointer");

        PlugWorkflowManager_sync(wf);
    } else if (!strcmp(arg, "rescan_books_full")) {
        PlugWorkflowManager *(*PlugWorkflowManager_sharedInstance)();
        reinterpret_cast<void*&>(PlugWorkflowManager_sharedInstance) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager14sharedInstanceEv");
        NM_ASSERT(PlugWorkflowManager_sharedInstance, "could not dlsym PlugWorkflowManager::sharedInstance");

        // this is what is called by PlugWorkflowManager::plugged after confirmation
        void (*PlugWorkflowManager_onCancelAndConnect)(PlugWorkflowManager*);
        reinterpret_cast<void*&>(PlugWorkflowManager_onCancelAndConnect) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager18onCancelAndConnectEv");
        NM_ASSERT(PlugWorkflowManager_onCancelAndConnect, "could not dlsym PlugWorkflowManager::onCancelAndConnect");

        void (*PlugWorkflowManager_unplugged)(PlugWorkflowManager*);
        reinterpret_cast<void*&>(PlugWorkflowManager_unplugged) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager9unpluggedEv");
        NM_ASSERT(PlugWorkflowManager_unplugged, "could not dlsym PlugWorkflowManager::unplugged");

        PlugWorkflowManager *wf = PlugWorkflowManager_sharedInstance();
        NM_ASSERT(wf, "could not get shared PlugWorkflowManager pointer");

        PlugWorkflowManager_onCancelAndConnect(wf);
        sleep(1);
        PlugWorkflowManager_unplugged(wf);
    } else if (!strcmp(arg, "force_usb_connection")) {
        // we could call libnickel directly, but I prefer not to
        FILE *nhs;
        NM_ASSERT((nhs = fopen("/tmp/nickel-hardware-status", "w")), "could not open nickel hardware status pipe: %s", strerror(errno));

        const char *msg = "usb plug add";
        NM_ASSERT(fputs(msg, nhs) >= 0, "could not write message '%s' to pipe: %s", msg, strerror(errno));

        fclose(nhs);
    } else {
        NM_RETURN_ERR("unknown action '%s'", arg);
    }
    NM_RETURN_OK(nm_action_result_silent());
    #undef NM_ERR_RET
}

NM_ACTION_(power) {
    #define NM_ERR_RET nullptr
    if (!strcmp(arg, "shutdown") || !strcmp(arg, "reboot")) {
        N3PowerWorkflowManager *(*N3PowerWorkflowManager_sharedInstance)();
        reinterpret_cast<void*&>(N3PowerWorkflowManager_sharedInstance) = dlsym(RTLD_DEFAULT, "_ZN22N3PowerWorkflowManager14sharedInstanceEv");
        NM_ASSERT(N3PowerWorkflowManager_sharedInstance, "could not dlsym N3PowerWorkflowManager::sharedInstance, so cannot perform action cleanly (if you must, report a bug and use cmd_spawn instead)");
        
        N3PowerWorkflowManager *pwm = N3PowerWorkflowManager_sharedInstance();
        NM_ASSERT(pwm, "could not get shared power manager pointer");

        if (!strcmp(arg, "shutdown")) {
            void (*N3PowerWorkflowManager_powerOff)(N3PowerWorkflowManager*, bool); // bool is for if it's due to low battery
            reinterpret_cast<void*&>(N3PowerWorkflowManager_powerOff) = dlsym(RTLD_DEFAULT, "_ZN22N3PowerWorkflowManager8powerOffEb");
            NM_ASSERT(N3PowerWorkflowManager_powerOff, "could not dlsym N3PowerWorkflowManager::powerOff");

            N3PowerWorkflowManager_powerOff(pwm, false);
            NM_RETURN_OK(nm_action_result_toast("Shutting down..."));
        } else if (!strcmp(arg, "reboot")) {
            void (*N3PowerWorkflowManager_reboot)(N3PowerWorkflowManager*);
            reinterpret_cast<void*&>(N3PowerWorkflowManager_reboot) = dlsym(RTLD_DEFAULT, "_ZN22N3PowerWorkflowManager6rebootEv");
            NM_ASSERT(N3PowerWorkflowManager_reboot, "could not dlsym N3PowerWorkflowManager::reboot");

            N3PowerWorkflowManager_reboot(pwm);
            NM_RETURN_OK(nm_action_result_toast("Rebooting..."));
        }
    } else {
        NM_RETURN_ERR("unknown power action '%s'", arg);
    }
    NM_RETURN_OK(nm_action_result_silent());
    #undef NM_ERR_RET
}

NM_ACTION_(cmd_spawn) {
    #define NM_ERR_RET nullptr
    QProcess proc;
    uint64_t pid;
    bool ok = proc.startDetached(
        QStringLiteral("/bin/sh"),
        QStringList(std::initializer_list<QString>{
            QStringLiteral("-c"),
            QString::fromUtf8(arg),
        }),
        QStringLiteral("/"),
        (qint64*)(&pid)
    );
    NM_ASSERT(ok, "could not start process");
    NM_RETURN_OK(nm_action_result_toast("Successfully started process with PID %lu.", (unsigned long)(pid)));
    #undef NM_ERR_RET
}

NM_ACTION_(cmd_output) {
    #define NM_ERR_RET nullptr

    char *tmp = strdup(arg);

    char *cmd = tmp;
    char *tmp1 = strsep(&cmd, ":"), *tmp2;
    long timeout = strtol(tmp1, &tmp2, 10);
    NM_ASSERT(*tmp1 && !*tmp2 && timeout > 0 && timeout < 10000, "invalid timeout '%s'", tmp1);

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.setWorkingDirectory(QStringLiteral("/"));
    proc.start(
        QStringLiteral("/bin/sh"),
        QStringList(std::initializer_list<QString>{
            QStringLiteral("-c"),
            QString::fromUtf8(cmd),
        }),
        QIODevice::ReadOnly
    );

    bool ok = proc.waitForFinished(timeout);
    if (!ok) {
        switch (proc.error()) {
        case QProcess::Timedout:
            NM_RETURN_ERR("could not run process: timed out");
        case QProcess::FailedToStart:
            NM_RETURN_ERR("could not run process: missing program or wrong permissions");
        case QProcess::Crashed:
            NM_RETURN_ERR("could not run process: process crashed");
        default:
            NM_RETURN_ERR("could not run process");
        }
    }
    if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() != 0)
        NM_RETURN_ERR("could not run process: process exited with status %d", proc.exitCode());

    QString out = proc.readAllStandardOutput();
    if (out.length() > 500)
        out = out.left(500) + "...";

    free(tmp);

    NM_RETURN_OK(nm_action_result_msg("%s", qPrintable(out)));

    #undef NM_ERR_RET
}
