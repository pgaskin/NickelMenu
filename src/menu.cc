#include <QAction>
#include <QCoreApplication>
#include <QMenu>
#include <QString>
#include <QWidget>

#include <cstdlib>
#include <dlfcn.h>

#include "dlhook.h"
#include "init.h"
#include "menu.h"
#include "util.h"

typedef QWidget MenuTextItem; // it's actually a subclass, but we don't need its functionality directly, so we'll stay on the safe side
typedef void MainWindowController;

// AbstractNickelMenuController::createMenuTextItem creates a menu item in the
// given QMenu with the specified label. The first bool parameter adds space to
// the left of the item for a checkbox, and the second bool checks it (see the
// search menu for an example of this being used). IDK what the last parameter
// is for, but I suspect it might be passed to QObject::setObjectName (I haven't
// tested it directly, and it's been empty in the menus I've tried so far).
static MenuTextItem* (*AbstractNickelMenuController_createMenuTextItem_orig)(void*, QMenu*, QString const&, bool, bool, QString const&) = NULL;
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

extern "C" int nm_menu_hook(void *libnickel, char **err_out) {
    #define NM_ERR_RET 1
    //libnickel 4.6 * _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_
    reinterpret_cast<void*&>(AbstractNickelMenuController_createMenuTextItem) = dlsym(libnickel, "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_");
    //libnickel 4.6 * _ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb
    reinterpret_cast<void*&>(AbstractNickelMenuController_createAction) = dlsym(libnickel, "_ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb");
    //libnickel 4.6 * _ZN25ConfirmationDialogFactory12showOKDialogERK7QStringS2_
    reinterpret_cast<void*&>(ConfirmationDialogFactory_showOKDialog) = dlsym(libnickel, "_ZN25ConfirmationDialogFactory12showOKDialogERK7QStringS2_");
    //libnickel 4.6 * _ZN20MainWindowController14sharedInstanceEv
    reinterpret_cast<void*&>(MainWindowController_sharedInstance) = dlsym(libnickel, "_ZN20MainWindowController14sharedInstanceEv");
    //libnickel 4.6 * _ZN20MainWindowController5toastERK7QStringS2_i
    reinterpret_cast<void*&>(MainWindowController_toast) = dlsym(libnickel, "_ZN20MainWindowController5toastERK7QStringS2_i");
    //libnickel 4.6 * _ZN18LightMenuSeparatorC2EP7QWidget
    reinterpret_cast<void*&>(LightMenuSeparator_LightMenuSeparator) = dlsym(libnickel, "_ZN18LightMenuSeparatorC2EP7QWidget");
    //libnickel 4.6 * _ZN17BoldMenuSeparatorC1EP7QWidget
    reinterpret_cast<void*&>(BoldMenuSeparator_BoldMenuSeparator) = dlsym(libnickel, "_ZN17BoldMenuSeparatorC1EP7QWidget");

    NM_ASSERT(AbstractNickelMenuController_createMenuTextItem, "unsupported firmware: could not find AbstractNickelMenuController::createMenuTextItem(void* _this, QMenu*, QString, bool, bool, QString const&)");
    NM_ASSERT(AbstractNickelMenuController_createAction, "unsupported firmware: could not find AbstractNickelMenuController::createAction(void* _this, QMenu*, QWidget*, bool, bool, bool)");
    NM_ASSERT(ConfirmationDialogFactory_showOKDialog, "unsupported firmware: could not find ConfirmationDialogFactory::showOKDialog(String const&, QString const&)");
    NM_ASSERT(MainWindowController_sharedInstance, "unsupported firmware: could not find MainWindowController::sharedInstance()");
    NM_ASSERT(MainWindowController_toast, "unsupported firmware: could not find MainWindowController::toast(QString const&, QString const&, int)");
    if (!LightMenuSeparator_LightMenuSeparator)
        NM_LOG("warning: could not find LightMenuSeparator constructor, falling back to generic separators");
    if (!BoldMenuSeparator_BoldMenuSeparator)
        NM_LOG("warning: could not find BoldMenuSeparator constructor, falling back to generic separators");

    void* nmh = dlsym(RTLD_DEFAULT, "_nm_menu_hook");
    NM_ASSERT(nmh, "internal error: could not dlsym _nm_menu_hook");

    char *err;
    //libnickel 4.6 * _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_
    reinterpret_cast<void*&>(AbstractNickelMenuController_createMenuTextItem_orig) = nm_dlhook(libnickel, "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_", nmh, &err);
    NM_ASSERT(AbstractNickelMenuController_createMenuTextItem_orig, "failed to hook _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_: %s", err);

    NM_RETURN_OK(0);
    #undef NM_ERR_RET
}

