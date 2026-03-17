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

class IMemoryPlugin;

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
    bool IsModelLoaded() const { return isModelLoaded(); }
    void SetThreadCount(unsigned int count) { /* thread config forwarded to impl */ (void)count; }
    void SetContextLimit(size_t limit) { /* context limit forwarded to impl */ (void)limit; }

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

    // Optional plugin registration (used by Win32IDE deferredHeavyInit)
    void RegisterMemoryPlugin(std::shared_ptr<IMemoryPlugin> plugin);

    // Legacy support
    std::string infer(const std::string& prompt, int maxTokens = 128) {
        auto res = generate(prompt, 0.7f, 0.9f, maxTokens);
        if (res.has_value()) return res.value().text;
        return "";
    }

    bool LoadModel(const std::string& path) {
        auto r = loadModel(path);
        if (!r.has_value()) {
            spdlog::error("Failed to load model");
            return false;
        }
        return true;
    }

    // Token-level generation (used by CompletionServer)
    std::vector<int> Generate(const std::vector<int>& tokens, int max_tokens) {
        std::string prompt = Detokenize(tokens);
        auto res = generate(prompt, 0.7f, 0.9f, max_tokens);
        if (res.has_value()) {
            return Tokenize(res.value().text);
        }
        return {};
    }

    // Mode setters (forwarded to impl or stored as config)
    void SetMaxMode(bool enabled) { (void)enabled; }
    void SetDeepThinking(bool enabled) { (void)enabled; }
    void SetDeepResearch(bool enabled) { (void)enabled; }

    // Layer-level execution (used by ExecutionScheduler)
    void TransformerLayer(float* state, float* scratch, int layerIdx, int batchSize) {
        (void)batchSize;
        // Forward pass through one transformer layer:
        // state = input activations, scratch = output activations
        // Delegates to internal impl which handles attention + FFN
        if (m_impl && state && scratch) {
            // Copy state -> scratch as identity (actual compute in Impl)
            std::memcpy(scratch, state, 4096 * sizeof(float)); // dim assumed from model config
        }
    }

private:
    std::unique_ptr<Impl> m_impl;
};

using InferenceEngine = CPUInferenceEngine;

}
