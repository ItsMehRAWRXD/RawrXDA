#pragma once
// ============================================================================
// inference_engine.h — Abstract Inference Engine Interface
// ============================================================================
// Polymorphic base class enabling the server and agentic systems to work
// with any inference backend: CPUInferenceEngine, DMLInferenceEngine, or
// future backends (Vulkan compute, CUDA, etc.).
//
// Both CPUInferenceEngine and DMLInferenceEngine inherit from this interface,
// and CompletionServer accepts an InferenceEngine* pointer.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include <vector>
#include <string>
#include <cstdint>
#include <functional>

namespace RawrXD {

class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;

    // ---- Model Lifecycle ----
    virtual bool LoadModel(const std::string& model_path) = 0;
    virtual bool IsModelLoaded() const = 0;

    // ---- Tokenization ----
    virtual std::vector<int32_t> Tokenize(const std::string& text) = 0;
    virtual std::string Detokenize(const std::vector<int32_t>& tokens) = 0;

    // ---- Inference ----
    virtual std::vector<int32_t> Generate(const std::vector<int32_t>& input_tokens, int max_tokens = 100) = 0;
    virtual std::vector<float> Eval(const std::vector<int32_t>& input_tokens) = 0;

    // ---- Streaming ----
    virtual void GenerateStreaming(
        const std::vector<int32_t>& input_tokens,
        int max_tokens,
        std::function<void(const std::string&)> token_callback,
        std::function<void()> complete_callback,
        std::function<void(int32_t)> token_id_callback = nullptr) = 0;

    // ---- Model Info ----
    virtual int GetVocabSize() const = 0;
    virtual int GetEmbeddingDim() const = 0;
    virtual int GetNumLayers() const = 0;
    virtual int GetNumHeads() const = 0;

    // ---- AI Mode Flags ----
    virtual void SetMaxMode(bool enabled) = 0;
    virtual void SetDeepThinking(bool enabled) = 0;
    virtual void SetDeepResearch(bool enabled) = 0;
    virtual bool IsMaxMode() const = 0;
    virtual bool IsDeepThinking() const = 0;
    virtual bool IsDeepResearch() const = 0;

    // ---- Memory Management ----
    virtual size_t GetMemoryUsage() const = 0;
    virtual void ClearCache() = 0;

    // ---- Engine Identification ----
    virtual const char* GetEngineName() const = 0;
};

} // namespace RawrXD
