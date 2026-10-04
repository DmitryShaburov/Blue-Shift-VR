# bsvr-install

Interactive single-file Windows installer for Half-Life: VR Mod - Blue Shift.

It finds Steam through the registry, suggests the Half-Life and Half-Life: VR Mod folders from their
Steam app manifests, warns when Half-Life/Blue Shift/VR Mod is missing because Steam only lists mods under it,
then creates `<Half-Life>\bsvr\` with:

- the embedded payload: `liblist.gam`, `delta.lst`, the `game.*` icons, `dlls\hl.dll`,
  `cl_dlls\client.dll`, `maps\ba_yard4a.ent`, licenses;
- `openvr_api.dll` and `EasyHook32.dll` next to `hl.exe` in the Half-Life folder, because `client.dll`
  imports them implicitly and Windows does not search `cl_dlls\` for imports;
- VR runtime assets copied from Half-Life: VR Mod (see the tables in `Assets.cs`, derived from `Docs/VR_ASSETS.md`);
- Blue Shift's maps, rewritten from the Blue Shift BSP lump order to the standard one (see `Maps.cs`),
  with entities replaced from any `maps\*.ent` in the payload.

## Payload

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
