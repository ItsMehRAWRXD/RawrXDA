#include "cpu_inference_engine.h"

#include <cmath>

namespace RawrXD {

CPUInferenceEngine::CPUInferenceEngine() = default;
CPUInferenceEngine::~CPUInferenceEngine() = default;

bool CPUInferenceEngine::MatVecQ4(const float* matrix, const float* vector, float* output,
                                 uint32_t rows, uint32_t cols) {
    if (!matrix || !vector || !output) return false;
    for (uint32_t r = 0; r < rows; ++r) {
        const float* rowPtr = matrix + (size_t)r * cols;
        float sum = 0.0f;
        for (uint32_t c = 0; c < cols; ++c) {
            sum += rowPtr[c] * vector[c];
        }
        output[r] = sum;
    }
    return true;
}

bool CPUInferenceEngine::Softmax(float* data, uint32_t size) {
    if (!data || size == 0) {
        return false;
    }
    float maxv = data[0];
    for (uint32_t i = 1; i < size; ++i) {
        if (data[i] > maxv) {
            maxv = data[i];
        }
    }
    double sum = 0.0;
    for (uint32_t i = 0; i < size; ++i) {
        data[i] = std::exp(data[i] - maxv);
        sum += data[i];
    }
    if (sum <= 0.0) {
        return false;
    }
    double inv = 1.0 / sum;
    for (uint32_t i = 0; i < size; ++i) {
        data[i] = static_cast<float>(data[i] * inv);
    }
    return true;
}

CPUInferenceEngine* CPUInferenceEngine::getInstance() {
    static CPUInferenceEngine instance;
    return &instance;
}

void CPUInferenceEngine::SetContextLimit(size_t limit) {
    m_contextLimit = limit;
}

void CPUInferenceEngine::RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin> plugin) {
    if (plugin) {
        m_memoryPlugins.push_back(std::move(plugin));
    }
}

std::vector<int> CPUInferenceEngine::Tokenize(const std::string& text) {
    std::vector<int> result;
    int token = 0;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (token != 0) {
                result.push_back(token);
                token = 0;
            }
        } else {
            token = token * 10 + (c % 10);
        }
    }
    if (token != 0) {
        result.push_back(token);
    }
    return result;
}

std::string CPUInferenceEngine::Detokenize(const std::vector<int>& tokens) {
    std::string out;
    for (size_t i = 0; i < tokens.size(); ++i) {
        out += std::to_string(tokens[i]);
        if (i + 1 < tokens.size()) {
            out += " ";
        }
    }
    return out;
}

bool CPUInferenceEngine::LoadModel(const std::string& /*model_path*/) {
    m_modelLoaded = true;
    return true;
}

bool CPUInferenceEngine::LoadWeights(const std::unordered_map<std::string, Tensor>& tensors) {
    m_weights = tensors;
    m_modelLoaded = true;
    return true;
}

std::vector<float> CPUInferenceEngine::Eval(const std::vector<int32_t>& /*input_tokens*/) {
    return {};
}

void CPUInferenceEngine::UpdateWeights(const std::vector<std::vector<float>>& /*layer_gradients*/, float /*learning_rate*/) {
}

void CPUInferenceEngine::UpdateOutputWeights(const std::vector<float>& /*gradients*/, float /*learningRate*/) {
}

void CPUInferenceEngine::GenerateStreaming(
    const std::vector<int32_t>& /*input_tokens*/,
    int /*max_tokens*/,
    std::function<void(const std::string&)> token_callback,
    std::function<void()> complete_callback,
    std::function<void(int32_t)> /*token_id_callback*/) {
    if (complete_callback) {
        complete_callback();
    }
}

size_t CPUInferenceEngine::GetMemoryUsage() const {
    return 0;
}

void CPUInferenceEngine::ClearCache() {
}

void CPUInferenceEngine::SetDeepThinking(bool enabled) {
    m_deepThinking = enabled;
}

void CPUInferenceEngine::SetDeepResearch(bool enabled) {
    m_deepResearch = enabled;
}

void CPUInferenceEngine::SetMaxMode(bool enabled) {
    m_maxMode = enabled;
}

} // namespace RawrXD
