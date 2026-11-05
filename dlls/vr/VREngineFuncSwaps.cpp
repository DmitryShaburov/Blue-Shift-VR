// Replaces entries in this DLL's copy of the engine function table (g_engfuncs), called once from GiveFnptrsToDll.
// Only calls made by this DLL are affected, the engine's own code is not touched.

void VRSwapEngineCallbacks(); // VREngineCallbacks.cpp

void VRSwapEngineFuncs()
{
	VRSwapEngineCallbacks();
}
