#include <QAction>
#include <QCoreApplication>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include <cstdlib>

#include <NickelHook.h>

#include "config.h"
#include "util.h"

typedef QWidget MenuTextItem; // it's actually a subclass, but we don't need its functionality directly, so we'll stay on the safe side
typedef void MainWindowController;

// AbstractNickelMenuController::createMenuTextItem creates a menu item in the
// given QMenu with the specified label. The first bool parameter adds space to
// the left of the item for a checkbox, and the second bool checks it (see the
// search menu for an example of this being used). IDK what the last parameter
// is for, but I suspect it might be passed to QObject::setObjectName (I haven't
// tested it directly, and it's been empty in the menus I've tried so far).
static MenuTextItem* (*AbstractNickelMenuController_createMenuTextItem)(void*, QMenu*, QString const&, bool, bool, QString const&);

// AbstractNickelMenuController::createAction finishes adding the action to the
// menu.
// First bool is whether to close the menu on tap (false: keep it open)
// Second bool is whether the entry is enabled (false: grayed out)
// Third bool is whether to add the default separator after the entry (false: no separator)
//     Note that, even in the Main Menu, it'll inherit the color of a Reader Menu separator (#66),
//     instead of the usual dim (#BB) or solid (#00) ones seen in the stock Main Menu.
// The code is basically:
//     act = new QWidgetAction(this, arg2);
//     act->setEnabled(arg4);
//     if (arg3)
//         connect(act, &QAction::triggered, &QWidget::hide, arg1);
//     arg1->addAction(act);
//     if (arg5)
//         arg1->addSeparator();
static QAction* (*AbstractNickelMenuController_createAction)(void*, QMenu*, QWidget*, bool, bool, bool);

// ConfirmationDialogFactory::showOKDialog does what it says, with the provided
// title and body text. Note that it should be called from the GUI thread (or
// a signal handler).
static void (*ConfirmationDialogFactory_showOKDialog)(QString const&, QString const&);

MainWindowController *(*MainWindowController_sharedInstance)();

// MainWindowController::toast shows a message (primary and secondary text) as
// an overlay for a number of milliseconds. It should also be called from the
// GUI thread.
void (*MainWindowController_toast)(MainWindowController*, QString const&, QString const&, int);

// *MenuSeparator::*MenuSeparator initializes a light main menu separator which
// can be added to the menu with QWidget::addAction. It should be passed at
// least 8 bytes (as of 14622, but should give more to be safe) malloc'd, and
// the menu as the parent. It initializes a QAction.
static void (*LightMenuSeparator_LightMenuSeparator)(void*, QWidget*);
static void (*BoldMenuSeparator_BoldMenuSeparator)(void*, QWidget*);

// TimeLabel::TimeLabel initializes the clock in the status bar on the main and
// reader views. The QWidget is the parent widget.
void (*TimeLabel_TimeLabel)(void*, QWidget*);

void (*TouchLabel_TouchLabel)(void*, QWidget*, QFlags<Qt::WindowType>);

// MainNavView::MainNavView is the new bottom tab bar on 15505+.
void (*MainNavView_MainNavView)(void*, QWidget*);

static struct nh_info NickelMenu = (struct nh_info){
    .name            = "NickelMenu",
    .desc            = "Integrated launcher for Nickel.",
    .uninstall_flag  = NM_CONFIG_DIR "/uninstall",
#ifdef NM_UNINSTALL_CONFIGDIR
    .uninstall_xflag = NM_CONFIG_DIR,
#else
    .uninstall_xflag = NULL,
#endif
    .failsafe_delay  = 2,
};

static struct nh_hook NickelMenuHook[] = {
    // menu injection
    {.sym = "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_", .sym_new = "_nm_menu_hook", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(AbstractNickelMenuController_createMenuTextItem)}, //libnickel 4.6 * _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_

    // trigger for custom main menu after removal in 15505
    {.sym = "_ZN9TimeLabelC1EP7QWidget", .sym_new = "_nm_menu_hook_15505_main_clocktrigger", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(TimeLabel_TimeLabel), .desc = "status bar time label on 15505+", .optional = true},  //libnickel 4.23.15505 * _ZN9TimeLabelC1EP7QWidget

    // null
    {0},
};

