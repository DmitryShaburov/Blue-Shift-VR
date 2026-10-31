#pragma once

// VGUI2 in the client (VRClientVGUI.cpp): the VR Options dialog in the game's own menu.
// Only plain types here, it's included by files built with the game's headers and by VRClientVGUI.cpp, which uses the VGUI2 SDK's.

// The client's IClientVGUI ("VClientVGUI001"), as CreateInterface returns it. Registered for the engine in VRClientVGUIExport.cpp.
void* VRGetClientVGUI();

// Opens the VR Options dialog on top of the game's menu (console command vr_options, used by resource/GameMenu.res).
// Returns why it couldn't, or nullptr.
const char* VROpenOptionsDialog();

// Opens the game's menu like the Esc key, without the console. False if the engine's UI interface isn't available.
bool VRActivateGameMenu();
