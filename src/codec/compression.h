#pragma once
#include <vector>
#include <cstdint>

namespace codec {
    std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool* success = nullptr);
    std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool* success = nullptr);
}
