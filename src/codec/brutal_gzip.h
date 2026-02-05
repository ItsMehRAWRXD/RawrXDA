#pragma once
#include <vector>
#include <cstdint>
#include <string>

namespace brutal {
    // Basic compression interface (mock/passthrough for now)
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data);
}
