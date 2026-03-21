// =============================================================================
// rawrxd/sovereign_lab_blob_io.hpp
// File-based code blob loading for lab / tests: read raw .bin or extract the
// `.text` payload from COFF (MSVC .obj), classic `ar` archives (.a / many .lib),
// ELF64 relocatable (.o), or pass through textual `.asm` / `.inc` for an
// external assembler. Feeds `compose*MinimalImage` in sovereign_emit_formats.hpp
// — see docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md.
// =============================================================================

#pragma once

#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rawrxd::sovereign::lab
{

struct ReadWholeFileResult
{
    std::vector<std::uint8_t> data{};
    std::string error{};  // empty on success

    explicit operator bool() const { return error.empty(); }
};

/// Reads an entire file into memory (e.g. flat machine-code `.bin`).
inline ReadWholeFileResult readWholeFile(const std::filesystem::path& path)
{
    ReadWholeFileResult out;
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
    {
        out.error = "open failed: " + path.string();
        return out;
    }
    const auto sz = static_cast<std::size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    out.data.resize(sz);
    if (sz > 0 && !f.read(reinterpret_cast<char*>(out.data.data()), static_cast<std::streamsize>(sz)))
    {
        out.data.clear();
        out.error = "read failed: " + path.string();
        return out;
    }
    return out;
}

inline std::uint16_t readLe16(const std::vector<std::uint8_t>& b, std::size_t o)
{
    if (o + 2 > b.size())
    {
        return 0;
    }
    return static_cast<std::uint16_t>(b[o]) | (static_cast<std::uint16_t>(b[o + 1]) << 8);
}

inline std::uint32_t readLe32(const std::vector<std::uint8_t>& b, std::size_t o)
{
    if (o + 4 > b.size())
    {
        return 0;
    }
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
    {
        v |= static_cast<std::uint32_t>(b[o + static_cast<std::size_t>(i)]) << (8 * i);
    }
    return v;
}

inline std::uint64_t readLe64(const std::vector<std::uint8_t>& b, std::size_t o)
{
    if (o + 8 > b.size())
    {
        return 0;
    }
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
    {
        v |= static_cast<std::uint64_t>(b[o + static_cast<std::size_t>(i)]) << (8 * i);
    }
    return v;
}

inline bool sectionNameMatches(const std::vector<std::uint8_t>& name8, std::string_view want)
{
    if (want.size() > 8)
    {
        return false;
    }
    for (std::size_t i = 0; i < 8; ++i)
    {
        const char c = i < want.size() ? want[i] : '\0';
        if (static_cast<char>(name8[i]) != c)
        {
            return false;
        }
    }
    return true;
}

/// MSVC COFF object: find a section by short name (≤8 chars, not "/n" indirection).
/// Returns raw bytes for that section (SizeOfRawData at PointerToRawData).
inline std::optional<std::vector<std::uint8_t>> tryExtractCoffSection(const std::vector<std::uint8_t>& file,
                                                                      std::string_view sectionName)
{
    if (file.size() < 20)
    {
        return std::nullopt;
    }
    const std::uint16_t nsects = readLe16(file, 2);
    const std::uint16_t opt = readLe16(file, 16);  // SizeOfOptionalHeader
    if (nsects == 0 || file.size() < 20ull + static_cast<std::uint64_t>(opt) +
                                       static_cast<std::uint64_t>(nsects) * 40ull)
    {
        return std::nullopt;
    }
    const std::size_t sec0 = 20u + static_cast<std::size_t>(opt);
    for (std::uint16_t i = 0; i < nsects; ++i)
    {
        const std::size_t sh = sec0 + static_cast<std::size_t>(i) * 40u;
        if (sh + 40 > file.size())
        {
            return std::nullopt;
        }
        std::vector<std::uint8_t> name8(8);
        for (int j = 0; j < 8; ++j)
        {
            name8[static_cast<std::size_t>(j)] = file[sh + static_cast<std::size_t>(j)];
        }
        if (!sectionNameMatches(name8, sectionName))
        {
            continue;
        }
        const std::uint32_t rawSize = readLe32(file, sh + 16);
        const std::uint32_t ptrRaw = readLe32(file, sh + 20);
        if (ptrRaw + rawSize > file.size())
        {
            return std::nullopt;
        }
        return std::vector<std::uint8_t>(file.begin() + static_cast<std::ptrdiff_t>(ptrRaw),
                                         file.begin() + static_cast<std::ptrdiff_t>(ptrRaw + rawSize));
    }
    return std::nullopt;
}

inline bool isElf64Little(const std::vector<std::uint8_t>& b)
{
    return b.size() >= 6 && b[0] == 0x7F && b[1] == 'E' && b[2] == 'L' && b[3] == 'F' && b[4] == 2 && b[5] == 1;
}

/// ELF64 ET_REL: extract SHT_PROGBITS section by name (via shstrtab). Lab-only.
inline std::optional<std::vector<std::uint8_t>> tryExtractElf64ProgBitsSection(
    const std::vector<std::uint8_t>& file, std::string_view sectionName)
{
    if (!isElf64Little(file) || file.size() < 64)
    {
        return std::nullopt;
    }
    const std::uint16_t e_type = readLe16(file, 16);
    const std::uint16_t e_machine = readLe16(file, 18);
    if (e_type != 1u || e_machine != 62u)
    {
        return std::nullopt;
    }
    const std::uint64_t e_shoff = readLe64(file, 40);
    const std::uint16_t e_shentsize = readLe16(file, 58);
    const std::uint16_t e_shnum = readLe16(file, 60);
    const std::uint16_t e_shstrndx = readLe16(file, 62);
    if (e_shoff == 0 || e_shnum == 0 || e_shentsize < 64 || e_shstrndx >= e_shnum)
    {
        return std::nullopt;
    }
    const std::size_t shstr_off = static_cast<std::size_t>(e_shoff) +
                                  static_cast<std::size_t>(e_shstrndx) * static_cast<std::size_t>(e_shentsize);
    if (shstr_off + 64 > file.size())
    {
        return std::nullopt;
    }
    const std::uint64_t strtab_fileoff = readLe64(file, shstr_off + 24);
    const std::uint64_t strtab_size = readLe64(file, shstr_off + 32);
    if (strtab_fileoff + strtab_size > file.size())
    {
        return std::nullopt;
    }
    auto stringAt = [&](std::uint32_t nameOff) -> std::string_view
    {
        if (static_cast<std::uint64_t>(nameOff) >= strtab_size)
        {
            return {};
        }
        const char* p = reinterpret_cast<const char*>(file.data()) + strtab_fileoff + nameOff;
        const char* end = reinterpret_cast<const char*>(file.data()) + strtab_fileoff + strtab_size;
        const char* q = p;
        while (q < end && *q != '\0')
        {
            ++q;
        }
        return std::string_view(p, static_cast<std::size_t>(q - p));
    };

    for (std::uint16_t i = 0; i < e_shnum; ++i)
    {
        const std::size_t sh = static_cast<std::size_t>(e_shoff) +
                               static_cast<std::size_t>(i) * static_cast<std::size_t>(e_shentsize);
        if (sh + 64 > file.size())
        {
            return std::nullopt;
        }
        const std::uint32_t sh_name = readLe32(file, sh);
        const std::uint32_t sh_type = readLe32(file, sh + 4);
        if (sh_type != 1u)
        {
            continue;
        }
        if (stringAt(sh_name) != sectionName)
        {
            continue;
        }
        const std::uint64_t off = readLe64(file, sh + 24);
        const std::uint64_t sz = readLe64(file, sh + 32);
        if (off + sz > file.size())
        {
            return std::nullopt;
        }
        return std::vector<std::uint8_t>(file.begin() + static_cast<std::ptrdiff_t>(off),
                                         file.begin() + static_cast<std::ptrdiff_t>(off + sz));
    }
    return std::nullopt;
}

// -----------------------------------------------------------------------------
// Classic GNU/BSD `ar` (magic `!<arch>\n`) — walk members; first COFF or ELF64
// relocatable with a matching `.text` wins. Thin archives (`!<thin>\n`) not
// supported here.
// -----------------------------------------------------------------------------

inline bool isArClassicArchiveMagic(const std::vector<std::uint8_t>& file)
{
    return file.size() >= 8 && std::memcmp(file.data(), "!<arch>\n", 8) == 0;
}

inline bool isArThinArchiveMagic(const std::vector<std::uint8_t>& file)
{
    return file.size() >= 8 && std::memcmp(file.data(), "!<thin>\n", 8) == 0;
}

inline std::optional<std::vector<std::uint8_t>> tryExtractCodeBlobFromArArchive(
    const std::vector<std::uint8_t>& ar, std::string_view sectionName = ".text")
{
    if (!isArClassicArchiveMagic(ar) || ar.size() < 8u + 60u)
    {
        return std::nullopt;
    }
    std::size_t pos = 8;
    while (pos + 60 <= ar.size())
    {
        if (ar[pos + 58] != 0x60 || ar[pos + 59] != static_cast<std::uint8_t>('\n'))
        {
            return std::nullopt;
        }
        const char* szp = reinterpret_cast<const char*>(ar.data() + pos + 48);
        std::uint64_t bodySize = 0;
        bool anyDigit = false;
        for (int i = 0; i < 10; ++i)
        {
            const unsigned char c = static_cast<unsigned char>(szp[i]);
            if (c == ' ')
            {
                if (anyDigit)
                {
                    break;
                }
                continue;
            }
            if (c < '0' || c > '9')
            {
                return std::nullopt;
            }
            anyDigit = true;
            bodySize = bodySize * 10ull + static_cast<std::uint64_t>(c - '0');
        }
        if (!anyDigit)
        {
            return std::nullopt;
        }
        const std::size_t bodyStart = pos + 60;
        if (bodyStart + bodySize > ar.size())
        {
            return std::nullopt;
        }
        std::vector<std::uint8_t> member(ar.begin() + static_cast<std::ptrdiff_t>(bodyStart),
                                         ar.begin() + static_cast<std::ptrdiff_t>(bodyStart + bodySize));
        if (auto c = tryExtractCoffSection(member, sectionName))
        {
            return c;
        }
        if (auto e = tryExtractElf64ProgBitsSection(member, sectionName))
        {
            return e;
        }
        std::size_t padded = bodySize;
        if ((padded & 1u) != 0u)
        {
            ++padded;
        }
        pos = bodyStart + padded;
    }
    return std::nullopt;
}

// -----------------------------------------------------------------------------
// Unified ingest: .obj (COFF), .o (ELF64 rel), .a/.lib (classic ar), .bin (raw),
// `.asm` / `.inc` (source text — not machine code; assemble before compose).
// -----------------------------------------------------------------------------

enum class LabIngestKind : std::uint8_t
{
    MachineCode = 0,
    AsmSource = 1,
};

struct LabIngestOutcome
{
    std::vector<std::uint8_t> data{};
    std::string error{};
    LabIngestKind kind{LabIngestKind::MachineCode};

    explicit operator bool() const { return error.empty(); }
    [[nodiscard]] bool isMachineCode() const { return kind == LabIngestKind::MachineCode; }
    [[nodiscard]] bool isAsmSource() const { return kind == LabIngestKind::AsmSource; }
};

inline std::string pathExtensionLower(const std::filesystem::path& p)
{
    std::string e = p.extension().string();
    for (char& c : e)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return e;
}

inline LabIngestOutcome ingestLabBlobFromBytes(std::vector<std::uint8_t> bytes,
                                               const std::filesystem::path& pathForExtension,
                                               std::string_view sectionName = ".text")
{
    LabIngestOutcome out;
    const std::string ext = pathExtensionLower(pathForExtension);

    if (ext == ".asm" || ext == ".inc")
    {
        out.data = std::move(bytes);
        out.kind = LabIngestKind::AsmSource;
        return out;
    }

    if (isArThinArchiveMagic(bytes))
    {
        out.error = "thin GNU ar (!<thin>) not supported by lab ingest";
        return out;
    }

    if (ext == ".a" || ext == ".lib")
    {
        if (isArClassicArchiveMagic(bytes))
        {
            if (auto t = tryExtractCodeBlobFromArArchive(bytes, sectionName))
            {
                out.data = std::move(*t);
                return out;
            }
            out.error = "archive: no COFF/ELF64 .text member found";
            return out;
        }
        if (auto t = tryExtractCoffSection(bytes, sectionName))
        {
            out.data = std::move(*t);
            return out;
        }
        out.error = ".lib/.a: not a classic ar and not a raw COFF object";
        return out;
    }

    if (ext == ".obj")
    {
        if (auto t = tryExtractCoffSection(bytes, sectionName))
        {
            out.data = std::move(*t);
            return out;
        }
        out.error = "COFF .obj: .text extract failed";
        return out;
    }

    if (ext == ".o" || ext == ".lo")
    {
        if (auto t = tryExtractElf64ProgBitsSection(bytes, sectionName))
        {
            out.data = std::move(*t);
            return out;
        }
        out.error = "ELF64 relocatable: .text extract failed";
        return out;
    }

    if (ext == ".bin")
    {
        out.data = std::move(bytes);
        return out;
    }

    // No / unknown extension: sniff
    if (isElf64Little(bytes))
    {
        if (auto t = tryExtractElf64ProgBitsSection(bytes, sectionName))
        {
            out.data = std::move(*t);
            return out;
        }
    }
    if (isArClassicArchiveMagic(bytes))
    {
        if (auto t = tryExtractCodeBlobFromArArchive(bytes, sectionName))
        {
            out.data = std::move(*t);
            return out;
        }
    }
    {
        const std::uint16_t mach = readLe16(bytes, 0);
        if (mach == 0x8664u || mach == 0x014cu)
        {
            if (auto t = tryExtractCoffSection(bytes, sectionName))
            {
                out.data = std::move(*t);
                return out;
            }
        }
    }
    out.data = std::move(bytes);
    return out;
}

inline LabIngestOutcome ingestLabBlobFromFile(const std::filesystem::path& path,
                                              std::string_view sectionName = ".text")
{
    LabIngestOutcome out;
    const auto rf = readWholeFile(path);
    if (!rf)
    {
        out.error = rf.error;
        return out;
    }
    return ingestLabBlobFromBytes(std::move(rf.data), path, sectionName);
}

}  // namespace rawrxd::sovereign::lab
