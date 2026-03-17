using System.Buffers.Binary;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

static class Program
{
    private const ushort IMAGE_FILE_MACHINE_I386 = 0x014C;
    private const ushort IMAGE_FILE_MACHINE_AMD64 = 0x8664;

    private const uint IMAGE_SCN_CNT_CODE = 0x00000020;
    private const uint IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040;
    private const uint IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080;

    private const uint IMAGE_SCN_MEM_EXECUTE = 0x20000000;
    private const uint IMAGE_SCN_MEM_READ = 0x40000000;
    private const uint IMAGE_SCN_MEM_WRITE = 0x80000000;

    private const uint IMAGE_REL_I386_DIR32 = 0x0006;
    private const uint IMAGE_REL_I386_REL32 = 0x0014;
    private const uint IMAGE_REL_AMD64_ADDR64 = 0x0001;
    private const uint IMAGE_REL_AMD64_REL32 = 0x0004;

    private const ushort IMAGE_SUBSYSTEM_WINDOWS_GUI = 2;
    private const ushort IMAGE_SUBSYSTEM_WINDOWS_CUI = 3;

    private const ushort IMAGE_FILE_EXECUTABLE_IMAGE = 0x0002;
    private const ushort IMAGE_FILE_LARGE_ADDRESS_AWARE = 0x0020;

    private const ushort IMAGE_NT_OPTIONAL_HDR32_MAGIC = 0x10B;
    private const ushort IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20B;

    private const ushort IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE = 0x0040;
    private const ushort IMAGE_DLLCHARACTERISTICS_NX_COMPAT = 0x0100;
    private const ushort IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE = 0x8000;

    private const int IMAGE_NUMBEROF_DIRECTORY_ENTRIES = 16;
    private const int DIR_IMPORT = 1;
    private const int DIR_BASERELOC = 5;

    private const uint IMAGE_BASE32 = 0x00400000;
    private const ulong IMAGE_BASE64 = 0x0000000000400000;
    private const uint SECTION_ALIGNMENT = 0x1000;
    private const uint FILE_ALIGNMENT = 0x200;

    private sealed record LinkOptions(
        string OutExe,
        string EntrySymbol,
        string Subsystem,
        bool DynamicBase,
        bool NxCompat,
        ushort TargetMachine,
        IReadOnlyList<string> Inputs,
        bool EmitMap,
        string? MapPath
    );

    private sealed record LinkMap(
        string outExe,
        string entrySymbol,
        string subsystem,
        int objectCount,
        int archiveCount,
        IReadOnlyList<string> importedDlls,
        IReadOnlyList<string> importedSymbols,
        IReadOnlyList<SectionMap> sections,
        IReadOnlyList<string> unresolved
    );

    private sealed record SectionMap(
        string name,
        uint rva,
        uint virtualSize,
        uint rawSize,
        uint rawPtr,
        string characteristics
    );

