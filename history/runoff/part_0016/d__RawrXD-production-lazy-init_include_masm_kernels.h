#pragma once

#include <cstddef>

extern "C" {
    // Compute dot product of two float arrays (len elements). Returns float sum.
    float dot_avx2(const float* a, const float* b, std::size_t len);
}

// C++ helpers
namespace masm {
    struct BackendInfo {
        bool compiled_with_avx2; // build-time
        bool cpu_avx2;          // cpu reports AVX2
        bool os_xsave_enabled;  // OS has enabled XMM/YMM state
        bool usable() const { return compiled_with_avx2 && cpu_avx2 && os_xsave_enabled; }
    };

    BackendInfo backend_info();
    // C = A(m x k) * B(k x n)  (row-major). C must be m*n floats.
    void matmul_masm(const float* A, const float* B, float* C, std::size_t m, std::size_t n, std::size_t k);
}
