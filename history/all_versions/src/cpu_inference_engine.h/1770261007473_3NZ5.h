#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "engine/transformer.h"
#include "engine/bpe_tokenizer.h"
#include "streaming_gguf_loader.h"

// Forward declarations
namespace RawrXD { 
    class IMemoryPlugin; 
    class BPETokenizer;
}

namespace CPUInference {

class StreamingGGUFLoader; // Forward decl

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

protected:
    bool loadModel(const std::string& path);
    std::vector<float> Eval(const std::vector<int32_t>& tokens);

private:
    bool m_modelLoaded;
    size_t m_contextLimit;
    
    // Model Metadata
    int m_embeddingDim;
    int m_numLayers;
    int m_numHeads;
    int m_vocabSize;

    // Components
    std::unique_ptr<StreamingGGUFLoader> m_loader;
    std::unique_ptr<RawrXD::BPETokenizer> m_tokenizer;
    std::vector<std::unique_ptr<TransformerLayer>> m_transformerLayers;

    // Flag to bypass checks
    bool m_bypassChecks = false;

    // Legacy/Real storage to satisfy cpu_inference_engine_real.cpp
public:
    std::map<std::string, std::vector<uint8_t>> m_weight_store;
    float* m_tok_embeddings = nullptr;
    float* m_output_norm = nullptr;
    uint8_t* m_output_weight = nullptr;
    
    std::unordered_map<std::string, Tensor> m_weights;
};

} // namespace CPUInference
