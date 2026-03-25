#include <atomic>
#include <cstdint>

namespace
{
std::atomic<uint64_t> g_modeCallCount{0};
std::atomic<uint32_t> g_lastModeHash{0};

inline uint32_t fnv1a32(const char* text)
{
    uint32_t hash = 2166136261u;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != '\0'; ++p)
    {
        hash ^= static_cast<uint32_t>(*p);
        hash *= 16777619u;
    }
    return hash;
}

inline void noteModeCall(const char* modeName)
{
    g_modeCallCount.fetch_add(1, std::memory_order_relaxed);
    g_lastModeHash.store(fnv1a32(modeName), std::memory_order_relaxed);
}
}  // namespace

extern "C" void AgentTraceMode(void)
{
    noteModeCall("AgentTraceMode");
}
extern "C" void GapFuzzMode(void)
{
    noteModeCall("GapFuzzMode");
}
extern "C" void IntelPTMode(void)
{
    noteModeCall("IntelPTMode");
}
extern "C" void DiffCovMode(void)
{
    noteModeCall("DiffCovMode");
}
extern "C" void AD_ProcessGGUF(void)
{
    noteModeCall("AD_ProcessGGUF");
}
extern "C" void SO_LoadExecFile(void)
{
    noteModeCall("SO_LoadExecFile");
}
extern "C" void SO_InitializeVulkan(void)
{
    noteModeCall("SO_InitializeVulkan");
}
extern "C" void SO_CreateMemoryArena(void)
{
    noteModeCall("SO_CreateMemoryArena");
}
extern "C" void SO_CreateComputePipelines(void)
{
    noteModeCall("SO_CreateComputePipelines");
}
extern "C" void SO_PrintStatistics(void)
{
    noteModeCall("SO_PrintStatistics");
}
extern "C" void SO_InitializeStreaming(void)
{
    noteModeCall("SO_InitializeStreaming");
}
