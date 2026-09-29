#include "action.h"
#include "capabilities.h"
#include "nickel.h"
#include "util.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QScreen>
#include <QSharedPointer>
#include <QShowEvent>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <memory>
#include <new>
#include <sys/stat.h>
#include <unistd.h>

namespace {
using nm6::Nickel;
using nm6::NickelSettings;
using Method = void (*)(void *);

bool split(const char *arg, QString &first, QString &second) {
    const QString text = QString::fromUtf8(arg ? arg : "");
    const qsizetype colon = text.indexOf(':');
    if (colon < 0) { NM_ERR_SET("expected two arguments separated by ':'"); return false; }
    first = text.left(colon).trimmed();
    second = text.mid(colon + 1).trimmed();
    return true;
}
enum class Switch { Enable, Disable, Toggle };
bool switch_mode(const QString &text, Switch &mode) {
    if (text == "enable") mode = Switch::Enable;
    else if (text == "disable") mode = Switch::Disable;
    else if (text == "toggle") mode = Switch::Toggle;
    else { NM_ERR_SET("unknown action '%s': expected enable, disable, or toggle", qPrintable(text)); return false; }
    return true;
}
bool switched(Switch mode, bool current) {
    return mode == Switch::Toggle ? !current : mode == Switch::Enable;
}
void *instance(const Nickel &nickel, const char *symbol) {
    auto get = nickel.resolve<void *(*)()>(symbol);
    if (!get) return nullptr;
    auto *object = get();
    if (!object) NM_ERR_SET("Nickel returned no instance for %s", symbol);
    return object;
}

// These QObject workflows retain asynchronous state. Parent one instance of
// each to the application so Wi-Fi/cloud callbacks never target freed storage.
QObject *browser_workflow(const Nickel &nickel) {
    auto *app = QCoreApplication::instance();
    if (auto *existing = app->findChild<QObject *>(QStringLiteral("nmQt6BrowserWorkflow"), Qt::FindDirectChildrenOnly)) {
        NM_CHECK(nullptr, existing->inherits("BrowserWorkflowManager"), "unexpected browser workflow object");
        return existing;
    }
    auto construct = nickel.resolve<void (*)(void *, QObject *)>("_ZN22BrowserWorkflowManagerC1EP7QObject");
    if (!construct) return nullptr;
    auto *storage = ::operator new(32, std::nothrow);
    if (!storage) { NM_ERR_SET("could not allocate BrowserWorkflowManager"); return nullptr; }
    construct(storage, app);
    auto *object = static_cast<QObject *>(storage);
    object->setObjectName(QStringLiteral("nmQt6BrowserWorkflow"));
    return object;
}
QObject *more_controller(const Nickel &nickel) {
    auto *app = QCoreApplication::instance();
    if (auto *existing = app->findChild<QObject *>(QStringLiteral("nmQt6MoreController"), Qt::FindDirectChildrenOnly)) {
        NM_CHECK(nullptr, existing->inherits("MoreController"), "unexpected cloud workflow object");
        return existing;
    }
    auto construct = nickel.resolve<Method>("_ZN14MoreControllerC1Ev");
    if (!construct) return nullptr;
    auto *storage = ::operator new(96, std::nothrow);
    if (!storage) { NM_ERR_SET("could not allocate MoreController"); return nullptr; }
    construct(storage);
    auto *object = static_cast<QObject *>(storage);
    object->setObjectName(QStringLiteral("nmQt6MoreController"));
    object->setParent(app);
    return object;
}

struct Setting {
    const char *name;
    const char *vtable;
    const char *key;
    const char *get;
    const char *set;
};
constexpr Setting settings[] = {
    {"invert", "_ZTV15FeatureSettings", nullptr, "_ZN15FeatureSettings12invertScreenEv", "_ZN15FeatureSettings15setInvertScreenEb"},
    {"screenshots", "_ZTV15FeatureSettings", "Screenshots", nullptr, nullptr},
    {"dark_mode", "_ZTV15ReadingSettings", nullptr, "_ZN15ReadingSettings11getDarkModeEv", "_ZN15ReadingSettings11setDarkModeEb"},
    {"lockscreen", "_ZTV13PowerSettings", nullptr, "_ZN13PowerSettings16getUnlockEnabledEv", "_ZN13PowerSettings16setUnlockEnabledEb"},
    {"force_wifi", "_ZTV11DevSettings", "ForceWifiOn", nullptr, nullptr},
    {"auto_usb_gadget", "_ZTV11DevSettings", "AutoUsbGadget", nullptr, nullptr},
};
struct Mixin {
    const char *constructor;
    const char *destructor;
};
struct Destination {
    const char *category;
    const char *view;
    const Mixin *mixin;
    const char *method;
};
constexpr Mixin discover{"_ZN16DiscoverNavMixinC1Ev", "_ZN16DiscoverNavMixinD1Ev"};
constexpr Mixin library{"_ZN15LibraryNavMixinC1Ev", "_ZN15LibraryNavMixinD1Ev"};
constexpr Mixin reading_life{"_ZN19ReadingLifeNavMixinC1Ev", "_ZN19ReadingLifeNavMixinD1Ev"};
constexpr Mixin store{"_ZN13StoreNavMixinC1Ev", "_ZN13StoreNavMixinD1Ev"};
constexpr Destination destinations[] = {
    {"discover", "storefront", &discover, "_ZN16DiscoverNavMixin10storefrontEv"},
    {"discover", "wishlist", &discover, "_ZN16DiscoverNavMixin8wishlistEv"},
    {"library", "library", &library, "_ZN15LibraryNavMixin18showLastLibraryTabEv"},
    {"library", "all", &library, "_ZN15LibraryNavMixin23showAllItemsWithoutSyncEv"},
    {"library", "authors", &library, "_ZN15LibraryNavMixin11showAuthorsEv"},
    {"library", "series", &library, "_ZN15LibraryNavMixin10showSeriesEv"},
    {"library", "shelves", &library, "_ZN15LibraryNavMixin11showShelvesEv"},
    {"library", "instapaper", &library, "_ZN15LibraryNavMixin21showInstapaperLibraryEv"},
    {"reading_life", "reading_life", &reading_life, "_ZN19ReadingLifeNavMixin14chooseActivityEv"},
    {"reading_life", "stats", &reading_life, "_ZN19ReadingLifeNavMixin5statsEv"},
    {"reading_life", "words", &reading_life, "_ZN19ReadingLifeNavMixin7myWordsEv"},
    {"store", "overdrive", &store, "_ZN13StoreNavMixin22overDriveFeaturedListsEv"},
    {"store", "search", &store, "_ZN13StoreNavMixin6searchEv"},
};

nm_action_result_t *force_usb() {
    // Executor dispatches this potentially blocking hardware FIFO write to a worker.
    FILE *status = fopen("/tmp/nickel-hardware-status", "w");
    NM_CHECK(nullptr, status, "could not open nickel hardware status pipe: %m");
    if (fputs("usb plug add", status) < 0) {
        const int error = errno;
        fclose(status);
        NM_ERR_RET(nullptr, "could not write USB event to nickel hardware status pipe: %s", strerror(error));
    }
    if (fclose(status) != 0) NM_ERR_RET(nullptr, "could not close nickel hardware status pipe: %m");
    return nm_action_result_silent();
}
}

