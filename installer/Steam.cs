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
        if (!OperatingSystem.IsWindows())
        {
            return FindRootLinux();
        }

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

    static string? FindRootLinux()
    {
        var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        string[] candidates =
        [
            Path.Combine(home, ".steam", "steam"),
            Path.Combine(home, ".local", "share", "Steam"),
            Path.Combine(home, ".var", "app", "com.valvesoftware.Steam", ".local", "share", "Steam"), // Flatpak
            Path.Combine(home, "snap", "steam", "common", ".local", "share", "Steam"),                // Snap
        ];

        // ~/.steam/steam is usually a symlink. Resolved, the game folder matches the path its running process reports.
        return candidates
            .Where(path => Directory.Exists(Path.Combine(path, "steamapps")))
            .Select(path => Directory.ResolveLinkTarget(path, returnFinalTarget: true)?.FullName ?? Path.GetFullPath(path))
            .FirstOrDefault();
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

    // Blue Shift pulls Half-Life's files onto the disk as a dependency (depots 1, 2, 3 and 96), with a manifest
    // that looks installed even when Half-Life itself is not. Depot 71 appears only once Half-Life is installed
    // as its own game and disappears again on uninstall. This is a heuristic based on observed manifests
    // (English only so far).
    const string HalfLifeOwnDepot = "71";

    public static bool IsOwnedAndInstalled(string steamRoot, int appId)
    {
        foreach (var library in LibraryFolders(steamRoot))
        {
            var manifest = Path.Combine(library, "steamapps", $"appmanifest_{appId}.acf");
            if (File.Exists(manifest) &&
                Regex.IsMatch(File.ReadAllText(manifest), "\"InstalledDepots\"\\s*\\{[\\s\\S]*?\"" + HalfLifeOwnDepot + "\"\\s*\\{"))
            {
                return FindApp(steamRoot, appId) is not null;
            }
        }
        return false;
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
        (File.Exists(Path.Combine(folder, "hl.exe")) || File.Exists(Path.Combine(folder, "hl_linux"))) &&
        Directory.Exists(Path.Combine(folder, "bshift"));

    public static bool IsHalfLifeVr(string folder) =>
        File.Exists(Path.Combine(folder, "hl.exe")) && File.Exists(Path.Combine(folder, "valve", "actions", "actions.manifest"));
}