static struct nh_dlsym NickelMenuDlsym[] = {
    // menu injection
    {.name = "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_", .out = nh_symoutptr(AbstractNickelMenuController_createMenuTextItem)}, //libnickel 4.6 * _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_
    {.name = "_ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb",                .out = nh_symoutptr(AbstractNickelMenuController_createAction)},       //libnickel 4.6 * _ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb
    {.name = "_ZN25ConfirmationDialogFactory12showOKDialogERK7QStringS2_",                   .out = nh_symoutptr(ConfirmationDialogFactory_showOKDialog)},          //libnickel 4.6 * _ZN25ConfirmationDialogFactory12showOKDialogERK7QStringS2_
    {.name = "_ZN20MainWindowController14sharedInstanceEv",                                  .out = nh_symoutptr(MainWindowController_sharedInstance)},             //libnickel 4.6 * _ZN20MainWindowController14sharedInstanceEv
    {.name = "_ZN20MainWindowController5toastERK7QStringS2_i",                               .out = nh_symoutptr(MainWindowController_toast)},                      //libnickel 4.6 * _ZN20MainWindowController5toastERK7QStringS2_i
    {.name = "_ZN18LightMenuSeparatorC2EP7QWidget",                                          .out = nh_symoutptr(LightMenuSeparator_LightMenuSeparator)},           //libnickel 4.6 * _ZN18LightMenuSeparatorC2EP7QWidget
    {.name = "_ZN17BoldMenuSeparatorC1EP7QWidget",                                           .out = nh_symoutptr(BoldMenuSeparator_BoldMenuSeparator)},             //libnickel 4.6 * _ZN17BoldMenuSeparatorC1EP7QWidget

    // trigger for custom main menu after removal in 15505
    {.name = "_ZN10TouchLabelC1EP7QWidget6QFlagsIN2Qt10WindowTypeEE", .out = nh_symoutptr(TouchLabel_TouchLabel), .desc = "status bar trigger label on 15505+", .optional = true}, //libnickel 4.23.15505 * _ZN10TouchLabelC1EP7QWidget6QFlagsIN2Qt10WindowTypeE

    // feature checks
    {.name = "_ZN11MainNavViewC1EP7QWidget", .out = nh_symoutptr(MainNavView_MainNavView), .desc = "new bottom nav on 15505+", .optional = true}, //libnickel 4.23.15505 * _ZN11MainNavViewC1EP7QWidget

    // null
    {0},
};

static int nm_init();

NickelHook(
    .init  = &nm_init,
    .info  = &NickelMenu,
    .hook  = NickelMenuHook,
    .dlsym = NickelMenuDlsym,
)

// AbstractNickelMenuController_createAction_before wraps
// AbstractNickelMenuController::createAction to use the correct separator for
// the menu location and to match the behaviour of QMenu::insertAction instead
// of QMenu::addAction. It also adds the property nm_action=true to the action
// and separator.
QAction *AbstractNickelMenuController_createAction_before(QAction *before, nm_menu_location_t loc, bool last_in_group, void *_this, QMenu *menu, QWidget *widget, bool close, bool enabled, bool separator);

// nm_menu_item_do runs a nm_menu_item_t and must be called from the thread of a
// signal handler.
static void nm_menu_item_do(nm_menu_item_t *it);

// _nm_menu_inject handles the QMenu::aboutToShow signal and injects menu items.
static void _nm_menu_inject(void *nmc, QMenu *menu, nm_menu_location_t loc, int at);

