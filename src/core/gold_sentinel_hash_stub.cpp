#include <cstdint>
#include <cstddef>

extern "C" uint64_t RawrXD_Sentinel_CalculateHash_MASM(void* addr, size_t size)
{
    const auto* bytes = static_cast<const uint8_t*>(addr);
    if (!bytes || size == 0)
    {
        return 0;
    }

    // FNV-1a 64-bit fallback hash for Gold link closure.
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < size; ++i)
    {
        h ^= static_cast<uint64_t>(bytes[i]);
        h *= 1099511628211ull;
    }
    return h;
}
