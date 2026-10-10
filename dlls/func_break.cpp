/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== bmodels.cpp ========================================================

  spawn, think, and use functions for entities that use brush models

*/
// BSVR start
#include <algorithm>
// BSVR end

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "saverestore.h"
#include "func_break.h"
#include "decals.h"
#include "explode.h"

// =================== FUNC_Breakable ==============================================

// Just add more items to the bottom of this array and they will automagically be supported
// This is done instead of just a classname in the FGD so we can control which entities can
// be spawned, and still remain fairly flexible
const char* CBreakable::pSpawnObjects[] =
	{
		NULL,				  // 0
		"item_battery",		  // 1
		"item_healthkit",	  // 2
		"weapon_9mmhandgun",  // 3
		"ammo_9mmclip",		  // 4
		"weapon_9mmAR",		  // 5
		"ammo_9mmAR",		  // 6
		"ammo_ARgrenades",	  // 7
		"weapon_shotgun",	  // 8
		"ammo_buckshot",	  // 9
		"weapon_crossbow",	  // 10
		"ammo_crossbow",	  // 11
		"weapon_357",		  // 12
		"ammo_357",			  // 13
		"weapon_rpg",		  // 14
		"ammo_rpgclip",		  // 15
		"ammo_gaussclip",	  // 16
		"weapon_handgrenade", // 17
		"weapon_tripmine",	  // 18
		"weapon_satchel",	  // 19
		"weapon_snark",		  // 20
		"weapon_hornetgun",	  // 21
};

bool CBreakable::KeyValue(KeyValueData* pkvd)
{
	// UNDONE_WC: explicitly ignoring these fields, but they shouldn't be in the map file!
	if (FStrEq(pkvd->szKeyName, "explosion"))
	{
		if (!stricmp(pkvd->szValue, "directed"))
			m_Explosion = expDirected;
		else if (!stricmp(pkvd->szValue, "random"))
			m_Explosion = expRandom;
		else
			m_Explosion = expRandom;

		if (!stricmp(pkvd->szValue, "1"))
		{
			m_Explosion = expDirected;
		}
		else
		{
			m_Explosion = expRandom;
		}

		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "material"))
	{
		int i = atoi(pkvd->szValue);

		// 0:glass, 1:metal, 2:flesh, 3:wood

		if ((i < 0) || (i >= matLastMaterial))
			m_Material = matWood;
		else
			m_Material = (Materials)i;

		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "deadmodel"))
	{
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "shards"))
	{
		//			m_iShards = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gibmodel"))
	{
		m_iszGibModel = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "spawnobject"))
	{
		int object = atoi(pkvd->szValue);
		if (object > 0 && object < ARRAYSIZE(pSpawnObjects))
			m_iszSpawnObject = MAKE_STRING(pSpawnObjects[object]);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "explodemagnitude"))
	{
		ExplosionSetMagnitude(atoi(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "lip"))
		return true;

	return CBaseDelay::KeyValue(pkvd);
}


//
// func_breakable - bmodel that breaks into pieces after taking damage
//
LINK_ENTITY_TO_CLASS(func_breakable, CBreakable);
TYPEDESCRIPTION CBreakable::m_SaveData[] =
	{
		DEFINE_FIELD(CBreakable, m_Material, FIELD_INTEGER),
		DEFINE_FIELD(CBreakable, m_Explosion, FIELD_INTEGER),

		// Don't need to save/restore these because we precache after restore
		//	DEFINE_FIELD( CBreakable, m_idShard, FIELD_INTEGER ),

		DEFINE_FIELD(CBreakable, m_angle, FIELD_FLOAT),
		DEFINE_FIELD(CBreakable, m_iszGibModel, FIELD_STRING),
		DEFINE_FIELD(CBreakable, m_iszSpawnObject, FIELD_STRING),

		// Explosion magnitude is stored in pev->impulse
};

IMPLEMENT_SAVERESTORE(CBreakable, CBaseEntity);

