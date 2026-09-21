#pragma once
class QWidget;
namespace nm {
using Logger = void (*)(const char *);
// `self` is the QWidget base at offset zero, verified for the pinned firmware.
void queue_menu(void *self, Logger);
bool insert_menu(QWidget *view, Logger);
}
