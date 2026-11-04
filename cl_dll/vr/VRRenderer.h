#pragma once

#include "vr/VRShared.h"

#define VEC_HULL_MIN      Vector(-16, -16, -36)
#define VEC_HULL_MAX      Vector(16, 16, 36)
#define VEC_DUCK_HULL_MIN Vector(-16, -16, -18)
#define VEC_DUCK_HULL_MAX Vector(16, 16, 18)

class VRHelper;

enum class VRHUDRenderType
{
	NONE = 0,
	HEALTH,
	BATTERY,
	FLASHLIGHT,
	AMMO,
	AMMO_SECONDARY,
	TRAINCONTROLS,
	DAMAGEDECALS,
	PAIN
};

class VRRenderer
{
public:
	VRRenderer();
	~VRRenderer();

	void Init();
	void VidInit();

	void Frame();
	void UpdateGameRenderState();
	void CalcRefdef(struct ref_params_s* pparams);

	void DrawTransparent();
	void DrawNormal();

	void InterceptHUDRedraw(float time, int intermission);

	void GetViewAngles(float* angles);
	Vector GetMovementAngles();


	// For studiomodelrenderer
	void ReverseCullface();
	void RestoreCullface();

	// Called in DrawTransparent()
	void RenderHUDSprites();
	void RenderScreenOverlays();
	void DrawScreenFade();
	void DarkenScreen(float opacity);

	// Called by HUD draw code
	void VRHUDDrawBegin(const VRHUDRenderType renderType);
	void InterceptSPR_Set(HSPRITE hPic, int r, int g, int b);
	void InterceptSPR_DrawAdditive(int frame, int x, int y, const Rect* prc);
	void VRHUDDrawFinished();

	VRHelper* GetHelper() { return vrHelper; }

	bool HasValidLeftController();
	bool HasValidRightController();

	bool IsDeadInGame();
	bool IsInGame();
	// The menu is open during a game: the paused world is rendered, darkened, behind the menu
	bool IsGameShownBehindMenu() { return m_isInMenu && IsInGame(); }
	// The game's menu with a transparent background, 0 when the menu is shown as captured
	unsigned int GetHLMenuTransparentTexture() { return m_hlMenuTransparentTexture; }
	struct cl_entity_s* SaveGetLocalPlayer();

	void RotateVectorX(Vector& vecToRotate, const float angle);
	void RotateVectorY(Vector& vecToRotate, const float angle);
	void RotateVectorZ(Vector& vecToRotate, const float angle);
	void RotateVector(Vector& vecToRotate, const Vector& vecAngles, const bool reverse = false);

	double m_clientTime{0.0}; // keeps track of current time

	bool IsHandModel(const char* modelname) const;
	bool IsHandSkeletalModel(const char* modelname) const;
	bool HasFingerDataForHand(bool isMirrored, float fingerCurl[5]) const;
	bool HasSkeletalDataForHand(bool isMirrored, VRQuaternion* bone_quaternions, Vector* bone_positions) const;

	bool GetLeftControllerAttachment(Vector& out, int attachment);
	bool GetRightControllerAttachment(Vector& out, int attachment);

	bool GetWeaponControllerAttachment(Vector& out, int attachment);
	bool GetHandControllerAttachment(Vector& out, int attachment);

private:
	float m_leftControllerAttachments[4][3]{0.f};
	float m_rightControllerAttachments[4][3]{0.f};
	int m_numLeftControllerAttachments{0};
	int m_numRightControllerAttachments{0};

	struct model_s* m_currentMapModel{nullptr};
	void UpdateSkyboxForMap();
	void RenderVRHandsAndHUDAndStuff();

	bool m_fIsOnlyClientDraw = false;

	VRHelper* vrHelper = nullptr;

	bool m_isInGame{false};
	bool m_isInMenu{true};
	// BSVR: m_wasMenuJustRendered removed, the menu is captured every frame it is open (see Frame)
	unsigned int m_hlMenuTransparentTexture{0};
	bool m_CalcRefdefWasCalled{false};
	bool m_HUDRedrawWasCalled{false};

	// HUD Render stuff
	VRHUDRenderType m_hudRenderType{VRHUDRenderType::NONE};
	HSPRITE m_hudSpriteHandle{0};
	color24 m_hudSpriteColor{0, 0, 0};
	int m_iHUDFirstSpriteX = 0;
	int m_iHUDFirstSpriteY = 0;
	bool m_fIsFirstSprite = true;

	float m_hudRedrawTime{0.f};
	int m_hudRedrawIntermission{0};

	bool m_isDrawingDamage{false};
	float m_flDrawingDamageTime{0.f};
	Vector m_damageColor;

	VREyeMode m_eyeMode{VREyeMode::Both};

	bool m_isVeryFirstFrameEver{true};

	void GetStartingPosForHUDRenderType(const VRHUDRenderType m_hudRenderType, float& hudStartPositionUpOffset, float& hudStartPositionRightOffset);
	bool GetHUDSpriteOriginAndOrientation(const VRHUDRenderType m_hudRenderType, Vector& origin, Vector& forward, Vector& right, Vector& up);
	bool GetHUDAmmoOriginAndOrientation(Vector& origin, Vector& forward, Vector& right, Vector& up);
	bool GetHUDHealthOriginAndOrientation(Vector& origin, Vector& forward, Vector& right, Vector& up);
	bool GetHUDFlashlightOriginAndOrientation(Vector& origin, Vector& forward, Vector& right, Vector& up);
};

extern cl_entity_t* SaveGetLocalPlayer();

extern VRRenderer gVRRenderer;
