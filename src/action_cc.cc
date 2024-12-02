#include <QApplication>
#include <QProcess>
#include <QScreen>
#include <QShowEvent>
#include <QString>
#include <QStringList>
#include <QTextDocument>
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
// unlikely to change (as judged by @pgaskin), is not deeply tied to other
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
typedef void WirelessWorkflowManager;
typedef void StatusBarView;
typedef void MoreController;
typedef void MainWindowController;
typedef void BluetoothManager;

#define NM_ACT_SYM(var, sym) reinterpret_cast<void*&>(var) = dlsym(RTLD_DEFAULT, sym)
#define NM_ACT_XSYM(var, symb, err) do { \
    NM_ACT_SYM(var, symb);               \
    NM_CHECK(nullptr, var, err);         \
} while(0)

NM_ACTION_(nickel_open) {
    char *tmp1 = strdupa(arg); // strsep and strtrim will modify it
    char *arg1 = strtrim(strsep(&tmp1, ":"));
    char *arg2 = strtrim(tmp1);
    NM_CHECK(nullptr, arg2, "could not find a : in the argument");

    //libnickel 4.23.15505 * _ZN11MainNavViewC1EP7QWidget
    if (dlsym(RTLD_DEFAULT, "_ZN11MainNavViewC1EP7QWidget")) {
        NM_LOG("nickel_open: detected firmware >15505 (new nav tab bar), checking special cases");

        if (!strcmp(arg1, "library") && !strcmp(arg2, "dropbox")) {
            //libnickel 4.23.15505 * _ZN14MoreControllerC1Ev
            MoreController *(*MoreController__MoreController)(MoreController* _this);
            NM_ACT_XSYM(MoreController__MoreController, "_ZN14MoreControllerC1Ev", "could not dlsym MoreController::MoreController");

            //libnickel 4.23.15505 * _ZN14MoreController7dropboxEv
            void (*MoreController_dropbox)(MoreController* _this);
            NM_ACT_XSYM(MoreController_dropbox, "_ZN14MoreController7dropboxEv", "could not dlsym MoreController::dropbox");

            //libnickel 4.23.15505 * _ZN14MoreControllerD0Ev
            MoreController *(*MoreController__deMoreController)(MoreController* _this);
            NM_ACT_XSYM(MoreController__deMoreController, "_ZN14MoreControllerD0Ev", "could not dlsym MoreController::~MoreController");

            // As of at least 16704, maybe earlier, a MoreController is required.
            // It seems 44 bytes is required, over allocate to be on the safe side
            MoreController *mc = reinterpret_cast<MoreController*>(::operator new(128));
            NM_CHECK(nullptr, mc, "could not allocate memory for MoreController");
            mc = MoreController__MoreController(mc);
            NM_CHECK(nullptr, mc, "MoreController::MoreController returned null pointer");

            MoreController_dropbox(mc);

            // Clean up after ourselves
            MoreController__deMoreController(mc);

            return nm_action_result_silent();
        }

        if (!strcmp(arg1, "library") && !strcmp(arg2, "gdrive")) {
            //libnickel 4.23.15505 * _ZN14MoreControllerC1Ev
            MoreController *(*MoreController__MoreController)(MoreController* _this);
            NM_ACT_XSYM(MoreController__MoreController, "_ZN14MoreControllerC1Ev", "could not dlsym MoreController::MoreController");
    
            //libnickel 4.23.15505 * _ZN14MoreController11googleDriveEv
            void (*MoreController_gdrive)(MoreController* _this);
            NM_ACT_XSYM(MoreController_gdrive, "_ZN14MoreController11googleDriveEv", "could not dlsym MoreController::gdrive");
    
            //libnickel 4.23.15505 * _ZN14MoreControllerD0Ev
            MoreController *(*MoreController__deMoreController)(MoreController* _this);
            NM_ACT_XSYM(MoreController__deMoreController, "_ZN14MoreControllerD0Ev", "could not dlsym MoreController::~MoreController");
    
            // As of at least 16704, maybe earlier, a MoreController is required.
            // It seems 44 bytes is required, over allocate to be on the safe side
            MoreController *mc = reinterpret_cast<MoreController*>(::operator new(128));
            NM_CHECK(nullptr, mc, "could not allocate memory for MoreController");
            mc = MoreController__MoreController(mc);
            NM_CHECK(nullptr, mc, "MoreController::MoreController returned null pointer");
    
            MoreController_gdrive(mc);
    
            // Clean up after ourselves
            MoreController__deMoreController(mc);
    
            return nm_action_result_silent();
        }

        NM_LOG("nickel_open: no special handling needed for '%s:%s' on fw >15505", arg1, arg2);
    }

    const char *sym_c = nullptr; // *NavMixin constructor (subclass of QObject)
    const char *sym_d = nullptr; // *NavMixin destructor (D1, not D0 because it also tries to call delete)
    const char *sym_f = nullptr; // *NavMixin::* function

    if (!strcmp(arg1, "discover")) {
        sym_c = "_ZN16DiscoverNavMixinC1Ev"; //libnickel 4.6 * _ZN16DiscoverNavMixinC1Ev
        sym_d = "_ZN16DiscoverNavMixinD1Ev"; //libnickel 4.6 * _ZN16DiscoverNavMixinD1Ev

        if      (!strcmp(arg2, "storefront")) sym_f = "_ZN16DiscoverNavMixin10storefrontEv"; //libnickel 4.6 * _ZN16DiscoverNavMixin10storefrontEv
        else if (!strcmp(arg2, "wishlist"))   sym_f = "_ZN16DiscoverNavMixin8wishlistEv";    //libnickel 4.6 * _ZN16DiscoverNavMixin8wishlistEv
    } else if (!strcmp(arg1, "library")) {
        sym_c = "_ZN15LibraryNavMixinC1Ev"; //libnickel 4.6 * _ZN15LibraryNavMixinC1Ev
        sym_d = "_ZN15LibraryNavMixinD1Ev"; //libnickel 4.6 * _ZN15LibraryNavMixinD1Ev

        if      (!strcmp(arg2, "library"))  sym_f = "_ZN15LibraryNavMixin18showLastLibraryTabEv";      //libnickel 4.6 * _ZN15LibraryNavMixin18showLastLibraryTabEv
        else if (!strcmp(arg2, "all"))      sym_f = "_ZN15LibraryNavMixin23showAllItemsWithoutSyncEv"; //libnickel 4.6 * _ZN15LibraryNavMixin23showAllItemsWithoutSyncEv
        else if (!strcmp(arg2, "authors"))  sym_f = "_ZN15LibraryNavMixin11showAuthorsEv";             //libnickel 4.6 * _ZN15LibraryNavMixin11showAuthorsEv
        else if (!strcmp(arg2, "series"))   sym_f = "_ZN15LibraryNavMixin10showSeriesEv";              //libnickel 4.20.14601 * _ZN15LibraryNavMixin10showSeriesEv
        else if (!strcmp(arg2, "shelves"))  sym_f = "_ZN15LibraryNavMixin11showShelvesEv";             //libnickel 4.6 * _ZN15LibraryNavMixin11showShelvesEv
        else if (!strcmp(arg2, "pocket"))   sym_f = "_ZN15LibraryNavMixin17showPocketLibraryEv";       //libnickel 4.6 * _ZN15LibraryNavMixin17showPocketLibraryEv
        else if (!strcmp(arg2, "dropbox"))  sym_f = "_ZN15LibraryNavMixin11showDropboxEv";             //libnickel 4.18.13737 4.22.15268 _ZN15LibraryNavMixin11showDropboxEv
        else if (!strcmp(arg2, "gdrive"))  sym_f = "_ZN14MoreController11googleDriveEv";             //libnickel 4.18.13737 4.22.15268 _ZN14MoreController11googleDriveEv
    } else if (!strcmp(arg1, "reading_life")) {
        sym_c = "_ZN19ReadingLifeNavMixinC1Ev"; //libnickel 4.6 * _ZN19ReadingLifeNavMixinC1Ev
        sym_d = "_ZN19ReadingLifeNavMixinD1Ev"; //libnickel 4.6 * _ZN19ReadingLifeNavMixinD1Ev

        if      (!strcmp(arg2, "reading_life")) sym_f = "_ZN19ReadingLifeNavMixin14chooseActivityEv"; //libnickel 4.6 * _ZN19ReadingLifeNavMixin14chooseActivityEv
        else if (!strcmp(arg2, "stats"))        sym_f = "_ZN19ReadingLifeNavMixin5statsEv";           //libnickel 4.6 * _ZN19ReadingLifeNavMixin5statsEv
        else if (!strcmp(arg2, "awards"))       sym_f = "_ZN19ReadingLifeNavMixin6awardsEv";          //libnickel 4.6 * _ZN19ReadingLifeNavMixin6awardsEv
        else if (!strcmp(arg2, "words"))        sym_f = "_ZN19ReadingLifeNavMixin7myWordsEv";         //libnickel 4.6 * _ZN19ReadingLifeNavMixin7myWordsEv
    } else if (!strcmp(arg1, "store")) {
        sym_c = "_ZN13StoreNavMixinC1Ev"; //libnickel 4.6 * _ZN13StoreNavMixinC1Ev
        sym_d = "_ZN13StoreNavMixinD1Ev"; //libnickel 4.6 * _ZN13StoreNavMixinD1Ev

        if      (!strcmp(arg2, "overdrive")) sym_f = "_ZN13StoreNavMixin22overDriveFeaturedListsEv"; //libnickel 4.10.11655 * _ZN13StoreNavMixin22overDriveFeaturedListsEv
        else if (!strcmp(arg2, "search"))    sym_f = "_ZN13StoreNavMixin6searchEv";                  //libnickel 4.6 * _ZN13StoreNavMixin6searchEv
    }

    NM_CHECK(nullptr, sym_c, "unknown category '%s' (in '%s:%s')", arg1, arg1, arg2);
    NM_CHECK(nullptr, sym_d, "destructor not specified (this is a bug)");
    NM_CHECK(nullptr, sym_f, "unknown view '%s' (in '%s:%s')", arg2, arg1, arg2);

    void (*fn_c)(void *_this);
    void (*fn_d)(void *_this);
    void (*fn_f)(void *_this);

    reinterpret_cast<void*&>(fn_c) = dlsym(RTLD_DEFAULT, sym_c);
    reinterpret_cast<void*&>(fn_d) = dlsym(RTLD_DEFAULT, sym_d);
    reinterpret_cast<void*&>(fn_f) = dlsym(RTLD_DEFAULT, sym_f);

    NM_CHECK(nullptr, fn_c, "could not find constructor %s (is your firmware too old?)", sym_c);
    NM_CHECK(nullptr, fn_d, "could not find destructor %s (is your firmware too old?)", sym_d);
    NM_CHECK(nullptr, fn_f, "could not find function %s (is your firmware too old?)", sym_f);
    NM_LOG("c: %s = %p; d: %s = %p; f: %s = %p", sym_c, fn_c, sym_d, fn_d, sym_f, fn_f);

    // HACK: I don't exactly know why this is needed, but without it, most of
    // the LibraryNavMixin ones segfault. On firmware versions before 15505, it
    // must be initialized as a QObject (I don't understand why, though). Before
    // 15505, I used to just declare it as a QObject on the stack like `QObject
    // obj(nullptr);`, but this doesn't work anymore after 15505 because it's
    // too small for the pointer to the LibraryBuilder the LibraryNavMixin now
    // uses. To solve that, we allocate more than enough memory on the stack,
    // then use placement new to initialize the QObject. Note that I'm not
    // calling the QObject destructor (even though we should) since I don't feel
    // comfortable with it because I don't totally understand how the
    // LibraryNavMixin does its initialization, and there isn't a risk of there
    // being signals pointing to this stack variable since the nav mixins don't
    // use signals/slots or keep references to themselves.
    // TODO: Figure out how this actually works.

    void *obj = alloca(512);
    new(obj) QObject();

    fn_c(obj);
    fn_f(obj);
    fn_d(obj);

    return nm_action_result_silent();
}