    public static int Main(string[] args)
    {
        try
        {
            args = ExpandResponseFiles(args);
            var opts = ParseArgs(args);
            var map = Link(opts);

            if (opts.EmitMap)
            {
                var mapJson = JsonSerializer.Serialize(map, new JsonSerializerOptions
                {
                    WriteIndented = true,
                    DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
                });
                var mapPath = opts.MapPath ?? (opts.OutExe + ".linkmap.json");
                File.WriteAllText(mapPath, mapJson, new UTF8Encoding(false));
                Console.WriteLine($"OK: wrote {mapPath}");
            }

            Console.WriteLine($"OK: wrote {opts.OutExe}");
            return map.unresolved.Count > 0 ? 3 : 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"rawrxd_linker: fatal: {ex}");
            return 1;
        }
    }

    private static LinkOptions ParseArgs(string[] args)
    {
        string? outExe = null;
        string? entry = null;
        var subsystem = "WINDOWS";
        var dynamicBase = true;
        var nx = true;
        var targetMachine = IMAGE_FILE_MACHINE_AMD64;
        var emitMap = false;
        string? mapPath = null;
        var inputs = new List<string>();
        var libPaths = new List<string>();
        var nodefaultlib = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        for (var i = 0; i < args.Length; i++)
        {
            var a = args[i];
            // Native CLI
            if (a.Equals("--out", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length) { outExe = args[++i]; continue; }
            if (a.Equals("--entry", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length) { entry = args[++i]; continue; }
            if (a.Equals("--subsystem", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length) { subsystem = args[++i]; continue; }
            if ((a.Equals("--machine", StringComparison.OrdinalIgnoreCase) || a.Equals("-m", StringComparison.OrdinalIgnoreCase)) && i + 1 < args.Length)
            {
                targetMachine = ParseMachineArg(args[++i]);
                continue;
            }
            if (a.Equals("--no-dynamicbase", StringComparison.OrdinalIgnoreCase)) { dynamicBase = false; continue; }
            if (a.Equals("--no-nxcompat", StringComparison.OrdinalIgnoreCase)) { nx = false; continue; }
            if (a.Equals("--map", StringComparison.OrdinalIgnoreCase)) { emitMap = true; continue; }
            if (a.Equals("--map-out", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length) { mapPath = args[++i]; emitMap = true; continue; }
            if (a.Equals("--in", StringComparison.OrdinalIgnoreCase))
            {
                for (i++; i < args.Length && !args[i].StartsWith("--", StringComparison.Ordinal); i++)
                    inputs.Add(args[i]);
                i--;
                continue;
            }

            // link.exe-compatible subset
            if (a.StartsWith("/OUT:", StringComparison.OrdinalIgnoreCase)) { outExe = Unquote(a[5..]); continue; }
            if (a.StartsWith("/ENTRY:", StringComparison.OrdinalIgnoreCase)) { entry = a[7..]; continue; }
            if (a.StartsWith("/SUBSYSTEM:", StringComparison.OrdinalIgnoreCase)) { subsystem = a[11..]; continue; }
            if (a.Equals("/DYNAMICBASE", StringComparison.OrdinalIgnoreCase)) { dynamicBase = true; continue; }
            if (a.Equals("/NXCOMPAT", StringComparison.OrdinalIgnoreCase)) { nx = true; continue; }
            if (a.Equals("/NOLOGO", StringComparison.OrdinalIgnoreCase)) { continue; }
            if (a.StartsWith("/LIBPATH:", StringComparison.OrdinalIgnoreCase)) { libPaths.Add(Unquote(a[9..])); continue; }
            if (a.StartsWith("/NODEFAULTLIB:", StringComparison.OrdinalIgnoreCase)) { nodefaultlib.Add(a[13..]); continue; }
            if (a.Equals("/LARGEADDRESSAWARE", StringComparison.OrdinalIgnoreCase)) { continue; }
            if (a.Equals("/OPT:REF", StringComparison.OrdinalIgnoreCase) || a.Equals("/OPT:ICF", StringComparison.OrdinalIgnoreCase)) { continue; }
            if (a.Equals("/INCREMENTAL:NO", StringComparison.OrdinalIgnoreCase)) { continue; }
            if (a.Equals("/LTCG", StringComparison.OrdinalIgnoreCase)) { continue; }
            if (a.StartsWith("/MACHINE:", StringComparison.OrdinalIgnoreCase))
            {
                targetMachine = ParseMachineArg(a[9..]);
                continue;
            }

            // Treat unknown switches as ignorable for now; bare args as inputs.
            if (!a.StartsWith("/", StringComparison.Ordinal) && !a.StartsWith("-", StringComparison.Ordinal))
            {
                // Handle wildcards in input files
                if (a.Contains("*"))
                {
                    var dir = Path.GetDirectoryName(a);
                    if (string.IsNullOrEmpty(dir)) dir = ".";
                    var pattern = Path.GetFileName(a);
                    var files = Directory.GetFiles(dir, pattern);
                    foreach (var file in files)
                    {
                        inputs.Add(Path.GetFullPath(file));
                    }
                }
                else
                {
                    inputs.Add(Unquote(a));
                }
            }
        }

        if (string.IsNullOrWhiteSpace(outExe)) throw new ArgumentException("Missing output exe (--out or /OUT:).");
        if (string.IsNullOrWhiteSpace(entry)) throw new ArgumentException("Missing entry symbol (--entry or /ENTRY:).");
        if (inputs.Count == 0) throw new ArgumentException("Missing inputs (--in or rsp args).");

        // Resolve relative inputs using rsp directory if present.
        var baseDir = FindResponseFileBaseDir(args) ?? Directory.GetCurrentDirectory();
        var resolvedInputs = new List<string>(inputs.Count);
        foreach (var inp in inputs)
        {
            if (Path.IsPathRooted(inp))
            {
                resolvedInputs.Add(inp);
                continue;
            }

            var candidate = Path.GetFullPath(Path.Combine(baseDir, inp));
            if (File.Exists(candidate))
            {
                resolvedInputs.Add(candidate);
                continue;
            }

            // Try libpaths for .lib only
            if (inp.EndsWith(".lib", StringComparison.OrdinalIgnoreCase))
            {
                string? found = null;
                foreach (var p in libPaths)
                {
                    try
                    {
                        var c = Path.Combine(p, inp);
                        if (File.Exists(c)) { found = c; break; }
                    }
                    catch { }
                }
                if (found != null) { resolvedInputs.Add(found); continue; }
            }

            resolvedInputs.Add(inp);
        }

        return new LinkOptions(
            OutExe: Path.GetFullPath(outExe),
            EntrySymbol: entry!,
            Subsystem: subsystem,
            DynamicBase: dynamicBase,
            NxCompat: nx,
            TargetMachine: targetMachine,
            Inputs: resolvedInputs,
            EmitMap: emitMap,
            MapPath: mapPath
        );
    }

    private static ushort ParseMachineArg(string raw)
    {
        var m = raw.Trim().ToUpperInvariant();
        return m switch
        {
            "X64" or "AMD64" => IMAGE_FILE_MACHINE_AMD64,
            "X86" or "I386" => IMAGE_FILE_MACHINE_I386,
            _ => throw new ArgumentException($"Unsupported machine '{raw}'. Use x64 or x86.")
        };
    }

    private static string Unquote(string s)
    {
        s = s.Trim();
        if (s.Length >= 2 && s[0] == '"' && s[^1] == '"') return s[1..^1];
        return s;
    }

    private static string[] ExpandResponseFiles(string[] args)
    {
        var expanded = new List<string>();
        foreach (var a in args)
        {
            if (a.Length > 1 && a[0] == '@')
            {
                var p = a[1..];
                p = Unquote(p);
                if (!File.Exists(p)) { expanded.Add(a); continue; }
                var text = File.ReadAllText(p);
                expanded.AddRange(TokenizeRsp(text));
                expanded.Add($"--__rsp_base__={Path.GetDirectoryName(Path.GetFullPath(p))}");
            }
            else
            {
                expanded.Add(a);
            }
        }
        return expanded.ToArray();
    }

    private static string? FindResponseFileBaseDir(string[] args)
    {
        var marker = args.FirstOrDefault(a => a.StartsWith("--__rsp_base__=", StringComparison.Ordinal));
        if (marker == null) return null;
        return marker["--__rsp_base__=".Length..];
    }

    private static IEnumerable<string> TokenizeRsp(string text)
    {
        // link.exe rsp: whitespace-separated, supports quotes. Newlines are whitespace.
        var tokens = new List<string>();
        var sb = new StringBuilder();
        var inQuote = false;
        for (var i = 0; i < text.Length; i++)
        {
            var ch = text[i];
            if (ch == '"')
            {
                inQuote = !inQuote;
                sb.Append(ch);
                continue;
            }
            if (!inQuote && char.IsWhiteSpace(ch))
            {
                if (sb.Length > 0)
                {
                    tokens.Add(sb.ToString());
                    sb.Clear();
                }
                continue;
            }
            sb.Append(ch);
        }
        if (sb.Length > 0) tokens.Add(sb.ToString());
        return tokens;
    }

    private sealed class CoffObject
    {
        public required string Path;
        public required CoffHeader Header;
        public required List<CoffSectionHeader> Sections;
        public required byte[] StringTable;
        public required List<CoffSymbol> SymbolsDense;
        public required List<CoffRelocation[]> RelocationsBySectionIndex;
        public required byte[] FileBytes;
    }

    private readonly struct CoffHeader
    {
        public readonly ushort Machine;
        public readonly int NumberOfSections;
        public readonly uint PointerToSymbolTable;
        public readonly uint NumberOfSymbols;
        public readonly bool IsBigObj;
        public CoffHeader(ushort machine, int numberOfSections, uint ptrSym, uint numSym, bool isBigObj)
        {
            Machine = machine;
            NumberOfSections = numberOfSections;
            PointerToSymbolTable = ptrSym;
            NumberOfSymbols = numSym;
            IsBigObj = isBigObj;
        }
    }

    private sealed class CoffSectionHeader
    {
        public required string Name;
        public required uint SizeOfRawData;
        public required uint PointerToRawData;
        public required uint PointerToRelocations;
        public required ushort NumberOfRelocations;
        public required uint Characteristics;
        public required int Index1Based;
    }

    private sealed class CoffSymbol
    {
        public required string Name;
        public required int SectionNumber; // 0 undef, >0 section
        public required uint Value;
        public required byte StorageClass;
        public required byte AuxCount;
    }

    private readonly struct CoffRelocation
    {
        public readonly uint VirtualAddress;
        public readonly uint SymbolTableIndex;
        public readonly uint Type;
        public CoffRelocation(uint va, uint symIndex, uint type)
        {
            VirtualAddress = va;
            SymbolTableIndex = symIndex;
            Type = type;
        }
    }

    private sealed record ImportSpec(string Dll, string Name);

    private sealed class ImportLibrarySymbol
    {
        public required string Name;
        public required string Dll;
        public required string ImportName;
    }

    private sealed class OutSection
    {
        public string Name { get; }
        public uint Characteristics { get; }
        public List<byte> Data { get; } = new();
        public uint Rva { get; set; }
        public uint RawPtr { get; set; }
        public OutSection(string name, uint characteristics) { Name = name; Characteristics = characteristics; }
    }

    private sealed class ImportLayout
    {
        public required uint ImportDirectorySize;
        public required Dictionary<ImportSpec, uint> IatRvaByImport;
        public required Action<List<OutSection>> FinalizeWithSectionRvas;
    }

    private static LinkMap Link(LinkOptions opts)
    {
        var inputs = opts.Inputs.Select(Path.GetFullPath).Distinct(StringComparer.OrdinalIgnoreCase).ToList();

        var objects = new List<CoffObject>();
        var importSymbols = new Dictionary<string, ImportLibrarySymbol>(StringComparer.Ordinal);
        var archiveCount = 0;

        foreach (var input in inputs)
        {
            if (input.EndsWith(".obj", StringComparison.OrdinalIgnoreCase))
            {
                objects.Add(ParseObj(input));
                continue;
            }

            if (input.EndsWith(".lib", StringComparison.OrdinalIgnoreCase))
            {
                archiveCount++;
                foreach (var sym in ParseImportLibrarySymbols(input, opts.TargetMachine))
                    importSymbols.TryAdd(sym.Name, sym);
                objects.AddRange(ExtractCoffObjectsFromLib(input, opts.TargetMachine));
                continue;
            }

            if (input.EndsWith(".res", StringComparison.OrdinalIgnoreCase))
                throw new NotSupportedException("Resources (.res) not supported yet.");

            throw new ArgumentException($"Unsupported input: {input}");
        }

        if (objects.Count == 0) throw new ArgumentException("No .obj inputs.");
        Directory.CreateDirectory(Path.GetDirectoryName(opts.OutExe)!);

        var defined = new Dictionary<string, (CoffObject obj, CoffSymbol sym)>(StringComparer.Ordinal);
        var undefined = new HashSet<string>(StringComparer.Ordinal);

        foreach (var obj in objects)
        {
            if (obj.Header.Machine != opts.TargetMachine)
                throw new NotSupportedException($"Object machine mismatch: {obj.Path} machine=0x{obj.Header.Machine:X4}, target=0x{opts.TargetMachine:X4}");

            foreach (var sym in obj.SymbolsDense)
            {
                if (sym.StorageClass != 2) continue; // external only
                if (sym.SectionNumber > 0) defined.TryAdd(sym.Name, (obj, sym));
                else if (sym.SectionNumber == 0) undefined.Add(sym.Name);
            }
        }

        var imports = new Dictionary<string, ImportSpec>(StringComparer.Ordinal);
        var unresolved = new List<string>();
        foreach (var u in undefined)
        {
            if (defined.ContainsKey(u)) continue;
            if (importSymbols.TryGetValue(u, out var imp))
                imports[u] = new ImportSpec(imp.Dll, imp.ImportName);
            else
                unresolved.Add(u);
        }

        if (!defined.TryGetValue(opts.EntrySymbol, out var entryDef))
            throw new ArgumentException($"Entry symbol not defined in objs: {opts.EntrySymbol}");

        var outSections = new List<OutSection>();
        OutSection GetOrCreate(string name, uint characteristics)
        {
            var existing = outSections.FirstOrDefault(s => s.Name == name);
            if (existing != null) return existing;
            var sec = new OutSection(name, characteristics);
            outSections.Add(sec);
            return sec;
        }

        var placement = new Dictionary<(CoffObject obj, int secIndex1), (OutSection outSec, uint outOffset)>();

        foreach (var obj in objects)
        {
            foreach (var sec in obj.Sections)
            {
                if (sec.Name.StartsWith(".debug", StringComparison.OrdinalIgnoreCase)) continue;
                if (sec.Name.Equals(".drectve", StringComparison.OrdinalIgnoreCase)) continue;

                var outName = NormalizeOutSectionName(sec.Name);
                var outChars = NormalizeCharacteristics(sec.Characteristics);
                var outSec = GetOrCreate(outName, outChars);

                byte[] raw;
                try { raw = ReadSectionBytes(obj, sec); }
                catch (InvalidDataException ex) {
                    Console.Error.WriteLine($"rawrxd_linker: warn: skipping corrupt section '{sec.Name}' in {obj.Path}: {ex.Message}");
                    continue;
                }
                var alignedOffset = Align(outSec.Data.Count, 16);
                if (alignedOffset > outSec.Data.Count)
                    outSec.Data.AddRange(new byte[alignedOffset - outSec.Data.Count]);
                var outOffset = (uint)outSec.Data.Count;
                outSec.Data.AddRange(raw);

                placement[(obj, sec.Index1Based)] = (outSec, outOffset);
            }
        }

        var rdata = GetOrCreate(".rdata", IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ);
        var idata = GetOrCreate(".idata", IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE);
        var relocSec = GetOrCreate(".reloc", IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ);

        Dictionary<ImportSpec, int>? thunkOffsets = null;
        if (imports.Count > 0)
        {
            var text = GetOrCreate(".text", IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);
            thunkOffsets = ReserveImportThunks(text, imports.Values.Distinct());
        }

        outSections = outSections.OrderBy(s => SectionOrderKey(s.Name)).ToList();
        LayoutSections(outSections);

        var baseRelocRvas = new List<uint>();
        ApplyInternalRelocations(objects, defined, imports, placement, baseRelocRvas, opts.TargetMachine);

        var importSec = opts.TargetMachine == IMAGE_FILE_MACHINE_I386 ? idata : rdata;
        var importLayout = BuildImports(importSec, imports.Values.Distinct().OrderBy(i => i.Dll, StringComparer.OrdinalIgnoreCase).ThenBy(i => i.Name, StringComparer.Ordinal).ToList(), opts.TargetMachine);
        importLayout.FinalizeWithSectionRvas(outSections);

        outSections = outSections.Where(s => s.Data.Count > 0).OrderBy(s => SectionOrderKey(s.Name)).ToList();
        LayoutSections(outSections);
        importLayout.FinalizeWithSectionRvas(outSections);

        var thunkRvaByImport = (thunkOffsets != null && importLayout.IatRvaByImport.Count > 0)
            ? PatchImportThunks(outSections, thunkOffsets, importLayout.IatRvaByImport, opts.TargetMachine, baseRelocRvas)
            : new Dictionary<ImportSpec, uint>();

        PatchImportRelocations(objects, imports, importLayout.IatRvaByImport, thunkRvaByImport, placement, baseRelocRvas, opts.TargetMachine);

        if (opts.DynamicBase && baseRelocRvas.Count > 0)
        {
            BuildBaseRelocs(relocSec, baseRelocRvas, opts.TargetMachine);
            if (!outSections.Contains(relocSec) && relocSec.Data.Count > 0)
            {
                outSections.Add(relocSec);
                outSections = outSections.OrderBy(s => SectionOrderKey(s.Name)).ToList();
                LayoutSections(outSections);
                importLayout.FinalizeWithSectionRvas(outSections);
                if (thunkOffsets != null && importLayout.IatRvaByImport.Count > 0)
                    thunkRvaByImport = PatchImportThunks(outSections, thunkOffsets, importLayout.IatRvaByImport, opts.TargetMachine, baseRelocRvas: new List<uint>());
            }
        }

        var entryPlaced = placement[(entryDef.obj, entryDef.sym.SectionNumber)];
        var entryRva = entryPlaced.outSec.Rva + entryPlaced.outOffset + entryDef.sym.Value;

        var hasRelocs = opts.DynamicBase && outSections.Any(s => s.Name == ".reloc" && s.Data.Count > 0);
        WritePe(opts, outSections, entryRva, hasRelocs, imports, opts.TargetMachine);

        var importedDlls = imports.Values.Select(i => i.Dll).Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(s => s, StringComparer.OrdinalIgnoreCase).ToList();
        var importedSyms = imports.Keys.OrderBy(s => s, StringComparer.Ordinal).ToList();
        var sectionMaps = outSections.Select(s => new SectionMap(
            name: s.Name,
            rva: s.Rva,
            virtualSize: (uint)s.Data.Count,
            rawSize: Align((uint)s.Data.Count, FILE_ALIGNMENT),
            rawPtr: s.RawPtr,
            characteristics: $"0x{s.Characteristics:X8}"
        )).ToList();

        return new LinkMap(
            outExe: opts.OutExe,
            entrySymbol: opts.EntrySymbol,
            subsystem: opts.Subsystem,
            objectCount: objects.Count,
            archiveCount: archiveCount,
            importedDlls: importedDlls,
            importedSymbols: importedSyms,
            sections: sectionMaps,
            unresolved: unresolved.OrderBy(s => s, StringComparer.Ordinal).ToList()
        );
    }

    private static uint NormalizeCharacteristics(uint c)
    {
        var outC = 0u;
        if ((c & IMAGE_SCN_CNT_CODE) != 0) outC |= IMAGE_SCN_CNT_CODE;
        if ((c & IMAGE_SCN_CNT_INITIALIZED_DATA) != 0) outC |= IMAGE_SCN_CNT_INITIALIZED_DATA;
        if ((c & IMAGE_SCN_CNT_UNINITIALIZED_DATA) != 0) outC |= IMAGE_SCN_CNT_UNINITIALIZED_DATA;
        if ((c & IMAGE_SCN_MEM_EXECUTE) != 0) outC |= IMAGE_SCN_MEM_EXECUTE;
        if ((c & IMAGE_SCN_MEM_READ) != 0) outC |= IMAGE_SCN_MEM_READ;
        if ((c & IMAGE_SCN_MEM_WRITE) != 0) outC |= IMAGE_SCN_MEM_WRITE;
        if (outC == 0) outC = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
        return outC;
    }

    private static string NormalizeOutSectionName(string inName)
    {
        if (inName.StartsWith(".text", StringComparison.OrdinalIgnoreCase)) return ".text";
        if (inName.StartsWith(".rdata", StringComparison.OrdinalIgnoreCase)) return ".rdata";
        if (inName.StartsWith(".data", StringComparison.OrdinalIgnoreCase)) return ".data";
        if (inName.StartsWith(".pdata", StringComparison.OrdinalIgnoreCase)) return ".pdata";
        if (inName.StartsWith(".xdata", StringComparison.OrdinalIgnoreCase)) return ".xdata";
        if (inName.StartsWith(".idata", StringComparison.OrdinalIgnoreCase)) return ".idata";
        if (inName.StartsWith(".", StringComparison.Ordinal)) return inName;
        if (inName.Equals("_RDATA", StringComparison.OrdinalIgnoreCase)) return ".rdata";
        if (inName.Equals("_DATA64", StringComparison.OrdinalIgnoreCase)) return ".data";
        if (inName.Equals("TELEMETR", StringComparison.OrdinalIgnoreCase)) return ".rdata";
        return "." + inName.ToLowerInvariant();
    }

    private static int SectionOrderKey(string name) => name switch
    {
        ".text" => 0,
        ".rdata" => 1,
        ".data" => 2,
        ".idata" => 3,
        ".pdata" => 3,
        ".xdata" => 4,
        ".rsrc" => 6,
        ".reloc" => 7,
        _ => 100
    };

    private static void LayoutSections(List<OutSection> sections)
    {
        var rva = 0x1000u;
        foreach (var s in sections)
        {
            s.Rva = Align(rva, SECTION_ALIGNMENT);
            rva = s.Rva + Align((uint)s.Data.Count, SECTION_ALIGNMENT);
        }
    }

    private static uint Align(uint v, uint a) => (v + (a - 1)) & ~(a - 1);
    private static int Align(int v, int a) => (v + (a - 1)) & ~(a - 1);

    private static byte[] ReadSectionBytes(CoffObject obj, CoffSectionHeader sec)
    {
        if (sec.SizeOfRawData == 0) return Array.Empty<byte>();
        var end = sec.PointerToRawData + sec.SizeOfRawData;
        
        const uint SECTION_THRESHOLD = 0x7FFF0000; // 2GB - 64KB safety margin
        if (sec.SizeOfRawData > SECTION_THRESHOLD) {
            // Split into multiple PE sections (stubbed for now)
            // In a real implementation, we would chunk the data here
        }
        
        if (end > obj.FileBytes.Length) throw new InvalidDataException($"Section out of range in {obj.Path}.");
        var bytes = new byte[sec.SizeOfRawData];
        Buffer.BlockCopy(obj.FileBytes, checked((int)sec.PointerToRawData), bytes, 0, bytes.Length);
        return bytes;
    }

    private static void EnsureCapacity(List<byte> buf, int needed)
    {
        if (buf.Count >= needed) return;
        buf.AddRange(new byte[needed - buf.Count]);
    }

    private static void ApplyInternalRelocations(
        List<CoffObject> objects,
        Dictionary<string, (CoffObject obj, CoffSymbol sym)> defined,
        Dictionary<string, ImportSpec> imports,
        Dictionary<(CoffObject obj, int secIndex1), (OutSection outSec, uint outOffset)> placement,
        List<uint> baseRelocRvas,
        ushort machine)
    {
        foreach (var obj in objects)
        {
            for (var i = 0; i < obj.Sections.Count; i++)
            {
                var sec = obj.Sections[i];
                var relocs = obj.RelocationsBySectionIndex[i];
                if (relocs.Length == 0) continue;
                if (!placement.TryGetValue((obj, sec.Index1Based), out var placed)) continue;
                foreach (var rel in relocs)
                {
                    var sym = obj.SymbolsDense[checked((int)rel.SymbolTableIndex)];
                    if (imports.ContainsKey(sym.Name)) continue; // patch later

                    if (!TryResolveSymbolRva(sym, obj, defined, placement, out var targetRva))
                        continue;

                    var patchRva = placed.outSec.Rva + placed.outOffset + rel.VirtualAddress;
                    var patchOffset = checked((int)(placed.outOffset + rel.VirtualAddress));

                    switch (rel.Type)
                    {
                        case IMAGE_REL_AMD64_ADDR64:
                            if (machine != IMAGE_FILE_MACHINE_AMD64) break;
                            EnsureCapacity(placed.outSec.Data, patchOffset + 8);
                            var va = IMAGE_BASE64 + targetRva;
                            BinaryPrimitives.WriteUInt64LittleEndian(CollectionsMarshal.AsSpan(placed.outSec.Data).Slice(patchOffset, 8), va);
                            baseRelocRvas.Add(patchRva);
                            break;
                        case IMAGE_REL_AMD64_REL32:
                            if (machine != IMAGE_FILE_MACHINE_AMD64) break;
                            EnsureCapacity(placed.outSec.Data, patchOffset + 4);
                            var rel32Amd = unchecked((int)targetRva - (int)(patchRva + 4));
                            BinaryPrimitives.WriteInt32LittleEndian(CollectionsMarshal.AsSpan(placed.outSec.Data).Slice(patchOffset, 4), rel32Amd);
                            break;
                        case IMAGE_REL_I386_DIR32:
                            if (machine != IMAGE_FILE_MACHINE_I386) break;
                            EnsureCapacity(placed.outSec.Data, patchOffset + 4);
                            var va32 = IMAGE_BASE32 + targetRva;
                            BinaryPrimitives.WriteUInt32LittleEndian(CollectionsMarshal.AsSpan(placed.outSec.Data).Slice(patchOffset, 4), va32);
                            baseRelocRvas.Add(patchRva);
                            break;
                        case IMAGE_REL_I386_REL32:
                            if (machine != IMAGE_FILE_MACHINE_I386) break;
                            EnsureCapacity(placed.outSec.Data, patchOffset + 4);
                            var rel32I386 = unchecked((int)targetRva - (int)(patchRva + 4));
                            BinaryPrimitives.WriteInt32LittleEndian(CollectionsMarshal.AsSpan(placed.outSec.Data).Slice(patchOffset, 4), rel32I386);
                            break;
                    }
                }
            }
        }
    }

    private static void PatchImportRelocations(
        List<CoffObject> objects,
        Dictionary<string, ImportSpec> importsBySymbol,
        Dictionary<ImportSpec, uint> iatRvaByImport,
        Dictionary<ImportSpec, uint> thunkRvaByImport,
        Dictionary<(CoffObject obj, int secIndex1), (OutSection outSec, uint outOffset)> placement,
        List<uint> baseRelocRvas,
        ushort machine)
    {
        var targetBySymbol = new Dictionary<string, uint>(StringComparer.Ordinal);
        foreach (var kv in importsBySymbol)
        {
            if (!iatRvaByImport.TryGetValue(kv.Value, out var iatRva)) continue;

            // If caller references __imp_foo, they want the IAT slot address.
            // Otherwise, they usually expect a callable thunk (jmp [IAT]).
            if (kv.Key.StartsWith("__imp_", StringComparison.Ordinal))
            {
                targetBySymbol[kv.Key] = iatRva;
            }
            else if (thunkRvaByImport.TryGetValue(kv.Value, out var thunkRva))
            {
                targetBySymbol[kv.Key] = thunkRva;
            }
            else
            {
                targetBySymbol[kv.Key] = iatRva;
            }
        }

        foreach (var obj in objects)
        {
            for (var i = 0; i < obj.Sections.Count; i++)
            {
                var sec = obj.Sections[i];
                var relocs = obj.RelocationsBySectionIndex[i];
                if (relocs.Length == 0) continue;
                if (!placement.TryGetValue((obj, sec.Index1Based), out var placed)) continue;

                foreach (var rel in relocs)
                {
                    var sym = obj.SymbolsDense[checked((int)rel.SymbolTableIndex)];
                    if (!targetBySymbol.TryGetValue(sym.Name, out var targetRva)) continue;

                    var patchRva = placed.outSec.Rva + placed.outOffset + rel.VirtualAddress;
                    var patchOffset = checked((int)(placed.outOffset + rel.VirtualAddress));

                    switch (rel.Type)
                    {
                        case IMAGE_REL_AMD64_ADDR64:
                            if (machine != IMAGE_FILE_MACHINE_AMD64) break;
                            EnsureCapacity(placed.outSec.Data, patchOffset + 8);
                            var va = IMAGE_BASE64 + targetRva;
                            BinaryPrimitives.WriteUInt64LittleEndian(CollectionsMarshal.AsSpan(placed.outSec.Data).Slice(patchOffset, 8), va);
                            baseRelocRvas.Add(patchRva);
                            break;
                        case IMAGE_REL_AMD64_REL32:
                            if (machine != IMAGE_FILE_MACHINE_AMD64) break;
                            EnsureCapacity(placed.outSec.Data, patchOffset + 4);
                            var rel32Amd = unchecked((int)targetRva - (int)(patchRva + 4));
                            BinaryPrimitives.WriteInt32LittleEndian(CollectionsMarshal.AsSpan(placed.outSec.Data).Slice(patchOffset, 4), rel32Amd);
                            break;
                        case IMAGE_REL_I386_DIR32:
                            if (machine != IMAGE_FILE_MACHINE_I386) break;
                            EnsureCapacity(placed.outSec.Data, patchOffset + 4);
                            var va32 = IMAGE_BASE32 + targetRva;
                            BinaryPrimitives.WriteUInt32LittleEndian(CollectionsMarshal.AsSpan(placed.outSec.Data).Slice(patchOffset, 4), va32);
                            baseRelocRvas.Add(patchRva);
                            break;
                        case IMAGE_REL_I386_REL32:
                            if (machine != IMAGE_FILE_MACHINE_I386) break;
                            EnsureCapacity(placed.outSec.Data, patchOffset + 4);
                            var rel32I386 = unchecked((int)targetRva - (int)(patchRva + 4));
                            BinaryPrimitives.WriteInt32LittleEndian(CollectionsMarshal.AsSpan(placed.outSec.Data).Slice(patchOffset, 4), rel32I386);
                            break;
                    }
                }
            }
        }
    }

    private static bool TryResolveSymbolRva(
        CoffSymbol sym,
        CoffObject curObj,
        Dictionary<string, (CoffObject obj, CoffSymbol sym)> defined,
        Dictionary<(CoffObject obj, int secIndex1), (OutSection outSec, uint outOffset)> placement,
        out uint targetRva)
    {
        if (sym.SectionNumber > 0)
        {
            if (!placement.TryGetValue((curObj, sym.SectionNumber), out var curPlaced))
            {
                targetRva = 0;
                return false; // section was skipped (corrupt)
            }
            targetRva = curPlaced.outSec.Rva + curPlaced.outOffset + sym.Value;
            return true;
        }

        if (defined.TryGetValue(sym.Name, out var def))
        {
            if (!placement.TryGetValue((def.obj, def.sym.SectionNumber), out var p))
            {
                targetRva = 0;
                return false; // section was skipped (corrupt)
            }
            targetRva = p.outSec.Rva + p.outOffset + def.sym.Value;
            return true;
        }

        targetRva = 0;
        return false;
    }

    private static CoffObject ParseObj(string path)
    {
        var bytes = File.ReadAllBytes(path);
        return ParseObjFromBytes(path, bytes);
    }

    private static CoffObject ParseObjFromBytes(string path, byte[] bytes)
    {
        if (bytes.Length < 20) throw new InvalidDataException("Too small for COFF.");

        var sig1 = BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(0, 2));
        var sig2 = BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(2, 2));

        ushort machine;
        int nsec;
        uint ptrSym;
        uint nSym;
        var isBigObj = false;
        var sectionHeadersOffset = 20;

        if (sig1 == 0x0000 && sig2 == 0xFFFF)
        {
            // MS bigobj/anonymous object (variant used by MSVC for /LTCG and large section tables).
            // Observed in this repo: section headers start at 0x34, ptr-to-symbol-table at 0x28.
            if (bytes.Length < 0x34 + 40) throw new InvalidDataException("Too small for bigobj header.");
            isBigObj = true;
            machine = BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(6, 2));
            ptrSym = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(0x28, 4));
            nSym = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(0x2C, 4));
            sectionHeadersOffset = 0x34;
            if (ptrSym < sectionHeadersOffset || ptrSym > bytes.Length)
                throw new InvalidDataException($"Invalid bigobj PtrToSymbolTable=0x{ptrSym:X8} size={bytes.Length} in {path}");
            var sectionBytes = ptrSym - (uint)sectionHeadersOffset;
            if ((sectionBytes % 40u) != 0)
                throw new InvalidDataException($"Invalid bigobj section header span in {path}: span=0x{sectionBytes:X} not divisible by 40");
            nsec = checked((int)(sectionBytes / 40u));
        }
        else
        {
            machine = sig1;
            nsec = BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(2, 2));
            ptrSym = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(8, 4));
            nSym = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(12, 4));
            var opt = BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(16, 2));
            if (opt != 0) throw new NotSupportedException("COFF optional header size != 0 (unsupported).");
        }

        var stringTable = ReadStringTable(bytes, ptrSym, nSym);

        var sections = new List<CoffSectionHeader>(nsec);
        for (var i = 0; i < nsec; i++)
        {
            var off = sectionHeadersOffset + i * 40;
            var name = DecodeName(bytes.AsSpan(off, 8), stringTable);
            var sizeRaw = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(off + 16, 4));
            var ptrRaw = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(off + 20, 4));
            var ptrRel = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(off + 24, 4));
            var nRel = BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(off + 32, 2));
            var characteristics = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(off + 36, 4));
            sections.Add(new CoffSectionHeader
            {
                Name = name,
                SizeOfRawData = sizeRaw,
                PointerToRawData = ptrRaw,
                PointerToRelocations = ptrRel,
                NumberOfRelocations = nRel,
                Characteristics = characteristics,
                Index1Based = i + 1
            });
        }

        var symbolsDense = ReadSymbolsDense(bytes, ptrSym, nSym, stringTable);
        var relocsBySection = new List<CoffRelocation[]>(nsec);
        foreach (var sec in sections)
        {
            var relocs = new CoffRelocation[sec.NumberOfRelocations];
            if (sec.NumberOfRelocations != 0)
            {
                var baseOff = checked((int)sec.PointerToRelocations);
                var bytesNeeded = baseOff + (sec.NumberOfRelocations * 10);
                if (baseOff < 0 || baseOff > bytes.Length || bytesNeeded < 0 || bytesNeeded > bytes.Length)
                    throw new InvalidDataException($"Invalid relocation table range in {path} section {sec.Name}: ptr=0x{sec.PointerToRelocations:X8} count={sec.NumberOfRelocations} size={bytes.Length}");
                for (var i = 0; i < sec.NumberOfRelocations; i++)
                {
                    var roff = baseOff + i * 10;
                    var va = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(roff, 4));
                    var symIndex = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(roff + 4, 4));
                    var type = BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(roff + 8, 2));
                    relocs[i] = new CoffRelocation(va, symIndex, type);
                }
            }
            relocsBySection.Add(relocs);
        }

        return new CoffObject
        {
            Path = path,
            Header = new CoffHeader(machine, nsec, ptrSym, nSym, isBigObj),
            Sections = sections,
            StringTable = stringTable,
            SymbolsDense = symbolsDense,
            RelocationsBySectionIndex = relocsBySection,
            FileBytes = bytes
        };
    }

    private static List<CoffObject> ExtractCoffObjectsFromLib(string libPath, ushort machineFilter)
    {
        var results = new List<CoffObject>();
        var bytes = File.ReadAllBytes(libPath);
        if (bytes.Length < 8) return results;
        if (Encoding.ASCII.GetString(bytes.AsSpan(0, 8)) != "!<arch>\n") return results;

        var off = 8;
        var memberIndex = 0;
        while (off + 60 <= bytes.Length)
        {
            var header = bytes.AsSpan(off, 60);
            var sizeStr = Encoding.ASCII.GetString(header.Slice(48, 10)).Trim();
            if (!int.TryParse(sizeStr, out var size) || size < 0) break;
            var dataOff = off + 60;
            if (dataOff + size > bytes.Length) break;

            var member = bytes.AsSpan(dataOff, size);
            // Heuristic: COFF object member starts with target machine and SizeOfOptionalHeader=0.
            if (member.Length >= 20)
            {
                var machine = BinaryPrimitives.ReadUInt16LittleEndian(member.Slice(0, 2));
                var opt = BinaryPrimitives.ReadUInt16LittleEndian(member.Slice(16, 2));
                if (machine == machineFilter && opt == 0)
                {
                    try
                    {
                        var memberBytes = member.ToArray();
                        results.Add(ParseObjFromBytes($"{libPath}#{memberIndex}", memberBytes));
                    }
                    catch
                    {
                        // ignore non-standard members
                    }
                }
            }

            off = dataOff + size;
            if ((off & 1) == 1) off++;
            memberIndex++;
        }

        return results;
    }

    private static byte[] ReadStringTable(byte[] obj, uint ptrSym, uint nSym)
    {
        if (ptrSym == 0 || nSym == 0) return Array.Empty<byte>();
        var stringOff = ptrSym + nSym * 18u;
        if (stringOff + 4 > obj.Length) return Array.Empty<byte>();
        var len = BinaryPrimitives.ReadUInt32LittleEndian(obj.AsSpan(checked((int)stringOff), 4));
        if (len < 4 || stringOff + len > obj.Length) return Array.Empty<byte>();
        var bytes = new byte[len];
        Buffer.BlockCopy(obj, checked((int)stringOff), bytes, 0, bytes.Length);
        return bytes;
    }

    private static List<CoffSymbol> ReadSymbolsDense(byte[] obj, uint ptrSym, uint nSym, byte[] stringTable)
    {
        var dense = new CoffSymbol[checked((int)nSym)];
        if (ptrSym == 0 || nSym == 0) return dense.ToList();

        var baseOff = checked((int)ptrSym);
        for (var i = 0u; i < nSym;)
        {
            var off = baseOff + checked((int)(i * 18));
            var name = DecodeSymbolName(obj.AsSpan(off, 8), stringTable);
            var value = BinaryPrimitives.ReadUInt32LittleEndian(obj.AsSpan(off + 8, 4));
            var secNum = BinaryPrimitives.ReadInt16LittleEndian(obj.AsSpan(off + 12, 2));
            var storage = obj[off + 16];
            var auxCount = obj[off + 17];

            var sym = new CoffSymbol
            {
                Name = name,
                SectionNumber = secNum,
                Value = value,
                StorageClass = storage,
                AuxCount = auxCount
            };
            dense[i] = sym;
            for (var a = 0; a < auxCount; a++)
            {
                dense[i + 1 + (uint)a] = new CoffSymbol
                {
                    Name = name,
                    SectionNumber = secNum,
                    Value = value,
                    StorageClass = storage,
                    AuxCount = 0
                };
            }
            i += 1u + auxCount;
        }

        return dense.ToList();
    }

    private static string DecodeName(ReadOnlySpan<byte> raw8, byte[] stringTable)
    {
        var s = Encoding.ASCII.GetString(raw8).TrimEnd('\0');
        if (s.Length == 0) return "";
        if (s[0] == '/' && stringTable.Length >= 4 && int.TryParse(s.AsSpan(1), out var offset))
        {
            if (offset > 0 && offset < stringTable.Length)
            {
                var end = offset;
                while (end < stringTable.Length && stringTable[end] != 0) end++;
                return Encoding.ASCII.GetString(stringTable, offset, end - offset);
            }
        }
        return s;
    }

    private static string DecodeSymbolName(ReadOnlySpan<byte> raw8, byte[] stringTable)
    {
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

    private static List<ImportLibrarySymbol> ParseImportLibrarySymbols(string libPath, ushort machineFilter)
    {
        var results = new List<ImportLibrarySymbol>();
        var bytes = File.ReadAllBytes(libPath);
        if (bytes.Length < 8) return results;
        if (Encoding.ASCII.GetString(bytes.AsSpan(0, 8)) != "!<arch>\n") return results;

        var off = 8;
        while (off + 60 <= bytes.Length)
        {
            var header = bytes.AsSpan(off, 60);
            var sizeStr = Encoding.ASCII.GetString(header.Slice(48, 10)).Trim();
            if (!int.TryParse(sizeStr, out var size) || size < 0) break;
            var dataOff = off + 60;
            if (dataOff + size > bytes.Length) break;

            var member = bytes.AsSpan(dataOff, size);
            results.AddRange(TryParseImportObject(member, machineFilter));

            off = dataOff + size;
            if ((off & 1) == 1) off++;
        }

        return results;
    }

    private static List<ImportLibrarySymbol> TryParseImportObject(ReadOnlySpan<byte> member, ushort machineFilter)
    {
        var results = new List<ImportLibrarySymbol>();
        // MS import object header:
        // Sig1=0xFFFF, Sig2=0x0000, Version, Machine, TimeDateStamp, SizeOfData, Ordinal/Hint, Type, NameType, Reserved.
        if (member.Length < 20) return results;
        var sig1 = BinaryPrimitives.ReadUInt16LittleEndian(member.Slice(0, 2));
        var sig2 = BinaryPrimitives.ReadUInt16LittleEndian(member.Slice(2, 2));
        var isImportObj = (sig1 == 0xFFFF && sig2 == 0x0000) || (sig1 == 0x0000 && sig2 == 0xFFFF);
        if (!isImportObj) return results;
        var machine = BinaryPrimitives.ReadUInt16LittleEndian(member.Slice(6, 2));
        if (machine != machineFilter) return results;
        var sizeOfData = BinaryPrimitives.ReadUInt32LittleEndian(member.Slice(12, 4));
        if (20 + sizeOfData > member.Length) return results;

        var strOff = 20;
        var importName = ReadZ(member, ref strOff);
        var dll = ReadZ(member, ref strOff);
        if (string.IsNullOrWhiteSpace(importName) || string.IsNullOrWhiteSpace(dll)) return results;

        var peImportName = NormalizeImportNameForPe(importName, machineFilter);
        results.Add(new ImportLibrarySymbol { Name = importName, Dll = dll, ImportName = peImportName });
        if (!importName.StartsWith("__imp_", StringComparison.Ordinal))
            results.Add(new ImportLibrarySymbol { Name = "__imp_" + importName, Dll = dll, ImportName = peImportName });
        return results;
    }

    private static string NormalizeImportNameForPe(string importName, ushort machine)
    {
        var s = importName;
        if (s.StartsWith("__imp_", StringComparison.Ordinal)) s = s["__imp_".Length..];
        if (machine != IMAGE_FILE_MACHINE_I386) return s;
        if (s.StartsWith("_", StringComparison.Ordinal)) s = s[1..];
        var at = s.LastIndexOf('@');
        if (at > 0)
        {
            var suffix = s[(at + 1)..];
            if (suffix.Length > 0 && suffix.All(char.IsDigit))
                s = s[..at];
        }
        return s;
    }

    private static string ReadZ(ReadOnlySpan<byte> buf, ref int off)
    {
        var start = off;
        while (off < buf.Length && buf[off] != 0) off++;
        var s = Encoding.ASCII.GetString(buf.Slice(start, off - start));
        if (off < buf.Length && buf[off] == 0) off++;
        return s;
    }

    private static ImportLayout BuildImports(OutSection importSec, List<ImportSpec> imports, ushort machine)
    {
        if (imports.Count == 0)
        {
            return new ImportLayout
            {
                ImportDirectorySize = 0,
                IatRvaByImport = new(),
                FinalizeWithSectionRvas = _ => { }
            };
        }
        var thunkSize = machine == IMAGE_FILE_MACHINE_AMD64 ? 8 : 4;

        var byDll = imports.GroupBy(i => i.Dll, StringComparer.OrdinalIgnoreCase)
            .OrderBy(g => g.Key, StringComparer.OrdinalIgnoreCase)
            .Select(g => new { Dll = g.Key, Imports = g.OrderBy(x => x.Name, StringComparer.Ordinal).ToList() })
            .ToList();

        var start = Align(importSec.Data.Count, 8);
        if (start > importSec.Data.Count) importSec.Data.AddRange(new byte[start - importSec.Data.Count]);

        var importDirOffset = importSec.Data.Count;
        importSec.Data.AddRange(new byte[(byDll.Count + 1) * 20]); // IMAGE_IMPORT_DESCRIPTOR[...], last null

        var dllNameOffsets = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        var hintNameOffsets = new Dictionary<ImportSpec, int>();
        var iltOffsets = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        var iatOffsets = new Dictionary<ImportSpec, int>();

        foreach (var dllGroup in byDll)
        {
            dllNameOffsets[dllGroup.Dll] = importSec.Data.Count;
            WriteAsciiZ(importSec.Data, dllGroup.Dll);
        }

        foreach (var dllGroup in byDll)
        {
            foreach (var imp in dllGroup.Imports)
            {
                hintNameOffsets[imp] = importSec.Data.Count;
                importSec.Data.AddRange(new byte[2]); // Hint=0
                WriteAsciiZ(importSec.Data, NormalizeImportNameForPe(imp.Name, machine));
                while ((importSec.Data.Count & 1) == 1) importSec.Data.Add(0);
            }
        }

        foreach (var dllGroup in byDll)
        {
            iltOffsets[dllGroup.Dll] = importSec.Data.Count;
            foreach (var _ in dllGroup.Imports) importSec.Data.AddRange(new byte[thunkSize]);
            importSec.Data.AddRange(new byte[thunkSize]); // null
        }

        foreach (var dllGroup in byDll)
        {
            foreach (var imp in dllGroup.Imports)
            {
                iatOffsets[imp] = importSec.Data.Count;
                importSec.Data.AddRange(new byte[thunkSize]);
            }
            importSec.Data.AddRange(new byte[thunkSize]); // null
        }

        var iatRvaByImport = new Dictionary<ImportSpec, uint>();

        void Finalize(List<OutSection> sections)
        {
            var importRva = importSec.Rva;

            for (var i = 0; i < byDll.Count; i++)
            {
                var g = byDll[i];
                var descOff = importDirOffset + i * 20;

                var iltRva = importRva + (uint)iltOffsets[g.Dll];
                var nameRva = importRva + (uint)dllNameOffsets[g.Dll];
                var firstThunkRva = importRva + (uint)iatOffsets[g.Imports[0]];

                BinaryPrimitives.WriteUInt32LittleEndian(CollectionsMarshal.AsSpan(importSec.Data).Slice(descOff + 0, 4), iltRva);
                BinaryPrimitives.WriteUInt32LittleEndian(CollectionsMarshal.AsSpan(importSec.Data).Slice(descOff + 12, 4), nameRva);
                BinaryPrimitives.WriteUInt32LittleEndian(CollectionsMarshal.AsSpan(importSec.Data).Slice(descOff + 16, 4), firstThunkRva);

                for (var j = 0; j < g.Imports.Count; j++)
                {
                    var imp = g.Imports[j];
                    var hnRva = importRva + (uint)hintNameOffsets[imp];

                    var iltOff = iltOffsets[g.Dll] + j * thunkSize;
                    var iatOff = iatOffsets[imp];

                    if (machine == IMAGE_FILE_MACHINE_AMD64)
                    {
                        BinaryPrimitives.WriteUInt64LittleEndian(CollectionsMarshal.AsSpan(importSec.Data).Slice(iltOff, 8), hnRva);
                        BinaryPrimitives.WriteUInt64LittleEndian(CollectionsMarshal.AsSpan(importSec.Data).Slice(iatOff, 8), hnRva);
                    }
                    else
                    {
                        BinaryPrimitives.WriteUInt32LittleEndian(CollectionsMarshal.AsSpan(importSec.Data).Slice(iltOff, 4), hnRva);
                        BinaryPrimitives.WriteUInt32LittleEndian(CollectionsMarshal.AsSpan(importSec.Data).Slice(iatOff, 4), hnRva);
                    }
                    iatRvaByImport[imp] = importRva + (uint)iatOff;
                }
            }
        }

        return new ImportLayout
        {
            ImportDirectorySize = (uint)((byDll.Count + 1) * 20),
            IatRvaByImport = iatRvaByImport,
            FinalizeWithSectionRvas = Finalize
        };
    }

    private static void WriteAsciiZ(List<byte> buf, string s)
    {
        buf.AddRange(Encoding.ASCII.GetBytes(s));
        buf.Add(0);
    }

    private static void BuildBaseRelocs(OutSection reloc, List<uint> rvas, ushort machine)
    {
        var relocType = machine == IMAGE_FILE_MACHINE_AMD64 ? (ushort)0xA : (ushort)0x3; // DIR64 / HIGHLOW
        rvas.Sort();
        foreach (var g in rvas.GroupBy(r => r & 0xFFFFF000u))
        {
            var page = g.Key;
            var entries = g.Select(r => (ushort)(((uint)relocType << 12) | (r & 0xFFFu)))
                .Distinct()
                .OrderBy(x => x)
                .ToList();

            var blockSize = 8 + entries.Count * 2;
            var baseOff = reloc.Data.Count;
            reloc.Data.AddRange(new byte[8]);
            BinaryPrimitives.WriteUInt32LittleEndian(CollectionsMarshal.AsSpan(reloc.Data).Slice(baseOff + 0, 4), page);
            BinaryPrimitives.WriteUInt32LittleEndian(CollectionsMarshal.AsSpan(reloc.Data).Slice(baseOff + 4, 4), (uint)blockSize);

            foreach (var e in entries)
            {
                var eo = reloc.Data.Count;
                reloc.Data.AddRange(new byte[2]);
                BinaryPrimitives.WriteUInt16LittleEndian(CollectionsMarshal.AsSpan(reloc.Data).Slice(eo, 2), e);
            }

            while ((reloc.Data.Count & 3) != 0) reloc.Data.Add(0);
        }
    }

    private static Dictionary<ImportSpec, int> ReserveImportThunks(OutSection text, IEnumerable<ImportSpec> imports)
    {
        var ordered = imports
            .Distinct()
            .OrderBy(i => i.Dll, StringComparer.OrdinalIgnoreCase)
            .ThenBy(i => i.Name, StringComparer.Ordinal)
            .ToList();

        var offsets = new Dictionary<ImportSpec, int>();
        foreach (var imp in ordered)
        {
            var off = Align(text.Data.Count, 16);
            if (off > text.Data.Count) text.Data.AddRange(new byte[off - text.Data.Count]);
            offsets[imp] = text.Data.Count;
            text.Data.AddRange(new byte[6]); // patched later
        }
        return offsets;
    }

    private static Dictionary<ImportSpec, uint> PatchImportThunks(
        List<OutSection> sections,
        Dictionary<ImportSpec, int> thunkOffsets,
        Dictionary<ImportSpec, uint> iatRvaByImport,
        ushort machine,
        List<uint> baseRelocRvas)
    {
        var text = sections.FirstOrDefault(s => s.Name == ".text");
        if (text == null) return new Dictionary<ImportSpec, uint>();

        var thunkRvaByImport = new Dictionary<ImportSpec, uint>();
        foreach (var kv in thunkOffsets)
        {
            var imp = kv.Key;
            if (!iatRvaByImport.TryGetValue(imp, out var iatRva)) continue;
            var thunkOff = kv.Value;
            var thunkRva = text.Rva + (uint)thunkOff;
            thunkRvaByImport[imp] = thunkRva;

            var span = CollectionsMarshal.AsSpan(text.Data).Slice(thunkOff, 6);
            if (machine == IMAGE_FILE_MACHINE_AMD64)
            {
                var disp = unchecked((int)iatRva - (int)(thunkRva + 6));
                span[0] = 0xFF;
                span[1] = 0x25;
                BinaryPrimitives.WriteInt32LittleEndian(span.Slice(2, 4), disp);
            }
            else
            {
                var absIatVa = IMAGE_BASE32 + iatRva;
                span[0] = 0xFF;
                span[1] = 0x25;
                BinaryPrimitives.WriteUInt32LittleEndian(span.Slice(2, 4), absIatVa);
                baseRelocRvas.Add(thunkRva + 2);
            }
        }

        return thunkRvaByImport;
    }

    private static void WritePe(
        LinkOptions opts,
        List<OutSection> sections,
        uint entryRva,
        bool hasRelocs,
        Dictionary<string, ImportSpec> imports,
        ushort machine)
    {
        var dosStubSize = 0x80;
        var peHeaderOffset = dosStubSize;
        var fileHeaderSize = 20;
        var isX64 = machine == IMAGE_FILE_MACHINE_AMD64;
        var optionalHeaderSize = isX64 ? 240 : 224;
        var sectionHeaderSize = 40 * sections.Count;
        var headersSize = Align((uint)(peHeaderOffset + 4 + fileHeaderSize + optionalHeaderSize + sectionHeaderSize), FILE_ALIGNMENT);

        var rawPtr = headersSize;
        foreach (var s in sections)
        {
            s.RawPtr = rawPtr;
            rawPtr += Align((uint)s.Data.Count, FILE_ALIGNMENT);
        }

        var sizeOfImage = Align(sections.Max(s => s.Rva + Align((uint)s.Data.Count, SECTION_ALIGNMENT)), SECTION_ALIGNMENT);
        var subsystem = opts.Subsystem.Equals("CONSOLE", StringComparison.OrdinalIgnoreCase)
            ? IMAGE_SUBSYSTEM_WINDOWS_CUI
            : IMAGE_SUBSYSTEM_WINDOWS_GUI;

        var dllChars = (ushort)IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE;
        if (opts.DynamicBase && hasRelocs) dllChars |= IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE;
        if (opts.NxCompat) dllChars |= IMAGE_DLLCHARACTERISTICS_NX_COMPAT;

        var dataDirs = new (uint rva, uint size)[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
        if (imports.Count > 0)
        {
            var importSection = sections.FirstOrDefault(s => s.Name == ".idata") ?? sections.First(s => s.Name == ".rdata");
            var dllCount = imports.Values.Select(i => i.Dll).Distinct(StringComparer.OrdinalIgnoreCase).Count();
            dataDirs[DIR_IMPORT] = (rva: importSection.Rva, size: (uint)((dllCount + 1) * 20)); // IMAGE_IMPORT_DESCRIPTOR table
        }
        else
        {
            var idata = sections.FirstOrDefault(s => s.Name == ".idata");
            if (idata != null)
                dataDirs[DIR_IMPORT] = (rva: idata.Rva, size: (uint)idata.Data.Count);
        }
        if (hasRelocs)
        {
            var reloc = sections.First(s => s.Name == ".reloc");
            dataDirs[DIR_BASERELOC] = (rva: reloc.Rva, size: (uint)reloc.Data.Count);
        }

        using var fs = File.Create(opts.OutExe);
        using var bw = new BinaryWriter(fs, Encoding.ASCII, leaveOpen: false);

        var dos = new byte[dosStubSize];
        dos[0] = (byte)'M'; dos[1] = (byte)'Z';
        BinaryPrimitives.WriteInt32LittleEndian(dos.AsSpan(0x3C, 4), peHeaderOffset);
        bw.Write(dos);

        bw.Write(Encoding.ASCII.GetBytes("PE\0\0"));

        bw.Write(machine);
        bw.Write((ushort)sections.Count);
        bw.Write((uint)0);
        bw.Write((uint)0);
        bw.Write((uint)0);
        bw.Write((ushort)optionalHeaderSize);
        var fileChars = IMAGE_FILE_EXECUTABLE_IMAGE;
        if (isX64) fileChars |= IMAGE_FILE_LARGE_ADDRESS_AWARE;
        bw.Write((ushort)fileChars);

        bw.Write((ushort)(isX64 ? IMAGE_NT_OPTIONAL_HDR64_MAGIC : IMAGE_NT_OPTIONAL_HDR32_MAGIC));
        bw.Write((byte)14);
        bw.Write((byte)0);

        var sizeOfCode = sections.Where(s => (s.Characteristics & IMAGE_SCN_CNT_CODE) != 0).Sum(s => Align((uint)s.Data.Count, FILE_ALIGNMENT));
        var sizeOfInitData = sections.Where(s => (s.Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) != 0).Sum(s => Align((uint)s.Data.Count, FILE_ALIGNMENT));

        bw.Write((uint)sizeOfCode);
        bw.Write((uint)sizeOfInitData);
        bw.Write((uint)0);
        bw.Write((uint)entryRva);
        var baseCode = (uint)(sections.FirstOrDefault(s => s.Name == ".text")?.Rva ?? sections[0].Rva);
        bw.Write(baseCode);
        if (!isX64)
        {
            var baseData = (uint)(sections.FirstOrDefault(s => s.Name == ".data")?.Rva
                ?? sections.FirstOrDefault(s => s.Name == ".rdata")?.Rva
                ?? 0u);
            bw.Write(baseData);
            bw.Write((uint)IMAGE_BASE32);
        }
        else
        {
            bw.Write((ulong)IMAGE_BASE64);
        }
        bw.Write((uint)SECTION_ALIGNMENT);
        bw.Write((uint)FILE_ALIGNMENT);
        bw.Write((ushort)6); bw.Write((ushort)0);
        bw.Write((ushort)0); bw.Write((ushort)0);
        bw.Write((ushort)6); bw.Write((ushort)0);
        bw.Write((uint)0);
        bw.Write((uint)sizeOfImage);
        bw.Write((uint)headersSize);
        bw.Write((uint)0);
        bw.Write((ushort)subsystem);
        bw.Write((ushort)dllChars);
        if (isX64)
        {
            bw.Write((ulong)0x00100000);
            bw.Write((ulong)0x00001000);
            bw.Write((ulong)0x00100000);
            bw.Write((ulong)0x00001000);
        }
        else
        {
            bw.Write((uint)0x00100000);
            bw.Write((uint)0x00001000);
            bw.Write((uint)0x00100000);
            bw.Write((uint)0x00001000);
        }
        bw.Write((uint)0);
        bw.Write((uint)IMAGE_NUMBEROF_DIRECTORY_ENTRIES);
        for (var i = 0; i < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; i++)
        {
            bw.Write(dataDirs[i].rva);
            bw.Write(dataDirs[i].size);
        }

        foreach (var s in sections)
        {
            var nameBytes = new byte[8];
            Encoding.ASCII.GetBytes(s.Name.Length > 8 ? s.Name[..8] : s.Name).CopyTo(nameBytes, 0);
            bw.Write(nameBytes);
            bw.Write((uint)s.Data.Count);
            bw.Write((uint)s.Rva);
            bw.Write(Align((uint)s.Data.Count, FILE_ALIGNMENT));
            bw.Write((uint)s.RawPtr);
            bw.Write((uint)0);
            bw.Write((uint)0);
            bw.Write((ushort)0);
            bw.Write((ushort)0);
            bw.Write((uint)s.Characteristics);
        }

        var cur = (uint)fs.Position;
        if (cur < headersSize) bw.Write(new byte[headersSize - cur]);

        foreach (var s in sections)
        {
            fs.Position = s.RawPtr;
            bw.Write(s.Data.ToArray());
            var pad = Align((uint)s.Data.Count, FILE_ALIGNMENT) - (uint)s.Data.Count;
            if (pad != 0) bw.Write(new byte[pad]);
        }
    }
}
