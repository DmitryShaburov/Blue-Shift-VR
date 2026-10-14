# bsvr-install

Interactive single-file Windows installer for Half-Life: VR Mod - Blue Shift.

It finds Steam through the registry, suggests the Half-Life and Half-Life: VR Mod folders from their
Steam app manifests, warns when Half-Life/Blue Shift/VR Mod is missing because Steam only lists mods under it,
then creates `<Half-Life>\bsvr\` and `<Half-Life>\bsvr_hd\` with:

- the embedded payload: `liblist.gam`, `delta.lst`, the `game.*` icons, `dlls\hl.dll`,
  `cl_dlls\client.dll`, `maps\ba_yard4a.ent`, the SD weapon view models in `models\` and `models\animov\`, licenses;
- the HD weapon view models in `bsvr_hd\models\` and `bsvr_hd\models\animov\`. The engine puts `bsvr_hd` ahead of
  `bsvr` when HD models are on (**Options > Graphics > Models**, which runs `_sethdmodels` and restarts the game);
- `openvr_api.dll` and `EasyHook32.dll` next to `hl.exe` in the Half-Life folder, because `client.dll`
  imports them implicitly and Windows does not search `cl_dlls\` for imports;
- VR runtime assets copied from Half-Life: VR Mod (see the tables in `Assets.cs`, derived from `Docs/VR_ASSETS.md`);
- Blue Shift's maps, rewritten from the Blue Shift BSP lump order to the standard one (see `Maps.cs`),
  with entities replaced from any `maps\*.ent` in the payload.

## Payload

`payload/` is the `bsvr` folder and `payload_hd/` the `bsvr_hd` folder (embedded as `halflife/bsvr_hd/...`, see the csproj).
The SD set has no `v_gauss.mdl` (SD gauss is not converted), so `payload/models` and `payload/models/animov` keep the HD gauss
for SD mode. Blue Shift's own NPC and world models are not copied: the engine finds them in `bshift` and, with HD on,
`bshift_hd`. Both animov sets must be complete, because the animov twin of every weapon is precached unconditionally.

`payload/liblist.gam`, the icons and `maps/ba_yard4a.ent` are tracked; the last four come from the
Blue Shift Updated release. `skill.cfg`, `settings.scr` and `user.scr` are deliberately not shipped:
the engine finds retail Blue Shift's through `fallback_dir`, which keeps Blue Shift's own skill values. `payload/dlls/hl.dll` and `payload/cl_dlls/client.dll` are not; the Visual Studio post-build step of
`hldll` and `hl_cdll` copies them there, so publish after building the configuration you want to ship.
copy them from the mod build before publishing. `delta.lst`, `LICENSE`, `THIRD_PARTY_LICENSES`,
`lib/public/openvr_api.dll` and `cl_dll/EasyHook/bin/EasyHook32.dll` are embedded straight from the
repository (see the csproj).

## Build

```
dotnet publish -c Release
```
