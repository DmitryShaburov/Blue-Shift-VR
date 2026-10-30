
#include <chrono>
#include <unordered_set>
#include <string>
#include <regex>

#include "VRTextureHelper.h"
#include "VRGUIRenderer.h"
#include "VRGameFunctions.h"

#include "Matrices.h"

#include "hud.h"
#include "cl_util.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"

#include "entity_state.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "in_defs.h"  // PITCH YAW ROLL
#include "pm_movevars.h"
#include "pm_shared.h"
#include "pm_defs.h"
#include "event_api.h"
#include "pmtrace.h"
#include "screenfade.h"
#include "shake.h"
#include "hltv.h"
#include "keydefs.h"
#include "com_model.h"
#include "studio.h"

#include "event_api.h"
#include "triangleapi.h"
#include "r_efx.h"
#include "studio_util.h"
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

#include "com_model.h"
#include "vr/VRComModelHL25.h"

#include "vr_gl.h"


// set by hud message from server
double gVR_LastLevelChangeTime = 0.f;

extern globalvars_t* gpGlobals;

VRRenderer gVRRenderer;

// Set when the menu starts a new game or loads a save, the menu stays hidden until the game is running
// (or this much time has passed, in case the load failed or froze)
static double gVR_GameLoadingStartTime = 0.0;
constexpr double VR_GAME_LOADING_MAX_MENU_HIDE_SECONDS = 5.0;

void VRNotifyGameLoading()
{
	gVR_GameLoadingStartTime = gVRRenderer.m_clientTime;
	VRGameFunctions::StopMenuMusic();
}

namespace
{
	std::unordered_set<std::string> g_handmodels{
		{
			"models/v_hand.mdl",
		} };
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

void VRRenderer::Frame(double frametime)
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

	bool isMenuVisible = false;
	if (m_isInMenu || !IsInGame())
	{
		vrHelper->PollEvents(false, m_isInMenu);

		// don't show menu during levelchange (we show it after 2 seconds, in case the load failed or froze)
		const bool isGameLoading = gVR_GameLoadingStartTime != 0.0 && m_clientTime - gVR_GameLoadingStartTime < VR_GAME_LOADING_MAX_MENU_HIDE_SECONDS;
		if (isGameLoading || (gVR_LastLevelChangeTime != 0.0 && gVR_LastLevelChangeTime > (m_clientTime - 2.f)))
		{
			g_vrInput.HideHLMenu();
		}
		else
		{
			g_vrInput.ShowHLMenu();
			isMenuVisible = true;
		}

		ResetOffsetThingies();
	}
	else
	{
		gVR_GameLoadingStartTime = 0.0;  // game is running
		VRGameFunctions::StopMenuMusic();  // also covers games that were not started from the menu
		g_vrInput.HideHLMenu();
	}

	if (!IsInGame() || (m_isInMenu && m_wasMenuJustRendered))
	{
		CaptureCurrentScreenToTexture(vrHelper->GetHLMenuTexture());
		m_wasMenuJustRendered = false;
		ResetOffsetThingies();
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


	// Only render the GUI while the menu overlay is shown.
	// Render the GUI into its own texture instead of the window, the window's framebuffer has no alpha channel
	// and the menu needs to be transparent around the boxes.
	// every time the menu is opened it starts at the main page
	static bool wasMenuVisible = false;
	if (isMenuVisible && !wasMenuVisible)
	{
		VRGUIRenderer::Instance().ResetMenu();
		VRGameFunctions::StartMenuMusic();
	}
	wasMenuVisible = isMenuVisible;

	if (isMenuVisible)
	{
		const GLuint guiTexture = vrHelper->GetVRGLGUITexture();  // (re)creates the texture if the viewport size changed
		static GLuint guiFramebuffer = 0;
		constexpr GLenum GL_FRAMEBUFFER_BINDING_ENUM = 0x8CA6;
		GLint previousFramebuffer = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING_ENUM, &previousFramebuffer);
		if (guiFramebuffer == 0)
		{
			glGenFramebuffers(1, &guiFramebuffer);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, guiFramebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, guiTexture, 0);

		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);

		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		VRGUIRenderer::Instance().UpdateGUI(vrHelper->GetVRGLGUITextureWidth(), vrHelper->GetVRGLGUITextureHeight(), IsInGame());
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glPopAttrib();

		glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
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

	if (!m_isInMenu)
		m_wasMenuJustRendered = false;
}


void VRRenderer::CalcRefdef(struct ref_params_s* pparams)
{
	const int in_nextview = pparams->nextView;

	m_CalcRefdefWasCalled = true;

	if (m_isInMenu || !IsInGame())
	{
		// Unfortunately the OpenVR overlay menu ignores controller input when we render the game scene.
		// (in fact controllers themselves aren't rendered at all)
		// So when the menu is open, we hide the game (the user will see a skybox and the menu)
		pparams->nextView = 0;
		pparams->onlyClientDraw = 1;
		m_fIsOnlyClientDraw = true;

		return;
	}

	VREyeMode eyeMode = m_eyeMode;

	if (pparams->nextView == 0)
	{
		eyeMode = m_eyeMode = static_cast<VREyeMode>(std::atoi(CVAR_GET_STRING("vr_eye_mode")));

		vrHelper->PollEvents(true, m_isInMenu); // 200~300�s

		if (!vrHelper->UpdatePositions(pparams->viewentity > pparams->maxclients ? pparams->viewentity : -1)) // 100~200�s, sometimes spikes to 3000+�s(!)
		{
			// No valid HMD input, render default (2D pancake mode)
			eyeMode = VREyeMode::PancakeMode;
			/*
			auto* player = SaveGetLocalPlayer();
			if (player)
			{
				VectorCopy(player->angles, pparams->cl_viewangles);
				VectorCopy(player->angles, pparams->viewangles);
			}
			pparams->onlyClientDraw = 0;
			m_fIsOnlyClientDraw = false;
			return;
			*/
		}

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
	// BSVR end

	m_numRightControllerAttachments = 0;
	if (drawHandsAndHUD && vrHelper->HasValidRightController())
	{
		g_StudioRenderer.StudioDrawVRHand(
			gHUD.m_rightControllerModelData,
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
			gHUD.m_leftControllerModelData,
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
	if (drawHandsAndHUD)
	{
		RenderHUDSprites();
		VR::WeaponWheel::Render();
	}
	// BSVR end
	RenderScreenOverlays();
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
		m_wasMenuJustRendered = false;
		return;
	}

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	if (CVAR_GET_FLOAT("vr_display_game") == 0.f)
	{
		return;
	}

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	if (m_fIsOnlyClientDraw)
	{
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
			m_wasMenuJustRendered = true;
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, vrHelper->GetVRTexture(vr::EVREye::Eye_Left));
			m_wasMenuJustRendered = false;
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
	}
	else
	{
		m_wasMenuJustRendered = false;
	}

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

void VRRenderer::GetViewOrg(float* origin)
{
	vrHelper->GetViewOrg(origin);
}

void VRRenderer::GetViewAngles(float* angles)
{
	vrHelper->GetViewAngles(vr::EVREye::Eye_Right, angles);
}

void VRRenderer::GetViewVectors(Vector& forward, Vector& right, Vector& up)
{
	vrHelper->GetViewVectors(forward, right, up);
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

// for studiomodelrenderer

void VRRenderer::EnableDepthTest()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthRange(0, 1);
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

vr::IVRSystem* VRRenderer::GetVRSystem()
{
	return vrHelper->GetVRSystem();
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
