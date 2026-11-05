// Weapon animations from the client events go to the server's VR weapon model, moved from ev_hldm.cpp

#include "hud.h"
#include "cl_util.h"
#include "event_api.h"

#include <string>

namespace
{
// Copy of the engine's event API table; gEngfuncs.pEventAPI points to the engine's own table, which must not be changed
event_api_t s_eventAPI;
}

void InterCept_EV_WeaponAnimation(int sequence, int body)
{
	// "Hack" - Since we use server side entities for our weapons bound to VR controllers,
	// and some weapon animations are client side only,
	// we simply send a message up to the server here instead of fixing all places in code
	// where client side animations are used. - Max Makes Mods, 2019-04-13

	std::string cmd = "vr_wpnanim " + std::to_string(sequence) + " " + std::to_string(body);
	gEngfuncs.pfnClientCmd(cmd.data());

	// gEngfuncs.pEventAPI->EV_WeaponAnimation(sequence, body);
}

// Every gEngfuncs.pEventAPI->EV_WeaponAnimation in the client (ev_hldm.cpp events) goes through the copy
void VRSwapWeaponAnimFuncs()
{
	s_eventAPI = *gEngfuncs.pEventAPI;
	s_eventAPI.EV_WeaponAnimation = InterCept_EV_WeaponAnimation;
	gEngfuncs.pEventAPI = &s_eventAPI;
}
