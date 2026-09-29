#include "native_ui.h"
#include <QMenu>
#include <QWidgetAction>
#include <memory>
#include <new>

namespace nm6 {
namespace {
// ARM32 allocation sizes and primary QWidget/QMenu bases are verified in both
// allowlisted firmware builds. Use C++ allocation: Qt's virtual deleting
// destructor, including sized delete, must receive compatible heap storage.
template<class Widget, size_t Size, class Constructor, class... Args>
std::unique_ptr<Widget> construct(Constructor ctor, Args... args) {
    void *storage = ::operator new(Size);
    auto *widget = static_cast<Widget *>(storage);
    try {
        ctor(widget, args...);
    } catch (...) {
        ::operator delete(storage);
        throw;
    }
    return std::unique_ptr<Widget>(widget);
}
}

NativeUi::NativeUi() {
    if (!nickel_) return;
    button_ctor_ = nickel_.resolve<decltype(button_ctor_)>("_ZN13MainNavButtonC1EP7QWidget");
    button_text_ = nickel_.resolve<decltype(button_text_)>("_ZN13MainNavButton7setTextERK7QString");
    button_pixmap_ = nickel_.resolve<decltype(button_pixmap_)>("_ZN13MainNavButton9setPixmapERK7QString");
    button_active_pixmap_ = nickel_.resolve<decltype(button_active_pixmap_)>("_ZN13MainNavButton15setActivePixmapERK7QString");
    menu_ctor_ = nickel_.resolve<decltype(menu_ctor_)>("_ZN15NickelTouchMenuC2EP7QWidget18DecorationPosition");
    item_ctor_ = nickel_.resolve<decltype(item_ctor_)>("_ZN12MenuTextItemC1EP7QWidgetbb");
    item_text_ = nickel_.resolve<decltype(item_text_)>("_ZN12MenuTextItem7setTextERK7QString");
    item_gestures_ = nickel_.resolve<decltype(item_gestures_)>("_ZN12MenuTextItem22registerForTapGesturesEv");
    ready_ = button_ctor_ && button_text_ && button_pixmap_ && button_active_pixmap_ &&
             menu_ctor_ && item_ctor_ && item_text_ && item_gestures_;
}

QWidget *NativeUi::button(QWidget *parent, const QString &text) const {
    if (!ready_) return nullptr;
    auto widget = construct<QWidget, 104>(button_ctor_, parent);
    button_text_(widget.get(), text);
    button_pixmap_(widget.get(), QStringLiteral(":/images/home/main_nav_more.png"));
    button_active_pixmap_(widget.get(), QStringLiteral(":/images/home/main_nav_more_active.png"));
    widget->setAccessibleName(text);
    return widget.release();
}

QMenu *NativeUi::menu(QWidget *parent) const {
    if (!ready_) return nullptr;
    // Qt5 NickelMenu used decoration position 3 for its bottom-navigation popup.
    return construct<QMenu, 180>(menu_ctor_, parent, 3).release();
}

QAction *NativeUi::item(QMenu *menu, const QString &text, bool enabled) const {
    if (!ready_ || !menu || !menu->inherits("NickelTouchMenu")) return nullptr;
    // Match the Qt5 main-menu path, not the controller factory: the latter
    // requires a live controller to register gestures. Widgets can do it alone.
    auto widget = construct<QWidget, 104>(item_ctor_, menu, false, true);
    item_text_(widget.get(), text);
    item_gestures_(widget.get());
    widget->setAccessibleName(text);
    widget->setEnabled(enabled);
    auto action = std::make_unique<QWidgetAction>(menu);
    action->setText(text);
    action->setEnabled(enabled);
    action->setDefaultWidget(widget.release());
    if (!QObject::connect(action->defaultWidget(), SIGNAL(tapped(bool)),
                          action.get(), SIGNAL(triggered()))) return nullptr;
    menu->addAction(action.get());
    return action.release();
}
}
