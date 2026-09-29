#include "executor.h"
#include "capabilities.h"
#include "util.h"
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <memory>
#include <utility>

namespace nm6 {
namespace {
struct StepResult {
    nm_action_result_type_t type = NM_ACTION_RESULT_TYPE_SILENT;
    QString message;
    QString error;
    int skip = 0;
};

struct AsyncState {
    Item item;
    Result result;
    QString last_error;
    int skip = 0;
    qsizetype next = 0;
    std::function<void(Result)> completion;
};

StepResult run_action(const Action &action, std::function<void(bool)> rescan_completion = {}) {
    StepResult step;
    nm_err_set(nullptr);
    auto *result = rescan_completion
        ? rescan_books_full(std::move(rescan_completion))
        : action.function(action.argument.constData());
    const char *error = nm_err();
    if (error || !result) {
        step.error = error ? QString::fromUtf8(error)
                           : QStringLiteral("Action returned no result");
    } else {
        step.type = result->type;
        step.skip = result->skip;
        if (result->type == NM_ACTION_RESULT_TYPE_MSG ||
            result->type == NM_ACTION_RESULT_TYPE_TOAST)
            step.message = QString::fromUtf8(result->msg ? result->msg : "");
    }
    nm_action_result_free(result);
    return step;
}

void apply_step(const StepResult &step, Result &result, QString &last_error,
                int &skip) {
    if (!step.error.isEmpty()) {
        last_error = step.error;
        result.success = false;
        return;
    }
    if (step.type == NM_ACTION_RESULT_TYPE_SKIP) {
        skip = step.skip;
        return;
    }

    result.success = true;
    last_error.clear();
    if (step.type == NM_ACTION_RESULT_TYPE_MSG ||
        step.type == NM_ACTION_RESULT_TYPE_TOAST) {
        if (!result.rich_text.isEmpty()) result.rich_text += QStringLiteral("<hr>");
        result.rich_text += step.message;
        if (step.type == NM_ACTION_RESULT_TYPE_MSG) result.toast_only = false;
    }
}

void finish(Result &result, const QString &last_error) {
    if (last_error.isEmpty()) return;
    if (!result.rich_text.isEmpty()) result.rich_text += QStringLiteral("<hr>");
    result.rich_text += last_error.toHtmlEscaped();
    result.toast_only = false;
}

bool runs_on_worker(const Action &action) {
    if (action.function == nm_action_cmd_output ||
        action.function == nm_action_cmd_spawn ||
        action.function == nm_action_kfmon ||
        action.function == nm_action_kfmon_id)
        return true;
    return action.function == nm_action_nickel_misc &&
           action.argument == QByteArrayLiteral("force_usb_connection");
}

void advance(const std::shared_ptr<AsyncState> &state) {
    while (state->next < state->item.actions.size()) {
        const Action action = state->item.actions.at(state->next++);
        if (state->skip) {
            if (state->skip > 0) --state->skip;
            continue;
        }
        if (!(state->result.success ? action.success : action.failure)) continue;

        if (action.function == nm_action_nickel_misc &&
            action.argument == QByteArrayLiteral("rescan_books_full")) {
            auto step = std::make_shared<StepResult>();
            *step = run_action(action, [state, step](bool completed) {
                if (!completed) step->error = QStringLiteral("Nickel's rescan workflow was destroyed before unplugging");
                apply_step(*step, state->result, state->last_error, state->skip);
                advance(state);
            });
            if (step->error.isEmpty()) return;
            // Launch failed before scheduling a callback; continue failure
            // chains immediately, as for any other action error.
            apply_step(*step, state->result, state->last_error, state->skip);
            continue;
        }

        if (runs_on_worker(action)) {
            auto step = std::make_shared<StepResult>();
            auto *worker = QThread::create([action, step] {
                *step = run_action(action);
            });
            QObject::connect(worker, &QThread::finished,
                             QCoreApplication::instance(), [state, step] {
                apply_step(*step, state->result, state->last_error, state->skip);
                advance(state);
            });
            QObject::connect(worker, &QThread::finished, worker,
                             &QObject::deleteLater);
            worker->start();
            return;
        }

        // Private Nickel calls require this GUI path.
        apply_step(run_action(action), state->result, state->last_error,
                   state->skip);
    }

    finish(state->result, state->last_error);
    state->completion(std::move(state->result));
}
}

Item snapshot(const nm_menu_item_t &item) {
    Item copy{QString::fromUtf8(item.lbl), {}};
    for (auto *action = item.action; action; action = action->next)
        copy.actions.append({action->act, QByteArray(action->arg),
                             action->on_success, action->on_failure});
    return copy;
}

Result execute(const Item &item) {
    Result result;
    QString last_error;
    int skip = 0;
    for (const auto &action : item.actions) {
        if (skip) {
            if (skip > 0) --skip;
            continue;
        }
        if (!(result.success ? action.success : action.failure)) continue;
        apply_step(run_action(action), result, last_error, skip);
    }
    finish(result, last_error);
    return result;
}

void execute_async(Item item, std::function<void(Result)> completion) {
    auto state = std::make_shared<AsyncState>();
    state->item = std::move(item);
    state->completion = std::move(completion);
    // Leave QAction signal delivery before invoking even GUI-thread actions.
    QTimer::singleShot(0, QCoreApplication::instance(), [state] {
        advance(state);
    });
}
}
