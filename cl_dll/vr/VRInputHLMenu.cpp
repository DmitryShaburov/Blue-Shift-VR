
#include <cmath>
#include <iostream>
#include <filesystem>
#include <string>
#include <functional>
#include <fstream>
#include <cctype>
#include <regex>

#include "hud.h"
#include "cl_util.h"
#include "VRInput.h"
#include "eiface.h"
#include "VRCommon.h"
#include "VRRenderer.h"
#include "VRHelper.h"
#include "VRTextureHelper.h"
#include "VRGUIRenderer.h"

namespace
{
	unsigned int GetVRMenuTextureHandle()
	{
		//return gVRRenderer.GetHelper()->GetHLMenuTexture();
		return gVRRenderer.GetHelper()->GetVRGLGUITexture();
	}

	// Sets the transparency of the menu overlay, only when the setting changed
	void ApplyHLMenuOpacity(vr::VROverlayHandle_t overlay)
	{
		static float appliedOpacity = -1.f;
		float opacity = CVAR_GET_FLOAT("vr_menu_opacity");
		if (opacity < 0.1f) opacity = 0.1f;
		if (opacity > 1.f) opacity = 1.f;
		if (opacity != appliedOpacity)
		{
			appliedOpacity = opacity;
			vr::VROverlay()->SetOverlayAlpha(overlay, opacity);  // no error handling, we don't care if this fails
		}
	}

	// Places the menu overlay at a fixed spot in the tracking space, in front of where the HMD is
	// looking right now (yaw only, so the menu stays upright). It stays there until it is shown again.
	void PlaceHLMenuInWorld(vr::VROverlayHandle_t overlay)
	{
		float scale = CVAR_GET_FLOAT("vr_menu_scale");
		if (scale < 0.1f) scale = 0.1f;
		if (scale > 10.f) scale = 10.f;
		const float distance = 1.f;
		const float heightOffset = CVAR_GET_FLOAT("vr_menu_height");  // meters relative to eye level (negative: below)

		vr::TrackedDevicePose_t hmdPose;
		vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.f, &hmdPose, 1);
		if (!hmdPose.bPoseIsValid)
			return;

		const vr::HmdMatrix34_t& hmd = hmdPose.mDeviceToAbsoluteTracking;

		// HMD forward projected onto the floor plane (OpenVR is Y-up, the HMD looks along its -Z)
		float fx = -hmd.m[0][2];
		float fz = -hmd.m[2][2];
		const float len = sqrtf(fx * fx + fz * fz);
		if (len < 0.001f)
		{
			// looking straight up/down, fall back to the tracking space's forward
			fx = 0.f;
			fz = -1.f;
		}
		else
		{
			fx /= len;
			fz /= len;
		}

		// Overlay faces +Z, so its +Z points back at the viewer. X = Y x Z, Y is up.
		vr::HmdMatrix34_t transform =
		{
			-fz * scale, 0.f,   -fx * scale, hmd.m[0][3] + fx * distance,
			0.f,         scale,  0.f,        hmd.m[1][3] + heightOffset,
			fx * scale,  0.f,   -fz * scale, hmd.m[2][3] + fz * distance
		};

		auto error = vr::VROverlay()->SetOverlayTransformAbsolute(overlay, vr::TrackingUniverseStanding, &transform);
		if (error != vr::VROverlayError_None)
		{
			gEngfuncs.Con_DPrintf("Could not set absolute transform for HL menu overlay: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(error));
		}
	}

	// Laser beam from the controller to where it points at the menu. A thin overlay stretched along the ray.
	vr::VROverlayHandle_t beamOverlay = vr::k_ulOverlayHandleInvalid;
	constexpr float BeamMissLength = 1.f;      // meters, beam length when not pointing at the menu
	// The overlay's width is set to the beam length each frame and its height follows from the texture's
	// aspect ratio, so the thickness is length / (BeamTexturePixelsX / BeamTexturePixelsY), 4 mm per meter.
	constexpr int BeamTexturePixelsX = 512;
	constexpr int BeamTexturePixelsY = 2;

