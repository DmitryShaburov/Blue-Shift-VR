#pragma once

class VREngineInterceptor
{
public:
	static bool HookEngineFunctions();
};

#ifndef _WIN32
// Linux: overwrites the start of an engine function with a jump to replacement, so every caller ends up there.
// The original is never called again, so replacement must do the whole job.
bool VRPatchEngineFunction(const char* name, void* function, void* replacement);
// Puts the original bytes back (called on shutdown, before client.so is unloaded)
void VRUnpatchEngineFunctions();
#endif
