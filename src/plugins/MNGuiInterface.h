#ifndef NM_GUI_INTERFACE_H
#define NM_GUI_INTERFACE_H

#include <QtPlugin>
#include "NMDialog.h"

class MNGuiInterface
{
    public:
        virtual ~MNGuiInterface();

        virtual void showPlugin(NMDialog* container);
};
Q_DECLARE_INTERFACE(MNGuiInterface, "NickelMenu.GuiInterface/1.0");

#endif // NM_GUI_INTERFACE_H