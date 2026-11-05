// Per-frame cache for the client CVAR_GET_* lookups, moved from util.cpp

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
}
void VRClearCvarCache()
{
	cvarfloatcache.clear();
	cvarstringcache.clear();
}

float CVAR_GET_FLOAT(const char* x)
{
	auto it = cvarfloatcache.find(x);
	if (it != cvarfloatcache.end())
	{
		return it->second;
	}
	else
	{
		float result = gEngfuncs.pfnGetCvarFloat(x);
		cvarfloatcache[x] = result;
		return result;
	}
}

const char* CVAR_GET_STRING(const char* x)
{
	auto it = cvarstringcache.find(x);
	if (it != cvarstringcache.end())
	{
		return it->second->data();
	}
	else
	{
		const char* result = gEngfuncs.pfnGetCvarString(x);
		cvarstringcache[x] = std::make_unique<std::string>(result);
		return result;
	}
}
