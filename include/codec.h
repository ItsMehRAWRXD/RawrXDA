<<<<<<< HEAD
#pragma once

// codec.h — C++20, Qt-free. QByteArray → std::vector<uint8_t>

#include <vector>
#include <cstdint>

namespace codec {
    std::vector<uint8_t> deflate(const std::vector<uint8_t>& input, bool* success = nullptr);
    std::vector<uint8_t> inflate(const std::vector<uint8_t>& input, bool* success = nullptr);
}
=======
#pragma once

// codec.h — C++20, Qt-free. QByteArray → std::vector<uint8_t>

#include <vector>
#include <cstdint>

namespace codec {
    std::vector<uint8_t> deflate(const std::vector<uint8_t>& input, bool* success = nullptr);
    std::vector<uint8_t> inflate(const std::vector<uint8_t>& input, bool* success = nullptr);
}
>>>>>>> origin/main
