# Half-Life: VR Mod - Blue Shift

Mod for Half-Life: Blue Shift based on Max Makes Mods [Half-Life: VR Mod](https://github.com/maxmakesmods/Half-Life-VR) and [Blue Shift Updated](https://github.com/twhl-community/halflife-bs-updated) SDK.

This is a free, non-commercial community project. It is not affiliated with, endorsed by, or supported by Valve Corporation or Gearbox Software. Playing it requires owning [Half-Life: Blue Shift](https://store.steampowered.com/app/130/HalfLife_Blue_Shift/) on Steam. Half-Life and Blue Shift are trademarks of Valve Corporation.

Third-party code and licenses are listed in [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES/README.md).

# Status

Beta. Most of the code is ported from Half-Life: VR Mod onto the latest SDK. For what's not ported - see the Differences section. Hand models are replaced. There are likely still bugs introduced by the migration.

# Differences from Half-Life: VR Mod

- Runs on the Half-Life 25th Anniversary Update engine and the latest community SDK instead of the legacy SDK.
- Original content only, no new NPC models or voice lines.
- Upscaled textures are not shipped with this mod.
- FMOD sound engine is not ported.
- Speech recognition is not ported.
- Steam API and achievements are not ported.
- The VR configuration tool is not ported.
- Configuration changes are:
  - Immersive ladders are disabled by default.
  - Default movement direction is HMD.
  - Autocrouch is disabled by default.

# Known issue in this mod

- Teleportation in some Xen places instantly kills you.
- Hand models on weapons are mess.

# Known issues inherited from Half-Life: VR Mod

- No Linux support.
- Immersive ladders are hard to climb and properly dismount.
- The mod may crash on launch, during level transitions or at random, and may corrupt saves.
- No manual reload or two-hand weapon holding.
- Any other known issues of the original mod.

# Installation

- Make sure you have [Half-Life: Blue Shift](https://store.steampowered.com/app/130/HalfLife_Blue_Shift/) and [Half-Life: VR Mod](https://store.steampowered.com/app/1908720/HalfLife_VR_Mod/) installed on Steam.
- Download latest version of installer `bsvr-install.exe` from this repository [Releases](https://github.com/DmitryShaburov/Blue-Shift-VR/releases) page.
- Launch the installer. It will look for Steam installations and install the mod.
- Restart the Steam client (via tray icon right click → Exit Steam or Steam → Exit top menu).
- Mod will appear in your Steam library as Half-Life: VR Mod - Blue Shift.

# Development

For building and installing follow the upstream [Blue Shift Updated](https://github.com/twhl-community/halflife-bs-updated#readme) documentation.
