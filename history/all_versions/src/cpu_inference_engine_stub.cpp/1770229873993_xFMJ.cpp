#include "cpu_inference_engine.h"
#include <iostream>

namespace CPUInference {

CPUInferenceEngine::CPUInferenceEngine() {
    std::cout << "[CPU INFERENCE] Engine stub initialized\n";
}

CPUInferenceEngine::~CPUInferenceEngine() {
    std::cout << "[CPU INFERENCE] Engine stub destroyed\n";
}

bool CPUInferenceEngine::LoadModel(const std::string& model_path) {
    std::cout << "[CPU INFERENCE] Load model stub: " << model_path << "\n";
    return false; // Stub - not actually loading
}

std::string CPUInferenceEngine::Inference(const std::string& prompt) {
    return "[CPU Inference Stub] Response to: " + prompt;
}

bool CPUInferenceEngine::IsModelLoaded() const {
    return false; // Stub always returns false
}

void CPUInferenceEngine::UnloadModel() {
    std::cout << "[CPU INFERENCE] Unload model stub\n";
}

void CPUInferenceEngine::SetTemperature(float temp) {
    // Stub - no-op
}

void CPUInferenceEngine::SetTopP(float top_p) {
    // Stub - no-op
}

void CPUInferenceEngine::SetMaxTokens(int max_tokens) {
    // Stub - no-op
}

} // namespace CPUInference
