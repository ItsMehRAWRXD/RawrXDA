#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <map>
#include "streaming_gguf_loader.h"
#include "engine/transformer.h"
#include "engine/sampler.h"

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

enum class TensorType {
    F32 = 0,
    F16 = 1,
    Q4_0 = 2,
    Q8_0 = 7,
};

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
            case TensorType::Q4_0: return 1;
            case TensorType::Q8_0: return 1;
            default: return 4;
        }
        return 4;
    }
};

class CPUInferenceEngine {
public:
    CPUInferenceEngine();
    ~CPUInferenceEngine();

    bool LoadModel(const std::string& path);
    bool IsModelLoaded() const { return m_modelLoaded; }
    
    // Core API
    std::vector<int32_t> Generate(const std::vector<int32_t>& input_tokens, int max_tokens);
    void GenerateStreaming(const std::vector<int32_t>& input_tokens, int max_tokens,
                          std::function<void(const std::string&)> token_callback,
                          std::function<void()> complete_callback = nullptr,
                          std::function<void(int32_t)> token_id_callback = nullptr);

    // Public Logic Accessors (for Coordinator/Agents)
    std::vector<float> Eval(const std::vector<int32_t>& input_tokens);
    std::vector<int32_t> Tokenize(const std::string& text);
    std::string Detokenize(const std::vector<int32_t>& tokens);

    // Config
    void SetContextLimit(size_t limit);
    void SetMaxMode(bool enabled);
    void SetDeepThinking(bool enabled);
    void SetDeepResearch(bool enabled);
    void ConfigureSampling(float temperature, float top_p, int top_k, float repeat_penalty);
    void RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin> plugin);

    // Getters
    int GetEmbeddingDim() const { return m_embeddingDim; }
    int GetNumLayers() const { return m_numLayers; }
    int GetNumHeads() const { return m_numHeads; }
    int GetVocabSize() const { return m_vocabSize; }
    size_t GetContextLimit() const { return m_contextLimit; }
    size_t GetMemoryUsage() const;

    // Internal Math Wrappers (Exposed due to implementation structure)
    void MatMul(const float* A, const float* B, float* C, int m, int n, int k);
    void Softmax(float* data, int size);
    void RMSNorm(float* data, int size, float epsilon);
    void LayerNorm(float* data, int size, float epsilon);
    void RoPE(float* data, int dim, int pos, int rotary_dim);
    void SiLU(float* data, int size);
    void GELU(float* data, int size);
    void FeedForward(const float* input, float* output, int layer_idx);
    void MultiHeadAttention(const float* Q, const float* K, const float* V, float* output, int seq_len, int head_dim, int num_heads, int layer_idx);
    
    // Weight Management
    bool LoadWeights(const std::unordered_map<std::string, Tensor>& tensors);
    void UpdateWeights(const std::vector<std::vector<float>>& gradients, float lr);
    void UpdateOutputWeights(const std::vector<float>& gradients, float lr);
    
    // Legacy / Duplicates found in CPP
    bool loadModel(const std::string& path);
    void TransformerLayer(const float* input, float* output, int layer_idx, int seq_len);
    void SetContextSize(size_t size);
    void ClearCache();
    float* AllocateTensor(size_t size);
    void DeallocateTensor(float* ptr);

private:
    void InitKVCache();
    bool RunForward(const std::vector<int32_t>& tokens, std::vector<float>& logits);
    
    std::unique_ptr<StreamingGGUFLoader> m_loader;
    std::vector<std::string> m_vocab;
    
    // Config
    int m_numLayers = 0;
    int m_numHeads = 0;
    int m_embeddingDim = 0;
    int m_vocabSize = 0;
    size_t m_contextLimit = 4096;
    int m_threadCount = 1;
    
    // State
    bool m_modelLoaded = false;
    int m_currentPos = 0;
    
    // Flags
    bool m_maxMode = false;
    bool m_deepThinking = false;
    bool m_deepResearch = false;
    
    std::unordered_map<std::string, Tensor> m_weights;
    Tensor m_tokenEmbeddings;
    Tensor m_outputNorm;
    Tensor m_outputWeights;

    // Layers
    struct LayerWeights {
        Tensor attention_query;
        Tensor attention_key;
        Tensor attention_value;
        Tensor attention_output;
        Tensor layer_norm1;
        Tensor feed_forward1;
        Tensor feed_forward2;
        Tensor feed_forward3;
        Tensor layer_norm2;
    };
    std::vector<LayerWeights> m_layers;
    std::vector<std::unique_ptr<TransformerLayer>> m_transformerLayers;
    std::unordered_map<std::string, TensorInfo> m_tensorInfo;
    std::unordered_map<std::string, int> m_vocabIndex;
    int m_unkTokenId = 0;
    int m_eosTokenId = -1;
    int m_maxTokenLength = 0;

    // KV Cache
    struct KVCache {
        std::vector<float> keys;
        std::vector<float> values;
    };
    std::vector<KVCache> m_kv_cache;

    std::vector<std::shared_ptr<RawrXD::IMemoryPlugin>> m_memoryPlugins;
    std::vector<std::unique_ptr<float[]>> m_memoryPool;
    size_t m_totalMemoryAllocated = 0;
    void* m_hTitanDLL = nullptr;
    std::vector<float> m_lastState;

    Sampler m_sampler;
};

namespace CPUOps {
    void MatMul(const float* A, const float* B, float* C, int m, int n, int k);
    void VectorAdd(const float* a, const float* b, float* c, int size);
    void VectorMul(const float* a, const float* b, float* c, int size);
    void VectorScale(float* data, float scale, int size);
    void Softmax(float* data, int size);
    void GELU(float* data, int size);
    void SiLU(float* data, int size);
    void RMSNorm(float* data, int size, float epsilon = 1e-5f);
}

} // namespace CPUInference
