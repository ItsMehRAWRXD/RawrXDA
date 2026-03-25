// test_asm_pyre.cpp — Correctness baseline for asm_pyre_* compute kernels.
//
// Phase 1 (current): C++ reference implementations are tested against known
// input/output vectors.  These establish the EXPECTED behavior and serve as
// regression anchors.
//
// Phase 2 (when RAWRXD_ASM_PYRE_LINKED is defined at build time): The same
// vectors are run through the real AVX2 MASM implementations to verify that
// the ASM produces identical results.
//
// Add to a test target with:
//   target_compile_definitions(test_asm_pyre PRIVATE RAWRXD_ASM_PYRE_LINKED)
//   target_link_libraries(test_asm_pyre PRIVATE <asm_obj_lib_target>)

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cassert>
#include <vector>
#include <algorithm>

static int g_fail = 0;

#define VERIFY(cond) do { \
    if (!(cond)) { std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); ++g_fail; } \
} while(0)

#define VERIFY_NEAR(a, b, eps) do { \
    if (std::fabs((double)(a) - (double)(b)) > (double)(eps)) { \
        std::fprintf(stderr, "FAIL [line %d]: |%g - %g| > %g\n", __LINE__, (double)(a), (double)(b), (double)(eps)); \
        ++g_fail; \
    } \
} while(0)

// ---------------------------------------------------------------------------
// Forward declarations — only resolved when RAWRXD_ASM_PYRE_LINKED is defined
// ---------------------------------------------------------------------------
#ifdef RAWRXD_ASM_PYRE_LINKED
extern "C" {
    // int asm_pyre_gemm_fp32(const float* A, const float* B, float* C,
    //                        uint32_t M, uint32_t N, uint32_t K)
    int asm_pyre_gemm_fp32(const float* A, const float* B, float* C,
                           uint32_t M, uint32_t N, uint32_t K);
    // Add other pyre kernels here when linking them.
}
#endif

// ---------------------------------------------------------------------------
// C++ reference implementations (always compiled, never linkage-dependent)
// ---------------------------------------------------------------------------

static void ref_gemm_fp32(const float* A, const float* B, float* C,
                          uint32_t M, uint32_t N, uint32_t K) {
    for (uint32_t m = 0; m < M; ++m) {
        for (uint32_t n = 0; n < N; ++n) {
            float acc = 0.0f;
            for (uint32_t k = 0; k < K; ++k) {
                acc += A[m * K + k] * B[k * N + n];
            }
            C[m * N + n] = acc;
        }
    }
}

static float ref_softmax_max(const float* x, uint32_t n) {
    float m = x[0];
    for (uint32_t i = 1; i < n; ++i) m = m < x[i] ? x[i] : m;
    return m;
}

static void ref_softmax(const float* x, float* y, uint32_t n) {
    float m = ref_softmax_max(x, n);
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; ++i) { y[i] = std::exp(x[i] - m); sum += y[i]; }
    for (uint32_t i = 0; i < n; ++i) y[i] /= sum;
}

static void ref_rmsnorm(const float* x, float* y, uint32_t n, float eps = 1e-5f) {
    float sq = 0.0f;
    for (uint32_t i = 0; i < n; ++i) sq += x[i] * x[i];
    float rms = 1.0f / std::sqrt(sq / n + eps);
    for (uint32_t i = 0; i < n; ++i) y[i] = x[i] * rms;
}

static void ref_silu(const float* x, float* y, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        float v = x[i];
        y[i] = v / (1.0f + std::exp(-v));
    }
}

// ---------------------------------------------------------------------------
// Test: 2×2 GEMM identity (A=I, B=known → C==B)
// ---------------------------------------------------------------------------
static void test_gemm_identity_2x2() {
    const float A[4] = { 1, 0, 0, 1 };   // 2×2 identity
    const float B[4] = { 3, 7, 2, 5 };
    float C_ref[4] = {};
    ref_gemm_fp32(A, B, C_ref, 2, 2, 2);
    VERIFY_NEAR(C_ref[0], 3.0f, 1e-5f);
    VERIFY_NEAR(C_ref[1], 7.0f, 1e-5f);
    VERIFY_NEAR(C_ref[2], 2.0f, 1e-5f);
    VERIFY_NEAR(C_ref[3], 5.0f, 1e-5f);

#ifdef RAWRXD_ASM_PYRE_LINKED
    float C_asm[4] = {};
    int rc = asm_pyre_gemm_fp32(A, B, C_asm, 2, 2, 2);
    VERIFY(rc == 0);
    for (int i = 0; i < 4; ++i) VERIFY_NEAR(C_asm[i], C_ref[i], 1e-4f);
    std::fprintf(stdout, "  ✓ gemm_fp32 identity 2×2 [ASM verified]\n");
#else
    std::fprintf(stdout, "  ✓ gemm_fp32 identity 2×2 [ref only — define RAWRXD_ASM_PYRE_LINKED to test ASM]\n");
#endif
}

