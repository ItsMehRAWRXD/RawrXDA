using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Text;

public static class BinaryWriterExtensions
{
    public static void WriteStruct<T>(this BinaryWriter writer, T structure) where T : struct
    {
        int size = Marshal.SizeOf(typeof(T));
        byte[] buffer = new byte[size];
        IntPtr ptr = Marshal.AllocHGlobal(size);
        try
        {
            Marshal.StructureToPtr(structure, ptr, false);
            Marshal.Copy(ptr, buffer, 0, size);
            writer.Write(buffer);
        }
        finally
        {
            Marshal.FreeHGlobal(ptr);
        }
    }
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct IMAGE_DOS_HEADER
{
    public ushort e_magic;
    public ushort e_cblp;
    public ushort e_cp;
    public ushort e_crlc;
    public ushort e_cparhdr;
    public ushort e_minalloc;
    public ushort e_maxalloc;
    public ushort e_ss;
    public ushort e_sp;
    public ushort e_csum;
    public ushort e_ip;
    public ushort e_cs;
    public ushort e_lfarlc;
    public ushort e_ovno;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
    public ushort[] e_res;
    public ushort e_oemid;
    public ushort e_oeminfo;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 10)]
    public ushort[] e_res2;
    public uint e_lfanew;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct IMAGE_FILE_HEADER
{
    public ushort Machine;
    public ushort NumberOfSections;
    public uint TimeDateStamp;
    public uint PointerToSymbolTable;
    public uint NumberOfSymbols;
    public ushort SizeOfOptionalHeader;
    public ushort Characteristics;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct IMAGE_OPTIONAL_HEADER64
{
    public ushort Magic;
    public byte MajorLinkerVersion;
    public byte MinorLinkerVersion;
    public uint SizeOfCode;
    public uint SizeOfInitializedData;
    public uint SizeOfUninitializedData;
    public uint AddressOfEntryPoint;
    public uint BaseOfCode;
    public ulong ImageBase;
    public uint SectionAlignment;
    public uint FileAlignment;
    public ushort MajorOperatingSystemVersion;
    public ushort MinorOperatingSystemVersion;
    public ushort MajorImageVersion;
    public ushort MinorImageVersion;
    public ushort MajorSubsystemVersion;
    public ushort MinorSubsystemVersion;
    public uint Win32VersionValue;
    public uint SizeOfImage;
    public uint SizeOfHeaders;
    public uint CheckSum;
    public ushort Subsystem;
    public ushort DllCharacteristics;
    public ulong SizeOfStackReserve;
    public ulong SizeOfStackCommit;
    public ulong SizeOfHeapReserve;
    public ulong SizeOfHeapCommit;
    public uint LoaderFlags;
    public uint NumberOfRvaAndSizes;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
    public IMAGE_DATA_DIRECTORY[] DataDirectory;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct IMAGE_DATA_DIRECTORY
{
    public uint VirtualAddress;
    public uint Size;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct IMAGE_SECTION_HEADER
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
    public byte[] Name;
    public uint VirtualSize;
    public uint VirtualAddress;
    public uint SizeOfRawData;
    public uint PointerToRawData;
    public uint PointerToRelocations;
    public uint PointerToLinenumbers;
    public ushort NumberOfRelocations;
    public ushort NumberOfLinenumbers;
    public uint Characteristics;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct IMAGE_NT_HEADERS64
{
    public uint Signature;
    public IMAGE_FILE_HEADER FileHeader;
    public IMAGE_OPTIONAL_HEADER64 OptionalHeader;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct IMAGE_IMPORT_DESCRIPTOR
{
    public uint OriginalFirstThunk;    // RVA to ILT
    public uint TimeDateStamp;
    public uint ForwarderChain;
    public uint Name;                  // RVA to DLL name
    public uint FirstThunk;           // RVA to IAT
}

public class ImportEntry
{
    public string DllName;
    public List<string> Functions;
    public uint DllNameRVA;
    public uint IATRVA;
    public uint ILTRVA;
    public uint HintNameTableRVA;
}

public class PeWriter
{
    private List<byte> textSection = new List<byte>();
    private List<byte> dataSection = new List<byte>();
    private List<ImportEntry> imports = new List<ImportEntry>();
    private Dictionary<string, uint> functionRVAs = new Dictionary<string, uint>();
    private uint currentTextOffset = 0;
    private uint currentDataOffset = 0;

    public void AddImport(string dllName, string functionName)
    {
        var import = imports.Find(i => i.DllName == dllName);
        if (import == null)
        {
            import = new ImportEntry { DllName = dllName, Functions = new List<string>() };
            imports.Add(import);
        }
        import.Functions.Add(functionName);
    }

    public X64Emitter GetEmitter()
    {
        return new X64Emitter(this);
    }

    public void AddTextBytes(byte[] bytes)
    {
        textSection.AddRange(bytes);
        currentTextOffset += (uint)bytes.Length;
    }

    public void AddDataBytes(byte[] bytes)
    {
        dataSection.AddRange(bytes);
        currentDataOffset += (uint)bytes.Length;
    }

    public uint GetTextOffset()
    {
        return currentTextOffset;
    }

    public uint GetDataOffset()
    {
        return currentDataOffset;
    }

    public void WritePE(string outputPath)
    {
        using (var fs = new FileStream(outputPath, FileMode.Create))
        using (var writer = new BinaryWriter(fs))
        {
            // Constants
            const uint sectionAlignment = 0x1000;
            const uint fileAlignment = 0x200;
            const ulong imageBase = 0x140000000;

            // Calculate import table sizes first
            uint importDirSize = (uint)(imports.Count + 1) * (uint)Marshal.SizeOf(typeof(IMAGE_IMPORT_DESCRIPTOR));
            uint dllNamesSize = 0;
            uint iltIatSize = 0;
            uint hintNameSize = 0;

            foreach (var import in imports)
            {
                dllNamesSize += (uint)(import.DllName.Length + 1);
                iltIatSize += (uint)(import.Functions.Count + 1) * 8 * 2; // ILT + IAT
                foreach (var func in import.Functions)
                {
                    hintNameSize += (uint)(2 + func.Length + 1);
                }
            }

            uint totalImportSize = importDirSize + dllNamesSize + iltIatSize + hintNameSize;

            // Calculate section sizes
            uint textRawSize = (uint)textSection.Count;
            uint dataRawSize = Math.Max((uint)dataSection.Count, totalImportSize);
            uint textRawSizeAligned = (textRawSize + fileAlignment - 1) & ~(fileAlignment - 1);
            uint dataRawSizeAligned = (dataRawSize + fileAlignment - 1) & ~(fileAlignment - 1);

            // Calculate RVAs
            uint textRVA = sectionAlignment;
            uint dataRVA = textRVA + ((textRawSize + sectionAlignment - 1) & ~(sectionAlignment - 1));
            uint importDirRVA = dataRVA;

            // Calculate import RVAs within data section
            uint currentImportOffset = 0;
            foreach (var import in imports)
            {
                import.DllNameRVA = importDirRVA + currentImportOffset;
                currentImportOffset += (uint)(import.DllName.Length + 1);
            }

            uint iltBaseRVA = importDirRVA + importDirSize + dllNamesSize;
            uint iatBaseRVA = iltBaseRVA + iltIatSize / 2;
            uint hintNameBaseRVA = iatBaseRVA + iltIatSize / 2;

            uint hintNameOffset = 0;
            foreach (var import in imports)
            {
                import.ILTRVA = iltBaseRVA + hintNameOffset * 8;
                import.IATRVA = iatBaseRVA + hintNameOffset * 8;
                import.HintNameTableRVA = hintNameBaseRVA + hintNameOffset * (uint)(2 + import.Functions[0].Length + 1);

                foreach (var func in import.Functions)
                {
                    hintNameOffset++;
                }
            }

            // Calculate total image size
            uint sizeOfImage = dataRVA + ((dataRawSize + sectionAlignment - 1) & ~(sectionAlignment - 1));
            uint sizeOfHeaders = fileAlignment;

            // DOS Header
            var dosHeader = new IMAGE_DOS_HEADER
            {
                e_magic = 0x5A4D,
                e_cblp = 0x90,
                e_cp = 3,
                e_cparhdr = 4,
                e_minalloc = 0,
                e_maxalloc = 0xFFFF,
                e_sp = 0xB8,
                e_lfarlc = 0x40,
                e_oemid = 0,
                e_oeminfo = 0,
                e_res = new ushort[4],
                e_res2 = new ushort[10],
                e_lfanew = 0x80
            };

            writer.WriteStruct(dosHeader);
            writer.Write(new byte[0x80 - Marshal.SizeOf(dosHeader)]);

            // NT Header
            var ntHeader = new IMAGE_NT_HEADERS64
            {
                Signature = 0x4550,
                FileHeader = new IMAGE_FILE_HEADER
                {
                    Machine = 0x8664,
                    NumberOfSections = 2,
                    TimeDateStamp = (uint)DateTime.Now.Ticks,
                    SizeOfOptionalHeader = (ushort)Marshal.SizeOf(typeof(IMAGE_OPTIONAL_HEADER64)),
                    Characteristics = 0x22
                },
                OptionalHeader = new IMAGE_OPTIONAL_HEADER64
                {
                    Magic = 0x20B,
                    MajorLinkerVersion = 14,
                    MinorLinkerVersion = 0,
                    SizeOfCode = textRawSize,
                    SizeOfInitializedData = dataRawSize,
                    SizeOfUninitializedData = 0,
                    AddressOfEntryPoint = textRVA,
                    BaseOfCode = textRVA,
                    ImageBase = imageBase,
                    SectionAlignment = sectionAlignment,
                    FileAlignment = fileAlignment,
                    MajorOperatingSystemVersion = 6,
                    MinorOperatingSystemVersion = 0,
                    MajorImageVersion = 0,
                    MinorImageVersion = 0,
                    MajorSubsystemVersion = 6,
                    MinorSubsystemVersion = 0,
                    Win32VersionValue = 0,
                    SizeOfImage = sizeOfImage,
                    SizeOfHeaders = sizeOfHeaders,
                    CheckSum = 0,
                    Subsystem = 3, // Console
                    DllCharacteristics = 0x8160,
                    SizeOfStackReserve = 0x100000,
                    SizeOfStackCommit = 0x1000,
                    SizeOfHeapReserve = 0x100000,
                    SizeOfHeapCommit = 0x1000,
                    LoaderFlags = 0,
                    NumberOfRvaAndSizes = 16,
                    DataDirectory = new IMAGE_DATA_DIRECTORY[16]
                }
            };

            // Set import directory RVA
            ntHeader.OptionalHeader.DataDirectory[1].VirtualAddress = importDirRVA;
            ntHeader.OptionalHeader.DataDirectory[1].Size = importDirSize;

            writer.WriteStruct(ntHeader);

            // Section Headers
            var textSectionHeader = new IMAGE_SECTION_HEADER
            {
                Name = Encoding.ASCII.GetBytes(".text\0\0\0"),
                VirtualSize = textRawSize,
                VirtualAddress = textRVA,
                SizeOfRawData = textRawSizeAligned,
                PointerToRawData = fileAlignment,
                Characteristics = 0x60000020 // Code, executable, readable
            };

            var dataSectionHeader = new IMAGE_SECTION_HEADER
            {
                Name = Encoding.ASCII.GetBytes(".data\0\0\0"),
                VirtualSize = dataRawSize,
                VirtualAddress = dataRVA,
                SizeOfRawData = dataRawSizeAligned,
                PointerToRawData = fileAlignment + textRawSizeAligned,
                Characteristics = 0xC0000040 // Initialized data, readable, writable
            };

            writer.WriteStruct(textSectionHeader);
            writer.WriteStruct(dataSectionHeader);

            // Padding to file alignment
            writer.Write(new byte[fileAlignment - (int)fs.Position]);

            // .text section
            writer.Write(textSection.ToArray());
            writer.Write(new byte[textRawSizeAligned - textSection.Count]);

            // .data section (including import tables)
            writer.Write(dataSection.ToArray());

            // Write import directory table
            long importDirPos = fs.Position;
            foreach (var import in imports)
            {
                var descriptor = new IMAGE_IMPORT_DESCRIPTOR
                {
                    OriginalFirstThunk = import.ILTRVA,
                    TimeDateStamp = 0,
                    ForwarderChain = 0,
                    Name = import.DllNameRVA,
                    FirstThunk = import.IATRVA
                };
                writer.WriteStruct(descriptor);
            }
            // Null terminator
            writer.Write(new byte[Marshal.SizeOf(typeof(IMAGE_IMPORT_DESCRIPTOR))]);

            // Write DLL names
            foreach (var import in imports)
            {
                writer.Write(Encoding.ASCII.GetBytes(import.DllName + "\0"));
            }

            // Write ILT and IAT
            hintNameOffset = 0;
            foreach (var import in imports)
            {
                // ILT
                foreach (var func in import.Functions)
                {
                    writer.Write(hintNameBaseRVA + hintNameOffset);
                    hintNameOffset += (uint)(2 + func.Length + 1);
                }
                writer.Write((ulong)0);

                // IAT
                hintNameOffset = 0; // Reset for IAT
                foreach (var func in import.Functions)
                {
                    writer.Write(hintNameBaseRVA + hintNameOffset);
                    hintNameOffset += (uint)(2 + func.Length + 1);
                }
                writer.Write((ulong)0);
            }

            // Write hint/name table
            foreach (var import in imports)
            {
                foreach (var func in import.Functions)
                {
                    writer.Write((ushort)0); // Hint
                    writer.Write(Encoding.ASCII.GetBytes(func + "\0"));
                }
            }

            // Fill to data section alignment
            long currentPos = fs.Position;
            long dataEndPos = fileAlignment + textRawSizeAligned + dataRawSizeAligned;
            if (currentPos < dataEndPos)
            {
                writer.Write(new byte[dataEndPos - currentPos]);
            }
        }
    }
}

public enum Reg64
{
    RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI,
    R8, R9, R10, R11, R12, R13, R14, R15,
    EAX = RAX, ECX = RCX, EDX = RDX, EBX = RBX,
    ESP = RSP, EBP = RBP, ESI = RSI, EDI = RDI,
    R8D = R8, R9D = R9, R10D = R10, R11D = R11,
    R12D = R12, R13D = R13, R14D = R14, R15D = R15
}

public class X64Emitter
{
    private PeWriter peWriter;
    private List<byte> code = new List<byte>();
    private Dictionary<string, uint> labels = new Dictionary<string, uint>();
    private Dictionary<string, List<uint>> labelRefs = new Dictionary<string, List<uint>>();
    private uint currentOffset = 0;

    public X64Emitter(PeWriter writer)
    {
        peWriter = writer;
    }

    public void EmitPrologue(int stackSize)
    {
        // push rbp
        code.Add(0x55);
        // mov rbp, rsp
        code.AddRange(new byte[] { 0x48, 0x89, 0xE5 });
        // sub rsp, stackSize
        if (stackSize > 0)
        {
            code.Add(0x48);
            code.Add(0x81);
            code.Add(0xEC);
            code.AddRange(BitConverter.GetBytes(stackSize));
        }
        currentOffset += 8 + (stackSize > 0 ? 8u : 0u);
    }

    public void EmitEpilogue(int stackSize)
    {
        // add rsp, stackSize
        if (stackSize > 0)
        {
            code.Add(0x48);
            code.Add(0x81);
            code.Add(0xC4);
            code.AddRange(BitConverter.GetBytes(stackSize));
        }
        // pop rbp
        code.Add(0x5D);
        // ret
        code.Add(0xC3);
    }

    public void EmitXorRegReg(Reg64 dst, Reg64 src)
    {
        code.Add(0x48); // REX.W
        code.Add(0x31); // XOR
        code.Add((byte)(0xC0 + ((int)dst << 3) + (int)src));
        currentOffset += 3;
    }

    public void EmitMovImm32(Reg64 reg, uint value)
    {
        code.Add(0x41); // REX.B if reg >= R8
        if ((int)reg >= 8) code.Add(0x40);
        code.Add((byte)(0xB8 + ((int)reg & 7)));
        code.AddRange(BitConverter.GetBytes(value));
        currentOffset += 5;
    }

    public void EmitLeaRIPRelative(Reg64 reg, string label)
    {
        // lea reg, [rip + offset]
        code.Add(0x48); // REX.W
        code.Add(0x8D);
        code.Add((byte)(0x05 + ((int)reg << 3)));
        // Placeholder for offset
        uint placeholderPos = (uint)code.Count;
        code.AddRange(new byte[4]);
        currentOffset += 8;

        if (!labelRefs.ContainsKey(label))
            labelRefs[label] = new List<uint>();
        labelRefs[label].Add(placeholderPos);
    }

    public void EmitCallImport(string functionName)
    {
        // call [IAT entry]
        // This is a placeholder - will be resolved when PE is written
        code.Add(0xFF);
        code.Add(0x15);
        uint placeholderPos = (uint)code.Count;
        code.AddRange(new byte[4]); // Placeholder for RVA
        currentOffset += 6;

        // Store reference for later resolution
        if (!labelRefs.ContainsKey(functionName))
            labelRefs[functionName] = new List<uint>();
        labelRefs[functionName].Add(placeholderPos);
    }

    public void EmitRet()
    {
        code.Add(0xC3);
        currentOffset += 1;
    }

    public byte[] GetBytes()
    {
        // Resolve labels
        foreach (var kvp in labelRefs)
        {
            string label = kvp.Key;
            if (labels.ContainsKey(label))
            {
                uint labelOffset = labels[label];
                foreach (uint refPos in kvp.Value)
                {
                    uint relativeOffset = labelOffset - (refPos - 4); // -4 because refPos points to start of 4-byte offset
                    Array.Copy(BitConverter.GetBytes(relativeOffset), 0, code.ToArray(), refPos, 4);
                }
            }
        }

        return code.ToArray();
    }
}

class Program
{
    static void Main()
    {
        // Go back to the working simple PE
        var p = new PeWriter();

        var e = p.GetEmitter();
        // Just return 42 - no prologue/epilogue needed for this simple case
        e.EmitMovImm32(Reg64.EAX, 42);
        e.EmitRet();

        p.WritePE("working_pe.exe");
        Console.WriteLine("Working PE generated!");
    }
}