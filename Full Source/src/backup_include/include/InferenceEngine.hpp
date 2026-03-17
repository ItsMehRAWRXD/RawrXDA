#pragma once

/**
 * @file InferenceEngine.hpp
 * @brief Stub: inference engine interface (used by tools/benchmarks).
 */

#include <string>

class InferenceEngine {
public:
    bool loadModel(const std::string& modelPath);
    std::string generate(const std::string& prompt, int maxTokens);
    void unloadModel();
private:
    bool m_loaded = false;
};
