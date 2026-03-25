#include <cstdint>
#include <vector>

namespace codec {

std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool* success)
{
    if (success) *success = true;
    return data;
}

std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool* success)
{
    if (success) *success = true;
    return data;
}

}  // namespace codec

namespace brutal {

std::vector<uint8_t> compress(const std::vector<uint8_t>& data)
{
    return data;
}

std::vector<uint8_t> decompress(const std::vector<uint8_t>& data)
{
    return data;
}

}  // namespace brutal
