#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <unordered_map>
#include <cstdint>

#include "engine/transformer.h"
#include "engine/bpe_tokenizer.h"
#include "streaming_gguf_loader.h"
#include "engine/sampler.h"

// Forward declarations
namespace RawrXD { 
    class IMemoryPlugin {
    public:
        virtual ~IMemoryPlugin() = default;
        virtual bool Optimize() = 0; 
        virtual std::string GetName() const = 0;
        virtual size_t GetMaxContext() const = 0;
        virtual bool Configure(size_t limit) = 0;
    };
    class BPETokenizer;
}

namespace CPUInference {

class StreamingGGUFLoader;

enum class TensorType {
    F32 = 0,
    F16 = 1,
    Q4_0 = 2,
    Q8_0 = 7,
};

struct Tensor {
    std::string name;
    std::vector<uint64_t> shape;
    TensorType type;
    size_t element_size;
    std::vector<uint8_t> data;
    
    Tensor() : type(TensorType::F32), element_size(4) {}
};

struct KVCache {
    std::vector<float> keys;
    std::vector<float> values;
};

class CPUInferenceEngine {
public:
    CPUInferenceEngine();
    ~CPUInferenceEngine();

    // Core API
    bool LoadModel(const std::string& path);
    // Compatibility alias
    bool loadModel(const std::string& path);
    
    std::vector<int32_t> Generate(const std::vector<int32_t>& input_tokens, int max_tokens);
    void GenerateStreaming(const std::vector<int32_t>& input_tokens, int max_tokens, 
                          std::function<void(const std::string&)> token_callback,
                          std::function<void()> complete_callback,
                          std::function<void(int32_t)> token_id_callback = nullptr);

    std::vector<int32_t> Tokenize(const std::string& text);
    std::string Detokenize(const std::vector<int32_t>& tokens);
    
    std::vector<float> Eval(const std::vector<int32_t>& input_tokens);

    // Config Methods
    void SetContextLimit(size_t limit);
    void SetThreadCount(int count);
    void SetMaxMode(bool enabled);
    void SetDeepThinking(bool enabled);
    void SetDeepResearch(bool enabled);
    void ConfigureSampling(float temp, float top_p, int top_k, float rep_pen);
    void RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin> plugin);
    
    bool IsModelLoaded() const { return m_modelLoaded; }
    size_t GetMemoryUsage() const;
    int GetEmbeddingDim() const { return m_embeddingDim; }
    int GetNumLayers() const { return m_numLayers; }
    int GetNumHeads() const { return m_numHeads; }

    // Static Math Wrappers (Public)
    void MatMul(const float* A, const float* B, float* C, int m, int n, int k);
    void Softmax(float* data, int size);
    void RMSNorm(float* data, int size, float epsilon);
    void LayerNorm(float* data, int size, float epsilon);
    void RoPE(float* data, int dim, int pos, int rotary_dim);
    static void SiLU(float* data, int size);
    static void GELU(float* data, int size);
    static void FeedForward(const float* input, float* output, int layer_idx);
    static void MultiHeadAttention(const float* Q, const float* K, const float* V, float* output, int n_heads, int d_head, int seq_len, int cache_pos);
    
    // Internal but exposed for legacy/compat
    void InitKVCache();
    void ClearCache();
    bool LoadWeights(const std::unordered_map<std::string, Tensor>& tensors);
    void UpdateWeights(const std::vector<std::vector<float>>& gradients, float lr);
    void UpdateOutputWeights(const std::vector<float>& gradients, float lr);
    // Renamed function to avoid shadowing global class
    void TransformerLayerFunc(const float* input, float* output, int layer_idx, int seq_len);
    float* AllocateTensor(size_t size);
    void DeallocateTensor(float* ptr);

private:
    std::unique_ptr<StreamingGGUFLoader> m_loader;
    std::unique_ptr<RawrXD::BPETokenizer> m_tokenizer;
    
    // Model Config
    int m_vocabSize = 0;
    int m_embeddingDim = 0;
    int m_numLayers = 0;
    int m_numHeads = 0;
    size_t m_contextLimit = 4096;
    int m_threadCount = 4;
    bool m_modelLoaded = false;
    int m_currentPos = 0;
    
    // Flags
    bool m_maxMode = false;
    bool m_deepThinking = false;
    bool m_deepResearch = false;

    // Data Storage
    std::map<std::string, std::vector<uint8_t>> m_weight_store;
    float* m_tok_embeddings = nullptr;
    float* m_output_norm = nullptr;
    uint8_t* m_output_weight_ptr = nullptr;

    // Components
    std::vector<std::unique_ptr<::TransformerLayer>> m_transformerLayers;
    std::vector<std::shared_ptr<RawrXD::IMemoryPlugin>> m_memoryPlugins;
    
    Sampler m_sampler;
    std::vector<std::string> m_vocab;
    
    std::vector<float> m_lastState; 
};

} // namespace CPUInference
