#include <QAction>
#include <QCoreApplication>
#include <QLayout>
#include <QMenu>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <QWidgetAction>

#include <cstdlib>

#include <NickelHook.h>

#include "action.h"
#include "config.h"
#include "nickelmenu.h"
#include "util.h"

typedef QWidget MenuTextItem; // it's actually a subclass, but we don't need its functionality directly, so we'll stay on the safe side
typedef void MainWindowController;

// AbstractNickelMenuController::createMenuTextItem creates a menu item
// (allocated on the heap). The last parameter has been empty everywhere I've
// seen it. The checked option does not have any effect unless checkable is true
// (see the search menu for an example).
static MenuTextItem* (*AbstractNickelMenuController_createMenuTextItem)(void*, QMenu* menu, QString const& text, bool checkable, bool checked, QString const& /*I think it's for QObject::setObjectName*/);

// AbstractNickelMenuController::createAction finishes adding the action to the
// menu. The actionWidget should be a MenuTextItem (allocated on the heap). If
// disabled is true, the item will appear greyed out. Note that for the
// separator, even in the main menu (removed in FW 15505), it'll inherit the
// color of a Reader Menu separator (#666), rather than the usual dim (#BBB) or
// solid (#000) ones seen in the stock main menu.
//
// The code is basically:
//     act = new QWidgetAction(this, arg2);
//     act->setEnabled(arg4);
//     if (arg3)
//         connect(act, &QAction::triggered, &QWidget::hide, arg1);
//     arg1->addAction(act);
//     if (arg5)
//         arg1->addSeparator();
static QAction* (*AbstractNickelMenuController_createAction)(void*, QMenu* menu, QWidget* actionWidget, bool closeOnTap, bool disabled, bool separatorAfter);

// ConfirmationDialogFactory::showOKDialog shows an dialog box with an OK
// button, and should only be called from the main thread (or a signal handler).
// Note that ConfirmationDialog uses Qt::RichText for the body.
static void (*ConfirmationDialogFactory_showOKDialog)(QString const& title, QString const& body);

MainWindowController *(*MainWindowController_sharedInstance)();

// MainWindowController::toast shows a message as an overlay, and should only be
// called from the main thread (or a signal handler).
void (*MainWindowController_toast)(MainWindowController*, QString const& primary, QString const& secondary, int milliseconds);

// *MenuSeparator::*MenuSeparator initializes a main menu separator which can be
// added to the menu with QWidget::addAction. The menu should be its parent. It
// initializes a QAction.
static void (*LightMenuSeparator_LightMenuSeparator)(void*, QWidget* parent);
static void (*BoldMenuSeparator_BoldMenuSeparator)(void*, QWidget* parent);

// New bottom tab bar which replaced the main menu on 15505+.
typedef QWidget MainNavButton;
typedef QWidget MainNavView;
void (*MainNavView_MainNavView)(MainNavView*, QWidget* parent);
void (*MainNavButton_MainNavButton)(MainNavButton*, QWidget* parent);
void (*MainNavButton_setPixmap)(MainNavButton*, QString const& pixmapName);
void (*MainNavButton_setActivePixmap)(MainNavButton*, QString const& pixmapName);
void (*MainNavButton_setText)(MainNavButton*, QString const& text);
void (*MainNavButton_tapped)(MainNavButton*); // signal

// Creating menus from scratch (for the menu above on 15505+).
typedef QMenu TouchMenu;
typedef TouchMenu NickelTouchMenu;
typedef int DecorationPosition;
void (*NickelTouchMenu_NickelTouchMenu)(NickelTouchMenu*, QWidget* parent, DecorationPosition position);
void (*MenuTextItem_MenuTextItem)(MenuTextItem*, QWidget* parent, bool checkable, bool italic);
void (*MenuTextItem_setText)(MenuTextItem*, QString const& text);
void (*MenuTextItem_registerForTapGestures)(MenuTextItem*);

static struct nh_info NickelMenu = (struct nh_info){
    .name            = "NickelMenu",
    .desc            = "Integrated launcher for Nickel.",
    .uninstall_flag  = NM_CONFIG_DIR "/uninstall",
#ifdef NM_UNINSTALL_CONFIGDIR
    .uninstall_xflag = NM_CONFIG_DIR,
#else
    .uninstall_xflag = NULL,
#endif
    .failsafe_delay  = 3,
};

static struct nh_hook NickelMenuHook[] = {
    // menu injection
    {.sym = "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_", .sym_new = "_nm_menu_hook", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(AbstractNickelMenuController_createMenuTextItem)}, //libnickel 4.6 * _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_

