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

NM_ACTION_(nickel_setting) {
    #define NM_ERR_RET nullptr

    //libnickel 4.6 * _ZN6Device16getCurrentDeviceEv
    Device *(*Device_getCurrentDevice)();
    reinterpret_cast<void*&>(Device_getCurrentDevice) = dlsym(RTLD_DEFAULT, "_ZN6Device16getCurrentDeviceEv");
    NM_ASSERT(Device_getCurrentDevice, "could not dlsym Device::getCurrentDevice");

    //libnickel 4.6 * _ZN8SettingsC2ERK6Deviceb _ZN8SettingsC2ERK6Device
    void *(*Settings_Settings)(Settings*, Device*, bool);
    void *(*Settings_SettingsLegacy)(Settings*, Device*);
    reinterpret_cast<void*&>(Settings_Settings) = dlsym(RTLD_DEFAULT, "_ZN8SettingsC2ERK6Deviceb");
    reinterpret_cast<void*&>(Settings_SettingsLegacy) = dlsym(RTLD_DEFAULT, "_ZN8SettingsC2ERK6Device");
    NM_ASSERT(Settings_Settings || Settings_SettingsLegacy, "could not dlsym Settings constructor (new and/or old)");

    //libnickel 4.6 * _ZN8SettingsD2Ev
    void *(*Settings_SettingsD)(Settings*);
    reinterpret_cast<void*&>(Settings_SettingsD) = dlsym(RTLD_DEFAULT, "_ZN8SettingsD2Ev");
    NM_ASSERT(Settings_SettingsD, "could not dlsym Settings destructor");

    // some settings don't have symbols in a usable form, and some are inlined, so we may need to set them directly
    //libnickel 4.6 * _ZN8Settings10getSettingERK7QStringRK8QVariant
    QVariant (*Settings_getSetting)(Settings*, QString const&, QVariant const&); // the last param is the default, also note that this requires a subclass of Settings
    reinterpret_cast<void*&>(Settings_getSetting) = dlsym(RTLD_DEFAULT, "_ZN8Settings10getSettingERK7QStringRK8QVariant");
    NM_ASSERT(Settings_getSetting, "could not dlsym Settings::getSetting");

    // ditto
    //libnickel 4.6 * _ZN8Settings11saveSettingERK7QStringRK8QVariantb
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

    //libnickel 4.6 * _ZTV8Settings
    void *Settings_vtable = dlsym(RTLD_DEFAULT, "_ZTV8Settings");
    NM_ASSERT(Settings_vtable, "could not dlsym the vtable for Settings");
    NM_ASSERT(vtable_ptr(settings) == vtable_target(Settings_vtable), "unexpected vtable layout (expected class to start with a pointer to 8 bytes into the vtable)");

    bool v = false;

    if (!strcmp(arg, "invert") || !strcmp(arg, "screenshots")) {
        //libnickel 4.6 * _ZTV15FeatureSettings
        void *FeatureSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV15FeatureSettings");
        NM_ASSERT(FeatureSettings_vtable, "could not dlsym the vtable for FeatureSettings");
        vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

        if (!strcmp(arg, "invert")) {
            //libnickel 4.6 * _ZN15FeatureSettings12invertScreenEv
            bool (*FeatureSettings_invertScreen)(Settings*);
            reinterpret_cast<void*&>(FeatureSettings_invertScreen) = dlsym(RTLD_DEFAULT, "_ZN15FeatureSettings12invertScreenEv");
            NM_ASSERT(FeatureSettings_invertScreen, "could not dlsym FeatureSettings::invertScreen");

            //libnickel 4.6 * _ZN15FeatureSettings15setInvertScreenEb
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
            //libnickel 4.12.12111 * _ZN13PowerSettings16getUnlockEnabledEv
            bool (*PowerSettings__getUnlockEnabled)(Settings*);
            reinterpret_cast<void*&>(PowerSettings__getUnlockEnabled) = dlsym(RTLD_DEFAULT, "_ZN13PowerSettings16getUnlockEnabledEv");
            NM_ASSERT(PowerSettings__getUnlockEnabled, "could not dlsym PowerSettings::getUnlockEnabled");

            //libnickel 4.12.12111 * _ZN13PowerSettings16setUnlockEnabledEb
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
        //libnickel 4.6 * _ZTV11DevSettings
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

NM_ACTION_(nickel_extras) {
    #define NM_ERR_RET nullptr

    if (!strcmp(arg, "web_browser")) {
        //libnickel 4.6 * _ZN26N3SettingsExtrasControllerC2Ev
        void (*N3SettingsExtrasController_N3SettingsExtrasController)(N3SettingsExtrasController*);
        reinterpret_cast<void*&>(N3SettingsExtrasController_N3SettingsExtrasController) = dlsym(RTLD_DEFAULT, "_ZN26N3SettingsExtrasControllerC2Ev");
        NM_ASSERT(N3SettingsExtrasController_N3SettingsExtrasController, "could not dlsym N3SettingsExtrasController constructor");

        //libnickel 4.6 * _ZN26N3SettingsExtrasControllerD1Ev
        void (*N3SettingsExtrasController_N3SettingsExtrasControllerD)(N3SettingsExtrasController*);
        reinterpret_cast<void*&>(N3SettingsExtrasController_N3SettingsExtrasControllerD) = dlsym(RTLD_DEFAULT, "_ZN26N3SettingsExtrasControllerD1Ev");
        NM_ASSERT(N3SettingsExtrasController_N3SettingsExtrasControllerD, "could not dlsym N3SettingsExtrasController destructor");

        //libnickel 4.6 * _ZN26N3SettingsExtrasController11openBrowserEv
        void (*N3SettingsExtrasController_openBrowser)(N3SettingsExtrasController*);
        reinterpret_cast<void*&>(N3SettingsExtrasController_openBrowser) = dlsym(RTLD_DEFAULT, "_ZN26N3SettingsExtrasController11openBrowserEv");
        NM_ASSERT(N3SettingsExtrasController_openBrowser, "could not dlsym BrowserWorkflowManager::openBrowser");

        N3SettingsExtrasController *nse = alloca(128); // as of 4.20.14622, it's actually 48 bytes, but we're going to stay on the safe side
        N3SettingsExtrasController_N3SettingsExtrasController(nse);
        N3SettingsExtrasController_openBrowser(nse);
        N3SettingsExtrasController_N3SettingsExtrasControllerD(nse);

        // the QObject is only used to pass events to it, but there's something I'm missing here which leads to it segfaulting after connecting to WiFi if it isn't already connected
        /*
        //libnickel 4.11.11911 * _ZN22BrowserWorkflowManagerC1EP7QObject
        void (*BrowserWorkflowManager_BrowserWorkflowManager)(BrowserWorkflowManager*, QObject*);
        reinterpret_cast<void*&>(BrowserWorkflowManager_BrowserWorkflowManager) = dlsym(RTLD_DEFAULT, "_ZN22BrowserWorkflowManagerC1EP7QObject");
        NM_ASSERT(BrowserWorkflowManager_BrowserWorkflowManager, "could not dlsym BrowserWorkflowManager constructor");

        //libnickel 4.6 * _ZN22BrowserWorkflowManager11openBrowserEbRK4QUrlRK7QString
        void (*BrowserWorkflowManager_openBrowser)(BrowserWorkflowManager*, bool, QUrl const&, QString const&); // the bool is whether to open it as a modal, the string is CSS to inject
        reinterpret_cast<void*&>(BrowserWorkflowManager_openBrowser) = dlsym(RTLD_DEFAULT, "_ZN22BrowserWorkflowManager11openBrowserEbRK4QUrlRK7QString");
        NM_ASSERT(BrowserWorkflowManager_openBrowser, "could not dlsym BrowserWorkflowManager::openBrowser");

        BrowserWorkflowManager *bwm = alloca(128); // as of 4.20.14622, it's actually 20 bytes, but we're going to stay on the safe side
        BrowserWorkflowManager_BrowserWorkflowManager(bwm, new QObject());
        BrowserWorkflowManager_openBrowser(bwm, false, QUrl(), QStringLiteral("")); // if !QUrl::isValid(), it loads the homepage
        */
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

    //libnickel 4.6 * _ZN18ExtrasPluginLoader10loadPluginEPKc
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
        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager14sharedInstanceEv
        PlugWorkflowManager *(*PlugWorkflowManager_sharedInstance)();
        reinterpret_cast<void*&>(PlugWorkflowManager_sharedInstance) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager14sharedInstanceEv");
        NM_ASSERT(PlugWorkflowManager_sharedInstance, "could not dlsym PlugWorkflowManager::sharedInstance");

        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager4syncEv
        void (*PlugWorkflowManager_sync)(PlugWorkflowManager*);
        reinterpret_cast<void*&>(PlugWorkflowManager_sync) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager4syncEv");
        NM_ASSERT(PlugWorkflowManager_sync, "could not dlsym PlugWorkflowManager::sync");

        PlugWorkflowManager *wf = PlugWorkflowManager_sharedInstance();
        NM_ASSERT(wf, "could not get shared PlugWorkflowManager pointer");

        PlugWorkflowManager_sync(wf);
    } else if (!strcmp(arg, "rescan_books_full")) {
        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager14sharedInstanceEv
        PlugWorkflowManager *(*PlugWorkflowManager_sharedInstance)();
        reinterpret_cast<void*&>(PlugWorkflowManager_sharedInstance) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager14sharedInstanceEv");
        NM_ASSERT(PlugWorkflowManager_sharedInstance, "could not dlsym PlugWorkflowManager::sharedInstance");

        // this is what is called by PlugWorkflowManager::plugged after confirmation
        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager18onCancelAndConnectEv
        void (*PlugWorkflowManager_onCancelAndConnect)(PlugWorkflowManager*);
        reinterpret_cast<void*&>(PlugWorkflowManager_onCancelAndConnect) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager18onCancelAndConnectEv");
        NM_ASSERT(PlugWorkflowManager_onCancelAndConnect, "could not dlsym PlugWorkflowManager::onCancelAndConnect");

        //libnickel 4.13.12638 * _ZN19PlugWorkflowManager9unpluggedEv
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
        //libnickel 4.13.12638 * _ZN22N3PowerWorkflowManager14sharedInstanceEv
        N3PowerWorkflowManager *(*N3PowerWorkflowManager_sharedInstance)();
        reinterpret_cast<void*&>(N3PowerWorkflowManager_sharedInstance) = dlsym(RTLD_DEFAULT, "_ZN22N3PowerWorkflowManager14sharedInstanceEv");
        NM_ASSERT(N3PowerWorkflowManager_sharedInstance, "could not dlsym N3PowerWorkflowManager::sharedInstance, so cannot perform action cleanly (if you must, report a bug and use cmd_spawn instead)");

        N3PowerWorkflowManager *pwm = N3PowerWorkflowManager_sharedInstance();
        NM_ASSERT(pwm, "could not get shared power manager pointer");

        if (!strcmp(arg, "shutdown")) {
            //libnickel 4.13.12638 * _ZN22N3PowerWorkflowManager8powerOffEb
            void (*N3PowerWorkflowManager_powerOff)(N3PowerWorkflowManager*, bool); // bool is for if it's due to low battery
            reinterpret_cast<void*&>(N3PowerWorkflowManager_powerOff) = dlsym(RTLD_DEFAULT, "_ZN22N3PowerWorkflowManager8powerOffEb");
            NM_ASSERT(N3PowerWorkflowManager_powerOff, "could not dlsym N3PowerWorkflowManager::powerOff");

            N3PowerWorkflowManager_powerOff(pwm, false);
            NM_RETURN_OK(nm_action_result_toast("Shutting down..."));
        } else if (!strcmp(arg, "reboot")) {
            //libnickel 4.13.12638 * _ZN22N3PowerWorkflowManager6rebootEv
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
    NM_ASSERT(ok, "could not start process");
    NM_RETURN_OK(quiet ? nm_action_result_silent() : nm_action_result_toast("Successfully started process with PID %lu.", (unsigned long)(pid)));
    #undef NM_ERR_RET
}

NM_ACTION_(cmd_output) {
    #define NM_ERR_RET nullptr

    char *tmp = strdup(arg);

    char *cmd = tmp;
    char *tmp1 = strtrim(strsep(&cmd, ":")), *tmp2;
    long timeout = strtol(tmp1, &tmp2, 10);
    cmd = strtrim(cmd);
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