static int nm_init() {
    #ifdef NM_UNINSTALL_CONFIGDIR
    NM_LOG("feature: NM_UNINSTALL_CONFIGDIR: true");
    #else
    NM_LOG("feature: NM_UNINSTALL_CONFIGDIR: false");
    #endif

    NM_LOG("updating config");

    int rev = nm_global_config_update();
    if (nm_err_peek())
        NM_LOG("... warning: error parsing config, will show a menu item with the error: %s", nm_err());

    size_t ntmp = SIZE_MAX;
    if (rev == -1) {
        NM_LOG("... info: no config file changes detected for initial config update (it should always return an error or update), stopping (this is a bug; err should have been returned instead)");
    } else if (!nm_global_config_items(&ntmp)) {
        NM_LOG("... warning: no menu items returned by nm_global_config_items, ignoring for now (this is a bug; it should always have a menu item whether the default, an error, or the actual config)");
    } else if (ntmp == SIZE_MAX) {
        NM_LOG("... warning: no size returned by nm_global_config_items, ignoring for now (this is a bug)");
    } else if (!ntmp) {
        NM_LOG("... warning: size returned by nm_global_config_items is 0, ignoring for now (this is a bug; it should always have a menu item whether the default, an error, or the actual config)");
    }

    if (MainNavView_MainNavView) {
        if (!TimeLabel_TimeLabel)
            NM_LOG("... warning: clock constructor not found, will not add custom main menu trigger on 15505+, main menu will not be accessible");
        if (!TouchLabel_TouchLabel)
            NM_LOG("... warning: TouchLabel constructor not found, will not add custom main menu trigger on 15505+, main menu will not be accessible");
    }

    return 0;
}

extern "C" __attribute__((visibility("default"))) MenuTextItem* _nm_menu_hook(void* _this, QMenu* menu, QString const& label, bool checkable, bool checked, QString const& thingy) {
    NM_LOG("AbstractNickelMenuController::createMenuTextItem(%p, `%s`, %d, %d, `%s`)", menu, qPrintable(label), checkable, checked, qPrintable(thingy));

    QString trmm = QCoreApplication::translate("StatusBarMenuController", "Settings");
    QString trrm = QCoreApplication::translate("DictionaryActionProxy", "Dictionary");
    QString trbm = QCoreApplication::translate("N3BrowserSettingsMenuController", "Keyboard");
    QString trlm = QCoreApplication::translate("LibraryViewMenuController", "Manage downloads");
    NM_LOG("Comparing against '%s', '%s', '%s', '%s'", qPrintable(trmm), qPrintable(trrm), qPrintable(trbm), qPrintable(trlm));

    nm_menu_location_t loc = {};
    if (label == trmm && !checkable) {
        NM_LOG("Intercepting main menu (label=Settings, checkable=false)...");
        loc = NM_MENU_LOCATION_MAIN_MENU;
    } else if (label == trrm && !checkable) {
        NM_LOG("Intercepting reader menu (label=Dictionary, checkable=false)...");
        loc = NM_MENU_LOCATION_READER_MENU;
    } else if (label == trbm && !checkable) {
        NM_LOG("Intercepting browser menu (label=Keyboard, checkable=false)...");
        loc = NM_MENU_LOCATION_BROWSER_MENU;
    } else if (label == trlm && !checkable) {
        NM_LOG("Intercepting library menu (label=Manage downloads, checkable=false)...");  // this is actually two menus: in "My Books", and in "My Articles"
        loc = NM_MENU_LOCATION_LIBRARY_MENU;
    }

    if (loc)
        QObject::connect(menu, &QMenu::aboutToShow, std::bind(_nm_menu_inject, _this, menu, loc, menu->actions().count()));

    return AbstractNickelMenuController_createMenuTextItem(_this, menu, label, checkable, checked, thingy);
}

