
#include <iostream>
#include <filesystem>
#include <string>
#include <functional>

#include "hud.h"
#include "cl_util.h"
#include "openvr.h"
#include "VRInput.h"
#include "eiface.h"
#include "VRRenderer.h"
#include "VRGameFunctions.h"

namespace VR
{
	namespace Input
	{
		void Interact::HandleGrab(const vr::InputDigitalActionData_t& data, const std::string& action)
		{
			bool on = data.bActive && data.bState;
			g_vrInput.SetDrag(g_vrInput.GetRole(data.activeOrigin), on);
		}

		void Interact::HandleLegacyUse(const vr::InputDigitalActionData_t& data, const std::string& action)
		{
			// BSVR start
			// Send +use/-use when the button state differs from what was sent, not only when OpenVR reports a change:
			// a release while the menu is open (or by the weapon hand, which is ignored) would otherwise leave use held,
			// and holding use slows the player to a third of the speed (PM_CheckParamters).
			// if (g_vrInput.IsWeapon(g_vrInput.GetRole(data.activeOrigin))) - original
			// 	return;
			// if (data.bChanged) { data.bActive && data.bState ? +use : -use } - original
			static bool useSent = false;
			const bool on = data.bActive && data.bState && !g_vrInput.IsWeapon(g_vrInput.GetRole(data.activeOrigin));
			if (on != useSent)
			{
				useSent = on;
				EngineClientCmd(on ? "+use" : "-use");
			}
			// BSVR end
		}

		void Interact::HandleTeleport(const vr::InputDigitalActionData_t& data, const std::string& action)
		{
			if (g_vrInput.IsInMenu())
			{
				// Teleport button re-centers the menu in front of the player
				if (data.bActive && data.bState && data.bChanged)
				{
					g_vrInput.RecenterHLMenu();
				}
				return;
			}

			if (data.bChanged)
			{
				bool on = data.bActive && data.bState;
				ServerCmd(on ? "vrtele 1" : "vrtele 0");
			}
		}

		void Interact::HandleFlashlight(const vr::InputDigitalActionData_t& data, const std::string& action)
		{
			if (CVAR_GET_FLOAT("vr_flashlight_toggle") != 0.f)
			{
				if (data.bActive && data.bState && data.bChanged)
				{
					EngineClientCmd("impulse 100");
				}
			}
			else
			{
				bool on = data.bActive && data.bState;
				if (gHUD.m_Flash.IsOn() != on)
				{
					EngineClientCmd("impulse 100");
				}
			}
		}

	}  // namespace Input
}  // namespace VR
