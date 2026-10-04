/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
//
// util.cpp
//
// implementation of class-less helper functions
//

#include <cstdio>
#include <cstdlib>

#include "hud.h"
#include "cl_util.h"
#include <string.h>

// BSVR start
#include <unordered_map>
#include <string>
#include <memory>
// BSVR end

HSPRITE LoadSprite(const char* pszName)
{
	int i;
	char sz[256];

	if (ScreenWidth < 640)
		i = 320;
	else
		i = 640;

	sprintf(sz, pszName, i);

	return SPR_Load(sz);
}

// BSVR start
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
// BSVR end
