#pragma once
#include <QString>
#include <QByteArray>
#include <QHash>
#include <vector>
#include <cstdint>

// Forward declarations for ggml
struct ggml_context;
struct ggml_tensor;
struct ggml_cgraph;

/**
 * @brief Lightweight transformer inference using ggml backend
 * 
 * Implements basic GPT-style autoregressive generation with:
 * - Token embedding lookup
 * - Multi-head self-attention
 * - Feed-forward MLP layers
 * - Layer normalization
 * - RoPE positional encoding
 */
class TransformerInference {
public:
    TransformerInference();
    ~TransformerInference();
    
    /**
     * @brief Load model weights from quantized tensor cache
     * @param tensorCache Map of tensor names to quantized data
     * @param nLayers Number of transformer layers
     * @param nEmbd Embedding dimension
     * @param nHead Number of attention heads
     * @param nVocab Vocabulary size
     * @return true if loaded successfully
     */
    bool loadWeights(const QHash<QString, QByteArray>& tensorCache,
                     int nLayers, int nEmbd, int nHead, int nVocab);
    
    /**
     * @brief Generate tokens autoregressively
     * @param prompt Input token IDs
     * @param maxTokens Maximum tokens to generate
     * @param temperature Sampling temperature (0.0 = greedy)
     * @return Generated token IDs
     */
    std::vector<int32_t> generate(const std::vector<int32_t>& prompt,
                                   int maxTokens, float temperature = 0.7f);
    
    /**
     * @brief Run a single forward pass
     * @param tokens Input token sequence
     * @return Logits for next token prediction [vocab_size]
     */
    std::vector<float> forward(const std::vector<int32_t>& tokens);
    
    /**
     * @brief Check if model is loaded and ready
     */
    bool isReady() const { return m_ready; }
    
private:
    // Model hyperparameters
    int m_nLayers{0};
    int m_nEmbd{0};
    int m_nHead{0};
    int m_nVocab{0};
    int m_ctxSize{2048};  // Context window
    
    // ggml computation context
    ggml_context* m_ctx{nullptr};
    ggml_context* m_kvCtx{nullptr};  // KV cache context
    
    // Model weights as ggml tensors
    ggml_tensor* m_tokenEmbed{nullptr};
    ggml_tensor* m_outputWeight{nullptr};
    
    struct LayerWeights {
        ggml_tensor* attn_q{nullptr};
        ggml_tensor* attn_k{nullptr};
        ggml_tensor* attn_v{nullptr};
        ggml_tensor* attn_proj{nullptr};
        ggml_tensor* ln1_weight{nullptr};
        ggml_tensor* ln1_bias{nullptr};
        ggml_tensor* mlp_fc1{nullptr};
        ggml_tensor* mlp_fc2{nullptr};
        ggml_tensor* ln2_weight{nullptr};
        ggml_tensor* ln2_bias{nullptr};
    };
    std::vector<LayerWeights> m_layers;
    
    // KV cache for efficient generation
    std::vector<ggml_tensor*> m_kCache;
    std::vector<ggml_tensor*> m_vCache;
    
    bool m_ready{false};
    
    // Helper methods
    ggml_tensor* createTensorFromCache(const QString& name, 
                                       const QHash<QString, QByteArray>& cache,
                                       const int64_t* shape, int nDims);
    ggml_tensor* buildGraph(ggml_context* ctx, const std::vector<int32_t>& tokens);
    int sampleToken(const std::vector<float>& logits, float temperature);
    void initKVCache();
    void freeContext();
};
