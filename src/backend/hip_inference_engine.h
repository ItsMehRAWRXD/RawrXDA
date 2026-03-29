#pragma once
#include "../RawrXD_Interfaces.h"
#include <memory>
#include <vector>
#include <string>

namespace RawrXD {

/**
 * @class HIPInferenceEngine
 * @brief AMD GPU-accelerated inference using HIP (Heterogeneous-compute Interface for Portability)
 * 
 * Implements the InferenceEngine interface with real HIP compute dispatch
 * for AMD GPUs (RDNA, CDNA architectures)
 */
class HIPInferenceEngine : public InferenceEngine {
public:
    HIPInferenceEngine();
    ~HIPInferenceEngine() override;

    bool LoadModel(const std::string& model_path) override;
    bool IsModelLoaded() const override;

    std::vector<int32_t> Tokenize(const std::string& text) override;
    std::string Detokenize(const std::vector<int32_t>& tokens) override;

    std::vector<int32_t> Generate(const std::vector<int32_t>& input_tokens, int max_tokens = 100) override;
    std::vector<float> Eval(const std::vector<int32_t>& input_tokens) override;

    void GenerateStreaming(
        const std::vector<int32_t>& input_tokens,
        int max_tokens,
        std::function<void(const std::string&)> token_callback,
        std::function<void()> complete_callback,
        std::function<void(int32_t)> token_id_callback = nullptr) override;

    int GetVocabSize() const override;
    int GetEmbeddingDim() const override;
    int GetNumLayers() const override;
    int GetNumHeads() const override;

    void SetMaxMode(bool enabled) override;
    void SetDeepThinking(bool enabled) override;
    void SetDeepResearch(bool enabled) override;
    bool IsMaxMode() const override;
    bool IsDeepThinking() const override;
    bool IsDeepResearch() const override;

    size_t GetMemoryUsage() const override;
    void ClearCache() override;

    const char* GetEngineName() const override { return "HIP-AMD-GPU"; }

private:
    bool model_loaded_ = false;
    bool max_mode_ = false;
    bool deep_thinking_ = false;
    bool deep_research_ = false;
    int vocab_size_ = 0;
    int embedding_dim_ = 0;
    int num_layers_ = 0;
    int num_heads_ = 0;
    void* hip_device_ = nullptr;  // HIP device handle
};

} // namespace RawrXD
