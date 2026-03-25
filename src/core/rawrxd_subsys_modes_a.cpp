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

extern "C" void CompileMode(void)
{
    noteModeCall("CompileMode");
}
extern "C" void EncryptMode(void)
{
    noteModeCall("EncryptMode");
}
extern "C" void InjectMode(void)
{
    noteModeCall("InjectMode");
}
extern "C" void UACBypassMode(void)
{
    noteModeCall("UACBypassMode");
}
extern "C" void PersistenceMode(void)
{
    noteModeCall("PersistenceMode");
}
extern "C" void SideloadMode(void)
{
    noteModeCall("SideloadMode");
}
extern "C" void AVScanMode(void)
{
    noteModeCall("AVScanMode");
}
