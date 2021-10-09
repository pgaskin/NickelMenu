#include <QApplication>
#include <QComboBox>
#include <QTouchEvent>

#include "NMDialog.h"

const char touchFilterSet[] = "touchFilterSet";

NMDialog::NMDialog(QWidget *parent) : QDialog(parent)
{
    setAttribute(Qt::WA_AcceptTouchEvents);
}

NMDialog::~NMDialog()
{
}

bool NMDialog::eventFilter(QObject *obj, QEvent *event)
{
    auto type = event->type();
    if (type == QEvent::TouchBegin || type == QEvent::TouchUpdate || type == QEvent::TouchEnd) {
        event->accept();
        auto tp = static_cast<QTouchEvent*>(event)->touchPoints().at(0);
        if (type == QEvent::TouchBegin) {
            QMouseEvent md(QEvent::MouseButtonPress, tp.pos(), tp.screenPos(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(obj, &md);
        } else if (type == QEvent::TouchUpdate) {
            QMouseEvent mm(QEvent::MouseMove, tp.pos(), tp.screenPos(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(obj, &mm);
        } else if (type == QEvent::TouchEnd) {
            QMouseEvent mu(QEvent::MouseButtonRelease, tp.pos(), tp.screenPos(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(obj, &mu);
        }
        return true;
    }
    return false;
}

void NMDialog::installEvFilter(QWidget *w)
{
    if (!w->property(touchFilterSet).isValid()) {
        w->setProperty(touchFilterSet, QVariant(true));
        w->setAttribute(Qt::WA_AcceptTouchEvents);
        w->installEventFilter(this);
    }
}

void NMDialog::showDlg()
{
    auto children = findChildren<QWidget*>();
    for (int i = 0; i < children.size(); ++i) {
        if (auto wi = children.at(i)) {
            installEvFilter(wi);
        }
    }
    open();
}