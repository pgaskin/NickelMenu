#include <Qt>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QRect>
#include <QScreen>
#include <QGuiApplication>

#include "NMDialog.h"

#include "NMExamplePlugin.h"

MNGuiInterface::~MNGuiInterface() {}

void NMExamplePlugin::runPlugin() {
    NMDialog *container = new NMDialog();
    container->setAttribute(Qt::WA_DeleteOnClose);
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect geom = screen->availableGeometry();
    int newW = geom.width() * 0.95;
    int newH = geom.height() * 0.95;
    int newX = (geom.width() - newW) / 2;
    int newY = (geom.height() - newH) / 2;
    container->setGeometry(newX, newY, newW, newH);
    QVBoxLayout *layout = new QVBoxLayout();
    QLabel *lbl = new QLabel();
    lbl->setText(QString("Screen dimensions are:\n%1 x %2").arg(geom.width()).arg(geom.height()));
    layout->addWidget(lbl);
    auto bb = new QDialogButtonBox(QDialogButtonBox::Close, container);
    connect(bb, &QDialogButtonBox::rejected, container, &QDialog::reject);
    layout->addWidget(bb);
    container->setLayout(layout);
    container->showDlg();
}
