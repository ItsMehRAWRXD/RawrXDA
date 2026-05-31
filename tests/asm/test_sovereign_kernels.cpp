// ============================================================================
// tests/asm/test_sovereign_kernels.cpp
// GoogleTest v1.14.0 — Sovereign ASM Kernel Validation Suite
// Tests: Dequantization, RMSNorm, MatVec, CopyBuffer, SpeculativeEngine
// ============================================================================
#include <gtest/gtest.h>
#include <immintrin.h>
#include <vector>
#include <random>
#include <cmath>
#include <cstring>
#include <chrono>
#include <intrin.h>

// ── ASM ABI Headers ─────────────────────────────────────────────────────────
#include "../../src/asm/sovereign_kernels.hpp"

// Note: SovereignSpeculativeEngine.asm requires LlamaNativeBridge C++ symbols
// which are not available in the standalone test target. Tests for that engine
// are performed via integration tests against the full Win32IDE build.

// ── Test Configuration ──────────────────────────────────────────────────────
constexpr float EPSILON_F32 = 1e-4f;
constexpr float EPSILON_Q4  = 0.05f;   // Q4_0 quantization error tolerance
constexpr int   SEED        = 42;

// ── Helper: Aligned allocator for AVX2 ──────────────────────────────────────
template<typename T, size_t Alignment = 32>
class AlignedAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    
    template<typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };
    
    AlignedAllocator() noexcept = default;
    template<typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}
    
    T* allocate(size_t n) {
        void* ptr = _aligned_malloc(n * sizeof(T), Alignment);
        if (!ptr) throw std::bad_alloc();
        return static_cast<T*>(ptr);
    }
    void deallocate(T* p, size_t) noexcept {
        _aligned_free(p);
    }
    
    bool operator==(const AlignedAllocator&) const noexcept { return true; }
    bool operator!=(const AlignedAllocator&) const noexcept { return false; }
};

template<typename T> using AlignedVec = std::vector<T, AlignedAllocator<T>>;

// ── Helper: Generate random f32 data ────────────────────────────────────────
static std::vector<float> generateRandomF32(size_t n, float min = -1.0f, float max = 1.0f) {
    static std::mt19937 gen(SEED);
    std::uniform_real_distribution<float> dis(min, max);
    std::vector<float> data(n);
    for (auto& v : data) v = dis(gen);
    return data;
}

// ── Helper: Reference RMSNorm implementation ────────────────────────────────
static void referenceRMSNorm(float* x, int64_t n, const float* weight, float eps) {
    double sum_sq = 0.0;
    for (int64_t i = 0; i < n; ++i) sum_sq += static_cast<double>(x[i]) * x[i];
    float rms = static_cast<float>(std::sqrt(sum_sq / n + eps));
    float scale = 1.0f / rms;
    for (int64_t i = 0; i < n; ++i) x[i] = x[i] * scale * weight[i];
}

// ── Helper: Reference MatVec implementation ─────────────────────────────────
static void referenceMatVec(const float* A, const float* x, float* y, int64_t n_rows, int64_t n_cols) {
    for (int64_t i = 0; i < n_rows; ++i) {
        double sum = 0.0;
        for (int64_t j = 0; j < n_cols; ++j) {
            sum += static_cast<double>(A[i * n_cols + j]) * static_cast<double>(x[j]);
        }
        y[i] = static_cast<float>(sum);
    }
}

// ── Helper: Reference Q4_0 dequantization ───────────────────────────────────
struct Q4_0_Block {
    uint16_t scale;    // f16 scale
    uint8_t  qs[16];   // 32 4-bit weights packed
};

