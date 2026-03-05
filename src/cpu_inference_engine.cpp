#include "cpu_inference_engine.h"
#include <iostream>

namespace RawrXD {
    static CPUInferenceEngine* instance = nullptr;
    CPUInferenceEngine* CPUInferenceEngine::getInstance() {
        if (!instance) instance = new CPUInferenceEngine();
        return instance;
    }
    bool CPUInferenceEngine::LoadModel(const std::string& path) {
        std::cout << "[TITAN] Loading: " << path << std::endl;
        return true;
    }
    std::vector<int32_t> CPUInferenceEngine::Tokenize(const std::string& text) {
        std::vector<int32_t> t;
        t.push_back(1);
        t.push_back(2);
        t.push_back(3);
        return t;
    }
    std::string CPUInferenceEngine::Detokenize(const std::vector<int32_t>& tokens) {
        return "Hello from TITAN Local Mode!";
    }
}
