#pragma once
#include "RawrXD_Interfaces.h"
#include "ai/ai_inference_real.h"
#include "ai/speculative_decoder.h"
#include <memory>
#include <string>
#include <vector>

namespace RawrXD {

/**
 * @brief Production Inference Engine Implementation
 * Bridges the abstract InferenceEngine interface to the real GGML-based backend
 * with integrated Speculative Decoding and MASM-accelerated memory management.
 */
class ProductionInferenceEngine : public RawrXD::InferenceEngine {
public:
    ProductionInferenceEngine();
    virtual ~ProductionInferenceEngine() override;

    // ---- Model Lifecycle ----
    virtual bool LoadModel(const std::string& model_path) override;
    virtual bool IsModelLoaded() const override;

    // ---- Tokenization ----
    virtual std::vector<int32_t> Tokenize(const std::string& text) override;
    virtual std::string Detokenize(const std::vector<int32_t>& tokens) override;

    // ---- Inference ----
    virtual std::vector<int32_t> Generate(const std::vector<int32_t>& input_tokens, int max_tokens = 100) override;
    virtual std::vector<float> Eval(const std::vector<int32_t>& input_tokens) override;

    // ---- Streaming ----
    virtual void GenerateStreaming(
        const std::vector<int32_t>& input_tokens,
        int max_tokens,
        std::function<void(const std::string&)> token_callback,
        std::function<void()> complete_callback,
        std::function<void(int32_t)> token_id_callback = nullptr) override;

    // ---- Model Info ----
    virtual int GetVocabSize() const override;
    virtual int GetEmbeddingDim() const override;
    virtual int GetNumLayers() const override;
    virtual int GetNumHeads() const override;

    // ---- AI Mode Flags ----
    virtual void SetMaxMode(bool enabled) override;
    virtual void SetDeepThinking(bool enabled) override;
    virtual void SetDeepResearch(bool enabled) override;
    virtual bool IsMaxMode() const override;
    virtual bool IsDeepThinking() const override;
    virtual bool IsDeepResearch() const override;

    // ---- Memory Management ----
    virtual size_t GetMemoryUsage() const override;
    virtual void ClearCache() override;

    // ---- Engine Identification ----
    virtual const char* GetEngineName() const override;

private:
    bool m_modelLoaded = false;
    std::string m_modelPath;
    size_t m_contextLimit = 4096;
    int m_threadCount = 4;

    bool m_maxMode = false;
    bool m_deepThinking = false;
    bool m_deepResearch = false;
};

} // namespace RawrXD
