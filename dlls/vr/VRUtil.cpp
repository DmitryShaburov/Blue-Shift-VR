// VR utility functions, moved from util.cpp

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "weapons.h"

#include <filesystem>
#include "vr/VRPhysicsHelper.h"

// Returns a path to the given file:
// a) in the current mod directory, if a mod is loaded and the file exists in the mod directory,
// b) or in the valve directory.
// If the file doesn't exist at all, the valve path is returned as well.
std::string UTIL_GetFilePath(const std::string& filename)
{
	char gameDir[1024] = {};
	g_engfuncs.pfnGetGameDir(gameDir);

	if (std::filesystem::exists(gameDir + filename))
	{
		return gameDir + filename;
	}
	else
	{
		return "valve" + filename;
	}
}

Vector& UTIL_AnglesMod(Vector& angles)
{
	angles.x = UTIL_AngleMod(angles.x);
	angles.y = UTIL_AngleMod(angles.y);
	angles.z = UTIL_AngleMod(angles.z);
	return angles;
}

bool UTIL_CheckClearSight(const Vector& pos1, const Vector& pos2, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t* pentIgnore)
{
	TraceResult tr;
	UTIL_TraceLine(pos1, pos2, igmon, ignoreGlass, pentIgnore, &tr);
	return tr.flFraction == 1.f;
}

// Taken from jyk's separating axis theorem code in https://www.gamedev.net/forums/topic/338987-aabb---line-segment-intersection-test/?tab=comments#comment-3209917 - Max Makes Mods, 2018-01-10
bool UTIL_TraceBBox(const Vector& vecStart, const Vector& vecEnd, const Vector& absmin, const Vector& absmax)
{
	const Vector vecHalfDir = (vecEnd - vecStart) * 0.5f;
	const Vector vecabsHalfDir(fabs(vecHalfDir.x), fabs(vecHalfDir.y), fabs(vecHalfDir.z));

	const Vector vecBBoxDir = (absmax - absmin) * 0.5f;

	const Vector vecBBoxCenter = (absmin + absmax) * 0.5f;
	const Vector vecStartInLocalSpace = vecStart - vecBBoxCenter;
	const Vector vecHalfEndInLocalSpace = vecStartInLocalSpace + vecHalfDir;

	if (fabs(vecHalfEndInLocalSpace[0]) > vecBBoxDir[0] + vecabsHalfDir[0])
		return false;
	if (fabs(vecHalfEndInLocalSpace[1]) > vecBBoxDir[1] + vecabsHalfDir[1])
		return false;
	if (fabs(vecHalfEndInLocalSpace[2]) > vecBBoxDir[2] + vecabsHalfDir[2])
		return false;

	if (fabs(vecHalfDir[1] * vecHalfEndInLocalSpace[2] - vecHalfDir[2] * vecHalfEndInLocalSpace[1]) > vecBBoxDir[1] * vecabsHalfDir[2] + vecBBoxDir[2] * vecabsHalfDir[1] + EPSILON)
		return false;
	if (fabs(vecHalfDir[2] * vecHalfEndInLocalSpace[0] - vecHalfDir[0] * vecHalfEndInLocalSpace[2]) > vecBBoxDir[2] * vecabsHalfDir[0] + vecBBoxDir[0] * vecabsHalfDir[2] + EPSILON)
		return false;
	if (fabs(vecHalfDir[0] * vecHalfEndInLocalSpace[1] - vecHalfDir[1] * vecHalfEndInLocalSpace[0]) > vecBBoxDir[0] * vecabsHalfDir[1] + vecBBoxDir[1] * vecabsHalfDir[0] + EPSILON)
		return false;

	return true;
}

