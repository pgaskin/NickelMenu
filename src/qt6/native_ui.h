#pragma once
#include "nickel.h"

class QAction;
class QMenu;
class QWidget;

namespace nm6 {
// Native widgets own their fonts, DPI-scaled geometry, touch handling and
// theme. Keep their private constructors behind the audited Nickel boundary.
class NativeUi {
public:
    NativeUi();
    explicit operator bool() const { return ready_; }
    QWidget *button(QWidget *parent, const QString &text) const;
    QMenu *menu(QWidget *parent) const;
    QAction *item(QMenu *, const QString &text, bool enabled = true) const;
private:
    Nickel nickel_;
    void (*button_ctor_)(QWidget *, QWidget *) = nullptr;
    void (*button_text_)(QWidget *, const QString &) = nullptr;
    void (*button_pixmap_)(QWidget *, const QString &) = nullptr;
    void (*button_active_pixmap_)(QWidget *, const QString &) = nullptr;
    void (*menu_ctor_)(QMenu *, QWidget *, int) = nullptr;
    void (*item_ctor_)(QWidget *, QWidget *, bool, bool) = nullptr;
    void (*item_text_)(QWidget *, const QString &) = nullptr;
    void (*item_gestures_)(QWidget *) = nullptr;
    bool ready_ = false;
};
}
