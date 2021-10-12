#ifndef NP_GUI_INTERFACE_HPP
#define NP_GUI_INTERFACE_HPP

#include <QtPlugin>

#define NPGuiInterfaceIID "NickelPlugin.GuiInterface/1.0"

class NPGuiInterface
{
    public:
        virtual ~NPGuiInterface() = default;

        virtual void showUI() = 0;
};
Q_DECLARE_INTERFACE(NPGuiInterface, NPGuiInterfaceIID);

#endif // NP_GUI_INTERFACE_HPP
