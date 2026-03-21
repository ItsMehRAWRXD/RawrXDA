// =============================================================================
// rawrxd/sovereign_emit_formats.hpp
// Tri-format container helpers (PE64 / ELF64 / Mach-O 64) for experiments and
// tests. Production images should still be produced with normal linkers
// (link.exe / lld / ld64) and platform signing where required — see
// docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md.
//
// Design (decoupled layers):
//   1) rawrxd::sovereign::TargetManifest — toolchain / OS intent (what you build).
//   2) EmitBlueprint — layout parameters for the binary wrapper (container).
//   3) HeaderEmitResult / compose* — on-disk header bytes (+ optional ELF image glue).
// Instruction bytes stay outside this file; append them after headers or use
// composeElf64MinimalImage for a patched ET_EXEC stub.
// =============================================================================

#pragma once

#include "rawrxd/sovereign_target_manifest.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace rawrxd::sovereign::format
{

enum class BinaryFormat : std::uint8_t
{
    Pe64 = 0,
    Elf64 = 1,
    MachO64 = 2,
};

enum class TargetArch : std::uint8_t
{
    X64 = 0,
    Arm64 = 1,
};

struct SegmentBlueprint
{
    std::array<char, 8> name{};
    std::uint32_t virtualSize = 0;
    std::uint32_t rawSize = 0;
    std::uint32_t characteristics = 0;
    const std::uint8_t* rawData = nullptr;
};

struct EmitBlueprint
{
    BinaryFormat format = BinaryFormat::Pe64;
    TargetArch arch = TargetArch::X64;
    std::uint64_t imageBase = 0;
    std::uint64_t entryVirtualAddress = 0;
    std::uint32_t fileAlignment = 0x200;
    std::uint32_t sectionAlignment = 0x1000;
    std::vector<SegmentBlueprint> segments{};
};

struct HeaderEmitResult
{
    std::vector<std::uint8_t> headerBytes{};
    std::uint32_t headerSize = 0;
};

inline constexpr std::uint32_t kPe64DosStubSize = 0x40;
inline constexpr std::uint32_t kPe64SignatureSize = 4;
inline constexpr std::uint32_t kElf64EhdrSize = 64;
inline constexpr std::uint32_t kElf64PhdrSize = 56;
inline constexpr std::uint32_t kMachO64HeaderSize = 32;

inline std::uint32_t alignUp(std::uint32_t value, std::uint32_t alignment)
{
    if (alignment == 0)
    {
        return value;
    }
    const std::uint32_t mask = alignment - 1u;
    return (value + mask) & ~mask;
}

inline std::uint64_t alignUp64(std::uint64_t value, std::uint64_t alignment)
{
    if (alignment == 0)
    {
        return value;
    }
    const std::uint64_t mask = alignment - 1ull;
    return (value + mask) & ~mask;
}

inline void writeLe16At(std::vector<std::uint8_t>& buf, size_t offset, std::uint16_t value)
{
    if (offset + 2 > buf.size())
    {
        buf.resize(offset + 2, 0);
    }
    buf[offset] = static_cast<std::uint8_t>(value & 0xFFu);
    buf[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
}

inline void writeLe32At(std::vector<std::uint8_t>& buf, size_t offset, std::uint32_t value)
{
    if (offset + 4 > buf.size())
    {
        buf.resize(offset + 4, 0);
    }
    for (size_t i = 0; i < 4; ++i)
    {
        buf[offset + i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFu);
    }
}

inline void writeLe64At(std::vector<std::uint8_t>& buf, size_t offset, std::uint64_t value)
{
    if (offset + 8 > buf.size())
    {
        buf.resize(offset + 8, 0);
    }
    for (size_t i = 0; i < 8; ++i)
    {
        buf[offset + i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFull);
    }
}

inline void appendLe16(std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

inline void appendLe32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

inline void appendLe64(std::vector<std::uint8_t>& out, std::uint64_t value)
{
    appendLe32(out, static_cast<std::uint32_t>(value & 0xFFFFFFFFull));
    appendLe32(out, static_cast<std::uint32_t>((value >> 32) & 0xFFFFFFFFull));
}

// Emits minimal header bytes for format bootstrap. This is intentionally a
// container-only primitive (no imports/resources/load-command graph yet).
inline HeaderEmitResult emitMinimalHeader(const EmitBlueprint& blueprint)
{
    HeaderEmitResult result{};
    auto& out = result.headerBytes;

    switch (blueprint.format)
    {
        case BinaryFormat::Pe64:
        {
            out.resize(kPe64DosStubSize, 0);
            out[0] = 'M';
            out[1] = 'Z';
            // e_lfanew -> start PE signature immediately after DOS stub.
            out[0x3C] = static_cast<std::uint8_t>(kPe64DosStubSize & 0xFFu);
            out[0x3D] = static_cast<std::uint8_t>((kPe64DosStubSize >> 8) & 0xFFu);
            out[0x3E] = static_cast<std::uint8_t>((kPe64DosStubSize >> 16) & 0xFFu);
            out[0x3F] = static_cast<std::uint8_t>((kPe64DosStubSize >> 24) & 0xFFu);
            out.push_back('P');
            out.push_back('E');
            out.push_back(0);
            out.push_back(0);
            result.headerSize = static_cast<std::uint32_t>(out.size());
            return result;
        }
        case BinaryFormat::Elf64:
        {
            out.reserve(kElf64EhdrSize + kElf64PhdrSize);
            // e_ident
            out.push_back(0x7F);
            out.push_back('E');
            out.push_back('L');
            out.push_back('F');
            out.push_back(2);  // 64-bit
            out.push_back(1);  // little-endian
            out.push_back(1);  // version
            out.push_back(0);  // System V ABI
            for (int i = 0; i < 8; ++i)
            {
                out.push_back(0);
            }

            appendLe16(out, 2);  // ET_EXEC
            appendLe16(out, blueprint.arch == TargetArch::Arm64 ? 183 : 62);
            appendLe32(out, 1);  // EV_CURRENT
            appendLe64(out, blueprint.entryVirtualAddress);
            appendLe64(out, kElf64EhdrSize);  // e_phoff
            appendLe64(out, 0);               // e_shoff
            appendLe32(out, 0);               // flags
            appendLe16(out, static_cast<std::uint16_t>(kElf64EhdrSize));
            appendLe16(out, static_cast<std::uint16_t>(kElf64PhdrSize));
            appendLe16(out, 1);  // one PT_LOAD in minimal bootstrap
            appendLe16(out, 0);  // shentsize
            appendLe16(out, 0);  // shnum
            appendLe16(out, 0);  // shstrndx
            // Single PT_LOAD program header (caller must patch sizes/offsets for a real image).
            constexpr std::uint32_t kPtLoad = 1u;
            constexpr std::uint32_t kPfReadExec = 5u;
            appendLe32(out, kPtLoad);
            appendLe32(out, kPfReadExec);
            appendLe64(out, 0);                    // p_offset
            appendLe64(out, blueprint.imageBase);  // p_vaddr
            appendLe64(out, blueprint.imageBase);  // p_paddr
            appendLe64(out, 0);                    // p_filesz (patch)
            appendLe64(out, 0);                    // p_memsz (patch)
            appendLe64(out, static_cast<std::uint64_t>(blueprint.sectionAlignment != 0 ? blueprint.sectionAlignment
                                                                                       : 0x1000u));  // p_align
            result.headerSize = static_cast<std::uint32_t>(out.size());
            return result;
        }
        case BinaryFormat::MachO64:
        {
            out.reserve(kMachO64HeaderSize);
            appendLe32(out, 0xFEEDFACFu);  // MH_MAGIC_64
            appendLe32(out, blueprint.arch == TargetArch::Arm64 ? 0x0100000Cu : 0x01000007u);
            appendLe32(out, blueprint.arch == TargetArch::Arm64 ? 0x00000000u : 0x00000003u);
            appendLe32(out, 0x2u);  // MH_EXECUTE
            appendLe32(out, 0u);    // ncmds, filled by caller later
            appendLe32(out, 0u);    // sizeofcmds, filled by caller later
            appendLe32(out, 0x1u);  // MH_NOUNDEFS
            appendLe32(out, 0u);    // reserved
            result.headerSize = static_cast<std::uint32_t>(out.size());
            return result;
        }
        default:
            return result;
    }
}

// -----------------------------------------------------------------------------
// Layer 1 → 2: map neutral TargetManifest to EmitBlueprint (container params).
// -----------------------------------------------------------------------------

inline sovereign::ObjectFormat defaultObjectFormatForOs(sovereign::TargetOs os)
{
    using namespace sovereign;
    switch (os)
    {
        case TargetOs::Windows:
            return ObjectFormat::Pe;
        case TargetOs::Linux:
        case TargetOs::Android:
            return ObjectFormat::Elf;
        case TargetOs::MacOS:
        case TargetOs::IOS:
            return ObjectFormat::MachO;
    }
    return ObjectFormat::Pe;
}

/// Build layout defaults from manifest.objectFormat + manifest.arch.
/// entryVirtualAddress is set to imageBase + 0x1000 for PE/ELF (common flat ET_EXEC
/// convention); use composeElf64MinimalImage for an entry that starts at the first
/// byte after Ehdr+Phdr.
inline EmitBlueprint emitBlueprintFromManifest(const sovereign::TargetManifest& m)
{
    EmitBlueprint b{};
    switch (m.objectFormat)
    {
        case sovereign::ObjectFormat::Pe:
            b.format = BinaryFormat::Pe64;
            b.imageBase = 0x0000000140000000ull;
            break;
        case sovereign::ObjectFormat::Elf:
            b.format = BinaryFormat::Elf64;
            b.imageBase = 0x400000ull;
            break;
        case sovereign::ObjectFormat::MachO:
            b.format = BinaryFormat::MachO64;
            b.imageBase = 0x0000000100000000ull;
            break;
    }
    b.arch = (m.arch == sovereign::TargetArch::Arm64) ? TargetArch::Arm64 : TargetArch::X64;
    b.entryVirtualAddress = b.imageBase + 0x1000ull;
    b.fileAlignment = 0x200;
    b.sectionAlignment = 0x1000;
    return b;
}

inline HeaderEmitResult emitMinimalHeaderFromManifest(const sovereign::TargetManifest& m)
{
    return emitMinimalHeader(emitBlueprintFromManifest(m));
}

// -----------------------------------------------------------------------------
// ELF: glue headers + raw code bytes; patch e_entry and PT_LOAD sizes (minimal ET_EXEC).
// -----------------------------------------------------------------------------

/// Returns empty if blueprint is not ELF64. Entry VA = imageBase + sizeof(Ehdr+Phdr).
/// Does not emit syscalls; caller supplies machine code bytes only.
inline std::vector<std::uint8_t> composeElf64MinimalImage(const EmitBlueprint& bp,
                                                          const std::vector<std::uint8_t>& codeBytes)
{
    if (bp.format != BinaryFormat::Elf64)
    {
        return {};
    }
    const std::uint64_t hdrSize = static_cast<std::uint64_t>(kElf64EhdrSize + kElf64PhdrSize);
    EmitBlueprint bp2 = bp;
    bp2.entryVirtualAddress = bp.imageBase + hdrSize;
    HeaderEmitResult h = emitMinimalHeader(bp2);
    std::vector<std::uint8_t> full = std::move(h.headerBytes);
    full.insert(full.end(), codeBytes.begin(), codeBytes.end());
    const std::uint64_t total = full.size();
    const std::uint64_t page = (bp.sectionAlignment != 0) ? static_cast<std::uint64_t>(bp.sectionAlignment) : 0x1000ull;
    const std::uint64_t memsz = alignUp64(total, page);
    // Elf64_Phdr p_filesz / p_memsz at file offsets 96 and 104 (one Phdr at offset 64).
    writeLe64At(full, 96, total);
    writeLe64At(full, 104, memsz);
    return full;
}

inline std::vector<std::uint8_t> composeElf64MinimalImageFromManifest(const sovereign::TargetManifest& m,
                                                                      const std::vector<std::uint8_t>& codeBytes)
{
    if (m.objectFormat != sovereign::ObjectFormat::Elf)
    {
        return {};
    }
    return composeElf64MinimalImage(emitBlueprintFromManifest(m), codeBytes);
}

// -----------------------------------------------------------------------------
// PE64: DOS + PE sig + COFF + PE32+ optional + one .text section; patch sizes.
// Lab-only — no imports, resources, or Authenticode.
// -----------------------------------------------------------------------------

inline constexpr std::uint32_t kPe64OptionalHeader64Size = 240;
inline constexpr std::uint32_t kPe64NtFileOffset = 0x80;  // e_lfanew target (DOS + pad)
inline constexpr std::uint32_t kPe64FirstSectionRva = 0x1000;

/// Minimal PE64: one `.text` at RVA 0x1000, entry at start of section. Patches
/// `AddressOfEntryPoint`, `SizeOfImage`, `SizeOfHeaders`, `.text` raw/size.
inline std::vector<std::uint8_t> composePe64MinimalImage(const EmitBlueprint& bp,
                                                         const std::vector<std::uint8_t>& codeBytes)
{
    if (bp.format != BinaryFormat::Pe64)
    {
        return {};
    }
    const std::uint32_t fileAlign = bp.fileAlignment != 0 ? bp.fileAlignment : 0x200u;
    const std::uint32_t sectAlign = bp.sectionAlignment != 0 ? bp.sectionAlignment : 0x1000u;
    const std::uint64_t imageBase = bp.imageBase != 0 ? bp.imageBase : 0x0000000140000000ull;
    const std::uint32_t virtualSize = static_cast<std::uint32_t>(codeBytes.size());
    const std::uint32_t sizeOfRawData = alignUp(virtualSize, fileAlign);
    const std::uint32_t hdrUnpadded = kPe64NtFileOffset + kPe64SignatureSize + 20u + kPe64OptionalHeader64Size + 40u;
    const std::uint32_t sizeOfHeaders = alignUp(hdrUnpadded, fileAlign);
    const std::uint32_t ptrToRaw = sizeOfHeaders;
    const std::uint32_t sizeOfImage = alignUp(kPe64FirstSectionRva + virtualSize, sectAlign);

    std::vector<std::uint8_t> out;
    out.resize(kPe64NtFileOffset, 0);
    out[0] = 'M';
    out[1] = 'Z';
    writeLe32At(out, 0x3C, kPe64NtFileOffset);
    out.push_back('P');
    out.push_back('E');
    out.push_back(0);
    out.push_back(0);
    appendLe16(out, 0x8664);
    appendLe16(out, 1);
    appendLe32(out, 0);
    appendLe32(out, 0);
    appendLe32(out, 0);
    appendLe16(out, static_cast<std::uint16_t>(kPe64OptionalHeader64Size));
    appendLe16(out, 0x0022);
    const size_t optStart = out.size();
    out.resize(out.size() + kPe64OptionalHeader64Size, 0);
    writeLe16At(out, optStart + 0x00, 0x20B);
    out[optStart + 0x02] = 14;
    out[optStart + 0x03] = 0;
    writeLe32At(out, optStart + 0x04, sizeOfRawData);
    writeLe32At(out, optStart + 0x10, kPe64FirstSectionRva);
    writeLe32At(out, optStart + 0x14, kPe64FirstSectionRva);
    writeLe64At(out, optStart + 0x18, imageBase);
    writeLe32At(out, optStart + 0x20, sectAlign);
    writeLe32At(out, optStart + 0x24, fileAlign);
    writeLe16At(out, optStart + 0x28, 6);
    writeLe16At(out, optStart + 0x2A, 0);
    writeLe16At(out, optStart + 0x2C, 0);
    writeLe16At(out, optStart + 0x2E, 0);
    writeLe16At(out, optStart + 0x30, 6);
    writeLe16At(out, optStart + 0x32, 0);
    writeLe32At(out, optStart + 0x34, 0);
    writeLe32At(out, optStart + 0x38, sizeOfImage);
    writeLe32At(out, optStart + 0x3C, sizeOfHeaders);
    writeLe32At(out, optStart + 0x40, 0);
    writeLe16At(out, optStart + 0x44, 3);
    writeLe16At(out, optStart + 0x46, 0);
    writeLe64At(out, optStart + 0x48, 0x100000ull);
    writeLe64At(out, optStart + 0x50, 0x1000ull);
    writeLe64At(out, optStart + 0x58, 0x100000ull);
    writeLe64At(out, optStart + 0x60, 0x1000ull);
    writeLe32At(out, optStart + 0x68, 0);
    writeLe32At(out, optStart + 0x6C, 16);
    const char textName[] = ".text\0\0\0";
    for (char c : textName)
    {
        out.push_back(static_cast<std::uint8_t>(c));
    }
    appendLe32(out, virtualSize);
    appendLe32(out, kPe64FirstSectionRva);
    appendLe32(out, sizeOfRawData);
    appendLe32(out, ptrToRaw);
    appendLe32(out, 0);
    appendLe32(out, 0);
    appendLe16(out, 0);
    appendLe16(out, 0);
    appendLe32(out, 0x60000020u);
    while (out.size() < sizeOfHeaders)
    {
        out.push_back(0);
    }
    out.insert(out.end(), codeBytes.begin(), codeBytes.end());
    while (out.size() < ptrToRaw + sizeOfRawData)
    {
        out.push_back(0);
    }
    return out;
}

inline std::vector<std::uint8_t> composePe64MinimalImageFromManifest(const sovereign::TargetManifest& m,
                                                                     const std::vector<std::uint8_t>& codeBytes)
{
    if (m.objectFormat != sovereign::ObjectFormat::Pe)
    {
        return {};
    }
    return composePe64MinimalImage(emitBlueprintFromManifest(m), codeBytes);
}

// -----------------------------------------------------------------------------
// Mach-O 64: mach_header + LC_SEGMENT_64 (__TEXT + __text) + LC_MAIN; lab only.
// -----------------------------------------------------------------------------

inline constexpr std::uint32_t kMachOLcSegment64Cmd = 0x19u;
inline constexpr std::uint32_t kMachOLcMainCmd = 0x80000028u;
inline constexpr std::uint32_t kMachOHeaderPadTo = 0x1000u;

/// Minimal MH_EXECUTE: LC_SEGMENT_64 (__TEXT + one __text) + LC_MAIN; code at file
/// offset 0x1000. Lab-only — not codesigned; dyld may still reject on real macOS.
inline std::vector<std::uint8_t> composeMacho64MinimalImage(const EmitBlueprint& bp,
                                                            const std::vector<std::uint8_t>& codeBytes)
{
    if (bp.format != BinaryFormat::MachO64)
    {
        return {};
    }
    const std::uint64_t imageBase = bp.imageBase != 0 ? bp.imageBase : 0x0000000100000000ull;
    const std::uint32_t cpuType = bp.arch == TargetArch::Arm64 ? 0x0100000Cu : 0x01000007u;
    const std::uint32_t cpuSubtype = bp.arch == TargetArch::Arm64 ? 0u : 3u;
    const std::uint32_t codeSize = static_cast<std::uint32_t>(codeBytes.size());
    const std::uint32_t segFileSize = kMachOHeaderPadTo + codeSize;
    const std::uint64_t segVmSize = alignUp64(static_cast<std::uint64_t>(segFileSize), 0x1000ull);
    const std::uint64_t textAddr = imageBase + 0x1000ull;

    auto pushName16 = [](std::vector<std::uint8_t>& o, const char* name)
    {
        size_t i = 0;
        for (; i < 16 && name[i] != '\0'; ++i)
        {
            o.push_back(static_cast<std::uint8_t>(name[i]));
        }
        for (; i < 16; ++i)
        {
            o.push_back(0);
        }
    };

    std::vector<std::uint8_t> out;
    out.reserve(kMachOHeaderPadTo + codeSize + 0x200u);
    appendLe32(out, 0xFEEDFACFu);
    appendLe32(out, cpuType);
    appendLe32(out, cpuSubtype);
    appendLe32(out, 2u);
    appendLe32(out, 0u);
    appendLe32(out, 0u);
    appendLe32(out, 0x1u);
    appendLe32(out, 0u);
    appendLe32(out, kMachOLcSegment64Cmd);
    appendLe32(out, 152u);
    pushName16(out, "__TEXT");
    appendLe64(out, imageBase);
    appendLe64(out, segVmSize);
    appendLe64(out, 0ull);
    appendLe64(out, static_cast<std::uint64_t>(segFileSize));
    // maxprot / initprot are uint32_t in segment_command_64 (not uint64_t).
    appendLe32(out, 7u);
    appendLe32(out, 5u);
    appendLe32(out, 1u);
    appendLe32(out, 0u);
    pushName16(out, "__text");
    pushName16(out, "__TEXT");
    appendLe64(out, textAddr);
    appendLe64(out, static_cast<std::uint64_t>(codeSize));
    appendLe32(out, kMachOHeaderPadTo);
    appendLe32(out, 4u);
    appendLe32(out, 0u);
    appendLe32(out, 0u);
    appendLe32(out, 0x80000400u);
    appendLe32(out, 0u);
    appendLe32(out, 0u);
    appendLe32(out, 0u);
    appendLe32(out, kMachOLcMainCmd);
    appendLe32(out, 24u);
    appendLe64(out, static_cast<std::uint64_t>(kMachOHeaderPadTo));
    appendLe64(out, 0ull);
    constexpr std::uint32_t kLoadCmdsTotal = 152u + 24u;
    writeLe32At(out, 16u, 2u);
    writeLe32At(out, 20u, kLoadCmdsTotal);
    while (out.size() < kMachOHeaderPadTo)
    {
        out.push_back(0);
    }
    out.insert(out.end(), codeBytes.begin(), codeBytes.end());
    return out;
}

inline std::vector<std::uint8_t> composeMacho64MinimalImageFromManifest(const sovereign::TargetManifest& m,
                                                                        const std::vector<std::uint8_t>& codeBytes)
{
    if (m.objectFormat != sovereign::ObjectFormat::MachO)
    {
        return {};
    }
    return composeMacho64MinimalImage(emitBlueprintFromManifest(m), codeBytes);
}

}  // namespace rawrxd::sovereign::format
