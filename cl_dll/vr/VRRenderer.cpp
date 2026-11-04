
#include <chrono>
#include <unordered_set>
#include <string>

#include "VRTextureHelper.h"

#include "hud.h"
#include "cl_util.h"

#include "cl_entity.h"
#include "ref_params.h"
#include "pm_defs.h"
#include "com_model.h"
#include "studio.h"

#include "r_studioint.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"

#include "com_weapons.h"

#include "VRRenderer.h"
#include "VRHelper.h"
#include "VRInput.h"
#include "VRSettings.h"
#include "VRRandom.h"
#include "VRSteamworksManager.h"
#include "VRWeaponWheel.h"

#include "vr_gl.h"


// set by hud message from server
double gVR_LastLevelChangeTime = 0.f;

extern globalvars_t* gpGlobals;

VRRenderer gVRRenderer;

// How dark the paused world behind the menu gets (opacity of black drawn over it)
constexpr float MenuWorldDarkening = 0.6f;

namespace
{
std::unordered_set<std::string> g_handmodels{
	{
		"models/v_hand.mdl",
	}};
std::unordered_set<std::string> g_handskeletalmodels{};
}


VRRenderer::VRRenderer()
{
	vrHelper = new VRHelper();
}

VRRenderer::~VRRenderer()
{
	delete vrHelper;
	vrHelper = nullptr;
}

void VRRenderer::Init()
{
	VRSteamworksManager::Init();

	vrHelper->Init();

	extern void InstallModExtraDataCacheHook();
	InstallModExtraDataCacheHook();
}

void VRRenderer::VidInit()
{
}

void VRRenderer::Frame()
{
	VRSteamworksManager::Update();

	extern void ResetOffsetThingies();

	const static auto begin = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	m_clientTime = std::chrono::duration<double>(now - begin).count();

	extern void VRClearCvarCache();
	VRClearCvarCache();

	if (m_isVeryFirstFrameEver)
	{
		m_isVeryFirstFrameEver = false;
		vrHelper->SetMaxFps();
		ResetOffsetThingies();
	}

	g_vrSettings.CheckCVARsForChanges();

	UpdateGameRenderState();

	// BSVR start
	// Captured before the menu overlay is shown below, which hands this texture to SteamVR. Otherwise the overlay gets
	// the previous capture, and on the first frame of a pause the plain menu texture with an old menu on black.
	// The menu in VR is this copy of the game's menu, so it is needed every frame the menu is up
	// if (!IsInGame() || (m_isInMenu && m_wasMenuJustRendered)) - original
	if (!IsInGame() || m_isInMenu)
	{
		CaptureCurrentScreenToTexture(vrHelper->GetHLMenuTexture());
		ResetOffsetThingies();
	}
	// Over the game world the menu's black background is made transparent, so only the menu's dialogs float in front of the player
	m_hlMenuTransparentTexture = IsGameShownBehindMenu() ? vrHelper->UpdateHLMenuTransparentTexture() : 0;
	// BSVR end

	if (m_isInMenu || !IsInGame())
	{
		// BSVR start
		// During a game the in-game controls are updated with the menu open too, and their buttons are read without
		// running them (VRInputAction::HandleInput). Otherwise a trigger press that clicks the menu and closes it,
		// like Resume Game, is seen as a new press once the game runs again, and fires the weapon.
		// vrHelper->PollEvents(false, m_isInMenu); - original
		vrHelper->PollEvents(IsInGame(), m_isInMenu);
		// BSVR end

		// don't show menu during levelchange (we show it after 2 seconds, in case the load failed or froze)
		if (gVR_LastLevelChangeTime != 0.0 && gVR_LastLevelChangeTime > (m_clientTime - 2.f))
		{
			g_vrInput.HideHLMenu();
		}
		else
		{
			g_vrInput.ShowHLMenu();
		}

		ResetOffsetThingies();
	}
	else
	{
		g_vrInput.HideHLMenu();
	}

	if (IsDeadInGame())
	{
		extern void ClearInputForDeath();
		ClearInputForDeath();

		extern playermove_t* pmove;
		if (pmove != nullptr)
		{
			pmove->oldbuttons = 0;
			pmove->cmd.buttons = 0;
			pmove->cmd.buttons_ex = 0;
		}

		gHUD.m_vrGrabbedLadderEntIndex = 0;

		ResetOffsetThingies();
	}

	if (!IsInGame())
	{
		gHUD.m_vrGrabbedLadderEntIndex = 0;
		ResetOffsetThingies();
	}

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
}

void VRRenderer::UpdateGameRenderState()
{
	m_isInGame = m_CalcRefdefWasCalled;
	m_CalcRefdefWasCalled = false;

	m_isInMenu = !m_HUDRedrawWasCalled;
	m_HUDRedrawWasCalled = false;
}


