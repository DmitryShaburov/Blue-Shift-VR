// VR gib limit and NPC reaction to thrown gibs, moved from combat.cpp and talkmonster.cpp

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "talkmonster.h"

void CGib::UpdateOnRemove()
{
	CBaseEntity::UpdateOnRemove();
	m_numGibs--;
}

void CGib::LimitNumberOfGibs()
{
	constexpr const int DEFAULT_MAX_GIBS = 50;
	int maxGibs = atoi(CVAR_GET_STRING("vr_max_interactive_debris"));
	if (maxGibs <= 0)
	{
		maxGibs = DEFAULT_MAX_GIBS;
	}

	if (m_numGibs >= maxGibs)
	{
		// Delete all gibs not in PVS
		int numGibs = m_numGibs;
		CBaseEntity* pGib = nullptr;
		int counter = 0;
		while (counter < m_numGibs && (pGib = UTIL_FindEntityByClassname(pGib, "gib")))
		{
			if (!pGib->m_isInPVS)
			{
				UTIL_Remove(pGib);
				numGibs--;
			}
			counter++;
		}

		// too many gibs are in PVS! remove 1/4 of all gibs immediately
		// (this automagically deletes gibs by age, as older gibs are "earlier" in the engine's edict array)
		counter = 0;
		while (counter < m_numGibs && numGibs >= (maxGibs * 3 / 4) && (pGib = UTIL_FindEntityByClassname(pGib, "gib")))
		{
			// Don't remove gibs currently being dragged
			if (!pGib->m_vrDragger)
			{
				UTIL_Remove(pGib);
				numGibs--;
			}
			counter++;
		}
		m_numGibs = numGibs;

		// Fade out enough gibs to reduce to half of max gibs
		// (this automagically fades out gibs by age, as older gibs are "earlier" in the engine's edict array)
		int fadeoutgibs = m_numGibs - maxGibs / 2;
		pGib = nullptr;
		while (fadeoutgibs > 0 && (pGib = UTIL_FindEntityByClassname(pGib, "gib")))
		{
			// Don't fade out gibs currently being dragged
			if (!pGib->m_vrDragger)
			{
				pGib->SetThink(&CGib::SUB_StartFadeOut);
				pGib->pev->nextthink = gpGlobals->time;
			}
			fadeoutgibs--;
		}
	}
}

// react to player throwing stuff at me
void CTalkMonster::GibAttack(EHandleT<CBaseEntity> thrower, const Vector& pos, int bloodcolor)
{
	if (!thrower || !thrower->IsPlayer())
		return;

	if (!FOkToSpeak())
		return;

	if ((bloodcolor == BLOOD_COLOR_RED || bloodcolor == BLOOD_COLOR_GREEN) && RANDOM_LONG(0, 1) == 0)
	{
		PlaySentence(m_szGrp[TLK_GIB_BLOODY], RANDOM_FLOAT(2.8, 3.2), VOL_NORM, ATTN_IDLE);
	}
	else
	{
		PlaySentence(m_szGrp[TLK_GIB_NEUTRAL], RANDOM_FLOAT(2.8, 3.2), VOL_NORM, ATTN_IDLE);
	}

	MakeIdealYaw(thrower->pev->origin);
	IdleHeadTurn(thrower->pev->origin);


	// if (thrower->IsNetClient())
	// {
	// 	UTIL_VRGiveAchievement(thrower, VRAchievement::GEN_CATCH);
	// }
}