NM_ACTION_(nickel_setting) {
    enum {
        mode_toggle,
        mode_enable,
        mode_disable,
    } mode;

    char *tmp1 = strdupa(arg); // strsep and strtrim will modify it
    char *arg1 = strtrim(strsep(&tmp1, ":"));
    char *arg2 = strtrim(tmp1);
    NM_CHECK(nullptr, arg2, "could not find a : in the argument");

    if (!strcmp(arg1, "toggle"))
        mode = mode_toggle;
    else if (!strcmp(arg1, "enable"))
        mode = mode_enable;
    else if (!strcmp(arg1, "disable"))
        mode = mode_disable;
    else
        NM_ERR_RET(nullptr, "unknown action '%s' for nickel_setting: expected 'toggle', 'enable', or 'disable'", arg1);

    //libnickel 4.6 * _ZN6Device16getCurrentDeviceEv
    Device *(*Device_getCurrentDevice)();
    NM_ACT_XSYM(Device_getCurrentDevice, "_ZN6Device16getCurrentDeviceEv", "could not dlsym Device::getCurrentDevice");

    //libnickel 4.6 * _ZN8SettingsC2ERK6Deviceb _ZN8SettingsC2ERK6Device
    void *(*Settings_Settings)(Settings*, Device*, bool);
    void *(*Settings_SettingsLegacy)(Settings*, Device*);
    NM_ACT_SYM(Settings_Settings, "_ZN8SettingsC2ERK6Deviceb");
    NM_ACT_SYM(Settings_SettingsLegacy, "_ZN8SettingsC2ERK6Device");
    NM_CHECK(nullptr, Settings_Settings || Settings_SettingsLegacy, "could not dlsym Settings constructor (new and/or old)");

    //libnickel 4.6 * _ZN8SettingsD2Ev
    void *(*Settings_SettingsD)(Settings*);
    NM_ACT_XSYM(Settings_SettingsD, "_ZN8SettingsD2Ev", "could not dlsym Settings destructor");

    // some settings don't have symbols in a usable form, and some are inlined, so we may need to set them directly
    //libnickel 4.6 * _ZN8Settings10getSettingERK7QStringRK8QVariant
    QVariant (*Settings_getSetting)(Settings*, QString const&, QVariant const&); // the last param is the default, also note that this requires a subclass of Settings
    NM_ACT_XSYM(Settings_getSetting, "_ZN8Settings10getSettingERK7QStringRK8QVariant", "could not dlsym Settings::getSetting");

    // ditto
    //libnickel 4.6 * _ZN8Settings11saveSettingERK7QStringRK8QVariantb
    void *(*Settings_saveSetting)(Settings*, QString const&, QVariant const&, bool); // the last param is whether to do a full disk sync immediately (rather than waiting for the kernel to do it)
    NM_ACT_XSYM(Settings_saveSetting, "_ZN8Settings11saveSettingERK7QStringRK8QVariantb", "could not dlsym Settings::saveSetting");

    Device *dev = Device_getCurrentDevice();
    NM_CHECK(nullptr, dev, "could not get shared nickel device pointer");

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

    #define vtable_ptr(x) *reinterpret_cast<void**&>(x)
    #define vtable_target(x) reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(x)+8)

    //libnickel 4.6 * _ZTV8Settings
    void *Settings_vtable = dlsym(RTLD_DEFAULT, "_ZTV8Settings");
    NM_CHECK(nullptr, Settings_vtable, "could not dlsym the vtable for Settings");
    NM_CHECK(nullptr, vtable_ptr(settings) == vtable_target(Settings_vtable), "unexpected vtable layout (expected class to start with a pointer to 8 bytes into the vtable)");

    bool v = mode == mode_disable; // this gets inverted

    if (!strcmp(arg2, "invert") || !strcmp(arg2, "screenshots")) {
        //libnickel 4.6 * _ZTV15FeatureSettings
        void *FeatureSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV15FeatureSettings");
        NM_CHECK(nullptr, FeatureSettings_vtable, "could not dlsym the vtable for FeatureSettings");
        vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

        if (!strcmp(arg2, "invert")) {
            //libnickel 4.6 * _ZN15FeatureSettings12invertScreenEv
            bool (*FeatureSettings_invertScreen)(Settings*);
            NM_ACT_XSYM(FeatureSettings_invertScreen, "_ZN15FeatureSettings12invertScreenEv", "could not dlsym FeatureSettings::invertScreen");

            //libnickel 4.6 * _ZN15FeatureSettings15setInvertScreenEb
            bool (*FeatureSettings_setInvertScreen)(Settings*, bool);
            NM_ACT_XSYM(FeatureSettings_setInvertScreen, "_ZN15FeatureSettings15setInvertScreenEb", "could not dlsym FeatureSettings::setInvertScreen");

            if (mode == mode_toggle) {
                v = FeatureSettings_invertScreen(settings);
                vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);
            }

            FeatureSettings_setInvertScreen(settings, !v);
            vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

            NM_CHECK(nullptr, FeatureSettings_invertScreen(settings) == !v, "failed to set setting");
            vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

            QWidget *w = QApplication::topLevelAt(25, 25);
            NM_LOG("updating top-level window %p after invert", w);
            if (w)
                w->update(); // TODO: figure out how to make it update _after_ the menu item redraws itself
        } else if (!strcmp(arg2, "screenshots")) {
            if (mode == mode_toggle) {
                QVariant v1 = Settings_getSetting(settings, QStringLiteral("Screenshots"), QVariant(false));
                vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);
                v = v1.toBool();
            }

            Settings_saveSetting(settings, QStringLiteral("Screenshots"), QVariant(!v), false);
            vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

            QVariant v2 = Settings_getSetting(settings, QStringLiteral("Screenshots"), QVariant(false));
            vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);
        }
    } else if (!strcmp(arg2, "dark_mode")) {
        void *ReadingSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV15ReadingSettings");
        NM_CHECK(nullptr, ReadingSettings_vtable, "could not dlsym the vtable for ReadingSettings");
        vtable_ptr(settings) = vtable_target(ReadingSettings_vtable);

        //libnickel 4.28.17623 * _ZN15ReadingSettings11getDarkModeEv
        bool (*ReadingSettings__getDarkMode)(Settings*);
        NM_ACT_XSYM(ReadingSettings__getDarkMode, "_ZN15ReadingSettings11getDarkModeEv", "could not dlsym PowerSettings::getDarkMode");

        //libnickel 4.28.17623 * _ZN15ReadingSettings11setDarkModeEb
        bool (*ReadingSettings__setDarkMode)(Settings*, bool);
        NM_ACT_XSYM(ReadingSettings__setDarkMode, "_ZN15ReadingSettings11setDarkModeEb", "could not dlsym ReadingSettings::setDarkMode");

        if (mode == mode_toggle) {
            v = ReadingSettings__getDarkMode(settings);
            vtable_ptr(settings) = vtable_target(ReadingSettings_vtable);
        }

        ReadingSettings__setDarkMode(settings, !v);
        vtable_ptr(settings) = vtable_target(ReadingSettings_vtable);

        NM_CHECK(nullptr, ReadingSettings__getDarkMode(settings) == !v, "failed to set setting");
        vtable_ptr(settings) = vtable_target(ReadingSettings_vtable);

        // if we're currently in the Reading view, we need to refresh the ReadingView

        //libnickel 4.6 * _ZN20MainWindowController14sharedInstanceEv
        MainWindowController *(*MainWindowController__sharedInstance)();
        NM_ACT_XSYM(MainWindowController__sharedInstance, "_ZN20MainWindowController14sharedInstanceEv", "could not dlsym MainWindowController::sharedInstance");

        //libnickel 4.21.15015 * _ZNK20MainWindowController11currentViewEv
        QWidget *(*MainWindowController__currentView)(MainWindowController *_this);
        NM_ACT_XSYM(MainWindowController__currentView, "_ZNK20MainWindowController11currentViewEv", "could not dlsym MainWindowController::currentView");

        MainWindowController *mwc = MainWindowController__sharedInstance();
        NM_CHECK(nullptr, mwc, "could not get MainWindowController shared instance");

        QWidget *cv = MainWindowController__currentView(mwc);
        NM_CHECK(nullptr, cv, "could not get current view from MainWindowController");

        if (cv->objectName() == "ReadingView") {
            QShowEvent ev;
            QApplication::sendEvent(cv, &ev);
        }
    } else if (!strcmp(arg2, "lockscreen")) {
        void *PowerSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV13PowerSettings");
        NM_CHECK(nullptr, PowerSettings_vtable, "could not dlsym the vtable for PowerSettings");
        vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

        //libnickel 4.12.12111 * _ZN13PowerSettings16getUnlockEnabledEv
        bool (*PowerSettings__getUnlockEnabled)(Settings*);
        NM_ACT_XSYM(PowerSettings__getUnlockEnabled, "_ZN13PowerSettings16getUnlockEnabledEv", "could not dlsym PowerSettings::getUnlockEnabled");

        //libnickel 4.12.12111 * _ZN13PowerSettings16setUnlockEnabledEb
        bool (*PowerSettings__setUnlockEnabled)(Settings*, bool);
        NM_ACT_XSYM(PowerSettings__setUnlockEnabled, "_ZN13PowerSettings16setUnlockEnabledEb", "could not dlsym PowerSettings::setUnlockEnabled");

        if (mode == mode_toggle) {
            v = PowerSettings__getUnlockEnabled(settings);
            vtable_ptr(settings) = vtable_target(PowerSettings_vtable);
        }

        PowerSettings__setUnlockEnabled(settings, !v);
        vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

        NM_CHECK(nullptr, PowerSettings__getUnlockEnabled(settings) == !v, "failed to set setting");
        vtable_ptr(settings) = vtable_target(PowerSettings_vtable);
    } else if (!strcmp(arg2, "force_wifi") || !strcmp(arg2, "auto_usb_gadget")) {
        //libnickel 4.6 * _ZTV11DevSettings
        void *PowerSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV11DevSettings");
        NM_CHECK(nullptr, PowerSettings_vtable, "could not dlsym the vtable for DevSettings");
        vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

        const QString st = !strcmp(arg2, "force_wifi")
            ? QStringLiteral("ForceWifiOn")
            : QStringLiteral("AutoUsbGadget");

        if (mode == mode_toggle) {
            QVariant v1 = Settings_getSetting(settings, st, QVariant(false));
            vtable_ptr(settings) = vtable_target(PowerSettings_vtable);
            v = v1.toBool();
        }

        Settings_saveSetting(settings, st, QVariant(!v), false);
        vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

        QVariant v2 = Settings_getSetting(settings, st, QVariant(false));
        vtable_ptr(settings) = vtable_target(PowerSettings_vtable);
    } else {
        // TODO: more settings?
        Settings_SettingsD(settings);
        NM_ERR_RET(nullptr, "unknown setting name '%s' (arg: '%s')", arg2, arg);
    }

    #undef vtable_ptr
    #undef vtable_target

    Settings_SettingsD(settings);

    return (strcmp(arg2, "invert") && strcmp(arg2, "dark_mode")) // invert and dark mode are obvious
        ? nm_action_result_toast("%s %s", v ? "disabled" : "enabled", arg2)
        : nm_action_result_silent();
}

