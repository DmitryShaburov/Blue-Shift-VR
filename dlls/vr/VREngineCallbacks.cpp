// Cached engine cvar/model lookups and the changelevel hook, swapped into g_engfuncs by VRHookEngineFuncs

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

// The engine's own functions, called on a cache miss
float (*s_engineCVarGetFloat)(const char* szVarName);
const char* (*s_engineCVarGetString)(const char* szVarName);
void* (*s_engineGetModelPtr)(edict_t* pEdict);
void (*s_engineChangeLevel)(const char* s1, const char* s2);

float VRCVarGetFloat(const char* x)
{
	auto it = cvarfloatcache.find(x);
	if (it != cvarfloatcache.end())
	{
		return it->second;
	}
	else
	{
		float result = s_engineCVarGetFloat(x);
		cvarfloatcache[x] = result;
		return result;
	}
}

const char* VRCVarGetString(const char* x)
{
	auto it = cvarstringcache.find(x);
	if (it != cvarstringcache.end() && it->second)
	{
		return it->second->data();
	}
	else
	{
		const char* result = s_engineCVarGetString(x);
		cvarstringcache[x] = std::make_unique<std::string>(result);
		return result;
	}
}

void* VRGetModelPtr(edict_t* pent)
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
			model = s_engineGetModelPtr(pent);
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

// Tells the client about the level change before passing it on
void VRChangeLevel(const char* s1, const char* s2)
{
	MESSAGE_BEGIN(MSG_ALL, gmsgVRLevelChange);
	MESSAGE_END();

	s_engineChangeLevel(s1, s2);
}
}

void VRClearCvarCache()
{
	cvarfloatcache.clear();
	cvarstringcache.clear();
	modelpointercache.clear();
}

// Called from GiveFnptrsToDll: every CVAR_GET_FLOAT, CVAR_GET_STRING, GET_MODEL_PTR and CHANGE_LEVEL in the DLL goes through g_engfuncs
void VRHookEngineFuncs()
{
	s_engineCVarGetFloat = g_engfuncs.pfnCVarGetFloat;
	s_engineCVarGetString = g_engfuncs.pfnCVarGetString;
	s_engineGetModelPtr = g_engfuncs.pfnGetModelPtr;
	s_engineChangeLevel = g_engfuncs.pfnChangeLevel;

	g_engfuncs.pfnCVarGetFloat = VRCVarGetFloat;
	g_engfuncs.pfnCVarGetString = VRCVarGetString;
	g_engfuncs.pfnGetModelPtr = VRGetModelPtr;
	g_engfuncs.pfnChangeLevel = VRChangeLevel;
}
