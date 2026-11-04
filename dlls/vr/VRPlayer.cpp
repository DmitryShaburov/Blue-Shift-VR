// VR methods of CBasePlayer, moved from player.cpp

#include <algorithm>

#include "extdll.h"
#include "util.h"

#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "pm_shared.h"
#include "UserMessages.h"

#include "pm_defs.h"
#include "pm_movevars.h"
#include "vr/VRPhysicsHelper.h"
#include "vr/VRMovementHandler.h"
#include "vr/VRControllerModel.h"

constexpr const float VR_RETINASCANNER_ACTIVATE_LOOK_TIME = 1.f;
std::unordered_map<EHandleT<CBaseEntity>, EHandleT<CBaseEntity>, EHandleT<CBaseEntity>::Hash, EHandleT<CBaseEntity>::Equal> g_vrRetinaScanners;
std::unordered_set<EHandleT<CBaseEntity>, EHandleT<CBaseEntity>::Hash, EHandleT<CBaseEntity>::Equal> g_vrRetinaScannerButtons;

bool CBasePlayer::CheckVRTRainButtonTouched(const Vector& buttonLeftPos, const Vector& buttonRightPos)
{
	for (auto& [id, controller] : m_vrControllers)
	{
		if (controller.IsValid())
		{
			if (VRPhysicsHelper::Instance().ModelIntersectsLine(controller.GetModel(), buttonLeftPos, buttonRightPos))
			{
				controller.AddTouch(VRController::TouchType::LIGHT_TOUCH, 0.1f);
				return true;
			}
		}
	}
	return false;
}

// VR methods:
void CBasePlayer::UpdateVRHeadset(const int timestamp, const Vector2D& hmdOffset, const float hmdOffsetZ, const Vector& hmdForward, const Vector2D& hmdYawOffsetDelta, float prevYaw, float currentYaw, bool hasReceivedRestoreYawMsg, bool hasReceivedSpawnYaw)
{
	// Filter out outdated updates
	if (timestamp <= vr_hmdLastUpdateClienttime && vr_hmdLastUpdateServertime >= gpGlobals->time)
	{
		return;
	}

	// Ignore all updates until client has processed yaw update (fixes timing issues)
	if (vr_needsToSendRestoreYawMsgToClient)
	{
		return;
	}
	if (vr_hasSentRestoreYawMsgToClient && !hasReceivedRestoreYawMsg)
	{
		return;
	}
	vr_hasSentRestoreYawMsgToClient = false;

	Vector TEMPDEBUG_originBefore = pev->origin;

	// Calculate view dir for view position offset
	Vector viewDir2D;
	UTIL_MakeVectorsPrivate(Vector{0.f, pev->angles.y, 0.f}, viewDir2D, nullptr, nullptr);
	viewDir2D.z = 0.f;
	viewDir2D = viewDir2D.Normalize();

	// Get view dir with length to hull bounds
	float vrViewOffsetDistToHullBounds = CVAR_GET_FLOAT("vr_view_dist_to_walls");
	Vector viewDirToHullBounds = viewDir2D * (VEC_HULL_MAX.x - vrViewOffsetDistToHullBounds);

	// If we just spawned we need to make sure that the HMD offset gets moved into the spawn position
	// Thus we set vr_lastHMDOffset to the current HMD offset,
	// and use the current HMD offset to set the client origin offset.
	// We also send the initial spawn yaw down to the client so it adjusts and looks in the direction of the spawn spot.
	// - Max Makes Mods, 2019-04-13
	if (vr_IsJustSpawned)
	{
		if (!vr_hasSentSpawnYawToClient)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgVRSetSpawnYaw, nullptr, pev);
			WRITE_ANGLE(vr_spawnYaw);
			MESSAGE_END();
			vr_spawnYaw = 0.f;
			vr_hasSentSpawnYawToClient = true;
			return;
		}
		if (vr_hasSentSpawnYawToClient && !hasReceivedSpawnYaw)
		{
			return;
		}
		vr_hasSentSpawnYawToClient = false;

		vr_lastHMDOffset = Vector{hmdOffset.x, hmdOffset.y, 0.f};
		vr_ClientOriginOffset = Vector{-hmdOffset.x, -hmdOffset.y, 0.f} + viewDirToHullBounds;
		vr_IsJustSpawned = false;

		return;
	}

	// We probably don't need this?
	if (vr_IsJustRestored)
	{
		//vr_ClientOriginOffset = vr_ClientOriginOffset + viewDir2D * VEC_HULL_MAX.x;
		vr_IsJustRestored = false;
	}

	vr_prevYaw = prevYaw;
	vr_currentYaw = currentYaw;

	vr_hmdLastUpdateClienttime = timestamp;
	vr_hmdLastUpdateServertime = gpGlobals->time;

	// First get origin where the client thinks it is:
	Vector clientOrigin = GetClientOrigin();

	// Add rotation offset delta into client origin (yaw rotates around play area center, so we need to adjust this here):
	clientOrigin = clientOrigin - hmdYawOffsetDelta;

	// Then get headset position:
	Vector newOrigin = clientOrigin + hmdOffset;

	// push origin back so that view position is on border of player's bounding box
	newOrigin = newOrigin - viewDirToHullBounds;

	// Use movement handler to move to new position (instead of simply teleporting)
	// Uses pm_shared code. Allows for climbing up stairs and handling all kinds of collisions with level geometry.
	pev->origin = VRMovementHandler::DoMovement(pev->origin, newOrigin, this);

	if (newOrigin.x != pev->origin.x || newOrigin.y != pev->origin.y)
	{
		//ALERT(at_console, "newOrigin != pev->origin: %f %f, %f %f\n", newOrigin.x, newOrigin.y, pev->origin.x, pev->origin.y);
		//ALERT(at_console, "Sending gmsgVRWalkedIntoWall\n");

		// tell client we walked into a wall
		MESSAGE_BEGIN(MSG_ONE, gmsgVRWalkedIntoWall, nullptr, pev);
		MESSAGE_END();
	}
	else
	{
		//ALERT(at_console, "newOrigin == pev->origin: %f %f, %f %f\n", newOrigin.x, newOrigin.y, pev->origin.x, pev->origin.y);
	}

	// Always add in newOrigin instead of pev->origin, creates much better and smoother results
	vr_ClientOriginOffset.x = clientOrigin.x - newOrigin.x;
	vr_ClientOriginOffset.y = clientOrigin.y - newOrigin.y;

	// Remember offset for wallcheck next call
	vr_lastHMDOffset.x = hmdOffset.x;
	vr_lastHMDOffset.y = hmdOffset.y;

	// HMD height and HMD direction for view_ofs and viewdir for looking at and interaction with stuff
	pev->view_ofs.z = pev->mins.z + hmdOffsetZ;
	vr_hmdForward = hmdForward;
	vr_hmdForward = vr_hmdForward.Normalize();
}

