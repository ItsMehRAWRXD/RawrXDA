#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <functional>
#include "gguf_loader.h"

namespace RawrXD {
    class IMemoryPlugin;
}

namespace RawrXD {

// Tensor data types
enum class TensorType {
    F32 = 0,    // 32-bit float
    F16 = 1,    // 16-bit float
    Q4_0 = 2,   // 4-bit quantization
    Q8_0 = 7,   // 8-bit quantization
    // Add more quantization types as needed
};

// Tensor structure
struct Tensor {
    std::vector<int64_t> shape;
    std::vector<uint8_t> data;
    TensorType type;
    size_t element_size;
    
    Tensor() : type(TensorType::F32), element_size(4) {}
    Tensor(const std::vector<int64_t>& shape, TensorType type) 
        : shape(shape), type(type), element_size(GetElementSize(type)) {}
    
    static size_t GetElementSize(TensorType type) {
        switch(type) {
            case TensorType::F32: return 4;
            case TensorType::F16: return 2;
            case TensorType::Q4_0: return 1; // Approximate
            case TensorType::Q8_0: return 1; // Approximate
            default: return 4;
        }
    }
    
    size_t GetTotalSize() const {
        size_t total_elements = 1;
        for (auto dim : shape) {
            total_elements *= dim;
        }
        return total_elements * element_size;
    }
};

// CPU inference engine class
class CPUInferenceEngine {
public:
    CPUInferenceEngine();
    ~CPUInferenceEngine();
    
    // Context Management
    void SetContextLimit(size_t limit);
    size_t GetContextLimit() const { return m_contextLimit; }
    void RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin> plugin);

    // Model loading
    bool LoadModel(const std::string& model_path);
    bool LoadWeights(const std::unordered_map<std::string, Tensor>& tensors);
    bool IsModelLoaded() const { return m_modelLoaded; }
    
    // Inference
    std::vector<float> Generate(const std::vector<int32_t>& input_tokens, int max_tokens = 100);
    // Explicit declarations for missing methods
    std::vector<float> Eval(const std::vector<int32_t>& input_tokens);
    void UpdateWeights(const std::vector<std::vector<float>>& layer_gradients, float learning_rate);
    void UpdateOutputWeights(const std::vector<float>& gradients, float learningRate);
    
    void GenerateStreaming(const std::vector<int32_t>& input_tokens,
                         int max_tokens,
                         std::function<void(const std::string&)> token_callback,
                         std::function<void()> complete_callback,
                         std::function<void(int32_t)> token_id_callback = nullptr);
    
    // Tokenization
    std::vector<int32_t> Tokenize(const std::string& text);
    std::string Detokenize(const std::vector<int32_t>& tokens);
    
    // Model information
    int GetVocabSize() const { return m_vocabSize; }
    int GetEmbeddingDim() const { return m_embeddingDim; }
    int GetNumLayers() const { return m_numLayers; }
    int GetNumHeads() const { return m_numHeads; }
    
    // AI Mode settings
    void SetMaxMode(bool enabled) { m_maxMode = enabled; }
    void SetDeepThinking(bool enabled) { m_deepThinking = enabled; }
    void SetDeepResearch(bool enabled) { m_deepResearch = enabled; }
    
    bool IsMaxMode() const { return m_maxMode; }
    bool IsDeepThinking() const { return m_deepThinking; }
    bool IsDeepResearch() const { return m_deepResearch; }

    // Performance settings
    void SetThreadCount(int threads) { m_threadCount = threads; }
    int GetThreadCount() const { return m_threadCount; }

    // Memory scaling for context
    void SetContextSize(size_t size);
    size_t GetContextSize() const { return m_contextLimit; }

    // Memory management
    size_t GetMemoryUsage() const;
    void ClearCache();
    
private:
    size_t m_contextLimit = 4096;
    std::vector<std::shared_ptr<RawrXD::IMemoryPlugin>> m_memoryPlugins;

