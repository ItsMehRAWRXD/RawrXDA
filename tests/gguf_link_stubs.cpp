#include <cstddef>
#include <cstdint>
#include <vector>

// These stubs exist so small, dependency-free smoke tests can link against
// `src/gguf_loader.cpp` without pulling in optional compression backends.
// The smoke test does not exercise compression paths.

namespace codec
{
std::vector<uint8_t> deflate(const std::vector<uint8_t>& in, bool* ok)
{
    if (ok)
        *ok = false;
    return in;
}

std::vector<uint8_t> inflate(const std::vector<uint8_t>& in, bool* ok)
{
    if (ok)
        *ok = false;
    return in;
}
}  // namespace codec

namespace brutal
{
std::vector<uint8_t> compress(const std::vector<uint8_t>& in)
{
    return in;
}

std::vector<uint8_t> compress(const void* data, std::size_t size)
{
    const auto* p = static_cast<const uint8_t*>(data);
    return std::vector<uint8_t>(p, p + size);
}
}  // namespace brutal
