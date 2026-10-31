using System.Text;

namespace BsvrInstaller;

// Greys out (or hides) options of the game's own Options dialog that do nothing in VR or that the mod overrides.
// Each tab reads its layout from resource/<tab>.res, where every control is a block with its "fieldName".
// This takes the player's own file (bshift, which the mod falls back to, then valve), sets "enabled" "0" (or
// "visible" "0") on the listed controls and writes it to the mod folder, which the game reads first. No Valve files
// are shipped. Run the installer again after a game update that changes these layouts.
static class OptionsLocks
{
    enum Lock { None, Disable, Hide }

    // File, controls (by fieldName) to disable, or with AllExcept the ones to keep, and controls to hide.
    // Hidden ones are those the game enables itself whatever the file says, so they can't stay greyed out.
    static readonly (string File, string[] Controls, bool AllExcept, string[] Hidden)[] Locks =
    [
        ("OptionsSubAudio.res",    ["Suit Slider", "suit label"], false, []),  // HEV suit volume slider and its label
        ("OptionsSubVideo.res",    ["TextureFiltering", "Brightness", "brightness label", "Gamma", "Gamma label"], true,
                                   ["AspectRatio", "Label4"]),  // everything except texture filtering, brightness and gamma; display mode hidden
        ("OptionsSubMouse.res",    [], true, []),  // Aim tab
        ("OptionsSubKeyboard.res", [], true, []),
    ];

    public static void Install(string halfLifeDir, string modDir)
    {
        foreach (var (file, controls, allExcept, hidden) in Locks)
        {
            var source = new[] { "bshift", "valve" }
                .Select(game => Path.Combine(halfLifeDir, game, "resource", file))
                .FirstOrDefault(File.Exists);
            if (source == null)
            {
                Console.WriteLine($"  warning: {file} not found, its options stay unlocked");
                continue;
            }

            var text = File.ReadAllText(source);
            var (patched, disabled, hiddenCount) = Patch(text, name =>
                hidden.Contains(name, StringComparer.OrdinalIgnoreCase) ? Lock.Hide
                : controls.Contains(name, StringComparer.OrdinalIgnoreCase) != allExcept ? Lock.Disable
                : Lock.None);

            var target = Path.Combine(modDir, "resource", file);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            File.WriteAllText(target, patched, new UTF8Encoding(false));
            Console.WriteLine($"  {file} ({disabled} controls disabled, {hiddenCount} hidden)");
        }
    }

    // A .res file is quoted (or bare) tokens and { } blocks, with // comments:
    //   "Resource\OptionsSubVideo.res" { "Resolution" { "fieldName" "Resolution" "enabled" "1" ... } ... }
    // In each control block (depth 2) whose fieldName gets a lock, sets "enabled" "0" (and "visible" "0" to hide it),
    // adding a key that is missing.
    static (string Text, int Disabled, int Hidden) Patch(string text, Func<string, Lock> getLock)
    {
        var newline = text.Contains("\r\n") ? "\r\n" : "\n";
        var edits = new List<(int Start, int End, string Replacement)>();
        var disabled = 0;
        var hidden = 0;

        var tokens = Tokenize(text);
        var depth = 0;
        string? fieldName = null;
        var values = new Dictionary<string, (int Start, int End)>(StringComparer.OrdinalIgnoreCase);
        var blockIndent = "";
        for (var i = 0; i < tokens.Count; i++)
        {
            var token = tokens[i];
            if (token.Text == "{")
            {
                depth++;
                if (depth == 2)
                {
                    fieldName = null;
                    values.Clear();
                }
                continue;
            }
            if (token.Text == "}")
            {
                var lockType = depth == 2 && fieldName != null ? getLock(fieldName) : Lock.None;
                if (lockType != Lock.None)
                {
                    var keys = lockType == Lock.Hide ? new[] { "enabled", "visible" } : new[] { "enabled" };
                    foreach (var key in keys)
                    {
                        if (values.TryGetValue(key, out var value))
                        {
                            edits.Add((value.Start, value.End, "\"0\""));
                        }
                        else
                        {
                            // before the closing brace's line, indented like the block's keys
                            var lineStart = text.LastIndexOf('\n', token.Start) + 1;
                            edits.Add((lineStart, lineStart, $"{blockIndent}\"{key}\"\t\t\"0\"{newline}"));
                        }
                    }
                    if (lockType == Lock.Hide) hidden++; else disabled++;
                }
                depth--;
                continue;
            }

            // key/value pairs inside a control block
            if (depth == 2 && i + 1 < tokens.Count && tokens[i + 1].Text is not "{" and not "}")
            {
                var key = Unquote(token.Text);
                var value = tokens[i + 1];
                if (key.Equals("fieldName", StringComparison.OrdinalIgnoreCase))
                {
                    fieldName = Unquote(value.Text);
                    var lineStart = text.LastIndexOf('\n', token.Start) + 1;
                    blockIndent = text[lineStart..token.Start];
                }
                values[key] = (value.Start, value.Start + value.Text.Length);
                i++;
            }
        }

        var result = new StringBuilder(text);
        foreach (var (start, end, replacement) in edits.OrderByDescending(e => e.Start))
        {
            result.Remove(start, end - start).Insert(start, replacement);
        }
        return (result.ToString(), disabled, hidden);
    }

    static List<(string Text, int Start)> Tokenize(string text)
    {
        var tokens = new List<(string, int)>();
        var i = 0;
        while (i < text.Length)
        {
            var c = text[i];
            if (char.IsWhiteSpace(c))
            {
                i++;
            }
            else if (c == '/' && i + 1 < text.Length && text[i + 1] == '/')
            {
                while (i < text.Length && text[i] != '\n') i++;
            }
            else if (c is '{' or '}')
            {
                tokens.Add((c.ToString(), i));
                i++;
            }
            else if (c == '"')
            {
                var end = text.IndexOf('"', i + 1);
                if (end < 0) end = text.Length - 1;
                tokens.Add((text[i..(end + 1)], i));
                i = end + 1;
            }
            else
            {
                var start = i;
                while (i < text.Length && !char.IsWhiteSpace(text[i]) && text[i] is not '{' and not '}' and not '"') i++;
                tokens.Add((text[start..i], start));
            }
        }
        return tokens;
    }

    static string Unquote(string token) =>
        token.Length >= 2 && token[0] == '"' && token[^1] == '"' ? token[1..^1] : token;
}
