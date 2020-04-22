#include <QAction>
#include <QMenu>
#include <QString>
#include <QWidget>

#include <cstdlib>

#include "dlhook.h"
#include "menu.h"
#include "util.h"

typedef QWidget MenuTextItem; // it's actually a subclass, but we don't need it's functionality directly, so we'll stay on the safe side

// AbstractNickelMenuController::createMenuTextItem creates a menu item in the
// given QMenu with the specified label. The first bool parameter adds space to
// the left of the item for a checkbox, and the second bool checks it (see the
// search menu for an example of this being used). IDK what the last parameter
// is for, but I suspect it might be passed to QObject::setObjectName (I haven't
// tested it directly, and it's been empty in the menus I've tried so far).
static MenuTextItem* (*AbstractNickelMenuController_createMenuTextItem_orig)(void*, QMenu*, QString const&, bool, bool, QString const&) = NULL;
static MenuTextItem* (*AbstractNickelMenuController_createMenuTextItem)(void*, QMenu*, QString const&, bool, bool, QString const&);

// AbstractNickelMenuController::createAction finishes adding the action to the
// menu. IDK what the bool params are for, but thing1 and thing2 always seem to
// be true (I know one or the other is for if it is greyed out).
static QAction* (*AbstractNickelMenuController_createAction)(void*, QMenu*, QWidget*, bool, bool, bool);

// ConfirmationDialogFactory::showOKDialog does what it says, with the provided
// title and body text. Note that it should be called from the GUI thread (or
// a signal handler).
static void (*ConfirmationDialogFactory_showOKDialog)(QString const&, QString const&);

extern "C" MenuTextItem* AbstractNickelMenuController_createMenuTextItem_hook(void* _this, QMenu* menu, QString const& label, bool checkable, bool checked, QString const& thingy) {
    NMI_LOG("AbstractNickelMenuController::createMenuTextItem(%p, `%s`, %d, %d, `%s`)", menu, qPrintable(label), checkable, checked, qPrintable(thingy));

    // TODO: add items

    return AbstractNickelMenuController_createMenuTextItem_orig(_this, menu, label, checkable, checked, thingy);
}

extern "C" int nmi_menu_hook(void *libnickel, nmi_menu_entry_t *entries, size_t entries_n, char **err_out) {
    #define NMI_ERR_RET 1
    NMI_SYM(AbstractNickelMenuController_createMenuTextItem, "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_");
    NMI_SYM(AbstractNickelMenuController_createAction, "_ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb");
    NMI_SYM(ConfirmationDialogFactory_showOKDialog, "_ZN25ConfirmationDialogFactory12showOKDialogERK7QStringS2_");

    NMI_ASSERT(AbstractNickelMenuController_createMenuTextItem, "unsupported firmware: could not find AbstractNickelMenuController::createMenuTextItem(void* _this, QMenu*, QString, bool, bool, QString const&)");
    NMI_ASSERT(AbstractNickelMenuController_createAction, "unsupported firmware: could not find AbstractNickelMenuController::createAction(void* _this, QMenu*, QWidget*, bool, bool, bool)");
    NMI_ASSERT(AbstractNickelMenuController_createAction, "unsupported firmware: could not find ConfirmationDialogFactory::showOKDialog(QString const&, QString const&)");

    char *err;
    reinterpret_cast<void*&>(AbstractNickelMenuController_createMenuTextItem_orig) = nmi_dlhook(libnickel, "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_", reinterpret_cast<void*&>(AbstractNickelMenuController_createMenuTextItem_hook), &err);
    NMI_ASSERT(AbstractNickelMenuController_createMenuTextItem_orig, "failed to hook _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_: %s", err);

    #undef NMI_ERR_RET
}

// TODO