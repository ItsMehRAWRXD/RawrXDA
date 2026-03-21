// Larger raw-code wrap — same tri-format compose* path as 01, with a multi-byte payload.
// "Shellcode" here means *unlinked machine bytes* (lab jargon), not exploit tooling.
// The built-in demo uses only 0x90 (NOP) + 0xC3 (RET). For your own bytes, pass a file path.
//
// Build (from repo root, MSVC):
//   cl /std:c++20 /EHsc /I include /Fe:build\\shellcode_payload_wrap.exe examples\\02_shellcode_payload_wrap.cpp
//
// Run:
//   build\\shellcode_payload_wrap.exe
//   build\\shellcode_payload_wrap.exe mybytes.bin
//
// Boundaries: docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md §6 — lab synthesis only.

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "rawrxd/sovereign_emit_formats.hpp"
#include "rawrxd/sovereign_lab_blob_io.hpp"
#include "rawrxd/sovereign_target_manifest.hpp"

namespace fmt = rawrxd::sovereign::format;
namespace sv = rawrxd::sovereign;

static std::vector<std::uint8_t> makeDemoPayload()
{
    // Benign x64 NOP sled + RET (same semantics as 01, stretched for padding tests).
    constexpr std::size_t kNopCount = 256;
    std::vector<std::uint8_t> out;
    out.resize(kNopCount + 1, 0x90);
    out.back() = 0xC3;
    return out;
}

int main(int argc, char** argv)
{
    std::vector<std::uint8_t> codeBytes;

    if (argc >= 2)
    {
        const auto r = rawrxd::sovereign::lab::readWholeFile(argv[1]);
        if (!r)
        {
            std::fprintf(stderr, "readWholeFile: %s\n", r.error.c_str());
            return 1;
        }
        codeBytes = std::move(r.data);
        if (codeBytes.empty())
        {
            std::fprintf(stderr, "payload file is empty\n");
            return 1;
        }
    }
    else
    {
        codeBytes = makeDemoPayload();
    }

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
    machM.os = sv::TargetOs::MacOS;
    machM.arch = sv::TargetArch::X86_64;
    machM.objectFormat = sv::ObjectFormat::MachO;
    const std::vector<std::uint8_t> mach = fmt::composeMacho64MinimalImageFromManifest(machM, codeBytes);

    if (elf.empty() || pe.empty() || mach.empty())
    {
        std::fprintf(stderr, "compose* returned empty image(s)\n");
        return 1;
    }

    std::fprintf(stderr,
                 "lab wrap OK: payload=%zu bytes -> ELF=%zu PE=%zu Mach-O=%zu\n",
                 codeBytes.size(),
                 elf.size(),
                 pe.size(),
                 mach.size());
    return 0;
}