NM_ACTION_(nickel_setting) {
    QString mode_text, name;
    Switch mode;
    if (!split(arg, mode_text, name) || !switch_mode(mode_text, mode)) return nullptr;
    const Setting *setting = nullptr;
    for (const auto &entry : settings) if (name == QLatin1String(entry.name)) setting = &entry;
    NM_CHECK(nullptr, setting, "unknown setting name '%s'", qPrintable(name));
    Nickel nickel;
    if (!nickel) return nullptr;
    auto get = setting->get ? nickel.resolve<bool (*)(void *)>(setting->get) : nullptr;
    auto set = setting->set ? nickel.resolve<void (*)(void *, bool)>(setting->set) : nullptr;
    if (setting->get && (!get || !set)) return nullptr;
    // Resolve reader refresh before changing the setting.
    auto current = name == "dark_mode"
        ? nickel.resolve<QWidget *(*)(void *)>("_ZNK20MainWindowController11currentViewEv") : nullptr;
    void *window = name == "dark_mode"
        ? instance(nickel, "_ZN20MainWindowController14sharedInstanceEv") : nullptr;
    if (name == "dark_mode" && (!current || !window)) return nullptr;
    NickelSettings object(nickel, setting->vtable);
    if (!object) return nullptr;
    const QString key = QString::fromLatin1(setting->key ? setting->key : "");
    const bool before = get ? get(object.object()) : object.get(key, false).toBool();
    const bool after = switched(mode, before);
    if (set) set(object.object(), after);
    else object.set(key, after);
    NM_CHECK(nullptr, (get ? get(object.object()) : object.get(key, false).toBool()) == after,
             "failed to set '%s'", qPrintable(name));
    if (name == "invert") {
        for (auto *widget : QApplication::topLevelWidgets()) widget->update();
    } else if (name == "dark_mode") {
        auto *view = current(window);
        if (view && view->inherits("ReadingView")) {
            QShowEvent event;
            QApplication::sendEvent(view, &event);
        }
    }
    return name == "invert" || name == "dark_mode"
        ? nm_action_result_silent()
        : nm_action_result_toast("%s %s", after ? "enabled" : "disabled", setting->name);
}

