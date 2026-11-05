// Cached engine cvar/model lookups and the changelevel hook, moved from util.cpp

#include "extdll.h"
#include "util.h"
#include "UserMessages.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// The CVAR_GET_* functions are incredibly slow. (O(n))
// They get called repeatedly during a single frame for the same cvars, in a worst case scenario this can lead to O(n�).
// This cache dramatically improves performance.
// VRClearCvarCache() needs to be called once per frame, ideally at the beginning.
// - Max Makes Mods, 2020-03-16
namespace
{
static std::unordered_map<std::string, float> cvarfloatcache;
static std::unordered_map<std::string, std::unique_ptr<std::string>> cvarstringcache;
static std::unordered_map<std::string, std::vector<unsigned char>> modelpointercache;
}

void VRClearCvarCache()
{
	cvarfloatcache.clear();
	cvarstringcache.clear();
	modelpointercache.clear();
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
		float result = g_engfuncs.pfnCVarGetFloat(x);
		cvarfloatcache[x] = result;
		return result;
	}
}

const char* CVAR_GET_STRING(const char* x)
{
	auto it = cvarstringcache.find(x);
	if (it != cvarstringcache.end() && it->second)
	{
		return it->second->data();
	}
	else
	{
		const char* result = g_engfuncs.pfnCVarGetString(x);
		cvarstringcache[x] = std::make_unique<std::string>(result);
		return result;
	}
}

void* GET_MODEL_PTR(edict_t* pent)
{
	constexpr const int STUDIO_HEADER_LENGTH_OFFSET = 72; // see studiohdr_t in studio.h

	std::string modelName = STRING(pent->v.model);

	auto it = modelpointercache.find(modelName);
	if (it != modelpointercache.end())
	{
		if (it->second.empty())
			return nullptr;
		else
			return it->second.data();
	}
	else
	{
		void* model = nullptr;
		if (modelName.size() > 4 && modelName[0] != '*' && modelName.find(".mdl") == modelName.size() - 4)
		{
			model = (*g_engfuncs.pfnGetModelPtr)(pent);
		}
#ifdef _DEBUG
		else
		{
			ALERT(at_console, "Warning: Tried to get model pointer to non-studio model (%s)\n", modelName.c_str());
		}
#endif
		if (model != nullptr)
		{
			// models live only temporarily (not even surviving during a single frame), so we create a copy here
			auto& modelcopy = modelpointercache[modelName];
			unsigned char* modeldata = static_cast<unsigned char*>(model);
			int length = *reinterpret_cast<int*>(modeldata + STUDIO_HEADER_LENGTH_OFFSET);
			modelcopy.assign(modeldata, modeldata + length);
		}
		return model;
	}
}

// From enginecallback.h, intercepts changelevel calls
void CHANGE_LEVEL(char* s1, char* s2)
{
	MESSAGE_BEGIN(MSG_ALL, gmsgVRLevelChange);
	MESSAGE_END();

	g_engfuncs.pfnChangeLevel(s1, s2);
}
