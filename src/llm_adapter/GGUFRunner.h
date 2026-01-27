#pragma once
#include <QHash>

#include <QObject>
#include <QString>

#include <QVector>
#include <QtGlobal>

#include <cstddef>
#include <vector>

#include "QuantBackend.h"  // Quantization backend switcher

// GGML tensor types
enum class GgmlType : quint32 {
    F32  = 0,
    F16  = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    Q8_1 = 9
};

// Q4_0 block: 32 weights in 16 bytes + 1 float16 delta
struct BlockQ4_0 {
    quint16 d;      // delta (float16)
    quint8 qs[16];  // 32 nibbles (2 per byte)
};

// Q8_0 block: 32 weights in 32 bytes + 1 float16 delta
struct BlockQ8_0 {
    quint16 d;      // delta (float16)
    qint8 qs[32];   // 32 signed bytes
};

/**
 * @brief GGUFRunner manages the high-performance execution of GGUF language models.
 */
class GGUFRunner : public QObject {
    Q_OBJECT

public:
    explicit GGUFRunner(QObject* parent = nullptr);
    ~GGUFRunner();

    /**
     * @brief Executes a full inference pass using the raw text prompt.
     * @param prompt Raw UTF-8 prompt that will be tokenized and embedded internally.
     * @param outputBuffer Buffer that will receive the logits (size must match vocab).
     */
    bool runInference(const QString& prompt, float* outputBuffer);
    
    // Model loading
    bool loadModel(const QString& filePath);
    
    // Generation parameter setters
    void setMaxTokens(int max) { context_.maxTokens = max; }
    void setTemperature(float temp) { context_.temperature = qMax(0.0f, temp); }
    void setTopP(float p) { context_.topP = qBound(0.0f, p, 1.0f); }
    void setRepeatPenalty(float penalty) { context_.repeatPenalty = qMax(1.0f, penalty); }
    
    // Quantization control
    bool setQuantizationMode(QuantMode mode);
    QuantMode currentQuantMode() const;
    float getCompressionRatio() const;
    
    // Model info getters
    QString modelPath() const { return context_.modelPath; }
    QString modelName() const { return context_.modelName; }
    QString architecture() const { return context_.architecture; }
    qsizetype vocabularySize() const { return context_.vocabSize; }
    qsizetype embeddingDim() const { return context_.embedDim; }
    bool isLoaded() const { return context_.mappedData != nullptr; }

    /**
     * @brief Compresses a raw buffer using the "Brutal" stored-block algorithm.
     *        This is extremely fast (0.2ms/MB) but offers no compression ratio.
     *        Useful for wrapping data in gzip format for compatibility without CPU cost.
     * @param data Pointer to raw data
     * @param len Length of data
     * @return QByteArray containing the gzip stream
     */
    static QByteArray compressBrutal(const void* data, size_t len);

signals:
    void tokenChunkGenerated(const QString& chunk);
    void inferenceComplete(bool success);
    void modelLoaded(const QString& path, qint64 sizeBytes);
    void loadingProgress(int percent);

private:
    enum class QuantType {
        F32,      // Full precision (13 GB for 7B)
        F16,      // Half precision (6.5 GB)
        Q4_0,     // 4-bit quantization (3.5 GB) - llama.cpp standard
        Q4_1,     // 4-bit with min/max (slightly better quality)
        Q5_0,     // 5-bit quantization (4.3 GB)
        Q5_1,     // 5-bit with min/max
        Q8_0      // 8-bit quantization (6.7 GB)
    };

    struct ModelContext {
        // Hardware features
        bool hasAVX2{false};
        bool hasAVX512{false};
        bool hasFMA{false};
        
        // Memory management
        float* mappedData{nullptr};
        bool usesMmap{false};
        qsizetype embedDim{0};
        qsizetype vocabSize{0};
        qsizetype nLayers{0};
        qsizetype nHeads{0};
        qsizetype nKVHeads{0};
        qsizetype headDim{0};       // embedDim / nHeads
        float ropeBase{10000.0f};   // RoPE frequency base
        std::vector<float> invFreq; // Precomputed inverse frequencies for RoPE [headDim/2]
        qint64 modelFileSize{0};
        