// AbstractNickelMenuController_createAction_before wraps
// AbstractNickelMenuController::createAction to use the correct separator for
// the menu location and to match the behaviour of QMenu::insertAction instead
// of QMenu::addAction. It also adds the property nm_action=true to the action
// and separator.
QAction *AbstractNickelMenuController_createAction_before(QAction *before, nm_menu_location_t loc, bool last_in_group, void *_this, QMenu *menu, QWidget *widget, bool close, bool enabled, bool separator);

// nm_menu_item_do runs a nm_menu_item_t and must be called from the thread of a
// signal handler.
void nm_menu_item_do(nm_menu_item_t *it);

// _nm_menu_inject handles the QMenu::aboutToShow signal and injects menu items.
void _nm_menu_inject(void *nmc, QMenu *menu, nm_menu_location_t loc, int at);

extern "C" NM_PUBLIC MenuTextItem* _nm_menu_hook(void* _this, QMenu* menu, QString const& label, bool checkable, bool checked, QString const& thingy) {
    NM_LOG("AbstractNickelMenuController::createMenuTextItem(%p, `%s`, %d, %d, `%s`)", menu, qPrintable(label), checkable, checked, qPrintable(thingy));

    QString trmm = QCoreApplication::translate("StatusBarMenuController", "Settings");
    QString trrm = QCoreApplication::translate("DictionaryActionProxy", "Dictionary");
    NM_LOG("Comparing against '%s', '%s'", qPrintable(trmm), qPrintable(trrm));

    nm_menu_location_t loc = {};
    if (label == trmm && !checkable) {
        NM_LOG("Intercepting main menu (label=Settings, checkable=false)...");
        loc = NM_MENU_LOCATION_MAIN_MENU;
    } else if (label == trrm && !checkable) {
        NM_LOG("Intercepting reader menu (label=Dictionary, checkable=false)...");
        loc = NM_MENU_LOCATION_READER_MENU;
    }

    if (loc)
        QObject::connect(menu, &QMenu::aboutToShow, std::bind(_nm_menu_inject, _this, menu, loc, menu->actions().count()));

    return AbstractNickelMenuController_createMenuTextItem_orig(_this, menu, label, checkable, checked, thingy);
}

void _nm_menu_inject(void *nmc, QMenu *menu, nm_menu_location_t loc, int at) {
    NM_LOG("inject %d @ %d", loc, at);

    NM_LOG("checking for config updates");
    bool updated = nm_global_config_update(NULL); // if there was an error it will be returned as a menu item anyways (and updated will be true)
    NM_LOG("updated = %d", updated);

    NM_LOG("checking for existing items added by nm");

    for (auto action : menu->actions()) {
        if (action->property("nm_action") == true) {
            if (!updated)
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

        NM_LOG("adding items '%s'...", it->lbl);

        MenuTextItem* item = AbstractNickelMenuController_createMenuTextItem_orig(nmc, menu, QString::fromUtf8(it->lbl), false, false, "");
        QAction* action = AbstractNickelMenuController_createAction_before(before, loc, i == items_n-1, nmc, menu, item, true, true, true);

        QObject::connect(action, &QAction::triggered, [it](bool){
            NM_LOG("item '%s' pressed...", it->lbl);
            nm_menu_item_do(it);
            NM_LOG("done");
        }); // note: we're capturing by value, i.e. the pointer to the global variable, rather then the stack variable, so this is safe
    }
}

void nm_menu_item_do(nm_menu_item_t *it) {
    char *err = NULL;
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

        free(err); // free the previous error if it is not NULL (so the last error can be saved for later)

        nm_action_result_t *res = cur->act(cur->arg, &err);
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
        free(err);
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
