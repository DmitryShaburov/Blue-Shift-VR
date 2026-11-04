using System.Text;

namespace BsvrInstaller;

// Adds the sentence groups the VR code plays to the game's sentences.txt: what guards and scientists say when the
// player points a gun at them (BA_GUN_*, SC_GUN_*) or hits them with gibs (BA_GIB_*, SC_GIB_*). Blue Shift's file has
// none of them, so without this they react silently. Lines are Half-Life: VR's (art/sound/sentences.txt), minus its
// female scientist groups. This takes the player's own file (bshift, which the mod falls back to, then valve), appends
// the groups it doesn't have yet and writes it to the mod folder, which the game reads first. No Valve files are
// shipped. Run the installer again after a game update that changes sentences.txt.
static class Sentences
{
    const string File = "sentences.txt";

    static readonly string[] VrLines =
    [
        "BA_GIB_NEUT0 barney/donthurtem",
        "BA_GIB_NEUT1 barney/nope",
        "BA_GIB_NEUT2 barney/ba_whatyou",
        "BA_GIB_NEUT3 barney/ba_whoathere",
        "BA_GIB_NEUT4 barney/ba_watchit",
        "BA_GIB_NEUT5 barney/ba_stepoff",
        "BA_GIB_NEUT6 barney/stop1",
        "BA_GIB_NEUT7 barney/ba_dotoyou",
        "BA_GIB_NEUT8 barney/soundsbad",
        "BA_GIB_NEUT9 barney/ba_tomb",
        "BA_GIB_NEUT10 barney/ba_close",
        "BA_GIB_NEUT11 barney/ba_pissme",
        "BA_GIB_NEUT12 barney/imhit",
        "BA_GIB_NEUT13 barney/gladof38",
        "BA_GIB_NEUT14 barney/ba_somuch",
        "BA_GIB_NEUT15 barney/ba_friends",
        "",
        "BA_GIB_BLD0 barney/ba_die3",
        "BA_GIB_BLD1 barney/whatisthat",
        "BA_GIB_BLD2 barney/checkwounds",
        "BA_GIB_BLD3 barney/stench",
        "BA_GIB_BLD4 barney/c1a2_ba_4zomb",
        "BA_GIB_BLD5 barney/ba_firepl",
        "",
        "SC_GIB_NEUT0 scientist/sci_die3",
        "SC_GIB_NEUT1 scientist/sci_fear3",
        "SC_GIB_NEUT2 scientist/sci_pain5",
        "SC_GIB_NEUT3 scientist/sci_fear10",
        "SC_GIB_NEUT4 scientist/sci_fear1",
        "SC_GIB_NEUT5 scientist/sci_fear12",
        "SC_GIB_NEUT6 scientist/whatyoudoing",
        "SC_GIB_NEUT7 scientist/cantbeserious",
        "SC_GIB_NEUT8 scientist/cantbeworse",
        "SC_GIB_NEUT9 scientist/nogrant",
        "SC_GIB_NEUT10 scientist/c1a0_sci_samp",
        "",
        "SC_GIB_BLD0 scientist/hungryyet",
        "SC_GIB_BLD1 scientist/sneeze",
        "SC_GIB_BLD2 scientist/stench",
        "SC_GIB_BLD3 scientist/newsample",
        "SC_GIB_BLD4 scientist/peculiarodor",
        "SC_GIB_BLD5 scientist/howinteresting",
        "SC_GIB_BLD6 scientist/importantspecies",
        "SC_GIB_BLD7 scientist/everseen",
        "SC_GIB_BLD8 scientist/goodpaper",
        "SC_GIB_BLD9 scientist/bloodsample",
        "SC_GIB_BLD10 scientist/peculiarmarks",
        "SC_GIB_BLD11 scientist/organicmatter",
        "SC_GIB_BLD12 scientist/neverseen",
        "",
        "BA_GUN_ONE0 barney/ba_whoathere",
        "BA_GUN_ONE1 barney/ba_watchit",
        "BA_GUN_ONE2 barney/c2a4_ba_1tau",
        "BA_GUN_TWO0 barney/ba_dotoyou",
        "BA_GUN_TWO1 barney/ba_whatyou",
        "BA_GUN_TWO2 barney/donthurtem",
        "BA_GUN_TWO3 barney/standback",
        "BA_GUN_THR0 barney/ba_stepoff",
        "BA_GUN_THR1 barney/ba_tomb",
        "BA_GUN_THR2 barney/ba_pissme",
        "BA_GUN_THR3 barney/ba_dontmake",
        "BA_GUN_THR4 barney/ba_somuch",
        "BA_GUN_ATT0 barney/ba_attack1",
        "BA_GUN_ATT1 barney/aimforhead",
        "BA_GUN_ATT2 barney/targetpractice",
        "",
        "SC_GUN_ONE0 scientist/scream15",
        "SC_GUN_ONE1 scientist/scream16",
        "SC_GUN_ONE2 scientist/scream17",
        "SC_GUN_ONE3 scientist/scream18",
        "SC_GUN_ONE4 scientist/scream19",
        "SC_GUN_ONE5 scientist/startle1",
        "SC_GUN_ONE6 scientist/startle2",
        "SC_GUN_ONE7 scientist/startle3",
        "SC_GUN_ONE8 scientist/startle4",
        "SC_GUN_ONE9 scientist/nooo",
        "SC_GUN_TWO0 scientist/c1a3_sci_team",
        "SC_GUN_TWO1 scientist/whatyoudoing",
        "SC_GUN_TWO2 scientist/reconsider",
        "SC_GUN_THR0 scientist/canttakemore",
        "SC_GUN_THR1 scientist/iwoundedbad",
        "SC_GUN_THR2 scientist/beenaburden",
        "SC_GUN_THR3 scientist/c1a0_sci_stayback",
        "SC_GUN_THR4 scientist/sci_fear14",
        "SC_GUN_RUN0 scientist/noplease",
        "SC_GUN_RUN1 scientist/dontwantdie",
    ];