void VRRenderer::CalcRefdef(struct ref_params_s* pparams)
{
	const int in_nextview = pparams->nextView;

	m_CalcRefdefWasCalled = true;

	// BSVR start
	// Upstream hid the game whenever the menu was open, because the OpenVR overlay menu ignored controller input
	// while the game scene was rendered. The menu now finds where the controller points and draws its own beam,
	// so during a game the paused world stays visible behind the menu, darkened (see RenderVRHandsAndHUDAndStuff).
	// if (m_isInMenu || !IsInGame()) - original
	if (!IsInGame())
	// BSVR end
	{
		// Unfortunately the OpenVR overlay menu ignores controller input when we render the game scene.
		// (in fact controllers themselves aren't rendered at all)
		// So when the menu is open, we hide the game (the user will see a skybox and the menu)
		pparams->nextView = 0;
		pparams->onlyClientDraw = 1;
		m_fIsOnlyClientDraw = true;

		return;
	}

	if (pparams->nextView == 0)
	{
		m_eyeMode = static_cast<VREyeMode>(std::atoi(CVAR_GET_STRING("vr_eye_mode")));

		// BSVR start
		// With the menu open, Frame() has already polled this frame's events and input
		// vrHelper->PollEvents(true, m_isInMenu); // 200~300�s - original
		if (!m_isInMenu)
		{
			vrHelper->PollEvents(true, false); // 200~300�s
		}
		// BSVR end

		vrHelper->UpdatePositions(pparams->viewentity > pparams->maxclients ? pparams->viewentity : -1); // 100~200�s, sometimes spikes to 3000+�s(!)

		VRTextureHelper::Instance().Init();
		UpdateSkyboxForMap();
	}

	// skip second eye if m_eyeMode is not Both
	if (pparams->nextView == 1 && m_eyeMode != VREyeMode::Both)
	{
		pparams->nextView = 2;
	}

	if (pparams->nextView == 0)
	{
		//200�s
		if (m_eyeMode == VREyeMode::RightOnly || m_eyeMode == VREyeMode::RightOnlyMirrored)
		{
			// right eye if RightOnly
			vrHelper->PrepareVRScene(VRHelper::VRSceneMode::RightEye);
		}
		else
		{
			// left eye if LeftOnly or Both
			vrHelper->PrepareVRScene(VRHelper::VRSceneMode::LeftEye);
		}

		pparams->nextView = 1;
		pparams->onlyClientDraw = 0;
		m_fIsOnlyClientDraw = false;
	}
	else if (pparams->nextView == 1)
	{
		//300�s
		RenderVRHandsAndHUDAndStuff();
		vrHelper->FinishVRScene(pparams->viewport[2], pparams->viewport[3]);
		vrHelper->PrepareVRScene(VRHelper::VRSceneMode::RightEye);

		pparams->nextView = 2;
		pparams->onlyClientDraw = 0;
		m_fIsOnlyClientDraw = false;
	}
	else if (pparams->nextView == 2)
	{
		//900�s
		RenderVRHandsAndHUDAndStuff();
		vrHelper->FinishVRScene(pparams->viewport[2], pparams->viewport[3]);
		vrHelper->SubmitImages(m_eyeMode);

		pparams->nextView = 0;
		pparams->onlyClientDraw = 1;
		m_fIsOnlyClientDraw = true;
	}

	vrHelper->GetViewOrg(pparams->vieworg);
	vrHelper->GetViewAngles(vr::EVREye::Eye_Left, pparams->viewangles);
	gEngfuncs.SetViewAngles(pparams->viewangles);

	// Override vieworg if we have a viewentity (trigger_camera)
	if (pparams->viewentity > pparams->maxclients)
	{
		cl_entity_t* viewentity;
		viewentity = gEngfuncs.GetEntityByIndex(pparams->viewentity);
		if (viewentity)
		{
			VectorCopy(viewentity->origin, pparams->vieworg);
		}
	}

	if (in_nextview == 0)
	{
		VRRandomBackupSeed();
	}
	else
	{
		VRRandomRestoreSeed();
	}
}


bool VRRenderer::IsHandModel(const char* modelname) const
{
	return (g_handmodels.find(modelname) != g_handmodels.end());
}

bool VRRenderer::IsHandSkeletalModel(const char* modelname) const
{
	return (g_handskeletalmodels.find(modelname) != g_handskeletalmodels.end());
}

bool VRRenderer::HasFingerDataForHand(bool isMirrored, float fingerCurl[5]) const
{
	return g_vrInput.HasFingerDataForHand(isMirrored ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand, fingerCurl);
}

