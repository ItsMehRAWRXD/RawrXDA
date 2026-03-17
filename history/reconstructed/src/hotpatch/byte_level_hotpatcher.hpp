#pragma once
#include <vector>
#include <cstdint>

namespace RawrXD {

struct ByteSearchResult;

ByteSearchResult direct_search(const char* moduleName, 
                                const unsigned char* pattern, 
                                size_t patternLen);

bool apply_patch(void* address, const std::vector<uint8_t>& bytes, 
                std::vector<uint8_t>* oldBytes = nullptr);

} // namespace RawrXD
