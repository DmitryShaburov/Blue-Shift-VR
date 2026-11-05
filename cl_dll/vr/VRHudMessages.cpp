// VR user messages of the HUD and their handlers, moved from hud.cpp and hud_msg.cpp

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include "parsemsg.h"

#include "vr/VRRenderer.h"

int __MsgFunc_VRRstrYaw(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	// a bit hacky, but oh well
	extern float g_vrRestoreYaw_PrevYaw;
	extern float g_vrRestoreYaw_CurrentYaw;
	extern bool g_vrRestoreYaw_HasData;
	g_vrRestoreYaw_PrevYaw = READ_ANGLE();
	g_vrRestoreYaw_CurrentYaw = READ_ANGLE();
	g_vrRestoreYaw_HasData = true;
	return 0;
}

int __MsgFunc_VRSpawnYaw(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	// a bit hacky, but oh well
	extern float g_vrSpawnYaw;
	extern bool g_vrSpawnYaw_HasData;
	g_vrSpawnYaw = READ_ANGLE();
	g_vrSpawnYaw_HasData = true;
	return 0;
}

int __MsgFunc_VRWlkWl(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	extern double g_vrWalkedIntoWall;
	g_vrWalkedIntoWall = gVRRenderer.m_clientTime;
	return 0;
}

int __MsgFunc_VRLvlChng(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	extern double gVR_LastLevelChangeTime;
	gVR_LastLevelChangeTime = gVRRenderer.m_clientTime;
	return 0;
}

int __MsgFunc_VRAchvmnt(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	int achievement = READ_LONG();
	// VRSteamworksManager::GiveAchievement((VRAchievement)achievement);
	return 0;
}

// Sends index of current grund entity
int __MsgFunc_GroundEnt(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_GroundEnt(pszName, iSize, pbuf));
}

int __MsgFunc_VRCtrlEnt(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_VRCtrlEnt(pszName, iSize, pbuf));
}

int __MsgFunc_TrainCtrl(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_TrainCtrl(pszName, iSize, pbuf));
}

int __MsgFunc_VRScrnShke(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_VRScrnShke(pszName, iSize, pbuf));
}

int __MsgFunc_GrbdLddr(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_GrbdLddr(pszName, iSize, pbuf));
}

int __MsgFunc_PullLdg(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_PullLdg(pszName, iSize, pbuf));
}

int __MsgFunc_VRUpdEgon(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	Vector beamStartPos{READ_FLOAT(), READ_FLOAT(), READ_FLOAT()};
	Vector beamEndPos{READ_FLOAT(), READ_FLOAT(), READ_FLOAT()};

	extern void EV_UpdateEgon(const Vector& beamStartPos, const Vector& beamEndPos);
	EV_UpdateEgon(beamStartPos, beamEndPos);

	return 0;
}

int __MsgFunc_VRTouch(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_VRTouch(pszName, iSize, pbuf));
}

// Registers the VR messages; called from CHud::Init
void VRHookHudMessages()
{
	HOOK_MESSAGE(VRRstrYaw);
	HOOK_MESSAGE(GroundEnt);
	HOOK_MESSAGE(VRCtrlEnt);
	HOOK_MESSAGE(VRSpawnYaw);
	HOOK_MESSAGE(TrainCtrl);
	HOOK_MESSAGE(VRScrnShke);
	HOOK_MESSAGE(GrbdLddr);
	HOOK_MESSAGE(PullLdg);
	HOOK_MESSAGE(VRUpdEgon);
	HOOK_MESSAGE(VRTouch);
	HOOK_MESSAGE(VRWlkWl);
	HOOK_MESSAGE(VRLvlChng);
	HOOK_MESSAGE(VRAchvmnt);
}

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

// Ground entity for rotating with them in VR (since player rotation is done client side completely) - Max Makes Mods, 2019-04-09
cl_entity_t* CHud::GetGroundEntity()
{
	if (m_iGroundEntIndex > 0)
	{
		return gEngfuncs.GetEntityByIndex(m_iGroundEntIndex);
	}
	else
	{
		return nullptr;
	}
}

bool CHud::GetTrainControlsOriginAndOrientation(Vector& origin, Vector& angles)
{
	if (0 != m_Train.m_iPos)
	{
		origin = m_trainControlPosition;
		angles.x = -20.f;
		angles.y = m_trainControlYaw;
		angles.z = 0.f;
		return true;
	}
	return false;
}
