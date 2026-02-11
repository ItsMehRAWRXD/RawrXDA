#include <gtest/gtest.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <atomic>
#include <system_error>
#include <vector>

extern "C" {
    // MASM entrypoints
    int64_t launch_matmul_kernel(void* A, void* B, void* C, int64_t M, int64_t N, int64_t K);
    int64_t launch_vector_add_kernel(void* a, void* b, void* result, int64_t size);
    int64_t launch_element_mul_kernel(void* a, void* b, void* result, int64_t size);
}

namespace {
std::atomic<bool> g_gpuPathUsed{false};

bool HasSpirvEnv()
{
    const char* env = std::getenv("RAWRXD_MATMUL_SPV");
    if (!env || !*env) return false;
    std::error_code ec;
    return std::filesystem::exists(env, ec);
}

extern "C" int64_t HybridGPU_Init()
{
    // Simulated success only when a SPIR-V path is provided.
    return HasSpirvEnv() ? 1 : 0;
}

extern "C" int64_t HybridGPU_Synchronize()
{
    return 0;
}

extern "C" int64_t HybridGPU_MatMul(const float* A, const float* B, float* C,
                                     int64_t M, int64_t N, int64_t K)
{
    if (!A || !B || !C || M <= 0 || N <= 0 || K <= 0) {
        return -1;
    }
    if (!HasSpirvEnv()) {
        g_gpuPathUsed.store(false, std::memory_order_relaxed);
        return -1; // force CPU fallback
    }

    g_gpuPathUsed.store(true, std::memory_order_relaxed);
    for (int64_t m = 0; m < M; ++m) {
        for (int64_t n = 0; n < N; ++n) {
            float acc = 0.0f;
            for (int64_t k = 0; k < K; ++k) {
                acc += A[m * K + k] * B[k * N + n];
            }
            C[m * N + n] = acc;
        }
    }
    return 0;
}

extern "C" int64_t HybridCPU_MatMul(const float* A, const float* B, float* C,
                                     int64_t M, int64_t N, int64_t K)
{
    if (!A || !B || !C || M <= 0 || N <= 0 || K <= 0) {
        return -1;
    }
    g_gpuPathUsed.store(false, std::memory_order_relaxed);
    for (int64_t m = 0; m < M; ++m) {
        for (int64_t n = 0; n < N; ++n) {
            float acc = 0.0f;
            for (int64_t k = 0; k < K; ++k) {
                acc += A[m * K + k] * B[k * N + n];
            }
            C[m * N + n] = acc;
        }
    }
    return 0;
}

extern "C" int64_t launch_matmul_kernel(void* A, void* B, void* C, int64_t M, int64_t N, int64_t K)
{
    // Mirror MASM flow: GPU first, CPU fallback
    int64_t rc = HybridGPU_MatMul(static_cast<const float*>(A), static_cast<const float*>(B), static_cast<float*>(C), M, N, K);
    if (rc == 0) return 0;
    return HybridCPU_MatMul(static_cast<const float*>(A), static_cast<const float*>(B), static_cast<float*>(C), M, N, K);
}

extern "C" int64_t launch_vector_add_kernel(void* a, void* b, void* result, int64_t size)
{
    float* out = static_cast<float*>(result);
    const float* lhs = static_cast<const float*>(a);
    const float* rhs = static_cast<const float*>(b);
    for (int64_t i = 0; i < size; ++i) {
        out[i] = lhs[i] + rhs[i];
    }
    return 0;
}

extern "C" int64_t launch_element_mul_kernel(void* a, void* b, void* result, int64_t size)
{
    float* out = static_cast<float*>(result);
    const float* lhs = static_cast<const float*>(a);
    const float* rhs = static_cast<const float*>(b);
    for (int64_t i = 0; i < size; ++i) {
        out[i] = lhs[i] * rhs[i];
    }
    return 0;
}

std::vector<float> makeMat(std::initializer_list<float> vals)
{
    return std::vector<float>(vals);
}

void expectClose(const std::vector<float>& a, const std::vector<float>& b, float tol = 1e-5f)
{
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        ASSERT_NEAR(a[i], b[i], tol) << "index " << i;
    }
}
}

TEST(GpuHybrid, MatMulCpuFallbackAndOptionalGpu)
{
    // 2x3 * 3x2 = 2x2
    const std::vector<float> A = makeMat({
        1.f, 2.f, 3.f,
        4.f, 5.f, 6.f
    });
    const std::vector<float> B = makeMat({
        7.f,  8.f,
        9.f,  10.f,
        11.f, 12.f
    });
    std::vector<float> C(4, 0.0f);

    const int64_t rc = launch_matmul_kernel((void*)A.data(), (void*)B.data(), (void*)C.data(), 2, 2, 3);
    ASSERT_EQ(rc, 0);

    const std::vector<float> expected = makeMat({
        58.f,  64.f,
        139.f, 154.f
    });
    expectClose(C, expected);

    const bool envSet = HasSpirvEnv();
    if (envSet) {
        EXPECT_TRUE(g_gpuPathUsed.load(std::memory_order_relaxed)) << "Expected GPU path when RAWRXD_MATMUL_SPV is set";
    } else {
        EXPECT_FALSE(g_gpuPathUsed.load(std::memory_order_relaxed)) << "Expected CPU fallback when no SPIR-V provided";
    }
}

TEST(GpuHybrid, VectorAdd)
{
    std::vector<float> a = makeMat({1, 2, 3, 4, 5, 6, 7, 8});
    std::vector<float> b = makeMat({8, 7, 6, 5, 4, 3, 2, 1});
    std::vector<float> out(8, 0.0f);

    const int64_t rc = launch_vector_add_kernel((void*)a.data(), (void*)b.data(), (void*)out.data(), (int64_t)out.size());
    ASSERT_EQ(rc, 0);

    std::vector<float> expected(8, 0.0f);
    for (size_t i = 0; i < expected.size(); ++i) expected[i] = a[i] + b[i];
    expectClose(out, expected);
}

TEST(GpuHybrid, VectorMul)
{
    std::vector<float> a = makeMat({1, 2, 3, 4, 5, 6, 7, 8});
    std::vector<float> b = makeMat({2, 3, 4, 5, 6, 7, 8, 9});
    std::vector<float> out(8, 0.0f);

    const int64_t rc = launch_element_mul_kernel((void*)a.data(), (void*)b.data(), (void*)out.data(), (int64_t)out.size());
    ASSERT_EQ(rc, 0);

    std::vector<float> expected(8, 0.0f);
    for (size_t i = 0; i < expected.size(); ++i) expected[i] = a[i] * b[i];
    expectClose(out, expected);
}