    // bottom nav main menu button injection (15505+)
    {.sym = "_ZN11MainNavViewC1EP7QWidget", .sym_new = "_nm_menu_hook2", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(MainNavView_MainNavView), .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN11MainNavViewC1EP7QWidget

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

    // bottom nav main menu button injection (15505+)
    {.name = "_ZN13MainNavButtonC1EP7QWidget",                       .out = nh_symoutptr(MainNavButton_MainNavButton),         .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButtonC1EP7QWidget
    {.name = "_ZN13MainNavButton9setPixmapERK7QString",              .out = nh_symoutptr(MainNavButton_setPixmap),             .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButton9setPixmapERK7QString
    {.name = "_ZN13MainNavButton15setActivePixmapERK7QString",       .out = nh_symoutptr(MainNavButton_setActivePixmap),       .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButton15setActivePixmapERK7QString
    {.name = "_ZN13MainNavButton7setTextERK7QString",                .out = nh_symoutptr(MainNavButton_setText),               .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButton7setTextERK7QString
    {.name = "_ZN13MainNavButton6tappedEv",                          .out = nh_symoutptr(MainNavButton_tapped),                .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButton6tappedEv
    {.name = "_ZN15NickelTouchMenuC2EP7QWidget18DecorationPosition", .out = nh_symoutptr(NickelTouchMenu_NickelTouchMenu),     .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN15NickelTouchMenuC2EP7QWidget18DecorationPosition
    {.name = "_ZN12MenuTextItemC1EP7QWidgetbb",                      .out = nh_symoutptr(MenuTextItem_MenuTextItem),           .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN12MenuTextItemC1EP7QWidgetbb
    {.name = "_ZN12MenuTextItem7setTextERK7QString",                 .out = nh_symoutptr(MenuTextItem_setText),                .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN12MenuTextItem7setTextERK7QString
    {.name = "_ZN12MenuTextItem22registerForTapGesturesEv",          .out = nh_symoutptr(MenuTextItem_registerForTapGestures), .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN12MenuTextItem22registerForTapGesturesEv

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
        loc = NM_MENU_LOCATION(main);
    } else if (label == trrm && !checkable) {
        NM_LOG("Intercepting reader menu (label=Dictionary, checkable=false)...");
        loc = NM_MENU_LOCATION(reader);
    } else if (label == trbm && !checkable) {
        NM_LOG("Intercepting browser menu (label=Keyboard, checkable=false)...");
        loc = NM_MENU_LOCATION(browser);
    } else if (label == trlm && !checkable) {
        NM_LOG("Intercepting library menu (label=Manage downloads, checkable=false)...");  // this is actually two menus: in "My Books", and in "My Articles"
        loc = NM_MENU_LOCATION(library);
    }

    if (loc)
        QObject::connect(menu, &QMenu::aboutToShow, std::bind(_nm_menu_inject, _this, menu, loc, menu->actions().count()));

    return AbstractNickelMenuController_createMenuTextItem(_this, menu, label, checkable, checked, thingy);
}

extern "C" __attribute__((visibility("default"))) void _nm_menu_hook2(MainNavView *_this, QWidget *parent) {
    NM_LOG("MainNavView::MainNavView(%p, %p)", _this, parent);
    MainNavView_MainNavView(_this, parent);

    NM_LOG("Adding main menu button in tab bar for firmware 4.23.15505+.");

    if (!MainNavButton_MainNavButton || !MainNavButton_setPixmap || !MainNavButton_setActivePixmap || !MainNavButton_setText || !MainNavButton_setText) {
        NM_LOG("Could not find required MainNavButton symbols, cannot add tab button for NickelMenu main menu.");
        return;
    }

    QHBoxLayout *bl = _this->findChild<QHBoxLayout*>();
    if (!bl) {
        NM_LOG("Could not find QHBoxLayout(should contain MainNavButtons and be contained in the QVBoxLayout of MainNavView) in MainNavView, cannot add tab button for NickelMenu main menu.");
        return;
    }

    MainNavButton *btn = reinterpret_cast<MainNavButton*>(calloc(1, 256));
    if (!btn) { // way larger than a MainNavButton, but better to be safe
        NM_LOG("Failed to allocate memory for MainNavButton, cannot add tab button for NickelMenu main menu.");
        return;
    }

    MainNavButton_MainNavButton(btn, parent);
    MainNavButton_setPixmap(btn, QStringLiteral(":/images/home/main_nav_more.png"));
    MainNavButton_setActivePixmap(btn, QStringLiteral(":/images/home/main_nav_more_active.png"));
    MainNavButton_setText(btn, nm_global_config_experimental("menu_main_15505_label") ?: "NickelMenu");
    btn->setObjectName("nmButton");

    QPushButton *sh = new QPushButton(_this); // HACK: we use a QPushButton as an adaptor so we can connect an old-style signal with the new-style connect without needing a custom QObject
    if (!QWidget::connect(btn, SIGNAL(tapped()), sh, SIGNAL(pressed()))) {
        NM_LOG("Failed to connect SIGNAL(tapped()) on TouchLabel to SIGNAL(pressed()) on the QPushButton shim, cannot add tab button for NickelMenu main menu.");
        return;
    }
    sh->setVisible(false);

    QWidget::connect(sh, &QPushButton::pressed, [btn] {
        if (!NickelTouchMenu_NickelTouchMenu || !MenuTextItem_MenuTextItem || !MenuTextItem_setText || !MenuTextItem_registerForTapGestures) {
            NM_LOG("could not find required NickelTouchMenu and MenuTextItem symbols for generating menu");
            ConfirmationDialogFactory_showOKDialog(QLatin1String("NickelMenu"), QLatin1String("Could not find required NickelTouchMenu and MenuTextItem symbols for generating menu (this is a bug)."));
            return;
        }

        NM_LOG("checking for config updates");
        int rev = nm_global_config_update();
        NM_LOG("revision = %d", rev);

        NM_LOG("building menu");

        size_t items_n;
        nm_menu_item_t **items = nm_global_config_items(&items_n);

        if (!items) {
            NM_LOG("failed to get menu items");
            ConfirmationDialogFactory_showOKDialog(QLatin1String("NickelMenu"), QLatin1String("Failed to get menu items (this might be a bug)."));
            return;
        }

        NickelTouchMenu *menu = reinterpret_cast<NickelTouchMenu*>(calloc(1, 512)); // about 3x larger than the largest menu I've seen in 15505 (most inherit from NickelTouchMenu) to be on the safe side
        if (!menu) {
            NM_LOG("failed to allocate memory for menu");
            ConfirmationDialogFactory_showOKDialog(QLatin1String("NickelMenu"), QLatin1String("Failed to allocate memory for menu."));
            return;
        }

        NickelTouchMenu_NickelTouchMenu(menu, nullptr, 3);

        for (size_t i = 0; i < items_n; i++) {
            nm_menu_item_t *it = items[i];
            if (it->loc != NM_MENU_LOCATION(main))
                continue;

            NM_LOG("adding item '%s'...", it->lbl);

            // based on _ZN23SelectionMenuController18createMenuTextItemEP7QWidgetRK7QString
            // (also see _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_, which seems to do the gestures itself instead of calling registerForTapGestures)

            MenuTextItem *mti = reinterpret_cast<MenuTextItem*>(calloc(1, 256)); // about 3x larger than the 15505 size (92)
            if (!it) {
                NM_LOG("failed to allocate memory for config item");
                menu->deleteLater();
                ConfirmationDialogFactory_showOKDialog(QLatin1String("NickelMenu"), QLatin1String("Failed to allocate memory for menu item."));
                return;
            }

            MenuTextItem_MenuTextItem(mti, menu, false, true);
            MenuTextItem_setText(mti, QString::fromUtf8(it->lbl));
            MenuTextItem_registerForTapGestures(mti); // this only makes the MenuTextItem::tapped signal connect so it highlights on tap, doesn't apply to the QAction::triggered below (which needs another GestureReceiver somewhere)

            // based on _ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb

            QWidgetAction *ac = new QWidgetAction(menu);
            ac->setDefaultWidget(mti);
            ac->setEnabled(true);

            menu->addAction(ac);

            QWidget::connect(ac, &QAction::triggered, menu, &QMenu::hide);

            if (i != items_n-1)
                menu->addSeparator();

            // shim so we don't need to deal with GestureReceiver directly like _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_ does
            // (similar to _ZN23SelectionMenuController11addMenuItemEP17SelectionMenuViewP12MenuTextItemPKc)

            if (!QWidget::connect(mti, SIGNAL(tapped(bool)), ac, SIGNAL(triggered()))) {
                NM_LOG("could not handle touch events for menu item (connection of SIGNAL(tapped(bool)) on MenuTextItem to SIGNAL(triggered()) on QWidgetAction failed)");
                menu->deleteLater();
                ConfirmationDialogFactory_showOKDialog(QLatin1String("NickelMenu"), QLatin1String("Could not attach touch event handlers to menu item (this is a bug)."));
                return;
            }

            // event handler

            QObject::connect(ac, &QAction::triggered, [it](bool) {
                NM_LOG("item '%s' pressed...", it->lbl);
                nm_menu_item_do(it);
                NM_LOG("done");
            }); // note: we're capturing by value, i.e. the pointer to the global variable, rather then the stack variable, so this is safe
        }

        NM_LOG("showing menu");

        QWidget::connect(menu, &QMenu::aboutToHide, menu, &QWidget::deleteLater);

        menu->ensurePolished();
        menu->popup(btn->mapToGlobal(btn->geometry().topRight() - QPoint(0, menu->sizeHint().height())));
    });

    bl->addWidget(btn, 1);
    _this->ensurePolished();

    NM_LOG("Added button.");
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
            ConfirmationDialogFactory_showOKDialog(QString::fromUtf8(it->lbl), QString::fromUtf8(res->msg));
            break;
        case NM_ACTION_RESULT_TYPE_TOAST:
            mwc = MainWindowController_sharedInstance();
            if (!mwc) {
                NM_LOG("toast: could not get shared main window controller pointer");
                break;
            }
            MainWindowController_toast(mwc, QString::fromUtf8(res->msg), QStringLiteral(""), 1500);
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
        if (loc == NM_MENU_LOCATION(main) && LightMenuSeparator_LightMenuSeparator && BoldMenuSeparator_BoldMenuSeparator) {
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

