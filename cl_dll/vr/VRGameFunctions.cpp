
#include "cl_dll.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cctype>
#include <string>

#include "VRGameFunctions.h"
#include "filesystem_utils.h"

// hides the menu until the game is running, see VRRenderer.cpp
extern void VRNotifyGameLoading();

extern void COM_FileBase(const char* in, char* out);

void VRGameFunctions::PrintToConsole(const char* s)
{
	gEngfuncs.Con_DPrintf(s);
}

namespace
{
	constexpr const char* MenuMusicTrack = "media/Prospero01.mp3";
	bool menuMusicPlaying = false;
	bool menuMusicDisabled = false;  // set once a game has started, the music does not come back
}

void VRGameFunctions::StartMenuMusic()
{
	if (menuMusicPlaying || menuMusicDisabled)
		return;

	// the engine looks the track up itself (mod folder, then the fallback folders) and prints an error if it can't find it
	gEngfuncs.Con_Printf("Menu music: mp3 loop %s\n", MenuMusicTrack);
	gEngfuncs.pfnClientCmd((std::string("mp3 loop ") + MenuMusicTrack).c_str());
	menuMusicPlaying = true;
}

void VRGameFunctions::StopMenuMusic()
{
	menuMusicDisabled = true;
	if (menuMusicPlaying)
	{
		gEngfuncs.pfnClientCmd("mp3 stop");
		menuMusicPlaying = false;
	}
}

int VRGameFunctions::GetModelsMode()
{
	// _sethdmodels has no way to read its state back. With HD models on, the engine mounts the "_hd" game folders
	// (e.g. bshift_hd) ahead of the regular ones, so a model that exists in both resolves to the _hd folder.
	if (g_pFileSystem == nullptr)
		return 0;

	char path[1024] = {};
	g_pFileSystem->GetLocalPath("models/barney.mdl", path, sizeof(path));

	std::string lowerPath{ path };
	std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return lowerPath.find("_hd") != std::string::npos ? 1 : 0;
}

void VRGameFunctions::SetModelsMode(int models)
{
	if ((models != 0) == (GetModelsMode() != 0))
		return;

	// The engine crashes on the next save or load unless it is restarted right after the switch
	gEngfuncs.pfnClientCmd(models ? "_sethdmodels 1;_restart" : "_sethdmodels 0;_restart");
}

int VRGameFunctions::GetTexturesMode()
{
	return gEngfuncs.pfnGetCvarFloat("vr_classic_mode") != 0.f ? 0 : 1;
}

void VRGameFunctions::SetTexturesMode(int textures)
{
	switch (textures)
	{
	case 0:
		// classic textures
		gEngfuncs.Cvar_SetValue("vr_classic_mode", 1);
		gEngfuncs.pfnClientCmd("gl_texturemode GL_NEAREST");
		gEngfuncs.pfnClientCmd("vr_texturemode GL_NEAREST");
		break;
	case 1:
		// smooth textures
		gEngfuncs.Cvar_SetValue("vr_classic_mode", 0);
		gEngfuncs.pfnClientCmd("gl_texturemode GL_LINEAR_MIPMAP_LINEAR");
		gEngfuncs.pfnClientCmd("vr_texturemode GL_LINEAR_MIPMAP_LINEAR");
		break;
	}
}

void VRGameFunctions::StartNewGame()
{
	VRNotifyGameLoading();
	gEngfuncs.pfnClientCmd("map ba_tram1");
}

void VRGameFunctions::StartHazardCourse()
{
	VRNotifyGameLoading();
	gEngfuncs.pfnClientCmd("map ba_hazard1");
}

void VRGameFunctions::OpenMenu()
{
	gEngfuncs.pfnClientCmd("toggleconsole");
}

void VRGameFunctions::CloseMenu()
{
	gEngfuncs.pfnClientCmd("escape");
}

std::vector<VRGameFunctions::SaveFile> VRGameFunctions::GetSaveFiles()
{
	std::vector<SaveFile> saves;
	if (g_pFileSystem == nullptr)
		return saves;

	FileFindHandle_t handle;
	// GAMECONFIG is the mod's own (writable) directory only, so saves of the fallback game ("valve") don't show up
	const char* file = g_pFileSystem->FindFirst("SAVE/*.sav", &handle, "GAMECONFIG");
	if (file == nullptr)
		return saves;

	for (; file != nullptr; file = g_pFileSystem->FindNext(handle))
	{
		if (g_pFileSystem->FindIsDirectory(handle))
			continue;

		std::string name = file;
		name.resize(name.size() - 4);  // ".sav"

		// the name ends up in a console command
		if (name.empty() || name.find_first_of("\";\r\n") != std::string::npos)
			continue;

		saves.push_back({ name, (long long)FileSystem_GetFileTime((std::string("SAVE/") + file).c_str()) });
	}
	g_pFileSystem->FindClose(handle);

	std::sort(saves.begin(), saves.end(), [](const SaveFile& a, const SaveFile& b) { return a.time > b.time; });
	return saves;
}

void VRGameFunctions::LoadGame(const std::string& name)
{
	VRNotifyGameLoading();
	std::string cmd = "load \"" + name + "\"";
	gEngfuncs.pfnClientCmd(cmd.c_str());
}

void VRGameFunctions::SaveGame()
{
	char mapName[256] = { 0 };
	COM_FileBase(gEngfuncs.pfnGetLevelName(), mapName);

	// seconds are part of the name so saves made in quick succession don't overwrite each other
	std::time_t now = std::time(nullptr);
	std::tm localTime{};
#ifdef _WIN32
	localtime_s(&localTime, &now);
#else
	localtime_r(&now, &localTime);
#endif
	char timeStr[32];
	std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H-%M-%S", &localTime);

	std::string cmd = std::string("save \"") + mapName + " - " + timeStr + "\"";
	gEngfuncs.pfnClientCmd(cmd.c_str());
}

bool VRGameFunctions::IsGameLoaded()
{
	const char* levelName = gEngfuncs.pfnGetLevelName();
	return levelName != nullptr && levelName[0] != '\0' && gEngfuncs.GetMaxClients() > 0;
}

void VRGameFunctions::QuitGame()
{
	// BSVR start
	// std::exit(0); - original
	// Let the engine shut down in order instead of killing the process mid-frame.
	gEngfuncs.pfnClientCmd("quit");
	// BSVR end
}

float VRGameFunctions::GetCVar(const char* cvar)
{
	return gEngfuncs.pfnGetCvarFloat(cvar);
}

void VRGameFunctions::SetCVar(const char* cvar, float value)
{
	gEngfuncs.Cvar_SetValue(cvar, value);
}