void CBreakable::Spawn()
{
	Precache();

	if (FBitSet(pev->spawnflags, SF_BREAK_TRIGGER_ONLY))
		pev->takedamage = DAMAGE_NO;
	else
		pev->takedamage = DAMAGE_YES;

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	m_angle = pev->angles.y;
	pev->angles.y = 0;

	// HACK:  matGlass can receive decals, we need the client to know about this
	//  so use class to store the material flag
	if (m_Material == matGlass)
	{
		pev->playerclass = 1;
	}

	SET_MODEL(ENT(pev), STRING(pev->model)); //set size and link into world.

	SetTouch(&CBreakable::BreakTouch);
	if (FBitSet(pev->spawnflags, SF_BREAK_TRIGGER_ONLY)) // Only break on trigger
		SetTouch(NULL);

	// Flag unbreakable glass as "worldbrush" so it will block ALL tracelines
	if (!IsBreakable() && pev->rendermode != kRenderNormal)
		pev->flags |= FL_WORLDBRUSH;
}


const char* CBreakable::pSoundsWood[] =
	{
		"debris/wood1.wav",
		"debris/wood2.wav",
		"debris/wood3.wav",
};

const char* CBreakable::pSoundsFlesh[] =
	{
		"debris/flesh1.wav",
		"debris/flesh2.wav",
		"debris/flesh3.wav",
		"debris/flesh5.wav",
		"debris/flesh6.wav",
		"debris/flesh7.wav",
};

const char* CBreakable::pSoundsMetal[] =
	{
		"debris/metal1.wav",
		"debris/metal2.wav",
		"debris/metal3.wav",
};

const char* CBreakable::pSoundsConcrete[] =
	{
		"debris/concrete1.wav",
		"debris/concrete2.wav",
		"debris/concrete3.wav",
};


const char* CBreakable::pSoundsGlass[] =
	{
		"debris/glass1.wav",
		"debris/glass2.wav",
		"debris/glass3.wav",
};

const char** CBreakable::MaterialSoundList(Materials precacheMaterial, int& soundCount)
{
	const char** pSoundList = NULL;

	switch (precacheMaterial)
	{
	case matWood:
		pSoundList = pSoundsWood;
		soundCount = ARRAYSIZE(pSoundsWood);
		break;
	case matFlesh:
		pSoundList = pSoundsFlesh;
		soundCount = ARRAYSIZE(pSoundsFlesh);
		break;
	case matComputer:
	case matUnbreakableGlass:
	case matGlass:
		pSoundList = pSoundsGlass;
		soundCount = ARRAYSIZE(pSoundsGlass);
		break;

	case matMetal:
		pSoundList = pSoundsMetal;
		soundCount = ARRAYSIZE(pSoundsMetal);
		break;

	case matCinderBlock:
	case matRocks:
		pSoundList = pSoundsConcrete;
		soundCount = ARRAYSIZE(pSoundsConcrete);
		break;


	case matCeilingTile:
	case matNone:
	default:
		soundCount = 0;
		break;
	}

	return pSoundList;
}

void CBreakable::MaterialSoundPrecache(Materials precacheMaterial)
{
	const char** pSoundList;
	int i, soundCount = 0;

	pSoundList = MaterialSoundList(precacheMaterial, soundCount);

	for (i = 0; i < soundCount; i++)
	{
		PRECACHE_SOUND((char*)pSoundList[i]);
	}
}

void CBreakable::MaterialSoundRandom(edict_t* pEdict, Materials soundMaterial, float volume)
{
	const char** pSoundList;
	int soundCount = 0;

	pSoundList = MaterialSoundList(soundMaterial, soundCount);

	if (0 != soundCount)
		EMIT_SOUND(pEdict, CHAN_BODY, pSoundList[RANDOM_LONG(0, soundCount - 1)], volume, 1.0);
}


