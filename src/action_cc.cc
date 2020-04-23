#include <QProcess>
#include <QString>
#include <QStringList>

#include <initializer_list>

#include <alloca.h>
#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "action_cc.h"
#include "util.h"

typedef void Device;
typedef void Settings;
typedef void PlugWorkflowManager;

extern "C" int nm_action_nickelsetting(const char *arg, char **err_out) {
    #define NM_ERR_RET 1

    Device *(*Device_getCurrentDevice)();
    reinterpret_cast<void*&>(Device_getCurrentDevice) = dlsym(RTLD_DEFAULT, "_ZN6Device16getCurrentDeviceEv");
    NM_ASSERT(Device_getCurrentDevice, "could not dlsym Device::getCurrentDevice");

    void *(*Settings_Settings)(Settings*, Device*, bool);
    reinterpret_cast<void*&>(Settings_Settings) = dlsym(RTLD_DEFAULT, "_ZN8SettingsC2ERK6Deviceb");
    NM_ASSERT(Device_getCurrentDevice, "could not dlsym Settings constructor");

    void *(*Settings_SettingsD)(Settings*);
    reinterpret_cast<void*&>(Settings_SettingsD) = dlsym(RTLD_DEFAULT, "_ZN8SettingsD2Ev");
    NM_ASSERT(Settings_SettingsD, "could not dlsym Settings destructor");

    Device *dev = Device_getCurrentDevice();
    NM_ASSERT(dev, "could not get shared nickel device pointer");

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
    NM_ASSERT(Settings_vtable, "could not dlsym the vtable for Settings");
    NM_ASSERT(vtable_ptr(settings) == vtable_target(Settings_vtable), "unexpected vtable layout (expected class to start with a pointer to 8 bytes into the vtable)");

    if (!strcmp(arg, "invert")) {
        void *FeatureSettings_vtable = dlsym(RTLD_DEFAULT, "_ZTV15FeatureSettings");
        NM_ASSERT(FeatureSettings_vtable, "could not dlsym the vtable for FeatureSettings");
        vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

        bool (*FeatureSettings_invertScreen)(Settings*);
        reinterpret_cast<void*&>(FeatureSettings_invertScreen) = dlsym(RTLD_DEFAULT, "_ZN15FeatureSettings12invertScreenEv");
        NM_ASSERT(FeatureSettings_invertScreen, "could not dlsym FeatureSettings::invertScreen");

        bool (*FeatureSettings_setInvertScreen)(Settings*, bool);
        reinterpret_cast<void*&>(FeatureSettings_setInvertScreen) = dlsym(RTLD_DEFAULT, "_ZN15FeatureSettings15setInvertScreenEb");
        NM_ASSERT(FeatureSettings_setInvertScreen, "could not dlsym FeatureSettings::setInvertScreen");

        bool v = FeatureSettings_invertScreen(settings);
        NM_LOG("invertScreen = %d", v);
        vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

        FeatureSettings_setInvertScreen(settings, !v);
        vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);

        NM_ASSERT(FeatureSettings_invertScreen(settings) == !v, "failed to set setting");
        vtable_ptr(settings) = vtable_target(FeatureSettings_vtable);
    } else {
        // TODO: more settings
        Settings_SettingsD(settings);
        NM_RETURN_ERR("unknown setting name '%s'", arg);
    }

    #undef vtable_ptr
    #undef vtable_target

    Settings_SettingsD(settings);

    NM_RETURN_OK(0);
    #undef NM_ERR_RET
}

extern "C" int nm_action_nickelextras(const char *arg, char **err_out) {
    #define NM_ERR_RET 1

    if (!strcmp(arg, "web_browser")) {
        NM_RETURN_ERR("not implemented yet"); // TODO
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

    NM_RETURN_OK(0);
    #undef NM_ERR_RET
}

extern "C" int nm_action_nickelmisc(const char *arg, char **err_out) {
    #define NM_ERR_RET 1
    if (!strcmp(arg, "rescan_books")) {
        PlugWorkflowManager *(*PlugWorkflowManager_sharedInstance)();
        reinterpret_cast<void*&>(PlugWorkflowManager_sharedInstance) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager14sharedInstanceEv");
        NM_ASSERT(PlugWorkflowManager_sharedInstance, "could not dlsym PlugWorkflowManager::sharedInstance");

        void (*PlugWorkflowManager_unplugged)(PlugWorkflowManager*);
        reinterpret_cast<void*&>(PlugWorkflowManager_unplugged) = dlsym(RTLD_DEFAULT, "_ZN19PlugWorkflowManager9unpluggedEv");
        NM_ASSERT(PlugWorkflowManager_unplugged, "could not dlsym PlugWorkflowManager::unplugged");

        PlugWorkflowManager *wf = PlugWorkflowManager_sharedInstance();
        NM_ASSERT(wf, "could not get shared PlugWorkflowManager pointer");

        PlugWorkflowManager_unplugged(wf);
        // TODO: finish this up
        NM_RETURN_ERR("not completely implemented yet");
    } else if (!strcmp(arg, "force_usb_connection")) {
        FILE *nhs;
        NM_ASSERT((nhs = fopen("/tmp/nickel-hardware-status", "w")), "could not open nickel hardware status pipe: %s", strerror(errno));

        const char *msg = "usb plug add";
        NM_ASSERT(fputs(msg, nhs) >= 0, "could not write message '%s' to pipe: %s", msg, strerror(errno));

        fclose(nhs);
    } else {
        NM_RETURN_ERR("unknown action '%s'", arg);
    }
    NM_RETURN_OK(0);
    #undef NM_ERR_RET
}

// TODO: stop abusing err_out to display messages, maybe add a msg_out arg
// for that kind of thing instead (I've marked those spots by returning 2 for
// now).

extern "C" int nm_action_cmdspawn(const char *arg, char **err_out) {
    #define NM_ERR_RET 1

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

    if (*err_out)
        asprintf(err_out, "Successfully started process with PID %lu.", pid);
    return 2;

    #undef NM_ERR_RET
}

extern "C" int nm_action_cmdoutput(const char *arg, char **err_out) {
    #define NM_ERR_RET 1

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

    if (err_out)
        *err_out = strdup(qPrintable(out));
    return 2;

    #undef NM_ERR_RET
}
