#include "codec/compression.h"
#include "codec/brutal_gzip.h"
#include <vector>

namespace codec {

std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool* success) {
    if (data.empty()) {
        if (success) *success = true;
        return {};
    }
    
    auto result = brutal::compress(data);
    if (success) *success = true;
    return result;
}

std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool* success) {
    if (data.empty()) {
        if (success) *success = true;
        return {};
    }
    
    auto result = brutal::decompress(data);
    if (success) *success = true;
    return result;
}

}