void CBasePlayer::UpdateVRController(const VRControllerID vrControllerID, const int timestamp, const bool isValid, const bool isMirrored, const Vector& offset, const Vector& angles, const Vector& velocity, bool isDragging, bool isFiring)
{
	int weaponId = WEAPON_BAREHAND;
	if (vrControllerID == VRControllerID::WEAPON)
	{
		ItemInfo itemInfo = {};
		if (m_pActiveItem != nullptr && m_pActiveItem->GetItemInfo(&itemInfo))
		{
			weaponId = itemInfo.iId;
		}
	}

	bool wasDragging = m_vrControllers[vrControllerID].IsDragging();

	m_vrControllers[vrControllerID].Update(this, timestamp, isValid, isMirrored, offset, angles, velocity, isDragging, isFiring, vrControllerID, weaponId);

	if (vrControllerID == VRControllerID::HAND && isValid && isDragging && !wasDragging && !m_vrControllers[vrControllerID].HasDraggedEntity())
	{
		constexpr const float VR_FLASHLIGHT_TOGGLE_DISTANCE_TO_HEAD = 16.f;
		float distance = (m_vrControllers[vrControllerID].GetPosition() - this->EyePosition()).Length();
		if (distance < VR_FLASHLIGHT_TOGGLE_DISTANCE_TO_HEAD + (std::max)(0.f, CVAR_GET_FLOAT("vr_view_dist_to_walls")))
		{
			if (FlashlightIsOn())
			{
				FlashlightTurnOff();
			}
			else
			{
				FlashlightTurnOn();
			}
		}
	}
}

const Vector CBasePlayer::GetWeaponPosition()
{
	if (m_vrControllers[VRControllerID::WEAPON].IsValid())
	{
		return GetClientOrigin() + m_vrControllers[VRControllerID::WEAPON].GetOffset();
	}
	else
	{
		return EyePosition();
	}
}

const Vector CBasePlayer::GetWeaponAngles()
{
	if (m_vrControllers[VRControllerID::WEAPON].IsValid())
	{
		return m_vrControllers[VRControllerID::WEAPON].GetAngles();
	}
	else
	{
		return UTIL_VecToAngles(vr_hmdForward);
	}
}

const Vector CBasePlayer::GetWeaponViewAngles()
{
	Vector angles = GetWeaponAngles();
	angles.x = -angles.x;
	return angles;
}

const Vector CBasePlayer::GetWeaponVelocity()
{
	if (m_vrControllers[VRControllerID::WEAPON].IsValid())
	{
		return m_vrControllers[VRControllerID::WEAPON].GetVelocity();
	}
	else
	{
		return Vector{};
	}
}

const Vector CBasePlayer::GetClientOrigin()
{
	return Vector(pev->origin.x + vr_ClientOriginOffset.x, pev->origin.y + vr_ClientOriginOffset.y, pev->origin.z);
}

const Vector CBasePlayer::GetClientViewOfs()
{
	return Vector((pev->origin + pev->view_ofs) - GetClientOrigin());
}