        // Inference state
        std::vector<float> logits;
        QVector<QString> vocabulary;
        QString modelPath;
        
        // Generation parameters
        int maxTokens{64};
        int eosTokenId{-1};
        float temperature{0.8f};     // 0.0 = greedy, 1.0 = creative, 2.0 = chaos
        float topP{0.95f};           // nucleus sampling threshold
        float repeatPenalty{1.1f};   // penalize token repetition
        
        // Quantization
        QuantType quantType{QuantType::F32};
        
        // GGUF metadata
        uint32_t ggufVersion{0};
        QString modelName;
        QString architecture;

        // GGUF tensors (essential weights)
        std::vector<float> tok_embeddings;           // [vocabSize, embedDim]
        std::vector<float> output_norm_w;            // output norm weight
        std::vector<float> output_w;                 // output projection weight
        std::vector<uint8_t> raw_q4_output;          // raw Q4_0 bytes for output.weight (optional)
        std::vector<int8_t> raw_q8_output;           // raw Q8_0 bytes for output.weight (optional)
        std::vector<float> ln_f_g;                   // final layernorm gamma [embedDim]
        std::vector<float> ln_f_b;                   // final layernorm beta [embedDim]

        struct Layer {
            // Attention projections: [embedDim, embedDim]
            std::vector<float> attn_q_w;
            std::vector<float> attn_k_w;
            std::vector<float> attn_v_w;
            std::vector<float> attn_o_w;
            // LayerNorm params
            std::vector<float> ln_1_g;
            std::vector<float> ln_1_b;
            std::vector<float> ln_2_g;
            std::vector<float> ln_2_b;
            // MLP (SwiGLU): up, gate, down: [embedDim, 4*embedDim] and [4*embedDim, embedDim]
            std::vector<float> mlp_up_w;
            std::vector<float> mlp_gate_w;
            std::vector<float> mlp_down_w;
        };
        std::vector<Layer> layers;

        // KV-cache: per layer K/V for past tokens (multi-head GQA)
        std::vector<float> keyCache;   // [nLayers, nKVHeads, maxTokens, headDim]
        std::vector<float> valueCache; // [nLayers, nKVHeads, maxTokens, headDim]
        size_t kvLen{0};

        // Tensor directory
        struct TensorDesc { QString name; std::vector<uint32_t> dims; GgmlType type; uint64_t offset; };
        QHash<QString, TensorDesc> tensorTable;
    };

    void checkCpuFeatures();
    void loadGGUFModel(const QString& filePath);
    void loadVocabulary(const QString& vocabPath);
    float* getLayerWeights();
    bool prepareLLMInput(const QString& prompt, std::vector<float>& embeddings);
    void applySoftmax(float* buffer);
    void applyTemperature(float* buffer, float temperature);
    size_t sampleNextToken(float* buffer);
    size_t sampleTopP(float* buffer, float topP);
    size_t sampleGreedy(float* buffer);
    QString decodeToken(size_t tokenId) const;
    void fallback_matrix_multiply(float* A, float* B, float* C, int N, int M, int K);
    void detectExtendedCpuFeatures();

    // GGUF tensor parsing
    bool parseGgufTensors(class QFile& file);
    bool parseGgufTensorTable(class QFile& file);
    bool readTensorFloat32(class QFile& file, qint64 offset, qint64 count, std::vector<float>& out);
    bool loadTensor(class QFile& file, const QString& name, std::vector<float>& weights);
    size_t ggmlTypeSize(GgmlType type);
    QByteArray readTensorData(class QFile& file, quint64 offset, quint64 numBytes);

    // Transformer forward (scalar)
private:
    void layerNorm(const float* x, float* y, const std::vector<float>& gamma, const std::vector<float>& beta, qsizetype dim);
    void matmul(const float* A, const float* B, float* C, int N, int M, int K);
    void attentionForward(int layerIdx, const float* x, float* y);
    void mlpForward(int layerIdx, const float* x, float* y);

    ModelContext context_;
};