	void UpdateHLMenuBeam(bool visible, const Vector& from, const Vector& dir, float length, const vr::HmdMatrix34_t& hmd)
	{
		if (beamOverlay == vr::k_ulOverlayHandleInvalid)
		{
			if (vr::VROverlay()->CreateOverlay("HalfLifeVRMenuBeam", "HalfLifeVRMenuBeam", &beamOverlay) != vr::VROverlayError_None)
			{
				beamOverlay = vr::k_ulOverlayHandleInvalid;
				return;
			}
			static unsigned char white[4 * BeamTexturePixelsX * BeamTexturePixelsY];
			memset(white, 255, sizeof(white));
			vr::VROverlay()->SetOverlayRaw(beamOverlay, white, BeamTexturePixelsX, BeamTexturePixelsY, 4);
			vr::VROverlay()->SetOverlayColor(beamOverlay, 196.f / 255.f, 181.f / 255.f, 80.f / 255.f);
			vr::VROverlay()->SetOverlayAlpha(beamOverlay, 0.8f);
			vr::VROverlay()->SetOverlaySortOrder(beamOverlay, 1);
		}

		if (!visible)
		{
			vr::VROverlay()->HideOverlay(beamOverlay);
			return;
		}

		// Beam runs along the overlay's X axis and its face is rolled around the ray to look at the HMD.
		const Vector center = from + dir * (length * 0.5f);
		Vector toHmd(hmd.m[0][3] - center.x, hmd.m[1][3] - center.y, hmd.m[2][3] - center.z);
		Vector z = toHmd - dir * DotProduct(toHmd, dir);
		if (z.Length() < 0.001f)
			return;
		z = z.Normalize();
		const Vector y = CrossProduct(z, dir);

		vr::HmdMatrix34_t transform =
		{
			dir.x, y.x, z.x, center.x,
			dir.y, y.y, z.y, center.y,
			dir.z, y.z, z.z, center.z
		};
		vr::VROverlay()->SetOverlayTransformAbsolute(beamOverlay, vr::TrackingUniverseStanding, &transform);

		vr::VROverlay()->SetOverlayWidthInMeters(beamOverlay, length);
		vr::VROverlay()->ShowOverlay(beamOverlay);
	}

}  // namespace

