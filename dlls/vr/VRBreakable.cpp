// VR methods of CBreakable and CPushable, moved from func_break.cpp

#include <algorithm>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "saverestore.h"
#include "func_break.h"

void CBreakable::VRSpawnTempEnts(
	int modelIndex,
	const Vector& pos,
	const Vector& size,
	Vector direction,
	float random,
	float life,
	int count,
	char flags)
{
	if (0 == modelIndex)
		return;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pos);
	WRITE_BYTE(TE_BREAKMODEL);
	WRITE_COORDS(pos);
	WRITE_COORDS(size);
	WRITE_COORDS(direction);
	WRITE_BYTE(static_cast<int>(random));
	WRITE_SHORT(modelIndex);
	WRITE_BYTE(count);
	WRITE_BYTE(static_cast<int>(life * 10));
	WRITE_BYTE(flags);
	MESSAGE_END();
}

// never spawn more than 10 gibs
constexpr const int MAX_BREAKABLE_GIBS = 10;

int CBreakable::VRGetGibCount(int totalcount)
{
	// if not killed by player, only spawn 10% gibs (rest is tempents)
	if (!m_hKiller || !m_hKiller->IsPlayer())
		return totalcount / 10;

	switch (m_killMethod)
	{
	case KillMethod::MELEE: return totalcount;			// melee'd by player, spawn as many gibs as possible
	case KillMethod::PROJECTILE: return totalcount / 2; // shot or exploded by player, spawn 50% gibs
	case KillMethod::TOUCH: return totalcount / 3;		// player fell on this, spawn 30% gibs
	case KillMethod::TRIGGER: return totalcount / 5;	// killed by player trigger, spawn 20% gibs
	default: return totalcount / 10;					// default 10% gibs
	}
}

int CBreakable::VRSpawnGibs(const char* model,
	const Vector& pos,
	const Vector& size,
	Vector direction,
	float random,
	float life,
	int count,
	int material,
	int body, int numBodies,
	char flags)
{
	if (!model || strlen(model) == 0)
		return 0;

	// Don't spawn any gibs if we aren't in the PVS
	if (!m_isInPVS)
		return 0;

	count = VRGetGibCount(count);
	if (count == 0)
		count = 1;

	// never spawn more than MAX_BREAKABLE_GIBS gibs
	if (count > MAX_BREAKABLE_GIBS)
		count = MAX_BREAKABLE_GIBS;

	bool randomdirection = direction.LengthSquared() == 0.f;

	char type = flags & BREAK_TYPEMASK;

	int spawnedgibs = 0;

	for (int i = 0; i < count; i++)
	{
		// find a spot that isn't stuck in something solid
		int numtries = 0;
		Vector vecSpot;
		bool isstuck = false;
		do
		{
			vecSpot.x = pos.x + g_engfuncs.pfnRandomFloat(-0.5f, 0.5f) * size.x;
			vecSpot.y = pos.y + g_engfuncs.pfnRandomFloat(-0.5f, 0.5f) * size.y;
			vecSpot.z = pos.z + g_engfuncs.pfnRandomFloat(-0.5f, 0.5f) * size.z;
			isstuck = UTIL_PointContents(vecSpot) == CONTENTS_SOLID;
			++numtries;
		} while (numtries < 8 && isstuck);

		if (isstuck)
			continue;

		CGib* pGib = GetClassPtr<CGib>(nullptr);
		if (pGib)
		{
			pGib->Spawn(model);

			if (body > 0)
				pGib->pev->body = body;
			else if (numBodies > 0)
				pGib->pev->body = g_engfuncs.pfnRandomLong(0, numBodies - 1);

			if ((type == BREAK_GLASS) || (flags & BREAK_TRANS) != 0)
			{
				pGib->pev->rendermode = kRenderTransTexture;
				pGib->pev->renderamt = 128;
			}

			pGib->m_material = material;
			// pGib->m_lifeTime = life + g_engfuncs.pfnRandomLong(0.0f, 1.0f);
			pGib->pev->solid = SOLID_BBOX;

			pGib->pev->origin = vecSpot;

			// new random direction for every gib
			if (randomdirection)
			{
				direction.x = g_engfuncs.pfnRandomFloat(-1.0f, 1.0f);
				direction.y = g_engfuncs.pfnRandomFloat(-1.0f, 1.0f);
				direction.z = g_engfuncs.pfnRandomFloat(-1.0f, 1.0f);
				direction.Normalize();
			}

			pGib->pev->velocity.x = direction.x + g_engfuncs.pfnRandomFloat(-random, random);
			pGib->pev->velocity.y = direction.y + g_engfuncs.pfnRandomFloat(-random, random);
			pGib->pev->velocity.z = direction.z + g_engfuncs.pfnRandomFloat(0, random);
			pGib->pev->velocity = pGib->pev->velocity * 100;

			if (g_engfuncs.pfnRandomLong(0, 255) < 200)
			{
				pGib->pev->angles.x = g_engfuncs.pfnRandomFloat(-256, 255);
				pGib->pev->angles.y = g_engfuncs.pfnRandomFloat(-256, 255);
				pGib->pev->angles.z = g_engfuncs.pfnRandomFloat(-256, 255);
			}

			if ((flags & BREAK_SMOKE) != 0 && (g_engfuncs.pfnRandomLong(0, 255) < 100))
			{
				// smoke trail
				MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY);
				WRITE_BYTE(TE_BEAMFOLLOW);
				WRITE_SHORT(pGib->entindex());
				WRITE_SHORT(m_iSmokeTrail);
				WRITE_BYTE(static_cast<int>(life * 10 * 0.5f));
				WRITE_BYTE(1);
				WRITE_BYTE(224);
				WRITE_BYTE(224);
				WRITE_BYTE(255);
				WRITE_BYTE(255); // rgba
				MESSAGE_END();
			}

			pGib->m_bloodColor = (m_Material == matFlesh) ? BLOOD_COLOR_RED : DONT_BLEED;

			UTIL_SetSize(pGib->pev, Vector(0, 0, 0), Vector(0, 0, 0));
			pGib->LimitVelocity();

			spawnedgibs++;
		}
	}

	return spawnedgibs;
}

