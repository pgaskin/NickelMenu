#include <QAction>
#include <QCoreApplication>
#include <QFile>
#include <QLayout>
#include <QMenu>
#include <QMetaProperty>
#include <QPushButton>
#include <QRect>
#include <QRegularExpression>
#include <QString>
#include <QUrl>
#include <QWidget>
#include <QWidgetAction>

#include <cstdlib>
#include <cstring>

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
void (*MenuTextItem_setTextItalics)(MenuTextItem*, bool italic);
void (*MenuTextItem_registerForTapGestures)(MenuTextItem*);

// NickelTouchMenu decoration (caret/pointer) API. Resolved optionally
// (15505+); used only when the menu_main_15505_caret option is set. The caret
// paints only when showDecoration(true) is set AND a target rect is given
// (paintEvent gates drawDecoration on the flag).
void (*NickelTouchMenu_showDecoration)(NickelTouchMenu*, bool);
void (*NickelTouchMenu_setTargetRect)(NickelTouchMenu*, QRect const&);
void (*NickelTouchMenu_setDecorationPosition)(NickelTouchMenu*, DecorationPosition);
void (*NickelTouchMenu_setDecorationPositionOffset)(NickelTouchMenu*, QPoint const&);

// Selection menu stuff (14622+).
typedef void SelectionMenuController; // note: items are re-initialized every time the menu is opened
typedef QWidget SelectionMenuView;
typedef void WebSearchMixinBase;
void (*SelectionMenuController_lookupWikipedia)(SelectionMenuController*);
void (*SelectionMenuController_onSearchInBook) (SelectionMenuController*);
void (*SelectionMenuController_lookupWeb)(SelectionMenuController*); // 14622-18838
void (*SelectionMenuController_lookupGoogle)(SelectionMenuController*); // 19086+ (replaces lookupWeb, alternative 1)
void (*SelectionMenuController_lookupBaidu)(SelectionMenuController*); // 19086+ (replaces lookupWeb, alternative 2)
void (*SelectionMenuController_addMenuItem)(SelectionMenuController*, SelectionMenuView* smv, MenuTextItem* mti, const char *slot); // note: the MenuTextItem is created by SelectionMenuController_createMenuTextItem, with smv as the parent (the first QWidget* argument)
void (*SelectionMenuView_addMenuItem)(SelectionMenuView*, MenuTextItem *mti); // note: this adds the separator and the item (it doesn't connect signals or things like that)
void (*WebSearchMixinBase_doWikipediaSearch)(WebSearchMixinBase *, QString const& selection, QString const& locale);

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

    // selection menu injection
    {.sym = "_ZN23SelectionMenuController11addMenuItemEP17SelectionMenuViewP12MenuTextItemPKc", .sym_new = "_nm_menu_hook3", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(SelectionMenuController_addMenuItem),  .desc = "selection menu injection",                     .optional = true}, //libnickel 4.20.14622 * _ZN23SelectionMenuController11addMenuItemEP17SelectionMenuViewP12MenuTextItemPKc
    {.sym = "_ZN18WebSearchMixinBase17doWikipediaSearchERK7QStringS2_",                         .sym_new = "_nm_menu_hook4", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(WebSearchMixinBase_doWikipediaSearch), .desc = "selection menu injection (wikipedia handler)", .optional = true}, //libnickel 4.20.14622 * _ZN18WebSearchMixinBase17doWikipediaSearchERK7QStringS2_

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
    {.name = "_ZN12MenuTextItem14setTextItalicsEb",                  .out = nh_symoutptr(MenuTextItem_setTextItalics),         .desc = "main menu popover italics (15505+)",             .optional = true}, //libnickel 4.23.15505 * _ZN12MenuTextItem14setTextItalicsEb
    {.name = "_ZN12MenuTextItem22registerForTapGesturesEv",          .out = nh_symoutptr(MenuTextItem_registerForTapGestures), .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN12MenuTextItem22registerForTapGesturesEv

    // main menu popover caret/decoration (15505+) — optional; used by menu_main_15505_caret
    {.name = "_ZN15NickelTouchMenu14showDecorationEb",                           .out = nh_symoutptr(NickelTouchMenu_showDecoration),             .desc = "main menu popover caret (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN15NickelTouchMenu14showDecorationEb
    {.name = "_ZN15NickelTouchMenu13setTargetRectERK5QRect",                     .out = nh_symoutptr(NickelTouchMenu_setTargetRect),              .desc = "main menu popover caret (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN15NickelTouchMenu13setTargetRectERK5QRect
    {.name = "_ZN15NickelTouchMenu21setDecorationPositionE18DecorationPosition", .out = nh_symoutptr(NickelTouchMenu_setDecorationPosition),       .desc = "main menu popover caret (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN15NickelTouchMenu21setDecorationPositionE18DecorationPosition
    {.name = "_ZN15NickelTouchMenu27setDecorationPositionOffsetERK6QPoint",      .out = nh_symoutptr(NickelTouchMenu_setDecorationPositionOffset), .desc = "main menu popover caret (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN15NickelTouchMenu27setDecorationPositionOffsetERK6QPoint

    // selection menu injection (14622+)
    {.name = "_ZN17SelectionMenuView11addMenuItemEP12MenuTextItem", .out = nh_symoutptr(SelectionMenuView_addMenuItem),           .desc = "selection menu injection (14622+)",        .optional = true}, //libnickel 4.20.14622 * _ZN17SelectionMenuView11addMenuItemEP12MenuTextItem
    {.name = "_ZN23SelectionMenuController15lookupWikipediaEv",     .out = nh_symoutptr(SelectionMenuController_lookupWikipedia), .desc = "selection menu injection (14622+)",        .optional = true}, //libnickel 4.20.14622 * _ZN23SelectionMenuController15lookupWikipediaEv
    {.name = "_ZN23SelectionMenuController9lookupWebEv",            .out = nh_symoutptr(SelectionMenuController_lookupWeb),       .desc = "selection menu injection (14622-18838)",   .optional = true}, //libnickel 4.20.14622 4.30.18838 _ZN23SelectionMenuController9lookupWebEv
    {.name = "_ZN23SelectionMenuController12lookupGoogleEv",        .out = nh_symoutptr(SelectionMenuController_lookupGoogle),    .desc = "selection menu injection (19086+, alt 1)", .optional = true}, //libnickel 4.31.19086 * _ZN23SelectionMenuController12lookupGoogleEv
    {.name = "_ZN23SelectionMenuController11lookupBaiduEv",         .out = nh_symoutptr(SelectionMenuController_lookupBaidu),     .desc = "selection menu injection (19086+, alt 2)", .optional = true}, //libnickel 4.31.19086 * _ZN23SelectionMenuController11lookupBaiduEv

    // selection menu injection (parental controls enabled)
    {.name = "_ZN23SelectionMenuController14onSearchInBookEv",     .out = nh_symoutptr(SelectionMenuController_onSearchInBook), .desc = "selection menu injection (14601+, parental controls enabled)", .optional = true}, //libnickel 4.20.14601 * _ZN23SelectionMenuController14onSearchInBookEv

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

// nm_argtranform_t transforms an action's argument and returns a new malloc'd
// string. On error, it should return NULL and set nm_err.
typedef char *(*nm_argtransform_t)(void *data, const char *arg);

// nm_menu_item_do runs a nm_menu_item_t and must be called from the thread of a
// signal handler. argtransform and argtransform_data are optional.
static void nm_menu_item_do(nm_menu_item_t *it, nm_argtransform_t argtransform, void *argtransform_data);

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

QString nm_menu_pixmap(const char *custom, const char *custom_temp_out, const char *fallback) {
    if (!custom || !custom_temp_out)
        return QString(fallback);

    QImage a;
    if (!a.load(QString(custom))) {
        NM_LOG("nm_menu_pixmap: error loading '%s', falling back to '%s': %s", custom, fallback, QFile::exists(custom) ? "failed to load image" : "image does not exist");
        return QString(fallback);
    }

    QPixmap b;
    if (!b.load(QString(fallback))) {
        NM_LOG("nm_menu_pixmap: error loading default pixmap '%s': %s", fallback, QFile::exists(fallback) ? "failed to load image" : "image does not exist");
        return QString(fallback);
    }

    QImage c = a.scaled(b.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    NM_LOG("nm_menu_pixmap: resized '%s' to match '%s' (%dx%d): %dx%d", custom, fallback, b.size().width(), b.size().height(), c.size().width(), c.size().height());

    if (!c.save(QString(custom_temp_out), "PNG")) {
        NM_LOG("nm_menu_pixmap: error saving resized pixmap to '%s'", custom_temp_out);
        return QString(fallback);
    }

    return QString(custom_temp_out);
}

static const char *nm_main_menu_config(int index, const char *option) {
    char buf[strlen("menu_main_15505_9_icon_active") + 1];
    if (snprintf(buf, sizeof(buf), "menu_main_15505_%d_%s", index, option) >= static_cast<ssize_t>(sizeof(buf))) {
        NM_LOG("Failed to create main menu config key for index %d, option %s", index, option);
        return nullptr;
    }
    return nm_global_config_experimental(buf);
}

static void main_nav_button_configure(MainNavButton *btn, const char *label, const char *icon, const char *icon_fallback, const char *icon_active, const char *icon_active_fallback) {
    if (label) {
        MainNavButton_setText(btn, label);
    }

    if (icon || icon_fallback) {
        QString pixmap = nm_menu_pixmap(
            icon,
            "/tmp/nm_menu.png",
            icon_fallback
        );
        if (!pixmap.isEmpty()) {
            MainNavButton_setPixmap(btn, pixmap);
        }
    }

    if (icon_active || icon_active_fallback) {
        QString pixmap = nm_menu_pixmap(
            icon_active,
            "/tmp/nm_menu.png",
            icon_active_fallback
        );
        if (!pixmap.isEmpty()) {
            MainNavButton_setActivePixmap(btn, pixmap);
        }
    }

    if (icon || icon_fallback || icon_active || icon_active_fallback) {
        QFile::remove("/tmp/nm_menu.png");
    }
}

extern "C" __attribute__((visibility("default"))) void _nm_menu_hook2(MainNavView *_this, QWidget *parent) {
    NM_LOG("MainNavView::MainNavView(%p, %p)", _this, parent);
    MainNavView_MainNavView(_this, parent);

    if (!MainNavButton_MainNavButton || !MainNavButton_setPixmap || !MainNavButton_setActivePixmap || !MainNavButton_setText || !MainNavButton_setText) {
        NM_LOG("Could not find required MainNavButton symbols, cannot add tab button for NickelMenu main menu.");
        return;
    }

    QHBoxLayout *bl = _this->findChild<QHBoxLayout*>();
    if (!bl) {
        NM_LOG("Could not find QHBoxLayout(should contain MainNavButtons and be contained in the QVBoxLayout of MainNavView) in MainNavView, cannot add tab button for NickelMenu main menu.");
        return;
    }

    NM_LOG("Default main menu has %d buttons", bl->count());
    for (int i = 0; i < bl->count(); ++i) {
        QWidget *widget = bl->itemAt(i)->widget();
        NM_LOG("Main menu button %d = %s", i, widget ->objectName().toUtf8().constData());

        MainNavButton *btn = qobject_cast<MainNavButton*>(widget);
        if (!btn) {
            NM_LOG("qobject_cast<MainNavButton*> failed on button %d", i);
            continue;
        }

        const char *label = nm_main_menu_config(i, "label");
        const char *icon = nm_main_menu_config(i, "icon");
        const char *icon_active = nm_main_menu_config(i, "icon_active");

        const char *enabled = nm_main_menu_config(i, "enabled");
        main_nav_button_configure(btn, label, icon, nullptr, icon_active, nullptr);
        if (enabled) {
            if (strcmp("0", enabled) == 0) {
                NM_LOG("Main menu button %d disabled", i);
                widget->hide();
            } else if (strcmp("1", enabled) == 0) {
                NM_LOG("Main menu button %d explicitly enabled", i);
                widget->show();
            }
        }
    }

    const char *enabled = nm_global_config_experimental("menu_main_15505_enabled");
    if (enabled && strcmp("0", enabled) == 0) {
        NM_LOG("Main menu NickelMenu button disabled");
        return;
    }

    NM_LOG("Adding main menu button in tab bar for firmware 4.23.15505+.");
    MainNavButton *btn = reinterpret_cast<MainNavButton*>(calloc(1, 256));
    if (!btn) { // way larger than a MainNavButton, but better to be safe
        NM_LOG("Failed to allocate memory for MainNavButton, cannot add tab button for NickelMenu main menu.");
        return;
    }

    MainNavButton_MainNavButton(btn, parent);
    main_nav_button_configure(btn,
        nm_global_config_experimental("menu_main_15505_label") ?: "NickelMenu",
        nm_global_config_experimental("menu_main_15505_icon"),
        ":/images/home/main_nav_more.png",
        nm_global_config_experimental("menu_main_15505_icon_active"),
        ":/images/home/main_nav_more_active.png"
    );
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

        // Appearance options for the from-scratch main-menu popover. Defaults
        // preserve the stock appearance, so all of these are no-ops unless set.
        // `decoration` is the DecorationPosition enum Nickel draws its native
        // popover caret with (the caret itself is gated on `caret`, since it
        // also needs a parent and a target rect — see below); offset_x/_y nudge
        // the popup, e.g. to add a screen-edge margin.
        const char *nm_deco_s  = nm_global_config_experimental("menu_main_15505_decoration");
        const char *nm_offx_s  = nm_global_config_experimental("menu_main_15505_offset_x");
        const char *nm_offy_s  = nm_global_config_experimental("menu_main_15505_offset_y");
        const char *nm_caret_s = nm_global_config_experimental("menu_main_15505_caret");
        int  nm_deco  = nm_deco_s ? (int) strtol(nm_deco_s, nullptr, 10) : 3;
        int  nm_offx  = nm_offx_s ? (int) strtol(nm_offx_s, nullptr, 10) : 0;
        int  nm_offy  = nm_offy_s ? (int) strtol(nm_offy_s, nullptr, 10) : 0;
        bool nm_caret = nm_caret_s && nm_caret_s[0] == '1';
        const char *nm_fsep_s = nm_global_config_experimental("menu_main_15505_flush_separators");
        bool nm_fsep = nm_fsep_s && nm_fsep_s[0] == '1';
        const char *nm_ital_s = nm_global_config_experimental("menu_main_15505_italic");
        bool nm_ital = nm_ital_s ? nm_ital_s[0] == '1' : true; // stock items are italic

        // The caret (drawDecoration) hard-requires a parent widget: its first
        // check is `if (!d->parent) return`, and it maps the caret anchor point
        // via parent->mapFromGlobal(). Parent to the top-level window (global ≈
        // window coords). Without the caret, keep the stock nullptr parent.
        QWidget *nm_parent = nm_caret ? btn->window() : nullptr;
        NickelTouchMenu_NickelTouchMenu(menu, nm_parent, nm_deco);

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

            MenuTextItem_MenuTextItem(mti, menu, false, nm_ital);
            MenuTextItem_setText(mti, QString::fromUtf8(it->lbl));
            // setText re-applies the item's italics, so the explicit setter is
            // needed in addition to the constructor flag.
            if (!nm_ital && MenuTextItem_setTextItalics)
                MenuTextItem_setTextItalics(mti, false);
            MenuTextItem_registerForTapGestures(mti); // this only makes the MenuTextItem::tapped signal connect so it highlights on tap, doesn't apply to the QAction::triggered below (which needs another GestureReceiver somewhere)

            // based on _ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb

            QWidgetAction *ac = new QWidgetAction(menu);
            ac->setDefaultWidget(mti);
            ac->setEnabled(true);

            menu->addAction(ac);

            QWidget::connect(ac, &QAction::triggered, menu, &QMenu::hide);

            if (i != items_n-1) {
                // Stock popovers draw full-width dividers with Nickel's own
                // separator action (an 8-byte QAction subclass, same pattern as
                // the pre-15505 main menu above); QMenu::addSeparator() insets
                // the line from the popover edges.
                QAction *nm_sep = nullptr;
                if (nm_fsep && LightMenuSeparator_LightMenuSeparator) {
                    nm_sep = reinterpret_cast<QAction*>(calloc(1, 32)); // it's actually 8 as of 14622, but better to be safe
                    if (nm_sep) {
                        LightMenuSeparator_LightMenuSeparator(nm_sep, menu);
                        menu->addAction(nm_sep);
                    }
                }
                if (!nm_sep)
                    menu->addSeparator();
            }

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
                nm_menu_item_do(it, NULL, NULL);
                NM_LOG("done");
            }); // note: we're capturing by value, i.e. the pointer to the global variable, rather then the stack variable, so this is safe
        }

        NM_LOG("showing menu");

        QWidget::connect(menu, &QMenu::aboutToHide, menu, &QWidget::deleteLater);

        // Enable the native popover caret when requested. paintEvent only calls
        // drawDecoration if showDecoration(true) was set, and the caret needs a
        // target to aim at — so both are required. setDecorationPosition picks
        // the edge (0=top, 1=right, 2=left, 3=bottom, 5=none).
        if (nm_caret && NickelTouchMenu_showDecoration && NickelTouchMenu_setTargetRect) {
            NickelTouchMenu_showDecoration(menu, true);
            if (NickelTouchMenu_setDecorationPosition)
                NickelTouchMenu_setDecorationPosition(menu, nm_deco);
            QRect nm_tgt(btn->mapToGlobal(QPoint(0, 0)), btn->size());
            NickelTouchMenu_setTargetRect(menu, nm_tgt);
            // The caret aims at this global point (stored at +0x4c; drawDecoration
            // maps it into parent coords and adds half the target rect's width
            // itself), so pass the target's top-left, NOT its centre. caret_dx/_dy
            // allow config-only nudging of the anchor.
            if (NickelTouchMenu_setDecorationPositionOffset) {
                const char *nm_cdx_s = nm_global_config_experimental("menu_main_15505_caret_dx");
                const char *nm_cdy_s = nm_global_config_experimental("menu_main_15505_caret_dy");
                int nm_cdx = nm_cdx_s ? (int) strtol(nm_cdx_s, nullptr, 10) : 0;
                int nm_cdy = nm_cdy_s ? (int) strtol(nm_cdy_s, nullptr, 10) : 0;
                QPoint nm_anchor = nm_tgt.topLeft() + QPoint(nm_cdx, nm_cdy);
                NickelTouchMenu_setDecorationPositionOffset(menu, nm_anchor);
            }
            NM_LOG("enabled menu caret (edge=%d)", nm_deco);
        }

        // Optional fixed popover width (pixels). Default (unset/0) keeps the
        // stock behaviour: auto-sized to the widest menu item.
        const char *nm_w_s = nm_global_config_experimental("menu_main_15505_width");
        int nm_w = nm_w_s ? (int) strtol(nm_w_s, nullptr, 10) : 0;
        if (nm_w > 0)
            menu->setFixedWidth(nm_w);

        menu->ensurePolished();
        menu->popup(btn->mapToGlobal(btn->geometry().topRight() - QPoint(0, menu->sizeHint().height())));
        // Apply the configured nudge AFTER popup(): popup()'s screen-fit
        // clamping repositions the menu (the stock popup point is far
        // off-screen right, so the clamp engages every time), which would
        // swallow any pre-popup offset. Post-popup move() makes the offsets
        // relative to the on-screen position the menu actually landed at.
        if (nm_offx || nm_offy) {
            menu->move(menu->pos() + QPoint(nm_offx, nm_offy));
            NM_LOG("nudged menu by (%d,%d)", nm_offx, nm_offy);
        }
        // Stretch the separator widgets to the popover's full width. The menu
        // lays out every action (items and separators alike) inset from the
        // edges, so even Nickel's native separator renders with side gaps; the
        // stock popovers draw their dividers edge-to-edge. Done after popup()
        // so the layout has already assigned geometry (the menu is rebuilt on
        // every open, so this re-applies each time).
        if (nm_fsep) {
            // The widget's true edge sits outside the painted frame, so a
            // separate inset (pixels from each edge) lines the dividers up
            // with the visible border. Default 0 = the widget's full width.
            const char *nm_si_s = nm_global_config_experimental("menu_main_15505_separator_inset");
            int nm_si = nm_si_s ? (int) strtol(nm_si_s, nullptr, 10) : 0;
            int nm_fixed = 0;
            for (QAction *nm_a : menu->actions()) {
                if (!nm_a->isSeparator())
                    continue;
                QWidgetAction *nm_wa = qobject_cast<QWidgetAction*>(nm_a);
                QWidget *nm_w = nm_wa ? nm_wa->defaultWidget() : nullptr;
                if (nm_w) {
                    QRect nm_g = nm_w->geometry();
                    nm_w->setGeometry(nm_si, nm_g.y(), menu->width() - 2*nm_si, nm_g.height());
                    nm_fixed++;
                }
            }
            if (!nm_fixed) {
                // The separator action may create its widget via
                // QWidgetAction::createWidget() rather than setDefaultWidget();
                // catch those as direct children by class name.
                for (QWidget *nm_w : menu->findChildren<QWidget*>()) {
                    if (strcmp(nm_w->metaObject()->className(), "LightMenuSeparator") != 0)
                        continue;
                    QRect nm_g = nm_w->geometry();
                    nm_w->setGeometry(nm_si, nm_g.y(), menu->width() - 2*nm_si, nm_g.height());
                    nm_fixed++;
                }
            }
            NM_LOG("stretched %d separators (inset=%d)", nm_fixed, nm_si);
        }
    });

    bl->addWidget(btn, 1);
    _this->ensurePolished();

    NM_LOG("Added button.");
}

// _nm_menu_hook4_item gets/sets the current menu item. It must only be called
// by one thread at a time. The item will be cleared after it has been used.
nm_menu_item_t *_nm_menu_hook4_item(nm_menu_item_t *it) {
    static nm_menu_item_t *its = NULL;
    if (it)
        return (its = it);
    if (!its)
        return NULL;
    nm_menu_item_t *tmp = its;
    its = NULL;
    return tmp;
}

extern "C" __attribute__((visibility("default"))) void _nm_menu_hook3(SelectionMenuController *_this, SelectionMenuView* smv, MenuTextItem* mti, const char *slot) {
    NM_LOG("hook3: %p %p %p %s", _this, smv, mti, slot);
    SelectionMenuController_addMenuItem(_this, smv, mti, slot);

    // don't list the search in book one (for parental controls) here so we don't mask bugs in this logic
    if (!SelectionMenuView_addMenuItem || !SelectionMenuController_lookupWikipedia || !(SelectionMenuController_lookupWeb || (SelectionMenuController_lookupGoogle && SelectionMenuController_lookupBaidu))) {
        NM_LOG("could not find required SelectionMenuView and SelectionMenuController symbols for adding selection menu items");
        ConfirmationDialogFactory_showOKDialog(QLatin1String("NickelMenu"), QLatin1String("Could not find required SelectionMenuView and SelectionMenuController symbols for adding selection menu items (this is a bug)."));
        return;
    }

    // this is important for another reason other than positioning: it only displays if Volume::canSearch()
    nm_menu_location_t loc;
    if (!strcmp(slot, "1showSearchOptions()")) //libnickel 4.20.14622 * _ZN23SelectionMenuController17showSearchOptionsEv
        loc = NM_MENU_LOCATION(selection);
    else if (!strcmp(slot, "1onSearchInBook()")) //libnickel 4.20.14601 * _ZN23SelectionMenuController14onSearchInBookEv
        loc = NM_MENU_LOCATION(selection); // this one is shown instead if parental controls are enabled
    else if (!strcmp(slot, "2lookupWeb()")) //libnickel 4.20.14622 4.30.18838 _ZN23SelectionMenuController9lookupWebEv
        loc = NM_MENU_LOCATION(selection_search);
    else if (!strcmp(slot, "2lookupGoogle()")) //libnickel 4.31.19086 * _ZN23SelectionMenuController12lookupGoogleEv
        loc = NM_MENU_LOCATION(selection_search);
    else if (!strcmp(slot, "2lookupBaidu()")) //libnickel 4.31.19086 * _ZN23SelectionMenuController11lookupBaiduEv
        loc = NM_MENU_LOCATION(selection_search);
    else
        return;

    NM_LOG("Found search item, injecting menu items after it.");

    NM_LOG("checking for config updates");
    int rev = nm_global_config_update();
    NM_LOG("revision = %d", rev);

    NM_LOG("adding items");

    size_t items_n;
    nm_menu_item_t **items = nm_global_config_items(&items_n);

    if (!items) {
        NM_LOG("failed to get menu items");
        ConfirmationDialogFactory_showOKDialog(QLatin1String("NickelMenu"), QLatin1String("Failed to get menu items (this might be a bug)."));
        return;
    }

    for (size_t i = 0; i < items_n; i++) {
        nm_menu_item_t *it = items[i];
        if (it->loc != loc)
            continue;

        NM_LOG("adding item '%s'...", it->lbl);

        // based on _ZN23SelectionMenuController18createMenuTextItemEP7QWidgetRK7QString

        MenuTextItem *mti = reinterpret_cast<MenuTextItem*>(calloc(1, 256)); // about 3x larger than the 15505 size (92)
        if (!it) {
            NM_LOG("failed to allocate memory for config item");
            return;
        }

        MenuTextItem_MenuTextItem(mti, smv, false, true);
        MenuTextItem_setText(mti, QString::fromUtf8(it->lbl));
        MenuTextItem_registerForTapGestures(mti);

        // based on _ZN23SelectionMenuController18setupSearchOptionsEb and _ZN23SelectionMenuController11addMenuItemEP17SelectionMenuViewP12MenuTextItemPKc, plus some custom stuff for better compatibility

        QPushButton *sh = new QPushButton(smv); // HACK: we use a QPushButton as an adaptor so we can connect an old-style signal with the new-style connect without needing a custom QObject
        if (!QWidget::connect(mti, SIGNAL(tapped(bool)), sh, SIGNAL(pressed()))) {
            NM_LOG("Failed to connect SIGNAL(tapped(bool)) on MenuTextItem to SIGNAL(pressed()) on the QPushButton shim, cannot add custom selection menu item.");
            return;
        }
        sh->setVisible(false);

        QObject::connect(sh, &QPushButton::pressed, [_this, it]() {
            NM_LOG("item '%s' pressed...", it->lbl);
            _nm_menu_hook4_item(it); // this is safe since it is a pointer captured by value
            NM_LOG("triggering lookupWikipedia() slot");
            SelectionMenuController_lookupWikipedia(_this);
        });

        SelectionMenuView_addMenuItem(smv, mti);
    }
}

typedef struct {
    QString const& selection;
} nm_selmenu_argtransform_data_t;

char *_nm_selmenu_argtransform(void *data, const char *arg) {
    nm_selmenu_argtransform_data_t *d = (nm_selmenu_argtransform_data_t*)(data);

    QString src = QString::fromUtf8(arg), res;
    QRegularExpression re = QRegularExpression("\\{([1])\\|([aAfnsSuwx]*)\\|([\"$%]*)\\}");

    for (QStringRef x = src.midRef(0); x.length() > 0;) {
        QRegularExpressionMatch m = re.match(x.toString());

        if (!m.hasMatch()) {
            res += x;
            x = x.mid(x.length());
            continue;
        }

        QString tmp;

        for (int k = 0; k < m.capturedLength(1); k++) {
            switch (m.capturedRef(1).at(k).toLatin1()) {
            case '1': tmp = d->selection; break;
            }
        }

        for (int k = 0; k < m.capturedLength(2); k++) {
            switch (m.capturedRef(2).at(k).toLatin1()) {
            case 'a': tmp = tmp.toLower(); break;
            case 'A': tmp = tmp.toUpper(); break;
            case 'f': tmp = tmp.split(QRegularExpression("\\s")).first(); break;
            case 'n': tmp = tmp.remove(QRegularExpression("[^0-9a-zA-Z]")); break;
            case 's': tmp = tmp.trimmed(); break;
            case 'S': tmp = tmp.simplified(); break;
            case 'u': if (tmp.length() == 0) { nm_err_set("argtransform: empty substitution result for %s", qPrintable(m.capturedRef(0).toString())); return NULL; }; break;
            case 'w': tmp = tmp.remove(QRegularExpression("\\s")); break;
            case 'x': tmp = tmp.replace(QRegularExpression("\\s"), "_"); break;
            }
        }

        for (int k = 0; k < m.capturedLength(3); k++) {
            switch (m.capturedRef(3).at(k).toLatin1()) {
            case '"':
                tmp = tmp
                    .replace("\"", "\\\"")
                    .replace("\n", "\\n")
                    .replace("\b", "\\b")
                    .replace("\t", "\\t")
                    .replace("\f", "\\f")
                    .replace("\r", "\\r")
                    .replace("\\", "\\\\");
                break;
            case '$':
                tmp = tmp.replace("'", "'\"'\"'");
                break;
            case '%':
                tmp = QUrl::toPercentEncoding(tmp);
                break;
            }
        }

        res += x.left(m.capturedStart());
        res += tmp;
        x = x.mid(m.capturedEnd());
    }

    char *x = strdup(res.toUtf8().data());
    if (!x)
        nm_err_set("argtransform: could not allocate memory: %m");
    return x;
}

extern "C" __attribute__((visibility("default"))) void _nm_menu_hook4(WebSearchMixinBase *_this, QString const& selection, QString const& locale) {
    NM_LOG("hook4: %p %s %s", _this, qPrintable(selection), qPrintable(locale));

    nm_menu_item_t *it = _nm_menu_hook4_item(NULL);
    if (!it) {
        NM_LOG("No current menu item, continuing with default wikipedia search.");
        WebSearchMixinBase_doWikipediaSearch(_this, selection, locale);
        return;
    }

    NM_LOG("continuing execution of item %p (%s)", it, it->lbl);
    nm_selmenu_argtransform_data_t data = (nm_selmenu_argtransform_data_t){
        .selection = selection,
    };
    nm_menu_item_do(it, _nm_selmenu_argtransform, (void*)(&data)); // this is safe since data will not be used after this returns
    NM_LOG("done");
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
            nm_menu_item_do(it, NULL, NULL);
            NM_LOG("done");
        }); // note: we're capturing by value, i.e. the pointer to the global variable, rather then the stack variable, so this is safe
    }

    NM_LOG("updating config revision property");
    menu->setProperty("nm_config_rev", rev_n);
}

void nm_menu_item_do(nm_menu_item_t *it, nm_argtransform_t argtransform, void *argtransform_data) {
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

        nm_action_result_t *res = NULL;
        if (!argtransform) {
            res = cur->act(cur->arg);
        } else {
            NM_LOG("...applying argtransform");
            char *arg = (*argtransform)(argtransform_data, cur->arg);
            if (arg) {
                NM_LOG("...applied argtransform: %s", arg);
                res = cur->act(arg);
                free(arg);
            }
        }
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