void CBreakable::Precache()
{
	const char* pGibName;

	switch (m_Material)
	{
	default: //Wood is default, needs to match constant used in KeyValue
	case matWood:
		pGibName = "models/woodgibs.mdl";
		// BSVR start
		m_numGibBodies = 3;
		// BSVR end


		PRECACHE_SOUND("debris/bustcrate1.wav");
		PRECACHE_SOUND("debris/bustcrate2.wav");
		break;
	case matFlesh:
		pGibName = "models/fleshgibs.mdl";
		// BSVR start
		m_numGibBodies = 4;
		// BSVR end

		PRECACHE_SOUND("debris/bustflesh1.wav");
		PRECACHE_SOUND("debris/bustflesh2.wav");
		break;
	case matComputer:
		PRECACHE_SOUND("buttons/spark5.wav");
		PRECACHE_SOUND("buttons/spark6.wav");
		pGibName = "models/computergibs.mdl";
		// BSVR start
		m_numGibBodies = 15;
		// BSVR end

		PRECACHE_SOUND("debris/bustmetal1.wav");
		PRECACHE_SOUND("debris/bustmetal2.wav");
		break;

	case matUnbreakableGlass:
	case matGlass:
		pGibName = "models/glassgibs.mdl";
		// BSVR start
		m_numGibBodies = 8;
		// BSVR end

		PRECACHE_SOUND("debris/bustglass1.wav");
		PRECACHE_SOUND("debris/bustglass2.wav");
		break;
	case matMetal:
		pGibName = "models/metalplategibs.mdl";
		// BSVR start
		m_numGibBodies = 13;
		// BSVR end

		PRECACHE_SOUND("debris/bustmetal1.wav");
		PRECACHE_SOUND("debris/bustmetal2.wav");
		break;
	case matCinderBlock:
		pGibName = "models/cindergibs.mdl";
		// BSVR start
		m_numGibBodies = 9;
		// BSVR end

		PRECACHE_SOUND("debris/bustconcrete1.wav");
		PRECACHE_SOUND("debris/bustconcrete2.wav");
		break;
	case matRocks:
		pGibName = "models/rockgibs.mdl";
		// BSVR start
		m_numGibBodies = 3;
		// BSVR end

		PRECACHE_SOUND("debris/bustconcrete1.wav");
		PRECACHE_SOUND("debris/bustconcrete2.wav");
		break;
	case matCeilingTile:
		pGibName = "models/ceilinggibs.mdl";
		// BSVR start
		m_numGibBodies = 4;
		// BSVR end

		PRECACHE_SOUND("debris/bustceiling.wav");
		break;
	}

	// BSVR start
	m_iSmokeTrail = PRECACHE_MODEL("sprites/smoke.spr");
	// BSVR end

	MaterialSoundPrecache(m_Material);

	// BSVR start - added else condition
	if (!FStringNull(m_iszGibModel))
	{
		pGibName = STRING(m_iszGibModel);
		// TODO: m_numGibBodies???
	}
	else if (pGibName)
	{
		m_iszGibModel = ALLOC_STRING(pGibName);
	}

	// added condition
	if (pGibName)
		m_idShard = PRECACHE_MODEL((char*)pGibName);
	// BSVR end

	// Precache the spawn item's data
	if (!FStringNull(m_iszSpawnObject))
		UTIL_PrecacheOther((char*)STRING(m_iszSpawnObject));
}

// play shard sound when func_breakable takes damage.
// the more damage, the louder the shard sound.


void CBreakable::DamageSound()
{
	int pitch;
	float fvol;
	const char* rgpsz[6];
	int i;
	int material = m_Material;

	//	if (RANDOM_LONG(0,1))
	//		return;

	if (RANDOM_LONG(0, 2))
		pitch = PITCH_NORM;
	else
		pitch = 95 + RANDOM_LONG(0, 34);

	fvol = RANDOM_FLOAT(0.75, 1.0);

	if (material == matComputer && RANDOM_LONG(0, 1))
		material = matMetal;

	switch (material)
	{
	case matComputer:
	case matGlass:
	case matUnbreakableGlass:
		rgpsz[0] = "debris/glass1.wav";
		rgpsz[1] = "debris/glass2.wav";
		rgpsz[2] = "debris/glass3.wav";
		i = 3;
		break;

	default: //Wood is default, needs to match constant used in KeyValue
	case matWood:
		rgpsz[0] = "debris/wood1.wav";
		rgpsz[1] = "debris/wood2.wav";
		rgpsz[2] = "debris/wood3.wav";
		i = 3;
		break;

	case matMetal:
		rgpsz[0] = "debris/metal1.wav";
		rgpsz[1] = "debris/metal3.wav";
		rgpsz[2] = "debris/metal2.wav";
		i = 2;
		break;

	case matFlesh:
		rgpsz[0] = "debris/flesh1.wav";
		rgpsz[1] = "debris/flesh2.wav";
		rgpsz[2] = "debris/flesh3.wav";
		rgpsz[3] = "debris/flesh5.wav";
		rgpsz[4] = "debris/flesh6.wav";
		rgpsz[5] = "debris/flesh7.wav";
		i = 6;
		break;

	case matRocks:
	case matCinderBlock:
		rgpsz[0] = "debris/concrete1.wav";
		rgpsz[1] = "debris/concrete2.wav";
		rgpsz[2] = "debris/concrete3.wav";
		i = 3;
		break;

	case matCeilingTile:
		// UNDONE: no ceiling tile shard sound yet
		i = 0;
		break;
	}

	if (0 != i)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, rgpsz[RANDOM_LONG(0, i - 1)], fvol, ATTN_NORM, 0, pitch);
}