NM_ACTION_(nickel_open) {
    QString category, view;
    if (!split(arg, category, view)) return nullptr;
    if ((category == "library" && view == "pocket") || (category == "reading_life" && view == "awards"))
        NM_ERR_RET(nullptr, "Nickel removed the '%s:%s' destination in Qt 6 firmware", qPrintable(category), qPrintable(view));
    if (category == "library" && (view == "dropbox" || view == "gdrive")) {
        Nickel nickel;
        if (!nickel) return nullptr;
        auto open = nickel.resolve<Method>(view == "dropbox" ? "_ZN14MoreController7dropboxEv" : "_ZN14MoreController11googleDriveEv");
        if (!open) return nullptr;
        auto *controller = more_controller(nickel);
        if (!controller) return nullptr;
        open(controller);
        return nm_action_result_silent();
    }
    const Destination *destination = nullptr;
    for (const auto &entry : destinations)
        if (category == QLatin1String(entry.category) && view == QLatin1String(entry.view)) destination = &entry;
    NM_CHECK(nullptr, destination, "unknown Nickel destination '%s:%s'", qPrintable(category), qPrintable(view));
    Nickel nickel;
    if (!nickel) return nullptr;
    auto construct = nickel.resolve<Method>(destination->mixin->constructor);
    auto destroy = nickel.resolve<Method>(destination->mixin->destructor);
    auto open = nickel.resolve<Method>(destination->method);
    if (!construct || !destroy || !open) return nullptr;
    // NavMixin classes are NOT QObjects. Constructors initialize every field;
    // all are 12 bytes except the 4-byte ReadingLifeNavMixin.
    alignas(void *) unsigned char storage[12]{};
    construct(storage);
    struct Cleanup { Method destroy; void *object; ~Cleanup() { destroy(object); } } cleanup{destroy, storage};
    open(storage);
    return nm_action_result_silent();
}

NM_ACTION_(nickel_browser) {
    QString text = QString::fromUtf8(arg ? arg : "").trimmed();
    const bool modal = text.section(':', 0, 0).trimmed() == "modal";
    if (modal) text = text.mid(text.indexOf(':') + 1).trimmed();
    const qsizetype space = text.indexOf(' ');
    const QString address = space < 0 ? text : text.left(space);
    const QString css = space < 0 ? QString() : text.mid(space + 1).trimmed();
    const QUrl url(address, QUrl::StrictMode);
    NM_CHECK(nullptr, address.isEmpty() || (url.isValid() && !url.isRelative()),
             "invalid absolute URL '%s' (escape spaces in URLs)", qPrintable(address));
    Nickel nickel;
    if (!nickel) return nullptr;
    auto open = nickel.resolve<void (*)(void *, bool, const QUrl &, const QString &, bool)>(
        "_ZN22BrowserWorkflowManager11openBrowserEbRK4QUrlRK7QStringb");
    if (!open) return nullptr;
    auto *manager = browser_workflow(nickel);
    if (!manager) return nullptr;
    // openBrowser copies URL/CSS before connecting Wi-Fi. The trailing bool
    // selects Onleihe styling, not lifetime or modality.
    open(manager, modal, url, css, false);
    return nm_action_result_silent();
}

NM_ACTION_(nickel_extras) {
    NM_ERR_RET(nullptr, "Nickel's extras plugin loader and games were removed in Qt 6 firmware ('%s')", arg);
}

