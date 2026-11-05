#include "cl_dll.h"

#include "VRGameFunctions.h"
#include "VRClientVGUI.h"

void VRGameFunctions::PrintToConsole(const char* s)
{
	gEngfuncs.Con_DPrintf(s);
}

void VRGameFunctions::OpenMenu()
{
	// like the Esc key. The "escape" command doesn't open the menu, "toggleconsole" (upstream) opens the console with it.
	if (!VRActivateGameMenu())
	{
		// toggleconsole makes the engine reopen the console every time the menu opens, until "hideconsole"
		gEngfuncs.Con_Printf("VR: VGUI2 isn't available, opening the menu with toggleconsole\n");
		gEngfuncs.pfnClientCmd("toggleconsole");
	}
}

void VRGameFunctions::CloseMenu()
{
	gEngfuncs.pfnClientCmd("escape");
}

float VRGameFunctions::GetCVar(const char* cvar)
{
	extern float VRGetCvarFloatUncached(const char* x); // vr/VRCvarCache.cpp
	return VRGetCvarFloatUncached(cvar);
}

void VRGameFunctions::SetCVar(const char* cvar, float value)
{
	gEngfuncs.Cvar_SetValue(cvar, value);
}