void CBreakable::BreakTouch(CBaseEntity* pOther)
{
	float flDamage;
	entvars_t* pevToucher = pOther->pev;

	// only players can break these right now
	if (!pOther->IsPlayer() || !IsBreakable())
	{
		return;
	}

	if (FBitSet(pev->spawnflags, SF_BREAK_TOUCH))
	{ // can be broken when run into
		flDamage = pevToucher->velocity.Length() * 0.01;

		if (flDamage >= pev->health)
		{
			SetTouch(NULL);
			TakeDamage(pevToucher, pevToucher, flDamage, DMG_CRUSH);

			// do a little damage to player if we broke glass or computer
			pOther->TakeDamage(pev, pev, flDamage / 4, DMG_SLASH);
		}
	}

	if (FBitSet(pev->spawnflags, SF_BREAK_PRESSURE) && pevToucher->absmin.z >= pev->maxs.z - 2)
	{ // can be broken when stood upon

		// play creaking sound here.
		DamageSound();

		// BSVR start
		m_hKiller = pOther;
		m_killMethod = KillMethod::TOUCH;
		// BSVR end

		SetThink(&CBreakable::Die);
		SetTouch(NULL);

		if (m_flDelay == 0)
		{ // !!!BUGBUG - why doesn't zero delay work?
			m_flDelay = 0.1;
		}

		pev->nextthink = pev->ltime + m_flDelay;
	}
}


//
// Smash the our breakable object
//

// Break when triggered
void CBreakable::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (IsBreakable())
	{
		pev->angles.y = m_angle;
		UTIL_MakeVectors(pev->angles);
		g_vecAttackDir = gpGlobals->v_forward;

		// BSVR start
		m_hKiller = pActivator;
		m_killMethod = KillMethod::TRIGGER;
		// BSVR end

		Die();
	}
}


void CBreakable::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	// random spark if this is a 'computer' object
	if (RANDOM_LONG(0, 1))
	{
		switch (m_Material)
		{
		case matComputer:
		{
			UTIL_Sparks(ptr->vecEndPos);

			float flVolume = RANDOM_FLOAT(0.7, 1.0); //random volume range
			switch (RANDOM_LONG(0, 1))
			{
			case 0:
				EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark5.wav", flVolume, ATTN_NORM);
				break;
			case 1:
				EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark6.wav", flVolume, ATTN_NORM);
				break;
			}
		}
		break;

		case matUnbreakableGlass:
			UTIL_Ricochet(ptr->vecEndPos, RANDOM_FLOAT(0.5, 1.5));
			break;
		}
	}

	CBaseDelay::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

