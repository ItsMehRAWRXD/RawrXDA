#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "utils/Expected.h"

namespace RawrXD {

struct ParsedGGUFModel {
    size_t totalSize;
    std::vector<std::string> vocab;
    std::vector<uint8_t> weights;
    // Add more metadata as needed
    std::vector<float> token_scores;
    std::vector<uint32_t> token_types;
    
    struct TensorMetadata {
        std::string name;
        size_t offset; // Offset within the weights vector
        size_t size;
        std::vector<uint64_t> shape;
        int type; // GGMLType enum value
    };
    std::vector<TensorMetadata> tensors;
};

class GGUFParser {
public:
    GGUFParser() = default;
    
    // Returns ParsedGGUFModel or error message string
    Expected<ParsedGGUFModel, std::string> parse(const std::string& path);
};
}
