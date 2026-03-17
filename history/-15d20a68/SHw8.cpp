// Stub implementations for CPU Inference Engine to support IDE linking
#include "cpu_inference_engine.h"
#include <iostream>

namespace CPUInference {

CPUInferenceEngine::CPUInferenceEngine() 
    : m_contextLimit(4096), m_modelLoaded(false), m_maxMode(false), 
      m_deepThinking(false), m_deepResearch(false), m_embeddingDim(768),
      m_vocabSize(32000) {}

CPUInferenceEngine::~CPUInferenceEngine() {}

void CPUInferenceEngine::SetContextLimit(size_t limit) {
    m_contextLimit = limit;
    for (auto& plugin : m_memoryPlugins) {
        plugin->Configure(limit);
    }
}

size_t CPUInferenceEngine::GetContextLimit() const {
    return m_contextLimit;
}

bool CPUInferenceEngine::IsModelLoaded() const {
    return m_modelLoaded;
}

void CPUInferenceEngine::RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin> plugin) {
    if (plugin) {
        m_memoryPlugins.push_back(plugin);
        std::cout << "[CPUInferenceEngine] Registered memory plugin: " << plugin->GetName() << std::endl;
    }
}

void CPUInferenceEngine::SetMaxMode(bool enabled) {
    m_maxMode = enabled;
    if (m_maxMode) {
        SetThreadCount(std::thread::hardware_concurrency());
        SetContextLimit(std::max(m_contextLimit, (size_t)131072)); // Min 128K tokens
        if (m_contextLimit < 32768) {
            SetContextLimit(32768);
        }
    }
}

void CPUInferenceEngine::SetDeepThinking(bool enabled) {
    m_deepThinking = enabled;
    if (m_deepThinking) {
        if (m_contextLimit < 65536) {
            SetContextLimit(65536);
        }
        std::cout << "[CPUInferenceEngine] Deep Thinking enabled with " << m_contextLimit << " tokens" << std::endl;
    }
}

void CPUInferenceEngine::SetDeepResearch(bool enabled) {
    m_deepResearch = enabled;
    if (m_deepResearch) {
        if (m_contextLimit < 1048576) {
            SetContextLimit(1048576); // 1M tokens
        }
        for (auto& plugin : m_memoryPlugins) {
            plugin->Optimize();
        }
        std::cout << "[CPUInferenceEngine] Deep Research mode enabled with " << m_contextLimit << " tokens" << std::endl;
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

void CPUInferenceEngine::SetThreadCount(int num_threads) {
    m_threadCount = num_threads;
}

int CPUInferenceEngine::GetThreadCount() const {
    return m_threadCount;
}

void CPUInferenceEngine::UpdateOutputWeights(const std::vector<float>& gradients, float learningRate) {
    if (m_lastState.empty() || m_lastState.size() != m_embeddingDim) return;
    
    float* weights = reinterpret_cast<float*>(m_outputWeights.data.data());
    for (int v = 0; v < m_vocabSize; ++v) {
        if (v >= gradients.size()) break;
        float grad_v = gradients[v];
        float* w_row = weights + v * m_embeddingDim;
        for (size_t k = 0; k < m_embeddingDim; ++k) {
            w_row[k] -= learningRate * grad_v * m_lastState[k];
        }
    }
}

} // namespace CPUInference
