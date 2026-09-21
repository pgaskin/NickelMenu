#include "navigation.h"
#include "launcher.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <cstdio>

namespace nm {
namespace {
bool valid_view(QWidget *view, Logger log) {
    if (!qobject_cast<QApplication *>(QCoreApplication::instance()) ||
        QThread::currentThread() != QCoreApplication::instance()->thread()) {
        log("REFUSED: UI insertion requires QApplication's GUI thread.");
        return false;
    }
    if (!view || !view->inherits("MainNavView") || view->thread() != QThread::currentThread()) {
        log("REFUSED: unexpected MainNavView QObject or thread.");
        return false;
    }
    return true;
}
}
bool insert_menu(QWidget *view, Logger log) {
    if (!valid_view(view, log)) return false;
    if (view->findChild<QObject *>(QStringLiteral("nmQt6Button"))) {
        log("REFUSED: diagnostic button already exists.");
        return false;
    }
    QHBoxLayout *row = nullptr;
    int inspected = 0;
    for (auto *candidate : view->findChildren<QHBoxLayout *>()) {
        if (++inspected > 8) {
            log("REFUSED: unexpectedly many horizontal layouts; UI unchanged.");
            return false;
        }
        char details[192];
        snprintf(details, sizeof(details), "ROW candidate=%d count=%d direct_parent=%d",
                 inspected, candidate->count(), candidate->parentWidget() == view);
        log(details);
        for (int i = 0; i < candidate->count() && i < 8; ++i) {
            const auto *widget = candidate->itemAt(i)->widget();
            snprintf(details, sizeof(details), "ROW child=%d class=%.100s", i,
                     widget ? widget->metaObject()->className() : "<not a widget>");
            log(details);
        }
        if (candidate->count() != 6 || candidate->parentWidget() != view) continue;
        bool matches = true;
        for (int i = 0; i < candidate->count(); ++i) {
            auto *widget = candidate->itemAt(i)->widget();
            if (!widget || !widget->inherits("MainNavButton") || widget->parentWidget() != view)
                matches = false;
        }
        if (!matches) continue;
        if (row) { log("REFUSED: ambiguous navigation layout."); return false; }
        row = candidate;
    }
    if (!row) { log("REFUSED: no six-button MainNavButton row; UI unchanged."); return false; }
    log("LAYOUT validated one six-button navigation row; existing widgets retained.");

    auto *button = new QPushButton(QStringLiteral("NickelMenu"), view);
    button->setObjectName(QStringLiteral("nmQt6Button"));
    button->setFocusPolicy(Qt::NoFocus);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    button->setStyleSheet(QStringLiteral("QPushButton { color: black; background: white; border: 2px solid black; padding: 4px; }"));
    nm6::attach(button, view);
    row->addWidget(button);
    button->show();
    log("ADDED NickelMenu launcher; configuration loads when opened.");
    return true;
}

void queue_menu(void *self, Logger log) {
    auto *view = static_cast<QWidget *>(self);
    if (!valid_view(view, log)) return;
    // Avoid adding widgets while Nickel's outer loadView caller is still
    // wiring up the navigation. The context cancels this if the view dies.
    QTimer::singleShot(0, view, [view, log] { insert_menu(view, log); });
    log("QUEUED UI insertion for the next event-loop turn.");
}
}
