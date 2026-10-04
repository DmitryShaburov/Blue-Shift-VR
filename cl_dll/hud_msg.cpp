/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
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
//
//  hud_msg.cpp
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "r_efx.h"

#include "particleman.h"
extern IParticleMan* g_pParticleMan;

extern BEAM* pBeam;
extern BEAM* pBeam2;


/// USER-DEFINED SERVER MESSAGE HANDLERS

bool CHud::MsgFunc_ResetHUD(const char* pszName, int iSize, void* pbuf)
{
	ASSERT(iSize == 0);

	// clear all hud data
	HUDLIST* pList = m_pHudList;

	while (pList)
	{
		if (pList->p)
			pList->p->Reset();
		pList = pList->pNext;
	}

	//Reset weapon bits.
	m_iWeaponBits = 0ULL;

	// reset sensitivity
	m_flMouseSensitivity = 0;

	// reset concussion effect
	m_iConcussionEffect = 0;

	return true;
}

void CAM_ToFirstPerson();

void CHud::MsgFunc_ViewMode(const char* pszName, int iSize, void* pbuf)
{
	CAM_ToFirstPerson();
}

void CHud::MsgFunc_InitHUD(const char* pszName, int iSize, void* pbuf)
{
	// prepare all hud data
	HUDLIST* pList = m_pHudList;

	while (pList)
	{
		if (pList->p)
			pList->p->InitHUDData();
		pList = pList->pNext;
	}


	//TODO: needs to be called on every map change, not just when starting a new game
	if (g_pParticleMan)
		g_pParticleMan->ResetParticles();

	//Probably not a good place to put this.
	pBeam = pBeam2 = NULL;
}


bool CHud::MsgFunc_GameMode(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	//Note: this user message could be updated to include multiple gamemodes, so make sure this checks for game mode 1
	//See CHalfLifeTeamplay::UpdateGameMode
	//TODO: define game mode constants
	m_Teamplay = READ_BYTE() == 1;

#ifdef STEAM_RICH_PRESENCE
	if (m_Teamplay)
		gEngfuncs.pfnClientCmd("richpresence_gamemode Teamplay\n");
	else
		gEngfuncs.pfnClientCmd("richpresence_gamemode\n"); // reset

	gEngfuncs.pfnClientCmd("richpresence_update\n");
#endif

	return true;
}


bool CHud::MsgFunc_Damage(const char* pszName, int iSize, void* pbuf)
{
	int armor, blood;
	Vector from;
	int i;
	float count;

	BEGIN_READ(pbuf, iSize);
	armor = READ_BYTE();
	blood = READ_BYTE();

	for (i = 0; i < 3; i++)
		from[i] = READ_COORD();

	count = (blood * 0.5) + (armor * 0.5);

	if (count < 10)
		count = 10;

	// TODO: kick viewangles,  show damage visually

	return true;
}

bool CHud::MsgFunc_Concuss(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iConcussionEffect = READ_BYTE();
	if (0 != m_iConcussionEffect)
	{
		int r, g, b;
		UnpackRGB(r, g, b, RGB_YELLOWISH);
		this->m_StatusIcons.EnableIcon("dmg_concuss", r, g, b);
	}
	else
		this->m_StatusIcons.DisableIcon("dmg_concuss");
	return true;
}

bool CHud::MsgFunc_Weapons(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	const std::uint64_t lowerBits = READ_LONG();
	const std::uint64_t upperBits = READ_LONG();

	m_iWeaponBits = (lowerBits & 0XFFFFFFFF) | ((upperBits & 0XFFFFFFFF) << 32ULL);

	return true;
}

// BSVR start
bool CHud::MsgFunc_GroundEnt(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iGroundEntIndex = READ_SHORT();
	return true;
}

bool CHud::MsgFunc_VRCtrlEnt(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	bool isLeftHand = READ_BYTE() != 0;
	auto& data = isLeftHand ? m_leftControllerModelData : m_rightControllerModelData;
	data.controller.body = READ_BYTE();
	data.controller.skin = READ_BYTE();
	data.controller.scale = READ_FLOAT();

	data.controller.sequence = READ_LONG();
	data.controller.frame = READ_FLOAT();
	data.controller.framerate = READ_FLOAT();
	data.controller.animtime = READ_FLOAT();

	strncpy(data.controller.modelname, READ_STRING(), sizeof(data.controller.modelname));

	data.hasDraggedEnt = false;

	bool hasDraggedEntity = READ_BYTE() != 0;
	if (hasDraggedEntity)
	{
		data.draggedEntIndex = READ_SHORT();

		data.draggedEntOriginOffset = Vector{ READ_FLOAT(), READ_FLOAT(), READ_FLOAT() };
		data.draggedEntAnglesOffset = Vector{ READ_FLOAT(), READ_FLOAT(), READ_FLOAT() };

		data.draggedEnt.body = READ_BYTE();
		data.draggedEnt.skin = READ_BYTE();
		data.draggedEnt.scale = READ_FLOAT();

		data.draggedEnt.sequence = READ_LONG();
		data.draggedEnt.frame = READ_FLOAT();
		data.draggedEnt.framerate = READ_FLOAT();
		data.draggedEnt.animtime = READ_FLOAT();

		data.draggedEnt.effects = READ_LONG();
		data.draggedEnt.rendermode = READ_BYTE() & 0xFF;
		data.draggedEnt.renderamt = READ_BYTE() & 0xFF;
		data.draggedEnt.renderfx = READ_BYTE() & 0xFF;
		data.draggedEnt.rendercolor.r = READ_BYTE() & 0xFF;
		data.draggedEnt.rendercolor.g = READ_BYTE() & 0xFF;
		data.draggedEnt.rendercolor.b = READ_BYTE() & 0xFF;

		strncpy(data.draggedEnt.modelname, READ_STRING(), sizeof(data.draggedEnt.modelname));

		data.hasDraggedEnt = true;
	}

	return true;
}

bool CHud::MsgFunc_TrainCtrl(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_trainControlPosition.x = READ_COORD();
	m_trainControlPosition.y = READ_COORD();
	m_trainControlPosition.z = READ_COORD();
	m_trainControlYaw = READ_ANGLE();
	return true;
}

bool CHud::MsgFunc_VRScrnShke(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_screenShakeAmplitude = READ_FLOAT();
	m_screenShakeDuration = READ_FLOAT();
	m_screenShakeFrequency = READ_FLOAT();
	m_hasScreenShake = true;
	return true;
}

bool CHud::MsgFunc_GrbdLddr(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_vrGrabbedLadderEntIndex = READ_SHORT();
	return true;
}

bool CHud::MsgFunc_PullLdg(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_vrIsPullingOnLedge = READ_BYTE() != 0;
	return true;
}

bool CHud::MsgFunc_VRTouch(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	bool isLeftHand = READ_BYTE() != 0;
	if (isLeftHand)
	{
		m_vrLeftHandTouchVibrateIntensity = READ_FLOAT();
	}
	else
	{
		m_vrRightHandTouchVibrateIntensity = READ_FLOAT();
	}
	return true;
}
// BSVR end
