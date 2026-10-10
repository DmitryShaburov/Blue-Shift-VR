/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
// shared event functions
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"

#include "r_efx.h"

#include "eventscripts.h"
#include "event_api.h"
#include "pm_shared.h"

#define IS_FIRSTPERSON_SPEC (g_iUser1 == OBS_IN_EYE || (g_iUser1 && (gHUD.m_Spectator.m_pip->value == INSET_IN_EYE)))
/*
=================
GetEntity

Return's the requested cl_entity_t
=================
*/
struct cl_entity_s* GetEntity(int idx)
{
	return gEngfuncs.GetEntityByIndex(idx);
}

/*
=================
GetViewEntity

Return's the current weapon/view model
=================
*/
struct cl_entity_s* GetViewEntity()
{
	return gEngfuncs.GetViewModel();
}

/*
=================
EV_CreateTracer

Creates a tracer effect
=================
*/
void EV_CreateTracer(float* start, float* end)
{
	gEngfuncs.pEfxAPI->R_TracerEffect(start, end);
}

/*
=================
EV_IsPlayer

Is the entity's index in the player range?
=================
*/
bool EV_IsPlayer(int idx)
{
	if (idx >= 1 && idx <= gEngfuncs.GetMaxClients())
		return true;

	return false;
}

/*
=================
EV_IsLocal

Is the entity == the local player
=================
*/
bool EV_IsLocal(int idx)
{
	// check if we are in some way in first person spec mode
	if (IS_FIRSTPERSON_SPEC)
		return (g_iUser2 == idx);
	else
		return gEngfuncs.pEventAPI->EV_IsLocal(idx - 1) != 0;
}

/*
=================
EV_GetGunPosition

Figure out the height of the gun
=================
*/
// BSVR start - migrate to VR guns
// void EV_GetGunPosition(event_args_t* args, float* pos, float* origin)
// {
// 	int idx;

// 	idx = args->entindex;

// 	Vector view_ofs = VEC_VIEW;

// 	if (EV_IsPlayer(idx))
// 	{
// 		// in spec mode use entity viewheigh, not own
// 		if (EV_IsLocal(idx) && !IS_FIRSTPERSON_SPEC)
// 		{
// 			// Grab predicted result for local player
// 			gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
// 		}
// 		else if (args->ducking == 1)
// 		{
// 			view_ofs = VEC_DUCK_VIEW;
// 		}
// 	}

// 	VectorAdd(origin, view_ofs, pos);
// }

// Gun position and aim vector in VR is given by special model attachments - Max Makes Mods, 2019-03-30 / 2019-04-07
void EV_GetGunPosition(float* pos)
{
	// we are in include hell, so just use an external global function here :/
	extern Vector VRGlobalGetGunPosition();
	VRGlobalGetGunPosition().CopyToArray(pos);
}

void EV_GetGunAim(float* forward, float* right, float* up, float* angles)
{
	// we are in include hell, so just use an external global function here :/
	Vector f, r, u, a;
	extern void VRGlobalGetGunAim(Vector&, Vector&, Vector&, Vector&);
	VRGlobalGetGunAim(f, r, u, a);
	f.CopyToArray(forward);
	r.CopyToArray(right);
	u.CopyToArray(up);
	a.CopyToArray(angles);
}
// BSVR end

/*
=================
EV_EjectBrass

Bullet shell casings
=================
*/
void EV_EjectBrass(float* origin, float* velocity, float rotation, int model, int soundtype)
{
	Vector endpos;
	VectorClear(endpos);
	endpos[1] = rotation;
	gEngfuncs.pEfxAPI->R_TempModel(origin, velocity, endpos, 2.5, model, soundtype);
}

/*
=================
EV_GetDefaultShellInfo

Determine where to eject shells from
=================
*/
void EV_GetDefaultShellInfo(event_args_t* args, float* origin, float* velocity, float* ShellVelocity, float* ShellOrigin, float* forward, float* right, float* up, float forwardScale, float upScale, float rightScale)
{
	// BSVR start commented upstream code
	// int i;
	// float fR, fU;

	// int idx;

	// idx = args->entindex;

	// Vector view_ofs = VEC_VIEW;

	// if (EV_IsPlayer(idx))
	// {
	// 	if (EV_IsLocal(idx))
	// 	{
	// 		gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
	// 	}
	// 	else if (args->ducking == 1)
	// 	{
	// 		view_ofs = VEC_DUCK_VIEW;
	// 	}
	// }

	// fR = gEngfuncs.pfnRandomFloat(50, 70);
	// fU = gEngfuncs.pfnRandomFloat(100, 150);

	// for (i = 0; i < 3; i++)
	// {
	// 	ShellVelocity[i] = velocity[i] + right[i] * fR + up[i] * fU + forward[i] * 25;
	// 	ShellOrigin[i] = origin[i] + view_ofs[i] + up[i] * upScale + forward[i] * forwardScale + right[i] * rightScale;
	// }
	// BSVR end commented upstream code

	// BSVR start
	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel != nullptr)
	{
		float fR = gEngfuncs.pfnRandomFloat(50, 70);
		float fU = gEngfuncs.pfnRandomFloat(100, 150);

		for (int i = 0; i < 3; i++)
		{
			ShellVelocity[i] = viewModel->curstate.velocity[i] + right[i] * fR + up[i] * fU + forward[i] * 25;
			ShellOrigin[i] = viewModel->curstate.origin[i] + up[i] * upScale + forward[i] * forwardScale + right[i] * rightScale;
		}
	}
	// BSVR end
}

/*
=================
EV_MuzzleFlash

Flag weapon/view model for muzzle flash
=================
*/
void EV_MuzzleFlash()
{
	// BSVR start commented upstream code
	// // Add muzzle flash to current weapon model
	// cl_entity_t* ent = GetViewEntity();
	// if (!ent)
	// {
	// 	return;
	// }

	// // Or in the muzzle flash
	// ent->curstate.effects |= EF_MUZZLEFLASH;
	// BSVR end commented upstream code

	// BSVR start
	// "Hack" - Since we use server side entities for our weapons bound to VR controllers,
	// we simply send a message up to the server,
	// so it can set the muzzle flash to the actual weapon. - Max Makes Mods, 2019-04-13

	gEngfuncs.pfnClientCmd("vr_muzzleflash");
	// BSVR end
}