NM_ACTION_(nickel_extras) {
    const char* mimetype;
    if (strchr(arg, '/'))                   mimetype = arg;
    else if (!strcmp(arg, "unblock_it"))    mimetype = "application/x-games-RushHour";
    else if (!strcmp(arg, "sketch_pad"))    mimetype = "application/x-games-Scribble";
    else if (!strcmp(arg, "solitaire"))     mimetype = "application/x-games-Solitaire";
    else if (!strcmp(arg, "sudoku"))        mimetype = "application/x-games-Sudoku";
    else if (!strcmp(arg, "word_scramble")) mimetype = "application/x-games-Boggle";
    else NM_ERR_RET(nullptr, "unknown beta feature name or plugin mimetype '%s'", arg);

    //libnickel 4.6 * _ZN18ExtrasPluginLoader10loadPluginEPKc
    void (*ExtrasPluginLoader_loadPlugin)(const char*);
    NM_ACT_XSYM(ExtrasPluginLoader_loadPlugin, "_ZN18ExtrasPluginLoader10loadPluginEPKc", "could not dlsym ExtrasPluginLoader::loadPlugin");
    ExtrasPluginLoader_loadPlugin(mimetype);

    return nm_action_result_silent();
}

NM_ACTION_(nickel_browser) {
    bool modal;
    QUrl *url;
    QString *css;

    if (!arg || !*arg) {
        modal = false;
        url = new QUrl();
        css = new QString("");
    } else {
        QString tmp = QString::fromUtf8(arg).trimmed();

        if (tmp.section(':', 0, 0).trimmed() == "modal") {
            modal = true;
            tmp = tmp.section(':', 1).trimmed();
        } else {
            modal = false;
        }

        if (tmp.contains(' ')) {
            css = new QString(tmp.section(' ', 1).trimmed());
            url = new QUrl(tmp.section(' ', 0, 0).trimmed(), QUrl::ParsingMode::StrictMode);
            if (!url->isValid() || url->isRelative())
                NM_ERR_RET(nullptr, "invalid url '%s' (argument: '%s') (note: if your url has spaces, they need to be escaped)", qPrintable(tmp.section(' ', 0, 0)), arg);
        } else if (tmp.length()) {
            url = new QUrl(tmp, QUrl::ParsingMode::StrictMode);
            css = new QString("");
            if (!url->isValid() || url->isRelative())
                NM_ERR_RET(nullptr, "invalid url '%s' (argument: '%s') (note: if your url has spaces, they need to be escaped)", qPrintable(tmp.section(' ', 0, 0)), arg);
        } else {
            url = new QUrl();
            css = new QString("");
        }
    }

    NM_LOG("web browser '%s' (modal=%d, url='%s', css='%s')", arg, modal, url->isValid() ? qPrintable(url->toString()) : "(home)", qPrintable(*css));

    //libnickel 4.6 * _ZN22BrowserWorkflowManager14sharedInstanceEv _ZN22BrowserWorkflowManagerC1EP7QObject
    BrowserWorkflowManager *(*BrowserWorkflowManager_sharedInstance)();
    void (*BrowserWorkflowManager_BrowserWorkflowManager)(BrowserWorkflowManager*, QObject*); // 4.11.11911+
    NM_ACT_SYM(BrowserWorkflowManager_sharedInstance, "_ZN22BrowserWorkflowManager14sharedInstanceEv");
    NM_ACT_SYM(BrowserWorkflowManager_BrowserWorkflowManager, "_ZN22BrowserWorkflowManagerC1EP7QObject");
    NM_CHECK(nullptr, BrowserWorkflowManager_sharedInstance || BrowserWorkflowManager_BrowserWorkflowManager, "could not dlsym BrowserWorkflowManager constructor (4.11.11911+) or sharedInstance");

    //libnickel 4.6 * _ZN22BrowserWorkflowManager11openBrowserEbRK4QUrlRK7QString
    void (*BrowserWorkflowManager_openBrowser)(BrowserWorkflowManager*, bool, QUrl*, QString*); // the bool is whether to open it as a modal, the QUrl is the URL to load(if !QUrl::isValid(), it loads the homepage), the string is CSS to inject
    NM_ACT_XSYM(BrowserWorkflowManager_openBrowser, "_ZN22BrowserWorkflowManager11openBrowserEbRK4QUrlRK7QString", "could not dlsym BrowserWorkflowManager::openBrowser");

    // note: everything must be on the heap since if it isn't connected, it
    //       passes it as-is to the connected signal, which will be used
    //       after this action returns.

    BrowserWorkflowManager *bwm;

    if (BrowserWorkflowManager_BrowserWorkflowManager) {
        bwm = calloc(1, 128); // as of 4.20.14622, it's actually 20 bytes, but we're going to stay on the safe side
        NM_CHECK(nullptr, bwm, "could not allocate memory for BrowserWorkflowManager");
        BrowserWorkflowManager_BrowserWorkflowManager(bwm, nullptr);
    } else {
        bwm = BrowserWorkflowManager_sharedInstance();
        NM_CHECK(nullptr, bwm, "could not get shared browser workflow manager pointer");
    }

    BrowserWorkflowManager_openBrowser(bwm, modal, url, css);

    return nm_action_result_silent();
}

