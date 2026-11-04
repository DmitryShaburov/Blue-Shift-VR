// Exposes the client's IClientVGUI (VRClientVGUI.cpp) to the engine.
// VRClientVGUI.cpp is built with the VGUI2 SDK's interface.h, this file with the game's, so the interface is registered here,
// in the same list as the client's other interfaces that CreateInterface (public/interface.cpp) looks through.

#include "interface.h"
#include "VRClientVGUI.h"

namespace
{
void* CreateClientVGUI()
{
	return VRGetClientVGUI();
}

InterfaceReg g_ClientVGUIReg(CreateClientVGUI, "VClientVGUI001");
}
