#pragma once

class VRGameFunctions
{
public:
	static void OpenMenu();
	static void CloseMenu();
	static void PrintToConsole(const char* s);
	static float GetCVar(const char* cvar);
	static void SetCVar(const char* cvar, float value);
};
