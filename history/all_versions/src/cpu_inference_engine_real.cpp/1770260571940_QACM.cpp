#include "cpu_inference_engine.h"
#include "streaming_gguf_loader.h"
#include "engine/inference_kernels.h"
#include "engine/transformer.h"
#include <iostream>
#include <cmath>
#include <thread>
#include <algorithm>
#include <vector>
#include <cstring>
#include <map>

namespace CPUInference {

// Helper to load tensor and store in map
static uint8_t* LoadTensorData(StreamingGGUFLoader* loader, std::map<std::string, std::vector<uint8_t>>& store, const std::string& name) {
    std::vector<uint8_t> data;
    if (loader->GetTensorData(name, data)) {
        store[name] = std::move(data);
        return store[name].data();
    }
    // Alternate naming
    std::string alt = name;
    if (name.find("token_embd.weight") != std::string::npos) alt = "model.embed_tokens.weight";
    else if (name.find("output_norm.weight") != std::string::npos) alt = "model.norm.weight";
    else if (name.find("output.weight") != std::string::npos) alt = "lm_head.weight";
    
    if (alt != name && loader->GetTensorData(alt, data)) {
        store[alt] = std::move(data);
        return store[alt].data();
    }
    
    // Fallback for layer weights
    // Try to map blk.N.attn_q -> model.layers.N.self_attn.q_proj
    // This mapping logic can be expanded. 
    return nullptr;
}

CPUInferenceEngine::CPUInferenceEngine() 
    : m_numLayers(0), m_numHeads(0), m_embeddingDim(0), m_vocabSize(0), 
      m_threadCount(std::thread::hardware_concurrency()), m_modelLoaded(false), 
      m_contextLimit(4096), m_currentPos(0)
{
    m_loader = std::make_unique<StreamingGGUFLoader>();
}

CPUInferenceEngine::~CPUInferenceEngine() {}

bool CPUInferenceEngine::LoadModel(const std::string& path) {
    if (!m_loader->Open(path)) return false;
    
    GGUFMetadata meta = m_loader->GetMetadata();
    m_vocabSize = meta.vocab_size;
    m_embeddingDim = meta.embedding_dim;
    m_numLayers = meta.layer_count;
    m_numHeads = meta.head_count;
    m_contextLimit = meta.context_length;
    
    int n_kv_heads = meta.head_count_kv; 
    if (n_kv_heads == 0) n_kv_heads = m_numHeads;
    
    if (!meta.tokens.empty()) {
        m_vocab = meta.tokens;
    } else {
        for(int i = 0; i < std::max(100, m_vocabSize); i++) 
            m_vocab.push_back("tok" + std::to_string(i));
    }

    m_layers.clear();
    m_weight_store.clear();
    
    // Global weights
    m_tok_embeddings = (float*)LoadTensorData(m_loader.get(), m_weight_store, "token_embd.weight");
    m_output_norm = (float*)LoadTensorData(m_loader.get(), m_weight_store, "output_norm.weight");
    m_output_weight = LoadTensorData(m_loader.get(), m_weight_store, "output.weight");

    if (!m_tok_embeddings || !m_output_norm || !m_output_weight) {
        std::cerr << "Warning: Critical weights missing, inference will fail/crash.\n";
    }
    
    int hidden_dim = meta.feed_forward_length;

    for (int i = 0; i < m_numLayers; i++) {
        auto layer = std::make_unique<TransformerLayer>(m_embeddingDim, m_numHeads, n_kv_heads, hidden_dim);
        std::string pre = "blk." + std::to_string(i) + ".";
        
        layer->attn_norm = (float*)LoadTensorData(m_loader.get(), m_weight_store, pre + "attn_norm.weight");
        layer->wq = LoadTensorData(m_loader.get(), m_weight_store, pre + "attn_q.weight");
        layer->wk = LoadTensorData(m_loader.get(), m_weight_store, pre + "attn_k.weight");
        layer->wv = LoadTensorData(m_loader.get(), m_weight_store, pre + "attn_v.weight");
        layer->wo = LoadTensorData(m_loader.get(), m_weight_store, pre + "attn_output.weight");
        
        layer->ffn_norm = (float*)LoadTensorData(m_loader.get(), m_weight_store, pre + "ffn_norm.weight");
        layer->w1 = LoadTensorData(m_loader.get(), m_weight_store, pre + "ffn_gate.weight"); 
        layer->w3 = LoadTensorData(m_loader.get(), m_weight_store, pre + "ffn_up.weight");
        layer->w2 = LoadTensorData(m_loader.get(), m_weight_store, pre + "ffn_down.weight");
        
        m_layers.push_back(std::move(layer));
        if (i % 5 == 0) std::cout << "Loaded layer " << i << "/" << m_numLayers << "\r" << std::flush;
    }
    std::cout << "Model Loaded Successfully.\n";
    
    m_modelLoaded = true;
    InitKVCache();
    return true;
}

void CPUInferenceEngine::InitKVCache() {
    m_currentPos = 0;
    for(auto& layer : m_layers) {
        layer->cache_pos = 0; 
    }
}

std::vector<int32_t> CPUInferenceEngine::Generate(const std::vector<int32_t>& input_tokens, int max_tokens) {
    std::vector<int32_t> result = input_tokens;
    if (!m_modelLoaded) return result;
    
    // Pre-fill context if needed?
    // TransformerLayer forwards one token at a time typically in this impl
    // We need to process the whole prompt first.
    // Assuming input_tokens are the prompt.
    // Reset cache.
    InitKVCache(); 

    // Prefill
    for (size_t i = 0; i < input_tokens.size(); ++i) {
         m_currentPos = i;
         std::vector<int32_t> single_tok = { input_tokens[i] };
         Eval(single_tok); // Just run without sampling
    }
    
    int current_token = result.back();
    m_currentPos = input_tokens.size();

    for (int i = 0; i < max_tokens; ++i) {
        std::vector<int32_t> ctx = { current_token };
        std::vector<float> logits = Eval(ctx);
        if (logits.empty()) break;
        
        int next_token = 0;
        float max_val = -1e9f;
        for (size_t j = 0; j < logits.size(); ++j) {
            if (logits[j] > max_val) {
                max_val = logits[j];
                next_token = (int)j;
            }
        }
        
        result.push_back(next_token);
        current_token = next_token;
        m_currentPos++;
        
        if (current_token == 2) break; // EOS
    }
    return result;
}

void CPUInferenceEngine::GenerateStreaming(const std::vector<int32_t>& input_tokens, int max_tokens,
                                          std::function<void(const std::string&)> token_callback,
                                          std::function<void()> complete_callback,
                                          std::function<void(int32_t)> token_id_callback) {
    if (!m_modelLoaded || input_tokens.empty()) return;

    InitKVCache();
    // Prefill
    for (size_t i = 0; i < input_tokens.size(); ++i) {
         m_currentPos = i;
         std::vector<int32_t> single_tok = { input_tokens[i] };
         Eval(single_tok);
    }
    
    int current_token = input_tokens.back();
    m_currentPos = input_tokens.size();

    for (int step = 0; step < max_tokens; ++step) {
        std::vector<int32_t> ctx = { current_token };
        std::vector<float> logits = Eval(ctx);
        if (logits.empty()) break;
        
        int next_token = 0;
        float max_val = -1e9f;
        for (size_t j = 0; j < logits.size(); ++j) {
             if (logits[j] > max_val) {
                 max_val = logits[j];
                 next_token = (int)j;
             }
        }
        
        if (token_id_callback) token_id_callback(next_token);
        if (token_callback && next_token < (int)m_vocab.size()) 
            token_callback(m_vocab[next_token]);
            
        current_token = next_token;
        m_currentPos++;
        
        if (current_token == 2) break;
    }
    
    if (complete_callback) complete_callback();
}

std::vector<float> CPUInferenceEngine::Eval(const std::vector<int32_t>& input_tokens) {
    if (!m_modelLoaded || input_tokens.empty()) return {};
    
    int token = input_tokens[0];
    int pos = m_currentPos; 
    
    std::vector<float> x(m_embeddingDim);
    
    // Embedding
    if (m_tok_embeddings) {
        float* emb = m_tok_embeddings + token * m_embeddingDim;
        std::memcpy(x.data(), emb, m_embeddingDim * sizeof(float));
    }
    
    // Layers
    for (auto& layer : m_layers) {
        layer->forward(x.data(), pos, 1);
    }
    
    // Final Norm
    if (m_output_norm) {
        InferenceKernels::rmsnorm_avx512(x.data(), x.data(), m_output_norm, m_embeddingDim);
    }
    
    // LM Head
    std::vector<float> logits(m_vocabSize);
    float* w = (float*)m_output_weight; 
    
    // Simple matmul
    for (int i = 0; i < m_vocabSize; ++i) {
        float sum = 0.0f;
        for (int j = 0; j < m_embeddingDim; ++j) {
            sum += x[j] * w[i * m_embeddingDim + j];
        }
        logits[i] = sum;
    }

    return logits;
}

} // namespace CPUInference
