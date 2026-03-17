#pragma once

#include <vector>
#include <cstdint>

namespace codec {
    // Convert from std::vector<uint8_t> to match the original signature
    // but without Qt dependencies
    std::vector<uint8_t> deflate(const std::vector<uint8_t>& input, bool* success = nullptr);
    std::vector<uint8_t> inflate(const std::vector<uint8_t>& input, bool* success = nullptr);
}