//=========================================================
// Special takedamage for func_breakable. Allows us to make
// exceptions that are breakable-specific
// bitsDamageType indicates the type of damage sustained ie: DMG_CRUSH
//=========================================================
bool CBreakable::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	Vector vecTemp;

	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin).
	if (pevAttacker == pevInflictor)
	{
		vecTemp = pevInflictor->origin - (pev->absmin + (pev->size * 0.5));

		// if a client hit the breakable with a crowbar, and breakable is crowbar-sensitive, break it now.
		if (FBitSet(pevAttacker->flags, FL_CLIENT) &&
			FBitSet(pev->spawnflags, SF_BREAK_CROWBAR) && (bitsDamageType & DMG_CLUB) != 0)
			flDamage = pev->health;
	}
	else
	// an actual missile was involved.
	{
		vecTemp = pevInflictor->origin - (pev->absmin + (pev->size * 0.5));
	}

	if (!IsBreakable())
		return false;

	// Breakables take double damage from the crowbar
	if ((bitsDamageType & DMG_CLUB) != 0)
		flDamage *= 2;

	// Boxes / glass / etc. don't take much poison damage, just the impact of the dart - consider that 10%
	if ((bitsDamageType & DMG_POISON) != 0)
		flDamage *= 0.1;

	// this global is still used for glass and other non-monster killables, along with decals.
	g_vecAttackDir = vecTemp.Normalize();

	// do the damage
	pev->health -= flDamage;
	if (pev->health <= 0)
	{
		Killed(pevAttacker, GIB_NORMAL);

		// BSVR start
		m_hKiller = CBaseEntity::SafeInstance<CBaseEntity>(pevInflictor);
		m_killMethod = (pevAttacker == pevInflictor) ? KillMethod::MELEE : KillMethod::PROJECTILE;
		// BSVR end

		Die();
		return false;
	}

	// Make a shard noise each time func breakable is hit.
	// Don't play shard noise if cbreakable actually died.

	DamageSound();

	return true;
}


