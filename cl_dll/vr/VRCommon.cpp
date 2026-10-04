
#include <filesystem>

#include "vr/VRShared.h"

#include "cl_dll.h"

#include "VRCommon.h"


std::filesystem::path GetRelPathFor(const std::string& file)
{
	if (std::filesystem::exists(gEngfuncs.pfnGetGameDirectory() + file))
	{
		return gEngfuncs.pfnGetGameDirectory() + file;
	}
	else
	{
		return "valve" + file;
	}
}

std::filesystem::path GetPathFor(const std::string& file)
{
	std::filesystem::path relativePath = GetRelPathFor(file);
	return std::filesystem::absolute(relativePath);
}
