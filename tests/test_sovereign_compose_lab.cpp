// Lab validation: file-based code blobs + compose*MinimalImage (tri-format envelope).
// Gold-standard structural checks: entry RVAs/VA, LC_MAIN entryoff, ELF p_filesz/p_memsz,
// PE SizeOfImage % SectionAlignment, PE CheckSum==0, Mach-O sizeofcmds / cputype;
// ingest layer: .a (ar), .obj, .asm.
// No host memory scraping — see include/rawrxd/sovereign_lab_blob_io.hpp.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "rawrxd/sovereign_emit_formats.hpp"
#include "rawrxd/sovereign_lab_blob_io.hpp"
#include "rawrxd/sovereign_target_manifest.hpp"

namespace
{
using namespace rawrxd::sovereign::format;
using namespace rawrxd::sovereign::lab;
using rawrxd::sovereign::ObjectFormat;
using rawrxd::sovereign::TargetManifest;
using rawrxd::sovereign::TargetOs;
using rawrxd::sovereign::format::writeLe32At;
using rawrxd::sovereign::lab::readLe64;

inline std::uint32_t readLe32(const std::vector<std::uint8_t>& b, size_t off)
{
    if (off + 4 > b.size())
    {
        return 0;
    }
    std::uint32_t v = 0;
    for (size_t i = 0; i < 4; ++i)
    {
        v |= static_cast<std::uint32_t>(b[off + i]) << (8 * i);
    }
    return v;
}

bool fail(const char* msg)
{
    std::fprintf(stderr, "[test_sovereign_compose_lab] FAIL: %s\n", msg);
    return false;
}

bool testSyntheticCoffExtract()
{
    // Minimal AMD64 COFF .obj: 20-byte file header + 40-byte section + 4 raw bytes.
    std::vector<std::uint8_t> f(64);
    f[0] = 0x64;
    f[1] = 0x86;  // IMAGE_FILE_MACHINE_AMD64
    f[2] = 1;
    f[3] = 0;  // NumberOfSections
    // TimeDateStamp, SymTable, NumSymbols at 4-15
    f[16] = 0;
    f[17] = 0;  // SizeOfOptionalHeader
    // Section ".text" at offset 20
    const char nm[] = ".text\0\0\0";
    std::memcpy(f.data() + 20, nm, 8);
    writeLe32At(f, 36, 4);            // SizeOfRawData
    writeLe32At(f, 40, 60);           // PointerToRawData
    writeLe32At(f, 56, 0x60000020u);  // Characteristics: code
    f[60] = 0xC3;                     // ret
    f[61] = 0x90;
    f[62] = 0x90;
    f[63] = 0x90;

    const auto text = tryExtractCoffSection(f, ".text");
    if (!text || text->size() != 4 || (*text)[0] != 0xC3)
    {
        return fail("COFF .text extract");
    }
    return true;
}

bool testReadAndComposeElf(const std::vector<std::uint8_t>& code)
{
    EmitBlueprint bp{};
    bp.format = BinaryFormat::Elf64;
    bp.arch = rawrxd::sovereign::format::TargetArch::X64;
    bp.imageBase = 0x400000ull;
    const auto img = composeElf64MinimalImage(bp, code);
    if (img.size() < 120)
    {
        return fail("ELF image too small");
    }
    if (readLe64(img, 96) != img.size())
    {
        return fail("ELF p_filesz mismatch");
    }
    return true;
}

bool testTempBinRoundTrip()
{
    namespace fs = std::filesystem;
    const fs::path tmp = fs::temp_directory_path() / "rawrxd_compose_lab_ret.bin";
    {
        std::ofstream out(tmp, std::ios::binary);
        const unsigned char b[] = {0xC3};
        out.write(reinterpret_cast<const char*>(b), 1);
    }
    const auto r = readWholeFile(tmp);
    if (!r || r.data.size() != 1 || r.data[0] != 0xC3)
    {
        return fail("readWholeFile temp .bin");
    }
    if (!testReadAndComposeElf(r.data))
    {
        return false;
    }
    std::error_code ec;
    fs::remove(tmp, ec);
    return true;
}

bool testManifestPeMachCompose()
{
    TargetManifest m{};
    m.os = TargetOs::Windows;
    m.arch = rawrxd::sovereign::TargetArch::X86_64;
    m.objectFormat = ObjectFormat::Pe;
    const std::vector<std::uint8_t> code = {0xC3};
    const auto pe = composePe64MinimalImageFromManifest(m, code);
    if (pe.empty())
    {
        return fail("PE compose");
    }
    m.os = TargetOs::MacOS;
    m.objectFormat = ObjectFormat::MachO;
    const auto mach = composeMacho64MinimalImageFromManifest(m, code);
    if (mach.empty() || mach.size() < 0x1000u)
    {
        return fail("Mach-O compose");
    }
    return true;
}

/// Gold-standard structural checks (NOP; ret payload): entry VA/RVA, LC_MAIN.entryoff.
bool testTriFormatEntryContract()
{
    const std::vector<std::uint8_t> code = {0x90, 0xC3};

    EmitBlueprint elfBp{};
    elfBp.format = BinaryFormat::Elf64;
    elfBp.arch = rawrxd::sovereign::format::TargetArch::X64;
    elfBp.imageBase = 0x400000ull;
    const auto elf = composeElf64MinimalImage(elfBp, code);
    if (elf.size() < 8 || elf[0] != 0x7F || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F')
    {
        return fail("ELF magic");
    }
    const std::uint64_t hdr = static_cast<std::uint64_t>(kElf64EhdrSize + kElf64PhdrSize);
    const std::uint64_t expectEntry = 0x400000ull + hdr;
    if (readLe64(elf, 24) != expectEntry)
    {
        return fail("ELF e_entry");
    }
    const std::uint64_t pFilesz = readLe64(elf, 96);
    const std::uint64_t pMemsz = readLe64(elf, 104);
    if (pFilesz != static_cast<std::uint64_t>(elf.size()))
    {
        return fail("ELF p_filesz");
    }
    if (pMemsz < pFilesz)
    {
        return fail("ELF p_memsz < p_filesz");
    }
    if (elf.size() < hdr + code.size() || elf[hdr] != 0x90 || elf[hdr + 1] != 0xC3)
    {
        return fail("ELF first instructions");
    }

    TargetManifest mWin{};
    mWin.os = TargetOs::Windows;
    mWin.arch = rawrxd::sovereign::TargetArch::X86_64;
    mWin.objectFormat = ObjectFormat::Pe;
    const auto pe = composePe64MinimalImageFromManifest(mWin, code);
    if (pe.size() < 0x100u || pe[0] != 'M' || pe[1] != 'Z')
    {
        return fail("PE MZ");
    }
    constexpr size_t kPeOptStart = 0x98;
    constexpr size_t kPeAddressOfEntryRva = kPeOptStart + 0x10;
    constexpr size_t kPeSectionAlignmentOff = kPeOptStart + 0x20;
    constexpr size_t kPeSizeOfImageOff = kPeOptStart + 0x38;
    constexpr size_t kPeSizeOfHeadersOff = kPeOptStart + 0x3C;
    constexpr size_t kPeCheckSumOff = kPeOptStart + 0x40;  // IMAGE_OPTIONAL_HEADER64.CheckSum
    if (readLe32(pe, kPeAddressOfEntryRva) != kPe64FirstSectionRva)
    {
        return fail("PE AddressOfEntryPoint");
    }
    const std::uint32_t sectionAlignment = readLe32(pe, kPeSectionAlignmentOff);
    const std::uint32_t sizeOfImagePe = readLe32(pe, kPeSizeOfImageOff);
    if (sectionAlignment == 0 || (sizeOfImagePe % sectionAlignment) != 0)
    {
        return fail("PE SizeOfImage % SectionAlignment");
    }
    if (readLe32(pe, kPeCheckSumOff) != 0u)
    {
        return fail("PE OptionalHeader CheckSum");
    }
    const std::uint32_t sizeOfHeaders = readLe32(pe, kPeSizeOfHeadersOff);
    if (sizeOfHeaders == 0 || pe.size() < sizeOfHeaders + code.size())
    {
        return fail("PE SizeOfHeaders / span");
    }
    if (pe[sizeOfHeaders] != 0x90 || pe[sizeOfHeaders + 1] != 0xC3)
    {
        return fail("PE .text start");
    }

    TargetManifest mMac{};
    mMac.os = TargetOs::MacOS;
    mMac.arch = rawrxd::sovereign::TargetArch::X86_64;
    mMac.objectFormat = ObjectFormat::MachO;
    const auto mach = composeMacho64MinimalImageFromManifest(mMac, code);
    if (readLe32(mach, 0) != 0xFEEDFACFu)
    {
        return fail("Mach-O MH_MAGIC_64");
    }
    // mach/machine.h — CPU_TYPE_X86_64 / CPU_TYPE_ARM64
    constexpr std::uint32_t kCpuTypeX8664 = 0x01000007u;
    constexpr std::uint32_t kCpuTypeArm64 = 0x0100000Cu;
    if (readLe32(mach, 4) != kCpuTypeX8664)
    {
        return fail("Mach-O cputype x86_64");
    }
    constexpr std::uint32_t kExpectedMachOSizeOfCmds = 152u + 24u;
    if (readLe32(mach, 20) != kExpectedMachOSizeOfCmds)
    {
        return fail("Mach-O sizeofcmds");
    }
    // LC_MAIN begins after mach_header (32) + LC_SEGMENT_64 (152) = 184; entryoff at 184+8 = 192.
    constexpr size_t kLcMainEntryoffOff = 192;
    if (readLe64(mach, kLcMainEntryoffOff) != static_cast<std::uint64_t>(kMachOHeaderPadTo))
    {
        return fail("Mach-O LC_MAIN entryoff");
    }
    // Load commands size must be 8-byte aligned (dyld parses uint64_t-aligned commands).
    if ((readLe32(mach, 20) % 8u) != 0u)
    {
        return fail("Mach-O sizeofcmds % 8");
    }
    if (mach.size() < kMachOHeaderPadTo + code.size() || mach[kMachOHeaderPadTo] != 0x90 ||
        mach[kMachOHeaderPadTo + 1] != 0xC3)
    {
        return fail("Mach-O __text start");
    }

    TargetManifest mMacArm{};
    mMacArm.os = TargetOs::MacOS;
    mMacArm.arch = rawrxd::sovereign::TargetArch::Arm64;
    mMacArm.objectFormat = ObjectFormat::MachO;
    const auto machArm = composeMacho64MinimalImageFromManifest(mMacArm, code);
    if (machArm.size() < 32 || readLe32(machArm, 4) != kCpuTypeArm64)
    {
        return fail("Mach-O cputype arm64");
    }

    // Multi-page payload: 5000 bytes — SizeOfImage / p_memsz / segment vmsize must roll forward.
    std::vector<std::uint8_t> big(5000u, 0x90);
    const std::uint64_t page = elfBp.sectionAlignment != 0 ? static_cast<std::uint64_t>(elfBp.sectionAlignment) : 0x1000ull;

    const auto elfBig = composeElf64MinimalImage(elfBp, big);
    const std::uint64_t elfTotal = static_cast<std::uint64_t>(elfBig.size());
    const std::uint64_t pMemBig = readLe64(elfBig, 104);
    if (pMemBig != alignUp64(elfTotal, page))
    {
        return fail("ELF multi-page p_memsz");
    }
    if ((pMemBig % page) != 0)
    {
        return fail("ELF p_memsz page aligned");
    }

    const auto peBig = composePe64MinimalImageFromManifest(mWin, big);
    const std::uint32_t vs = static_cast<std::uint32_t>(big.size());
    if (readLe32(peBig, kPeSizeOfImageOff) != alignUp(kPe64FirstSectionRva + vs, sectionAlignment))
    {
        return fail("PE multi-page SizeOfImage");
    }
    if (readLe32(peBig, kPeCheckSumOff) != 0u)
    {
        return fail("PE multi-page CheckSum");
    }

    const auto machBig = composeMacho64MinimalImageFromManifest(mMac, big);
    if ((readLe32(machBig, 20) % 8u) != 0u)
    {
        return fail("Mach-O multi-page sizeofcmds % 8");
    }
    const std::uint64_t segVm = readLe64(machBig, 64);
    const std::uint32_t csBig = static_cast<std::uint32_t>(big.size());
    const std::uint64_t expectSegVm = alignUp64(static_cast<std::uint64_t>(kMachOHeaderPadTo + csBig), 0x1000ull);
    if (segVm != expectSegVm)
    {
        return fail("Mach-O multi-page __TEXT vmsize");
    }

    return true;
}

/// Ingest layer: classic `ar` + COFF `.obj` + `.asm` source passthrough (see sovereign_lab_blob_io.hpp).
bool testLabIngestLayerArObjAsm()
{
    std::vector<std::uint8_t> coff(64);
    coff[0] = 0x64;
    coff[1] = 0x86;
    coff[2] = 1;
    coff[3] = 0;
    coff[16] = 0;
    coff[17] = 0;
    const char nm[] = ".text\0\0\0";
    std::memcpy(coff.data() + 20, nm, 8);
    writeLe32At(coff, 36, 4);
    writeLe32At(coff, 40, 60);
    writeLe32At(coff, 56, 0x60000020u);
    coff[60] = 0xC3;
    coff[61] = 0x90;
    coff[62] = 0x90;
    coff[63] = 0x90;

    std::vector<std::uint8_t> ar;
    static const char kMag[] = "!<arch>\n";
    ar.insert(ar.end(), kMag, kMag + 8);
    char h[60];
    std::memset(h, ' ', 60);
    {
        const char name[] = "tiny.o/";
        for (std::size_t k = 0; k < sizeof(name) - 1 && k < 16; ++k)
        {
            h[k] = name[k];
        }
    }
    {
        char szbuf[16];
        std::snprintf(szbuf, sizeof(szbuf), "%10llu", static_cast<unsigned long long>(coff.size()));
        std::memcpy(h + 48, szbuf, 10);
    }
    h[58] = '`';
    h[59] = '\n';
    ar.insert(ar.end(), h, h + 60);
    ar.insert(ar.end(), coff.begin(), coff.end());
    if ((coff.size() & 1u) != 0u)
    {
        ar.push_back('\n');
    }

    const auto arText = tryExtractCodeBlobFromArArchive(ar, ".text");
    if (!arText || arText->size() != 4 || (*arText)[0] != 0xC3)
    {
        return fail("ar -> COFF .text");
    }

    const LabIngestOutcome inA = ingestLabBlobFromBytes(std::move(ar), std::filesystem::path("x.a"));
    if (!inA || !inA.isMachineCode() || inA.data.size() != 4 || inA.data[0] != 0xC3)
    {
        return fail("ingest .a");
    }

    const LabIngestOutcome inObj = ingestLabBlobFromBytes(std::move(coff), std::filesystem::path("x.obj"));
    if (!inObj || !inObj.isMachineCode() || inObj.data.size() != 4 || inObj.data[0] != 0xC3)
    {
        return fail("ingest .obj");
    }

    namespace fs = std::filesystem;
    const fs::path asmPath = fs::temp_directory_path() / "rawrxd_lab_ingest.asm";
    {
        std::ofstream out(asmPath, std::ios::binary);
        out << "nop\nret\n";
    }
    const LabIngestOutcome inAsm = ingestLabBlobFromFile(asmPath);
    std::error_code ec;
    fs::remove(asmPath, ec);
    if (!inAsm || !inAsm.isAsmSource() || inAsm.data.empty())
    {
        return fail("ingest .asm");
    }

    return true;
}

}  // namespace

int main()
{
    if (!testSyntheticCoffExtract())
    {
        return 1;
    }
    if (!testReadAndComposeElf({0xC3}))
    {
        return 1;
    }
    if (!testTempBinRoundTrip())
    {
        return 1;
    }
    if (!testManifestPeMachCompose())
    {
        return 1;
    }
    if (!testTriFormatEntryContract())
    {
        return 1;
    }
    if (!testLabIngestLayerArObjAsm())
    {
        return 1;
    }
    std::puts("[test_sovereign_compose_lab] OK");
    return 0;
}