void CBreakable::Die()
{
	// BSVR start
	// destroyed by trigger
	// if (m_killMethod == KillMethod::TRIGGER)
	// {
	// 	// map with kitchen
	// 	if (FStrEq(STRING(INDEXENT(0)->v.model), "maps/c1a0d.bsp"))
	// 	{
	// 		// microwave stew
	// 		if (FStrEq(STRING(pev->targetname), "stewgibs"))
	// 		{
	// 			UTIL_VRGiveAchievementAll(VRAchievement::AM_MAGNUSSON);
	// 		}
	// 	}
	// }

	// // Blast Pit map outside tentacles/rocket silo
	// if (FStrEq(STRING(INDEXENT(0)->v.model), "maps/c1a4b.bsp"))
	// {
	// 	if (FStrEq(STRING(pev->targetname), "barrel_explosion_1")
	// 		|| FStrEq(STRING(pev->targetname), "barrel_explosion_1d")
	// 		|| FStrEq(STRING(pev->targetname), "barrel_explosion_1e"))
	// 	{
	// 		VRAchievementsAndStatsTracker::PlayerDestroyedBlastPitBridge();
	// 	}
	// }
	// // Blast Pit map with that big fan
	// else if (FStrEq(STRING(INDEXENT(0)->v.model), "maps/c1a4e.bsp"))
	// {
	// 	// grate above big fan, if this breaks, player solved puzzle or cheated
	// 	if (FStrEq(STRING(pev->model), "*24"))
	// 	{
	// 		VRAchievementsAndStatsTracker::PlayerSolvedBlastPitFan();
	// 	}
	// }
	// BSVR end

	// Don't allow explosives to damage this again to prevent spawning multiple copies of items and gibs.
	if (pev->solid == SOLID_NOT)
	{
		return;
	}

	Vector vecSpot;		// shard origin
	Vector vecVelocity; // shard velocity
	CBaseEntity* pEntity = NULL;
	char cFlag = 0;
	int pitch;
	float fvol;

	pitch = 95 + RANDOM_LONG(0, 29);

	if (pitch > 97 && pitch < 103)
		pitch = 100;

	// The more negative pev->health, the louder
	// the sound should be.

	fvol = RANDOM_FLOAT(0.85, 1.0) + (fabs(pev->health) / 100.0);

	if (fvol > 1.0)
		fvol = 1.0;


	switch (m_Material)
	{
	case matGlass:
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustglass1.wav", fvol, ATTN_NORM, 0, pitch);
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustglass2.wav", fvol, ATTN_NORM, 0, pitch);
			break;
		}
		cFlag = BREAK_GLASS;
		break;

	case matWood:
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustcrate1.wav", fvol, ATTN_NORM, 0, pitch);
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustcrate2.wav", fvol, ATTN_NORM, 0, pitch);
			break;
		}
		cFlag = BREAK_WOOD;
		break;

	case matComputer:
	case matMetal:
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustmetal1.wav", fvol, ATTN_NORM, 0, pitch);
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustmetal2.wav", fvol, ATTN_NORM, 0, pitch);
			break;
		}
		cFlag = BREAK_METAL;
		break;

	case matFlesh:
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustflesh1.wav", fvol, ATTN_NORM, 0, pitch);
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustflesh2.wav", fvol, ATTN_NORM, 0, pitch);
			break;
		}
		cFlag = BREAK_FLESH;
		break;

	case matRocks:
	case matCinderBlock:
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustconcrete1.wav", fvol, ATTN_NORM, 0, pitch);
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustconcrete2.wav", fvol, ATTN_NORM, 0, pitch);
			break;
		}
		cFlag = BREAK_CONCRETE;
		break;

	case matCeilingTile:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustceiling.wav", fvol, ATTN_NORM, 0, pitch);
		break;
	}


	if (m_Explosion == expDirected)
		vecVelocity = -g_vecAttackDir * 200;
	else
	{
		vecVelocity.x = 0;
		vecVelocity.y = 0;
		vecVelocity.z = 0;
	}

	vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;
	// BSVR start
	VRSpawnBreakModels(vecSpot, pev->size, vecVelocity, 10, 2.5f, 0, STRING(m_iszGibModel), m_idShard, m_Material, 0, m_numGibBodies, cFlag);
	// original code:
	// MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSpot);
	// WRITE_BYTE(TE_BREAKMODEL);

	// // position
	// WRITE_COORD(vecSpot.x);
	// WRITE_COORD(vecSpot.y);
	// WRITE_COORD(vecSpot.z);

	// // size
	// WRITE_COORD(pev->size.x);
	// WRITE_COORD(pev->size.y);
	// WRITE_COORD(pev->size.z);

	// // velocity
	// WRITE_COORD(vecVelocity.x);
	// WRITE_COORD(vecVelocity.y);
	// WRITE_COORD(vecVelocity.z);

	// // randomization
	// WRITE_BYTE(10);

	// // Model
	// WRITE_SHORT(m_idShard); //model id#

	// // # of shards
	// WRITE_BYTE(0); // let client decide

	// // duration
	// WRITE_BYTE(25); // 2.5 seconds

	// // flags
	// WRITE_BYTE(cFlag);
	// MESSAGE_END();

	// float size = pev->size.x;
	// if (size < pev->size.y)
	// 	size = pev->size.y;
	// if (size < pev->size.z)
	// 	size = pev->size.z;
	// BSVR end

	// !!! HACK  This should work!
	// Build a box above the entity that looks like an 8 pixel high sheet
	Vector mins = pev->absmin;
	Vector maxs = pev->absmax;
	mins.z = pev->absmax.z;
	maxs.z += 8;

	// BUGBUG -- can only find 256 entities on a breakable -- should be enough
	CBaseEntity* pList[256];
	int count = UTIL_EntitiesInBox(pList, 256, mins, maxs, FL_ONGROUND);
	if (0 != count)
	{
		for (int i = 0; i < count; i++)
		{
			ClearBits(pList[i]->pev->flags, FL_ONGROUND);
			pList[i]->pev->groundentity = NULL;

			// BSVR start
			// give pushables a nudge to fall down
			CPushable* pPushable = dynamic_cast<CPushable*>(pList[i]);
			if (pPushable)
			{
				pPushable->pev->velocity.z -= 1;
			}
			// BSVR end
		}
	}

	// Don't fire something that could fire myself
	pev->targetname = 0;

	pev->solid = SOLID_NOT;
	// Fire targets on break
	SUB_UseTargets(NULL, USE_TOGGLE, 0);

	SetThink(&CBreakable::SUB_Remove);
	pev->nextthink = pev->ltime + 0.1;
	if (!FStringNull(m_iszSpawnObject))
		CBaseEntity::Create((char*)STRING(m_iszSpawnObject), VecBModelOrigin(pev), pev->angles, edict());


	if (Explodable())
	{
		ExplosionCreate(Center(), pev->angles, edict(), ExplosionMagnitude(), true);
	}
}

