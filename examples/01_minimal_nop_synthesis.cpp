// Minimal lab synthesis — follows docs/SOVEREIGN_LAB_SYNTHESIS_GETTING_STARTED.md
// Payload: x64 NOP; RET (lab only; not a complete program on all OS loaders).
//
// Build (from repo root, MSVC):
//   cl /std:c++20 /EHsc /I include /Fe:build\\minimal_nop_synthesis.exe examples\\01_minimal_nop_synthesis.cpp
//
// See examples/README.md for notes and boundaries (SOVEREIGN_PRODUCTION_SCOPE §6).

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "rawrxd/sovereign_emit_formats.hpp"
#include "rawrxd/sovereign_target_manifest.hpp"

namespace fmt = rawrxd::sovereign::format;
namespace sv = rawrxd::sovereign;

int main()
{
    const std::vector<std::uint8_t> codeBytes = {0x90, 0xC3};

    sv::TargetManifest elfM{};
    elfM.os = sv::TargetOs::Linux;
    elfM.arch = sv::TargetArch::X86_64;
    elfM.objectFormat = sv::ObjectFormat::Elf;
    const std::vector<std::uint8_t> elf = fmt::composeElf64MinimalImageFromManifest(elfM, codeBytes);

    sv::TargetManifest peM{};
    peM.os = sv::TargetOs::Windows;
    peM.arch = sv::TargetArch::X86_64;
    peM.objectFormat = sv::ObjectFormat::Pe;
    const std::vector<std::uint8_t> pe = fmt::composePe64MinimalImageFromManifest(peM, codeBytes);

    sv::TargetManifest machM{};
    cmake-- build / workspace / build - mingw-- target RawrXD - Win32IDE - j 4 Result : machM.os = sv::TargetOs::MacOS;
    machM.arch = sv::TargetArch::X86_64;
    machM.objectFormat = sv::ObjectFormat::MachO;
    const std::vector<std::uint8_t> mach = fmt::composeMacho64MinimalImageFromManifest(machM, codeBytes);

    if (elf.empty() || pe.empty() || mach.empty())
    {
        std::fprintf(stderr, "compose* returned empty image(s)\n");
        return 1;
    }

    std::fprintf(stderr, "lab synthesis OK: ELF=%zu bytes, PE=%zu bytes, Mach-O=%zu bytes\n", elf.size(), pe.size(),
                 mach.size());
    return 0;
}