extern "C" __attribute__((visibility("default"))) void _nm_menu_hook_15505_main_clocktrigger(void *_this, QWidget *parent) {
    NM_LOG("TimeLabel::TimeLabel(%p, %p)", _this, parent);

    QHBoxLayout *pl;
    QLabel *tmp;
    QPushButton *signalHack;

    if (!TouchLabel_TouchLabel) {
        NM_LOG("Could not find TouchLabel constructor, cannot add trigger for custom main menu.");
        goto _nm_menu_hook_15505_main_clocktrigger_orig;
    }

    if (!parent) {
        NM_LOG("No parent widget provided, cannot add trigger for custom main menu.");
        goto _nm_menu_hook_15505_main_clocktrigger_orig;
    }

    if (parent->objectName() != "statusbarContainer") {
        NM_LOG("Parent widget '%s' != 'statusbarContainer', not adding trigger for custom main menu.", qPrintable(parent->objectName()));
        goto _nm_menu_hook_15505_main_clocktrigger_orig;
    }

    if (!parent->layout()) {
        NM_LOG("Parent widget layout is nullptr, cannot add trigger for custom main menu.");
        goto _nm_menu_hook_15505_main_clocktrigger_orig;
    }

    if (!(pl = qobject_cast<QHBoxLayout*>(parent->layout()))) {
        NM_LOG("Parent widget layout is not a QHBoxLayout, cannot add trigger for custom main menu.");
        goto _nm_menu_hook_15505_main_clocktrigger_orig;
    }

    NM_LOG("%s (size: %dx%d) (layout: %s)", qPrintable(parent->objectName()), parent->width(), parent->height(), parent->layout()->metaObject()->className());

    tmp = reinterpret_cast<QLabel*>(calloc(1, 256)); // way larger than a TouchLabel, but better to be safe
    TouchLabel_TouchLabel(tmp, parent, Qt::Widget);
    tmp->setText(QStringLiteral("NickelMenu"));
    pl->addWidget(tmp);

    // TODO: padding, sizing
    // TODO: figure out why this makes the clock disappear

    signalHack = new QPushButton(nullptr); // HACK: we use a QPushButton as an adaptor so we can connect an old-style signal with the new-style connect witout needing a custom QObject
    if (!QWidget::connect(tmp, SIGNAL(tapped(bool)), signalHack, SIGNAL(clicked(bool)))) {
        NM_LOG("Cannot connect SIGNAL(tapped(bool)) on TouchLabel, cannot add trigger for custom main menu.");
        goto _nm_menu_hook_15505_main_clocktrigger_orig;
    }
    QWidget::connect(signalHack, &QPushButton::clicked, [=] {
        NM_ACTION(test_menu)("");
    });

    NM_LOG("Added trigger for custom main menu.");

_nm_menu_hook_15505_main_clocktrigger_orig:
    TimeLabel_TimeLabel(_this, parent);
}

void _nm_menu_inject(void *nmc, QMenu *menu, nm_menu_location_t loc, int at) {
    NM_LOG("inject %d @ %d", loc, at);

    int rev_o = menu->property("nm_config_rev").toInt();

    NM_LOG("checking for config updates (current revision: %d)", rev_o);
    int rev_n = nm_global_config_update(); // if there was an error it will be returned as a menu item anyways (and updated will be true)
    NM_LOG("new revision = %d%s", rev_n, rev_n == rev_o ? "" : " (changed)");

    NM_LOG("checking for existing items added by nm");

    for (auto action : menu->actions()) {
        if (action->property("nm_action") == true) {
            if (rev_o == rev_n)
                return; // already added items, menu is up to date
            menu->removeAction(action);
            delete action;
        }
    }

    NM_LOG("getting insertion point");

    auto actions = menu->actions();
    auto before = at < actions.count()
        ? actions.at(at)
        : nullptr;

    if (before == nullptr)
        NM_LOG("it seems the original item to add new ones before was never actually added to the menu (number of items when the action was created is %d, current is %d), appending to end instead", at, actions.count());

    NM_LOG("injecting new items");

    size_t items_n;
    nm_menu_item_t **items = nm_global_config_items(&items_n);

    if (!items) {
        NM_LOG("items is NULL (either the config hasn't been parsed yet or there was a memory allocation error), not adding");
        return;
    }

    // if it segfaults in createMenuTextItem, it's likely because
    // AbstractNickelMenuController is invalid, which shouldn't happen while the
    // menu which we added the signal from still can be shown... (but
    // theoretically, it's possible)

    for (size_t i = 0; i < items_n; i++) {
        nm_menu_item_t *it = items[i];
        if (it->loc != loc)
            continue;

        NM_LOG("adding item '%s'...", it->lbl);

        MenuTextItem* item = AbstractNickelMenuController_createMenuTextItem(nmc, menu, QString::fromUtf8(it->lbl), false, false, "");
        QAction* action = AbstractNickelMenuController_createAction_before(before, loc, i == items_n-1, nmc, menu, item, true, true, true);

        QObject::connect(action, &QAction::triggered, [it](bool){
            NM_LOG("item '%s' pressed...", it->lbl);
            nm_menu_item_do(it);
            NM_LOG("done");
        }); // note: we're capturing by value, i.e. the pointer to the global variable, rather then the stack variable, so this is safe
    }

    NM_LOG("updating config revision property");
    menu->setProperty("nm_config_rev", rev_n);
}

