
// Random implementation using std random classes for overriding engine's random functions
// For Half-Life: VR by Max Makes Mods - 2020-02-22

#ifdef _WIN32
#include "easyhook.h"
#endif

#include "VREngineInterceptor.h"
#include "VRRandom.h"

#include <vector>
#include <memory>

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
	std::shared_ptr<HOOK_TRACE_INFO> hook = std::make_shared<HOOK_TRACE_INFO>(HOOK_TRACE_INFO{nullptr});
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
		ULONG ACLEntries[1] = {0};
		LhSetInclusiveACL(ACLEntries, 1, hook.get());
		return true;
	}
}
#endif
}

#ifdef _WIN32
bool VREngineInterceptor::HookEngineFunctions()
{
	return HookEngineMethod("RandomFloat", gEngfuncs.pfnRandomFloat, VRRandomFloat) && HookEngineMethod("RandomLong", gEngfuncs.pfnRandomLong, VRRandomLong);
}

#else // Linux

#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>

namespace
{
constexpr size_t JumpSize = 5; // jmp rel32

struct EnginePatch
{
	unsigned char* address;
	unsigned char original[JumpSize];
};
std::vector<EnginePatch> enginePatches;

// Code pages are read-only, so they are made writable just for the copy
bool WriteCode(unsigned char* address, const unsigned char* bytes)
{
	const uintptr_t pageSize = sysconf(_SC_PAGESIZE);
	const uintptr_t start = reinterpret_cast<uintptr_t>(address) & ~(pageSize - 1);
	const size_t length = reinterpret_cast<uintptr_t>(address) + JumpSize - start;
	if (mprotect(reinterpret_cast<void*>(start), length, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
		return false;
	std::memcpy(address, bytes, JumpSize);
	mprotect(reinterpret_cast<void*>(start), length, PROT_READ | PROT_EXEC);
	return true;
}
}

bool VRPatchEngineFunction(const char* name, void* function, void* replacement)
{
	auto* address = static_cast<unsigned char*>(function);
	if (address == nullptr)
	{
		gEngfuncs.Con_DPrintf("Warning: Failed to patch engine function %s: no address\n", name);
		return false;
	}

	EnginePatch patch{address};
	std::memcpy(patch.original, address, JumpSize);

	const int32_t distance = static_cast<int32_t>(reinterpret_cast<uintptr_t>(replacement) - (reinterpret_cast<uintptr_t>(address) + JumpSize));
	unsigned char jump[JumpSize] = {0xE9};
	std::memcpy(jump + 1, &distance, sizeof(distance));
	if (!WriteCode(address, jump))
	{
		gEngfuncs.Con_DPrintf("Warning: Failed to patch engine function %s: mprotect failed\n", name);
		return false;
	}
	enginePatches.push_back(patch);

	Dl_info info{};
	if (0 != dladdr(function, &info) && info.dli_fname)
		gEngfuncs.Con_DPrintf("Patched engine function %s at %s+0x%x\n", name, info.dli_fname, unsigned(reinterpret_cast<uintptr_t>(function) - reinterpret_cast<uintptr_t>(info.dli_fbase)));
	return true;
}

void VRUnpatchEngineFunctions()
{
	for (const auto& patch : enginePatches)
		WriteCode(patch.address, patch.original);
	enginePatches.clear();
}

bool VREngineInterceptor::HookEngineFunctions()
{
	return VRPatchEngineFunction("RandomFloat", reinterpret_cast<void*>(gEngfuncs.pfnRandomFloat), reinterpret_cast<void*>(VRRandomFloat)) && VRPatchEngineFunction("RandomLong", reinterpret_cast<void*>(gEngfuncs.pfnRandomLong), reinterpret_cast<void*>(VRRandomLong));
}
#endif
