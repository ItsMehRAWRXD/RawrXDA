#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>

extern "C" {
    // Brutal Deflate - optimized interface
    void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len);
    void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len);
}

namespace CPUInference {
namespace brutal {
    std::vector<uint8_t> compress_std(const std::vector<uint8_t>& input);
}

namespace codec {
    std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool* success);
    std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool* success);
}
}
