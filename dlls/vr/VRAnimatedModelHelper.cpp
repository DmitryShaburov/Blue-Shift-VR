#include "extdll.h"
#include "util.h"
#include "cbase.h"

extern const char* GetAnimatedWeaponModelName(const char* modelName);

int PRECACHE_MODEL2(const char* s)
{
	const char* anims = GetAnimatedWeaponModelName(s);
	if (anims != s)
		PRECACHE_MODEL3(const_cast<char*>(anims));
	return PRECACHE_MODEL3(s);
}

int PRECACHE_GENERIC2(const char* s)
{
	const char* anims = GetAnimatedWeaponModelName(s);
	if (anims != s)
		PRECACHE_GENERIC3(const_cast<char*>(anims));
	return PRECACHE_GENERIC3(s);
}

int PRECACHE_MODEL(const char* s)
{
	return PRECACHE_MODEL2(s);
}

int PRECACHE_GENERIC(const char* s)
{
	return PRECACHE_GENERIC2(s);
}