NM_ACTION_(nickel_wifi) {
    const QString action = QString::fromUtf8(arg);
    Switch mode = Switch::Toggle;
    const bool connect = action == "autoconnect" || action == "autoconnect_silent";
    if (!connect && !switch_mode(action, mode)) return nullptr;
    Nickel nickel;
    if (!nickel) return nullptr;
    void *manager = instance(nickel, "_ZN23WirelessWorkflowManager14sharedInstanceEv");
    if (!manager) return nullptr;
    if (action == "autoconnect") {
        auto start = nickel.resolve<void (*)(void *, bool, bool)>("_ZN23WirelessWorkflowManager15connectWirelessEbb");
        if (!start) return nullptr;
        start(manager, true, false);
    } else if (action == "autoconnect_silent") {
        auto start = nickel.resolve<Method>("_ZN23WirelessWorkflowManager23connectWirelessSilentlyEv");
        if (!start) return nullptr;
        start(manager);
    } else {
        auto get = nickel.resolve<bool (*)(void *)>("_ZN23WirelessWorkflowManager14isAirplaneModeEv");
        auto set = nickel.resolve<void (*)(void *, bool)>("_ZN23WirelessWorkflowManager15setAirplaneModeEb");
        if (!get || !set) return nullptr;
        const bool enabled = !get(manager);
        const bool requested = switched(mode, enabled);
        if (enabled != requested) set(manager, !requested);
    }
    return nm_action_result_silent();
}

NM_ACTION_(nickel_bluetooth) {
    const QString action = QString::fromUtf8(arg);
    Switch mode = Switch::Toggle;
    if (action != "check" && action != "scan" && !switch_mode(action, mode)) return nullptr;
    Nickel nickel;
    if (!nickel) return nullptr;
    // ServiceLocator returns std::shared_ptr; getService returns QSharedPointer.
    // Keep both owners alive. IBluetoothService has its QObject base at zero.
    auto locator = nickel.resolve<std::shared_ptr<void> (*)()>("_ZN14ServiceLocator14sharedInstanceEv");
    auto service = nickel.resolve<QSharedPointer<QObject> (*)(const void *)>(
        "_ZNK14ServiceLocator10getServiceI17IBluetoothServiceEEDTcldtcl7declvalIS_EE7resolveIT_EscPS2_LDnEEEv");
    if (!locator || !service) return nullptr;
    auto owner = locator();
    NM_CHECK(nullptr, owner, "Nickel's service locator is unavailable");
    auto bluetooth = service(owner.get());
    NM_CHECK(nullptr, bluetooth && bluetooth->inherits("BluetoothService"), "Nickel's Bluetooth service is unavailable on this device");
    auto up = nickel.resolve<bool (*)(void *)>("_ZNK16BluetoothService2upEv");
    if (!up) return nullptr;
    const bool enabled = up(bluetooth.data());
    if (action == "check") return nm_action_result_toast("Bluetooth is %s.", enabled ? "on" : "off");
    if (action == "scan") {
        auto scan = nickel.resolve<Method>("_ZN16BluetoothManager4scanEv");
        if (!scan) return nullptr;
        void *manager = instance(nickel, "_ZN16BluetoothManager14sharedInstanceEv");
        if (!manager) return nullptr;
        scan(manager);
        return nm_action_result_toast("Bluetooth scan initiated.");
    }
    const bool requested = switched(mode, enabled);
    auto change = nickel.resolve<Method>(requested ? "_ZN16BluetoothService13requestTurnOnEv" : "_ZN16BluetoothService7turnOffEv");
    if (!change) return nullptr;
    if (requested != enabled) change(bluetooth.data());
    return nm_action_result_toast("Bluetooth %s requested.", requested ? "enable" : "disable");
}

