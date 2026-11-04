// The VR settings shown in the VR Options dialog (VRClientVGUI.cpp), a page per tab.

#include <iterator>

#include "VRSettingsPages.h"
#include "VRGameFunctions.h"

namespace
{
VRSettingItem MakeChoice(const std::string& label, std::vector<std::string> options,
	std::function<float()> getValue, std::function<void(float)> setValue)
{
	VRSettingItem item;
	item.label = label;
	item.type = VRSettingItem::Type::Choice;
	item.options = std::move(options);
	item.getValue = std::move(getValue);
	item.setValue = std::move(setValue);
	return item;
}

// Choice that sets a cvar to the option index, an On/Off setting is { "Off", "On" }
VRSettingItem MakeCvarChoice(const std::string& label, const std::string& cvar, std::vector<std::string> options)
{
	return MakeChoice(label, std::move(options), [cvar]
		{
			return VRGameFunctions::GetCVar(cvar.c_str());
		},
		[cvar](float index)
		{
			VRGameFunctions::SetCVar(cvar.c_str(), index);
		});
}

VRSettingItem MakeStepper(const std::string& label,
	std::function<float()> getValue, std::function<void(float)> setValue,
	float min, float max, float step, const std::string& suffix = "", float displayScale = 1.f)
{
	VRSettingItem item;
	item.label = label;
	item.type = VRSettingItem::Type::Stepper;
	item.getValue = std::move(getValue);
	item.setValue = std::move(setValue);
	item.min = min;
	item.max = max;
	item.step = step;
	item.suffix = suffix;
	item.displayScale = displayScale;
	return item;
}

VRSettingItem MakeCvarStepper(const std::string& label, const std::string& cvar,
	float min, float max, float step, const std::string& suffix = "", float displayScale = 1.f)
{
	return MakeStepper(label, [cvar]
		{
			return VRGameFunctions::GetCVar(cvar.c_str());
		},
		[cvar](float value)
		{
			VRGameFunctions::SetCVar(cvar.c_str(), value);
		},
		min, max, step, suffix, displayScale);
}

// One game unit is 1/37.5 m (METER_TO_UNIT in VRHelper.cpp). World size changes this a little, so the menu says "about".
constexpr float UnitsToCm = 100.f / 37.5f;
constexpr float OneCmInUnits = 37.5f / 100.f;

std::vector<VRSettingItem> HandsPage()
{
	return {
		MakeCvarChoice("Weapon hand",
			"vr_lefthand_mode", {"Right (default)", "Left"}),
		MakeCvarChoice("Walk in the direction of",
			"vr_movement_attachment", {"Free hand", "Weapon hand", "Head (default)"}),
		MakeCvarChoice("Flashlight on",
			"vr_flashlight_attachment", {"Free hand (default)", "Weapon hand", "Head"}),
		MakeCvarChoice("Aim teleport with",
			"vr_teleport_attachment", {"Free hand (default)", "Weapon hand", "Head"}),
	};
}

std::vector<VRSettingItem> MovementPage()
{
	std::vector<VRSettingItem> items;
	items.push_back(MakeCvarChoice("Turning",
		"vr_move_snapturn_enabled", {"Smooth", "Snap, in steps (default)"}));
	if (VRGameFunctions::GetCVar("vr_move_snapturn_enabled") != 0.f)
	{
		items.push_back(MakeCvarStepper("Snap turn step",
			"vr_move_snapturn_angle", 15.f, 90.f, 15.f, "\xC2\xB0"));
	}
	else
	{
		items.push_back(MakeCvarStepper("Smooth turn speed",
			"vr_yawspeed", 60.f, 360.f, 10.f, "\xC2\xB0/s"));
	}

	// acceleration and deceleration are one setting here
	items.push_back(MakeChoice("Instant start and stop", {"Off", "On"}, []
		{
			return (VRGameFunctions::GetCVar("vr_move_instant_accelerate") != 0.f && VRGameFunctions::GetCVar("vr_move_instant_decelerate") != 0.f) ? 1.f : 0.f;
		},
		[](float index)
		{
			VRGameFunctions::SetCVar("vr_move_instant_accelerate", index > 0.5f ? 1.f : 0.f);
			VRGameFunctions::SetCVar("vr_move_instant_decelerate", index > 0.5f ? 1.f : 0.f);
		}));

	// forward, back and sideways speed are one setting here. 270 (the default) is 100%. The top step is 324 (120%),
	// the closest 5% step to the server's limit (sv_maxspeed 320), which caps it. The vr_ cvars are copied to
	// cl_*speed every frame (SyncCvars).
	items.push_back(MakeStepper("Walking speed", []
		{
			return VRGameFunctions::GetCVar("vr_forwardspeed");
		},
		[](float value)
		{
			VRGameFunctions::SetCVar("vr_forwardspeed", value);
			VRGameFunctions::SetCVar("vr_backspeed", value);
			VRGameFunctions::SetCVar("vr_sidespeed", value);
		},
		108.f, 324.f, 13.5f, "%", 100.f / 270.f));

	items.push_back(MakeCvarChoice("Turn with moving trains",
		"vr_dont_rotate_with_trains", {"On", "Off"}));
	return items;
}

std::vector<VRSettingItem> BodyPage()
{
	std::vector<VRSettingItem> items;
	items.push_back(MakeCvarStepper("World size",
		"vr_world_scale", 0.5f, 1.5f, 0.01f, "%", 100.f));

	// shown in cm, the range is a whole number of 1 cm steps so the default 0 is one of them
	items.push_back(MakeCvarStepper("Raise or lower view",
		"vr_headset_offset", -130.f * OneCmInUnits, 130.f * OneCmInUnits, OneCmInUnits, " cm", UnitsToCm));

	items.push_back(MakeCvarChoice("Crouch for real",
		"vr_rl_ducking_enabled", {"Off", "On"}));
	if (VRGameFunctions::GetCVar("vr_rl_ducking_enabled") != 0.f)
	{
		// 25 to 160 cm in 1 cm steps, the default 36 units (96 cm) is one of them
		items.push_back(MakeCvarStepper("Crouch when head is below",
			"vr_rl_duck_height", 25.f * OneCmInUnits, 160.f * OneCmInUnits, OneCmInUnits, " cm", UnitsToCm));
	}
	items.push_back(MakeCvarChoice("Auto-crouch in low spaces",
		"vr_autocrouch_enabled", {"Off", "On"}));
	return items;
}

std::vector<VRSettingItem> GameplayPage()
{
	return {
		MakeCvarChoice("Grenade throwing",
			"vr_weapon_grenade_mode", {"Realistic, hand speed (default)", "Classic, fixed speed"}),
		MakeCvarChoice("Laser sight on guns",
			"vr_enable_aim_laser", {"Off", "On"}),
		MakeCvarChoice("Ladder climbing",
			"vr_ladder_mode", {"Hands or stick", "Hands only", "Stick only, classic (default)"}),
		MakeCvarChoice("Aiming at friendly characters",
			"vr_npc_gunpoint", {"No reaction", "They warn you (default)", "They warn, then turn hostile"}),
		MakeCvarStepper("Swing needed for melee hits",
			"vr_melee_swing_speed", 20.f, 300.f, 10.f),
	};
}

std::vector<VRSettingItem> HudAndMenuPage()
{
	std::vector<VRSettingItem> items;
	// vr_hud_mode: 0 off, 1 in view (not offered), 2 on the controllers
	items.push_back(MakeChoice("Health and ammo display", {"Off", "On controllers (default)"}, []
		{
			return VRGameFunctions::GetCVar("vr_hud_mode") != 0.f ? 1.f : 0.f;
		},
		[](float index)
		{
			VRGameFunctions::SetCVar("vr_hud_mode", index != 0.f ? 2.f : 0.f);
		}));
	if (VRGameFunctions::GetCVar("vr_hud_mode") != 0.f)
	{
		items.push_back(MakeCvarChoice("Show health", "vr_hud_health", {"Off", "On"}));
		items.push_back(MakeCvarChoice("Show ammo", "vr_hud_ammo", {"Off", "On"}));
	}

	// Size and height apply when the menu is placed again (opened or recentered), the opacity at once
	items.push_back(MakeCvarStepper("Menu size",
		"vr_menu_scale", 0.5f, 2.f, 0.1f, "%", 100.f));
	items.push_back(MakeCvarStepper("Menu opacity",
		"vr_menu_opacity", 0.3f, 1.f, 0.05f, "%", 100.f));
	items.push_back(MakeCvarStepper("Menu height (from eye level)",
		"vr_menu_height", -0.6f, 0.3f, 0.05f, " cm", 100.f));
	return items;
}

struct SettingsPage
{
	const char* name;
	std::vector<VRSettingItem> (*getItems)();
};
const SettingsPage SettingsPages[] = {
	{"Hands", HandsPage},
	{"Movement and Comfort", MovementPage},
	{"Body and Height", BodyPage},
	{"Gameplay", GameplayPage},
	{"HUD and Menu", HudAndMenuPage},
};
}

std::vector<std::string> VRGetSettingsPageNames()
{
	std::vector<std::string> names;
	for (const SettingsPage& page : SettingsPages)
		names.push_back(page.name);
	return names;
}

std::vector<VRSettingItem> VRGetSettingsPageItems(int page)
{
	if (page < 0 || page >= (int)std::size(SettingsPages))
		return {};
	return SettingsPages[page].getItems();
}
