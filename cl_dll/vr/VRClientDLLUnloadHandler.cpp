
#ifdef _WIN32

#include "EasyHook/include/easyhook.h"

#include <Windows.h>
#include <algorithm>

namespace
{
	bool g_wasthreadattached = false;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_DETACH)
	{
		// We need to uninstall all EasyHook hooks here.
		// EasyHook32.dll calls LhUninstallAllHooks when it is unloaded,
		// but hooks might be invalid by then as DLLs we hooked might be unloaded already,
		// which causes a crash when EasyHook calls IsBadReadPtr on every hook.
		// (See also https://devblogs.microsoft.com/oldnewthing/20060927-07/?p=29563)
		// BSVR: The hooks are normally already removed by VRHelper::Shutdown() from HUD_Shutdown, so this is
		// only a safety net. Upstream also called std::exit(0) here to kill the process before the engine
		// finished shutting down; on the 25th anniversary engine that trips tier0 and filesystem asserts
		// (worker threads terminated, mod still mounted) and the game appears to hang on exit.
		if (g_wasthreadattached)
		{
			LhUninstallAllHooks();
		}
	}
	if (fdwReason == DLL_THREAD_ATTACH)
	{
		g_wasthreadattached = true;
	}
	return TRUE;
}

#endif