NM_ACTION_(nickel_misc) {
    if (!strcmp(arg, "home")) {
        //libnickel 4.6 * _ZN19StatusBarController4homeEv _ZN17MainNavController4homeEv
        void (*StatusBarController_home)(void*);
        void (*MainNavController_home)(void*);

        NM_ACT_SYM(StatusBarController_home, "_ZN19StatusBarController4homeEv");
        NM_ACT_SYM(MainNavController_home, "_ZN17MainNavController4homeEv");
        NM_CHECK(nullptr, StatusBarController_home || MainNavController_home, "could not dlsym StatusBarController::home (pre-4.23.15505) or MainNavController::home (4.23.15505+)");

        // technically, we need an instance, but it isn't used so it doesn't matter (and if it does crash, we can fix it later as it's not critical) as of 15505
        (StatusBarController_home ?: MainNavController_home)(nullptr);
    } else if (!strcmp(arg, "rescan_books")) {
        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager14sharedInstanceEv
        PlugWorkflowManager *(*PlugWorkflowManager_sharedInstance)();
        NM_ACT_XSYM(PlugWorkflowManager_sharedInstance, "_ZN19PlugWorkflowManager14sharedInstanceEv", "could not dlsym PlugWorkflowManager::sharedInstance");

        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager4syncEv
        void (*PlugWorkflowManager_sync)(PlugWorkflowManager*);
        NM_ACT_XSYM(PlugWorkflowManager_sync, "_ZN19PlugWorkflowManager4syncEv", "could not dlsym PlugWorkflowManager::sync");

        PlugWorkflowManager *wf = PlugWorkflowManager_sharedInstance();
        NM_CHECK(nullptr, wf, "could not get shared PlugWorkflowManager pointer");

        PlugWorkflowManager_sync(wf);
    } else if (!strcmp(arg, "rescan_books_full")) {
        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager14sharedInstanceEv
        PlugWorkflowManager *(*PlugWorkflowManager_sharedInstance)();
        NM_ACT_XSYM(PlugWorkflowManager_sharedInstance, "_ZN19PlugWorkflowManager14sharedInstanceEv", "could not dlsym PlugWorkflowManager::sharedInstance");

        // this is what is called by PlugWorkflowManager::plugged after confirmation
        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager18onCancelAndConnectEv
        void (*PlugWorkflowManager_onCancelAndConnect)(PlugWorkflowManager*);
        NM_ACT_XSYM(PlugWorkflowManager_onCancelAndConnect, "_ZN19PlugWorkflowManager18onCancelAndConnectEv", "could not dlsym PlugWorkflowManager::onCancelAndConnect");

        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager9unpluggedEv
        void (*PlugWorkflowManager_unplugged)(PlugWorkflowManager*);
        NM_ACT_XSYM(PlugWorkflowManager_unplugged, "_ZN19PlugWorkflowManager9unpluggedEv", "could not dlsym PlugWorkflowManager::unplugged");

        PlugWorkflowManager *wf = PlugWorkflowManager_sharedInstance();
        NM_CHECK(nullptr, wf, "could not get shared PlugWorkflowManager pointer");

        PlugWorkflowManager_onCancelAndConnect(wf);
        sleep(1);
        PlugWorkflowManager_unplugged(wf);
    } else if (!strcmp(arg, "force_usb_connection")) {
        // we could call libnickel directly, but I prefer not to
        FILE *nhs;
        NM_CHECK(nullptr, (nhs = fopen("/tmp/nickel-hardware-status", "w")), "could not open nickel hardware status pipe: %m");

        const char *msg = "usb plug add";
        NM_CHECK(nullptr, fputs(msg, nhs) >= 0, "could not write message '%s' to pipe: %m", msg);

        fclose(nhs);
    } else {
        NM_ERR_RET(nullptr, "unknown action '%s'", arg);
    }
    return nm_action_result_silent();
}