bool VRRenderer::HasSkeletalDataForHand(bool isMirrored, VRQuaternion* bone_quaternions, Vector* bone_positions) const
{
	return g_vrInput.HasSkeletalDataForHand(isMirrored ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand, bone_quaternions, bone_positions);
}

void VRRenderer::RenderVRHandsAndHUDAndStuff()
{
	extern CGameStudioModelRenderer g_StudioRenderer;

	// BSVR start
	// While a trigger_camera controls the view, the head is at the camera but the hands (and the
	// HUD attached to them) are still positioned at the body, so they would appear floating behind
	// the player. Don't draw them. Screen fades and damage overlays below are view-relative and stay.
	bool drawHandsAndHUD = !vrHelper->HasViewEntOverride();
	// While the menu is open, the menu's beam comes from the controller, a weapon pointing elsewhere would be confusing,
	// so a hand holding something is drawn empty: like the other hand if that one is empty, else as a plain hand model.
	// The weapon's own data is kept, it's drawn again when the menu closes.
	auto isEmptyHand = [this](const ControllerModelData& modelData)
	{
		return IsHandModel(modelData.controller.modelname) || IsHandSkeletalModel(modelData.controller.modelname);
	};
	auto getShownModel = [&](const ControllerModelData& own, const ControllerModelData& other, ControllerModelData& emptyHand) -> const ControllerModelData&
	{
		if (!m_isInMenu || isEmptyHand(own))
			return own;
		emptyHand = ControllerModelData{};
		if (isEmptyHand(other))
			emptyHand.controller = other.controller; // not what the other hand carries
		else
			strncpy(emptyHand.controller.modelname, "models/v_hand.mdl", sizeof(emptyHand.controller.modelname) - 1);
		return emptyHand;
	};
	ControllerModelData rightEmptyHand;
	ControllerModelData leftEmptyHand;
	const ControllerModelData& rightModel = getShownModel(gHUD.m_rightControllerModelData, gHUD.m_leftControllerModelData, rightEmptyHand);
	const ControllerModelData& leftModel = getShownModel(gHUD.m_leftControllerModelData, gHUD.m_rightControllerModelData, leftEmptyHand);
	// BSVR end

	m_numRightControllerAttachments = 0;
	if (drawHandsAndHUD && vrHelper->HasValidRightController())
	{
		g_StudioRenderer.StudioDrawVRHand(
			rightModel, // BSVR: gHUD.m_rightControllerModelData - original
			vrHelper->GetRightControllerPosition(),
			vrHelper->GetRightControllerAngles(),
			false,
			&m_numRightControllerAttachments,
			m_rightControllerAttachments);
	}

	m_numLeftControllerAttachments = 0;
	if (drawHandsAndHUD && vrHelper->HasValidLeftController())
	{
		g_StudioRenderer.StudioDrawVRHand(
			leftModel, // BSVR: gHUD.m_leftControllerModelData - original
			vrHelper->GetLeftControllerPosition(),
			vrHelper->GetLeftControllerAngles(),
			true,
			&m_numLeftControllerAttachments,
			m_leftControllerAttachments);
	}

	if (CVAR_GET_FLOAT("vr_debug_controllers") != 0.f)
	{
		vrHelper->TestRenderControllerPositions();
	}

	// BSVR start
	// RenderHUDSprites(); - original, now skipped while a view entity controls the view (see above)
	// and while the menu is open, the game doesn't update the HUD then
	if (drawHandsAndHUD && !m_isInMenu)
	{
		RenderHUDSprites();
		VR::WeaponWheel::Render();
	}
	// BSVR end
	RenderScreenOverlays();
	// BSVR start
	if (m_isInMenu)
	{
		DarkenScreen(MenuWorldDarkening);
	}
	// BSVR end
}

bool VRRenderer::GetLeftControllerAttachment(Vector& out, int attachment)
{
	if (attachment < m_numLeftControllerAttachments)
	{
		out = m_leftControllerAttachments[attachment];
		return true;
	}
	return false;
}

bool VRRenderer::GetRightControllerAttachment(Vector& out, int attachment)
{
	if (attachment < m_numRightControllerAttachments)
	{
		out = m_rightControllerAttachments[attachment];
		return true;
	}
	return false;
}



void VRRenderer::DrawNormal()
{
}

