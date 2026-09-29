#include "launcher.h"
#include "config.h"
#include "executor.h"
#include "native_ui.h"
#include "util.h"
#include <QGuiApplication>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QVariant>
#include <memory>
#include <utility>

namespace nm6 {
namespace {
void display(QWidget *view, const QString &title, const Result &result) {
    if (result.rich_text.isEmpty()) return;
    auto *box = new QMessageBox(result.success ? QMessageBox::Information
                                               : QMessageBox::Warning,
                                title, result.rich_text, QMessageBox::Ok, view);
    box->setObjectName(QStringLiteral("nmQt6Result"));
    box->setTextFormat(Qt::RichText);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowModality(Qt::WindowModal);
    box->open();
    if (result.toast_only) QTimer::singleShot(1500, box, &QDialog::accept);
}

void run(Item item, QWidget *button, QWidget *view, QMenu *menu) {
    if (button->property("nmBusy").toBool()) return;
    menu->hide();
    button->setProperty("nmBusy", true);
    button->setEnabled(false);

    const QString title = item.label;
    const QPointer<QWidget> button_guard(button);
    const QPointer<QWidget> view_guard(view);
    execute_async(std::move(item), [button_guard, view_guard, title](Result result) {
        NM_LOG("Qt6 action chain complete; success=%d", result.success);
        if (button_guard) {
            button_guard->setProperty("nmBusy", false);
            button_guard->setEnabled(true);
        }
        if (view_guard) display(view_guard, title, result);
    });
}

QMenu *build_menu(QWidget *button, QWidget *view) {
    NativeUi native;
    auto *menu = native.menu(button);
    if (!menu) {
        nm_err_set(nullptr);
        menu = new QMenu(button);
        menu->setFont(button->font());
        menu->setPalette(button->palette());
    }
    menu->setObjectName(QStringLiteral("nmQt6Menu"));
    QObject::connect(menu, &QMenu::aboutToHide, menu, [menu] {
        // Kobo's QPA cannot reliably reuse a popup after a modal dialog.
        menu->setObjectName(QString());
        menu->deleteLater();
    });

    nm_global_config_update();
    nm_err_set(nullptr); // parse failures are represented by a Config Error item
    size_t count = 0;
    auto **items = nm_global_config_items(&count);
    bool failed = false;
    auto add_item = [menu, &native, &failed](const QString &text, bool enabled = true) {
        if (menu->inherits("NickelTouchMenu")) {
            if (!menu->actions().isEmpty()) menu->addSeparator();
            auto *action = native.item(menu, text, enabled);
            failed |= !action;
            return action;
        }
        auto *action = menu->addAction(text);
        action->setEnabled(enabled);
        return action;
    };
    size_t omitted = 0;
    for (size_t i = 0; items && i < count; ++i) {
        if (items[i]->loc != NM_MENU_LOCATION(main)) {
            ++omitted;
            continue;
        }
        const Item item = snapshot(*items[i]);
        auto *action = add_item(item.label);
        if (!action) continue;
        QObject::connect(action, &QAction::triggered, button,
                         [button, view, menu, item] {
            run(item, button, view, menu);
        });
    }
    if (omitted) {
        add_item(QStringLiteral("Other menu locations unavailable (%1)").arg(omitted), false);
    }
    if (menu->actions().isEmpty()) {
        add_item(QStringLiteral("No main-menu items configured"), false);
    }
    if (failed) {
        delete menu;
        display(view, QStringLiteral("NickelMenu"),
                {QStringLiteral("Could not connect Nickel's menu item tap handler."), false, false});
        return nullptr;
    }
    return menu;
}
}

bool attach(QWidget *button, QWidget *view) {
    // QAction bridges Nickel's runtime-only tapped() signal to a typed
    // connection without introducing a hidden QPushButton or a private class.
    auto *tap = new QAction(button);
    if (auto *fallback = qobject_cast<QPushButton *>(button)) {
        QObject::connect(fallback, &QPushButton::clicked, tap, &QAction::trigger);
    } else if (button->inherits("MainNavButton")) {
        if (!QObject::connect(button, SIGNAL(tapped()), tap, SIGNAL(triggered()))) {
            delete tap;
            return false;
        }
    } else {
        delete tap;
        return false;
    }
    auto current = std::make_shared<QPointer<QMenu>>();
    NM_LOG("Qt6 launcher attached; platform=%s",
           qPrintable(QGuiApplication::platformName()));

    QObject::connect(tap, &QAction::triggered, button,
                     [button, view, current] {
        if (button->property("nmBusy").toBool()) return;
        if (*current) (*current)->hide();
        auto *menu = build_menu(button, view);
        if (!menu) return;
        *current = menu;
        menu->ensurePolished();
        const auto size = menu->sizeHint();
        menu->popup(button->mapToGlobal(QPoint(button->width() - size.width(), -size.height())));
    });
    return true;
}
}
