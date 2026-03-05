// codec_stubs.cpp — Stub implementations for codec and brutal compression
// These are forward-declared in gguf_loader.cpp. Real implementations would
// link against zlib/deflate. For CPU inference, GGUF files are uncompressed
// so these are never called in the hot path.

#include <vector>
#include <cstdint>
#include <cstring>

namespace codec {
    std::vector<uint8_t> deflate(const std::vector<uint8_t>& in, bool* ok) {
        if (ok) *ok = false;
        return {};
    }
    std::vector<uint8_t> inflate(const std::vector<uint8_t>& in, bool* ok) {
        if (ok) *ok = false;
        return in; // Pass-through for uncompressed data
    }
}

namespace brutal {
    std::vector<uint8_t> compress(const std::vector<uint8_t>& in) {
        return in; // No-op
    }
    std::vector<uint8_t> compress(const void* data, std::size_t size) {
        std::vector<uint8_t> out(size);
        std::memcpy(out.data(), data, size);
        return out;
    }
}
