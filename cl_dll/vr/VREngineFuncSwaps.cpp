// Replaces entries in this DLL's copy of the engine function table (gEngfuncs), called once from Initialize.
// Only calls made by this DLL are affected, the engine's own code is not touched (unlike the code patches in VREngineInterceptor.cpp).

void VRSwapCvarFuncs();		  // VRCvarCache.cpp
void VRSwapHUDSpriteFuncs();  // VRHUDRenderer.cpp
void VRSwapWeaponAnimFuncs(); // VRWeaponAnimation.cpp

void VRSwapEngineFuncs()
{
	VRSwapCvarFuncs();
	VRSwapHUDSpriteFuncs();
	VRSwapWeaponAnimFuncs();
}
