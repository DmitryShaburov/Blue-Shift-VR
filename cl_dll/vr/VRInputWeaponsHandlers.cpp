
#include <string>

#include "hud.h"
#include "cl_util.h"
#include "openvr.h"
#include "VRInput.h"

#include "VRWeaponWheel.h"

extern WEAPON* gpActiveSel;

namespace
{
WEAPON* g_pHolsteredWeapon{nullptr};

void HolsterWeapon()
{
	if (gHUD.m_Ammo.m_pWeapon)
	{
		// Select hand
		ServerCmd("weapon_barehand");
		PlaySound("common/wpn_select.wav", 1);
		g_pHolsteredWeapon = gHUD.m_Ammo.m_pWeapon;
		gHUD.m_Ammo.m_pWeapon = nullptr;
	}
}

void SelectWeapon(WEAPON* weapon)
{
	if (weapon == gHUD.m_Ammo.m_pWeapon)
		return;

	g_pHolsteredWeapon = nullptr;
	gpActiveSel = weapon;
	gHUD.m_iKeyBits |= IN_ATTACK;
	gHUD.m_Ammo.Think();
}

#ifndef MAX_COMMAND_SIZE
#define MAX_COMMAND_SIZE 256
#endif

} // namespace

namespace VR
{
namespace Input
{
void Weapons::HandleFire(const vr::InputDigitalActionData_t& data, const std::string& action)
{
	g_vrInput.SetFiring(g_vrInput.GetRole(data.activeOrigin), data.bActive && data.bState);

	// BSVR start
	// When SteamVR takes the input (dashboard), the release comes as inactive with no controller, so the hand
	// can't be checked. Release anyway, or the weapon keeps firing until the trigger is pressed again.
	if (data.bChanged && !data.bActive)
	{
		EngineClientCmd("-attack");
		return;
	}
	// BSVR end

	if (!g_vrInput.IsWeapon(g_vrInput.GetRole(data.activeOrigin)))
		return;

	if (data.bChanged)
	{
		if (data.bActive && data.bState)
		{
			EngineClientCmd("+attack");
		}
		else
		{
			EngineClientCmd("-attack");
		}
	}
}

void Weapons::HandleAltFire(const vr::InputDigitalActionData_t& data, const std::string& action)
{
	// BSVR start
	// Same as in HandleFire: a release when SteamVR takes the input comes with no controller
	if (data.bChanged && !data.bActive)
	{
		EngineClientCmd("-attack2");
		return;
	}
	// BSVR end

	if (!g_vrInput.IsWeapon(g_vrInput.GetRole(data.activeOrigin)))
		return;

	if (data.bChanged)
	{
		if (data.bActive && data.bState)
		{
			EngineClientCmd("+attack2");
		}
		else
		{
			EngineClientCmd("-attack2");
		}
	}
}

void Weapons::HandleReload(const vr::InputDigitalActionData_t& data, const std::string& action)
{
	if (data.bChanged)
	{
		if (data.bActive && data.bState)
			EngineClientCmd("+reload");
		else
			EngineClientCmd("-reload");
	}
}

void Weapons::HandleHolster(const vr::InputDigitalActionData_t& data, const std::string& action)
{
	if (data.bActive && data.bState && data.bChanged)
	{
		if (gHUD.m_Ammo.m_pWeapon)
		{
			HolsterWeapon();
		}
		else if (g_pHolsteredWeapon)
		{
			// Select holstered weapon
			gpActiveSel = gHUD.m_Ammo.m_pWeapon = g_pHolsteredWeapon;
			g_pHolsteredWeapon = nullptr;
			gHUD.m_iKeyBits |= IN_ATTACK;
			gHUD.m_Ammo.Think();
		}
		else
		{
			// No holstered weapon, select any weapon
			gHUD.m_Ammo.UserCmd_NextWeapon();
			gHUD.m_iKeyBits |= IN_ATTACK;
			gHUD.m_Ammo.Think();
		}
	}
}

void Weapons::HandleNext(const vr::InputDigitalActionData_t& data, const std::string& action)
{
	if (data.bActive && data.bState && data.bChanged)
	{
		if (gHUD.m_Ammo.IsCurrentWeaponLastWeapon())
		{
			// Select hand when current weapon is last weapon
			ServerCmd("weapon_barehand");
			PlaySound("common/wpn_select.wav", 1);
			gHUD.m_Ammo.m_pWeapon = nullptr;
		}
		else
		{
			gHUD.m_Ammo.UserCmd_NextWeapon();
			gHUD.m_iKeyBits |= IN_ATTACK;
			gHUD.m_Ammo.Think();
		}
	}
}

void Weapons::HandlePrevious(const vr::InputDigitalActionData_t& data, const std::string& action)
{
	if (data.bActive && data.bState && data.bChanged)
	{
		if (gHUD.m_Ammo.IsCurrentWeaponFirstWeapon())
		{
			// Select hand when current weapon is first weapon
			ServerCmd("weapon_barehand");
			PlaySound("common/wpn_select.wav", 1);
			gHUD.m_Ammo.m_pWeapon = nullptr;
		}
		else
		{
			gHUD.m_Ammo.UserCmd_PrevWeapon();
			gHUD.m_iKeyBits |= IN_ATTACK;
			gHUD.m_Ammo.Think();
		}
	}
}

void Weapons::HandleSelect(const vr::InputDigitalActionData_t& data, const std::string& action)
{
	// When SteamVR takes the input (dashboard) or the controller goes away, the release comes as inactive.
	// An unbound action is inactive all the time but never reports a change, so it doesn't close the wheel.
	if (data.bChanged && !data.bActive)
	{
		WeaponWheel::Close();
		return;
	}

	// Hold to show the weapon selection wheel
	if (data.bActive && data.bChanged)
	{
		if (data.bState)
		{
			WeaponWheel::Open();
		}
		else
		{
			const WeaponWheel::Selection selection = WeaponWheel::CloseAndGetSelection();
			if (selection.holster)
				HolsterWeapon();
			else if (selection.weapon)
				SelectWeapon(selection.weapon);
		}
	}
}
} // namespace Input
} // namespace VR
