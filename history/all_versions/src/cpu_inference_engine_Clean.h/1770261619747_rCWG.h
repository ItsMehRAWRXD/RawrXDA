#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <unordered_map>
#include "engine/transformer.h"
#include "engine/bpe_tokenizer.h"
#include "streaming_gguf_loader.h"
#include "memory_core.h"

// Forward declarations
class BPETokenizer; 

namespace RawrXD { 
    class IMemoryPlugin {
    public:
        virtual ~IMemoryPlugin() = default;
        virtual bool Optimize() = 0; 
        virtual std::string GetName() const = 0;
        virtual size_t GetMaxContext() const = 0;
        virtual bool Configure(size_t limit) = 0;
    };
}

namespace CPUInference {

class StreamingGGUFLoader; 

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
            case TensorType::Q4_0: return 0;
            case TensorType::Q8_0: return 0;
            default: return 1;
        }
    }
};

struct KVCache {
    std::vector<float> keys;
    std::vector<float> values;
};

class CPUInferenceEngine {
public:
    CPUInferenceEngine();
    ~CPUInferenceEngine();

    bool LoadModel(const std::string& path);
    bool IsModelLoaded() const { return m_modelLoaded; }

    std::vector<int32_t> Generate(const std::vector<int32_t>& input_tokens, int max_tokens);
    void GenerateStreaming(const std::vector<int32_t>& input_tokens, int max_tokens, 
                          std::function<void(const std::string&)> token_callback,
                          std::function<void()> complete_callback,
                          std::function<void(int32_t)> token_id_callback = nullptr);

    std::vector<int32_t> Tokenize(const std::string& text);
    std::string Detokenize(const std::vector<int32_t>& tokens);

    void SetContextLimit(size_t limit);
    void SetThreadCount(int count);
    void SetMaxMode(bool enabled) { m_maxMode = enabled; }
    void SetDeepThinking(bool enabled) { m_deepThinking = enabled; }
    void SetDeepResearch(bool enabled) { m_deepResearch = enabled; }

    void ConfigureSampling(float temp, float top_p, int top_k, float rep_pen);
    void RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin> plugin);
    size_t GetMemoryUsage() const;

    void InitKVCache();

    struct SamplerConfig {
        float temp = 0.8f;   
        float top_p = 0.9f;
        int top_k = 40;
        float repeat_penalty = 1.1f;
    } m_sampler;

    // Public for real.cpp access
    std::map<std::string, std::vector<uint8_t>> m_weight_store;
    float* m_tok_embeddings = nullptr;
    float* m_output_norm = nullptr;
    uint8_t* m_output_weight = nullptr; 
    
    std::unordered_map<std::string, Tensor> m_weights;

    // Missing API methods from real.cpp implementation
    bool loadModel(const std::string& path); 
    
    std::vector<float> Eval(const std::vector<int32_t>& tokens);

    void MultiHeadAttention(const float* Q, const float* K, const float* V, float* output, int seq_len, int head_dim, int num_heads, int layer_idx);
    bool LoadWeights(const std::unordered_map<std::string, Tensor>& tensors);
    void UpdateWeights(const std::vector<std::vector<float>>& gradients, float lr);
    void UpdateOutputWeights(const std::vector<float>& gradients, float lr);
    void TransformerLayerMain(const float* input, float* output, int layer_idx, int seq_len); 
    void ClearCache();
    float* AllocateTensor(size_t size);
    void DeallocateTensor(float* ptr);
    
    // Internal wrappers
    void MatMul(const float* A, const float* B, float* C, int m, int n, int k);
    void Softmax(float* data, int size);
    void RMSNorm(float* data, int size, float epsilon);
    void LayerNorm(float* data, int size, float epsilon);
    void RoPE(float* data, int dim, int pos, int rotary_dim);
    static void SiLU(float* data, int size);
    static void GELU(float* data, int size);
    void FeedForward(const float* input, float* output, int layer_idx);

private:
    std::unique_ptr<StreamingGGUFLoader> m_loader;
    std::unique_ptr<::BPETokenizer> m_tokenizer;
    std::shared_ptr<RawrXD::IMemoryPlugin> m_memoryPlugin;
    std::vector<std::shared_ptr<RawrXD::IMemoryPlugin>> m_memoryPlugins;
    
    int m_vocabSize = 32000;
    int m_embeddingDim = 4096;
    int m_numLayers = 32;
    int m_numHeads = 32;
    int m_numKVHeads = 32;
    int m_hiddenDim = 11008;
    
    size_t m_contextLimit = 4096;
    int m_threadCount = 4;
    bool m_modelLoaded = false;
    int m_currentPos = 0;
    
    bool m_maxMode = false;
    bool m_deepThinking = false;
    bool m_deepResearch = false;

    std::vector<std::unique_ptr<::TransformerLayer>> m_transformerLayers;
    std::vector<std::string> m_vocab;
    std::vector<KVCache> m_kv_cache;
    
    int m_unkTokenId = 0;
    int m_eosTokenId = -1;
    int m_maxTokenLength = 0;
};

} // namespace CPUInference
