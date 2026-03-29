#pragma once
#include "../RawrXD_Interfaces.h"
#include "../vulkan_compute.h"
#include <memory>
#include <vector>
#include <string>

namespace RawrXD {

/**
 * @class VulkanInferenceEngine
 * @brief GPU-accelerated inference using Vulkan compute shaders
 * 
 * Implements the InferenceEngine interface with real Vulkan compute dispatch
 * for tensor operations (GEMM, attention, RoPE, softmax, etc.)
 */
class VulkanInferenceEngine : public InferenceEngine {
public:
    VulkanInferenceEngine();
    ~VulkanInferenceEngine() override;

    // Model lifecycle
    bool LoadModel(const std::string& model_path) override;
    bool IsModelLoaded() const override;

    // Tokenization (delegates to host CPU)
    std::vector<int32_t> Tokenize(const std::string& text) override;
    std::string Detokenize(const std::vector<int32_t>& tokens) override;

    // Inference on GPU
    std::vector<int32_t> Generate(const std::vector<int32_t>& input_tokens, int max_tokens = 100) override;
    std::vector<float> Eval(const std::vector<int32_t>& input_tokens) override;

    // Streaming generation
    void GenerateStreaming(
        const std::vector<int32_t>& input_tokens,
        int max_tokens,
        std::function<void(const std::string&)> token_callback,
        std::function<void()> complete_callback,
        std::function<void(int32_t)> token_id_callback = nullptr) override;

    // Model metadata
    int GetVocabSize() const override;
    int GetEmbeddingDim() const override;
    int GetNumLayers() const override;
    int GetNumHeads() const override;

    // Mode flags
    void SetMaxMode(bool enabled) override;
    void SetDeepThinking(bool enabled) override;
    void SetDeepResearch(bool enabled) override;
    bool IsMaxMode() const override;
    bool IsDeepThinking() const override;
    bool IsDeepResearch() const override;

    // Memory management
    size_t GetMemoryUsage() const override;
    void ClearCache() override;

    // Engine identification
    const char* GetEngineName() const override { return "Vulkan-GPU"; }

private:
    std::unique_ptr<VulkanCompute> vulkan_compute_;
    bool model_loaded_ = false;
    bool max_mode_ = false;
    bool deep_thinking_ = false;
    bool deep_research_ = false;
    int vocab_size_ = 0;
    int embedding_dim_ = 0;
    int num_layers_ = 0;
    int num_heads_ = 0;
};

} // namespace RawrXD
