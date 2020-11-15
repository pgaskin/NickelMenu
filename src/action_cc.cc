#include <QApplication>
#include <QProcess>
#include <QScreen>
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

NM_ACTION_(nickel_open) {
    char *tmp1 = strdupa(arg); // strsep and strtrim will modify it
    char *arg1 = strtrim(strsep(&tmp1, ":"));
    char *arg2 = strtrim(tmp1);
    NM_CHECK(nullptr, arg2, "could not find a : in the argument");

    //libnickel 4.23.15505 * _ZN11MainNavViewC1EP7QWidget
    if (dlsym(RTLD_DEFAULT, "_ZN11MainNavViewC1EP7QWidget")) {
        NM_LOG("nickel_open: detected firmware >15505 (new nav tab bar), checking special cases");

        if (!strcmp(arg1, "library") && !strcmp(arg2, "dropbox")) {
            //libnickel 4.23.15505 * _ZN14MoreController7dropboxEv
            void (*MoreController_dropbox)(void*);
            reinterpret_cast<void*&>(MoreController_dropbox) = dlsym(RTLD_DEFAULT, "_ZN14MoreController7dropboxEv");
            NM_CHECK(nullptr, MoreController_dropbox, "could not dlsym MoreController::dropbox");

            // technically, we need a MoreController, but it isn't used as of 15505, so it doesn't matter (and if it ever does, it's not going to crash in a critical place)
            MoreController_dropbox(nullptr);

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
    reinterpret_cast<void*&>(Device_getCurrentDevice) = dlsym(RTLD_DEFAULT, "_ZN6Device16getCurrentDeviceEv");
    NM_CHECK(nullptr, Device_getCurrentDevice, "could not dlsym Device::getCurrentDevice");

    //libnickel 4.6 * _ZN8SettingsC2ERK6Deviceb _ZN8SettingsC2ERK6Device
    void *(*Settings_Settings)(Settings*, Device*, bool);
    void *(*Settings_SettingsLegacy)(Settings*, Device*);
    reinterpret_cast<void*&>(Settings_Settings) = dlsym(RTLD_DEFAULT, "_ZN8SettingsC2ERK6Deviceb");
    reinterpret_cast<void*&>(Settings_SettingsLegacy) = dlsym(RTLD_DEFAULT, "_ZN8SettingsC2ERK6Device");
    NM_CHECK(nullptr, Settings_Settings || Settings_SettingsLegacy, "could not dlsym Settings constructor (new and/or old)");

    //libnickel 4.6 * _ZN8SettingsD2Ev
    void *(*Settings_SettingsD)(Settings*);
    reinterpret_cast<void*&>(Settings_SettingsD) = dlsym(RTLD_DEFAULT, "_ZN8SettingsD2Ev");
    NM_CHECK(nullptr, Settings_SettingsD, "could not dlsym Settings destructor");

    // some settings don't have symbols in a usable form, and some are inlined, so we may need to set them directly
    //libnickel 4.6 * _ZN8Settings10getSettingERK7QStringRK8QVariant
    QVariant (*Settings_getSetting)(Settings*, QString const&, QVariant const&); // the last param is the default, also note that this requires a subclass of Settings
    reinterpret_cast<void*&>(Settings_getSetting) = dlsym(RTLD_DEFAULT, "_ZN8Settings10getSettingERK7QStringRK8QVariant");
    NM_CHECK(nullptr, Settings_getSetting, "could not dlsym Settings::getSetting");

    // ditto
    //libnickel 4.6 * _ZN8Settings11saveSettingERK7QStringRK8QVariantb
    void *(*Settings_saveSetting)(Settings*, QString const&, QVariant const&, bool); // the last param is whether to do a full disk sync immediately (rather than waiting for the kernel to do it)
    reinterpret_cast<void*&>(Settings_saveSetting) = dlsym(RTLD_DEFAULT, "_ZN8Settings11saveSettingERK7QStringRK8QVariantb");
    NM_CHECK(nullptr, Settings_saveSetting, "could not dlsym Settings::saveSetting");

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
            reinterpret_cast<void*&>(FeatureSettings_invertScreen) = dlsym(RTLD_DEFAULT, "_ZN15FeatureSettings12invertScreenEv");
            NM_CHECK(nullptr, FeatureSettings_invertScreen, "could not dlsym FeatureSettings::invertScreen");

            //libnickel 4.6 * _ZN15FeatureSettings15setInvertScreenEb
            bool (*FeatureSettings_setInvertScreen)(Settings*, bool);
            reinterpret_cast<void*&>(FeatureSettings_setInvertScreen) = dlsym(RTLD_DEFAULT, "_ZN15FeatureSettings15setInvertScreenEb");
            NM_CHECK(nullptr, FeatureSettings_setInvertScreen, "could not dlsym FeatureSettings::setInvertScreen");

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
    } else if (!strcmp(arg2, "lockscreen")) {
        void *PowerSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV13PowerSettings");
        NM_CHECK(nullptr, PowerSettings_vtable, "could not dlsym the vtable for PowerSettings");
        vtable_ptr(settings) = vtable_target(PowerSettings_vtable);

        //libnickel 4.12.12111 * _ZN13PowerSettings16getUnlockEnabledEv
        bool (*PowerSettings__getUnlockEnabled)(Settings*);
        reinterpret_cast<void*&>(PowerSettings__getUnlockEnabled) = dlsym(RTLD_DEFAULT, "_ZN13PowerSettings16getUnlockEnabledEv");
        NM_CHECK(nullptr, PowerSettings__getUnlockEnabled, "could not dlsym PowerSettings::getUnlockEnabled");

        //libnickel 4.12.12111 * _ZN13PowerSettings16setUnlockEnabledEb
        bool (*PowerSettings__setUnlockEnabled)(Settings*, bool);
        reinterpret_cast<void*&>(PowerSettings__setUnlockEnabled) = dlsym(RTLD_DEFAULT, "_ZN13PowerSettings16setUnlockEnabledEb");
        NM_CHECK(nullptr, PowerSettings__setUnlockEnabled, "could not dlsym PowerSettings::setUnlockEnabled");

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
        NM_ERR_RET(nullptr, "unknown setting name '%s' (arg: '%s')", arg1, arg);
    }

    #undef vtable_ptr
    #undef vtable_target

    Settings_SettingsD(settings);

    return strcmp(arg2, "invert") // invert is obvious
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
    reinterpret_cast<void*&>(ExtrasPluginLoader_loadPlugin) = dlsym(RTLD_DEFAULT, "_ZN18ExtrasPluginLoader10loadPluginEPKc");
    NM_CHECK(nullptr, ExtrasPluginLoader_loadPlugin, "could not dlsym ExtrasPluginLoader::loadPlugin");
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
    reinterpret_cast<void*&>(BrowserWorkflowManager_sharedInstance) = dlsym(RTLD_DEFAULT, "_ZN22BrowserWorkflowManager14sharedInstanceEv");
    reinterpret_cast<void*&>(BrowserWorkflowManager_BrowserWorkflowManager) = dlsym(RTLD_DEFAULT, "_ZN22BrowserWorkflowManagerC1EP7QObject");
    NM_CHECK(nullptr, BrowserWorkflowManager_sharedInstance || BrowserWorkflowManager_BrowserWorkflowManager, "could not dlsym BrowserWorkflowManager constructor (4.11.11911+) or sharedInstance");

    //libnickel 4.6 * _ZN22BrowserWorkflowManager11openBrowserEbRK4QUrlRK7QString
    void (*BrowserWorkflowManager_openBrowser)(BrowserWorkflowManager*, bool, QUrl*, QString*); // the bool is whether to open it as a modal, the QUrl is the URL to load(if !QUrl::isValid(), it loads the homepage), the string is CSS to inject
    reinterpret_cast<void*&>(BrowserWorkflowManager_openBrowser) = dlsym(RTLD_DEFAULT, "_ZN22BrowserWorkflowManager11openBrowserEbRK4QUrlRK7QString");
    NM_CHECK(nullptr, BrowserWorkflowManager_openBrowser, "could not dlsym BrowserWorkflowManager::openBrowser");

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

        reinterpret_cast<void*&>(StatusBarController_home) = dlsym(RTLD_DEFAULT, "_ZN19StatusBarController4homeEv");
        reinterpret_cast<void*&>(MainNavController_home) = dlsym(RTLD_DEFAULT, "_ZN17MainNavController4homeEv");
        NM_CHECK(nullptr, StatusBarController_home || MainNavController_home, "could not dlsym StatusBarController::home (pre-4.23.15505) or MainNavController::home (4.23.15505+)");

        // technically, we need an instance, but it isn't used so it doesn't matter (and if it does crash, we can fix it later as it's not critical) as of 15505
        (StatusBarController_home ?: MainNavController_home)(nullptr);
    } else if (!strcmp(arg, "rescan_books")) {
        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager14sharedInstanceEv
        PlugWorkflowManager *(*PlugWorkflowManager_sharedInstance)();
        reinterpret_cast<void*&>(PlugWorkflowManager_sharedInstance) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager14sharedInstanceEv");
        NM_CHECK(nullptr, PlugWorkflowManager_sharedInstance, "could not dlsym PlugWorkflowManager::sharedInstance");

        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager4syncEv
        void (*PlugWorkflowManager_sync)(PlugWorkflowManager*);
        reinterpret_cast<void*&>(PlugWorkflowManager_sync) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager4syncEv");
        NM_CHECK(nullptr, PlugWorkflowManager_sync, "could not dlsym PlugWorkflowManager::sync");

        PlugWorkflowManager *wf = PlugWorkflowManager_sharedInstance();
        NM_CHECK(nullptr, wf, "could not get shared PlugWorkflowManager pointer");

        PlugWorkflowManager_sync(wf);
    } else if (!strcmp(arg, "rescan_books_full")) {
        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager14sharedInstanceEv
        PlugWorkflowManager *(*PlugWorkflowManager_sharedInstance)();
        reinterpret_cast<void*&>(PlugWorkflowManager_sharedInstance) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager14sharedInstanceEv");
        NM_CHECK(nullptr, PlugWorkflowManager_sharedInstance, "could not dlsym PlugWorkflowManager::sharedInstance");

        // this is what is called by PlugWorkflowManager::plugged after confirmation
        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager18onCancelAndConnectEv
        void (*PlugWorkflowManager_onCancelAndConnect)(PlugWorkflowManager*);
        reinterpret_cast<void*&>(PlugWorkflowManager_onCancelAndConnect) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager18onCancelAndConnectEv");
        NM_CHECK(nullptr, PlugWorkflowManager_onCancelAndConnect, "could not dlsym PlugWorkflowManager::onCancelAndConnect");

        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager9unpluggedEv
        void (*PlugWorkflowManager_unplugged)(PlugWorkflowManager*);
        reinterpret_cast<void*&>(PlugWorkflowManager_unplugged) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager9unpluggedEv");
        NM_CHECK(nullptr, PlugWorkflowManager_unplugged, "could not dlsym PlugWorkflowManager::unplugged");

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
        reinterpret_cast<void*&>(N3PowerWorkflowManager_sharedInstance) = dlsym(RTLD_DEFAULT, "_ZN22N3PowerWorkflowManager14sharedInstanceEv");
        NM_CHECK(nullptr, N3PowerWorkflowManager_sharedInstance, "could not dlsym N3PowerWorkflowManager::sharedInstance, so cannot perform action cleanly (if you must, report a bug and use cmd_spawn instead)");

        N3PowerWorkflowManager *pwm = N3PowerWorkflowManager_sharedInstance();
        NM_CHECK(nullptr, pwm, "could not get shared power manager pointer");

        if (!strcmp(arg, "shutdown")) {
            //libnickel 4.13.12638 * _ZN22N3PowerWorkflowManager8powerOffEb
            void (*N3PowerWorkflowManager_powerOff)(N3PowerWorkflowManager*, bool); // bool is for if it's due to low battery
            reinterpret_cast<void*&>(N3PowerWorkflowManager_powerOff) = dlsym(RTLD_DEFAULT, "_ZN22N3PowerWorkflowManager8powerOffEb");
            NM_CHECK(nullptr, N3PowerWorkflowManager_powerOff, "could not dlsym N3PowerWorkflowManager::powerOff");

            N3PowerWorkflowManager_powerOff(pwm, false);
            return nm_action_result_toast("Shutting down...");
        } else if (!strcmp(arg, "reboot")) {
            //libnickel 4.13.12638 * _ZN22N3PowerWorkflowManager6rebootEv
            void (*N3PowerWorkflowManager_reboot)(N3PowerWorkflowManager*);
            reinterpret_cast<void*&>(N3PowerWorkflowManager_reboot) = dlsym(RTLD_DEFAULT, "_ZN22N3PowerWorkflowManager6rebootEv");
            NM_CHECK(nullptr, N3PowerWorkflowManager_reboot, "could not dlsym N3PowerWorkflowManager::reboot");

            N3PowerWorkflowManager_reboot(pwm);
            return nm_action_result_toast("Rebooting...");
        } else if (!strcmp(arg, "sleep")) {
            //libnickel 4.13.12638 * _ZN22N3PowerWorkflowManager12requestSleepEv
            void (*N3PowerWorkflowManager_requestSleep)(N3PowerWorkflowManager*);
            reinterpret_cast<void*&>(N3PowerWorkflowManager_requestSleep) = dlsym(RTLD_DEFAULT, "_ZN22N3PowerWorkflowManager12requestSleepEv");
            NM_CHECK(nullptr, N3PowerWorkflowManager_requestSleep, "could not dlsym N3PowerWorkflowManager::requestSleep");

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
    reinterpret_cast<void*&>(WirelessWorkflowManager_sharedInstance) = dlsym(RTLD_DEFAULT, "_ZN23WirelessWorkflowManager14sharedInstanceEv");
    NM_CHECK(nullptr, WirelessWorkflowManager_sharedInstance, "could not dlsym WirelessWorkflowManager::sharedInstance");

    WirelessWorkflowManager *wfm = WirelessWorkflowManager_sharedInstance();
    NM_CHECK(nullptr, wfm, "could not get shared wireless manager pointer");

    if (!strcmp(arg, "autoconnect")) {
        //libnickel 4.6 * _ZN23WirelessWorkflowManager15connectWirelessEbb
        void (*WirelessWorkflowManager_connectWireless)(WirelessWorkflowManager*, bool, bool); // I haven't looked into what the params are for, so I'm just using what the browser uses when opening it
        reinterpret_cast<void*&>(WirelessWorkflowManager_connectWireless) = dlsym(RTLD_DEFAULT, "_ZN23WirelessWorkflowManager15connectWirelessEbb");
        NM_CHECK(nullptr, WirelessWorkflowManager_connectWireless, "could not dlsym WirelessWorkflowManager::connectWireless");

        WirelessWorkflowManager_connectWireless(wfm, true, false);
    } else if (!strcmp(arg, "autoconnect_silent")) {
        //libnickel 4.6 * _ZN23WirelessWorkflowManager23connectWirelessSilentlyEv
        void (*WirelessWorkflowManager_connectWirelessSilently)(WirelessWorkflowManager*);
        reinterpret_cast<void*&>(WirelessWorkflowManager_connectWirelessSilently) = dlsym(RTLD_DEFAULT, "_ZN23WirelessWorkflowManager23connectWirelessSilentlyEv");
        NM_CHECK(nullptr, WirelessWorkflowManager_connectWirelessSilently, "could not dlsym WirelessWorkflowManager::connectWirelessSilently");

        WirelessWorkflowManager_connectWirelessSilently(wfm);
    } else if (!strcmp(arg, "enable") || !strcmp(arg, "disable") || !strcmp(arg, "toggle")) {
        //libnickel 4.6 * _ZN23WirelessWorkflowManager14isAirplaneModeEv
        bool (*WirelessWorkflowManager_isAirplaneMode)(WirelessWorkflowManager*);
        reinterpret_cast<void*&>(WirelessWorkflowManager_isAirplaneMode) = dlsym(RTLD_DEFAULT, "_ZN23WirelessWorkflowManager14isAirplaneModeEv");
        NM_CHECK(nullptr, WirelessWorkflowManager_isAirplaneMode, "could not dlsym WirelessWorkflowManager::isAirplaneMode");

        bool e = WirelessWorkflowManager_isAirplaneMode(wfm);
        NM_LOG("wifi disabled: %d", e);

        //libnickel 4.6 * _ZN23WirelessWorkflowManager15setAirplaneModeEb
        void (*WirelessWorkflowManager_setAirplaneMode)(WirelessWorkflowManager*, bool);
        reinterpret_cast<void*&>(WirelessWorkflowManager_setAirplaneMode) = dlsym(RTLD_DEFAULT, "_ZN23WirelessWorkflowManager15setAirplaneModeEb");
        NM_CHECK(nullptr, WirelessWorkflowManager_setAirplaneMode, "could not dlsym WirelessWorkflowManager::setAirplaneMode");

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
    reinterpret_cast<void*&>(QWindowSystemInterface_handleScreenOrientationChange) = dlsym(RTLD_DEFAULT, "_ZN22QWindowSystemInterface29handleScreenOrientationChangeEP7QScreenN2Qt17ScreenOrientationE");
    NM_CHECK(nullptr, QWindowSystemInterface_handleScreenOrientationChange, "could not dlsym QWindowSystemInterface::handleScreenOrientationChange (did the way Nickel handles the screen orientation sensor change?)");

    //libnickel 4.6 * _ZN6Device16getCurrentDeviceEv
    Device *(*Device_getCurrentDevice)();
    reinterpret_cast<void*&>(Device_getCurrentDevice) = dlsym(RTLD_DEFAULT, "_ZN6Device16getCurrentDeviceEv");
    NM_CHECK(nullptr, Device_getCurrentDevice, "could not dlsym Device::getCurrentDevice");

    //libnickel 4.11.11911 * _ZNK6Device20hasOrientationSensorEv
    bool (*Device_hasOrientationSensor)(Device*);
    reinterpret_cast<void*&>(Device_hasOrientationSensor) = dlsym(RTLD_DEFAULT, "_ZNK6Device20hasOrientationSensorEv");
    NM_CHECK(nullptr, Device_getCurrentDevice, "could not dlsym Device::hasOrientationSensor");

    //libnickel 4.6 * _ZN8SettingsC2ERK6Deviceb _ZN8SettingsC2ERK6Device
    void *(*Settings_Settings)(Settings*, Device*, bool);
    void *(*Settings_SettingsLegacy)(Settings*, Device*);
    reinterpret_cast<void*&>(Settings_Settings) = dlsym(RTLD_DEFAULT, "_ZN8SettingsC2ERK6Deviceb");
    reinterpret_cast<void*&>(Settings_SettingsLegacy) = dlsym(RTLD_DEFAULT, "_ZN8SettingsC2ERK6Device");
    NM_CHECK(nullptr, Settings_Settings || Settings_SettingsLegacy, "could not dlsym Settings constructor (new and/or old)");

    //libnickel 4.6 * _ZN8SettingsD2Ev
    void *(*Settings_SettingsD)(Settings*);
    reinterpret_cast<void*&>(Settings_SettingsD) = dlsym(RTLD_DEFAULT, "_ZN8SettingsD2Ev");
    NM_CHECK(nullptr, Settings_SettingsD, "could not dlsym Settings destructor");

    void *ApplicationSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV19ApplicationSettings");
    NM_CHECK(nullptr, ApplicationSettings_vtable, "could not dlsym the vtable for ApplicationSettings");

    //libnickel 4.13.12638 * _ZN19ApplicationSettings20setLockedOrientationE6QFlagsIN2Qt17ScreenOrientationEE
    bool (*ApplicationSettings_setLockedOrientation)(Settings*, Qt::ScreenOrientation);
    reinterpret_cast<void*&>(ApplicationSettings_setLockedOrientation) = dlsym(RTLD_DEFAULT, "_ZN19ApplicationSettings20setLockedOrientationE6QFlagsIN2Qt17ScreenOrientationEE");
    NM_CHECK(nullptr, ApplicationSettings_setLockedOrientation, "could not dlsym ApplicationSettings::setLockedOrientation");

    //libnickel 4.13.12638 * _ZN19ApplicationSettings17lockedOrientationEv
    int (*ApplicationSettings_lockedOrientation)(Settings*);
    reinterpret_cast<void*&>(ApplicationSettings_lockedOrientation) = dlsym(RTLD_DEFAULT, "_ZN19ApplicationSettings17lockedOrientationEv");
    NM_CHECK(nullptr, ApplicationSettings_lockedOrientation, "could not dlsym ApplicationSettings::lockedOrientation");

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
