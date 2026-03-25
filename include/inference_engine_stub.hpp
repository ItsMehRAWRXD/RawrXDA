<<<<<<< HEAD
#pragma once

// C++20, no Qt. GGUF inference engine stub; QString → std::string.

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <random>
#include <cstddef>

class GGUFLoader;
class VulkanCompute;
class TransformerBlockScalar;

class InferenceEngine
{
public:
    InferenceEngine() = default;
    ~InferenceEngine();

    bool Initialize(const std::string& model_path);
    bool isModelLoaded() const;
    std::string modelPath() const;
    void Cleanup();

    std::vector<int32_t> tokenize(const std::string& text);
    std::vector<int32_t> generate(const std::vector<int32_t>& prompts, int maxTokens);
    std::string detokenize(const std::vector<int32_t>& tokens);

    std::string GenerateToken(const std::string& prompt, uint32_t max_tokens = 1);
    bool HotPatchModel(const std::string& model_path);

    void processCommand(const std::string& command);
    std::string processChat(const std::string& message);
    std::string analyzeCode(const std::string& code);

private:
    struct GpuRuntime;

    bool InitializeVulkan();
    bool LoadModelFromGGUF(const std::string& model_path);
    bool UploadTensorsToGPU();
    std::vector<float> EmbedTokens(const std::vector<int32_t>& token_ids);
    std::vector<float> RunForwardPass(const std::vector<float>& input_embedding);
    int32_t SampleNextToken(const std::vector<float>& logits);
    bool LoadTransformerWeights();
    std::vector<float> ApplyOutputProjection(const std::vector<float>& hidden_states);
    std::vector<float> ApplyOutputProjectionCPU(const float* hidden_last_token);

    std::unique_ptr<GGUFLoader> m_loader;
    std::unique_ptr<TransformerBlockScalar> m_transformer;
    std::unique_ptr<GpuRuntime> m_gpuRuntime;
    std::string m_modelPath;
    bool m_initialized = false;
    uint32_t m_vocabSize = 0;
    uint32_t m_embeddingDim = 0;
    uint32_t m_layerCount = 0;
    uint32_t m_headCount = 32;
    uint32_t m_headDim = 128;
    std::vector<float> m_embeddingTable;
    std::vector<float> m_outputWeights;

    bool m_gpuRequested = false;
    bool m_gpuReady = false;
    bool m_gpuSoftmax = false;
    bool m_gpuParityCheck = false;
    float m_gpuParityTolerance = 1e-2f;
    std::size_t m_gpuMatVecCalls = 0;
    std::size_t m_gpuFallbacks = 0;

    uint32_t m_q4OutputColsAligned = 0;
    std::vector<uint8_t> m_q4OutputPacked;

    static std::mt19937 m_rng;
    static std::uniform_real_distribution<float> m_embedding_dist;
};
=======
#pragma once

// C++20, no Qt. GGUF inference engine (IDE build variant; full implementation linked in benchmark/inference build). QString → std::string.

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <random>

class GGUFLoader;
class VulkanCompute;
class TransformerBlockScalar;

enum class InferenceMode : uint8_t { STANDARD, METADATA_ONLY, LAZY_PAGED };

class InferenceEngine
{
public:
    InferenceEngine() = default;
    explicit InferenceEngine(void*) : InferenceEngine() {}
    ~InferenceEngine();

    bool Initialize(const std::string& model_path);
    bool isModelLoaded() const;
    std::string modelPath() const;
    InferenceMode inferenceMode() const { return InferenceMode::STANDARD; }
    void Cleanup();

    std::vector<int32_t> tokenize(const std::string& text);
    std::vector<int32_t> generate(const std::vector<int32_t>& prompts, int maxTokens);
    std::string detokenize(const std::vector<int32_t>& tokens);

    std::string GenerateToken(const std::string& prompt, uint32_t max_tokens = 1);
    bool HotPatchModel(const std::string& model_path);

    void processCommand(const std::string& command);
    std::string processChat(const std::string& message);
    std::string analyzeCode(const std::string& code);

private:
    bool InitializeVulkan();
    bool LoadModelFromGGUF(const std::string& model_path);
    bool UploadTensorsToGPU();
    std::vector<float> EmbedTokens(const std::vector<int32_t>& token_ids);
    std::vector<float> RunForwardPass(const std::vector<float>& input_embedding);
    int32_t SampleNextToken(const std::vector<float>& logits);
    bool LoadTransformerWeights();
    std::vector<float> ApplyOutputProjection(const std::vector<float>& hidden_states);

    std::unique_ptr<GGUFLoader> m_loader;
    std::unique_ptr<TransformerBlockScalar> m_transformer;
    std::string m_modelPath;
    bool m_initialized = false;
    uint32_t m_vocabSize = 0;
    uint32_t m_embeddingDim = 0;
    uint32_t m_layerCount = 0;
    uint32_t m_headCount = 32;
    uint32_t m_headDim = 128;
    std::vector<float> m_embeddingTable;
    std::vector<float> m_outputWeights;
    static std::mt19937 m_rng;
    static std::uniform_real_distribution<float> m_embedding_dist;
};
>>>>>>> origin/main