NM_ACTION_(power) {
    if (!strcmp(arg, "shutdown") || !strcmp(arg, "reboot") || !strcmp(arg, "sleep")) {
        //libnickel 4.13.12638 * _ZN22N3PowerWorkflowManager14sharedInstanceEv
        N3PowerWorkflowManager *(*N3PowerWorkflowManager_sharedInstance)();
        NM_ACT_XSYM(N3PowerWorkflowManager_sharedInstance, "_ZN22N3PowerWorkflowManager14sharedInstanceEv",
            "could not dlsym N3PowerWorkflowManager::sharedInstance, so cannot perform action cleanly (if you must, report a bug and use cmd_spawn instead)");

        N3PowerWorkflowManager *pwm = N3PowerWorkflowManager_sharedInstance();
        NM_CHECK(nullptr, pwm, "could not get shared power manager pointer");

        if (!strcmp(arg, "shutdown")) {
            //libnickel 4.13.12638 * _ZN22N3PowerWorkflowManager8powerOffEb
            void (*N3PowerWorkflowManager_powerOff)(N3PowerWorkflowManager*, bool); // bool is for if it's due to low battery
            NM_ACT_XSYM(N3PowerWorkflowManager_powerOff, "_ZN22N3PowerWorkflowManager8powerOffEb", "could not dlsym N3PowerWorkflowManager::powerOff");

            N3PowerWorkflowManager_powerOff(pwm, false);
            return nm_action_result_toast("Shutting down...");
        } else if (!strcmp(arg, "reboot")) {
            //libnickel 4.13.12638 * _ZN22N3PowerWorkflowManager6rebootEv
            void (*N3PowerWorkflowManager_reboot)(N3PowerWorkflowManager*);
            NM_ACT_XSYM(N3PowerWorkflowManager_reboot, "_ZN22N3PowerWorkflowManager6rebootEv", "could not dlsym N3PowerWorkflowManager::reboot");

            N3PowerWorkflowManager_reboot(pwm);
            return nm_action_result_toast("Rebooting...");
        } else if (!strcmp(arg, "sleep")) {
            //libnickel 4.13.12638 * _ZN22N3PowerWorkflowManager12requestSleepEv
            void (*N3PowerWorkflowManager_requestSleep)(N3PowerWorkflowManager*);
            NM_ACT_XSYM(N3PowerWorkflowManager_requestSleep, "_ZN22N3PowerWorkflowManager12requestSleepEv", "could not dlsym N3PowerWorkflowManager::requestSleep");

            N3PowerWorkflowManager_requestSleep(pwm);
            return nm_action_result_silent();
        }
    } else {
        NM_ERR_RET(nullptr, "unknown power action '%s'", arg);
    }
    return nm_action_result_silent();
}

