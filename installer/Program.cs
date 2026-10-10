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

if (steam is not null && Steam.FindApp(steam, Steam.HalfLifeAppId) is null)
{
    Console.WriteLine();
    Console.WriteLine("Half-Life itself is not installed. Steam lists mods only under Half-Life, so the mod will not");
    Console.WriteLine("appear in the library until Half-Life is installed. Until then it can be started by adding");
    Console.WriteLine("    -game bsvr");
    Console.WriteLine("to the launch options of Half-Life: Blue Shift in Steam.");
    Console.WriteLine();
}

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
Console.WriteLine("Done. Restart Steam, then start \"Half-Life: VR Mod - Blue Shift\" from the library.");
Pause();
return 0;

static string AskFolder(string what, string? suggestion, Func<string, bool> looksRight, string notFoundHint)
{
    if (suggestion is null)
    {
        Console.WriteLine(notFoundHint);
        Console.WriteLine("Press Enter without a folder to quit.");
    }

    while (true)
    {
        Console.Write(suggestion is null ? $"{what}: " : $"{what} [{suggestion}]: ");
        var answer = ReadLine().Trim().Trim('"');
        var folder = answer.Length > 0 ? answer : suggestion;
        if (folder is null)
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
