namespace BsvrInstaller;

// Retail Blue Shift maps are standard version 30 BSPs with the entity and plane lumps swapped in the
// header. The engine only compensates for that when the game folder is bshift, so maps for bsvr are
// rewritten in standard order, the same thing Blue Shift Updated's installer does.
//
// A "maps/<name>.ent" file in the mod folder (from the payload) replaces that map's entities, which is
// how Blue Shift Updated ships its fix for ba_yard4a.
static class Maps
{
    const int HeaderLumps = 15;
    const int PlaneSize = 20;

    public static void Install(string halfLifeDir, string modDir)
    {
        var sourceDir = Path.Combine(halfLifeDir, "bshift", "maps");
        var targetDir = Path.Combine(modDir, "maps");
        Directory.CreateDirectory(targetDir);

        var maps = Directory.GetFiles(sourceDir, "*.bsp");
        foreach (var map in maps)
        {
            var name = Path.GetFileNameWithoutExtension(map);
            var entFile = Path.Combine(targetDir, name + ".ent");
            var entities = File.Exists(entFile) ? File.ReadAllBytes(entFile) : null;

            Convert(map, Path.Combine(targetDir, name + ".bsp"), entities);
            if (entities is not null)
            {
                Console.WriteLine($"  {name}.bsp: entities replaced from {name}.ent");
            }
        }
        Console.WriteLine($"  {maps.Length} maps converted");
    }

    static void Convert(string sourcePath, string targetPath, byte[]? entities)
    {
        var data = File.ReadAllBytes(sourcePath);
        var lumps = new (int Offset, int Length)[HeaderLumps];
        for (var i = 0; i < HeaderLumps; i++)
        {
            lumps[i] = (BitConverter.ToInt32(data, 4 + i * 8), BitConverter.ToInt32(data, 8 + i * 8));
        }

        // Leave maps that are already in standard order alone: entities start with '{' and planes are 20 bytes each.
        var isStandard = data[lumps[0].Offset] == (byte)'{' && lumps[1].Length % PlaneSize == 0;
        if (!isStandard)
        {
            (lumps[0], lumps[1]) = (lumps[1], lumps[0]);
        }

        if (entities is not null)
        {
            // Append the new entity text, null terminated, and point the lump at it. The old text stays as dead bytes.
            lumps[0] = (data.Length, entities.Length + 1);
            Array.Resize(ref data, data.Length + entities.Length + 1);
            entities.CopyTo(data, lumps[0].Offset);
            data[^1] = 0;
        }

        for (var i = 0; i < HeaderLumps; i++)
        {
            BitConverter.GetBytes(lumps[i].Offset).CopyTo(data, 4 + i * 8);
            BitConverter.GetBytes(lumps[i].Length).CopyTo(data, 8 + i * 8);
        }
        File.WriteAllBytes(targetPath, data);
    }
}
