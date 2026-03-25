#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <cmath>

namespace
{
    std::mutex             g_hwsynthMtx;
    uint32_t               g_targetArch   = 0;
    uint32_t               g_clockMhz     = 0;
    uint32_t               g_lutCount     = 0;
    std::atomic<uint64_t>  g_profileCount{0};
    std::atomic<uint64_t>  g_gemmSpecCount{0};
    bool                   g_initialized  = false;
}

extern "C"
{

int asm_hwsynth_init(uint32_t targetArch, uint32_t clockMhz, uint32_t lutCount)
{
    std::lock_guard<std::mutex> lock(g_hwsynthMtx);
    g_targetArch  = targetArch;
    g_clockMhz    = clockMhz;
    g_lutCount    = lutCount;
    g_initialized = true;
    return 1;
}

int asm_hwsynth_profile_dataflow(const void* irGraph, uint32_t nodeCount, void* profileOut)
{
    if (irGraph == nullptr || profileOut == nullptr)
    {
        return -1;
    }
    uint32_t* out = static_cast<uint32_t*>(profileOut);
    out[0] = nodeCount;
    g_profileCount.fetch_add(1u, std::memory_order_relaxed);
    return 1;
}

int asm_hwsynth_gen_gemm_spec(uint32_t M, uint32_t K, uint32_t N, void* specOut, uint32_t* specLen)
{
    if (specOut == nullptr)
    {
        return -1;
    }
    uint32_t* out = static_cast<uint32_t*>(specOut);
    out[0] = M;
    out[1] = K;
    out[2] = N;
    if (specLen != nullptr)
    {
        *specLen = 12u;
    }
    g_gemmSpecCount.fetch_add(1u, std::memory_order_relaxed);
    return 1;
}

int asm_hwsynth_analyze_memhier(const void* layout, uint32_t layoutLen, void* analysisOut)
{
    if (layout == nullptr || analysisOut == nullptr)
    {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_hwsynthMtx);
    const uint64_t bwEst = static_cast<uint64_t>(g_clockMhz) * 4u;
    uint64_t* out = static_cast<uint64_t*>(analysisOut);
    out[0] = bwEst;
    (void)layoutLen;
    return 1;
}

float asm_hwsynth_predict_perf(const void* spec, uint32_t specLen)
{
    if (spec == nullptr)
    {
        return 0.0f;
    }
    std::lock_guard<std::mutex> lock(g_hwsynthMtx);
    return static_cast<float>(specLen) * static_cast<float>(g_clockMhz) / 1000.0f;
}

uint64_t asm_hwsynth_est_resources(const void* spec, uint32_t specLen)
{
    if (spec == nullptr)
    {
        return 0u;
    }
    std::lock_guard<std::mutex> lock(g_hwsynthMtx);
    return static_cast<uint64_t>(g_lutCount) * static_cast<uint64_t>(specLen);
}

int asm_hwsynth_gen_jtag_header(const void* bitstream, uint32_t len, void* headerOut, uint32_t* headerLen)
{
    if (bitstream == nullptr || headerOut == nullptr)
    {
        return -1;
    }
    uint8_t* out = static_cast<uint8_t*>(headerOut);
    // JTAG sync word (0xFFFF) followed by 4-byte length field
    out[0] = 0xFFu;
    out[1] = 0xFFu;
    out[2] = static_cast<uint8_t>( len        & 0xFFu);
    out[3] = static_cast<uint8_t>((len >>  8u) & 0xFFu);
    out[4] = static_cast<uint8_t>((len >> 16u) & 0xFFu);
    out[5] = static_cast<uint8_t>((len >> 24u) & 0xFFu);
    if (headerLen != nullptr)
    {
        *headerLen = 6u;
    }
    return 1;
}

void asm_hwsynth_get_stats(void* statsOut)
{
    if (statsOut == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(g_hwsynthMtx);
    uint64_t* out = static_cast<uint64_t*>(statsOut);
    out[0] = static_cast<uint64_t>(g_targetArch);
    out[1] = static_cast<uint64_t>(g_clockMhz);
    out[2] = static_cast<uint64_t>(g_lutCount);
    out[3] = g_profileCount.load(std::memory_order_relaxed);
    out[4] = g_gemmSpecCount.load(std::memory_order_relaxed);
}

void asm_hwsynth_shutdown(void)
{
    std::lock_guard<std::mutex> lock(g_hwsynthMtx);
    g_targetArch  = 0u;
    g_clockMhz    = 0u;
    g_lutCount    = 0u;
    g_profileCount.store(0u, std::memory_order_relaxed);
    g_gemmSpecCount.store(0u, std::memory_order_relaxed);
    g_initialized = false;
}

} // extern "C"
