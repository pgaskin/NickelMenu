#include "navigation.h"
#include "launcher.h"
#include <QApplication>
#include <QEvent>
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

class NavMonitor final : public QObject {
public:
    NavMonitor(QApplication *application, Logger log)
        : QObject(application), log_(log) {
        setObjectName(QStringLiteral("nmQt6NavMonitor"));
        application->installEventFilter(this);
    }

    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Show) {
            auto *view = qobject_cast<QWidget *>(watched);
            if (view && view->inherits("MainNavView")) queue_menu(view, log_);
        }
        return false;
    }

private:
    Logger log_;
};
}
bool insert_menu(QWidget *view, Logger log) {
    if (!valid_view(view, log)) return false;
    if (view->findChild<QObject *>(QStringLiteral("nmQt6Button"))) {
        log("REFUSED: NickelMenu button already exists.");
        return false;
    }
    QHBoxLayout *row = nullptr;
    for (auto *candidate : view->findChildren<QHBoxLayout *>()) {
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
    if (view->property("nmQt6MenuQueued").toBool() ||
        view->findChild<QObject *>(QStringLiteral("nmQt6Button"))) return;
    view->setProperty("nmQt6MenuQueued", true);
    // Avoid adding widgets while Nickel's outer loadView caller is still
    // wiring up the navigation. The context cancels this if the view dies.
    QTimer::singleShot(0, view, [view, log] {
        view->setProperty("nmQt6MenuQueued", false);
        insert_menu(view, log);
    });
    log("QUEUED UI insertion for the next event-loop turn.");
}

void monitor_menu(void *self, Logger log) {
    auto *view = static_cast<QWidget *>(self);
    if (!valid_view(view, log)) return;
    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!application->findChild<QObject *>(QStringLiteral("nmQt6NavMonitor"),
                                          Qt::FindDirectChildrenOnly)) {
        new NavMonitor(application, log);
        log("MONITOR installed for later MainNavView show events.");
    }
    queue_menu(view, log);
}
}
