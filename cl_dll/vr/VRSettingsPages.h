#pragma once

// The VR settings (VRSettingsPages.cpp), shown by the VR Options dialog in the game's own menu (VRClientVGUI.cpp).
// Only standard headers here: VRClientVGUI.cpp is built with the VGUI2 SDK's headers instead of the game's.

#include <functional>
#include <string>
#include <vector>

struct VRSettingItem
{
	enum class Type
	{
		Choice,
		Stepper
	};

	std::string label;
	Type type = Type::Choice;

	// Choice: the option index. Stepper: the value.
	std::function<float()> getValue;
	std::function<void(float)> setValue;

	// Choice
	std::vector<std::string> options;

	// Stepper: shown as value * displayScale followed by suffix
	float min = 0.f;
	float max = 1.f;
	float step = 0.1f;
	std::string suffix;
	float displayScale = 1.f;
};

// The settings pages in menu order (Hands, Movement and Comfort, ...)
std::vector<std::string> VRGetSettingsPageNames();

// Some items depend on other settings (e.g. "Snap angle" only exists with snap turning), so get them again after a change
std::vector<VRSettingItem> VRGetSettingsPageItems(int page);
