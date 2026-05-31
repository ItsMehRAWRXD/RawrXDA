#include <atomic>
#include <cstdint>
#include <intrin.h>

namespace {
std::atomic<uint32_t> g_masmAgentFailureChecks{0};
std::atomic<uint32_t> g_masmAgentFailureCount{0};

// CPU feature flags from CPUID
struct CpuFeatures {
    bool has_sse    = false;
    bool has_sse2   = false;
    bool has_sse3   = false;
    bool has_ssse3  = false;
    bool has_sse41  = false;
    bool has_sse42  = false;
    bool has_avx    = false;
    bool has_avx2   = false;
    bool has_fma    = false;
    bool has_avx512f = false;
    bool has_avx512vl = false;
    bool has_avx512bw = false;
    bool has_avx512dq = false;
    bool initialized = false;
};

CpuFeatures g_cpuFeatures;

void detect_cpu_features() {
    if (g_cpuFeatures.initialized) return;

    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);

    // ECX bits
    g_cpuFeatures.has_sse3   = (cpuInfo[2] & (1 << 0)) != 0;
    g_cpuFeatures.has_ssse3  = (cpuInfo[2] & (1 << 9)) != 0;
    g_cpuFeatures.has_sse41  = (cpuInfo[2] & (1 << 19)) != 0;
    g_cpuFeatures.has_sse42  = (cpuInfo[2] & (1 << 20)) != 0;
    g_cpuFeatures.has_avx    = (cpuInfo[2] & (1 << 28)) != 0;
    g_cpuFeatures.has_fma    = (cpuInfo[2] & (1 << 12)) != 0;

    // EDX bits
    g_cpuFeatures.has_sse    = (cpuInfo[3] & (1 << 25)) != 0;
    g_cpuFeatures.has_sse2   = (cpuInfo[3] & (1 << 26)) != 0;

    // Check AVX2 and AVX-512 via extended CPUID
    if (g_cpuFeatures.has_avx) {
        __cpuid(cpuInfo, 7);
        g_cpuFeatures.has_avx2    = (cpuInfo[1] & (1 << 5)) != 0;
        g_cpuFeatures.has_avx512f  = (cpuInfo[1] & (1 << 16)) != 0;
        g_cpuFeatures.has_avx512dq = (cpuInfo[1] & (1 << 17)) != 0;
        g_cpuFeatures.has_avx512bw = (cpuInfo[1] & (1 << 30)) != 0;
        g_cpuFeatures.has_avx512vl = (cpuInfo[1] & (1 << 31)) != 0;
    }

    g_cpuFeatures.initialized = true;
}

// Verify OS supports AVX by attempting a test instruction
bool verify_os_avx_support() {
    if (!g_cpuFeatures.has_avx) return false;
    __try {
        __m256 test = _mm256_set1_ps(1.0f);
        (void)test;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Verify OS supports AVX-512 by attempting a test instruction
bool verify_os_avx512_support() {
    if (!g_cpuFeatures.has_avx512f) return false;
    __try {
        __m512 test = _mm512_set1_ps(1.0f);
        (void)test;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
} // anonymous namespace

extern "C" void masm_agent_failure_detect_simd(void) {
    g_masmAgentFailureChecks.fetch_add(1, std::memory_order_relaxed);

    detect_cpu_features();

    uint32_t failures = 0;

    // Check for critical SIMD support gaps
    if (!g_cpuFeatures.has_sse)    failures |= (1u << 0);
    if (!g_cpuFeatures.has_sse2)   failures |= (1u << 1);
    if (!g_cpuFeatures.has_avx && verify_os_avx_support()) {
        // AVX reported by CPUID but OS doesn't support it
        failures |= (1u << 2);
    }
    if (g_cpuFeatures.has_avx && !verify_os_avx_support()) {
        // CPU supports AVX but OS disabled it
        failures |= (1u << 3);
    }
    if (g_cpuFeatures.has_avx512f && !verify_os_avx512_support()) {
        // CPU supports AVX-512 but OS disabled it
        failures |= (1u << 4);
    }

    if (failures) {
        g_masmAgentFailureCount.fetch_add(1, std::memory_order_relaxed);
    }
}

extern "C" uint32_t masm_agent_get_failure_count(void) {
    return g_masmAgentFailureCount.load(std::memory_order_relaxed);
}

extern "C" uint32_t masm_agent_get_check_count(void) {
    return g_masmAgentFailureChecks.load(std::memory_order_relaxed);
}

extern "C" uint32_t masm_agent_get_cpu_feature_mask(void) {
    detect_cpu_features();
    uint32_t mask = 0;
    if (g_cpuFeatures.has_sse)    mask |= (1u << 0);
    if (g_cpuFeatures.has_sse2)   mask |= (1u << 1);
    if (g_cpuFeatures.has_sse3)   mask |= (1u << 2);
    if (g_cpuFeatures.has_ssse3)  mask |= (1u << 3);
    if (g_cpuFeatures.has_sse41)  mask |= (1u << 4);
    if (g_cpuFeatures.has_sse42)  mask |= (1u << 5);
    if (g_cpuFeatures.has_avx)    mask |= (1u << 6);
    if (g_cpuFeatures.has_avx2)   mask |= (1u << 7);
    if (g_cpuFeatures.has_fma)    mask |= (1u << 8);
    if (g_cpuFeatures.has_avx512f) mask |= (1u << 9);
    return mask;
}