void VRInput::CreateHLMenu()
{
	auto error = vr::VROverlay()->CreateOverlay("HalfLifeVRMenu", "HalfLifeVRMenu", &m_hlMenu);
	if (error != vr::VROverlayError_None)
	{
		gEngfuncs.Con_DPrintf("Could not create HL menu overlay: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(error));
		return;
	}

	vr::Texture_t texture;
	texture.eColorSpace = vr::ColorSpace_Gamma;
	texture.eType = vr::TextureType_OpenGL;
	texture.handle = reinterpret_cast<void*>(GetVRMenuTextureHandle());
	error = vr::VROverlay()->SetOverlayTexture(m_hlMenu, &texture);
	if (error != vr::VROverlayError_None)
	{
		gEngfuncs.Con_DPrintf("Could not set texture for HL menu overlay: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(error));
	}

	vr::VRTextureBounds_t textureBounds = { 0.f, 0.f, 1.f, 1.f };
	vr::VROverlay()->SetOverlayTextureBounds(m_hlMenu, &textureBounds);

	ApplyHLMenuOpacity(m_hlMenu);

	error = vr::VROverlay()->SetOverlayInputMethod(m_hlMenu, vr::VROverlayInputMethod_Mouse);
	if (error != vr::VROverlayError_None)
	{
		gEngfuncs.Con_DPrintf("Could not set input mode for HL menu overlay: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(error));
	}

	vr::HmdVector2_t mouseScale{ 1.f, 1.f };
	error = vr::VROverlay()->SetOverlayMouseScale(m_hlMenu, &mouseScale);
	if (error != vr::VROverlayError_None)
	{
		gEngfuncs.Con_DPrintf("Could not set mouse scale for HL menu overlay: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(error));
	}
}

void VRInput::RecenterHLMenu()
{
	if (m_isHLMenuShown && m_hlMenu != vr::k_ulOverlayHandleInvalid)
	{
		PlaceHLMenuInWorld(m_hlMenu);
	}
}

void VRInput::HandleHLMenuInput()
{
	// opacity can be changed while the menu is open
	ApplyHLMenuOpacity(m_hlMenu);

	const bool useRightController = CVAR_GET_FLOAT("vr_lefthand_mode") == 0.f;

	vr::TrackedDeviceIndex_t controllerDeviceIndex;
	if (useRightController)
	{
		controllerDeviceIndex = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_RightHand);
	}
	else
	{
		controllerDeviceIndex = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_LeftHand);
	}

	static std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> trackedDevicePoses = { { 0 } };

	vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseStanding);
	vr::VRCompositor()->WaitGetPoses(trackedDevicePoses.data(), vr::k_unMaxTrackedDeviceCount, nullptr, 0);

	Vector position;
	Vector direction;
	bool hasController = false;

	if (controllerDeviceIndex > 0
		&& controllerDeviceIndex != vr::k_unTrackedDeviceIndexInvalid
		&& trackedDevicePoses[controllerDeviceIndex].bDeviceIsConnected
		&& trackedDevicePoses[controllerDeviceIndex].bPoseIsValid)
	{
		position[0] = trackedDevicePoses[controllerDeviceIndex].mDeviceToAbsoluteTracking.m[0][3];
		position[1] = trackedDevicePoses[controllerDeviceIndex].mDeviceToAbsoluteTracking.m[1][3];
		position[2] = trackedDevicePoses[controllerDeviceIndex].mDeviceToAbsoluteTracking.m[2][3];
		direction[0] = -trackedDevicePoses[controllerDeviceIndex].mDeviceToAbsoluteTracking.m[0][2];
		direction[1] = -trackedDevicePoses[controllerDeviceIndex].mDeviceToAbsoluteTracking.m[1][2];
		direction[2] = -trackedDevicePoses[controllerDeviceIndex].mDeviceToAbsoluteTracking.m[2][2];
		hasController = true;
	}

	// Aim from the pointer (tip) pose like SteamVR does, the raw device pose points a different way
	vr::HmdMatrix34_t pointerPose;
	if (GetPointerPose(useRightController, pointerPose))
	{
		for (int i = 0; i < 3; i++)
		{
			position[i] = pointerPose.m[i][3];
			direction[i] = -pointerPose.m[i][2];
		}
		hasController = true;
	}

	// While the SteamVR dashboard is open (or something else has taken the input) the game gets no controller
	// input, and SteamVR shows its own pointer. Don't show a second one, and don't interact with the menu.
	if (!vr::VRSystem()->IsInputAvailable())
	{
		hasController = false;
	}

	vr::VROverlayIntersectionParams_t params{};
	params.eOrigin = vr::TrackingUniverseStanding;
	for (int i = 0; i < 3; i++)
	{
		params.vSource.v[i] = position[i];
		params.vDirection.v[i] = direction[i];
	}

	VRGUIRenderer::VRControllerState uiControllerState;
	uiControllerState.isPressed = g_vrInput.GetVRMenuClickStatus();

	vr::VROverlayIntersectionResults_t results;
	float beamLength = BeamMissLength;
	if (hasController && vr::VROverlay()->ComputeOverlayIntersection(m_hlMenu, &params, &results))
	{
		// ComputeOverlayIntersection returns UVs as if the overlay were as tall as it is wide, but the
		// overlay is shown with the aspect ratio of its texture, so rescale v around the center.
		const float texWidth = (float)gVRRenderer.GetHelper()->GetVRGLGUITextureWidth();
		const float texHeight = (float)gVRRenderer.GetHelper()->GetVRGLGUITextureHeight();
		const float aspect = (texWidth > 0.f && texHeight > 0.f) ? texHeight / texWidth : 1.f;
		const float v = 0.5f - (0.5f - results.vUVs.v[1]) / aspect;

		uiControllerState.isOverGUI = v >= 0.f && v <= 1.f;
		uiControllerState.x = results.vUVs.v[0];
		uiControllerState.y = v;
		beamLength = results.fDistance;
	}
	else
	{
		uiControllerState.isOverGUI = false;
	}

	UpdateHLMenuBeam(hasController, position, direction, beamLength, trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);

	VRGUIRenderer::Instance().UpdateControllerState(uiControllerState);
}

void VRInput::ShowHLMenu()
{
	if (m_isHLMenuShown)
	{
		HandleHLMenuInput();
	}

	if (m_hlMenu == vr::k_ulOverlayHandleInvalid)
	{
		CreateHLMenu();
	}

	if (!m_isHLMenuShown)
		PlaceHLMenuInWorld(m_hlMenu);

	auto error = vr::VROverlay()->ShowOverlay(m_hlMenu);
	if (error != vr::VROverlayError_None)
	{
		gEngfuncs.Con_DPrintf("Could not show HL menu overlay: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(error));
	}

	vr::Texture_t texture;
	texture.eColorSpace = vr::ColorSpace_Auto;
	texture.eType = vr::TextureType_OpenGL;
	texture.handle = reinterpret_cast<void*>(GetVRMenuTextureHandle());
	error = vr::VROverlay()->SetOverlayTexture(m_hlMenu, &texture);
	if (error != vr::VROverlayError_None)
	{
		gEngfuncs.Con_DPrintf("Could not update HL menu overlay texture: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(error));
	}

	m_isHLMenuShown = true;
}

void VRInput::HideHLMenu()
{
	if (!m_isHLMenuShown)
		return;

	if (beamOverlay != vr::k_ulOverlayHandleInvalid)
	{
		vr::VROverlay()->HideOverlay(beamOverlay);
	}

	auto error = vr::VROverlay()->HideOverlay(m_hlMenu);
	if (error != vr::VROverlayError_None)
	{
		gEngfuncs.Con_DPrintf("Could not hide HL menu overlay: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(error));
		return;
	}

	vr::VROverlay()->HideKeyboard();

	m_isHLMenuShown = false;
}
