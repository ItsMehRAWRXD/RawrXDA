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

extern "C" void EntropyMode(void)
{
    noteModeCall("EntropyMode");
}
extern "C" void StubGenMode(void)
{
    noteModeCall("StubGenMode");
}
extern "C" void TraceEngineMode(void)
{
    noteModeCall("TraceEngineMode");
}
extern "C" void AgenticMode(void)
{
    noteModeCall("AgenticMode");
}
extern "C" void BasicBlockCovMode(void)
{
    noteModeCall("BasicBlockCovMode");
}
extern "C" void CovFusionMode(void)
{
    noteModeCall("CovFusionMode");
}
extern "C" void DynTraceMode(void)
{
    noteModeCall("DynTraceMode");
}
