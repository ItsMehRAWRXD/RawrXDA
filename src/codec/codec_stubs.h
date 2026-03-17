#pragma once
#include <vector>
#include <cstdint>

namespace codec {
    inline std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool* success) {
        if (success) *success = true;
        return data;
    }

    inline std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool* success) {
        if (success) *success = true;
        return data;
    }
}