void VRRenderer::DrawTransparent()
{
	if (!m_fIsOnlyClientDraw)
	{
		return;
	}

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	// BSVR start
	// The menu over the game world is captured from the window with its black background made transparent, so keep it black
	// if (CVAR_GET_FLOAT("vr_display_game") == 0.f) - original
	if (CVAR_GET_FLOAT("vr_display_game") == 0.f || m_hlMenuTransparentTexture != 0)
	// BSVR end
	{
		return;
	}

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	GLuint backgroundTexture = VRTextureHelper::Instance().GetTexture("background.png");

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glDisable(GL_CULL_FACE);
	glCullFace(GL_FRONT_AND_BACK);

	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);

	bool renderUpsideDown = false;
	if (m_isInMenu)
	{
		glBindTexture(GL_TEXTURE_2D, backgroundTexture);
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, vrHelper->GetVRTexture(vr::EVREye::Eye_Left));
		renderUpsideDown = true;
	}

	glColor4f(1, 1, 1, 1);

	float size = 1.f;

	glBegin(GL_QUADS);
	glTexCoord2f(0, renderUpsideDown ? 0 : 1);
	glVertex3f(-size, -size, -1.f);
	glTexCoord2f(1, renderUpsideDown ? 0 : 1);
	glVertex3f(size, -size, -1.f);
	glTexCoord2f(1, renderUpsideDown ? 1 : 0);
	glVertex3f(size, size, -1.f);
	glTexCoord2f(0, renderUpsideDown ? 1 : 0);
	glVertex3f(-size, size, -1.f);
	glEnd();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glBindTexture(GL_TEXTURE_2D, 0);

	glPopAttrib();
}

void VRRenderer::InterceptHUDRedraw(float time, int intermission)
{
	// We don't call the HUD redraw code here - instead we just store the parameters
	// HUD redraw will be called by VRRenderer::RenderHUDSprites(), which gets called by StudioModelRenderer after rendering the viewmodels (see further below)
	// We then intercept SPR_Set and SPR_DrawAdditive and render the HUD sprites in 3d space at the controller positions

	m_hudRedrawTime = time;
	m_hudRedrawIntermission = intermission;

	m_HUDRedrawWasCalled = true;
}

void VRRenderer::GetViewAngles(float* angles)
{
	vrHelper->GetViewAngles(vr::EVREye::Eye_Right, angles);
}

bool VRRenderer::GetWeaponControllerAttachment(Vector& out, int attachment)
{
	return (CVAR_GET_FLOAT("vr_lefthand_mode") != 0.f) ? GetLeftControllerAttachment(out, attachment) : GetRightControllerAttachment(out, attachment);
}

bool VRRenderer::GetHandControllerAttachment(Vector& out, int attachment)
{
	return (CVAR_GET_FLOAT("vr_lefthand_mode") != 0.f) ? GetRightControllerAttachment(out, attachment) : GetLeftControllerAttachment(out, attachment);
}

Vector VRRenderer::GetMovementAngles()
{
	return vrHelper->GetMovementAngles();
}


void VRRenderer::ReverseCullface()
{
	glFrontFace(GL_CW);
}

void VRRenderer::RestoreCullface()
{
	glFrontFace(GL_CCW);
}

// Sets the VR skybox whenever a different map is loaded
void VRRenderer::UpdateSkyboxForMap()
{
	cl_entity_s* map = gEngfuncs.GetEntityByIndex(0);
	if (map != nullptr && map->model != nullptr)
	{
		if (map->model != m_currentMapModel)
		{
			m_currentMapModel = map->model;
			vrHelper->SetSkyboxFromMap(map->model->name);
		}
	}
	else
	{
		m_currentMapModel = nullptr;
	}
}

bool VRRenderer::IsDeadInGame()
{
	return gEngfuncs.pfnGetLevelName() != 0 && gEngfuncs.pfnGetLevelName()[0] != '\0' && gEngfuncs.GetMaxClients() > 0 && CL_IsDead();
}

bool VRRenderer::IsInGame()
{
	return m_isInGame && gEngfuncs.pfnGetLevelName() != 0 && gEngfuncs.pfnGetLevelName()[0] != '\0' && gEngfuncs.GetMaxClients() > 0 && !CL_IsDead() && !gHUD.m_iIntermission;
}

cl_entity_t* VRRenderer::SaveGetLocalPlayer()
{
	if (IsInGame())
	{
		return gEngfuncs.GetLocalPlayer();
	}
	else
	{
		return nullptr;
	}
}

cl_entity_t* SaveGetLocalPlayer()
{
	return gVRRenderer.SaveGetLocalPlayer();
}

// For ev_common.cpp
Vector VRGlobalGetGunPosition()
{
	return gVRRenderer.GetHelper()->GetGunPosition();
}

void VRGlobalGetGunAim(Vector& forward, Vector& right, Vector& up, Vector& angles)
{
	gVRRenderer.GetHelper()->GetGunAim(forward, right, up, angles);
}
