#pragma once

#include <string>
#include <vector>

class VRGameFunctions
{
public:
	struct SaveFile
	{
		std::string name;  // without the .sav extension, this is what the load command takes
		long long time;    // last modification time
	};

	// Lists the saves in the SAVE directory, newest first
	static std::vector<SaveFile> GetSaveFiles();
	static void LoadGame(const std::string& name);
	// Models: 0 (SD) or 1 (HD)
	static int GetModelsMode();
	static void SetModelsMode(int models);
	// Textures: 0 (Classic) or 1 (Smooth)
	static int GetTexturesMode();
	static void SetTexturesMode(int textures);

	static void StartNewGame();
	static void StartHazardCourse();
	static void OpenMenu();
	static void CloseMenu();
	// Saves as "<map> - YYYY-MM-DD HH-MM-SS"
	static void SaveGame();
	// A map is loaded (the player may be dead or at an intermission, then saving is not possible)
	static bool IsGameLoaded();
	static void QuitGame();
	static void PrintToConsole(const char* s);
	// Menu music: starts once when the menu is first shown, and is stopped for the rest of the session when a game starts
	static void StartMenuMusic();
	static void StopMenuMusic();
	static float GetCVar(const char* cvar);
	static void SetCVar(const char* cvar, float value);
};