bool UTIL_IsPointInEntity(CBaseEntity* pEntity, const Vector& p)
{
	if (!pEntity)
		return false;

	// No model or model is studio model
	if (0 == pEntity->pev->model || STRING(pEntity->pev->model)[0] == 0 || STRING(pEntity->pev->model)[0] != '*')
	{
		// TODO: Do proper hitbox check
		return UTIL_PointInsideBBox(p, pEntity->pev->origin + pEntity->pev->mins, pEntity->pev->origin + pEntity->pev->maxs);
	}

	int backupskin = pEntity->pev->skin;
	pEntity->pev->skin = CONTENTS_VR_TEMP_HACK;

	edict_t* pent = nullptr;
	int contents = UTIL_PointContents(p, true, &pent);

	pEntity->pev->skin = backupskin;

	return (contents == CONTENTS_VR_TEMP_HACK) || (pEntity->edict() == pent);
}

// Convenience method to check if a point is inside a bbox - Max Makes Mods, 2017-12-27
bool UTIL_PointInsideBBox(const Vector& vec, const Vector& absmin, const Vector& absmax)
{
	return absmin.x < vec.x && absmin.y < vec.y && absmin.z < vec.z && absmax.x > vec.x && absmax.y > vec.y && absmax.z > vec.z;
}

// From https://stackoverflow.com/a/3235902/9199167
bool GetIntersection(float fDst1, float fDst2, const Vector& vec1, const Vector& vec2, Vector& result)
{
	if ((fDst1 * fDst2) >= 0.0f)
		return false;
	if (fDst1 == fDst2)
		return false;
	result = vec1 + (vec2 - vec1) * (-fDst1 / (fDst2 - fDst1));
	return true;
}

bool InBox(const Vector& v, const Vector& absmin, const Vector& absmax, int axis)
{
	if (axis == 1 && v.z > absmin.z && v.z < absmax.z && v.y > absmin.y && v.y < absmax.y)
		return true;
	if (axis == 2 && v.z > absmin.z && v.z < absmax.z && v.x > absmin.x && v.x < absmax.x)
		return true;
	if (axis == 3 && v.x > absmin.x && v.x < absmax.x && v.y > absmin.y && v.y < absmax.y)
		return true;
	return false;
}

bool UTIL_GetLineIntersectionWithBBox(const Vector& vec1, const Vector& vec2, const Vector& absmin, const Vector& absmax, Vector& result)
{
	if (vec2.x < absmin.x && vec1.x < absmin.x)
		return false;
	if (vec2.x > absmax.x && vec1.x > absmax.x)
		return false;
	if (vec2.y < absmin.y && vec1.y < absmin.y)
		return false;
	if (vec2.y > absmax.y && vec1.y > absmax.y)
		return false;
	if (vec2.z < absmin.z && vec1.z < absmin.z)
		return false;
	if (vec2.z > absmax.z && vec1.z > absmax.z)
		return false;
	if (vec1.x > absmin.x && vec1.x < absmax.x &&
		vec1.y > absmin.y && vec1.y < absmax.y &&
		vec1.z > absmin.z && vec1.z < absmax.z)
	{
		result = vec1;
		return true;
	}

	if ((GetIntersection(vec1.x - absmin.x, vec2.x - absmin.x, vec1, vec2, result) && InBox(result, absmin, absmax, 1)) || (GetIntersection(vec1.y - absmin.y, vec2.y - absmin.y, vec1, vec2, result) && InBox(result, absmin, absmax, 2)) || (GetIntersection(vec1.z - absmin.z, vec2.z - absmin.z, vec1, vec2, result) && InBox(result, absmin, absmax, 3)) || (GetIntersection(vec1.x - absmax.x, vec2.x - absmax.x, vec1, vec2, result) && InBox(result, absmin, absmax, 1)) || (GetIntersection(vec1.y - absmax.y, vec2.y - absmax.y, vec1, vec2, result) && InBox(result, absmin, absmax, 2)) || (GetIntersection(vec1.z - absmax.z, vec2.z - absmax.z, vec1, vec2, result) && InBox(result, absmin, absmax, 3)))
	{
		return true;
	}

	return false;
}

