# hl1_source_sdk (subset)

VGUI2 for the client, used by the VR Options dialog (`cl_dll/vr/VRClientVGUI.cpp`).

- Source: https://github.com/tmp64/hl1_source_sdk, commit `11f26656677c881c7c630d03957900e7701da62e` (2025-06-18),
  Source SDK 2013 code ported to GoldSrc and updated for the 25th anniversary engine (build 9891).
- License: Source 1 SDK License (`LICENSE`, `thirdpartylegalnotices.txt`), also in `THIRD_PARTY_LICENSES`.
- Only the files the dialog needs are here, unchanged: the tier1, tier2 and vgui_controls sources the client links
  (found with the linker map) and the headers they include. To use more controls, copy their files from the same commit.
- `lib/public`: Valve's tier0 and vstdlib to link against. The game ships the real ones next to hl.exe / hl_linux.
- `vgui_controls_forced_includes.h` is ours: the headers the SDK's CMake build precompiles for vgui_controls,
  which its sources rely on. The build force-includes it for those files.

Build settings (`linux/Makefile.hl_cdll`, `projects/vs2019/hl_cdll.vcxproj`): the SDK files and `VRClientVGUI.cpp` get the
SDK's include paths and defines (`NO_STEAM`, `NO_MALLOC_OVERRIDE`, `MATHLIB_HEADER_ONLY`, ...) instead of the game's, because
both have headers with the same names (`interface.h`, `FileSystem.h`). tier1's `interface.cpp` is left out, the client's
`public/interface.cpp` provides `CreateInterface`.
