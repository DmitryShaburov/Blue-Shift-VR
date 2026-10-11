using System.Diagnostics;
using BsvrInstaller;

Console.WriteLine("Half-Life: VR Mod - Blue Shift installer");
Console.WriteLine();

var steam = Steam.FindRoot();
if (steam is null)
{
    Console.WriteLine("Steam was not found in the registry, please enter the folders by hand.");
}

var halfLifeDir = AskFolder(
    "Half-Life folder (contains hl.exe and bshift\\)",
    steam is null ? null : Steam.FindApp(steam, Steam.BlueShiftAppId),
    Steam.IsHalfLifeWithBlueShift,
    "Half-Life: Blue Shift was not found in Steam. Install it and run it once, or enter its folder.");

WaitForHalfLife(steam);

var halfLifeVrDir = AskFolder(
    "Half-Life: VR Mod folder (contains hl.exe and valve\\actions\\)",
    steam is null ? null : Steam.FindApp(steam, Steam.HalfLifeVrAppId),
    Steam.IsHalfLifeVr,
    "Half-Life: VR Mod was not found in Steam. It is free and provides the VR assets; install it or enter its folder.");

var modDir = Path.Combine(halfLifeDir, "bsvr");
Console.WriteLine();
Console.WriteLine($"Installing to {modDir}");
if (!AskYesNo("Continue?", defaultYes: true))
{
    return 1;
}

WaitForSteamToExit();

Console.WriteLine();
try
{
    Assets.Install(halfLifeDir, halfLifeVrDir, modDir);
}
catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
{
    Console.WriteLine();
    Console.WriteLine($"Failed: {ex.Message}");
    Console.WriteLine("Close Steam and the game, or run the installer as administrator, and try again.");
    Pause();
    return 1;
}

Console.WriteLine();
Console.WriteLine("Done. Start Steam, then start \"Half-Life: VR Mod - Blue Shift\" from the library.");
Pause();
return 0;

static string AskFolder(string what, string? suggestion, Func<string, bool> looksRight, string notFoundHint)
{
    if (suggestion is not null && Directory.Exists(suggestion) && looksRight(suggestion))
    {
        var found = Path.GetFullPath(suggestion);
        Console.WriteLine($"{what}: {found} (found via Steam)");
        return found;
    }

    Console.WriteLine(notFoundHint);
    Console.WriteLine("Press Enter without a folder to quit.");

    while (true)
    {
        Console.Write($"{what}: ");
        var folder = ReadLine().Trim().Trim('"');
        if (folder.Length == 0)
        {
            Console.WriteLine("Nothing to install without it, quitting.");
            Environment.Exit(1);
        }
        if (Directory.Exists(folder) && looksRight(folder))
        {
            return Path.GetFullPath(folder);
        }
        Console.WriteLine("That folder does not look right, please try again.");
    }
}

static bool IsSteamRunning() =>
    Process.GetProcessesByName("steam").Length > 0 || Process.GetProcessesByName("steamwebhelper").Length > 0;

// Steam lists mods only under Half-Life, so the mod will not show up in the library without it.
static void WaitForHalfLife(string? steam)
{
    if (steam is null)
    {
        return;
    }

    while (!Steam.IsOwnedAndInstalled(steam, Steam.HalfLifeAppId))
    {
        Console.WriteLine();
        Console.WriteLine("Half-Life is not installed. Steam lists mods only under Half-Life, so the mod will not");
        Console.WriteLine("appear in the library without it. Add Half-Life to your Steam library, install it as its own game");
        Console.WriteLine("(Blue Shift alone is not enough) and wait for the download to finish.");
        Console.Write("Press Enter to check again (close this window to cancel): ");
        ReadLine();
    }
}

// Steam reads the list of mods only at startup, and closing its window just minimizes it to the tray.
static void WaitForSteamToExit()
{
    while (IsSteamRunning())
    {
        Console.WriteLine();
        Console.WriteLine("Steam is running. It must be fully exited for the mod to appear in the library:");
        Console.WriteLine("    Steam menu -> Exit, or right-click the Steam tray icon -> Exit.");
        Console.WriteLine("(Closing the Steam window is not enough, it keeps running in the tray.)");
        Console.Write("Press Enter once Steam has exited (close this window to cancel): ");
        ReadLine();
    }
}

static bool AskYesNo(string question, bool defaultYes)
{
    Console.Write($"{question} [{(defaultYes ? "Y/n" : "y/N")}] ");
    var answer = ReadLine().Trim().ToLowerInvariant();
    return answer.Length == 0 ? defaultYes : answer.StartsWith('y');
}

static string ReadLine()
{
    var line = Console.ReadLine();
    if (line is null)
    {
        Console.WriteLine();
        Console.WriteLine("Input closed, aborting.");
        Environment.Exit(1);
    }
    return line;
}

static void Pause()
{
    Console.Write("Press Enter to exit.");
    Console.ReadLine();
}
