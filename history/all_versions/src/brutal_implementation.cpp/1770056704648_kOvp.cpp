#include <vector>
#include <cstddef>
#include <cstdint>

namespace brutal {

std::vector<uint8_t> compress(const std::vector<uint8_t>& in) {
    // Stub implementation: Just return input (no compression) or empty.
    // Since we are verifying logic, return input if safe, or empty to avoid logic confusing uncompressed with compressed.
    // If GGUF saver expects compressed, passing uncompressed might break.
    // But since this is a stub for linking, return empty is safer than crashing.
    return in; 
}

std::vector<uint8_t> compress(const void* data, std::size_t size) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    return std::vector<uint8_t>(ptr, ptr + size);
}

} // namespace brutal
