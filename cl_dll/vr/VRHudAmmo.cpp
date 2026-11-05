// VR weapon slot helpers of CHudAmmo, moved from ammo.cpp

#include "hud.h"

#include "ammohistory.h"

int CHudAmmo::FirstSlotWithWeapon()
{
	for (int slot = 0; slot < MAX_WEAPON_SLOTS; slot++)
	{
		if (FirstSlotPosWithWeapon(slot) >= 0)
		{
			return slot;
		}
	}
	return -1;
}

int CHudAmmo::FirstSlotPosWithWeapon(const int slot)
{
	for (int pos = 0; pos < MAX_WEAPON_POSITIONS; pos++)
	{
		WEAPON* wsp = gWR.GetWeaponSlot(slot, pos);
		if (wsp && gWR.HasAmmo(wsp))
		{
			return pos;
		}
	}
	return -1;
}

int CHudAmmo::LastSlotWithWeapon()
{
	for (int slot = MAX_WEAPON_SLOTS - 1; slot >= 0; slot--)
	{
		if (FirstSlotPosWithWeapon(slot) >= 0)
		{
			return slot;
		}
	}
	return -1;
}

int CHudAmmo::LastSlotPosWithWeapon(const int slot)
{
	for (int pos = MAX_WEAPON_POSITIONS - 1; pos >= 0; pos--)
	{
		WEAPON* wsp = gWR.GetWeaponSlot(slot, pos);
		if (wsp && gWR.HasAmmo(wsp))
		{
			return pos;
		}
	}
	return -1;
}

bool CHudAmmo::IsCurrentWeaponFirstWeapon()
{
	return m_pWeapon != nullptr && m_pWeapon->iSlot <= FirstSlotWithWeapon() && m_pWeapon->iSlotPos <= FirstSlotPosWithWeapon(m_pWeapon->iSlot);
}

bool CHudAmmo::IsCurrentWeaponLastWeapon()
{
	return m_pWeapon != nullptr && m_pWeapon->iSlot >= LastSlotWithWeapon() && m_pWeapon->iSlotPos >= LastSlotPosWithWeapon(m_pWeapon->iSlot);
}
