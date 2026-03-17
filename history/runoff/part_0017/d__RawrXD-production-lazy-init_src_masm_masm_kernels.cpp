#include "masm_kernels.h"
#include <vector>
#include <immintrin.h>
#include <intrin.h>

extern "C" float dot_avx2(const float* a, const float* b, std::size_t len);

namespace {
    bool has_avx2()
    {
        int cpuInfo[4] = {0,0,0,0};
        __cpuid(cpuInfo, 0);
        int nIds = cpuInfo[0];
        if (nIds < 1) return false;

        int cpuInfo1[4];
        __cpuid(cpuInfo1, 1);
        bool osxsave = (cpuInfo1[2] & (1 << 27)) != 0;
        bool avx = (cpuInfo1[2] & (1 << 28)) != 0;

        // To use AVX/AVX2 instructions safely the OS must support XSAVE/XGETBV
        bool os_supports_avx = false;
        if (osxsave && avx) {
            unsigned long long xcr0 = _xgetbv(0);
            // XMM (bit 1) and YMM (bit 2) must be enabled in XCR0
            os_supports_avx = ((xcr0 & 0x6ULL) == 0x6ULL);
        }

        if (!os_supports_avx) return false;

        if (nIds >= 7) {
            int cpuInfo7[4];
            __cpuidex(cpuInfo7, 7, 0);
            // EBX bit 5 indicates AVX2
            return (cpuInfo7[1] & (1 << 5)) != 0;
        }
        return false;
    }
}

namespace masm {

BackendInfo backend_info()
{
    // Some toolchains (MSVC) don't define __AVX2__ even when /arch:AVX2
    // is enabled. Treat MSVC x64 builds as compiled-with-AVX2 if the
    // platform is x64 (we also enable /arch:AVX2 in CMake for MSVC).
#if defined(__AVX2__) || defined(_MSC_VER)
    constexpr bool compiled = true;
#else
    constexpr bool compiled = false;
#endif
    BackendInfo info{};
    info.compiled_with_avx2 = compiled;
    info.cpu_avx2 = false;
    info.os_xsave_enabled = false;

    // Only probe CPUID/XGETBV on x86/x64
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
    // Reuse has_avx2's probes but split out values
    int cpuInfo[4] = {0,0,0,0};
    __cpuid(cpuInfo, 0);
    int nIds = cpuInfo[0];
    if (nIds >= 1) {
        int cpuInfo1[4];
        __cpuid(cpuInfo1, 1);
        bool osxsave = (cpuInfo1[2] & (1 << 27)) != 0;
        bool avx = (cpuInfo1[2] & (1 << 28)) != 0;
        bool os_supports_avx = false;
        if (osxsave && avx) {
            unsigned long long xcr0 = _xgetbv(0);
            os_supports_avx = ((xcr0 & 0x6ULL) == 0x6ULL);
            info.os_xsave_enabled = os_supports_avx;
        }
        if (nIds >= 7) {
            int cpuInfo7[4];
            __cpuidex(cpuInfo7, 7, 0);
            info.cpu_avx2 = (cpuInfo7[1] & (1 << 5)) != 0;
        }
    }
#endif

    return info;
}

// Fallback AVX2 implementation using intrinsics when MASM assembly not available
extern "C" float dot_avx2(const float* a, const float* b, std::size_t len)
{
#if defined(__AVX2__) || defined(_MSC_VER)
    __m256 sum = _mm256_setzero_ps();
    std::size_t i = 0;
    for (; i + 7 < len; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        sum = _mm256_add_ps(sum, _mm256_mul_ps(va, vb));
    }
    float tmp[8];
    _mm256_storeu_ps(tmp, sum);
    float res = tmp[0] + tmp[1] + tmp[2] + tmp[3] + tmp[4] + tmp[5] + tmp[6] + tmp[7];
    for (; i < len; ++i) res += a[i] * b[i];
    return res;
#else
    // Portable scalar fallback
    float s = 0.0f;
    for (std::size_t i = 0; i < len; ++i) s += a[i] * b[i];
    return s;
#endif
}

void matmul_masm(const float* A, const float* B, float* C, std::size_t m, std::size_t n, std::size_t k)
{
    auto info = backend_info();
    if (!info.usable()) {
        // Fallback naive matmul
        for (std::size_t i = 0; i < m; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                float s = 0.0f;
                const float* arow = A + i * k;
                for (std::size_t t = 0; t < k; ++t) s += arow[t] * B[t * n + j];
                C[i * n + j] = s;
            }
        }
        return;
    }

    std::vector<float> col;
    col.resize(k);

    for (std::size_t j = 0; j < n; ++j) {
        // gather column j of B into contiguous buffer
        for (std::size_t t = 0; t < k; ++t) col[t] = B[t * n + j];

        for (std::size_t i = 0; i < m; ++i) {
            const float* arow = A + i * k;
            float v = dot_avx2(arow, col.data(), k);
            C[i * n + j] = v;
        }
    }
}

} // namespace masm
