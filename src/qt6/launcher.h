#pragma once
#include "nickelmenu.h"
#include <QString>
#include <QVector>
class QWidget;
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
void attach(QWidget *button, QWidget *view);
}
