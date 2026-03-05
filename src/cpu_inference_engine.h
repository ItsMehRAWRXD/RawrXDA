#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {
class CPUInferenceEngine {
public:
    static CPUInferenceEngine* getInstance();
    bool LoadModel(const std::string& path);
    std::vector<int32_t> Tokenize(const std::string& text);
    std::string Detokenize(const std::vector<int32_t>& tokens);
};
}
