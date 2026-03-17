// ═══════════════════════════════════════════════════════════════════════════════
// RawrXD Transformer Kernel — COFF Object Emitter
// ═══════════════════════════════════════════════════════════════════════════════
// Emits a COFF .obj containing AVX-512 matmul kernels + BPE scanner as
// linkable symbols. Feed the .obj straight into rawrxd_linker.
// ═══════════════════════════════════════════════════════════════════════════════

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace RawrXD.TransformerKernel;

/// <summary>
/// Writes a minimal COFF x64 object file whose .text section contains
/// the emitted AVX-512 kernels, with proper symbol table entries so
/// the rawrxd linker can resolve them.
/// </summary>
public static class CoffKernelEmitter
{
    private const ushort IMAGE_FILE_MACHINE_AMD64 = 0x8664;
    private const uint IMAGE_SCN_CNT_CODE           = 0x00000020;
    private const uint IMAGE_SCN_ALIGN_16BYTES      = 0x00500000;
    private const uint IMAGE_SCN_MEM_EXECUTE         = 0x20000000;
    private const uint IMAGE_SCN_MEM_READ            = 0x40000000;
    private const byte IMAGE_SYM_CLASS_EXTERNAL      = 2;

    /// <summary>
    /// Build a COFF .obj containing all transformer kernels as linkable symbols.
    /// </summary>
    public static byte[] EmitCoffObject()
    {
        var (codeBlob, symbolOffsets) = TransformerRunner.EmitAllKernelsFlat();

        // ── Build string table ───────────────────────────────────────────
        var stringTable = new List<byte>();
        // First 4 bytes = total size of string table (including this field)
        stringTable.AddRange(new byte[4]); // placeholder
        var stringOffsets = new Dictionary<string, uint>();

        foreach (var name in symbolOffsets.Keys)
        {
            stringOffsets[name] = (uint)stringTable.Count;
            stringTable.AddRange(Encoding.UTF8.GetBytes(name));
            stringTable.Add(0); // null terminator
        }

        // Patch string table size
        BinaryPrimitives.WriteUInt32LittleEndian(
            System.Runtime.InteropServices.CollectionsMarshal.AsSpan(stringTable).Slice(0, 4),
            (uint)stringTable.Count);

        // ── Section header ───────────────────────────────────────────────
        int numSections = 1;
        int numSymbols = symbolOffsets.Count;

        // COFF header: 20 bytes
        // Section headers: 40 bytes each
        int headerSize = 20 + numSections * 40;
        int sectionDataOffset = headerSize;
        int symbolTableOffset = sectionDataOffset + codeBlob.Length;

        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms, Encoding.UTF8, leaveOpen: true);

        // ── COFF File Header (20 bytes) ──────────────────────────────────
        w.Write(IMAGE_FILE_MACHINE_AMD64);       // Machine
        w.Write((ushort)numSections);             // NumberOfSections
        w.Write((uint)0);                         // TimeDateStamp
        w.Write((uint)symbolTableOffset);         // PointerToSymbolTable
        w.Write((uint)numSymbols);                // NumberOfSymbols
        w.Write((ushort)0);                       // SizeOfOptionalHeader
        w.Write((ushort)0);                       // Characteristics

        // ── Section Header: .text (40 bytes) ─────────────────────────────
        // Name (8 bytes, padded)
        w.Write(Encoding.UTF8.GetBytes(".text\0\0\0"), 0, 8);
        w.Write((uint)codeBlob.Length);           // VirtualSize
        w.Write((uint)0);                         // VirtualAddress
        w.Write((uint)codeBlob.Length);           // SizeOfRawData
        w.Write((uint)sectionDataOffset);         // PointerToRawData
        w.Write((uint)0);                         // PointerToRelocations
        w.Write((uint)0);                         // PointerToLinenumbers
        w.Write((ushort)0);                       // NumberOfRelocations
        w.Write((ushort)0);                       // NumberOfLinenumbers
        w.Write(IMAGE_SCN_CNT_CODE | IMAGE_SCN_ALIGN_16BYTES |
                IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ); // Characteristics

        // ── Section Data: .text ──────────────────────────────────────────
        w.Write(codeBlob);

