#pragma once

#include "nickelmenu.h"
#include <QByteArray>
#include <QString>
#include <QVector>
#include <functional>

namespace nm6 {
struct Action {
    nm_action_fn_t function;
    QByteArray argument;
    bool success;
    bool failure;
};

struct Item {
    QString label;
    QVector<Action> actions;
};

struct Result {
    QString rich_text;
    bool toast_only = true;
    bool success = true;
};

Item snapshot(const nm_menu_item_t &);
Result execute(const Item &);

// Starts on the GUI thread. Blocking actions are dispatched to workers and the
// completion callback runs on the GUI thread. The chain owns a deep Item copy.
void execute_async(Item, std::function<void(Result)> completion);
}