    public static void Install(string halfLifeDir, string modDir)
    {
        var source = new[] { "bshift", "valve" }
            .Select(game => Path.Combine(halfLifeDir, game, "sound", File))
            .FirstOrDefault(System.IO.File.Exists);
        if (source == null)
        {
            Console.WriteLine($"  warning: {File} not found, NPCs stay silent at gunpoint");
            return;
        }

        // Latin1 keeps the file's non-ASCII bytes as they are
        var text = System.IO.File.ReadAllText(source, Encoding.Latin1);
        var newline = text.Contains("\r\n") ? "\r\n" : "\n";

        var existing = text.Split('\n')
            .Select(line => line.Trim())
            .Where(line => line.Length > 0 && !line.StartsWith("//"))
            .Select(line => line.Split((char[]?)null, 2, StringSplitOptions.RemoveEmptyEntries)[0])
            .ToHashSet(StringComparer.OrdinalIgnoreCase);

        var added = VrLines.Where(line => line.Length > 0 && !existing.Contains(line.Split(' ')[0])).ToList();
        var lines = VrLines.Where(line => line.Length == 0 || added.Contains(line));

        var result = new StringBuilder(text.TrimEnd('\r', '\n'));
        result.Append(newline).Append(newline).Append("// VR: gunpoint and gib reactions").Append(newline);
        foreach (var line in lines)
        {
            result.Append(line).Append(newline);
        }

        var target = Path.Combine(modDir, "sound", File);
        Directory.CreateDirectory(Path.GetDirectoryName(target)!);
        System.IO.File.WriteAllText(target, result.ToString(), Encoding.Latin1);
        Console.WriteLine($"  {File} ({added.Count} VR sentences added)");
    }
}
