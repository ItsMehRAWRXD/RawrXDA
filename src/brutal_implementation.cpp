#include <cstddef>
#include <cstdint>
#include <vector>


namespace brutal
{

std::vector<uint8_t> compress(const std::vector<uint8_t>& in)
{
    // Pass-through stub: returns input uncompressed.
    // Used for linking when full compression backend is not available.
    return in;
}

std::vector<uint8_t> compress(const void* data, std::size_t size)
{
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    return std::vector<uint8_t>(ptr, ptr + size);
}

std::vector<uint8_t> decompress(const std::vector<uint8_t>& in)
{
    // Pass-through stub: assumes data is already uncompressed.
    // This keeps "packed GGUF/RXA" paths linkable in zero-dep lanes.
    return in;
}

}  // namespace brutal
