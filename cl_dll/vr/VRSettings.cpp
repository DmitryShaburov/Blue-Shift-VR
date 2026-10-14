#include "VRSettings.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <regex>

#define NOGDI
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN

#ifdef _WIN32
#define HSPRITE WINDOWS_HSPRITE
#include <Windows.h>
#undef HSPRITE
#endif

#include "hud.h"
#include "cl_util.h"
#include "VRCommon.h"
// #include "VRSpeechListener.h"

VRSettings g_vrSettings;

constexpr const char* VR_DEFAULT_VIEW_DIST_TO_WALLS_AS_STRING = "5";  // for cvar


void VRSettings::Init()
{
	// These are separate, we don't store them and don't make them accessible through HLVRConfig's config menu
	CVAR_CREATE("vr_debug_physics", "0", 0);
	CVAR_CREATE("vr_debug_controllers", "0", 0);
	CVAR_CREATE("vr_noclip", "0", 0);
	CVAR_CREATE("vr_cheat_enable_healing_exploit", "0", 0);
	CVAR_CREATE("vr_show_achievements", "0", 0);

	// These are all stored in hlsettings.cfg and synchronized with HLVRConfig
	RegisterCVAR("vr_357_scale", "1");
	RegisterCVAR("vr_9mmar_scale", "1");
	RegisterCVAR("vr_9mmhandgun_scale", "1");
	RegisterCVAR("vr_crossbow_scale", "1");
	RegisterCVAR("vr_crowbar_scale", "1");
	RegisterCVAR("vr_crowbar_vanilla_attack_enabled", "0");
	RegisterCVAR("vr_disable_func_friction", "1");
	//RegisterCVAR("vr_disable_triggerpush", "0");
	RegisterCVAR("vr_egon_scale", "1");
	RegisterCVAR("vr_enable_aim_laser", "0");
	RegisterCVAR("vr_speech_commands_enabled", "0");
	RegisterCVAR("vr_speech_language_id", "1400");
	RegisterCVAR("vr_flashlight_attachment", "0");
	RegisterCVAR("vr_flashlight_toggle", "1");
	RegisterCVAR("vr_movement_attachment", "2"); // BSVR changed to HMD
	RegisterCVAR("vr_force_introtrainride", "1");
	RegisterCVAR("vr_gauss_scale", "1");
	RegisterCVAR("vr_gordon_hand_scale", "1");
	RegisterCVAR("vr_grenade_scale", "1");
	RegisterCVAR("vr_hgun_scale", "1");
	RegisterCVAR("vr_hud_ammo", "1");
	RegisterCVAR("vr_hud_ammo_offset_x", "3");
	RegisterCVAR("vr_hud_ammo_offset_y", "-3");
	RegisterCVAR("vr_hud_damage_indicator", "1");
	RegisterCVAR("vr_hud_flashlight", "1");
	RegisterCVAR("vr_hud_flashlight_offset_x", "-5");
	RegisterCVAR("vr_hud_flashlight_offset_y", "2");
	RegisterCVAR("vr_hud_health", "1");
	RegisterCVAR("vr_hud_health_offset_x", "-4");
	RegisterCVAR("vr_hud_health_offset_y", "-3");
	RegisterCVAR("vr_hud_mode", "2");
	RegisterCVAR("vr_hud_size", "1");
	RegisterCVAR("vr_hud_textscale", "1");

	RegisterCVAR("vr_ladder_mode", "2"); // BSVR changed to VR_LADDER_MODE_LEGACY_ONLY
	RegisterCVAR("vr_ladder_immersive_movement_swinging_enabled", "1");
	RegisterCVAR("vr_ladder_legacy_movement_speed", "200");
	RegisterCVAR("vr_ladder_legacy_sideways_speed", "50");

	RegisterCVAR("vr_ledge_pull_mode", "1");
	RegisterCVAR("vr_ledge_pull_speed", "50");
	RegisterCVAR("vr_lefthand_mode", "0");
	RegisterCVAR("vr_train_controls", "0");
	RegisterCVAR("vr_train_autostop", "0");
	RegisterCVAR("vr_train_speed_slow", "0.2");
	RegisterCVAR("vr_train_speed_medium", "0.4");
	RegisterCVAR("vr_train_speed_fast", "0.6");
	RegisterCVAR("vr_train_speed_back", "0.2");
	RegisterCVAR("vr_melee_swing_speed", "100");

	RegisterCVAR("vr_move_analogforward_inverted", "0");
	RegisterCVAR("vr_move_analogsideways_inverted", "0");
	RegisterCVAR("vr_move_analogturn_inverted", "0");
	RegisterCVAR("vr_move_analogupdown_inverted", "0");
	RegisterCVAR("vr_move_analog_deadzone", "0.1");
	RegisterCVAR("vr_move_instant_accelerate", "1");
	RegisterCVAR("vr_move_instant_decelerate", "1");
	RegisterCVAR("vr_move_snapturn_enabled", "1");
	RegisterCVAR("vr_move_snapturn_angle", "30");

	RegisterCVAR("vr_forwardspeed", "270");		// cl_forwardspeed
	RegisterCVAR("vr_backspeed", "270");		// cl_backspeed
	RegisterCVAR("vr_sidespeed", "270");		// cl_sidespeed
	RegisterCVAR("vr_upspeed", "270");			// cl_upspeed
	RegisterCVAR("vr_yawspeed", "210");			// cl_yawspeed
	RegisterCVAR("vr_walkspeedfactor", "0.3");	// cl_movespeedkey
	RegisterCVAR("vr_togglewalk", "0");

	RegisterCVAR("vr_toggle_crouch", "0");

	RegisterCVAR("vr_no_gauss_recoil", "1");
	RegisterCVAR("vr_npcscale", "1");
	//RegisterCVAR("vr_playerturn_enabled", "1");
	RegisterCVAR("vr_rl_duck_height", "36");
	RegisterCVAR("vr_rl_ducking_enabled", "1");
	RegisterCVAR("vr_dont_rotate_with_trains", "1");
	RegisterCVAR("vr_rpg_scale", "1");
	RegisterCVAR("vr_satchel_radio_scale", "1");
	RegisterCVAR("vr_satchel_scale", "1");
	RegisterCVAR("vr_semicheat_spinthingyspeed", "50");
	RegisterCVAR("vr_shotgun_scale", "1");
	RegisterCVAR("vr_squeak_scale", "1");
	RegisterCVAR("vr_teleport_attachment", "0");
	RegisterCVAR("vr_tripmine_scale", "1");
	RegisterCVAR("vr_use_animated_weapons", "0");
	RegisterCVAR("vr_view_dist_to_walls", VR_DEFAULT_VIEW_DIST_TO_WALLS_AS_STRING);
	RegisterCVAR("vr_weapon_grenade_mode", "0");
	RegisterCVAR("vr_weaponscale", "1");
	RegisterCVAR("vr_world_scale", "0.97");
	RegisterCVAR("vr_world_z_strech", "1");
	RegisterCVAR("vr_xenjumpthingies_teleporteronly", "0");
	RegisterCVAR("vr_autocrouch_enabled", "0"); // BSVR changed to disabled
	RegisterCVAR("vr_tankcontrols", "2");
	RegisterCVAR("vr_tankcontrols_max_distance", "128");
	RegisterCVAR("vr_legacy_tankcontrols_enabled", "0");
	RegisterCVAR("vr_make_levers_nonsolid", "1");
	RegisterCVAR("vr_make_mountedguns_nonsolid", "1");
	RegisterCVAR("vr_tankcontrols_instant_turn", "0");
	RegisterCVAR("vr_smooth_steps", "0");
	RegisterCVAR("vr_smooth_steps_intensity", "0.5");
	RegisterCVAR("vr_headset_offset", "0");
	RegisterCVAR("vr_classic_mode", "0");
	RegisterCVAR("vr_enable_interactive_debris", "1");
	RegisterCVAR("vr_max_interactive_debris", "50");
	RegisterCVAR("vr_drag_onlyhand", "0");
	RegisterCVAR("vr_npc_gunpoint", "1");
	RegisterCVAR("vr_eye_mode", "0");
	RegisterCVAR("vr_breakable_gib_percentage", "100");
	RegisterCVAR("vr_breakable_max_gibs", "50");
	RegisterCVAR("vr_blastpit_fan_nonsolid", "1");
	RegisterCVAR("vr_blastpit_fan_delay", "1");

	RegisterCVAR("vr_texturemode", "GL_LINEAR_MIPMAP_LINEAR");
	RegisterCVAR("vr_display_game", "0");

	RegisterCVAR("vr_menu_scale", "1");
	RegisterCVAR("vr_menu_height", "-0.3");  // meters relative to eye level
	RegisterCVAR("vr_menu_opacity", "0.99");
}

