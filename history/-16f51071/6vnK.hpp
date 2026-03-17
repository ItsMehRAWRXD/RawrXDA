#pragma once

#include <QObject>
#include <QString>
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <random>

// Forward declarations
class IGGUFLoader; // Changed from GGUFLoader
class VulkanCompute;
class TransformerBlockScalar;

/**
 * @brief Real GGUF inference engine with Vulkan GPU acceleration
 * Provides tokenization, model loading, and token generation
 */
class InferenceEngine : public QObject {
    Q_OBJECT
public:
    InferenceEngine(QObject* parent = nullptr);
    ~InferenceEngine();

    // Model initialization with REAL GGUF loading
    bool Initialize(const std::string& model_path);
    
    // Model lifecycle
    bool isModelLoaded() const;
    std::string modelPath() const;
    void Cleanup();

    // Real tokenization from GGUF model vocabulary
    std::vector<int32_t> tokenize(const QString& text);
    
    // Real autoregressive token generation with sampler
    std::vector<int32_t> generate(const std::vector<int32_t>& prompts, int maxTokens);
    
    // Real detokenization from model vocabulary
    QString detokenize(const std::vector<int32_t>& tokens);

    // Standard interface
    std::string GenerateToken(const std::string& prompt, uint32_t max_tokens = 1);
    
    // Hotpatching functionality
    bool HotPatchModel(const std::string& model_path);
    
public slots:
    void processCommand(const QString& command);
    QString processChat(const QString& message);
    QString analyzeCode(const QString& code);

private:
    // Real inference pipeline methods
    bool InitializeVulkan();
    bool LoadModelFromGGUF(const std::string& model_path);
    bool UploadTensorsToGPU();
    std::vector<float> EmbedTokens(const std::vector<int32_t>& token_ids);
    std::vector<float> RunForwardPass(const std::vector<float>& input_embedding);
    int32_t SampleNextToken(const std::vector<float>& logits);
    
    // Production transformer inference
    bool LoadTransformerWeights();
    std::vector<float> ApplyOutputProjection(const std::vector<float>& hidden_states);
    bool RunSelfTest();
    bool ValidateModelArchitecture();
    
    // State
    std::unique_ptr<IGGUFLoader> m_loader; // Changed from GGUFLoader to allow StreamingGGUFLoader
    std::unique_ptr<TransformerBlockScalar> m_transformer;
    std::unique_ptr<VulkanCompute> m_vulkan;
    
    std::string m_modelPath;
    bool m_initialized = false;
    uint32_t m_vocabSize = 0;
    uint32_t m_embeddingDim = 0;
    uint32_t m_layerCount = 0;
    uint32_t m_headCount = 32;
    uint32_t m_headDim = 128;
    bool m_useGPU = false;
    
    std::vector<float> m_embeddingTable;
    std::vector<float> m_outputWeights;
    
    // Pre-initialized RNG (avoid repeated initialization overhead - Bottleneck #13 fix)
    static std::mt19937 m_rng;
    static std::uniform_real_distribution<float> m_embedding_dist;
};