// BSVR start
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
	if (!modelIndex)
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
	case KillMethod::MELEE: return totalcount;				// melee'd by player, spawn as many gibs as possible
	case KillMethod::PROJECTILE: return totalcount / 2;		// shot or exploded by player, spawn 50% gibs
	case KillMethod::TOUCH: return totalcount / 3;			// player fell on this, spawn 30% gibs
	case KillMethod::TRIGGER: return totalcount / 5;		// killed by player trigger, spawn 20% gibs
	default: return totalcount / 10;						// default 10% gibs
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

			if ((type == BREAK_GLASS) || (flags & BREAK_TRANS))
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

			if ((flags & BREAK_SMOKE) && (g_engfuncs.pfnRandomLong(0, 255) < 100))
			{
				// smoke trail
				MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY);
				WRITE_BYTE(TE_BEAMFOLLOW);
				WRITE_SHORT(pGib->entindex());
				WRITE_SHORT(m_iSmokeTrail);
				WRITE_BYTE(static_cast<int>(life * 10 * 0.5f));
				WRITE_BYTE(1);
				WRITE_BYTE(224); WRITE_BYTE(224); WRITE_BYTE(255); WRITE_BYTE(255);	// rgba
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
// BSVR end



bool CBreakable::IsBreakable()
{
	return m_Material != matUnbreakableGlass;
}


int CBreakable::DamageDecal(int bitsDamageType)
{
	if (m_Material == matGlass)
		return DECAL_GLASSBREAK1 + RANDOM_LONG(0, 2);

	if (m_Material == matUnbreakableGlass)
		return DECAL_BPROOF1;

	return CBaseEntity::DamageDecal(bitsDamageType);
}


// BSVR start
// CPushable class declaration moved to func_break.h so VR controller code can dynamic_cast to it
// BSVR end

TYPEDESCRIPTION CPushable::m_SaveData[] =
	{
		DEFINE_FIELD(CPushable, m_maxSpeed, FIELD_FLOAT),
		DEFINE_FIELD(CPushable, m_soundTime, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CPushable, CBreakable);

LINK_ENTITY_TO_CLASS(func_pushable, CPushable);

const char* CPushable::m_soundNames[3] = {"debris/pushbox1.wav", "debris/pushbox2.wav", "debris/pushbox3.wav"};


void CPushable::Spawn()
{
	if ((pev->spawnflags & SF_PUSH_BREAKABLE) != 0)
		CBreakable::Spawn();
	else
		Precache();

	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), STRING(pev->model));

	if (pev->friction > 399)
		pev->friction = 399;

	m_maxSpeed = 400 - pev->friction;
	SetBits(pev->flags, FL_FLOAT);
	pev->friction = 0;

	pev->origin.z += 1; // Pick up off of the floor
	UTIL_SetOrigin(pev, pev->origin);

	// Multiply by area of the box's cross-section (assume 1000 units^3 standard volume)
	pev->skin = (pev->skin * (pev->maxs.x - pev->mins.x) * (pev->maxs.y - pev->mins.y)) * 0.0005;
	m_soundTime = 0;
}


void CPushable::Precache()
{
	for (int i = 0; i < 3; i++)
		PRECACHE_SOUND(m_soundNames[i]);

	if ((pev->spawnflags & SF_PUSH_BREAKABLE) != 0)
		CBreakable::Precache();
}


bool CPushable::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "size"))
	{
		int bbox = atoi(pkvd->szValue);

		switch (bbox)
		{
		case 0: // Point
			UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
			break;

		case 2: // Big Hull!?!?	!!!BUGBUG Figure out what this hull really is
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN * 2, VEC_DUCK_HULL_MAX * 2);
			break;

		case 3: // Player duck
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
			break;

		default:
		case 1: // Player
			UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
			break;
		}

		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "buoyancy"))
	{
		pev->skin = atof(pkvd->szValue);
		return true;
	}

	return CBreakable::KeyValue(pkvd);
}


