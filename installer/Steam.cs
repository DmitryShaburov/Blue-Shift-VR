using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace BsvrInstaller;

static class Steam
{
    // Half-Life. Steam only lists mods under this app, so it must be installed for the mod to show up.
    public const int HalfLifeAppId = 70;

    // Half-Life: Blue Shift. Lives inside the shared "Half-Life" folder as bshift\.
    public const int BlueShiftAppId = 130;

    // Half-Life: VR Mod. A standalone copy of Half-Life with the VR assets under valve\.
    public const int HalfLifeVrAppId = 1908720;

    public static string? FindRoot()
    {
        try
        {
            // Steam is 32-bit, so its machine-wide key is under WOW6432Node; the per-user key is not redirected.
            var path = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam")?.GetValue("SteamPath") as string
                ?? RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32)
                    .OpenSubKey(@"SOFTWARE\Valve\Steam")?.GetValue("InstallPath") as string;

            return path is not null && Directory.Exists(Path.Combine(path, "steamapps")) ? Path.GetFullPath(path) : null;
        }
        catch (Exception)
        {
            return null;
        }
    }

    public static string? FindApp(string steamRoot, int appId)
    {
        foreach (var library in LibraryFolders(steamRoot))
        {
            var manifest = Path.Combine(library, "steamapps", $"appmanifest_{appId}.acf");
            if (!File.Exists(manifest))
            {
                continue;
            }

            var installDir = Regex.Match(File.ReadAllText(manifest), "\"installdir\"\\s+\"([^\"]+)\"").Groups[1].Value;
            var folder = Path.Combine(library, "steamapps", "common", installDir);
            if (installDir.Length > 0 && Directory.Exists(folder))
            {
                return folder;
            }
        }
        return null;
    }

    static IEnumerable<string> LibraryFolders(string steamRoot)
    {
        yield return steamRoot;

        var vdf = Path.Combine(steamRoot, "steamapps", "libraryfolders.vdf");
        if (!File.Exists(vdf))
        {
            yield break;
        }
        foreach (Match match in Regex.Matches(File.ReadAllText(vdf), "\"path\"\\s+\"([^\"]+)\""))
        {
            yield return match.Groups[1].Value.Replace(@"\\", @"\");
        }
    }

    public static bool IsHalfLifeWithBlueShift(string folder) =>
        File.Exists(Path.Combine(folder, "hl.exe")) && Directory.Exists(Path.Combine(folder, "bshift"));

    public static bool IsHalfLifeVr(string folder) =>
        File.Exists(Path.Combine(folder, "hl.exe")) && File.Exists(Path.Combine(folder, "valve", "actions", "actions.manifest"));
}
