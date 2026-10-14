
// Random implementation using std random classes for overriding engine's random functions
// For Half-Life: VR by Max Makes Mods - 2020-02-22

#ifdef _WIN32
#include "easyhook.h"
#endif

#include "VREngineInterceptor.h"
#include "VRRandom.h"

#include <vector>
#include <memory>
#include <string>
#include <sstream>

#include <sys/types.h>
#include <stdio.h>

#include "hud.h"

#define _WIN32_WINNT            0x500
#define _WIN32_IE_              _WIN32_IE_WIN2KSP4

#ifdef _WIN32
#include <windows.h>
#include <winnt.h>
#include <winternl.h>
#endif

namespace
{
    #ifdef _WIN32
    std::vector<std::shared_ptr<HOOK_TRACE_INFO>> hooks;

    bool HookEngineMethod(const char* name, void* originalmethod, void* hookmethod)
    {
        std::shared_ptr<HOOK_TRACE_INFO> hook = std::make_shared<HOOK_TRACE_INFO>(HOOK_TRACE_INFO{ nullptr });
        hooks.push_back(hook);

        NTSTATUS result = LhInstallHook(
            originalmethod,
            hookmethod,
            nullptr,
            hook.get());

        if (FAILED(result))
        {
            gEngfuncs.Con_DPrintf("Warning: Failed to install hook for engine's random method %s: %S\n", name, RtlGetLastErrorString());
            return false;
        }
        else
        {
            ULONG ACLEntries[1] = { 0 };
            LhSetInclusiveACL(ACLEntries, 1, hook.get());
            return true;
        }
    }
    #endif
}

bool VREngineInterceptor::HookEngineFunctions()
{
    #ifdef _WIN32
    return HookEngineMethod("RandomFloat", gEngfuncs.pfnRandomFloat, VRRandomFloat)
        && HookEngineMethod("RandomLong", gEngfuncs.pfnRandomLong, VRRandomLong)
        ;
    #else
    return true;
    #endif
}