bool CBasePlayer::IsWeaponUnderWater()
{
	return UTIL_PointContents(GetWeaponPosition()) == CONTENTS_WATER || UTIL_PointContents(GetGunPosition()) == CONTENTS_WATER;
}

#define CROWBAR_BODYHIT_VOLUME 128
#define CROWBAR_WALLHIT_VOLUME 512
void CBasePlayer::PlayMeleeSmackSound(CBaseEntity* pSmackedEntity, const int weaponId, const Vector& pos, const Vector& velocity)
{
	TraceResult fakeTrace = {0};
	fakeTrace.pHit = pSmackedEntity->edict();
	fakeTrace.vecEndPos = pos;
	fakeTrace.flFraction = 0.5f;

	// Meat sounds
	if (weaponId == WEAPON_HORNETGUN)
	{
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(m_pActiveItem->edict(), CHAN_ITEM, "weapons/bullet_hit1.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
			break;
		case 1:
			EMIT_SOUND_DYN(m_pActiveItem->edict(), CHAN_ITEM, "weapons/bullet_hit2.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
			break;
		}
	}
	// Squeek and soft meat sounds
	else if (weaponId == WEAPON_SNARK)
	{
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(m_pActiveItem->edict(), CHAN_ITEM, "weapons/bullet_hit1.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
			break;
		case 1:
			EMIT_SOUND_DYN(m_pActiveItem->edict(), CHAN_ITEM, "weapons/bullet_hit2.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
			break;
		}
		EMIT_SOUND_DYN(m_pActiveItem->edict(), CHAN_VOICE, (RANDOM_FLOAT(0, 1) <= 0.5) ? "squeek/sqk_hunt2.wav" : "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 105);
	}
	// TODO: Hand slap sounds
	else if (weaponId == WEAPON_BAREHAND)
	{
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(this->edict(), CHAN_ITEM, "weapons/bullet_hit1.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
			break;
		case 1:
			EMIT_SOUND_DYN(this->edict(), CHAN_ITEM, "weapons/bullet_hit2.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
			break;
		}
	}
	// Any other weapon makes crowbar sounds
	else if (pSmackedEntity->Classify() == CLASS_NONE || pSmackedEntity->Classify() == CLASS_MACHINE)
	{
		this->m_iWeaponVolume = CROWBAR_WALLHIT_VOLUME;

		TEXTURETYPE_PlaySound(&fakeTrace, pos - velocity.Normalize() * 32.f, pos + velocity.Normalize() * 32.f, BULLET_PLAYER_CROWBAR);

		// also play crowbar strike
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(pSmackedEntity->edict(), CHAN_ITEM, "weapons/cbar_hit1.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
			break;
		case 1:
			EMIT_SOUND_DYN(pSmackedEntity->edict(), CHAN_ITEM, "weapons/cbar_hit2.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
			break;
		}

		DecalGunshot(&fakeTrace, BULLET_PLAYER_CROWBAR);
	}
	else
	{
		this->m_iWeaponVolume = CROWBAR_BODYHIT_VOLUME;

		// play thwack or smack sound
		switch (RANDOM_LONG(0, 2))
		{
		case 0:
			EMIT_SOUND_DYN(pSmackedEntity->edict(), CHAN_ITEM, "weapons/cbar_hitbod1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			EMIT_SOUND_DYN(pSmackedEntity->edict(), CHAN_ITEM, "weapons/cbar_hitbod2.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 2:
			EMIT_SOUND_DYN(pSmackedEntity->edict(), CHAN_ITEM, "weapons/cbar_hitbod3.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
	}
}

void CBasePlayer::PlayVRWeaponAnimation(int iAnim, int body)
{
	//pev->weaponanim = iAnim;
	m_vrControllers[VRControllerID::WEAPON].PlayWeaponAnimation(iAnim, body);
}

void CBasePlayer::PlayVRWeaponMuzzleflash()
{
	m_vrControllers[VRControllerID::WEAPON].PlayWeaponMuzzleflash();
}

void CBasePlayer::UpdateFlashlight()
{
	// always call GetFlashlightPose, as it sets/unsets hand model flashlight body on the appropriate controller(s)
	Vector position;
	Vector dir;
	GetFlashlightPose(position, dir);

	if (FlashlightIsOn())
	{
		if (hFlashlightMonster)
		{
			ClearBits(hFlashlightMonster->pev->effects, EF_DIMLIGHT);
			hFlashlightMonster = nullptr;
		}
		if (hFlashLight)
		{
			TraceResult tr;
			UTIL_TraceLine(position, dir * 8192., dont_ignore_monsters, edict(), &tr);
			CBaseMonster* pFlashlightMonster = CBaseEntity::SafeInstance<CBaseMonster>(tr.pHit);
			if (pFlashlightMonster != nullptr)
			{
				SetBits(pFlashlightMonster->pev->effects, EF_DIMLIGHT);
				hFlashlightMonster = pFlashlightMonster;
				hFlashLight->pev->effects = EF_NODRAW;
			}
			else
			{
				UTIL_SetOrigin(hFlashLight->pev, tr.vecEndPos - dir);
				hFlashLight->pev->effects = EF_DIMLIGHT;
			}
		}
	}
}

void CBasePlayer::StartVRTele()
{
	Vector telePos, dummy;
	GetTeleporterPose(telePos, dummy);
	m_vrControllerTeleporter.StartTele(this, telePos);
}

void CBasePlayer::StopVRTele()
{
	m_vrControllerTeleporter.StopTele(this);
}

void CBasePlayer::UpdateVRTele()
{
	Vector telePos, teleDir;
	GetTeleporterPose(telePos, teleDir);
	m_vrControllerTeleporter.UpdateTele(this, telePos, teleDir);
}

void CBasePlayer::SetFlashlightPose(const Vector& offset, const Vector& angles)
{
	m_vrFlashlightOffset = offset;
	m_vrFlashlightAngles = angles;
	m_vrHasFlashlightPose = true;
}

void CBasePlayer::ClearFlashlightPose()
{
	m_vrHasFlashlightPose = false;
}

constexpr const int VR_FLASHLIGHT_ATTACHMENT_HAND = 0;
constexpr const int VR_FLASHLIGHT_ATTACHMENT_WEAPON = 1;
constexpr const int VR_FLASHLIGHT_ATTACHMENT_HEAD = 2;
constexpr const int VR_FLASHLIGHT_ATTACHMENT_POSE = 3;

// TODO: Merge with GetTeleporterPose, super redundant code
void CBasePlayer::GetFlashlightPose(Vector& position, Vector& dir)
{
	int attachment = int(CVAR_GET_FLOAT("vr_flashlight_attachment"));

	for (auto& controller : m_vrControllers)
	{
		controller.second.SetHasFlashlight(false);
	}

	if (attachment == VR_FLASHLIGHT_ATTACHMENT_POSE)
	{
		if (m_vrHasFlashlightPose)
		{
			position = GetClientOrigin() + m_vrFlashlightOffset;
			UTIL_MakeAimVectorsPrivate(m_vrFlashlightAngles, dir, nullptr, nullptr);
			return;
		}
		else
		{
			// fallback to hand
			attachment = VR_FLASHLIGHT_ATTACHMENT_HAND;
		}
	}

	if (attachment == VR_FLASHLIGHT_ATTACHMENT_HAND)
	{
		if (m_vrControllers[VRControllerID::HAND].IsValid())
		{
			Vector pos1;
			Vector pos2;
			bool result1 = m_vrControllers[VRControllerID::HAND].GetAttachment(VR_MUZZLE_ATTACHMENT, pos1);
			bool result2 = m_vrControllers[VRControllerID::HAND].GetAttachment(VR_MUZZLE_ATTACHMENT + 1, pos2);
			if (result1 && result2 && pos2 != pos1)
			{
				position = pos1;
				dir = (pos2 - pos1).Normalize();
			}
			else
			{
				position = m_vrControllers[VRControllerID::HAND].GetPosition();
				UTIL_MakeAimVectorsPrivate(m_vrControllers[VRControllerID::HAND].GetAngles(), dir, nullptr, nullptr);
			}
			m_vrControllers[VRControllerID::HAND].SetHasFlashlight(true);
			return;
		}
		else
		{
			// fallback to weapon
			attachment = VR_FLASHLIGHT_ATTACHMENT_WEAPON;
		}
	}

	if (attachment == VR_FLASHLIGHT_ATTACHMENT_WEAPON)
	{
		if (m_vrControllers[VRControllerID::WEAPON].IsValid())
		{
			Vector pos1;
			Vector pos2;
			bool result1 = m_vrControllers[VRControllerID::WEAPON].GetAttachment(VR_MUZZLE_ATTACHMENT, pos1);
			bool result2 = m_vrControllers[VRControllerID::WEAPON].GetAttachment(VR_MUZZLE_ATTACHMENT + 1, pos2);
			if (result1 && result2 && pos2 != pos1)
			{
				position = pos1;
				dir = (pos2 - pos1).Normalize();
			}
			else
			{
				position = m_vrControllers[VRControllerID::WEAPON].GetPosition();
				UTIL_MakeAimVectorsPrivate(m_vrControllers[VRControllerID::WEAPON].GetAngles(), dir, nullptr, nullptr);
			}
			m_vrControllers[VRControllerID::WEAPON].SetHasFlashlight(true);
			return;
		}
		else
		{
			// fallback to pose
			attachment = VR_FLASHLIGHT_ATTACHMENT_WEAPON;
		}
	}

	// either attachment is pose or we fallback to pose because no other attachment was available
	position = EyePosition();
	UTIL_MakeVectorsPrivate(pev->v_angle + pev->punchangle, dir, nullptr, nullptr);
}

void CBasePlayer::SetTeleporterPose(const Vector& offset, const Vector& angles)
{
	m_vrTeleporterOffset = offset;
	m_vrTeleporterAngles = angles;
	m_vrHasTeleporterPose = true;
}

void CBasePlayer::ClearTeleporterPose()
{
	m_vrHasTeleporterPose = false;
}

// TODO: Merge with GetFlashlightPose, super redundant code
void CBasePlayer::GetTeleporterPose(Vector& position, Vector& dir)
{
	int attachment = int(CVAR_GET_FLOAT("vr_teleport_attachment"));

	if (attachment == VR_FLASHLIGHT_ATTACHMENT_POSE)
	{
		if (m_vrHasTeleporterPose)
		{
			position = GetClientOrigin() + m_vrTeleporterOffset;
			UTIL_MakeAimVectorsPrivate(m_vrTeleporterAngles, dir, nullptr, nullptr);
			return;
		}
		else
		{
			// fallback to hand
			attachment = VR_FLASHLIGHT_ATTACHMENT_HAND;
		}
	}

	if (attachment == VR_FLASHLIGHT_ATTACHMENT_HAND)
	{
		if (m_vrControllers[VRControllerID::HAND].IsValid())
		{
			Vector pos1;
			Vector pos2;
			bool result1 = m_vrControllers[VRControllerID::HAND].GetAttachment(VR_MUZZLE_ATTACHMENT, pos1);
			bool result2 = m_vrControllers[VRControllerID::HAND].GetAttachment(VR_MUZZLE_ATTACHMENT + 1, pos2);
			if (result1 && result2 && pos2 != pos1)
			{
				position = pos1;
				dir = (pos2 - pos1).Normalize();
			}
			else
			{
				position = m_vrControllers[VRControllerID::HAND].GetPosition();
				UTIL_MakeAimVectorsPrivate(m_vrControllers[VRControllerID::HAND].GetAngles(), dir, nullptr, nullptr);
			}
			return;
		}
		else
		{
			// fallback to weapon
			attachment = VR_FLASHLIGHT_ATTACHMENT_WEAPON;
		}
	}

	if (attachment == VR_FLASHLIGHT_ATTACHMENT_WEAPON)
	{
		if (m_vrControllers[VRControllerID::WEAPON].IsValid())
		{
			Vector pos1;
			Vector pos2;
			bool result1 = m_vrControllers[VRControllerID::WEAPON].GetAttachment(VR_MUZZLE_ATTACHMENT, pos1);
			bool result2 = m_vrControllers[VRControllerID::WEAPON].GetAttachment(VR_MUZZLE_ATTACHMENT + 1, pos2);
			if (result1 && result2 && pos2 != pos1)
			{
				position = pos1;
				dir = (pos2 - pos1).Normalize();
			}
			else
			{
				position = m_vrControllers[VRControllerID::WEAPON].GetPosition();
				UTIL_MakeAimVectorsPrivate(m_vrControllers[VRControllerID::WEAPON].GetAngles(), dir, nullptr, nullptr);
			}
			return;
		}
		else
		{
			// fallback to pose
			attachment = VR_FLASHLIGHT_ATTACHMENT_WEAPON;
		}
	}

	// either attachment is pose or we fallback to pose because no other attachment was available
	position = EyePosition();
	UTIL_MakeVectorsPrivate(pev->v_angle + pev->punchangle, dir, nullptr, nullptr);
}

// In VR it's rather difficult to longjump (run + crouch + jump + correct timing),
// so players can map the long jump on a simple VR input action.
// - Max Makes Mods - 2019-04-13
#define BUNNYJUMP_MAX_SPEED_FACTOR 1.7f
#define PLAYER_LONGJUMP_SPEED      350
void CBasePlayer::DoLongJump(bool playStepSound, bool force)
{
	if (!force)
	{
		if (0 != pev->waterlevel)
			return;

		if ((pev->button & IN_JUMP) != 0)
			return;

		if ((pev->oldbuttons & IN_JUMP) != 0)
			return;

		if (0 != pmove->waterjumptime)
			return;

		if ((pev->flags & FL_ONGROUND) == 0)
			return;
	}

	float maxscaledspeed = BUNNYJUMP_MAX_SPEED_FACTOR * pmove->maxspeed;
	if (maxscaledspeed <= 0.f)
		return;

	if (playStepSound)
	{
		extern void PM_PlayStepSound(int step, float fvol);
		extern int PM_MapTextureTypeStepType(char chTextureType);
		PM_PlayStepSound(PM_MapTextureTypeStepType(pmove->chtexturetype), 1.0f);
	}

	if (m_fLongJump)
	{
		pev->punchangle.x = -5.f;

		Vector forward = pmove->forward;
		if (pmove->forward.Length2D() > EPSILON)
		{
			forward = pmove->forward;
		}
		else if (pmove->velocity.Length2D() > EPSILON)
		{
			forward = pmove->velocity;
		}
		else if (pev->velocity.Length2D() > EPSILON)
		{
			forward = pev->velocity;
		}
		else
		{
			forward = vr_hmdForward; // TODO: This should fall back to the device used for movement, not simply the HMD
		}
		forward.z = 0.f;
		forward = forward.Normalize();

		pev->velocity.x = forward.x * PLAYER_LONGJUMP_SPEED * 1.6f;
		pev->velocity.y = forward.y * PLAYER_LONGJUMP_SPEED * 1.6f;
		pev->velocity.z = sqrt(2.f * 800.f * 56.f);

		SetAnimation(PLAYER_SUPERJUMP);
	}
	else
	{
		pev->velocity.z = sqrt(2.f * 800.f * 56.f);
		SetAnimation(PLAYER_JUMP);
	}

	//pev->velocity.z -= (pev->gravity * g_psv_gravity->value * gpGlobals->frametime * 0.5f);

	if (pev->velocity.x > pmove->movevars->maxvelocity)
		pev->velocity.x = pmove->movevars->maxvelocity;
	if (pev->velocity.y > pmove->movevars->maxvelocity)
		pev->velocity.y = pmove->movevars->maxvelocity;
	if (pev->velocity.z > pmove->movevars->maxvelocity)
		pev->velocity.z = pmove->movevars->maxvelocity;
	if (pev->velocity.x < -pmove->movevars->maxvelocity)
		pev->velocity.x = -pmove->movevars->maxvelocity;
	if (pev->velocity.y < -pmove->movevars->maxvelocity)
		pev->velocity.y = -pmove->movevars->maxvelocity;
	if (pev->velocity.z < -pmove->movevars->maxvelocity)
		pev->velocity.z = -pmove->movevars->maxvelocity;
}

void CBasePlayer::HolsterWeapon(bool force)
{
	// force last item to be nullptr if we are already holstered
	// (SelectItem doesn't do that as it's a no-op if the item doesn't change)
	if (force && m_pActiveItem == nullptr)
	{
		m_pLastItem = nullptr;
	}
	SelectItem("weapon_barehand");
}

void CBasePlayer::SetLadderGrabbingController(VRControllerID controller, CBaseEntity* pLadder)
{
	m_ladderGrabbingControllers.push_back(VRLadderGrabbingController{controller, pLadder});

	MESSAGE_BEGIN(MSG_ONE, gmsgVRGrabbedLadder, nullptr, pev);
	WRITE_SHORT(ENTINDEX(pLadder->edict()));
	MESSAGE_END();
}

void CBasePlayer::ClearLadderGrabbingControllers()
{
	if (m_ladderGrabbingControllers.empty())
		return;

	m_ladderGrabbingControllers.clear();
	MESSAGE_BEGIN(MSG_ONE, gmsgVRGrabbedLadder, nullptr, pev);
	WRITE_SHORT(0);
	MESSAGE_END();
}

void CBasePlayer::ClearLadderGrabbingController(VRControllerID controller)
{
	if (m_ladderGrabbingControllers.empty())
		return;

	m_ladderGrabbingControllers.erase(
		std::remove_if(
			m_ladderGrabbingControllers.begin(),
			m_ladderGrabbingControllers.end(),
			[controller](const VRLadderGrabbingController& v)
			{
				return v.controller == controller;
			}),
		m_ladderGrabbingControllers.end());

	if (m_ladderGrabbingControllers.empty())
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgVRGrabbedLadder, nullptr, pev);
		WRITE_SHORT(0);
		MESSAGE_END();
	}
}

bool CBasePlayer::IsLadderGrabbingController(VRControllerID controller, CBaseEntity* pLadder)
{
	if (m_ladderGrabbingControllers.empty())
		return false;

	return m_ladderGrabbingControllers.back().controller == controller && m_ladderGrabbingControllers.back().ladder == pLadder;
}

int CBasePlayer::GetGrabbedLadderEntIndex()
{
	if (m_ladderGrabbingControllers.empty() || (VRGetLadderMode() == VR_LADDER_MODE_LEGACY_ONLY))
		return 0;

	return ENTINDEX(m_ladderGrabbingControllers.back().ladder.Get());
}


void CBasePlayer::StartPullingLedge(const Vector& ledgeTargetPosition, float speed)
{
	m_vrLedgeTargetPosition = ledgeTargetPosition;
	m_vrLedgePullSpeed = speed;
	m_vrLedgePullStartTime = gpGlobals->time;
	m_vrLedgePullStartPosition = pev->origin;
	m_vrIsPullingOnLedge = true;

	MESSAGE_BEGIN(MSG_ONE, gmsgVRPullingLedge, nullptr, pev);
	WRITE_BYTE(1);
	MESSAGE_END();
}

void CBasePlayer::StopPullingLedge()
{
	if (!m_vrIsPullingOnLedge)
		return;

	m_vrIsPullingOnLedge = false;

	MESSAGE_BEGIN(MSG_ONE, gmsgVRPullingLedge, nullptr, pev);
	WRITE_BYTE(0);
	MESSAGE_END();
}

constexpr const int SF_TANK_CANCONTROL = 0x0020;

bool IsValidTankDraggingController(const VRController& controller, entvars_t* pevTank)
{
	return controller.IsValid() &&
		   controller.IsDragging() &&
		   !controller.HasDraggedEntity() &&
		   ((controller.GetPosition() - pevTank->origin).Length() < CVAR_GET_FLOAT("vr_tankcontrols_max_distance"));
}

bool CBasePlayer::IsTankVRControlled(entvars_t* pevTank)
{
	if (CVAR_GET_FLOAT("vr_tankcontrols") == 0.f)
		return false;

	if (!pevTank)
		return false;

	if (FBitSet(pevTank->spawnflags, SF_TANK_CANCONTROL) && UTIL_StartsWith(STRING(pevTank->classname), "func_tank"))
	{
		// vr_tankcontrols is 0 for off (checked above)
		// or 1 for one handed
		// or 2 for two handed tank controls
		if (CVAR_GET_FLOAT("vr_tankcontrols") == 1.f)
		{
			return IsValidTankDraggingController(m_vrControllers[VRControllerID::HAND], pevTank) ||
				   IsValidTankDraggingController(m_vrControllers[VRControllerID::WEAPON], pevTank);
		}
		else
		{
			return IsValidTankDraggingController(m_vrControllers[VRControllerID::HAND], pevTank) &&
				   IsValidTankDraggingController(m_vrControllers[VRControllerID::WEAPON], pevTank);
		}
	}

	return false;
}

CBaseEntity* CBasePlayer::VRFindTank(const char* func_tank_classname)
{
	edict_t* pent = FIND_ENTITY_BY_CLASSNAME(nullptr, func_tank_classname);
	while (!FNullEnt(pent))
	{
		if (IsTankVRControlled(&pent->v) &&
			(UTIL_IsFacing(pev->origin, pev->angles.ToViewAngles(), pent->v.origin) || UTIL_IsFacing(pev->origin, pev->angles.ToViewAngles(), (pent->v.absmax + pent->v.absmin) * 0.5)))
		{
			return CBaseEntity::SafeInstance<CBaseEntity>(pent);
		}
		pent = FIND_ENTITY_BY_CLASSNAME(pent, func_tank_classname);
	}
	return nullptr;
}

void CBasePlayer::VRUseOrUnuseTank()
{
	if (m_vrIsUsingTankWithVRControllers)
	{
		bool cancelUse = false;
		if (m_pTank && IsTankVRControlled(m_pTank->pev))
		{
			std::vector<Vector> allControllerPositions;
			for (auto& [id, controller] : m_vrControllers)
			{
				if (IsValidTankDraggingController(controller, m_pTank->pev))
				{
					allControllerPositions.push_back(controller.GetPosition());
				}
			}

			if (allControllerPositions.empty())
			{
				cancelUse = true;
			}
			else
			{
				Vector position;
				for (auto& controllerPosition : allControllerPositions)
				{
					position = position + controllerPosition;
				}
				position = position / float(allControllerPositions.size());

				Vector direction = (m_pTank->pev->origin - position).Normalize();
				m_vrTankVRControllerAngles = UTIL_VecToAngles(direction);

				// make sure no weapon is selected (noop if no weapon is selected)
				HolsterWeapon(true);

				// fire the gun
				m_pTank->Use(this, this, USE_SET, 2);
			}
		}
		else
		{
			cancelUse = true;
		}

		if (cancelUse)
		{
			if (m_pTank)
				m_pTank->Use(this, this, USE_OFF, 0);
			m_pTank = nullptr;
			if (m_pActiveItem == nullptr)
			{
				SelectLastItem();
			}
			m_vrIsUsingTankWithVRControllers = false;
			m_vrTankVRControllerAngles = Vector{};
		}
	}

	if (!m_vrIsUsingTankWithVRControllers)
	{
		CBaseEntity* pTank = VRFindTank("func_tank");
		if (!pTank)
			pTank = VRFindTank("func_tanklaser");
		if (!pTank)
			pTank = VRFindTank("func_tankrocket");
		if (!pTank)
			pTank = VRFindTank("func_tankmortar");

		if (pTank)
		{
			if (m_pTank && m_pTank != pTank)
			{
				m_pTank->Use(this, this, USE_OFF, 0);
			}
			m_pTank = pTank;
			m_pTank->Use(this, this, USE_ON, 1337); // 1337 is hacky way to tell CFuncTank that this is a VR initiated control
			m_vrIsUsingTankWithVRControllers = true;
			m_vrTankVRControllerAngles = m_pTank->pev->angles;
			HolsterWeapon(true);
		}
	}
}

Vector CBasePlayer::GetTankControlAngles()
{
	if (m_vrIsUsingTankWithVRControllers)
	{
		return m_vrTankVRControllerAngles;
	}
	else
	{
		Vector angles = pev->v_angle;
		angles.x = -angles.x;
		return angles;
	}
}

bool CBasePlayer::VRCanAttack()
{
	TraceResult tr{0};
	UTIL_TraceLine(pev->origin, GetGunPosition(), ignore_monsters, nullptr, &tr);
	return tr.flFraction == 1.f;
}

void CBasePlayer::VRJustTeleported(const Vector& fromOrigin, const Vector& fromAngles)
{
	// Let go off ladders if teleporting
	for (auto& bla : m_ladderGrabbingControllers)
	{
		m_vrControllers[bla.controller].ClearDraggedEntity();
	}
	ClearLadderGrabbingControllers();

	// Let go off ledges if teleporting
	StopPullingLedge();

	// Make sure grabbed objects come with us
	for (auto& [controllerID, controller] : m_vrControllers)
	{
		controller.VRJustTeleported(this, fromOrigin, fromAngles);
	}
}

void EXPORT CBaseEntity::DragThink(void)
{
	// can happen afer a levelchange, should get fixed in the next VRControllerInteractionManager::HandleGrabbables update
	if (!m_vrDragger)
	{
		return;
	}

	if (m_isFirstDragThink)
	{
		m_isFirstDragThink = false;
		HandleDragStart();
	}

	m_pfnThink = nullptr;

	EHandleT<CBasePlayer> hPlayer = m_vrDragger;
	if (hPlayer && m_vrDragController != VRControllerID::INVALID)
	{
		auto& controller = hPlayer->GetController(m_vrDragController);
		if (controller.IsValid())
		{
			Vector origin = controller.GetGunPosition();
			Vector velocity = controller.GetVelocity();
			Vector angles = controller.GetAngles();

			HandleDragUpdate(origin, velocity, UTIL_AnglesMod(angles));
		}
	}
}

void CBasePlayer::VRHandleRetinaScanners()
{
	for (auto [hRetinaScanner, hRetinaScannerButton] : g_vrRetinaScanners)
	{
		Vector retinaScannerPosition = (hRetinaScanner->pev->absmax + hRetinaScanner->pev->absmin) * 0.5;
		bool isLookingAtRetinaScanner =
			UTIL_IsFacing(pev->origin, pev->angles.ToViewAngles(), retinaScannerPosition) && ((EyePosition() - retinaScannerPosition).Length() < 32.f) && (EyePosition().z >= hRetinaScanner->pev->absmin.z) && (EyePosition().z <= hRetinaScanner->pev->absmax.z);

		if (isLookingAtRetinaScanner)
		{
			if (m_vrHRetinaScanner == hRetinaScanner)
			{
				if ((gpGlobals->time - m_vrRetinaScannerLookTime) >= VR_RETINASCANNER_ACTIVATE_LOOK_TIME)
				{
					if (!m_vrRetinaScannerUsed)
					{
						g_vrRetinaScanners[hRetinaScanner]->Use(this, this, USE_SET, 1);
						m_vrRetinaScannerUsed = true;
					}
				}
			}
			else
			{
				m_vrHRetinaScanner = hRetinaScanner;
				m_vrRetinaScannerLookTime = gpGlobals->time;
				m_vrRetinaScannerUsed = false;
			}
		}
		else
		{
			if (m_vrHRetinaScanner == hRetinaScanner)
			{
				m_vrHRetinaScanner = nullptr;
				m_vrRetinaScannerLookTime = 0.f;
				m_vrRetinaScannerUsed = false;
			}
		}
	}
}

void CBasePlayer::UpdateVRLaserSpot()
{
	int weaponId = WEAPON_BAREHAND;
	ItemInfo itemInfo = {};
	if (m_pActiveItem != nullptr && m_pActiveItem->GetItemInfo(&itemInfo))
	{
		weaponId = itemInfo.iId;
	}

	// No laser dot while a camera controls the view (trigger_camera): the hands and weapon aren't drawn then
	if (IsAlive() && IsWeaponWithVRLaserSpot(weaponId) && CVAR_GET_FLOAT("vr_enable_aim_laser") != 0.f && !m_hViewEntity)
	{
		if (!m_hLaserSpot)
		{
			CLaserSpot* pLaserSpot = CLaserSpot::CreateSpot();
			pLaserSpot->Revive();
			pLaserSpot->pev->scale = 0.1f;
			m_hLaserSpot = pLaserSpot;
		}

		TraceResult tr;
		UTIL_TraceLine(GetWeaponPosition(), GetWeaponPosition() + (GetAutoaimVector() * 8192.f), dont_ignore_monsters, edict(), &tr);
		UTIL_SetOrigin(m_hLaserSpot->pev, tr.vecEndPos);
	}
	else
	{
		if (m_hLaserSpot)
		{
			UTIL_Remove(m_hLaserSpot);
			m_hLaserSpot = nullptr;
		}
	}
}

bool CBasePlayer::IsAnyControllerFiringAndHoldingThisTank(CBaseEntity* pTank)
{
	if (pTank != m_pTank)
		return false;

	for (auto& [id, controller] : m_vrControllers)
	{
		if (controller.IsFiring() && IsValidTankDraggingController(controller, pTank->pev))
		{
			return true;
		}
	}

	return false;
}
