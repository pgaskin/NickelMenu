#include "launcher.h"
#include "config.h"
#include "util.h"
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVariant>
#include <QWindow>
#include <memory>

namespace nm6 {
// Public Qt state only: do not inspect Nickel's private window structures.
static void log_window(const char *stage, QWidget *widget) {
    const QRect r = widget->geometry();
    auto *window = widget->windowHandle();
    auto *modal = QApplication::activeModalWidget();
    auto *popup = QApplication::activePopupWidget();
    NM_LOG("Qt6 UI %s object=%s visible=%d enabled=%d rect=%d,%d,%d,%d exposed=%d active=%d modal=%s popup=%s",
           stage, qPrintable(widget->objectName()), widget->isVisible(), widget->isEnabled(),
           r.x(), r.y(), r.width(), r.height(), window && window->isExposed(),
           widget->isActiveWindow(), modal ? modal->metaObject()->className() : "none",
           popup ? popup->metaObject()->className() : "none");
}

class WindowTrace final : public QObject {
public:
    explicit WindowTrace(QWidget *widget) : QObject(widget) { widget->installEventFilter(this); }
    bool eventFilter(QObject *watched, QEvent *event) override {
        const char *stage = nullptr;
        switch (event->type()) {
        case QEvent::Show: stage = "show"; break;
        case QEvent::Hide: stage = "hide"; break;
        case QEvent::Close: stage = "close"; break;
        case QEvent::WindowActivate: stage = "activate"; break;
        case QEvent::WindowDeactivate: stage = "deactivate"; break;
        default: break;
        }
        if (stage) log_window(stage, static_cast<QWidget *>(watched));
        return false;
    }
};

Item snapshot(const nm_menu_item_t &item) {
    Item copy{QString::fromUtf8(item.lbl), {}};
    for (auto *a = item.action; a; a = a->next)
        copy.actions.append({a->act, QByteArray(a->arg), a->on_success, a->on_failure});
    return copy;
}

Result execute(const Item &item) {
    Result out;
    QString last_error;
    int skip = 0;
    for (const auto &a : item.actions) {
        if (skip) { if (skip > 0) --skip; continue; }
        if (!(out.success ? a.success : a.failure)) continue;
        nm_err_set(nullptr);
        auto *result = a.function(a.argument.constData());
        const char *error = nm_err();
        if (error || !result) {
            last_error = error ? QString::fromUtf8(error) : QStringLiteral("Action returned no result");
            out.success = false;
        } else if (result->type == NM_ACTION_RESULT_TYPE_SKIP) {
            skip = result->skip; // preserve success/failure and outstanding error
        } else {
            out.success = true;
            last_error.clear();
            if (result->type == NM_ACTION_RESULT_TYPE_MSG || result->type == NM_ACTION_RESULT_TYPE_TOAST) {
                if (!out.rich_text.isEmpty()) out.rich_text += QStringLiteral("<hr>");
                out.rich_text += QString::fromUtf8(result->msg ? result->msg : "");
                if (result->type == NM_ACTION_RESULT_TYPE_MSG) out.toast_only = false;
            }
        }
        nm_action_result_free(result);
    }
    if (!last_error.isEmpty()) {
        if (!out.rich_text.isEmpty()) out.rich_text += QStringLiteral("<hr>");
        out.rich_text += last_error.toHtmlEscaped();
        out.toast_only = false;
    }
    return out;
}

static void display(QWidget *view, const QString &title, const Result &result) {
    if (result.rich_text.isEmpty()) return;
    auto *box = new QMessageBox(result.success ? QMessageBox::Information : QMessageBox::Warning,
                                title, result.rich_text, QMessageBox::Ok, view);
    box->setObjectName(QStringLiteral("nmQt6Result"));
    box->setTextFormat(Qt::RichText);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowModality(Qt::WindowModal);
    new WindowTrace(box);
    QObject::connect(box, &QDialog::finished, view, [view](int code) {
        NM_LOG("Qt6 dialog finished: %d", code);
        QTimer::singleShot(0, view, [view] { log_window("after-dialog", view); });
    });
    box->open(); // no nested modal event loop
    log_window("dialog-open", box);
    if (result.toast_only) QTimer::singleShot(1500, box, &QDialog::accept);
}

void attach(QWidget *widget, QWidget *view) {
    auto *button = qobject_cast<QPushButton *>(widget);
    Q_ASSERT(button);
    auto current = std::make_shared<QPointer<QMenu>>();
    NM_LOG("Qt6 launcher attached: popup-lifecycle-v2 platform=%s", qPrintable(QGuiApplication::platformName()));
    QObject::connect(button, &QPushButton::clicked, button, [button, view, current] {
        log_window("launcher-click", button);
        if (button->property("nmBusy").toBool()) return;
        // Recreate the popup instead of retaining a native popup window across
        // modal dialog cycles. Retire asynchronously: a click can be forwarded
        // from a popup's own event handler, so immediate deletion is unsafe.
        if (*current) {
            (*current)->hide();
            (*current)->setObjectName(QString());
            (*current)->deleteLater();
        }
        auto *menu = new QMenu(button);
        *current = menu;
        menu->setObjectName(QStringLiteral("nmQt6Menu"));
        menu->setStyleSheet(QStringLiteral("QMenu { color: black; background: white; border: 2px solid black; } QMenu::item { padding: 12px 20px; } QMenu::item:selected { color: white; background: black; }"));
        new WindowTrace(menu);
        QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
        // Only refresh on user request; never start actions or generators at
        // startup. Deep snapshots keep callbacks valid after a config reload.
        nm_global_config_update();
        nm_err_set(nullptr); // parser provides a Config Error item on failure
        size_t count = 0;
        auto **items = nm_global_config_items(&count);
        size_t omitted = 0;
        for (size_t i = 0; items && i < count; ++i) {
            if (items[i]->loc != NM_MENU_LOCATION(main)) { ++omitted; continue; }
            const Item item = snapshot(*items[i]);
            auto *action = menu->addAction(item.label);
            QObject::connect(action, &QAction::triggered, button, [button, view, menu, item] {
                if (button->property("nmBusy").toBool()) return;
                menu->hide();
                button->setProperty("nmBusy", true);
                button->setEnabled(false);
                auto result = std::make_shared<Result>();
                // Enabled actions use no private Nickel or GUI APIs. Keep
                // blocking QProcess waits off Nickel's UI thread.
                auto *worker = QThread::create([item, result] { *result = execute(item); });
                QObject::connect(worker, &QThread::finished, button, [button, view, item, result] {
                    button->setProperty("nmBusy", false);
                    button->setEnabled(true);
                    NM_LOG("Qt6 action chain complete; success=%d", result->success);
                    display(view, item.label, *result);
                });
                QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
                worker->start();
            });
        }
        if (omitted) {
            auto *warning = menu->addAction(QStringLiteral("Other menu locations unavailable (%1)").arg(omitted));
            warning->setEnabled(false);
        }
        if (menu->actions().isEmpty()) {
            auto *empty = menu->addAction(QStringLiteral("No main-menu items configured"));
            empty->setEnabled(false);
        }
        NM_LOG("Qt6 menu rebuilt: %lld entries", static_cast<long long>(menu->actions().size()));
        const auto height = menu->sizeHint().height();
        menu->popup(button->mapToGlobal(QPoint(0, -height)));
        log_window("popup-return", menu);
        QTimer::singleShot(0, menu, [menu] { log_window("popup-next-turn", menu); });
        QTimer::singleShot(250, menu, [menu] { log_window("popup-250ms", menu); });
    });
}
}