        // ── Symbol Table (18 bytes per symbol) ───────────────────────────
        foreach (var (name, offset) in symbolOffsets)
        {
            // Name: if ≤8 bytes, inline; else use string table offset
            if (name.Length <= 8)
            {
                var nameBytes = new byte[8];
                Encoding.UTF8.GetBytes(name, nameBytes);
                w.Write(nameBytes, 0, 8);
            }
            else
            {
                w.Write((uint)0);                     // Zeroes (indicates string table)
                w.Write(stringOffsets[name]);          // Offset into string table
            }

            w.Write((uint)offset);                    // Value (offset into section)
            w.Write((short)1);                        // SectionNumber (1 = .text)
            w.Write((ushort)0x20);                    // Type (function)
            w.Write(IMAGE_SYM_CLASS_EXTERNAL);        // StorageClass
            w.Write((byte)0);                         // NumberOfAuxSymbols
        }

        // ── String Table ─────────────────────────────────────────────────
        w.Write(stringTable.ToArray());

        w.Flush();
        return ms.ToArray();
    }

    /// <summary>
    /// Write the COFF object to a file, ready for rawrxd_linker consumption.
    /// </summary>
    public static void EmitToFile(string outputPath)
    {
        var obj = EmitCoffObject();
        File.WriteAllBytes(outputPath, obj);
    }
}

/// <summary>
/// CLI entry point: emit transformer kernels as a COFF .obj file.
/// Usage: dotnet run -- --emit-obj transformer_kernels.obj
///        dotnet run -- --emit-bin transformer_kernels.bin   (flat binary)
///        dotnet run -- --dump-map                           (print symbol map)
/// </summary>
public static class KernelCLI
{
    public static int Run(string[] args)
    {
        if (args.Length == 0)
        {
            Console.WriteLine("RawrXD Transformer Kernel Emitter");
            Console.WriteLine("  --emit-obj <path>   Emit COFF .obj for rawrxd_linker");
            Console.WriteLine("  --emit-bin <path>   Emit flat binary blob");
            Console.WriteLine("  --dump-map          Print kernel symbol map");
            return 0;
        }

        for (int i = 0; i < args.Length; i++)
        {
            switch (args[i].ToLowerInvariant())
            {
                case "--emit-obj":
                {
                    var path = (i + 1 < args.Length) ? args[++i] : "transformer_kernels.obj";
                    CoffKernelEmitter.EmitToFile(path);
                    Console.WriteLine($"OK: wrote COFF object → {path}");
                    var kernels = TransformerRunner.GetEmittedKernels();
                    foreach (var (name, code) in kernels)
                        Console.WriteLine($"  {name}: {code.Length} bytes");
                    break;
                }

                case "--emit-bin":
                {
                    var path = (i + 1 < args.Length) ? args[++i] : "transformer_kernels.bin";
                    var (blob, symbols) = TransformerRunner.EmitAllKernelsFlat();
                    File.WriteAllBytes(path, blob);
                    Console.WriteLine($"OK: wrote flat binary → {path} ({blob.Length} bytes)");
                    foreach (var (name, offset) in symbols)
                        Console.WriteLine($"  0x{offset:X8}  {name}");
                    break;
                }

                case "--dump-map":
                {
                    var kernels = TransformerRunner.GetEmittedKernels();
                    int totalBytes = 0;
                    Console.WriteLine("Kernel Symbol Map:");
                    Console.WriteLine("─────────────────────────────────────────────");
                    foreach (var (name, code) in kernels)
                    {
                        Console.WriteLine($"  {name,-30} {code.Length,6} bytes");
                        totalBytes += code.Length;
                    }
                    Console.WriteLine("─────────────────────────────────────────────");
                    Console.WriteLine($"  Total:                       {totalBytes,6} bytes");

                    // Dump first 32 bytes hex of each kernel
                    Console.WriteLine();
                    foreach (var (name, code) in kernels)
                    {
                        Console.Write($"  {name}: ");
                        for (int b = 0; b < Math.Min(32, code.Length); b++)
                            Console.Write($"{code[b]:X2} ");
                        if (code.Length > 32) Console.Write("...");
                        Console.WriteLine();
                    }
                    break;
                }
            }
        }

        return 0;
    }
}
