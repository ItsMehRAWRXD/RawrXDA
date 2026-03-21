// Regression: tri-format compose* header magics + patched size fields (patch-and-append).
// Complements test_sovereign_compose_lab.cpp (blob I/O + contracts).

#include <cstdio>
#include <vector>

#include "rawrxd/sovereign_emit_formats.hpp"

namespace
{
using namespace rawrxd::sovereign::format;

inline std::uint32_t readLe32(const std::vector<std::uint8_t>& b, std::size_t off)
{
    if (off + 4 > b.size())
    {
        return 0;
    }
    std::uint32_t v = 0;
    for (std::size_t i = 0; i < 4; ++i)
    {
        v |= static_cast<std::uint32_t>(b[off + i]) << (8 * i);
    }
    return v;
}

inline std::uint64_t readLe64(const std::vector<std::uint8_t>& b, std::size_t off)
{
    if (off + 8 > b.size())
    {
        return 0;
    }
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < 8; ++i)
    {
        v |= static_cast<std::uint64_t>(b[off + i]) << (8 * i);
    }
    return v;
}

bool fail(const char* msg)
{
    std::fprintf(stderr, "[sovereign_header_tests] FAIL: %s\n", msg);
    return false;
}

bool testElf64NopAndPaging()
{
    const std::vector<std::uint8_t> nop1 = {0x90};
    EmitBlueprint bp{};
    bp.format = BinaryFormat::Elf64;
    bp.arch = TargetArch::X64;
    bp.imageBase = 0x400000ull;
    bp.sectionAlignment = 0x1000u;

    const auto img = composeElf64MinimalImage(bp, nop1);
    if (img.size() < 8 || img[0] != 0x7F || img[1] != 'E' || img[2] != 'L' || img[3] != 'F')
    {
        return fail("ELF magic");
    }
    if (img[4] != 2 || img[5] != 1)
    {
        return fail("ELF class / data");
    }
    const std::uint64_t total = static_cast<std::uint64_t>(img.size());
    const std::uint64_t expectMem = alignUp64(total, 0x1000ull);
    if (readLe64(img, 96) != total)
    {
        return fail("ELF p_filesz");
    }
    if (readLe64(img, 104) != expectMem)
    {
        return fail("ELF p_memsz page align");
    }

    std::vector<std::uint8_t> big(4097u, 0x90);
    const auto bigImg = composeElf64MinimalImage(bp, big);
    const std::uint64_t bigTotal = static_cast<std::uint64_t>(bigImg.size());
    if (readLe64(bigImg, 104) != alignUp64(bigTotal, 0x1000ull))
    {
        return fail("ELF p_memsz multi-page");
    }
    return true;
}

bool testPe64NopAndSizeOfImage()
{
    const std::vector<std::uint8_t> nop1 = {0x90};
    EmitBlueprint bp{};
    bp.format = BinaryFormat::Pe64;
    bp.arch = TargetArch::X64;
    bp.imageBase = 0x140000000ull;
    bp.fileAlignment = 0x200u;
    bp.sectionAlignment = 0x1000u;

    const auto pe = composePe64MinimalImage(bp, nop1);
    if (pe.size() < 0x100u || pe[0] != 'M' || pe[1] != 'Z')
    {
        return fail("PE MZ");
    }
    if (readLe32(pe, 0x3C) != kPe64NtFileOffset)
    {
        return fail("PE e_lfanew");
    }
    if (pe[kPe64NtFileOffset] != 'P' || pe[kPe64NtFileOffset + 1] != 'E')
    {
        return fail("PE signature");
    }
    constexpr std::size_t kOpt = kPe64NtFileOffset + 4 + 20;
    if (readLe32(pe, kOpt + 0x38) != alignUp(kPe64FirstSectionRva + 1u, 0x1000u))
    {
        return fail("PE SizeOfImage small payload");
    }
    if (readLe32(pe, kOpt + 0x10) != kPe64FirstSectionRva)
    {
        return fail("PE AddressOfEntryPoint");
    }

    std::vector<std::uint8_t> big(5000u, 0x90);
    const auto peBig = composePe64MinimalImage(bp, big);
    const std::uint32_t vs = static_cast<std::uint32_t>(big.size());
    if (readLe32(peBig, kOpt + 0x38) != alignUp(kPe64FirstSectionRva + vs, 0x1000u))
    {
        return fail("PE SizeOfImage large payload");
    }
    return true;
}

bool testMacho64NopX64AndArm64()
{
    // AArch64 NOP (little-endian encoding of 0xD503201F).
    const std::vector<std::uint8_t> nopA64 = {0x1Fu, 0x20u, 0x03u, 0xD5u};

    EmitBlueprint bpX{};
    bpX.format = BinaryFormat::MachO64;
    bpX.arch = TargetArch::X64;
    bpX.imageBase = 0x100000000ull;
    const auto mx = composeMacho64MinimalImage(bpX, nopA64);
    if (readLe32(mx, 0) != 0xFEEDFACFu)
    {
        return fail("Mach-O MH_MAGIC_64");
    }
    if (readLe32(mx, 4) != 0x01000007u)
    {
        return fail("Mach-O CPU_TYPE_X86_64");
    }
    const std::uint32_t cs = static_cast<std::uint32_t>(nopA64.size());
    const std::uint64_t segFile = static_cast<std::uint64_t>(kMachOHeaderPadTo + cs);
    const std::uint64_t segVm = alignUp64(segFile, 0x1000ull);
    if (readLe64(mx, 64) != segVm)
    {
        return fail("Mach-O __TEXT vmsize");
    }
    // LC_MAIN follows 32-byte mach_header + 152-byte LC_SEGMENT_64; entryoff at cmd+8.
    if (readLe64(mx, 192) != static_cast<std::uint64_t>(kMachOHeaderPadTo))
    {
        return fail("Mach-O LC_MAIN entryoff");
    }
    if (mx.size() < kMachOHeaderPadTo + cs || mx[kMachOHeaderPadTo] != 0x1F)
    {
        return fail("Mach-O first code byte");
    }

    EmitBlueprint bpA{};
    bpA.format = BinaryFormat::MachO64;
    bpA.arch = TargetArch::Arm64;
    bpA.imageBase = 0x100000000ull;
    const auto ma = composeMacho64MinimalImage(bpA, nopA64);
    if (readLe32(ma, 4) != 0x0100000Cu)
    {
        return fail("Mach-O CPU_TYPE_ARM64");
    }
    if (readLe64(ma, 64) != segVm)
    {
        return fail("Mach-O ARM64 vmsize");
    }
    return true;
}

}  // namespace

int main()
{
    if (!testElf64NopAndPaging())
    {
        return 1;
    }
    if (!testPe64NopAndSizeOfImage())
    {
        return 1;
    }
    if (!testMacho64NopX64AndArm64())
    {
        return 1;
    }
    std::puts("[sovereign_header_tests] OK");
    return 0;
}