NM_ACTION_(nickel_wifi) {
    //libnickel 4.6 * _ZN23WirelessWorkflowManager14sharedInstanceEv
    WirelessWorkflowManager *(*WirelessWorkflowManager_sharedInstance)();
    NM_ACT_XSYM(WirelessWorkflowManager_sharedInstance, "_ZN23WirelessWorkflowManager14sharedInstanceEv", "could not dlsym WirelessWorkflowManager::sharedInstance");

    WirelessWorkflowManager *wfm = WirelessWorkflowManager_sharedInstance();
    NM_CHECK(nullptr, wfm, "could not get shared wireless manager pointer");

    if (!strcmp(arg, "autoconnect")) {
        //libnickel 4.6 * _ZN23WirelessWorkflowManager15connectWirelessEbb
        void (*WirelessWorkflowManager_connectWireless)(WirelessWorkflowManager*, bool, bool); // I haven't looked into what the params are for, so I'm just using what the browser uses when opening it
        NM_ACT_XSYM(WirelessWorkflowManager_connectWireless, "_ZN23WirelessWorkflowManager15connectWirelessEbb", "could not dlsym WirelessWorkflowManager::connectWireless");

        WirelessWorkflowManager_connectWireless(wfm, true, false);
    } else if (!strcmp(arg, "autoconnect_silent")) {
        //libnickel 4.6 * _ZN23WirelessWorkflowManager23connectWirelessSilentlyEv
        void (*WirelessWorkflowManager_connectWirelessSilently)(WirelessWorkflowManager*);
        NM_ACT_XSYM(WirelessWorkflowManager_connectWirelessSilently, "_ZN23WirelessWorkflowManager23connectWirelessSilentlyEv", "could not dlsym WirelessWorkflowManager::connectWirelessSilently");

        WirelessWorkflowManager_connectWirelessSilently(wfm);
    } else if (!strcmp(arg, "enable") || !strcmp(arg, "disable") || !strcmp(arg, "toggle")) {
        //libnickel 4.6 * _ZN23WirelessWorkflowManager14isAirplaneModeEv
        bool (*WirelessWorkflowManager_isAirplaneMode)(WirelessWorkflowManager*);
        NM_ACT_XSYM(WirelessWorkflowManager_isAirplaneMode, "_ZN23WirelessWorkflowManager14isAirplaneModeEv", "could not dlsym WirelessWorkflowManager::isAirplaneMode");

        bool e = WirelessWorkflowManager_isAirplaneMode(wfm);
        NM_LOG("wifi disabled: %d", e);

        //libnickel 4.6 * _ZN23WirelessWorkflowManager15setAirplaneModeEb
        void (*WirelessWorkflowManager_setAirplaneMode)(WirelessWorkflowManager*, bool);
        NM_ACT_XSYM(WirelessWorkflowManager_setAirplaneMode, "_ZN23WirelessWorkflowManager15setAirplaneModeEb", "could not dlsym WirelessWorkflowManager::setAirplaneMode");

        if (!strcmp(arg, "enable")) {
            if (e)
                WirelessWorkflowManager_setAirplaneMode(wfm, false);
        } else if (!strcmp(arg, "disable")) {
            if (!e)
                WirelessWorkflowManager_setAirplaneMode(wfm, true);
        } else if (!strcmp(arg, "toggle")) {
            WirelessWorkflowManager_setAirplaneMode(wfm, !e);
        }
    } else {
        NM_ERR_RET(nullptr, "unknown wifi action '%s'", arg);
    }

    return nm_action_result_silent();
}

