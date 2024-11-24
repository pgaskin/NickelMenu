#ifndef NM_EXAMPLE_PLUGIN_H
#define NM_EXAMPLE_PLUGIN_H

#include <QObject>
#include <QtPlugin>

#include "../MNGuiInterface.h"

class NMExamplePlugin : public QObject, public MNGuiInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "NickelMenu.GuiInterface")
    Q_INTERFACES(MNGuiInterface)

    public:
        void runPlugin();
};

#endif // NM_EXAMPLE_PLUGIN_H
