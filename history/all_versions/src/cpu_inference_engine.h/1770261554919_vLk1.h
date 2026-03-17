#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <unordered_map>
#include "engine/transformer.h"
#include "engine/bpe_tokenizer.h"
#include "engine/sampler.h"
#include "engine/common_types.h"
#include "streaming_gguf_loader.h"

// Forward declarations
class BPETokenizer; 

namespace RawrXD { 
    class IMemoryPlugin; 
}

// Global scope forward declaration for TransformerLayer from engine/transformer.h
class TransformerLayer;

namespace CPUInference {

class StreamingGGUFLoader; // Forward decl

// Tensor Types usually found in engine logic
enum class TensorType {
    F32,
    F16,
    Q4_0,
    Q8_0,
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

// Internal weight structure for a layer
struct LayerWeights {
    Tensor attention_query;
    Tensor attention_key;
    Tensor attention_value;
    Tensor attention_output;
    Tensor feed_forward1;
    Tensor feed_forward2;
    Tensor feed_forward3;
    Tensor attention_norm;
    Tensor ffn_norm;
};

// KV Cache for attention
struct KVCache {
    std::vector<float> keys;
    std::vector<float> values;
};

class CPUInferenceEngine {
public:
    CPUInferenceEngine();
    ~CPUInferenceEngine();

    bool LoadModel(const std::string& path);
    bool IsModelLoaded() const { return m_modelLoaded; } // Added accessor

    std::vector<int32_t> Generate(const std::vector<int32_t>& input_tokens, int max_tokens);
    void GenerateStreaming(const std::vector<int32_t>& input_tokens, int max_tokens, 
                          std::function<void(const std::string&)> token_callback,
                          std::function<void()> complete_callback,
                          std::function<void(int32_t)> token_id_callback = nullptr);

    std::vector<int32_t> Tokenize(const std::string& text);
    std::string Detokenize(const std::vector<int32_t>& tokens);

    // Getters for Sovereign Engines / Coordinator
    int GetEmbeddingDim() const { return m_embeddingDim; }
    int GetNumLayers() const { return m_numLayers; }
    int GetNumHeads() const { return m_numHeads; }
    int GetVocabSize() const { return m_vocabSize; }
    size_t GetContextLimit() const { return m_contextLimit; }

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
    
    // Moved Eval to public for UnifiedEngineCoordinator
    std::vector<float> Eval(const std::vector<int32_t>& tokens);

    void RMSNorm(float* data, int size, float epsilon);
    void LayerNorm(float* data, int size, float epsilon);
    void RoPE(float* data, int dim, int pos, int rotary_dim);

    // Helpers
    void InitKVCache();
    
    // Sampler wrapper class to match usage in cpp
    class EngineSampler : public Sampler {
    public:
        using Sampler::Sampler;
        int Sample(const std::vector<float>& logits) {
            return sample(const_cast<float*>(logits.data()), static_cast<int>(logits.size()));
        }
    } m_sampler;

    // Internal methods (non-static to access members)
    void SiLU(float* data, int size);
    void GELU(float* data, int size);
    void FeedForward(const float* input, float* output, int layer_idx);
    void MultiHeadAttention(const float* Q, const float* K, const float* V, float* output, int num_heads, int layer_idx);
    void TransformerLayer(const float* input, float* output, int layer_idx, int seq_len);
    
    // Memory management helpers
    float* AllocateTensor(size_t size);
    void DeallocateTensor(float* ptr);
    void ClearCache();
    void UpdateWeights(const std::vector<std::vector<float>>& gradients, float lr);
    void UpdateOutputWeights(const std::vector<float>& gradients, float lr);

    // Helper for RunForward
    bool RunForward(const std::vector<int32_t>& tokens, std::vector<float>& logits);

protected:
    bool loadModel(const std::string& path);
    // Eval moved to public
    // Internal Loading
    bool LoadWeights(const std::unordered_map<std::string, Tensor>& tensors);

private:
    bool m_modelLoaded = false;
    size_t m_contextLimit = 4096;
    int m_threadCount = 1;
    int m_maxTokenLength = 128; // Max length for single token match
    
    // Flags
    bool m_maxMode = false;
    bool m_deepThinking = false;
    bool m_deepResearch = false;
    
    // Model Metadata
    int m_embeddingDim = 0;
    int m_numLayers = 0;
    int m_numHeads = 0;
    int m_vocabSize = 0;
    int m_eosTokenId = 2; // Default
    int m_unkTokenId = 0; // Default
    ;
    int m_currentPos = 0; // Current context position
    std::vector<KVCache> m_kv_cache;
    // Vocab
    std::vector<std::string> m_vocab;
    std::unordered_map<std::string, int> m_vocabIndex;
std::unordered_map<std::string, Tensor> m_weights; 
    // Components
    std::unique_ptr<StreamingGGUFLoader> m_loader;
    std::unique_ptr<RawrXD::BPETokenizer> m_tokenizer;
    std::vector<LayerWeights> m_layers;
    std::vector<std::unique_ptr<::TransformerLayer>> m_transformerLayers;Tensor m_outputWeights;

    std::vector<std::shared_ptr<RawrXD::IMemoryPlugin>> m_memoryPlugins;
    std::vector<KVCache> m_kv_cache;  std::vector<float*> m_memoryPool;
    };
    // Metrics
    size_t m_totalMemoryAllocated = 0;} // namespace CPUInference












} // namespace CPUInference 