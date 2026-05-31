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

int asm_hwsynth_init(uint32_t targetArch = 0, uint32_t clockMhz = 100, uint32_t lutCount = 10000)
{
    std::lock_guard<std::mutex> lock(g_hwsynthMtx);
    g_targetArch  = targetArch;
    g_clockMhz    = clockMhz;
    g_lutCount    = lutCount;
    g_initialized = true;
    return 1;
}

int asm_hwsynth_profile_dataflow(const void* tensorBase, uint32_t M, uint32_t N, uint32_t K, uint32_t elemBits, void* profileOut)
{
    if (tensorBase == nullptr || profileOut == nullptr)
    {
        return -1;
    }
    uint32_t* out = static_cast<uint32_t*>(profileOut);
    out[0] = M;
    out[1] = N;
    out[2] = K;
    out[3] = elemBits;

    g_profileCount.fetch_add(1u, std::memory_order_relaxed);
    return 1;
}

int asm_hwsynth_gen_gemm_spec(const void* profile, uint32_t fpgaFamily, void* specOut)
{
    if (profile == nullptr || specOut == nullptr)
    {
        return -1;
    }
    const uint32_t* p = static_cast<const uint32_t*>(profile);
    uint32_t* out = static_cast<uint32_t*>(specOut);
    
    // Simple design logic for specOut (GemmSpec struct)
    out[0] = (p[0] > 128) ? 32 : 16; // arrayDimM
    out[1] = (p[1] > 128) ? 32 : 16; // arrayDimN
    out[2] = p[3];                   // peDataWidth
    out[3] = 32;                     // accumWidth
    out[4] = 1;                      // numUnits
    
    g_gemmSpecCount.fetch_add(1u, std::memory_order_relaxed);
    return 1;
}

int asm_hwsynth_analyze_memhier(const void* profile, void* analysisOut)
{
    if (profile == nullptr || analysisOut == nullptr)
    {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_hwsynthMtx);
    const uint64_t bwEst = static_cast<uint64_t>(g_clockMhz) * 4u;
    uint64_t* out = static_cast<uint64_t*>(analysisOut);
    out[0] = bwEst;
    return 1;
}

float asm_hwsynth_predict_perf(const void* gemmSpec, const void* profile)
{
    if (gemmSpec == nullptr || profile == nullptr)
    {
        return 0.0f;
    }
    std::lock_guard<std::mutex> lock(g_hwsynthMtx);
    return static_cast<float>(g_clockMhz) / 10.0f;
}

int asm_hwsynth_est_resources(const void* gemmSpec, uint32_t fpgaFamily, void* resourcesOut)
{
    if (gemmSpec == nullptr || resourcesOut == nullptr)
    {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_hwsynthMtx);
    uint64_t* out = static_cast<uint64_t*>(resourcesOut);
    out[0] = static_cast<uint64_t>(g_lutCount);
    return 1;
}

int asm_hwsynth_shutdown()
{
    std::lock_guard<std::mutex> lock(g_hwsynthMtx);
    g_initialized = false;
    return 1;
}

void* asm_hwsynth_get_stats()
{
    static uint32_t stats[8];
    stats[0] = g_profileCount.load();
    stats[1] = g_gemmSpecCount.load();
    stats[2] = g_clockMhz;
    stats[3] = g_lutCount;
    return stats;
}

int asm_spengine_cpu_optimize(const void* profile)
{
    (void)profile;
    return 1;
}

int asm_hwsynth_gen_jtag_header(void* outBuf, uint64_t bufSize, uint32_t fpgaFamily, const void* gemmSpec)
{
    if (outBuf == nullptr || gemmSpec == nullptr)
    {
        return -1;
    }
    uint32_t* out = static_cast<uint32_t*>(outBuf);
    out[0] = 0x5258444A; // RXDJ
    return 1;
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
