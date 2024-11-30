#ifndef NP_GUI_INTERFACE_HPP
#define NP_GUI_INTERFACE_HPP

#include <QtPlugin>

#define NPGuiInterfaceIID "NickelPlugin.GuiInterface/1.1"

class NPGuiInterface
{
    public:
        virtual ~NPGuiInterface() = default;

        virtual void showUi() = 0;
};
Q_DECLARE_INTERFACE(NPGuiInterface, NPGuiInterfaceIID);

#endif // NP_GUI_INTERFACE_HPP
