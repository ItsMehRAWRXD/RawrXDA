#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include "engine/transformer.h"
#include "engine/bpe_tokenizer.h"
#include "streaming_gguf_loader.h"
#include "memory_core.h"

// Forward declarations
namespace RawrXD { 
    class IMemoryPlugin; 
    class BPETokenizer;
}

namespace CPUInference {

class StreamingGGUFLoader; // Forward decl

// Tensor Types usually found in engine logic
enum class TensorType {
    F32,
    F16,
    Q4_0,
    Q8_0,
    // Add others as needed
};

struct Tensor {
    std::string name;
    std::vector<uint64_t> shape;
    TensorType type;
    size_t element_size;
    std::vector<uint8_t> data;

    static size_t GetElementSize(TensorType t) {
        switch(t) {
            case TensorType::F32: return 4;
            case TensorType::F16: return 2;
            case TensorType::Q4_0: return 0; // block based
            case TensorType::Q8_0: return 0; // block based
            default: return 1;
        }
    }
};

struct KVCache {
    std::vector<float> keys;
    std::vector<float> values;
};

// Forward declare TransformerLayer
class TransformerLayer;

class CPUInferenceEngine {
public:
    CPUInferenceEngine();
    ~CPUInferenceEngine();

    bool LoadModel(const std::string& path);
    std::vector<int32_t> Generate(const std::vector<int32_t>& input_tokens, int max_tokens);
    void GenerateStreaming(const std::vector<int32_t>& input_tokens, int max_tokens, 
                          std::function<void(const std::string&)> token_callback,
                          std::function<void()> complete_callback,
                          std::function<void(int32_t)> token_id_callback = nullptr);

    std::vector<int32_t> Tokenize(const std::string& text);
    std::string Detokenize(const std::vector<int32_t>& tokens);

    void SetContextLimit(size_t limit);
    void SetThreadCount(int count);
    void SetMaxMode(bool enabled);
    void SetDeepThinking(bool enabled);
    void SetDeepResearch(bool enabled);

    void ConfigureSampling(float temp, float top_p, int top_k, float rep_pen);
    void RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin> plugin);
    size_t GetMemoryUsage() const;

    // Internal Math Wrappers (Public for visibility/testing)
    void MatMul(const float* A, const float* B, float* C, int m, int n, int k);
    void Softmax(float* data, int size);
    void RMSNorm(float* data, int size, float epsilon);
    void LayerNorm(float* data, int size, float epsilon);
    void RoPE(float* data, int dim, int pos, int rotary_dim);

    // Helpers
    void InitKVCache();

    struct SamplerConfig {
        float temperature = 0.8f;
        float top_p = 0.9f;
        int top_k = 40;
        float repetition_penalty = 1.1f;
    } m_sampler;

    // Public method wrappers for CPUOps if needed by static helpers
    static void SiLU(float* data, int size);
    static void GELU(float* data, int size);
    static void FeedForward(const float* input, float* output, int layer_idx); // Simplified sig

private:
    // Core components
    std::unique_ptr<StreamingGGUFLoader> m_loader;
    std::unique_ptr<RawrXD::BPETokenizer> m_tokenizer;
    std::shared_ptr<RawrXD::IMemoryPlugin> m_memoryPlugin;
    
    // Model architecture
    int m_vocabSize = 32000;
    int m_embeddingDim = 4096;
    int m_numLayers = 32;
    int m_numHeads = 32;
    int m_numKVHeads = 32;
    int m_hiddenDim = 11008;
    
    // Runtime state
    size_t m_contextLimit = 4096;
    int m_threadCount = 4;
    bool m_modelLoaded = false;
    int m_currentPos = 0;
    
    // Model weights
    Tensor m_tokenEmbeddings;
    Tensor m_outputNorm;
    Tensor m_outputWeights;
    std::vector<Tensor> m_layerWeights;
    
    // KV cache
    std::vector<KVCache> m_kvCache;
    
    // Generation config
    SamplerConfig m_samplerConfig;
    
    // Transformer layers (simplified for now)
    std::vector<std::unique_ptr<TransformerLayer>> m_transformerLayers;
    
    // Vocabulary
    std::vector<std::string> m_vocab;
    std::unordered_map<std::string, int> m_vocabIndex;
    int m_bosTokenId = 1;
    int m_eosTokenId = 2;
    int m_unkTokenId = 0;
    int m_maxTokenLength = 32;
    
    // Internal methods
    bool RunForward(const std::vector<int32_t>& tokens, std::vector<float>& logits);
    bool LookupEmbedding(const Tensor& emb, int token, int dim, std::vector<float>& out);
    bool LinearTensor(const Tensor& weight, const float* input, int input_dim, std::vector<float>& output);
    
    // Weight loading
    void* LoadTensorData(StreamingGGUFLoader* loader, std::map<std::string, std::vector<uint8_t>>& store, const std::string& name);
    bool LoadWeights();
    
    // Utility
    static void ConvertF16ToF32(const uint16_t* src, float* dst, size_t count);
};
    static void MultiHeadAttention(const float* Q, const float* K, const float* V, float* output, int n_heads, int d_head, int seq_len, int cache_pos); // Simplified sig
    static void TransformerLayer(const float* input, float* output, int layer_idx, int seq_len); // Simplified sig
    
    // Memory management helpers (stubbed)
    float* AllocateTensor(size_t size);
    void DeallocateTensor(float* ptr);
    void ClearCache();
    void UpdateWeights(const std::vector<std::vector<float>>& gradients, float lr);
    void UpdateOutputWeights(const std::vector<float>& gradients, float lr);

    // Helper for RunForward
    bool RunForward(const std::vector<int32_t>& tokens, std::vector<float>& logits);

protected:
    bool loadModel(const std::string& path);
    std::vector<float> Eval(const std::vector<int32_t>& tokens);
    
    // Internal Loading
    bool LoadWeights(const std::map<std::string, Tensor>& tensors); // Changed to map for cleaner look

private:
    bool m_modelLoaded = false;
    size_t m_contextLimit = 4096;
    int m_threadCount = 1;
    
    // Flags
    bool m_maxMode = false;
    bool m_deepThinking = false;
    bool m_deepResearch = false;
    
    // Model Metadata
    int m_embeddingDim = 0;
    int m_numLayers = 0;
    int m_numHeads = 0;
    int m_vocabSize = 0;
    
    // Vocab
    std::vector<std::string> m_vocab;

    // Components
    std::unique_ptr<StreamingGGUFLoader> m_loader;
    std::unique_ptr<RawrXD::BPETokenizer> m_tokenizer;
    std::vector<std::unique_ptr<::TransformerLayer>> m_transformerLayers;

    std::vector<std::shared_ptr<RawrXD::IMemoryPlugin>> m_memoryPlugins;
    std::vector<KVCache> m_kv_cache;
    
    // Metrics
    size_t m_totalMemoryAllocated = 0;
    std::map<std::string, Tensor> m_weights; // Stored weights
};

} // namespace CPUInference

