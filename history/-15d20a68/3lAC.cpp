// Stub implementations for CPU Inference Engine to support IDE linking
#include "cpu_inference_engine.h"
#include <iostream>
#include <thread>

namespace CPUInference {

CPUInferenceEngine::CPUInferenceEngine() 
    : m_contextLimit(4096), m_modelLoaded(false), m_maxMode(false), 
      m_deepThinking(false), m_deepResearch(false), m_embeddingDim(768),
      m_vocabSize(32000), m_threadCount(4) {}

CPUInferenceEngine::~CPUInferenceEngine() {}

void CPUInferenceEngine::SetContextLimit(size_t limit) {
    m_contextLimit = limit;
    for (auto& plugin : m_memoryPlugins) {
        plugin->Configure(limit);
    }
}

void CPUInferenceEngine::RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin> plugin) {
    if (plugin) {
        m_memoryPlugins.push_back(plugin);
        std::cout << "[CPUInferenceEngine] Registered memory plugin: " << plugin->GetName() << std::endl;
    }
}

bool CPUInferenceEngine::LoadModel(const std::string& model_path) {
    std::cout << "[CPUInferenceEngine] Loading model from: " << model_path << std::endl;
    m_modelLoaded = true;
    return true;
}

bool CPUInferenceEngine::LoadWeights(const std::unordered_map<std::string, Tensor>& tensors) {
    m_weights = tensors;
    return true;
}

std::vector<float> CPUInferenceEngine::Generate(const std::vector<int32_t>& input_tokens, int max_tokens) {
    std::vector<float> output;
    for (int i = 0; i < max_tokens; ++i) {
        output.push_back(0.5f); // Dummy output
    }
    return output;
}

void CPUInferenceEngine::GenerateStreaming(
    const std::vector<int32_t>& input_tokens,
    int max_tokens,
    std::function<void(const std::string&)> on_token,
    std::function<void()> on_complete,
    std::function<void(int)> on_progress) {
    // Stub: dummy streaming
    if (on_progress) on_progress(0);
    if (on_token) on_token("stub ");
    if (on_complete) on_complete();
}

std::vector<int32_t> CPUInferenceEngine::Tokenize(const std::string& text) {
    std::vector<int32_t> tokens;
    for (size_t i = 0; i < text.size(); ++i) {
        tokens.push_back(static_cast<int32_t>(text[i]));
    }
    return tokens;
}

std::string CPUInferenceEngine::Detokenize(const std::vector<int32_t>& tokens) {
    std::string result;
    for (int32_t token : tokens) {
        result += static_cast<char>(token % 256);
    }
    return result;
}

} // namespace CPUInference
