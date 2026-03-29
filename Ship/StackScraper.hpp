#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>

#pragma pack(push, 8)
struct RawrxdCpuContextLite {
    std::uint64_t rip{};
    std::uint64_t rsp{};
    std::uint64_t rbp{};
    std::uint64_t rax{};
    std::uint64_t rbx{};
    std::uint64_t rcx{};
    std::uint64_t rdx{};
    std::uint64_t rsi{};
    std::uint64_t rdi{};
    std::uint64_t r8{};
    std::uint64_t r9{};
    std::uint64_t r10{};
    std::uint64_t r11{};
    std::uint64_t r12{};
    std::uint64_t r13{};
    std::uint64_t r14{};
    std::uint64_t r15{};
    std::uint64_t stackPeek[8]{};
};
#pragma pack(pop)

static_assert(sizeof(RawrxdCpuContextLite) == 200, "RawrxdCpuContextLite must match StackScraper.asm");

extern "C" {

#if defined(RAWRXD_HAS_STACK_SCRAPER_ASM) && RAWRXD_HAS_STACK_SCRAPER_ASM
void CaptureCpuContextLite(RawrxdCpuContextLite* pOut);
// Returns number of return addresses written (0 .. maxFrames).
std::size_t WalkRbpReturnAddresses(std::uint64_t* outRips, std::uint32_t maxFrames, std::uint64_t rbpSeed);
#else
inline void CaptureCpuContextLite(RawrxdCpuContextLite* pOut)
{
    if (pOut) {
        *pOut = {};
    }
}
inline std::size_t WalkRbpReturnAddresses(std::uint64_t* /*outRips*/, std::uint32_t /*maxFrames*/,
                                          std::uint64_t /*rbpSeed*/)
{
    return 0;
}
#endif
}

namespace rawrxd {

// Fill struct from a Win32 CONTEXT (correct for suspended threads / exceptions).
void fillCpuContextLiteFromContext(RawrxdCpuContextLite* out, const CONTEXT& ctx);

// Best-effort 64-byte copy from RSP; returns false on fault (guard page, etc.).
bool tryCopyStackPeek(std::uint64_t rsp, std::uint64_t outQwords[8]);

// Current thread: RtlCaptureContext + stack peek. Prefer this over CaptureCpuContextLite for GPR truth.
void captureCpuContextLiteAccurate(RawrxdCpuContextLite* out);

} // namespace rawrxd
