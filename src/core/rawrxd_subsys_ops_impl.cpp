#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <windows.h>

// ---------------------------------------------------------------------------
// Internal telemetry (mirrors subsystem_mode_fallbacks.cpp pattern)
// ---------------------------------------------------------------------------
namespace
{
std::atomic<uint64_t> g_subsysCallCount{0};
std::atomic<uint32_t> g_lastSubsysHash{0};

inline uint32_t fnv1a32_subsys(const char* text)
{
    uint32_t hash = 2166136261u;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != '\0'; ++p)
    {
        hash ^= static_cast<uint32_t>(*p);
        hash *= 16777619u;
    }
    return hash;
}

inline void noteSubsysCall(const char* name)
{
    g_subsysCallCount.fetch_add(1, std::memory_order_relaxed);
    g_lastSubsysHash.store(fnv1a32_subsys(name), std::memory_order_relaxed);
}
}  // namespace

// ---------------------------------------------------------------------------
// SO_ subsystem stubs
// ---------------------------------------------------------------------------
extern "C" void SO_CreateThreadPool(void)
{
    noteSubsysCall("SO_CreateThreadPool");
}

extern "C" void SO_StartDEFLATEThreads(void)
{
    noteSubsysCall("SO_StartDEFLATEThreads");
}

extern "C" void SO_InitializePrefetchQueue(void)
{
    noteSubsysCall("SO_InitializePrefetchQueue");
}

extern "C" void SO_PrintMetrics(void)
{
    noteSubsysCall("SO_PrintMetrics");
}

// ---------------------------------------------------------------------------
// matmul_kernel_avx2 – naive row-major triple-loop GEMM (C = A * B)
// A: M x K,  B: K x N,  C: M x N
// ---------------------------------------------------------------------------
extern "C" void matmul_kernel_avx2(const float* A, const float* B, float* C, int M, int K, int N)
{
    if (A == nullptr || B == nullptr || C == nullptr || M <= 0 || K <= 0 || N <= 0)
    {
        return;
    }
    for (int m = 0; m < M; ++m)
    {
        for (int n = 0; n < N; ++n)
        {
            float acc = 0.0f;
            for (int k = 0; k < K; ++k)
            {
                acc += A[static_cast<size_t>(m) * static_cast<size_t>(K) + static_cast<size_t>(k)]
                     * B[static_cast<size_t>(k) * static_cast<size_t>(N) + static_cast<size_t>(n)];
            }
            C[static_cast<size_t>(m) * static_cast<size_t>(N) + static_cast<size_t>(n)] = acc;
        }
    }
}

// ---------------------------------------------------------------------------
// ggml_gemm_q4_0 – no-op stub; real Q4_0 requires quantization context.
// Zero-fills dst so callers receive a valid (zeroed) result rather than trash.
// ---------------------------------------------------------------------------
extern "C" void ggml_gemm_q4_0(const void* src0, const void* src1, float* dst, int64_t ne00, int64_t ne01)
{
    (void)src0;
    (void)src1;
    if (dst != nullptr && ne00 > 0 && ne01 > 0)
    {
        std::memset(dst, 0, static_cast<size_t>(ne00) * static_cast<size_t>(ne01) * sizeof(float));
    }
}

// ---------------------------------------------------------------------------
// RawrXD_Native_Log – exact impl from rawrxd_native_log_fallback.cpp
// ---------------------------------------------------------------------------
extern "C" void RawrXD_Native_Log(const char* fmt, ...)
{
    if (fmt == nullptr)
    {
        return;
    }

    std::array<char, 2048> stackBuffer = {};
    va_list args;
    va_start(args, fmt);
    const int written = std::vsnprintf(stackBuffer.data(), stackBuffer.size(), fmt, args);
    va_end(args);
    if (written < 0)
    {
        return;
    }

    const char* text = stackBuffer.data();
    std::string heapBuffer;
    if (static_cast<size_t>(written) >= stackBuffer.size())
    {
        heapBuffer.resize(static_cast<size_t>(written) + 1U, '\0');
        va_start(args, fmt);
        std::vsnprintf(heapBuffer.data(), heapBuffer.size(), fmt, args);
        va_end(args);
        text = heapBuffer.c_str();
    }

    std::fputs(text, stderr);
    std::fputc('\n', stderr);
    OutputDebugStringA(text);
    OutputDebugStringA("\n");
}
