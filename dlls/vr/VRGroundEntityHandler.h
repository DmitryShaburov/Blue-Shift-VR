#pragma once

#include <unordered_set>
#include <string>

class CBasePlayer;

class VRGroundEntityHandler
{
public:
	VRGroundEntityHandler(CBasePlayer* pPlayer) : m_pPlayer{pPlayer}
	{
	}

	void HandleMovingWithSolidGroundEntities();

private:
	void DetectAndSetGroundEntity();
	bool CheckIfPotentialGroundEntityForPlayer(edict_t* pent);
	bool IsExcludedAsGroundEntity(edict_t* pent);
	edict_t* ChoseBetterGroundEntityForPlayer(edict_t* pent1, edict_t* pent2);
	void SendGroundEntityToClient();
	void MoveWithGroundEntity();
	Vector CalculateNewOrigin();
	bool CheckIfNewOriginIsValid(const Vector& newOrigin);

	EHandleT<CBasePlayer> m_pPlayer;
	EHandleT<CBaseEntity> m_hGroundEntity;

	Vector m_lastGroundEntityOrigin;
	Vector m_lastGroundEntityAngles;
};
