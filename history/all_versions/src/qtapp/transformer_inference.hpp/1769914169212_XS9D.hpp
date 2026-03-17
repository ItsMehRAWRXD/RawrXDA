#pragma once


#include <vector>
#include <cstdint>

// Forward declarations for ggml
struct ggml_context;
struct ggml_tensor;
struct ggml_cgraph;

// Forward declaration to avoid circular dependency
class InferenceEngine;

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
    friend class InferenceEngine; // Allow InferenceEngine to configure internal state
public:
    TransformerInference();
    ~TransformerInference();
    
    /**
     * @brief Load model weights from std::vector<uint8_t> hash (legacy overload)
     * @param tensorCache Map of tensor names to quantized data
     * @param nLayers Number of transformer layers
     * @param nEmbd Embedding dimension
     * @param nHead Number of attention heads
     * @param nVocab Vocabulary size
     * @return true if loaded successfully
     */
    bool loadWeights(const std::unordered_map<std::string, std::vector<uint8_t>>& tensorCache,
                     int nLayers, int nEmbd, int nHead, int nVocab);
    
    /**
     * @brief Load model weights with explicit quantization type information
     * @param tensorCacheWithTypes Map of tensor names to (data, type_id) pairs
     *        Each pair contains the quantized data and its GGML type ID
     * @param nLayers Number of transformer layers
     * @param nEmbd Embedding dimension
     * @param nHead Number of attention heads
     * @param nVocab Vocabulary size
     * @return true if loaded successfully
     * 
     * This overload is preferred for models with mixed quantization types,
     * as it preserves the actual quantization type of each tensor instead of
     * assuming all tensors are F32.
     */
    bool loadWeightsWithTypes(const std::unordered_map<std::string, std::pair<std::vector<uint8_t>, int>>& tensorCacheWithTypes,
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
    
    /**
     * @brief Mark transformer as ready when using GGUF direct inference
     * This is called when the model uses GGUF loader directly instead of
     * loading weights into the transformer's own tensors.
     */
    void markReadyForGGUFInference() { 
        m_ready = true; 
        m_ggufDirectMode = true;
    }
    
    /**
     * @brief Initialize the Native Titan Engine (ASM Backend)
     * @param modelPath Path to the GGUF model file
     * @return true if initialized successfully
     */
    bool initTitanBackend(const std::string& modelPath);
    
private:
    // Model hyperparameters
    int m_nLayers{0};
    int m_nEmbd{0};
    int m_nHead{0};
    int m_nVocab{0};
    int m_ctxSize{2048};  // Context window
    
    // GGUF direct mode flag (for models loaded via GGUF loader without transformer weight loading)
    bool m_ggufDirectMode{false};
    
    // Legacy/Mixed Mode Tensor Data
    std::unordered_map<std::string, std::vector<uint8_t>> m_weights; // Plain F32 cache
    
    // Titan Engine (Native ASM) Backend
    void* m_titanContext{nullptr};
    void* m_titanDll{nullptr};
    std::string m_modelPath;
    
    // Function Pointers for Titan DLL
    typedef int (*PFN_Titan_Initialize)(void** ppContext);
    typedef int (*PFN_Titan_LoadModel)(void* pContext, const char* path);
    typedef int (*PFN_Titan_RunInferenceStep)(void* pContext, int32_t* token, int32_t* out_len); // Updated to match ASM
    typedef int (*PFN_Titan_Shutdown)(void* pContext);
    
    PFN_Titan_Initialize m_pfnTitanInit{nullptr};
    PFN_Titan_LoadModel m_pfnTitanLoad{nullptr};
    PFN_Titan_RunInferenceStep m_pfnTitanRunInferenceStep{nullptr}; // Fixed name to match CPP
    PFN_Titan_Shutdown m_pfnTitanShutdown{nullptr};

    // Internal structures
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
        
        // FIX 4: Add Bias Terms
        ggml_tensor* attn_q_bias{nullptr};
        ggml_tensor* attn_k_bias{nullptr};
        ggml_tensor* attn_v_bias{nullptr};
        ggml_tensor* attn_output_bias{nullptr};
        ggml_tensor* mlp_fc1_bias{nullptr};
        ggml_tensor* mlp_fc2_bias{nullptr};
    };
    std::vector<LayerWeights> m_layers;
    
    // KV cache for efficient generation
    std::vector<ggml_tensor*> m_kCache;
    std::vector<ggml_tensor*> m_vCache;
    
    bool m_ready{false};
    
    // Helper methods
    ggml_tensor* createTensorFromCache(const std::string& name, 
                                       const std::unordered_map<std::string, std::vector<uint8_t>>& cache,
                                       const int64_t* shape, int nDims);
    
    // New overload that accepts type ID
    ggml_tensor* createTensorFromCache(const std::vector<uint8_t>& data,
                                       int typeId,
                                       const std::vector<int64_t>& dimensions);
    
    ggml_tensor* buildGraph(ggml_context* ctx, const std::vector<int32_t>& tokens);
    int sampleToken(const std::vector<float>& logits, float temperature);
    void initKVCache();
    void freeContext();
};