void CBreakable::VRSpawnBreakModels(
	const Vector& pos,
	const Vector& size,
	Vector direction,
	float random,
	float life,
	int count,
	const char* model,
	int modelIndex,
	int material,
	int body, int numBodies,
	char flags)
{
	int maxcount = atoi(CVAR_GET_STRING("vr_breakable_max_gibs"));
	if (maxcount < 1)
		return;

	if (count <= 0)
	{
		constexpr const float volumePerShard = 432.f;
		count = (size[0] * size[1] + size[1] * size[2] + size[2] * size[0]) / (volumePerShard);
	}

	int countpercentage = atoi(CVAR_GET_STRING("vr_breakable_gib_percentage"));
	if (countpercentage > 0 && countpercentage < 100)
	{
		count = (count * countpercentage) / 100;
	}

	count = std::clamp(count, 1, maxcount);

	if (random == 0.f)
		random = 10.f;

	int numtempents;

	// boring classic client-side temp entities that disappear quickly and can't be picked up
	if (CVAR_GET_FLOAT("vr_enable_interactive_debris") == 0.f)
	{
		numtempents = count;
	}
	else
	{
		int numgibsspawned = VRSpawnGibs(model, pos, size, direction, random, life, count, material, body, numBodies, flags);
		numtempents = count - numgibsspawned;
	}

	if (numtempents > 0)
	{
		VRSpawnTempEnts(modelIndex, pos, size, direction, random, life, numtempents, flags);
	}
}

// extracted from CPushable::Move so VR controller code can play push sounds
void CPushable::EmitPushSound(float length)
{
	if ((gpGlobals->time - m_soundTime) > 0.7f)
	{
		m_soundTime = gpGlobals->time;
		if (length > 0 && FBitSet(pev->flags, FL_ONGROUND))
		{
			m_lastSound = RANDOM_LONG(0, 2);
			EMIT_SOUND(ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound], 0.5, ATTN_NORM);
		}
		else
			STOP_SOUND(ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound]);
	}
}
