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
        ("valve/fonts/Roboto-Regular.ttf",   "fonts/"),
        ("valve/textures/hud/",              "textures/hud/"),     // HUD sprite sheets as PNG
        ("valve/textures/background.png",    "textures/"),
        ("valve/textures/null.png",          "textures/"),
        ("valve/textures/skybox/",           "textures/skybox/"),  // VR sky for maps whose sky name matches
        ("valve/sprites/black.spr",          "sprites/"),
        ("valve/sound/plats/train_use2.wav", "sound/plats/"),
    ];

    public static void Install(string halfLifeDir, string halfLifeVrDir, string modDir)
    {
        Directory.CreateDirectory(modDir);

        Console.WriteLine("Mod files");
        ExtractPayload(modDir, halfLifeDir);

        Console.WriteLine("VR assets from Half-Life: VR");
        CopyAll(halfLifeVrDir, FromHalfLifeVr, modDir);

        Console.WriteLine("Maps from Blue Shift");
        Maps.Install(halfLifeDir, modDir);
    }

    // Embedded resource "payload/dlls/hl.dll" goes to "<mod>/dlls/hl.dll", and so on.
    // "halflife/openvr_api.dll" goes next to hl.exe, where Windows resolves client.dll's imports.
    static void ExtractPayload(string modDir, string halfLifeDir)
    {
        var assembly = Assembly.GetExecutingAssembly();
        var names = assembly.GetManifestResourceNames();
        if (!names.Any(n => n.EndsWith("hl.dll")) || !names.Any(n => n.EndsWith("client.dll")))
        {
            Console.WriteLine("  warning: game DLLs are not embedded in this build, the mod will not start");
        }

        foreach (var name in names)
        {
            var normalized = name.Replace('\\', '/');
            var isHalfLifeRoot = normalized.StartsWith("halflife/");
            var relative = normalized[(isHalfLifeRoot ? "halflife/".Length : "payload/".Length)..];
            var target = Path.Combine(isHalfLifeRoot ? halfLifeDir : modDir, relative);
            Console.WriteLine(isHalfLifeRoot ? $"  {relative} (Half-Life folder)" : $"  {relative}");

            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            using var source = assembly.GetManifestResourceStream(name)!;
            using var destination = File.Create(target);
            source.CopyTo(destination);
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