NM_ACTION_(nickel_orientation) {
    auto *screen = QGuiApplication::primaryScreen();
    NM_CHECK(nullptr, screen, "no primary screen");
    Qt::ScreenOrientation orientation;
    if (!strcmp(arg, "portrait")) orientation = Qt::PortraitOrientation;
    else if (!strcmp(arg, "landscape")) orientation = Qt::LandscapeOrientation;
    else if (!strcmp(arg, "inverted_portrait")) orientation = Qt::InvertedPortraitOrientation;
    else if (!strcmp(arg, "inverted_landscape")) orientation = Qt::InvertedLandscapeOrientation;
    else if (!strcmp(arg, "invert") || !strcmp(arg, "swap")) {
        const bool invert = !strcmp(arg, "invert");
        switch (screen->orientation()) {
        case Qt::PortraitOrientation: orientation = invert ? Qt::InvertedPortraitOrientation : Qt::LandscapeOrientation; break;
        case Qt::LandscapeOrientation: orientation = invert ? Qt::InvertedLandscapeOrientation : Qt::PortraitOrientation; break;
        case Qt::InvertedPortraitOrientation: orientation = invert ? Qt::PortraitOrientation : Qt::InvertedLandscapeOrientation; break;
        case Qt::InvertedLandscapeOrientation: orientation = invert ? Qt::LandscapeOrientation : Qt::InvertedPortraitOrientation; break;
        default: NM_ERR_RET(nullptr, "could not determine the current screen orientation");
        }
    } else NM_ERR_RET(nullptr, "unknown nickel_orientation action '%s'", arg);
    Nickel nickel;
    if (!nickel) return nullptr;
    auto lock = nickel.resolve<void (*)(void *, Qt::ScreenOrientations)>("_ZN19ApplicationSettings20setLockedOrientationE6QFlagsIN2Qt17ScreenOrientationEE");
    auto change = reinterpret_cast<void (*)(QScreen *, Qt::ScreenOrientation)>(dlsym(RTLD_DEFAULT,
        "_ZN22QWindowSystemInterface29handleScreenOrientationChangeEP7QScreenN2Qt17ScreenOrientationE"));
    NM_CHECK(nullptr, change, "Qt does not provide screen orientation changes");
    if (!lock) return nullptr;
    NickelSettings settings(nickel, "_ZTV19ApplicationSettings");
    if (!settings) return nullptr;
    // Qt 6 removed QScreen::setOrientationUpdateMask. Nickel's persisted lock
    // remains the constraint used by sensor and view orientation handlers.
    lock(settings.object(), Qt::ScreenOrientations(orientation));
    change(screen, orientation);
    NM_LOG("orientation requested: %d; devices/views without rotation support may need ForceAllowLandscape", orientation);
    return nm_action_result_silent();
}

NM_ACTION_(nickel_screenshot) {
    NM_CHECK(nullptr, !strcmp(arg, "capture"), "unknown nickel_screenshot action '%s'", arg);
    Nickel nickel;
    if (!nickel) return nullptr;
    auto construct = nickel.resolve<Method>("_ZN15ScreenshotMakerC1Ev");
    auto capture = nickel.resolve<Method>("_ZN15ScreenshotMaker17saveCurrentScreenEv");
    auto filename = nickel.resolve<QString (*)(void *)>("_ZN15ScreenshotMaker8fileNameEv");
    auto gadget = nickel.resolve<bool (*)(void *)>("_ZNK11PlugManager10gadgetModeEv");
    if (!construct || !capture || !filename || !gadget) return nullptr;
    auto *plug = instance(nickel, "_ZN11PlugManager14sharedInstanceEv");
    if (!plug) return nullptr;
    NM_CHECK(nullptr, !gadget(plug), "screenshots are unavailable while USB gadget mode is active");
    // ScreenshotMaker is a four-byte counter with no destructor. Calling it
    // directly avoids temporarily enabling power-key screenshots.
    int counter = 0;
    construct(&counter);
    capture(&counter);
    const QString path = filename(&counter);
    NM_CHECK(nullptr, QFile::exists(path), "Nickel could not save a screenshot (USB gadget mode may be active)");
    return nm_action_result_silent();
}

nm_action_result_t *nm6::rescan_books_full(std::function<void(bool)> completion) {
    Nickel nickel;
    if (!nickel) return nullptr;
    auto start = nickel.resolve<Method>("_ZN19PlugWorkflowManager18onCancelAndConnectEv");
    auto unplug = nickel.resolve<Method>("_ZN19PlugWorkflowManager9unpluggedEv");
    if (!start || !unplug) return nullptr;
    auto *manager = instance(nickel, "_ZN19PlugWorkflowManager14sharedInstanceEv");
    if (!manager) return nullptr;
    const QPointer<QObject> guard(static_cast<QObject *>(manager));
    start(manager);
    // Allow the synthetic USB view and pending database work to run without
    // sleeping on the GUI thread or entering a nested event loop.
    QTimer::singleShot(1000, QCoreApplication::instance(), [guard, unplug, completion] {
        if (guard) unplug(guard.data());
        if (completion) completion(!guard.isNull());
    });
    return nm_action_result_silent();
}

