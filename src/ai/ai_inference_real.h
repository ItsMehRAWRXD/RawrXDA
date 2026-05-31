#pragma once
#include <string>
#include <vector>

namespace RawrXD {

struct InferenceResult {
    std::string text;
    std::string error;
};

// Forward declaration of functions from ai_inference_real.cpp
bool RunVulkanTruthPreflight();
bool LoadModelReal(const char* path);
InferenceResult RunInferenceReal(const std::string& prompt);

} // namespace RawrXD
