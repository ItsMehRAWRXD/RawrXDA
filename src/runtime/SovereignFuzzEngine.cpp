// SovereignFuzzEngine.cpp — Phase 52: SEH-guarded mutation fuzzer
#include "SovereignFuzzEngine.h"
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <random>
#include <vector>

namespace RawrXD::Runtime {

// ---------------------------------------------------------------------------
// Mutation strategies
// ---------------------------------------------------------------------------
enum class MutationType : uint8_t {
    BitFlip       = 0,
    ByteSwap      = 1,
    BoundaryZero  = 2,
    BoundaryFF    = 3,
    BoundaryHalf  = 4,
};

static void applyMutation(std::vector<uint8_t>& buf, uint32_t seed) {
    if (buf.empty()) return;
    std::mt19937 rng(seed);
    auto type = static_cast<MutationType>(rng() % 5);
    size_t idx = rng() % buf.size();
    switch (type) {
    case MutationType::BitFlip:
        buf[idx] ^= static_cast<uint8_t>(1u << (rng() % 8));
        break;
    case MutationType::ByteSwap:
        if (buf.size() >= 2) std::swap(buf[idx], buf[rng() % buf.size()]);
        break;
    case MutationType::BoundaryZero:
        buf[idx] = 0x00;
        break;
    case MutationType::BoundaryFF:
        buf[idx] = 0xFF;
        break;
    case MutationType::BoundaryHalf:
        buf[idx] = 0x80;
        break;
    }
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// SEH probe — no C++ objects in scope (required to avoid MSVC C2712)
// ---------------------------------------------------------------------------
static uint32_t sehProbe(void* scratch, const uint8_t* data, size_t sz) {
    using KernelFn = uint32_t(*)(const uint8_t*, size_t);
    uint32_t code = 0;
    __try {
        reinterpret_cast<KernelFn>(scratch)(data, sz);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        code = GetExceptionCode();
    }
    return code;
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
static std::mutex g_fuzzMutex;

SovereignFuzzEngine& SovereignFuzzEngine::instance() {
    static SovereignFuzzEngine inst;
    return inst;
}

SovereignFuzzEngine::SovereignFuzzEngine()
    : m_rng(std::random_device{}()) {}

// ---------------------------------------------------------------------------
// Core fuzz loop
// ---------------------------------------------------------------------------
FuzzReport SovereignFuzzEngine::startFuzzCycle(void*  kernelAddr,
                                               size_t kernelSize,
                                               const std::vector<uint8_t>& seedInput,
                                               uint32_t iterations) {
    std::lock_guard<std::mutex> lk(g_fuzzMutex);

    FuzzReport report{};
    report.iterations = iterations;

    if (!kernelAddr || kernelSize == 0 || kernelSize > 4 * 1024 * 1024) return report;
    if (iterations == 0 || iterations > 100000) return report;

    using KernelFn = uint32_t(*)(const uint8_t*, size_t);

    for (uint32_t i = 0; i < iterations; ++i) {
        // 1. Allocate scratch execution page
        void* scratch = VirtualAlloc(nullptr, kernelSize,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!scratch) { ++report.allocFailures; continue; }

        // 2. Copy kernel into scratch
        std::memcpy(scratch, kernelAddr, kernelSize);
        FlushInstructionCache(GetCurrentProcess(), scratch, kernelSize);

        // 3. Mutate input
        std::vector<uint8_t> mutant = seedInput;
        if (mutant.empty()) mutant.push_back(0x00);
        uint32_t seed = m_rng();
        applyMutation(mutant, seed);

        // 4. Call under SEH guard
        uint32_t exceptionCode = sehProbe(scratch, mutant.data(), mutant.size());

        // 5. Record crash
        if (exceptionCode != 0) {
            ++report.crashes;
            if (report.crashes <= FuzzReport::kMaxCrashDetails) {
                report.crashSeeds.push_back(seed);
                report.crashCodes.push_back(exceptionCode);
            }
        }

        // 6. Release scratch
        VirtualFree(scratch, 0, MEM_RELEASE);
    }
    return report;
}

} // namespace RawrXD::Runtime
