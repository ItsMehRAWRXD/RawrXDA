#pragma once
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include "CommonTypes.h"
#include "utils/Expected.h"

namespace RawrXD {

enum class InferenceError {
    None = 0,
    ModelNotFound,
    ContextFull,
    InternalError
};

class CPUInferenceEngine {
public:
    class Impl;

    CPUInferenceEngine();
    ~CPUInferenceEngine();

    RawrXD::Expected<void, InferenceError> loadModel(const std::string& path);
    bool isModelLoaded() const; // Added for CompletionEngine compat

    // Core Generation Interface
    struct GenerationResult {
        std::string text;
        float confidence;
        int tokens_generated;
    };

    RawrXD::Expected<GenerationResult, InferenceError> generate(
        const std::string& prompt,
        float temp = 0.7f,
        float top_p = 0.9f,
        int max_tokens = 512
    );

    // Streaming support
    using StreamCallback = std::function<void(const std::string&)>;
    using DoneCallback = std::function<void()>;
    
    void GenerateStreaming(
        const std::vector<int>& tokens,
        int max_tokens,
        StreamCallback on_token,
        DoneCallback on_done
    );

    // Tokenization
    std::vector<int> Tokenize(const std::string& text);
    std::string Detokenize(const std::vector<int>& tokens);

    // Status & Metrics
    nlohmann::json getStatus() const;

    // Legacy support
    std::string infer(const std::string& prompt, int maxTokens = 128) {
        auto res = generate(prompt, 0.7f, 0.9f, maxTokens);
        if (res.has_value()) return res.value().text;
        return "";
    }

    void LoadModel(const std::string& path) {
        if (!loadModel(path).has_value()) {
            spdlog::error("Failed to load model");
        }
    }

private:
    std::unique_ptr<Impl> m_impl;
};

using InferenceEngine = CPUInferenceEngine;

}
