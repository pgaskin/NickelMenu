#pragma once
class QWidget;
namespace nm {
using Logger = void (*)(const char *);
// `self` is the QWidget base at offset zero, verified for the pinned firmware.
void queue_menu(void *self, Logger);
// Restore the private constructor hook, then use public Qt show events to cover
// later MainNavView instances created by USB or other in-process transitions.
void monitor_menu(void *self, Logger);
bool insert_menu(QWidget *view, Logger);
}