    // Internal tensor operations
    void MatMul(const float* A, const float* B, float* C, int m, int n, int k);
    void Softmax(float* data, int size);
    void LayerNorm(float* data, int size, float epsilon = 1e-5f);
    void GELU(float* data, int size);
    void RMSNorm(float* data, int size, float epsilon = 1e-5f);
    void RoPE(float* data, int dim, int pos, int rotary_dim);
    
    // Attention mechanism
    void MultiHeadAttention(const float* query, const float* key, const float* value,
                           float* output, int seq_len, int embed_dim, int num_heads, int layer_idx = 0);
    
    // Feed-forward network
    void FeedForward(const float* input, float* output, int dim);
    
    // Transformer layer
    void TransformerLayer(const float* input, float* output, int layer_idx, int seq_len);
    
    // Apply Norm Helper
    void ApplyNorm(const std::string& name, float* data);
    
    // KV Cache Init
    void InitKVCache();
    void DequantizeTensor(const std::vector<uint8_t>& src, float* dst, size_t size, TensorType type);

    // Memory management
    float* AllocateTensor(size_t size);
    void DeallocateTensor(float* ptr);
    
    // Model state
    bool m_modelLoaded = false;
    int m_vocabSize = 0;
    int m_embeddingDim = 0;
    int m_numLayers = 0;
    int m_numHeads = 0;
    int m_threadCount = 1;
    
    size_t m_contextSize = 0;
    
    // AI Flag State
    bool m_maxMode = false;
    bool m_deepThinking = false;
    bool m_deepResearch = false;

    // Model weights and Data
    std::unordered_map<std::string, Tensor> m_weights;
    std::vector<std::string> m_vocab;
    
    // Token embeddings
    Tensor m_tokenEmbeddings;
    Tensor m_outputWeights;
    
    // Loader and execution state
    std::unique_ptr<CPUInference::IGGUFLoader> m_loader; // Updated to namespace
    
    // Titan ASM Integration
    void* m_pTitanContext = nullptr;
    using TitanInferenceFunc = void (*)(void*, float*, float*);
    TitanInferenceFunc fnTitan_RunInferenceStep = nullptr;

    // Execution State
    int m_currentPos = 0;
    std::vector<float> m_lastState;

    // Layer weights
    struct LayerWeights {
        Tensor attention_query;
        Tensor attention_key;
        Tensor attention_value;
        Tensor attention_output;
        Tensor layer_norm1;
        Tensor feed_forward1;
        Tensor feed_forward2;
        Tensor layer_norm2;
    };
    std::vector<LayerWeights> m_layers;
    
    // Memory pool
    std::vector<std::unique_ptr<float[]>> m_memoryPool;
    size_t m_totalMemoryAllocated = 0;
    
    // Performance tracking
    mutable size_t m_inferenceCount = 0;
    mutable double m_totalInferenceTime = 0.0;
};

// Utility functions
namespace CPUOps {
    // Optimized matrix multiplication
    void MatMul(const float* A, const float* B, float* C, int m, int n, int k);
    
    // Vector operations
    void VectorAdd(const float* a, const float* b, float* c, int size);
    void VectorMul(const float* a, const float* b, float* c, int size);
    void VectorScale(float* data, float scale, int size);
    
    // Activation functions
    void Softmax(float* data, int size);
    void GELU(float* data, int size);
    void SiLU(float* data, int size);
    
    // Normalization
    void LayerNorm(float* data, int size, float epsilon = 1e-5f);
    void RMSNorm(float* data, int size, float epsilon = 1e-5f);
    
    // Quantization support
    void DequantizeQ4_0(const uint8_t* quantized, float* output, int size);
    void DequantizeQ8_0(const uint8_t* quantized, float* output, int size);
    
    // Performance optimization flags
    void EnableAVX2(bool enable);
    void EnableMultiThreading(bool enable);
}

} // namespace RawrXD