static void referenceDequantQ4_0(const void* src, float* dst, int64_t n) {
    const Q4_0_Block* blocks = static_cast<const Q4_0_Block*>(src);
    int64_t n_blocks = n / 32;
    for (int64_t b = 0; b < n_blocks; ++b) {
        float scale = _mm_cvtss_f32(_mm_cvtph_ps(_mm_cvtsi32_si128(blocks[b].scale)));
        for (int i = 0; i < 32; ++i) {
            uint8_t q = (i < 16) ? (blocks[b].qs[i] & 0x0F) : (blocks[b].qs[i - 16] >> 4);
            dst[b * 32 + i] = (static_cast<float>(q) - 8.0f) * scale;
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// TEST SUITE: Sovereign_DequantizeRow_Q4_0_AVX2
// ═══════════════════════════════════════════════════════════════════════════════
class DequantQ4_0_Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure AVX2 is available
        if (!checkAVX2()) {
            GTEST_SKIP() << "AVX2 not available on this CPU";
        }
    }
    
    static bool checkAVX2() {
        int cpuInfo[4];
        __cpuid(cpuInfo, 1);
        bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
        bool avx     = (cpuInfo[2] & (1 << 28)) != 0;
        if (!osxsave || !avx) return false;
        uint64_t xcr0 = _xgetbv(0);
        if ((xcr0 & 0x6) != 0x6) return false;
        __cpuidex(cpuInfo, 7, 0);
        return (cpuInfo[1] & (1 << 5)) != 0;
    }
};

TEST_F(DequantQ4_0_Test, Basic_32Elements) {
    constexpr int64_t N = 32;
    Q4_0_Block block;
    block.scale = 0x3C00;  // f16 = 1.0
    for (int i = 0; i < 16; ++i) block.qs[i] = 0x88;  // All weights = 8 → value 0
    
    AlignedVec<float> dst(N);
    Sovereign_DequantizeRow_Q4_0_AVX2(&block, dst.data(), N);
    
    for (int i = 0; i < N; ++i) {
        EXPECT_NEAR(dst[i], 0.0f, EPSILON_Q4) << "Mismatch at index " << i;
    }
}

TEST_F(DequantQ4_0_Test, Basic_256Elements) {
    constexpr int64_t N = 256;
    constexpr int64_t n_blocks = N / 32;
    std::vector<Q4_0_Block> blocks(n_blocks);
    
    // Fill with deterministic pattern
    for (int b = 0; b < n_blocks; ++b) {
        blocks[b].scale = 0x3800;  // f16 = 0.5
        for (int i = 0; i < 16; ++i) {
            blocks[b].qs[i] = static_cast<uint8_t>((i * 17 + b * 3) & 0xFF);
        }
    }
    
    AlignedVec<float> dst_asm(N);
    AlignedVec<float> dst_ref(N);
    
    Sovereign_DequantizeRow_Q4_0_AVX2(blocks.data(), dst_asm.data(), N);
    referenceDequantQ4_0(blocks.data(), dst_ref.data(), N);
    
    for (int i = 0; i < N; ++i) {
        EXPECT_NEAR(dst_asm[i], dst_ref[i], EPSILON_Q4) << "Mismatch at index " << i;
    }
}

TEST_F(DequantQ4_0_Test, Large_4096Elements) {
    constexpr int64_t N = 4096;
    constexpr int64_t n_blocks = N / 32;
    std::vector<Q4_0_Block> blocks(n_blocks);
    
    std::mt19937 gen(SEED);
    std::uniform_int_distribution<int> scale_dis(0x3400, 0x3C00);  // f16 range
    std::uniform_int_distribution<int> qs_dis(0, 255);
    
    for (auto& block : blocks) {
        block.scale = static_cast<uint16_t>(scale_dis(gen));
        for (int i = 0; i < 16; ++i) block.qs[i] = static_cast<uint8_t>(qs_dis(gen));
    }
    
    AlignedVec<float> dst_asm(N);
    AlignedVec<float> dst_ref(N);
    
    Sovereign_DequantizeRow_Q4_0_AVX2(blocks.data(), dst_asm.data(), N);
    referenceDequantQ4_0(blocks.data(), dst_ref.data(), N);
    
    double max_err = 0.0;
    for (int i = 0; i < N; ++i) {
        double err = std::abs(dst_asm[i] - dst_ref[i]);
        if (err > max_err) max_err = err;
    }
    EXPECT_LT(max_err, EPSILON_Q4) << "Max error: " << max_err;
}

// ═════════════════════════════════════════════════════════════════════════════
// TEST SUITE: Sovereign_RMSNorm_F32_AVX2
// ═══════════════════════════════════════════════════════════════════════════════
class RMSNorm_Test : public ::testing::Test {
protected:
    static bool checkAVX2() {
        int cpuInfo[4];
        __cpuid(cpuInfo, 1);
        bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
        bool avx     = (cpuInfo[2] & (1 << 28)) != 0;
        if (!osxsave || !avx) return false;
        uint64_t xcr0 = _xgetbv(0);
        if ((xcr0 & 0x6) != 0x6) return false;
        __cpuidex(cpuInfo, 7, 0);
        return (cpuInfo[1] & (1 << 5)) != 0;
    }
    
    void SetUp() override {
        if (!checkAVX2()) GTEST_SKIP() << "AVX2 not available";
    }
};

TEST_F(RMSNorm_Test, Basic_64Elements) {
    constexpr int64_t N = 64;
    auto x_asm = generateRandomF32(N);
    auto x_ref = x_asm;
    auto weight = generateRandomF32(N, 0.5f, 1.5f);
    
    Sovereign_RMSNorm_F32_AVX2(x_asm.data(), N, weight.data(), 1e-6f);
    referenceRMSNorm(x_ref.data(), N, weight.data(), 1e-6f);
    
    for (int i = 0; i < N; ++i) {
        EXPECT_NEAR(x_asm[i], x_ref[i], EPSILON_F32) << "Mismatch at index " << i;
    }
}

TEST_F(RMSNorm_Test, Large_8192Elements) {
    constexpr int64_t N = 8192;
    auto x_asm = generateRandomF32(N);
    auto x_ref = x_asm;
    auto weight = generateRandomF32(N, 0.5f, 1.5f);
    
    Sovereign_RMSNorm_F32_AVX2(x_asm.data(), N, weight.data(), 1e-6f);
    referenceRMSNorm(x_ref.data(), N, weight.data(), 1e-6f);
    
    double max_err = 0.0;
    for (int i = 0; i < N; ++i) {
        double err = std::abs(x_asm[i] - x_ref[i]);
        if (err > max_err) max_err = err;
    }
    EXPECT_LT(max_err, EPSILON_F32) << "Max error: " << max_err;
}

TEST_F(RMSNorm_Test, UnityWeight_Identity) {
    constexpr int64_t N = 128;
    std::vector<float> x(N, 2.0f);
    std::vector<float> weight(N, 1.0f);
    
    Sovereign_RMSNorm_F32_AVX2(x.data(), N, weight.data(), 1e-6f);
    
    // RMS of all 2.0s = sqrt(4.0) = 2.0, scale = 1/2.0 = 0.5
    // Result = 2.0 * 0.5 * 1.0 = 1.0
    for (int i = 0; i < N; ++i) {
        EXPECT_NEAR(x[i], 1.0f, EPSILON_F32) << "Mismatch at index " << i;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// TEST SUITE: Sovereign_MatVec_F32_AVX2
// ═══════════════════════════════════════════════════════════════════════════════
class MatVec_Test : public ::testing::Test {
protected:
    static bool checkAVX2() {
        int cpuInfo[4];
        __cpuid(cpuInfo, 1);
        bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
        bool avx     = (cpuInfo[2] & (1 << 28)) != 0;
        if (!osxsave || !avx) return false;
        uint64_t xcr0 = _xgetbv(0);
        if ((xcr0 & 0x6) != 0x6) return false;
        __cpuidex(cpuInfo, 7, 0);
        return (cpuInfo[1] & (1 << 5)) != 0;
    }
    
    void SetUp() override {
        if (!checkAVX2()) GTEST_SKIP() << "AVX2 not available";
    }
};

TEST_F(MatVec_Test, Small_8x8) {
    constexpr int64_t N_ROWS = 8;
    constexpr int64_t N_COLS = 8;
    auto A = generateRandomF32(N_ROWS * N_COLS);
    auto x = generateRandomF32(N_COLS);
    AlignedVec<float> y_asm(N_ROWS);
    AlignedVec<float> y_ref(N_ROWS);
    
    Sovereign_MatVec_F32_AVX2(A.data(), x.data(), y_asm.data(), N_ROWS, N_COLS);
    referenceMatVec(A.data(), x.data(), y_ref.data(), N_ROWS, N_COLS);
    
    for (int i = 0; i < N_ROWS; ++i) {
        EXPECT_NEAR(y_asm[i], y_ref[i], EPSILON_F32) << "Mismatch at row " << i;
    }
}

TEST_F(MatVec_Test, Medium_256x4096) {
    constexpr int64_t N_ROWS = 256;
    constexpr int64_t N_COLS = 4096;
    auto A = generateRandomF32(N_ROWS * N_COLS);
    auto x = generateRandomF32(N_COLS);
    AlignedVec<float> y_asm(N_ROWS);
    AlignedVec<float> y_ref(N_ROWS);
    
    Sovereign_MatVec_F32_AVX2(A.data(), x.data(), y_asm.data(), N_ROWS, N_COLS);
    referenceMatVec(A.data(), x.data(), y_ref.data(), N_ROWS, N_COLS);
    
    double max_err = 0.0;
    for (int i = 0; i < N_ROWS; ++i) {
        double err = std::abs(y_asm[i] - y_ref[i]);
        if (err > max_err) max_err = err;
    }
    EXPECT_LT(max_err, EPSILON_F32) << "Max error: " << max_err;
}

TEST_F(MatVec_Test, Large_4096x4096) {
    constexpr int64_t N_ROWS = 4096;
    constexpr int64_t N_COLS = 4096;
    auto A = generateRandomF32(N_ROWS * N_COLS);
    auto x = generateRandomF32(N_COLS);
    AlignedVec<float> y_asm(N_ROWS);
    AlignedVec<float> y_ref(N_ROWS);
    
    Sovereign_MatVec_F32_AVX2(A.data(), x.data(), y_asm.data(), N_ROWS, N_COLS);
    referenceMatVec(A.data(), x.data(), y_ref.data(), N_ROWS, N_COLS);
    
    double max_err = 0.0;
    for (int i = 0; i < N_ROWS; ++i) {
        double err = std::abs(y_asm[i] - y_ref[i]);
        if (err > max_err) max_err = err;
    }
    EXPECT_LT(max_err, EPSILON_F32) << "Max error: " << max_err;
}

// ═════════════════════════════════════════════════════════════════════════════
// TEST SUITE: Sovereign_CopyBuffer_NT_AVX2
// ═══════════════════════════════════════════════════════════════════════════════
class CopyBuffer_Test : public ::testing::Test {
protected:
    static bool checkAVX2() {
        int cpuInfo[4];
        __cpuid(cpuInfo, 1);
        bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
        bool avx     = (cpuInfo[2] & (1 << 28)) != 0;
        if (!osxsave || !avx) return false;
        uint64_t xcr0 = _xgetbv(0);
        if ((xcr0 & 0x6) != 0x6) return false;
        __cpuidex(cpuInfo, 7, 0);
        return (cpuInfo[1] & (1 << 5)) != 0;
    }
    
    void SetUp() override {
        if (!checkAVX2()) GTEST_SKIP() << "AVX2 not available";
    }
};

TEST_F(CopyBuffer_Test, Basic_64Bytes) {
    constexpr int64_t N = 64;
    AlignedVec<uint8_t> src(N);
    AlignedVec<uint8_t> dst(N);
    for (int i = 0; i < N; ++i) src[i] = static_cast<uint8_t>(i);
    std::memset(dst.data(), 0, N);
    
    Sovereign_CopyBuffer_NT_AVX2(dst.data(), src.data(), N);
    
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(dst[i], src[i]) << "Mismatch at byte " << i;
    }
}

TEST_F(CopyBuffer_Test, Large_1MB) {
    constexpr int64_t N = 1024 * 1024;
    AlignedVec<uint8_t> src(N);
    AlignedVec<uint8_t> dst(N);
    std::mt19937 gen(SEED);
    std::uniform_int_distribution<int> dis(0, 255);
    for (int i = 0; i < N; ++i) src[i] = static_cast<uint8_t>(dis(gen));
    std::memset(dst.data(), 0, N);
    
    Sovereign_CopyBuffer_NT_AVX2(dst.data(), src.data(), N);
    
    EXPECT_EQ(std::memcmp(dst.data(), src.data(), N), 0) << "1MB copy mismatch";
}

// ═════════════════════════════════════════════════════════════════════════════
// PERFORMANCE BENCHMARKS (using GoogleTest)
// ═══════════════════════════════════════════════════════════════════════════════
class PerformanceBenchmark : public ::testing::Test {
protected:
    static bool checkAVX2() {
        int cpuInfo[4];
        __cpuid(cpuInfo, 1);
        bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
        bool avx     = (cpuInfo[2] & (1 << 28)) != 0;
        if (!osxsave || !avx) return false;
        uint64_t xcr0 = _xgetbv(0);
        if ((xcr0 & 0x6) != 0x6) return false;
        __cpuidex(cpuInfo, 7, 0);
        return (cpuInfo[1] & (1 << 5)) != 0;
    }
    
    void SetUp() override {
        if (!checkAVX2()) GTEST_SKIP() << "AVX2 not available";
    }
};

TEST_F(PerformanceBenchmark, MatVec_4096x4096_Throughput) {
    constexpr int64_t N_ROWS = 4096;
    constexpr int64_t N_COLS = 4096;
    constexpr int ITERATIONS = 100;
    
    auto A = generateRandomF32(N_ROWS * N_COLS);
    auto x = generateRandomF32(N_COLS);
    AlignedVec<float> y(N_ROWS);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        Sovereign_MatVec_F32_AVX2(A.data(), x.data(), y.data(), N_ROWS, N_COLS);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    double gflops = (2.0 * N_ROWS * N_COLS * ITERATIONS) / (ms * 1e6);
    
    std::cout << "MatVec 4096x4096: " << ms / ITERATIONS << " ms/op, "
              << gflops << " GFLOP/s" << std::endl;
    
    RecordProperty("latency_ms", ms / ITERATIONS);
    RecordProperty("throughput_gflops", gflops);
    
    EXPECT_GT(gflops, 10.0) << "Throughput below 10 GFLOP/s";
}

TEST_F(PerformanceBenchmark, RMSNorm_8192_Throughput) {
    constexpr int64_t N = 8192;
    constexpr int ITERATIONS = 10000;
    
    auto x = generateRandomF32(N);
    auto weight = generateRandomF32(N, 0.5f, 1.5f);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        Sovereign_RMSNorm_F32_AVX2(x.data(), N, weight.data(), 1e-6f);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    double ns_per_element = (ms * 1e6) / (N * ITERATIONS);
    
    std::cout << "RMSNorm 8192: " << ms / ITERATIONS << " ms/op, "
              << ns_per_element << " ns/element" << std::endl;
    
    RecordProperty("latency_ms", ms / ITERATIONS);
    RecordProperty("ns_per_element", ns_per_element);
}

TEST_F(PerformanceBenchmark, DequantQ4_4096_Throughput) {
    constexpr int64_t N = 4096;
    constexpr int ITERATIONS = 1000;
    constexpr int64_t n_blocks = N / 32;
    
    std::vector<Q4_0_Block> blocks(n_blocks);
    std::mt19937 gen(SEED);
    std::uniform_int_distribution<int> scale_dis(0x3400, 0x3C00);
    std::uniform_int_distribution<int> qs_dis(0, 255);
    for (auto& block : blocks) {
        block.scale = static_cast<uint16_t>(scale_dis(gen));
        for (int i = 0; i < 16; ++i) block.qs[i] = static_cast<uint8_t>(qs_dis(gen));
    }
    
    AlignedVec<float> dst(N);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        Sovereign_DequantizeRow_Q4_0_AVX2(blocks.data(), dst.data(), N);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    double gb_per_sec = (N * sizeof(float) * ITERATIONS) / (ms * 1e6);
    
    std::cout << "Dequant Q4_0 4096: " << ms / ITERATIONS << " ms/op, "
              << gb_per_sec << " GB/s" << std::endl;
    
    RecordProperty("latency_ms", ms / ITERATIONS);
    RecordProperty("bandwidth_gbps", gb_per_sec);
}

// ═════════════════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════════════════
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