NM_ACTION_(nickel_orientation) {
    Qt::ScreenOrientation o;
    if      (!strcmp(arg, "portrait"))           o = Qt::PortraitOrientation;
    else if (!strcmp(arg, "landscape"))          o = Qt::LandscapeOrientation;
    else if (!strcmp(arg, "inverted_portrait"))  o = Qt::InvertedPortraitOrientation;
    else if (!strcmp(arg, "inverted_landscape")) o = Qt::InvertedLandscapeOrientation;
    else if (!strcmp(arg, "invert") || !strcmp(arg, "swap")) {
        switch ((o = QGuiApplication::primaryScreen()->orientation())) {
        case Qt::PrimaryOrientation:           NM_ERR_RET(nullptr, "could not get current screen orientation");                         break;
        case Qt::PortraitOrientation:          o = !strcmp(arg, "invert") ? Qt::InvertedPortraitOrientation : Qt::LandscapeOrientation; break;
        case Qt::LandscapeOrientation:         o = !strcmp(arg, "invert") ? Qt::InvertedLandscapeOrientation : Qt::PortraitOrientation; break;
        case Qt::InvertedPortraitOrientation:  o = !strcmp(arg, "invert") ? Qt::PortraitOrientation : Qt::InvertedLandscapeOrientation; break;
        case Qt::InvertedLandscapeOrientation: o = !strcmp(arg, "invert") ? Qt::LandscapeOrientation : Qt::InvertedPortraitOrientation; break;
        default:                               NM_ERR_RET(nullptr, "unknown screen orientation %d", o);                                 break;
        }
    }
    else NM_ERR_RET(nullptr, "unknown nickel_orientation action '%s'", arg);

    // ---

    //libnickel 4.6 * _ZN22QWindowSystemInterface29handleScreenOrientationChangeEP7QScreenN2Qt17ScreenOrientationE
    void (*QWindowSystemInterface_handleScreenOrientationChange)(QScreen*, Qt::ScreenOrientation);
    NM_ACT_XSYM(QWindowSystemInterface_handleScreenOrientationChange, "_ZN22QWindowSystemInterface29handleScreenOrientationChangeEP7QScreenN2Qt17ScreenOrientationE", "could not dlsym QWindowSystemInterface::handleScreenOrientationChange (did the way Nickel handles the screen orientation sensor change?)");

    //libnickel 4.6 * _ZN6Device16getCurrentDeviceEv
    Device *(*Device_getCurrentDevice)();
    NM_ACT_XSYM(Device_getCurrentDevice, "_ZN6Device16getCurrentDeviceEv", "could not dlsym Device::getCurrentDevice");

    //libnickel 4.11.11911 * _ZNK6Device20hasOrientationSensorEv
    bool (*Device_hasOrientationSensor)(Device*);
    NM_ACT_XSYM(Device_hasOrientationSensor, "_ZNK6Device20hasOrientationSensorEv", "could not dlsym Device::hasOrientationSensor");

    //libnickel 4.6 * _ZN8SettingsC2ERK6Deviceb _ZN8SettingsC2ERK6Device
    void *(*Settings_Settings)(Settings*, Device*, bool);
    void *(*Settings_SettingsLegacy)(Settings*, Device*);
    NM_ACT_SYM(Settings_Settings, "_ZN8SettingsC2ERK6Deviceb");
    NM_ACT_SYM(Settings_SettingsLegacy, "_ZN8SettingsC2ERK6Device");
    NM_CHECK(nullptr, Settings_Settings || Settings_SettingsLegacy, "could not dlsym Settings constructor (new and/or old)");

    //libnickel 4.6 * _ZN8SettingsD2Ev
    void *(*Settings_SettingsD)(Settings*);
    NM_ACT_XSYM(Settings_SettingsD, "_ZN8SettingsD2Ev", "could not dlsym Settings destructor");

    void *ApplicationSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV19ApplicationSettings");
    NM_CHECK(nullptr, ApplicationSettings_vtable, "could not dlsym the vtable for ApplicationSettings");

    //libnickel 4.13.12638 * _ZN19ApplicationSettings20setLockedOrientationE6QFlagsIN2Qt17ScreenOrientationEE
    bool (*ApplicationSettings_setLockedOrientation)(Settings*, Qt::ScreenOrientation);
    NM_ACT_XSYM(ApplicationSettings_setLockedOrientation, "_ZN19ApplicationSettings20setLockedOrientationE6QFlagsIN2Qt17ScreenOrientationEE", "could not dlsym ApplicationSettings::setLockedOrientation");

    //libnickel 4.13.12638 * _ZN19ApplicationSettings17lockedOrientationEv
    int (*ApplicationSettings_lockedOrientation)(Settings*);
    NM_ACT_XSYM(ApplicationSettings_lockedOrientation, "_ZN19ApplicationSettings17lockedOrientationEv", "could not dlsym ApplicationSettings::lockedOrientation");

    Device *dev = Device_getCurrentDevice();
    NM_CHECK(nullptr, dev, "could not get shared nickel device pointer");

    Settings *settings = alloca(128); // way larger than it is, but better to be safe
    if (Settings_Settings)
        Settings_Settings(settings, dev, false);
    else if (Settings_SettingsLegacy)
        Settings_SettingsLegacy(settings, dev);

    #define vtable_ptr(x) *reinterpret_cast<void**&>(x)
    #define vtable_target(x) reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(x)+8)

    // ---

    // note: these notes were last updated for 4.22.15268, but should remain
    // correct for the forseeable future and should be relevant for all
    // supported versions (4.13.12638+).

    // Prevent the sensor (if active) from changing to a different orientation
    // behind our backs.
    //
    // Without this, the orientation sensor may override our orientation if the
    // sensor is in auto mode, and may invert our orientation if the sensor is
    // in portrait/landscape mode. This would happen the moment the sensor
    // reading is updated, which would be essentially instantly if the device
    // isn't on a perfectly still surface.

    QGuiApplication::primaryScreen()->setOrientationUpdateMask(o);

    // Set the current locked orientation to the new one to ensure our new
    // orientation will be allowed.
    //
    // Without this, our orientation may not have any effect since the
    // orientation set by QWindowSystemInterface::handleScreenOrientationChange
    // is limited by if the orientation's bit is set in [ApplicationSettings]
    // LockedOrientation (note that auto is all of them, and portrait/landscape
    // is the normal and inverted variants) (it's a bitmask of
    // Qt::ScreenOrientation).

    vtable_ptr(settings) = vtable_target(ApplicationSettings_vtable);
    int icon_mask = ApplicationSettings_lockedOrientation(settings);

    vtable_ptr(settings) = vtable_target(ApplicationSettings_vtable);
    ApplicationSettings_setLockedOrientation(settings, o);

    // If the device doesn't have an orientation sensor, the user needs to set
    // the [DeveloperSettings] ForceAllowLandscape setting to true or apply the
    // "Allow rotation on all devices" patch (the patch will also show the
    // built-in rotation menu in the reader and other views). If this is not
    // done, this action will silently fail to work. We can't really check this
    // specifically since kobopatch only changes a few places which call
    // Device::hasOrientationSensor, not the function itself.
    //
    // If the device has an orientation sensor, the user can optionally set the
    // [DeveloperSettings] ForceAllowLandscape setting to true or apply the
    // "Allow rotation on all devices" patch to make the rotation take effect
    // even if the current view doesn't normally allow it.
    //
    // This is because in addition to the previous limitation, if
    // [DeveloperSettings] ForceAllowLandscape if not true, the orientation is
    // limited based on if Device::hasOrientationSensor and the allowed
    // orientations for the current view.

    NM_LOG(Device_hasOrientationSensor(dev)
        ? "nickel_orientation: if this didn't do anything, you may need to set [DeveloperSettings] ForceAllowLandscape=true to allow landscape orientations on all views"
        : "nickel_orientation: if this didn't do anything, you may need to apply the 'Allow rotation on all devices' kobopatch patch, or that you have set [DeveloperSettings] ForceAllowLandscape=true");

    // Set the orientation.
    //
    // This is the only thing which is really required here, but if used alone,
    // the orientation sensor may override it, and the orientation will only be
    // set if it is within the constraints of the current rotation mode of
    // auto/portrait/landscape.
    //
    // Note that this function is actually part of the private Qt QPA API, and
    // Nickel really should be doing this part of the rotation logic (including
    // the sensors) from the libkobo QPA plugin. But, this is how Nickel does
    // it, so we need to do it the same way (this does make it easier for both
    // us and Kobo, I guess, due to how intertwined the orientation stuff is).
    //
    // This is the same function which is called by Nickel's handler for the
    // orientation sensor's reading.

    QWindowSystemInterface_handleScreenOrientationChange(QGuiApplication::primaryScreen(), o);

    // We don't update the status bar rotation icon, since we would need to get
    // ahold of the StatusBarView, and I don't like the current options for
    // doing that in a version-agnostic way: walking the entire QObject tree,
    // hooking the constructor, or using offsets directly to get ahold of the
    // StatusBarView from the StatusBarController from the MainWindowController.
    // I might reconsider this in the future if there is demand for it.

    if ((icon_mask&o) == 0)
        NM_LOG("nickel_orientation: the status bar rotate icon may be out of date (this is expected)");

    // If we ever wanted to add an option for setting to rotation to auto/locked
    // portrait/locked landscape, we'd need to call RotatePopupController
    // ::on{Auto,Portrait,Landscape} on StatusBarView::rotatePopupController.
    // This is how the rotate popup and the dropdown in the reading settings
    // does it.

    // ---

    #undef vtable_ptr
    #undef vtable_target

    Settings_SettingsD(settings);

    return nm_action_result_silent();
}

