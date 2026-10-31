using System.Diagnostics;
using BsvrInstaller;

// Started by double-click, the console window closes on a crash before anyone can read why.
AppDomain.CurrentDomain.UnhandledException += (_, e) =>
{
    Console.WriteLine();
    Console.WriteLine($"Unexpected error: {e.ExceptionObject}");
    Console.WriteLine("Please report this message to the mod authors.");
    Pause();
};

Console.WriteLine("Half-Life: VR Mod - Blue Shift installer");
Console.WriteLine();

var steam = Steam.FindRoot();
if (steam is null)
{
    return Quit("Steam was not found. Install Steam and the games below, then run the installer again.");
}

var halfLifeDir = FindFolder(steam, Steam.BlueShiftAppId, Steam.IsHalfLifeWithBlueShift, "Half-Life folder");
if (halfLifeDir is null)
{
    return Quit("Half-Life: Blue Shift was not found in Steam. Install it, run it once and run the installer again.");
}

var modDir = Path.Combine(halfLifeDir, "bsvr");
var installed = File.Exists(Path.Combine(modDir, "liblist.gam"));
if (!installed)
{
    WaitForHalfLife(steam);
}

var halfLifeVrDir = FindFolder(steam, Steam.HalfLifeVrAppId, Steam.IsHalfLifeVr, "Half-Life: VR Mod folder");
if (halfLifeVrDir is null)
{
    return Quit("Half-Life: VR Mod was not found in Steam. It is free and provides the VR assets; install it and run the installer again.");
}

Console.WriteLine();
if (installed)
{
    // Steam already lists the mod, so it can keep running; only the game locks the files.
    Console.WriteLine($"The mod is already installed in {modDir}");
    if (!AskYesNo("Update it?", defaultYes: true))
    {
        return 1;
    }
}
else
{
    Console.WriteLine($"Installing to {modDir}");
    if (!AskYesNo("Continue?", defaultYes: true))
    {
        return 1;
    }
    WaitForSteamToExit();
}
WaitForGameToExit();

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
Console.WriteLine(installed
    ? "Done. Start \"Half-Life: VR Mod - Blue Shift\" from the Steam library."
    : "Done. Start Steam, then start \"Half-Life: VR Mod - Blue Shift\" from the library.");
Pause();
return 0;

static string? FindFolder(string steam, int appId, Func<string, bool> looksRight, string what)
{
    var folder = Steam.FindApp(steam, appId);
    if (folder is null || !looksRight(folder))
    {
        return null;
    }
    folder = Path.GetFullPath(folder);
    Console.WriteLine($"{what}: {folder}");
    return folder;
}

static int Quit(string message)
{
    Console.WriteLine(message);
    Pause();
    return 1;
}

// The running game keeps the mod DLLs and openvr_api.dll locked.
static void WaitForGameToExit()
{
    while (Process.GetProcessesByName("hl").Length > 0)
    {
        Console.WriteLine();
        Console.WriteLine("The game (hl.exe) is running. Quit it so its files can be replaced.");
        Console.Write("Press Enter once the game has exited (close this window to cancel): ");
        ReadLine();
    }
}

static bool IsSteamRunning() =>
    Process.GetProcessesByName("steam").Length > 0 || Process.GetProcessesByName("steamwebhelper").Length > 0;

// Steam lists mods only under Half-Life, so the mod will not show up in the library without it.
static void WaitForHalfLife(string steam)
{
    while (!Steam.IsOwnedAndInstalled(steam, Steam.HalfLifeAppId))
    {
        Console.WriteLine();
        Console.WriteLine("Half-Life is not installed. Steam lists mods only under Half-Life, so the mod will not");
        Console.WriteLine("appear in the library without it. Add Half-Life to your Steam library, install it as its own game");
        Console.WriteLine("(Blue Shift alone is not enough) and wait for the download to finish.");
        Console.WriteLine("If Half-Life is installed and this check is wrong, type C to continue anyway.");
        Console.Write("Press Enter to check again (close this window to cancel): ");
        if (ReadLine().Trim().Equals("c", StringComparison.OrdinalIgnoreCase))
        {
            return;
        }
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
