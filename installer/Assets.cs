using System.Reflection;

namespace BsvrInstaller;

// Everything that goes into the bsvr mod folder: the embedded payload, VR assets copied from the Steam
// install of Half-Life: VR (where the repository's art/ and game/ folders land under valve/), and the
// converted Blue Shift maps.
//
// A source ending in "/" copies a whole folder tree. A "*" copies matching files from one folder.
// Targets are folders relative to the mod folder; a target not ending in "/" is a file path, which
// renames a single source file.
static class Assets
{
    static readonly (string From, string To)[] FromHalfLifeVr =
    [
        ("valve/actions/",                   "actions/"),          // SteamVR input manifest and bindings
        ("valve/textures/hud/",              "textures/hud/"),     // HUD sprite sheets as PNG
        ("valve/textures/background.png",    "textures/"),
        ("valve/textures/skybox/",           "textures/skybox/"),  // VR sky for maps whose sky name matches
        ("valve/sprites/black.spr",          "sprites/"),
        ("valve/sound/plats/train_use2.wav", "sound/plats/"),
    ];

    // Files earlier versions installed that are no longer used (the old VR menu), removed on update
    static readonly string[] Obsolete =
    [
        "fonts/Roboto-Regular.ttf",
        "textures/null.png",
        "THIRD_PARTY_LICENSES/Nuklear.txt",
    ];

    public static void Install(string halfLifeDir, string halfLifeVrDir, string modDir)
    {
        Directory.CreateDirectory(modDir);
        RemoveObsolete(modDir);

        Console.WriteLine("Mod files");
        ExtractPayload(modDir, halfLifeDir);

        Console.WriteLine("VR assets from Half-Life: VR");
        CopyAll(halfLifeVrDir, FromHalfLifeVr, modDir);

        Console.WriteLine("Maps from Blue Shift");
        Maps.Install(halfLifeDir, modDir);

        Console.WriteLine("Options that do nothing in VR");
        OptionsLocks.Install(halfLifeDir, modDir);

        Console.WriteLine("NPC lines for VR");
        Sentences.Install(halfLifeDir, modDir);

        Console.WriteLine("Saved games");
        RenameSaves(modDir);
    }

    // The old game menu allowed spaces in save names and the new one cannot load those saves, so the spaces
    // are removed. The save and its thumbnail share a name, so every file in SAVE\ is renamed the same way.
    static void RenameSaves(string modDir)
    {
        var saveDir = Path.Combine(modDir, "SAVE");
        if (!Directory.Exists(saveDir))
        {
            return;
        }

        foreach (var file in Directory.GetFiles(saveDir))
        {
            var name = Path.GetFileName(file);
            if (!name.Contains(' '))
            {
                continue;
            }

            var newName = name.Replace(" ", "");
            var target = Path.Combine(saveDir, newName);
            if (File.Exists(target))
            {
                Console.WriteLine($"  warning: {name} not renamed, {newName} already exists");
                continue;
            }
            File.Move(file, target);
            Console.WriteLine($"  {name} -> {newName}");
        }
    }

    // Embedded resource "payload/dlls/hl.dll" goes to "<mod>/dlls/hl.dll", and so on.
    // "halflife/openvr_api.dll" goes next to hl.exe, where Windows resolves client.dll's imports; likewise
    // "halflife/libopenvr_api.so" goes next to hl.sh, which puts that folder on LD_LIBRARY_PATH for client.so.
    // Each set is installed only for the game it is for: native Linux Half-Life has no hl.exe, a Windows one
    // (also under Proton) has no hl_linux.
    // "halflife/bsvr_hd/models/v_357.mdl" goes to the HD game folder next to the mod, "<Half-Life>/bsvr_hd/models/v_357.mdl".
    static void ExtractPayload(string modDir, string halfLifeDir)
    {
        var assembly = Assembly.GetExecutingAssembly();
        var names = assembly.GetManifestResourceNames();
        if (!names.Any(n => n.EndsWith("/hl.dll") || n.EndsWith("/hl.so")) ||
            !names.Any(n => n.EndsWith("/client.dll") || n.EndsWith("/client.so")))
        {
            Console.WriteLine("  warning: game DLLs are not embedded in this build, the mod will not start");
        }

        foreach (var name in names)
        {
            var normalized = name.Replace('\\', '/');
            var isHalfLifeRoot = normalized.StartsWith("halflife/");
            var relative = normalized[(isHalfLifeRoot ? "halflife/".Length : "payload/".Length)..];
            var engine = Path.GetExtension(relative) switch { ".dll" => "hl.exe", ".so" => "hl_linux", _ => null };
            if (isHalfLifeRoot && engine is not null && !File.Exists(Path.Combine(halfLifeDir, engine)))
            {
                continue;
            }
            var target = Path.Combine(isHalfLifeRoot ? halfLifeDir : modDir, relative);
            Console.WriteLine(isHalfLifeRoot ? $"  {relative} (Half-Life folder)" : $"  {relative}");

            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            using var source = assembly.GetManifestResourceStream(name)!;
            using var destination = File.Create(target);
            source.CopyTo(destination);
        }
    }

    static void RemoveObsolete(string modDir)
    {
        foreach (var relative in Obsolete)
        {
            var path = Path.Combine(modDir, relative);
            if (!File.Exists(path))
            {
                continue;
            }
            Console.WriteLine($"Removing old {relative}");
            File.Delete(path);

            var dir = Path.GetDirectoryName(path)!;
            if (!Directory.EnumerateFileSystemEntries(dir).Any())
            {
                Directory.Delete(dir);
            }
        }
    }

    static void CopyAll(string sourceRoot, IEnumerable<(string From, string To)> items, string modDir)
    {
        foreach (var item in items)
        {
            var count = Copy(sourceRoot, item, modDir);
            Console.WriteLine(count > 0
                ? $"  {item.From} ({count} files)"
                : $"  warning: nothing found for {item.From}");
        }
    }

    static int Copy(string sourceRoot, (string From, string To) item, string modDir)
    {
        var source = Path.Combine(sourceRoot, item.From);
        var targetDir = Path.Combine(modDir, item.To);

        if (item.From.EndsWith('/'))
        {
            return Directory.Exists(source) ? CopyTree(source, targetDir) : 0;
        }

        if (!item.To.EndsWith('/'))
        {
            if (!File.Exists(source))
            {
                return 0;
            }
            Directory.CreateDirectory(Path.GetDirectoryName(targetDir)!);
            File.Copy(source, targetDir, overwrite: true);
            return 1;
        }

        var sourceDir = Path.GetDirectoryName(source)!;
        if (!Directory.Exists(sourceDir))
        {
            return 0;
        }

        var files = Directory.GetFiles(sourceDir, Path.GetFileName(source));
        if (files.Length > 0)
        {
            Directory.CreateDirectory(targetDir);
        }
        foreach (var file in files)
        {
            File.Copy(file, Path.Combine(targetDir, Path.GetFileName(file)), overwrite: true);
        }
        return files.Length;
    }

    static int CopyTree(string sourceDir, string targetDir)
    {
        Directory.CreateDirectory(targetDir);
        var count = 0;
        foreach (var file in Directory.GetFiles(sourceDir))
        {
            File.Copy(file, Path.Combine(targetDir, Path.GetFileName(file)), overwrite: true);
            count++;
        }
        foreach (var dir in Directory.GetDirectories(sourceDir))
        {
            count += CopyTree(dir, Path.Combine(targetDir, Path.GetFileName(dir)));
        }
        return count;
    }
}