void nm_menu_item_do(nm_menu_item_t *it) {
    const char *err = NULL;
    bool success = true;
    int skip = 0;

    for (nm_menu_action_t *cur = it->action; cur; cur = cur->next) {
        NM_LOG("action %p with argument %s : ", cur->act, cur->arg);
        NM_LOG("...success=%d ; on_success=%d on_failure=%d skip=%d", success, cur->on_success, cur->on_failure, skip);

        if (skip != 0) {
            NM_LOG("...skipping action due to skip flag (remaining=%d)", skip);
            if (skip > 0)
                skip--;
            continue;
        } else if (!((success && cur->on_success) || (!success && cur->on_failure))) {
            NM_LOG("...skipping action due to condition flags");
            continue;
        }

        nm_action_result_t *res = cur->act(cur->arg);
        err = nm_err();

        if (err == NULL && res && res->type == NM_ACTION_RESULT_TYPE_SKIP) {
            NM_LOG("...not updating success flag (value=%d) for skip result", success);
        } else if (!(success = err == NULL)) {
            NM_LOG("...error: '%s'", err);
            continue;
        } else if (!res) {
            NM_LOG("...warning: you should have returned a result with type silent, not null, upon success");
            continue;
        }

        NM_LOG("...result: type=%d msg='%s', handling...", res->type, res->msg);

        MainWindowController *mwc;
        switch (res->type) {
        case NM_ACTION_RESULT_TYPE_SILENT:
            break;
        case NM_ACTION_RESULT_TYPE_MSG:
            ConfirmationDialogFactory_showOKDialog(QString::fromUtf8(it->lbl), QLatin1String(res->msg));
            break;
        case NM_ACTION_RESULT_TYPE_TOAST:
            mwc = MainWindowController_sharedInstance();
            if (!mwc) {
                NM_LOG("toast: could not get shared main window controller pointer");
                break;
            }
            MainWindowController_toast(mwc, QLatin1String(res->msg), QStringLiteral(""), 1500);
            break;
        case NM_ACTION_RESULT_TYPE_SKIP:
            skip = res->skip;
            break;
        }

        if (skip == -1)
            NM_LOG("...skipping remaining actions");
        else if (skip != 0)
            NM_LOG("...skipping next %d actions", skip);

        nm_action_result_free(res);
    }

    if (err) {
        NM_LOG("last action returned error %s", err);
        ConfirmationDialogFactory_showOKDialog(QString::fromUtf8(it->lbl), QString::fromUtf8(err));
    }
}

QAction *AbstractNickelMenuController_createAction_before(QAction *before, nm_menu_location_t loc, bool last_in_group, void *_this, QMenu *menu, QWidget *widget, bool close, bool enabled, bool separator) {
    int n = menu->actions().count();
    QAction* action = AbstractNickelMenuController_createAction(_this, menu, widget, /*close*/false, enabled, /*separator*/false);

    action->setProperty("nm_action", true);

    if (!menu->actions().contains(action)) {
        NM_LOG("could not find added action at end of menu (note: old count is %d, new is %d), not moving it to the right spot or adding separator", n, menu->actions().count());
        return action;
    }

    if (before != nullptr) {
        menu->removeAction(action);
        menu->insertAction(before, action);
    }

    if (close) {
        // we can't use the signal which createAction can create, as it gets lost when moving the item
        QWidget::connect(action, &QAction::triggered, [=](bool){ menu->hide(); });
    }

    if (separator) {
        // if it's the main menu, we generally want to use a custom separator
        QAction *sep;
        if (loc == NM_MENU_LOCATION_MAIN_MENU && LightMenuSeparator_LightMenuSeparator && BoldMenuSeparator_BoldMenuSeparator) {
            sep = reinterpret_cast<QAction*>(calloc(1, 32)); // it's actually 8 as of 14622, but better to be safe
            (last_in_group
                ? BoldMenuSeparator_BoldMenuSeparator
                : LightMenuSeparator_LightMenuSeparator
            )(sep, reinterpret_cast<QWidget*>(_this));
            menu->insertAction(before, sep);
        } else {
            sep = menu->insertSeparator(before);
        }
        sep->setProperty("nm_action", true);
    }

    return action;
}