void SyncCvars(const char* src, const char* dst)
{
	gEngfuncs.Cvar_SetValue(dst, gEngfuncs.pfnGetCvarFloat(src));
}

void VRSettings::CheckCVARsForChanges()
{
	// make sure these are always properly set
	gEngfuncs.pfnClientCmd("gl_vsync 0");
	gEngfuncs.pfnClientCmd("default_fov 180");
	gEngfuncs.pfnClientCmd("cl_mousegrab 0");
	//gEngfuncs.pfnClientCmd("firstperson");

	// reset cheats if cheats are disabled
	if (gEngfuncs.pfnGetCvarFloat("sv_cheats") == 0.f)
	{
		gEngfuncs.Cvar_SetValue("vr_debug_physics", 0.f);
		gEngfuncs.Cvar_SetValue("vr_debug_controllers", 0.f);
		gEngfuncs.Cvar_SetValue("vr_noclip", 0.f);
		gEngfuncs.Cvar_SetValue("vr_cheat_enable_healing_exploit", 0.f);
	}

	SyncCvars("vr_forwardspeed", "cl_forwardspeed");
	SyncCvars("vr_backspeed", "cl_backspeed");
	SyncCvars("vr_sidespeed", "cl_sidespeed");
	SyncCvars("vr_upspeed", "cl_upspeed");
	SyncCvars("vr_yawspeed", "cl_yawspeed");
	SyncCvars("vr_walkspeedfactor", "cl_movespeedkey");

	// classic mode uses unfiltered textures
	if (gEngfuncs.pfnGetCvarFloat("vr_classic_mode") != 0.f)
	{
		gEngfuncs.pfnClientCmd("gl_texturemode GL_NEAREST");
		gEngfuncs.pfnClientCmd("vr_texturemode GL_NEAREST");
	}
	else
	{
		UpdateTextureMode();
	}
}