// Convenience method to check if a point is inside a brush model (Algorithm could be more performant, but this is HL after all) - Max Makes Mods, 2017-08-26
bool UTIL_PointInsideBSPModel(const Vector& vec, const Vector& absmin, const Vector& absmax)
{
	bool inside = false;
	if (UTIL_PointInsideBBox(vec, absmin, absmax))
	{
		TraceResult tr;
		Vector startPos = absmin;
		Vector vecDirEpsilon = (vec - startPos).Normalize() * 0.01f;
		startPos = startPos - vecDirEpsilon;
		while (startPos.x < vec.x && startPos.y < vec.y && startPos.z < vec.z)
		{
			if (inside)
			{
				UTIL_TraceLine(vec, startPos, ignore_monsters, nullptr, &tr);
			}
			else
			{
				UTIL_TraceLine(startPos, vec, ignore_monsters, nullptr, &tr);
			}
			if (tr.flFraction < 1.f)
			{
				inside = !inside;
				startPos = tr.vecEndPos + vecDirEpsilon;
			}
			else
			{
				break;
			}
		}
	}
	return inside;
}

// Returns true if the given bboxes intersect - Max Makes Mods, 2018-02-11
bool UTIL_BBoxIntersectsBBox(const Vector& absmins1, const Vector& absmaxs1, const Vector& absmins2, const Vector& absmaxs2)
{
	if (absmaxs1.x < absmins2.x || absmins1.x > absmaxs2.x)
		return false;
	if (absmaxs1.y < absmins2.y || absmins1.y > absmaxs2.y)
		return false;
	if (absmaxs1.z < absmins2.z || absmins1.z > absmaxs2.z)
		return false;
	return true;
}

// Returns true if the point is inside the rotated bbox - Max Makes Mods, 2018-02-11
bool UTIL_PointInsideRotatedBBox(const Vector& bboxCenter, const Vector& bboxAngles, const Vector& bboxMins, const Vector& bboxMaxs, const Vector& checkVec)
{
	Vector rotatedLocalCheckVec = checkVec - bboxCenter;
	VRPhysicsHelper::Instance().RotateVector(rotatedLocalCheckVec, -bboxAngles, Vector(), true);
	return UTIL_PointInsideBBox(rotatedLocalCheckVec, bboxMins, bboxMaxs);
}

const Vector UTIL_WaterLevelPos(const Vector& start, const Vector& end)
{
	if (UTIL_PointContents(start) != CONTENTS_WATER)
	{
		return start;
	}

	if (UTIL_PointContents(end) == CONTENTS_WATER)
	{
		return end;
	}

	const Vector middle = (start + end) * 0.5f;
	const float dist = (end - start).Length();
	if (dist > 1.0f)
	{
		if (UTIL_PointContents(middle) == CONTENTS_WATER)
		{
			return UTIL_WaterLevelPos(middle, end);
		}
		else
		{
			return UTIL_WaterLevelPos(start, middle);
		}
	}
	else
	{
		return middle;
	}
}

// Convenience function to get the constants of a parabola function (in 2d space) from 3 points - 2017-12-25, Max Makes Mods
void UTIL_ParabolaFromPoints(const Vector2D& p1, const Vector2D& p2, const Vector2D& p3, Vector& parabola)
{
	// General function is: y = A*x*x + B*x + C

	// Resolve for C with first point
	// A*p1.x*p1.x + B*p1.x + C = p1.y
	// ==> C = p1.y - A*p1.x*p1.x - B*p1.x

	// Resolve for B with C and second point
	// A*p2.x*p2.x + B*p2.x + p1.y - A*p1.x*p1.x - B*p1.x = p2.y
	// ===> B = (p2.y - p1.y - A*(p2.x*p2.x - p1.x*p1.x)) / (p2.x - p1.x)

	// Resolve for A with B and C and third point
	// A*p3.x*p3.x + ((p2.y - p1.y - A*(p2.x*p2.x - p1.x*p1.x)) / (p2.x - p1.x))*p3.x + p1.y - A*p1.x*p1.x - ((p2.y - p1.y - A*(p2.x*p2.x - p1.x*p1.x)) / (p2.x - p1.x))*p1.x
	// ===> A = ((p2.y - p1.y)*(p1.x - p3.x) + (p3.y - p1.y)*(p2.x - p1.x)) / ((p1.x - p3.x)*(p2.x*p2.x - p1.x*p1.x) + (p2.x - p1.x)*(p3.x*p3.x - p1.x*p1.x))

	// Do the actual calculations:
	float A = ((p2.y - p1.y) * (p1.x - p3.x) + (p3.y - p1.y) * (p2.x - p1.x)) / ((p1.x - p3.x) * (p2.x * p2.x - p1.x * p1.x) + (p2.x - p1.x) * (p3.x * p3.x - p1.x * p1.x));
	float B = ((p2.y - p1.y) - A * (p2.x * p2.x - p1.x * p1.x)) / (p2.x - p1.x);
	float C = p1.y - A * p1.x * p1.x - B * p1.x;

	parabola.x = A;
	parabola.y = B;
	parabola.z = C;
}

