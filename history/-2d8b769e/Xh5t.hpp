#pragma once
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QMutex>
#include <QHash>
#include <QElapsedTimer>
#include <vector>
#include <cstdint>
#include <random>
#include "gguf_loader.hpp"
#include "transformer_inference.hpp"
#include "bpe_tokenizer.hpp"
#include "sentencepiece_tokenizer.hpp"
#include "vocabulary_loader.hpp"/**
 * @brief Inference engine for GGUF models with brutal_gzip compression support
 * 
 * This class runs in a worker thread and handles model loading, tensor decompression,
 * and inference requests. It integrates with the existing brutal_gzip MASM/NEON
 * deflate implementation for fast compression/decompression.
 */
class InferenceEngine : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Construct an inference engine
     * @param ggufPath Path to the GGUF model file (can be empty, loaded later)
     * @param parent QObject parent
     */
    explicit InferenceEngine(const QString& ggufPath = QString(), QObject* parent = nullptr);
    
    /**
     * @brief Load a GGUF model file
     * @param path Path to the GGUF file
     * @return true if loaded successfully
     *
     * This is declared Q_INVOKABLE so it can be called via
     * QMetaObject::invokeMethod(..., Qt::QueuedConnection)
     */
    Q_INVOKABLE bool loadModel(const QString& path);
    
    /**
     * @brief Check if a model is currently loaded
     */
    bool isModelLoaded() const;
    
    /**
     * @brief Get the current model path
     */
    QString modelPath() const;
    
    /**
     * @brief Get list of tensor names from the loaded model
     */
    QStringList tensorNames() const;
    
    /**
     * @brief Get memory usage in MB
     */
    qint64 memoryUsageMB() const;
    
    /**
     * @brief Get tokens per second performance metric
     */
    double tokensPerSecond() const;
    
    /**
     * @brief Get current temperature setting
     */
    double temperature() const;
    
    /**
     * @brief Generate tokens synchronously (for server API)
     * @param inputTokens Input token sequence
     * @param maxTokens Maximum number of tokens to generate
     * @return Generated token sequence
     */
    std::vector<int32_t> generate(const std::vector<int32_t>& inputTokens, int maxTokens = 100);
    
    /**
     * @brief Tokenize text (public for server API)
     */
    std::vector<int32_t> tokenize(const QString& text);
    
    /**
     * @brief Detokenize tokens to text (public for server API)
     */
    QString detokenize(const std::vector<int32_t>& tokens);

public slots:
    /**
     * @brief Process an inference request
     * @param prompt User input text
     * @param reqId Request ID for correlation
     */
    void request(const QString& prompt, qint64 reqId);
    
    /**
     * @brief Unload the current model
     */
    void unloadModel();
    
    /**
     * @brief Change quantization mode at runtime
     * @param mode Quantization type: Q4_0, Q4_1, Q5_0, Q5_1, Q6_K, Q8_K, F16, F32
     */
    void setQuantMode(const QString& mode);
    
    /**
     * @brief Set quantization for a specific tensor layer
     * @param tensorName Name of the tensor
     * @param quant Quantization type for this layer
     */
    void setLayerQuant(const QString& tensorName, const QString& quant);

signals:
    /**
     * @brief Emitted when inference completes successfully
     * @param reqId Request ID
     * @param answer Generated response
     */
    void resultReady(qint64 reqId, const QString& answer);
    
    /**
     * @brief Emitted when an error occurs
     * @param reqId Request ID
     * @param errorMsg Error description
     */
    void error(qint64 reqId, const QString& errorMsg);
    
    /**
     * @brief Emitted when model loading status changes
     * @param loaded true if model is loaded
     * @param modelName Name of the loaded model
     */
    void modelLoadedChanged(bool loaded, const QString& modelName);
    
    /**
     * @brief Emitted for each token during streaming inference
     * @param reqId Request ID
     * @param token Single token string
     */
    void streamToken(qint64 reqId, const QString& token);
    
    /**
     * @brief Emitted when streaming inference completes
     * @param reqId Request ID
     */
    void streamFinished(qint64 reqId);
    
    /**
     * @brief Emitted when quantization mode changes
     * @param mode New quantization mode
     */
    void quantChanged(const QString& mode);
    
    /**
     * @brief Emitted when inference completes (alias for resultReady)
     * @param requestId Request ID
     * @param result Generated text
     */
    void inferenceComplete(const QString& requestId, const QString& result);
    
    /**
     * @brief Emitted when inference error occurs (alias for error)
     * @param requestId Request ID  
     * @param errorMessage Error description
     */
    void inferenceError(const QString& requestId, const QString& errorMessage);

private:
    QString m_modelPath;
    GGUFLoader* m_loader;
    mutable QMutex m_mutex;
    QString m_quantMode{"Q4_0"};  // Default quantization
    QHash<QString, QString> m_perLayerQuant;  // Tensor-specific quants
    QHash<QString, QByteArray> m_tensorCache;  // Cached quantized tensors
    
    // Performance tracking
    qint64 m_memoryUsageMB{0};
    double m_tokensPerSecond{0.0};
    double m_temperature{0.8};
    QElapsedTimer m_inferenceTimer;
    
    // Transformer inference
    TransformerInference m_transformer;
    
    // Tokenizers (auto-detect which to use)
    BPETokenizer m_bpeTokenizer;
    SentencePieceTokenizer m_spTokenizer;
    VocabularyLoader m_vocab;
    enum TokenizerMode {
        TOKENIZER_FALLBACK,  // Simple word-based fallback
        TOKENIZER_BPE,       // BPE (GPT-2/GPT-3 style)
        TOKENIZER_SP         // SentencePiece (LLaMA/Mistral)
    } m_tokenizerMode{TOKENIZER_FALLBACK};
    
    // Helper methods
    QString extractModelName(const QString& path) const;
    void rebuildTensorCache();
    void initializeTokenizer();
    
    // Advanced sampling
    int32_t sampleNextToken(std::vector<float>& logits, double temperature, double topP);
    float getRandomFloat(float min, float max);
    
    // Sampling configuration
    double m_topP{0.9};  // Top-P (nucleus) sampling threshold
    std::mt19937 m_randomEngine;  // Thread-safe random number generator
    bool m_kvCacheReady{false};  // Track if KV-cache is prefilled
};