void SetCVAR(const char* name, const char* value)
{
	gEngfuncs.Cvar_SetValue(const_cast<char*>(name), atof(value));
	gEngfuncs.pfnClientCmd((std::string(name) + " " + value).data());
}

void LoadCvarsIntoSettings(const std::unordered_map<std::string, std::string>& settings, const std::unordered_map<std::string, std::string>& cvar_cache)
{
	for (auto [cvarname, value] : settings)
	{
		auto cachedcvar = cvar_cache.find(cvarname);
		if (cachedcvar != cvar_cache.end())
		{
			value = cachedcvar->second;
		}
	}
}

void VRSettings::RegisterCVAR(const char* name, const char* value)
{
	CVAR_CREATE(name, value, FCVAR_ARCHIVE);
	m_cvarCache[name] = value;
}

void VRSettings::UpdateTextureMode()
{
	std::string vr_texturemode = CVAR_GET_STRING("vr_texturemode");
	std::transform(vr_texturemode.begin(), vr_texturemode.end(), vr_texturemode.begin(), ::toupper);

	std::string gl_texturemode = CVAR_GET_STRING("gl_texturemode");
	std::transform(gl_texturemode.begin(), gl_texturemode.end(), gl_texturemode.begin(), ::toupper);

	if (vr_texturemode != m_vr_texturemode || vr_texturemode != gl_texturemode)
	{
		if (vr_texturemode == "GL_NEAREST")
		{
			gEngfuncs.pfnClientCmd("gl_texturemode GL_NEAREST");
			gEngfuncs.pfnClientCmd("vr_texturemode GL_NEAREST");
		}
		else if (vr_texturemode == "GL_LINEAR")
		{
			gEngfuncs.pfnClientCmd("gl_texturemode GL_LINEAR");
			gEngfuncs.pfnClientCmd("vr_texturemode GL_LINEAR");
		}
		else if (vr_texturemode == "GL_NEAREST_MIPMAP_NEAREST")
		{
			gEngfuncs.pfnClientCmd("gl_texturemode GL_NEAREST_MIPMAP_NEAREST");
			gEngfuncs.pfnClientCmd("vr_texturemode GL_NEAREST_MIPMAP_NEAREST");
		}
		else if (vr_texturemode == "GL_LINEAR_MIPMAP_NEAREST")
		{
			gEngfuncs.pfnClientCmd("gl_texturemode GL_LINEAR_MIPMAP_NEAREST");
			gEngfuncs.pfnClientCmd("vr_texturemode GL_LINEAR_MIPMAP_NEAREST");
		}
		else if (vr_texturemode == "GL_NEAREST_MIPMAP_LINEAR")
		{
			gEngfuncs.pfnClientCmd("gl_texturemode GL_NEAREST_MIPMAP_LINEAR");
			gEngfuncs.pfnClientCmd("vr_texturemode GL_NEAREST_MIPMAP_LINEAR");
		}
		else /*if (newtexturemode == "GL_LINEAR_MIPMAP_LINEAR")*/
		{
			gEngfuncs.pfnClientCmd("gl_texturemode GL_LINEAR_MIPMAP_LINEAR");
			gEngfuncs.pfnClientCmd("vr_texturemode GL_LINEAR_MIPMAP_LINEAR");
			vr_texturemode = "GL_LINEAR_MIPMAP_LINEAR";
		}
		m_vr_texturemode = vr_texturemode;
	}
}