NM_ACTION_(cmd_spawn) {
    char *tmp = strdup(arg); // strsep and strtrim will modify it
    char *tmp1 = tmp; // so we can still free tmp later
    char *tmp2 = strtrim(strsep(&tmp1, ":")); // get the part before the : into tmp2, if any

    bool quiet = tmp1 && !strcmp(tmp2, "quiet");
    const char *cmd = (tmp1 && quiet)
        ? strtrim(tmp1) // trim the actual command
        : arg; // restore the original arg if there wasn't any option field or if it wasn't "quiet"

    QProcess proc;
    uint64_t pid;
    bool ok = proc.startDetached(
        QStringLiteral("/bin/sh"),
        QStringList(std::initializer_list<QString>{
            QStringLiteral("-c"),
            QString::fromUtf8(cmd),
        }),
        QStringLiteral("/"),
        (qint64*)(&pid)
    );

    free(tmp);

    NM_CHECK(nullptr, ok, "could not start process");
    return quiet
        ? nm_action_result_silent()
        : nm_action_result_toast("Successfully started process with PID %lu.", (unsigned long)(pid));
}

NM_ACTION_(cmd_output) {
    // split the timeout into timeout, put the command into cmd
    char *tmp = strdup(arg);
    char *cmd = tmp;
    char *tmp1 = strtrim(strsep(&cmd, ":")), *tmp2;
    long timeout = strtol(tmp1, &tmp2, 10);
    cmd = strtrim(cmd);
    NM_CHECK(nullptr, *tmp1 && !*tmp2 && timeout > 0 && timeout < 10000, "invalid timeout '%s'", tmp1);

    // parse the quiet option and update cmd if it's specified
    char *tmp3 = strdup(cmd);
    char *tmp4 = tmp3;
    char *tmp5 = strtrim(strsep(&tmp4, ":"));
    bool quiet = tmp4 && !strcmp(tmp5, "quiet");
    if (tmp4 && quiet)
        cmd = strtrim(tmp4); // update cmd to exclude "quiet:"

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

    free(tmp3);
    free(tmp);

    bool ok = proc.waitForFinished(timeout);
    if (!ok) {
        switch (proc.error()) {
        case QProcess::Timedout:
            NM_ERR_RET(nullptr, "could not run process: timed out");
        case QProcess::FailedToStart:
            NM_ERR_RET(nullptr, "could not run process: missing program or wrong permissions");
        case QProcess::Crashed:
            NM_ERR_RET(nullptr, "could not run process: process crashed");
        default:
            NM_ERR_RET(nullptr, "could not run process");
        }
    }

    if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() != 0)
        NM_ERR_RET(nullptr, "could not run process: process exited with status %d", proc.exitCode());

    QString out = proc.readAllStandardOutput();
    if (out.length() > 500)
        out = out.left(500) + "...";

    return quiet
        ? nm_action_result_silent()
        : nm_action_result_msg("%s", qPrintable(Qt::convertFromPlainText(out, Qt::WhiteSpacePre)));
}

NM_ACTION_(nickel_bluetooth) {
    enum BLUETOOTH_ACTION {
        ENABLE  = 0b00001,
        DISABLE = 0b00010,
        TOGGLE  = 0b00100,
        CHECK   = 0b01000,
        SCAN    = 0b10000
    };

    int action = 0;
    if (!strcmp(arg, "enable"))       action |= ENABLE;
    else if (!strcmp(arg, "disable")) action |= DISABLE;
    else if (!strcmp(arg, "toggle"))  action |= TOGGLE;
    else if (!strcmp(arg, "check"))   action |= CHECK;
    else if (!strcmp(arg, "scan"))    action |= SCAN;
    else
        NM_ERR_RET(nullptr, "unknown nickel_bluetooth action '%s'", arg);

    //libnickel 4.34.20097 * _ZN16BluetoothManager14sharedInstanceEv
    BluetoothManager *(*BluetoothManager_sharedInstance)();
    NM_ACT_XSYM(BluetoothManager_sharedInstance, "_ZN16BluetoothManager14sharedInstanceEv", "could not dlsym BluetoothManager::sharedInstance");

    //libnickel 4.34.20097 * _ZNK16BluetoothManager2upEv
    uint (*BluetoothManager_up)(BluetoothManager *);
    NM_ACT_XSYM(BluetoothManager_up, "_ZNK16BluetoothManager2upEv", "could not dlsym BluetoothManager::up");

    //libnickel 4.34.20097 * _ZN16BluetoothManager2onEv
    void (*BluetoothManager_on)(BluetoothManager *);
    NM_ACT_XSYM(BluetoothManager_on, "_ZN16BluetoothManager2onEv", "could not dlsym BluetoothManager::on");

    //libnickel 4.34.20097 * _ZN16BluetoothManager4scanEv
    void (*BluetoothManager_scan)(BluetoothManager *);
    NM_ACT_XSYM(BluetoothManager_scan, "_ZN16BluetoothManager4scanEv", "could not dlsym BluetoothManager::BluetoothManager::scanEv");

    //libnickel 4.34.20097 * _ZN16BluetoothManager8stopScanEv
    void (*BluetoothManager_stopScan)(BluetoothManager *);
    NM_ACT_XSYM(BluetoothManager_stopScan, "_ZN16BluetoothManager8stopScanEv", "could not dlsym BluetoothManager::stopScan");

    //libnickel 4.34.20097 * _ZN16BluetoothManager3offEv
    void (*BluetoothManager_off)(BluetoothManager *);
    NM_ACT_XSYM(BluetoothManager_off, "_ZN16BluetoothManager3offEv", "could not dlsym BluetoothManager::off");

    BluetoothManager *btm = BluetoothManager_sharedInstance();
    NM_CHECK(nullptr, btm, "could not get shared bluetooth manager pointer");

    uint isUp = BluetoothManager_up(btm);
    if (action & TOGGLE)
        action = (action & ~TOGGLE) | (isUp ? DISABLE : ENABLE);

     switch (action) {
        case CHECK:
            return nm_action_result_toast("Bluetooth is %s.", isUp ? "on" : "off");
        case ENABLE:
            BluetoothManager_on(btm);
            BluetoothManager_scan(btm);
            return nm_action_result_toast("Bluetooth turned on.");
        case DISABLE:
            BluetoothManager_stopScan(btm);
            BluetoothManager_off(btm);
            return nm_action_result_toast("Bluetooth turned off.");
        case SCAN:
            BluetoothManager_scan(btm);
            return nm_action_result_toast("Bluetooth scan initiated.");
        default:
            NM_ERR_RET(nullptr, "unknown nickel_bluetooth action '%s'", arg);
            break;
    }
}