// Moved from barney.cpp and made 3d (why tf was this 2d?) - Max Makes Mods, 2018-01-02
bool UTIL_IsFacing(const Vector& viewPos, const Vector& viewAngles, const Vector& reference)
{
	Vector referenceDir = (reference - viewPos);
	Vector viewDir;
	UTIL_MakeVectorsPrivate(viewAngles, viewDir, nullptr, nullptr);
	if (DotProduct(viewDir, referenceDir) > 0.96f) // +/- 15 degrees or so
	{
		return true;
	}
	return false;
}

bool UTIL_StartsWith(const char* s1, const char* s2)
{
	return strncmp(s1, s2, strlen(s2)) == 0;
}

// Checks if a trace would hit this entity (does not check if stuff is in the way, use UTIL_CheckClearSight for that), entity must be solid - Max Makes Mods, 2018-01-06
bool UTIL_CheckTraceIntersectsEntity(const Vector& pos1, const Vector& pos2, CBaseEntity* pCheck)
{
	// Check if line starts inside box
	if (UTIL_PointInsideBBox(pos1, pCheck->pev->absmin, pCheck->pev->absmax))
	{
		return true;
	}

	TraceResult tr;
	UTIL_TraceLine(pos1, pos2, dont_ignore_monsters, dont_ignore_glass, nullptr, &tr);
	if (tr.pHit == pCheck->edict() || UTIL_PointInsideBBox(tr.vecEndPos, pCheck->pev->absmin, pCheck->pev->absmax))
	{
		return true;
	}
	else if (tr.flFraction > 0.f && tr.flFraction < 1.f && (pos2 - tr.vecEndPos).Length() > 2.f)
	{
		return UTIL_CheckTraceIntersectsEntity(tr.vecEndPos + (pos2 - pos1).Normalize(), pos2, pCheck);
	}
	else
	{
		return false;
	}
}

#include <skill.h>

float UTIL_CalculateMeleeDamage(int iId, float speed)
{
	if (speed >= GetMeleeSwingSpeed())
	{
		float baseDamage = gSkillData.plrDmgCrowbar * speed / GetMeleeSwingSpeed();
		switch (iId)
		{
		case WEAPON_CROWBAR:
			return baseDamage;
		case WEAPON_HORNETGUN:
			return baseDamage * 0.05f;
		case WEAPON_NONE:
		case WEAPON_BAREHAND:
		case WEAPON_SNARK:
			return baseDamage * 0.1f;
		case WEAPON_HANDGRENADE:
		case WEAPON_TRIPMINE:
		case WEAPON_SATCHEL:
			return baseDamage * 0.2f;
		case WEAPON_GLOCK:
		case WEAPON_PYTHON:
			return baseDamage * 0.3f;
		case WEAPON_MP5:
		case WEAPON_SHOTGUN:
		case WEAPON_CROSSBOW:
			return baseDamage * 0.4f;
		case WEAPON_RPG:
		case WEAPON_GAUSS:
		case WEAPON_EGON:
			return baseDamage * 0.5f;
		default:
			return baseDamage * 0.1f;
		}
	}
	else
	{
		return 0;
	}
}

int UTIL_DamageTypeFromWeapon(int iId)
{
	switch (iId)
	{
	case WEAPON_CROWBAR:
		return DMG_CLUB;
	default:
		return DMG_CLUB | DMG_NEVERGIB;
	}
}