// Pull the func_pushable
void CPushable::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
	{
		if ((pev->spawnflags & SF_PUSH_BREAKABLE) != 0)
			this->CBreakable::Use(pActivator, pCaller, useType, value);
		return;
	}

	if (pActivator->pev->velocity != g_vecZero)
		Move(pActivator, false);
}


void CPushable::Touch(CBaseEntity* pOther)
{
	if (FClassnameIs(pOther->pev, "worldspawn"))
		return;

	Move(pOther, true);
}


void CPushable::Move(CBaseEntity* pOther, bool push)
{
	entvars_t* pevToucher = pOther->pev;
	bool playerTouch = false;

	// Is entity standing on this pushable ?
	if (FBitSet(pevToucher->flags, FL_ONGROUND) && pevToucher->groundentity && VARS(pevToucher->groundentity) == pev)
	{
		// Only push if floating
		if (pev->waterlevel > 0)
			pev->velocity.z += pevToucher->velocity.z * 0.1;

		return;
	}


	if (pOther->IsPlayer())
	{
		// JoshA: Used to check for FORWARD too and logic was inverted
		// from comment which seems wrong.
		// Fixed to just check for USE being not set for PUSH.
		// Should have the right effect.
		if (push && !!(pevToucher->button & IN_USE)) // Don't push unless the player is not useing (pull)
			return;
		playerTouch = true;
	}

	float factor;

	if (playerTouch)
	{
		if ((pevToucher->flags & FL_ONGROUND) == 0) // Don't push away from jumping/falling players unless in water
		{
			if (pev->waterlevel < 1)
				return;
			else
				factor = 0.1;
		}
		else
			factor = 1;
	}
	else
		factor = 0.25;

	// This used to be added every 'frame', but to be consistent at high fps,
	// now act as if it's added at a constant rate with a fudge factor.
	extern cvar_t sv_pushable_fixed_tick_fudge;

	if (!push && sv_pushable_fixed_tick_fudge.value >= 0.0f)
	{
		factor *= gpGlobals->frametime * sv_pushable_fixed_tick_fudge.value;
	}

	// JoshA: Always apply this if pushing, or if under the player's velocity.
	if (push || (abs(pev->velocity.x) < abs(pevToucher->velocity.x - pevToucher->velocity.x * factor)))
		pev->velocity.x += pevToucher->velocity.x * factor;
	if (push || (abs(pev->velocity.y) < abs(pevToucher->velocity.y - pevToucher->velocity.y * factor)))
		pev->velocity.y += pevToucher->velocity.y * factor;

	float length = sqrt(pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y);
	if (length > MaxSpeed())
	{
		pev->velocity.x = (pev->velocity.x * MaxSpeed() / length);
		pev->velocity.y = (pev->velocity.y * MaxSpeed() / length);
	}
	if (playerTouch)
	{
		// JoshA: Match the player to our pushable's velocity.
		// Previously this always happened, but it should only
		// happen if the player is pushing (or rather, being pushed.)
		// This either stops the player in their tracks or nudges them along.
		if (push)
		{
			pevToucher->velocity.x = pev->velocity.x;
			pevToucher->velocity.y = pev->velocity.y;
		}

		// BSVR start
		EmitPushSound(length);
		// BSVR end
	}
}

// BSVR start - extracted from CPushable::Move so VR controller code can play push sounds
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
// BSVR end

#if 0
void CPushable::StopSound()
{
	Vector dist = pev->oldorigin - pev->origin;
	if ( dist.Length() <= 0 )
		STOP_SOUND( ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound] );
}
#endif

bool CPushable::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if ((pev->spawnflags & SF_PUSH_BREAKABLE) != 0)
		return CBreakable::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);

	return true;
}

int CPushable::DamageDecal(int bitsDamageType)
{
	if (FBitSet(pev->spawnflags, SF_PUSH_BREAKABLE))
		return CBreakable::DamageDecal(bitsDamageType);

	return CBaseEntity::DamageDecal(bitsDamageType);
}
