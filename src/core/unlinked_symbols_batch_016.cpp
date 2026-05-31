// Gold link closure batch 16 — agent registry + Sentinel hash (Detour MASM not in MASM_OBJECTS).

#include <cstdint>

namespace RawrXD::Sovereign
{
extern "C"
{
    uint64_t g_AgentRegistry[32] = {};
}
}  // namespace RawrXD::Sovereign

extern "C" uint64_t RawrXD_Sentinel_CalculateHash_MASM(void* addr, size_t size)
{
    if (addr == nullptr || size == 0)
    {
        return 0;
    }
    constexpr uint64_t kOffset = 14695981039346656037ull;
    constexpr uint64_t kPrime = 1099511628211ull;
    uint64_t h = kOffset;
    const auto* p = static_cast<const unsigned char*>(addr);
    for (size_t i = 0; i < size; ++i)
    {
        h ^= static_cast<uint64_t>(p[i]);
        h *= kPrime;
    }
    return h;
}
