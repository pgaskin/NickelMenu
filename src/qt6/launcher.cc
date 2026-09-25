#include "launcher.h"
#include "config.h"
#include "executor.h"
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

void run(Item item, QPushButton *button, QWidget *view, QMenu *menu) {
    if (button->property("nmBusy").toBool()) return;
    menu->hide();
    button->setProperty("nmBusy", true);
    button->setEnabled(false);

    const QString title = item.label;
    const QPointer<QPushButton> button_guard(button);
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

QMenu *build_menu(QPushButton *button, QWidget *view) {
    auto *menu = new QMenu(button);
    menu->setObjectName(QStringLiteral("nmQt6Menu"));
    // Inherit the font and palette copied from Nickel's live MainNavButton.
    // Leaving QMenu unstyled also lets Nickel's application style provide its
    // normal selection colours instead of imposing a fixed black/white theme.
    menu->setFont(button->font());
    menu->setPalette(button->palette());
    QObject::connect(menu, &QMenu::aboutToHide, menu, [menu] {
        // Kobo's QPA cannot reliably reuse a popup after a modal dialog.
        menu->setObjectName(QString());
        menu->deleteLater();
    });

    nm_global_config_update();
    nm_err_set(nullptr); // parse failures are represented by a Config Error item
    size_t count = 0;
    auto **items = nm_global_config_items(&count);
    size_t omitted = 0;
    for (size_t i = 0; items && i < count; ++i) {
        if (items[i]->loc != NM_MENU_LOCATION(main)) {
            ++omitted;
            continue;
        }
        const Item item = snapshot(*items[i]);
        auto *action = menu->addAction(item.label);
        QObject::connect(action, &QAction::triggered, button,
                         [button, view, menu, item] {
            run(item, button, view, menu);
        });
    }
    if (omitted) {
        auto *warning = menu->addAction(
            QStringLiteral("Other menu locations unavailable (%1)").arg(omitted));
        warning->setEnabled(false);
    }
    if (menu->actions().isEmpty()) {
        auto *empty = menu->addAction(
            QStringLiteral("No main-menu items configured"));
        empty->setEnabled(false);
    }
    return menu;
}
}

void attach(QWidget *widget, QWidget *view) {
    auto *button = qobject_cast<QPushButton *>(widget);
    Q_ASSERT(button);
    auto current = std::make_shared<QPointer<QMenu>>();
    NM_LOG("Qt6 launcher attached; platform=%s",
           qPrintable(QGuiApplication::platformName()));

    QObject::connect(button, &QPushButton::clicked, button,
                     [button, view, current] {
        if (button->property("nmBusy").toBool()) return;
        if (*current) (*current)->hide();
        auto *menu = build_menu(button, view);
        *current = menu;
        const int height = menu->sizeHint().height();
        menu->popup(button->mapToGlobal(QPoint(0, -height)));
    });
}
}
