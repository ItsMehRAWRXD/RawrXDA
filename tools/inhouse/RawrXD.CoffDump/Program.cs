using System.Buffers.Binary;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

static class Program
{
    private const ushort IMAGE_FILE_MACHINE_AMD64 = 0x8664;
    private const uint IMAGE_SCN_LNK_COMDAT = 0x00001000;

    private const byte IMAGE_SYM_CLASS_EXTERNAL = 2;

    private sealed record CoffSummary(
        string path,
        long length,
        string machine,
        int sectionCount,
        int symbolCount,
        IReadOnlyList<CoffSection> sections,
        IReadOnlyList<string> undefinedExternals,
        string? drectve
    );

    private sealed record CoffSection(
        string name,
        uint sizeOfRawData,
        uint pointerToRawData,
        uint pointerToRelocations,
        ushort numberOfRelocations,
        uint characteristics,
        bool isComdat
    );

    public static int Main(string[] args)
    {
        if (args.Length == 0 || args.Contains("--help") || args.Contains("-h"))
        {
            Console.WriteLine("rawrxd_coffdump: COFF x64 .obj analyzer");
            Console.WriteLine("Usage:");
            Console.WriteLine("  rawrxd_coffdump --in <obj...> [--out <json>] [--max-undef 200]");
            Console.WriteLine("  rawrxd_coffdump --in-list <file_with_paths.txt> [--out <json>]");
            return 2;
        }

        var inputs = new List<string>();
        string? inList = null;
        string? outPath = null;
        var maxUndef = 200;

        for (var i = 0; i < args.Length; i++)
        {
            var a = args[i];
            if (a.Equals("--in", StringComparison.OrdinalIgnoreCase))
            {
                for (i++; i < args.Length && !args[i].StartsWith("--", StringComparison.Ordinal); i++)
                    inputs.Add(args[i]);
                i--;
                continue;
            }
            if (a.Equals("--in-list", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
            {
                inList = args[++i];
                continue;
            }
            if (a.Equals("--out", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
            {
                outPath = args[++i];
                continue;
            }
            if (a.Equals("--max-undef", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
            {
                if (!int.TryParse(args[++i], out maxUndef) || maxUndef < 0) maxUndef = 200;
                continue;
            }
        }

        if (inList != null)
        {
            foreach (var line in File.ReadAllLines(inList))
            {
                var trimmed = line.Trim();
                if (trimmed.Length == 0) continue;
                inputs.Add(trimmed.Trim('"'));
            }
        }

        inputs = inputs
            .Where(p => !string.IsNullOrWhiteSpace(p))
            .Select(p => Path.GetFullPath(p))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToList();

        if (inputs.Count == 0)
        {
            Console.Error.WriteLine("rawrxd_coffdump: no inputs.");
            return 2;
        }

        var results = new List<CoffSummary>(inputs.Count);
        foreach (var path in inputs)
        {
            try
            {
                results.Add(AnalyzeObj(path, maxUndef));
            }
            catch (Exception ex)
            {
                results.Add(new CoffSummary(
                    path: path,
                    length: File.Exists(path) ? new FileInfo(path).Length : -1,
                    machine: "ERROR",
                    sectionCount: 0,
                    symbolCount: 0,
                    sections: Array.Empty<CoffSection>(),
                    undefinedExternals: new[] { ex.Message },
                    drectve: null
                ));
            }
        }

        var jsonOptions = new JsonSerializerOptions
        {
            WriteIndented = true,
            DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
        };

        var json = JsonSerializer.Serialize(results, jsonOptions);
        if (!string.IsNullOrWhiteSpace(outPath))
        {
            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outPath))!);
            File.WriteAllText(outPath, json, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
            Console.WriteLine($"OK: wrote {outPath}");
        }
        else
        {
            Console.WriteLine(json);
        }

        return 0;
    }

    private static CoffSummary AnalyzeObj(string path, int maxUndef)
    {
        using var fs = File.OpenRead(path);
        using var br = new BinaryReader(fs, Encoding.ASCII, leaveOpen: false);

        if (fs.Length < 20) throw new InvalidDataException("Too small to be COFF.");

        var fileHeader = br.ReadBytes(20);
        var machine = BinaryPrimitives.ReadUInt16LittleEndian(fileHeader.AsSpan(0, 2));
        var numberOfSections = BinaryPrimitives.ReadUInt16LittleEndian(fileHeader.AsSpan(2, 2));
        var pointerToSymbolTable = BinaryPrimitives.ReadUInt32LittleEndian(fileHeader.AsSpan(8, 4));
        var numberOfSymbols = BinaryPrimitives.ReadUInt32LittleEndian(fileHeader.AsSpan(12, 4));
        var sizeOfOptionalHeader = BinaryPrimitives.ReadUInt16LittleEndian(fileHeader.AsSpan(16, 2));

        if (sizeOfOptionalHeader != 0)
        {
            // Some tools emit "bigobj"; we don't support it yet.
            throw new NotSupportedException($"Unsupported COFF optional header size: {sizeOfOptionalHeader} (bigobj?)");
        }

        var stringTable = ReadStringTable(br, pointerToSymbolTable, numberOfSymbols);

        var sections = new List<CoffSection>(numberOfSections);
        var sectionNameToIndex = new Dictionary<string, int>(StringComparer.Ordinal);

        fs.Position = 20;
        for (var i = 0; i < numberOfSections; i++)
        {
            var sh = br.ReadBytes(40);
            var name = DecodeSectionName(sh.AsSpan(0, 8), stringTable);
            var sizeOfRawData = BinaryPrimitives.ReadUInt32LittleEndian(sh.AsSpan(16, 4));
            var pointerToRawData = BinaryPrimitives.ReadUInt32LittleEndian(sh.AsSpan(20, 4));
            var pointerToRelocations = BinaryPrimitives.ReadUInt32LittleEndian(sh.AsSpan(24, 4));
            var numberOfRelocations = BinaryPrimitives.ReadUInt16LittleEndian(sh.AsSpan(32, 2));
            var characteristics = BinaryPrimitives.ReadUInt32LittleEndian(sh.AsSpan(36, 4));
            var isComdat = (characteristics & IMAGE_SCN_LNK_COMDAT) != 0;

            sectionNameToIndex.TryAdd(name, i + 1); // COFF section numbers are 1-based
            sections.Add(new CoffSection(
                name,
                sizeOfRawData,
                pointerToRawData,
                pointerToRelocations,
                numberOfRelocations,
                characteristics,
                isComdat
            ));
        }

        var drectve = TryReadSectionAscii(br, fs, sections, ".drectve");
        var undefined = ReadUndefinedExternals(br, fs, pointerToSymbolTable, numberOfSymbols, stringTable, maxUndef);

        return new CoffSummary(
            path: path,
            length: fs.Length,
            machine: machine switch
            {
                IMAGE_FILE_MACHINE_AMD64 => "AMD64",
                _ => $"0x{machine:X4}"
            },
            sectionCount: numberOfSections,
            symbolCount: checked((int)Math.Min(int.MaxValue, numberOfSymbols)),
            sections: sections,
            undefinedExternals: undefined,
            drectve: drectve
        );
    }

    private static byte[] ReadStringTable(BinaryReader br, uint pointerToSymbolTable, uint numberOfSymbols)
    {
        var symbolTableOffset = pointerToSymbolTable;
        var stringTableOffset = symbolTableOffset + numberOfSymbols * 18u;
        var fs = br.BaseStream;
        if (stringTableOffset + 4 > fs.Length) return Array.Empty<byte>();

        fs.Position = stringTableOffset;
        var lenBytes = br.ReadBytes(4);
        var length = BinaryPrimitives.ReadUInt32LittleEndian(lenBytes);
        if (length < 4 || stringTableOffset + length > fs.Length) return Array.Empty<byte>();

        fs.Position = stringTableOffset;
        return br.ReadBytes(checked((int)length));
    }

    private static string DecodeSectionName(ReadOnlySpan<byte> raw8, byte[] stringTable)
    {
        var s = Encoding.ASCII.GetString(raw8).TrimEnd('\0');
        if (s.Length == 0) return "";

        if (s[0] == '/' && stringTable.Length >= 4)
        {
            if (!int.TryParse(s.AsSpan(1), out var offset) || offset <= 0) return s;
            if (offset >= stringTable.Length) return s;
            var end = offset;
            while (end < stringTable.Length && stringTable[end] != 0) end++;
            return Encoding.ASCII.GetString(stringTable, offset, end - offset);
        }

        return s;
    }

    private static string? TryReadSectionAscii(BinaryReader br, Stream fs, IReadOnlyList<CoffSection> sections, string sectionName)
    {
        var sec = sections.FirstOrDefault(s => string.Equals(s.name, sectionName, StringComparison.Ordinal));
        if (sec == null || sec.sizeOfRawData == 0 || sec.pointerToRawData == 0) return null;
        if (sec.pointerToRawData + sec.sizeOfRawData > fs.Length) return null;

        fs.Position = sec.pointerToRawData;
        var bytes = br.ReadBytes(checked((int)sec.sizeOfRawData));
        var text = Encoding.ASCII.GetString(bytes).TrimEnd('\0', '\r', '\n', ' ');
        return text.Length == 0 ? null : text;
    }

    private static IReadOnlyList<string> ReadUndefinedExternals(
        BinaryReader br,
        Stream fs,
        uint pointerToSymbolTable,
        uint numberOfSymbols,
        byte[] stringTable,
        int maxUndef)
    {
        if (pointerToSymbolTable == 0 || numberOfSymbols == 0) return Array.Empty<string>();
        if (pointerToSymbolTable + numberOfSymbols * 18u > fs.Length) return Array.Empty<string>();

        fs.Position = pointerToSymbolTable;
        var undefined = new List<string>();

        for (uint i = 0; i < numberOfSymbols;)
        {
            var entry = br.ReadBytes(18);
            var name = DecodeSymbolName(entry.AsSpan(0, 8), stringTable);
            var sectionNumber = BinaryPrimitives.ReadInt16LittleEndian(entry.AsSpan(12, 2));
            var storageClass = entry[16];
            var auxCount = entry[17];

            var isUndefinedExternal = storageClass == IMAGE_SYM_CLASS_EXTERNAL && sectionNumber == 0;
            if (isUndefinedExternal && name.Length != 0)
            {
                undefined.Add(name);
                if (undefined.Count >= maxUndef && maxUndef > 0) break;
            }

            // Skip aux entries
            if (auxCount > 0)
                br.BaseStream.Position += auxCount * 18;

            i += 1u + auxCount;
        }

        return undefined.Distinct(StringComparer.Ordinal).OrderBy(s => s, StringComparer.Ordinal).ToList();
    }

    private static string DecodeSymbolName(ReadOnlySpan<byte> raw8, byte[] stringTable)
    {
        // IMAGE_SYMBOL.Name: either short name bytes[8] or {Zeroes=0, Offset!=0} into string table.
        var zeroes = BinaryPrimitives.ReadUInt32LittleEndian(raw8[..4]);
        var offset = BinaryPrimitives.ReadUInt32LittleEndian(raw8.Slice(4, 4));

        if (zeroes == 0 && offset != 0 && stringTable.Length >= 4 && offset < stringTable.Length)
        {
            var off = checked((int)offset);
            var end = off;
            while (end < stringTable.Length && stringTable[end] != 0) end++;
            return Encoding.ASCII.GetString(stringTable, off, end - off);
        }

        return Encoding.ASCII.GetString(raw8).TrimEnd('\0');
    }
}

