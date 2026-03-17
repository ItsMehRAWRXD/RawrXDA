#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <unordered_map>
#include "engine/transformer.h"
#include "engine/bpe_tokenizer.h"
#include "engine/sampler.h"_loader.h"
#include "engine/common_types.h"#include "memory_core.h"
#include "streaming_gguf_loader.h"
ions
// Forward declarations
namespace RawrXD { ; 
    class IMemoryPlugin {   class BPETokenizer;
    public:}
        virtual ~IMemoryPlugin() = default;
        virtual std::string GetName() const = 0;namespace CPUInference {
        virtual size_t GetMaxContext() const = 0;
        virtual bool Configure(size_t limit) = 0;class StreamingGGUFLoader; // Forward decl
        virtual bool Optimize() = 0;
    }; found in engine logic
ss TensorType {
    class BPETokenizer;
}

namespace CPUInference {

class StreamingGGUFLoader; // Forward decl

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
    ~CPUInferenceEngine(); as global or namespace based on usage
::TransformerLayer
    bool LoadModel(const std::string& path);// So we don't need to declare 'class TransformerLayer' here unless we use it.
    std::vector<int32_t> Generate(const std::vector<int32_t>& input_tokens, int max_tokens);::TransformerLayer>
    void GenerateStreaming(const std::vector<int32_t>& input_tokens, int max_tokens, 
                          std::function<void(const std::string&)> token_callback,{
                          std::function<void()> complete_callback,
                          std::function<void(int32_t)> token_id_callback = nullptr);    CPUInferenceEngine();

    std::vector<int32_t> Tokenize(const std::string& text);
    std::string Detokenize(const std::vector<int32_t>& tokens);

    void SetContextLimit(size_t limit);t_tokens, int max_tokens);
    void SetThreadCount(int count); 
    void SetMaxMode(bool enabled);                          std::function<void(const std::string&)> token_callback,
    void SetDeepThinking(bool enabled);llback,
    void SetDeepResearch(bool enabled);_callback = nullptr);

    void ConfigureSampling(float temp, float top_p, int top_k, float rep_pen); std::string& text);
    void RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin> plugin);d::vector<int32_t>& tokens);
    size_t GetMemoryUsage() const;

    // Internal Math Wrappers (Public for visibility/testing)
    void MatMul(const float* A, const float* B, float* C, int m, int n, int k);    void SetMaxMode(bool enabled) { m_maxMode = enabled; }
    void Softmax(float* data, int size);
    void RMSNorm(float* data, int size, float epsilon);
    void LayerNorm(float* data, int size, float epsilon);
    void RoPE(float* data, int dim, int pos, int rotary_dim);    void ConfigureSampling(float temp, float top_p, int top_k, float rep_pen);
Plugin> plugin);
    // Helpers
    void InitKVCache();
    
    // Sampler wrapper class to match usage in cpp
    class EngineSampler : public Sampler {
    public:    struct SamplerConfig {
        using Sampler::Sampler;temp = 0.8f;   // Matches real.cpp usage m_sampler.temp
        int Sample(const std::vector<float>& logits) {.9f;
            return sample(const_cast<float*>(logits.data()), static_cast<int>(logits.size()));        int top_k = 40;
        }ty = 1.1f; // real.cpp expected repeat_penalty
    } m_sampler;

    // Internal methods (non-static to access members)age to satisfy cpu_inference_engine_real.cpp
    void SiLU(float* data, int size);
    void GELU(float* data, int size);::string, std::vector<uint8_t>> m_weight_store;
    void FeedForward(const float* input, float* output, int layer_idx);    float* m_tok_embeddings = nullptr;
    void MultiHeadAttention(const float* Q, const float* K, const float* V, float* output, int num_heads, int layer_idx);
    void TransformerLayer(const float* input, float* output, int layer_idx, int seq_len);enamed from ptr to match real.cpp usage
    
    // Memory management helpers
    float* AllocateTensor(size_t size);
    void DeallocateTensor(float* ptr);issing API methods from real.cpp implementation
    void ClearCache();st std::string& path); // lower camelCase alias
    void UpdateWeights(const std::vector<std::vector<float>>& gradients, float lr);
    void UpdateOutputWeights(const std::vector<float>& gradients, float lr);
kens);
    // Helper for RunForward
    bool RunForward(const std::vector<int32_t>& tokens, std::vector<float>& logits);cpp implements but weren't in header?
on(...) 
protected:tions for these. We should declare them.
    bool loadModel(const std::string& path);on(const float* Q, const float* K, const float* V, float* output, int seq_len, int head_dim, int num_heads, int layer_idx);
    std::vector<float> Eval(const std::vector<int32_t>& tokens);st std::unordered_map<std::string, Tensor>& tensors);
    st std::vector<std::vector<float>>& gradients, float lr);
    // Internal Loading(const std::vector<float>& gradients, float lr);
    bool LoadWeights(const std::unordered_map<std::string, Tensor>& tensors);void TransformerLayerMain(const float* input, float* output, int layer_idx, int seq_len); // Renamed to avoid collision with constructor
);
private:size);
    bool m_modelLoaded = false;float* ptr);
    size_t m_contextLimit = 4096;
    int m_threadCount = 1;d in real.cpp or expected?
    int m_maxTokenLength = 128; // Max length for single token matchstatic void SiLU(float* data, int size);
    (float* data, int size);
    // Flags
    bool m_maxMode = false;
    bool m_deepThinking = false;ngGGUFLoader> m_loader;
    bool m_deepResearch = false;r> m_tokenizer;
    std::shared_ptr<RawrXD::IMemoryPlugin> m_memoryPlugin;
    // Model Metadata
    int m_embeddingDim = 0;
    int m_numLayers = 0;int m_vocabSize = 32000;
    int m_numHeads = 0; 4096;
    int m_vocabSize = 0;
    int m_eosTokenId = 2; // Defaultint m_numHeads = 32;
    int m_unkTokenId = 0; // Defaulteal.cpp?
    
    int m_currentPos = 0; // Current context position// Runtime state
    extLimit = 4096;
    // Vocab
    std::vector<std::string> m_vocab;
    std::unordered_map<std::string, int> m_vocabIndex;

    // Components
    std::unique_ptr<StreamingGGUFLoader> m_loader;
    std::unique_ptr<RawrXD::BPETokenizer> m_tokenizer;bool m_deepThinking = false;
    std::vector<std::unique_ptr<RawrXD::TransformerLayer>> m_transformerLayers; = false;

    std::vector<std::shared_ptr<RawrXD::IMemoryPlugin>> m_memoryPlugins;
    std::vector<KVCache> m_kv_cache;
    
    // Metrics
    size_t m_totalMemoryAllocated = 0;
    std::unordered_map<std::string, Tensor> m_weights; 
    std::vector<TensorInfo> m_tensorInfo;// Internal Loading helper
    uint8_t* LoadTensorData... (static in cpp)
    // Weights organized by layer
    std::vector<LayerWeights> m_layers;
    Tensor m_outputWeights;
    
    // Memory pool for tensors    std::vector<float*> m_memoryPool;};} // namespace CPUInference