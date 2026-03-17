#pragma once

// Phase 28 - EMPIRICAL BENCHMARK HARNESS
// Validating the 70B Ternary / Medusa-Cascade architecture.

#include "../hyper_150tps.hpp"

struct BenchmarkMetrics {
    double Prefill_TPS;
    double Sustained_Decode_TPS;
    double VRAM_Peak_MB;
    double KV_Cache_Growth_MB_per_100_Tokens;
    double Draft_Acceptance_Rate;
    double Verified_Tokens_Per_Sweep;
    double CPU_Deduction_Time_ms;
};

class Phase28Harness {
public:
    Phase28Harness();
    ~Phase28Harness();

    // Execute a strict, timed benchmark over 1,000 generated tokens
    BenchmarkMetrics RunValidationTrace(HyperVelocityEngine* activeEngine, UINT32 targetTokens);

private:
    UINT64 m_RDTSC_Start;
    UINT64 m_RDTSC_End;
};
