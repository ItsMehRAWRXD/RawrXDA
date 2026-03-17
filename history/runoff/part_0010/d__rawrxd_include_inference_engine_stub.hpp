#pragma once

#include <QObject>
#include <QString>
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <random>

// Forward declarations
class IGGUFLoader;
class VulkanCompute;
class TransformerBlockScalar;

namespace RawrXD {
    class LazyTensorPager;
}

/**
 * @brief Inference mode for model loading strategy
 */
enum class InferenceMode {
    STANDARD,        // Full model loaded into RAM
    METADATA_ONLY,   // Only metadata parsed (for inspection)
    LAZY_PAGED       // Layer-wise demand paging for 800B+ models
};

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
    
    // Inference mode accessor
    InferenceMode inferenceMode() const { return m_mode; }

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
    
    // Lazy pager integration
    bool InitializeLazyPager(const std::string& model_path, uint64_t file_size);
    
    // State
    std::unique_ptr<IGGUFLoader> m_loader;
    std::unique_ptr<TransformerBlockScalar> m_transformer;
    std::unique_ptr<RawrXD::LazyTensorPager> m_pager;  // Lazy tensor pager for 800B+ models
    // Note: Vulkan GPU support deferred - CPU inference is functional
    
    InferenceMode m_mode = InferenceMode::STANDARD;
    std::string m_modelPath;
    bool m_initialized = false;
    uint32_t m_vocabSize = 0;
    uint32_t m_embeddingDim = 0;
    uint32_t m_layerCount = 0;
    uint32_t m_headCount = 32;
    uint32_t m_headDim = 128;
    
    std::vector<float> m_embeddingTable;
    std::vector<float> m_outputWeights;
    
    // Pre-initialized RNG (avoid repeated initialization overhead - Bottleneck #13 fix)
    static std::mt19937 m_rng;
    static std::uniform_real_distribution<float> m_embedding_dist;
};
