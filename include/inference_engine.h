#pragma once
#include <QObject>
#include <QString>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

class VulkanCompute;
class GGUFLoader;
class TransformerBlockScalar;
struct VulkanTensor;

class InferenceEngine : public QObject {
    Q_OBJECT
public:
    InferenceEngine(QObject* parent = nullptr);
    ~InferenceEngine();

    bool Initialize(const std::string& model_path);
    std::string GenerateToken(const std::string& prompt, uint32_t max_tokens = 1);
    void Cleanup();
    bool HotPatchModel(const std::string& model_path);
    
    // Enhanced tokenization for training
    std::vector<uint32_t> TokenizeText(const std::string& text);
    std::string DetokenizeIds(const std::vector<uint32_t>& token_ids);
    uint32_t GetVocabSize() const { return vocab_size_; }
    uint32_t GetEmbeddingDim() const { return embedding_dim_; }
    
    // Model information
    bool IsModelLoaded() const { return initialized_; }
    QString GetCurrentModelPath() const { return QString::fromStdString(current_model_path_); }
    
public slots:
    void processCommand(const QString& command);
    QString processChat(const QString& message);
    QString analyzeCode(const QString& code);

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
    
    // Production transformer inference
    bool LoadTransformerWeights();
    std::vector<float> RunTransformerForward(const std::vector<float>& embeddings, uint32_t seqLen);
    std::vector<float> ApplyOutputProjection(const std::vector<float>& hidden_states);

    std::unique_ptr<VulkanCompute> vulkan_;
    std::unique_ptr<GGUFLoader> loader_;
    std::unique_ptr<TransformerBlockScalar> transformer_;
    bool initialized_{false};
    uint32_t vocab_size_{0};
    uint32_t embedding_dim_{0};
    uint32_t layer_count_{0};
    uint32_t head_count_{8};  // Typical multi-head attention
    uint32_t head_dim_{64};   // embedding_dim / head_count
    std::vector<float> embedding_table_;  // Token embeddings
    std::vector<float> output_weights_;   // Final projection to vocab
    std::string current_model_path_;      // Track current model path
};
