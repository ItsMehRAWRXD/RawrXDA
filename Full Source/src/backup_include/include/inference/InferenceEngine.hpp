#pragma once
/**
 * @file InferenceEngine.hpp
 * @brief Inference engine interface (stub for builds with -I include).
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
