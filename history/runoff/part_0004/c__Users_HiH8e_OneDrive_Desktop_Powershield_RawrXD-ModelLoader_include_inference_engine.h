#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

class VulkanCompute;
class GGUFLoader;
struct VulkanTensor;

class InferenceEngine {
public:
    InferenceEngine();
    ~InferenceEngine();

    bool Initialize(const std::string& model_path);
    std::string GenerateToken(const std::string& prompt, uint32_t max_tokens = 1);
    void Cleanup();

private:
    bool InitializeVulkan();
    bool LoadModel(const std::string& model_path);
    bool UploadTensors();
    bool RunFirstMatMulTest();
    bool RunGPUTest();
    std::vector<float> Tokenize(const std::string& text);
    std::string Detokenize(const std::vector<uint32_t>& token_ids);
    std::vector<float> RunForwardPass(const std::vector<float>& input_embedding);
    uint32_t SampleToken(const std::vector<float>& logits);

    std::unique_ptr<VulkanCompute> vulkan_;
    std::unique_ptr<GGUFLoader> loader_;
    bool initialized_{false};
    uint32_t vocab_size_{0};
    uint32_t embedding_dim_{0};
    uint32_t layer_count_{0};
};
