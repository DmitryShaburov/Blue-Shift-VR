// Dragging and batting grenades in VR, moved from ggrenade.cpp

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"

bool CGrenade::IsDraggable()
{
	if (pev->dmgtime <= gpGlobals->time)
		return false;

	if (FStringNull(pev->model))
		return false;

	if (strcmp(STRING(pev->model), "models/w_satchel.mdl") == 0)
	{
		// satchel, can be picked up and placed somewhere else
		return true;
	}
	else if (strcmp(STRING(pev->model), "models/w_grenade.mdl") == 0)
	{
		// normal grenade, can be picked up and thrown (will explode in hand if timer runs out)
		return true;
	}
	else if (strcmp(STRING(pev->model), "models/w_squeak.mdl") == 0)
	{
		// snark, can be picked up as ammo
		return true;
	}

	// 9mmAR grenade or unknown grenade type or exploded, can't be grabbed
	return false;
}

void CGrenade::HandleDragStart()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	UTIL_SetOrigin(pev, pev->origin);

	if (strcmp(STRING(pev->model), "models/w_squeak.mdl") == 0)
	{
		// snark, can be picked up as ammo

		EHandleT<CBasePlayer> hDragger = m_vrDragger;
		if (hDragger)
		{
			// kill this snark
			pev->effects = EF_NODRAW;
			pev->flags = 0;
			pev->takedamage = DAMAGE_NO;
			pev->deadflag = DEAD_DEAD;

			UTIL_Remove(this);

			// add snark item to player
			// extern int gEvilImpulse101;
			bool backup = gEvilImpulse101;
			gEvilImpulse101 = true;
			hDragger->GiveNamedItem("weapon_snark");
			gEvilImpulse101 = backup;

			// switch to snark item
			EHandleT<CBasePlayerItem> hItem = hDragger->m_rgpPlayerItems[5];
			while (hItem)
			{
				if (FClassnameIs(hItem->pev, "weapon_snark"))
				{
					hDragger->SwitchWeapon(hItem);
					break;
				}
				hItem = hItem->m_pNext;
			}
		}

		m_backupTouch = nullptr;
		m_backupThink = nullptr;

		SetTouch(nullptr);
		SetThink(nullptr);
	}
	else
	{
		m_backupTouch = m_pfnTouch;
		m_backupThink = m_pfnThink;

		SetTouch(nullptr);
		SetThink(nullptr);
	}
}

void CGrenade::HandleDragStop()
{
	if (strcmp(STRING(pev->model), "models/w_squeak.mdl") == 0)
		return;

	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_BOUNCE;
	UTIL_SetOrigin(pev, pev->origin);

	SetTouch(m_backupTouch);
	SetThink(m_backupThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CGrenade::HandleDragUpdate(const Vector& origin, const Vector& velocity, const Vector& angles)
{
	if (strcmp(STRING(pev->model), "models/w_squeak.mdl") == 0)
		return;

	CBaseEntity::HandleDragUpdate(origin, velocity, angles);
}

void CGrenade::BaseBalled(CBaseEntity* pPlayer, const Vector& velocity)
{
	// only play baseball if we are a draggable grenade
	if (IsDraggable())
		pev->velocity = velocity * 1.5f;
}