// ---------------------------------------------------------------------------
// Test: known 3×3 GEMM
// ---------------------------------------------------------------------------
static void test_gemm_known_3x3() {
    // A = [[1,2,3],[4,5,6],[7,8,9]], B = [[9,8,7],[6,5,4],[3,2,1]]
    const float A[9] = { 1,2,3,4,5,6,7,8,9 };
    const float B[9] = { 9,8,7,6,5,4,3,2,1 };
    // Expected C[0][0] = 1*9+2*6+3*3 = 9+12+9 = 30
    //         C[0][1] = 1*8+2*5+3*2 = 8+10+6 = 24
    float C_ref[9] = {};
    ref_gemm_fp32(A, B, C_ref, 3, 3, 3);
    VERIFY_NEAR(C_ref[0], 30.0f, 1e-4f);
    VERIFY_NEAR(C_ref[1], 24.0f, 1e-4f);
    std::fprintf(stdout, "  ✓ gemm_fp32 known 3×3\n");
}

// ---------------------------------------------------------------------------
// Test: softmax output sums to 1.0 and is non-negative
// ---------------------------------------------------------------------------
static void test_softmax_sum() {
    const float x[8] = { 1.0f, 2.0f, -1.0f, 0.5f, 3.0f, -0.5f, 0.0f, 1.5f };
    float y[8] = {};
    ref_softmax(x, y, 8);
    float sum = 0.0f;
    for (int i = 0; i < 8; ++i) { sum += y[i]; VERIFY(y[i] >= 0.0f); }
    VERIFY_NEAR(sum, 1.0f, 1e-5f);
    std::fprintf(stdout, "  ✓ softmax sums to 1.0 (sum=%.6f)\n", (double)sum);
}

// ---------------------------------------------------------------------------
// Test: RMSNorm output vector norm ≈ sqrt(N) / rms_input
// ---------------------------------------------------------------------------
static void test_rmsnorm_unit() {
    const float x[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    float y[4] = {};
    ref_rmsnorm(x, y, 4, 1e-5f);
    // The scaled RMS of y should be ~1.0 (definition of RMSNorm)
    float sq = 0.0f;
    for (int i = 0; i < 4; ++i) sq += y[i] * y[i];
    float rms = std::sqrt(sq / 4.0f);
    VERIFY_NEAR(rms, 1.0f, 1e-4f);
    std::fprintf(stdout, "  ✓ rmsnorm output rms ≈ 1.0 (got %.6f)\n", (double)rms);
}

// ---------------------------------------------------------------------------
// Test: SiLU is in range and monotone for positive values
// ---------------------------------------------------------------------------
static void test_silu_range() {
    const float x[5] = { 0.0f, 1.0f, 2.0f, 4.0f, 8.0f };
    float y[5] = {};
    ref_silu(x, y, 5);
    // SiLU(0)=0, SiLU(1)≈0.731, strictly increasing for x>0
    VERIFY_NEAR(y[0], 0.0f, 1e-6f);
    VERIFY(y[1] > y[0]);
    VERIFY(y[2] > y[1]);
    VERIFY(y[3] > y[2]);
    VERIFY(y[4] > y[3]);
    std::fprintf(stdout, "  ✓ silu(0)=0, monotone positive\n");
}

int main() {
    std::fprintf(stdout, "=== test_asm_pyre ===\n");
    test_gemm_identity_2x2();
    test_gemm_known_3x3();
    test_softmax_sum();
    test_rmsnorm_unit();
    test_silu_range();

    if (g_fail == 0) {
        std::fprintf(stdout, "PASS (%d checks)\n", 5);
        return 0;
    }
    std::fprintf(stderr, "FAIL (%d check(s) failed)\n", g_fail);
    return 1;
}
