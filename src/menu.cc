#include <QAction>
#include <QMenu>
#include <QString>
#include <QWidget>

#include <cstdlib>
#include <dlfcn.h>

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

static nmi_menu_entry_t *_entries;
static size_t _entries_n;

extern "C" int nmi_menu_hook(void *libnickel, nmi_menu_entry_t *entries, size_t entries_n, char **err_out) {
    #define NMI_ERR_RET 1
    reinterpret_cast<void*&>(AbstractNickelMenuController_createMenuTextItem) = dlsym(libnickel, "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_");
    reinterpret_cast<void*&>(AbstractNickelMenuController_createAction) = dlsym(libnickel, "_ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb");
    reinterpret_cast<void*&>(ConfirmationDialogFactory_showOKDialog) = dlsym(libnickel, "_ZN25ConfirmationDialogFactory12showOKDialogERK7QStringS2_");

    NMI_ASSERT(AbstractNickelMenuController_createMenuTextItem, "unsupported firmware: could not find AbstractNickelMenuController::createMenuTextItem(void* _this, QMenu*, QString, bool, bool, QString const&)");
    NMI_ASSERT(AbstractNickelMenuController_createAction, "unsupported firmware: could not find AbstractNickelMenuController::createAction(void* _this, QMenu*, QWidget*, bool, bool, bool)");
    NMI_ASSERT(AbstractNickelMenuController_createAction, "unsupported firmware: could not find ConfirmationDialogFactory::showOKDialog(QString const&, QString const&)");

    void* nmh = dlsym(RTLD_DEFAULT, "_nmi_menu_hook");
    NMI_ASSERT(nmh, "internal error: could not dlsym _nmi_menu_hook");

    char *err;
    reinterpret_cast<void*&>(AbstractNickelMenuController_createMenuTextItem_orig) = nmi_dlhook(libnickel, "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_", nmh, &err);
    NMI_ASSERT(AbstractNickelMenuController_createMenuTextItem_orig, "failed to hook _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_: %s", err);

    _entries = entries;
    _entries_n = entries_n;

    #undef NMI_ERR_RET
}

extern "C" MenuTextItem* _nmi_menu_hook(void* _this, QMenu* menu, QString const& label, bool checkable, bool checked, QString const& thingy) {
    NMI_LOG("AbstractNickelMenuController::createMenuTextItem(%p, `%s`, %d, %d, `%s`)", menu, qPrintable(label), checkable, checked, qPrintable(thingy));

    // TODO: test on other locales
    bool ismm, isrm;
    if ((ismm = label == "Help" && !checkable))
        NMI_LOG("Intercepting main menu (label=Help, checkable=false)...");
    if ((isrm = label == "Dictionary" && !checkable))
        NMI_LOG("Intercepting reader menu (label=Dictionary, checkable=false)...");

    bool add[_entries_n];
    for (size_t i = 0; i < _entries_n; i++) {
        nmi_menu_entry_t *ent = &_entries[i];
        switch (_entries[i].loc) {
        case NMI_MENU_LOCATION_MAIN_MENU:
            add[i] = ismm;
        case NMI_MENU_LOCATION_READER_MENU:
            add[i] = isrm;
        }
    }

    for (size_t i = 0; i < _entries_n; i++) {
        nmi_menu_entry_t *ent = &_entries[i];
        if (add[i]) {
            NMI_LOG("Adding item '%s'...", ent->lbl);

            QString lbl(ent->lbl);
            MenuTextItem* item = AbstractNickelMenuController_createMenuTextItem_orig(_this, menu, lbl, false, false, "");
            QAction* action = AbstractNickelMenuController_createAction(_this, menu, item, true, true, false);

            // note: we're capturing by value, i.e. the pointer to the global variable, rather then the stack variable, so this is safe
            QObject::connect(action, &QAction::triggered, std::function<void(bool)>([ent](bool checked){
                NMI_LOG("Item '%s' pressed...", ent->lbl);
                char *err;
                if (ent->execute(ent->arg, &err) && err) {
                    NMI_LOG("Got error %s, displaying...", err);
                    QString title("nickel-menu-inject");
                    QString text(err);
                    ConfirmationDialogFactory_showOKDialog(title, text);
                    free(err);
                    return;
                }
                NMI_LOG("Success!", err);
            }));
        }
    }

    return AbstractNickelMenuController_createMenuTextItem_orig(_this, menu, label, checkable, checked, thingy);
}
