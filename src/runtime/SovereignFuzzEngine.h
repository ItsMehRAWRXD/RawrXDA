#pragma once
// =============================================================================
// SovereignFuzzEngine.h — Phase 52: SEH-guarded mutation fuzzer
// =============================================================================
#include <cstdint>
#include <random>
#include <vector>

namespace RawrXD::Runtime {

struct FuzzReport {
    uint32_t iterations   = 0;
    uint32_t crashes      = 0;
    uint32_t allocFailures = 0;

    static constexpr uint32_t kMaxCrashDetails = 16;
    std::vector<uint32_t> crashSeeds; ///< RNG seeds that produced crashes
    std::vector<uint32_t> crashCodes; ///< GetExceptionCode() values
};

class SovereignFuzzEngine {
public:
    static SovereignFuzzEngine& instance();

    /// Run a fuzz cycle against the given kernel function.
    /// kernelAddr: pointer to the start of executable code under test.
    /// kernelSize: byte size of the code region to copy per iteration.
    /// seedInput:  initial input vector (mutated each iteration).
    /// iterations: number of probes (clamped to [1, 100 000]).
    FuzzReport startFuzzCycle(void* kernelAddr,
                              size_t kernelSize,
                              const std::vector<uint8_t>& seedInput,
                              uint32_t iterations = 1000);

    SovereignFuzzEngine();

private:
    std::mt19937 m_rng;
};

} // namespace RawrXD::Runtime
