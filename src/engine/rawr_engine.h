#pragma once
#include "gguf_core.h"
#include "bpe_tokenizer.h"
#include "sampler.h"
#include "transformer.h"
#include "inference_kernels.h"
#include <memory>
#include <functional>

class RawrEngine {
    EngineGGUFLoader model;
    BPETokenizer tokenizer;
    Sampler sampler;
    
    std::vector<std::unique_ptr<TransformerLayer>> layers;
    float* tok_embeddings;
    float* output_norm;
    void* output_weight;
    ggml_type output_weight_type = GGML_TYPE_F32;
    
    int dim, n_layers, n_heads, n_kv_heads, vocab_size, hidden_dim;
    int head_dim;
    
public:
    bool load(const char* model_path, const char* tok_path, const char* merge_path);
    std::string generate(const std::string& prompt, int max_tokens, 
                         std::function<void(const char*)> callback);
};
