// Per-frame cache for the client cvar lookups, swapped into gEngfuncs by VRSwapCvarFuncs

#include "hud.h"
#include "cl_util.h"

#include <unordered_map>
#include <string>
#include <memory>

// The CVAR_GET_* functions are incredibly slow. (O(n))
// They get called repeatedly during a single frame for the same cvars, in a worst case scenario this can lead to O(n�).
// This cache dramatically improves performance.
// VRClearCvarCache() needs to be called once per frame, ideally at the beginning.
// - Max Makes Mods, 2020-03-16
namespace
{
static std::unordered_map<std::string, float> cvarfloatcache;
static std::unordered_map<std::string, std::unique_ptr<std::string>> cvarstringcache;

// The engine's own functions, called on a cache miss
float (*s_engineGetCvarFloat)(const char* szName);
const char* (*s_engineGetCvarString)(const char* szName);

float VRGetCvarFloat(const char* x)
{
	auto it = cvarfloatcache.find(x);
	if (it != cvarfloatcache.end())
	{
		return it->second;
	}
	else
	{
		float result = s_engineGetCvarFloat(x);
		cvarfloatcache[x] = result;
		return result;
	}
}

const char* VRGetCvarString(const char* x)
{
	auto it = cvarstringcache.find(x);
	if (it != cvarstringcache.end())
	{
		return it->second->data();
	}
	else
	{
		const char* result = s_engineGetCvarString(x);
		cvarstringcache[x] = std::make_unique<std::string>(result);
		return result;
	}
}
}

void VRClearCvarCache()
{
	cvarfloatcache.clear();
	cvarstringcache.clear();
}

// For code that reads a cvar right after setting it (VR menu), the cache would return the old value until the next frame
float VRGetCvarFloatUncached(const char* x)
{
	return s_engineGetCvarFloat(x);
}

// Every CVAR_GET_FLOAT and CVAR_GET_STRING in the client goes through gEngfuncs
void VRSwapCvarFuncs()
{
	s_engineGetCvarFloat = gEngfuncs.pfnGetCvarFloat;
	s_engineGetCvarString = gEngfuncs.pfnGetCvarString;

	gEngfuncs.pfnGetCvarFloat = VRGetCvarFloat;
	gEngfuncs.pfnGetCvarString = VRGetCvarString;
}