NM_ACTION_(nickel_misc) {
    if (!strcmp(arg, "force_usb_connection")) return force_usb();
    if (!strcmp(arg, "rescan_books_full")) return nm6::rescan_books_full();
    NM_CHECK(nullptr, !strcmp(arg, "home") || !strcmp(arg, "rescan_books"),
             "unknown nickel_misc action '%s'", arg);
    Nickel nickel;
    if (!nickel) return nullptr;
    if (!strcmp(arg, "home")) {
        auto controller = nickel.resolve<void *(*)(void *)>("_ZNK20MainWindowController17mainNavControllerEv");
        auto home = nickel.resolve<Method>("_ZN17MainNavController4homeEv");
        if (!controller || !home) return nullptr;
        auto *window = instance(nickel, "_ZN20MainWindowController14sharedInstanceEv");
        if (!window) return nullptr;
        auto *navigation = controller(window);
        NM_CHECK(nullptr, navigation, "Nickel's main navigation controller is unavailable");
        home(navigation);
    } else {
        auto start = nickel.resolve<Method>("_ZN19PlugWorkflowManager4syncEv");
        if (!start) return nullptr;
        auto *manager = instance(nickel, "_ZN19PlugWorkflowManager14sharedInstanceEv");
        if (!manager) return nullptr;
        start(manager);
    }
    return nm_action_result_silent();
}

NM_ACTION_(power) {
    NM_CHECK(nullptr, !strcmp(arg, "shutdown") || !strcmp(arg, "reboot") || !strcmp(arg, "sleep"), "unknown power action '%s'", arg);
    Nickel nickel;
    if (!nickel) return nullptr;
    auto *manager = instance(nickel, "_ZN22N3PowerWorkflowManager14sharedInstanceEv");
    if (!manager) return nullptr;
    if (!strcmp(arg, "shutdown")) {
        auto shutdown = nickel.resolve<void (*)(void *, bool)>("_ZN22N3PowerWorkflowManager8powerOffEb");
        if (!shutdown) return nullptr;
        shutdown(manager, false);
    } else {
        auto action = nickel.resolve<Method>(!strcmp(arg, "sleep")
            ? "_ZN22N3PowerWorkflowManager12requestSleepEv" : "_ZN22N3PowerWorkflowManager6rebootEv");
        if (!action) return nullptr;
        action(manager);
    }
    return nm_action_result_silent();
}

NM_ACTION_(uninstall) {
    (void)arg;
    // Retain the resident library and configuration. Resolve clean reboot
    // before persisting the runtime's existing disable marker.
    Nickel nickel;
    if (!nickel) return nullptr;
    auto reboot = nickel.resolve<Method>("_ZN22N3PowerWorkflowManager6rebootEv");
    if (!reboot) return nullptr;
    auto *manager = instance(nickel, "_ZN22N3PowerWorkflowManager14sharedInstanceEv");
    if (!manager) return nullptr;
    NM_CHECK(nullptr, QDir().mkpath(QStringLiteral(NM_CONFIG_DIR)), "could not create configuration directory");
    const int fd = open(NM_CONFIG_DIR "/uninstall", O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0644);
    NM_CHECK(nullptr, fd >= 0, "could not create uninstall flag: %m");
    struct stat st{};
    const bool regular = fstat(fd, &st) == 0 && S_ISREG(st.st_mode);
    const bool flushed = regular && fsync(fd) == 0;
    const int error = errno;
    close(fd);
    NM_CHECK(nullptr, regular, "uninstall flag is not a regular file");
    NM_CHECK(nullptr, flushed, "could not persist uninstall flag: %s", strerror(error));
    const int directory = open(NM_CONFIG_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    NM_CHECK(nullptr, directory >= 0, "could not open configuration directory: %m");
    const bool persisted = fsync(directory) == 0;
    const int directory_error = errno;
    close(directory);
    NM_CHECK(nullptr, persisted, "could not persist configuration directory: %s", strerror(directory_error));
    NM_LOG("Qt6 uninstall: loading disabled; retaining library and configuration; rebooting");
    reboot(manager);
    return nm_action_result_silent();
}
