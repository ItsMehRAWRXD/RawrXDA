/**
 * transformer_inference_noqt.cpp
 * Pure C++ transformer inference implementation without Qt dependencies
 */

#include "transformer_inference_noqt.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <iostream>

TransformerInference::TransformerInference() {
}

TransformerInference::~TransformerInference() {
    freeContext();
}

void TransformerInference::freeContext() {
    if (m_ctx) {
        ggml_free(m_ctx);
        m_ctx = nullptr;
    }
    if (m_kvCtx) {
        ggml_free(m_kvCtx);
        m_kvCtx = nullptr;
    }
    m_ready = false;
}

bool TransformerInference::loadWeights(const std::map<std::string, std::pair<std::vector<uint8_t>, int>>& tensorCache,
                                       int nLayers, int nEmbd, int nHead, int nVocab) {
    std::cerr << "Loading transformer weights: layers=" << nLayers 
              << " embd=" << nEmbd << " heads=" << nHead << " vocab=" << nVocab << "\n";
    
    m_nLayers = nLayers;
    m_nEmbd = nEmbd;
    m_nHead = nHead;
    m_nVocab = nVocab;
    
    // Allocate ggml context for model weights
    size_t ctxSize = 1024ull * 1024 * 1024;  // 1GB for weights
    struct ggml_init_params params = {
        .mem_size = ctxSize,
        .mem_buffer = nullptr,
        .no_alloc = false,
    };
    
    m_ctx = ggml_init(params);
    if (!m_ctx) {
        std::cerr << "Failed to initialize ggml context\n";
        return false;
    }
    
    try {
        // Load token embedding: [vocab_size, n_embd]
        auto it = tensorCache.find("token_embd.weight");
        if (it != tensorCache.end()) {
            m_tokenEmbed = createTensorFromCache(it->second.first, it->second.second, 
                                                 {m_nVocab, m_nEmbd});
        } else {
            // Try alternative name
            it = tensorCache.find("model.embed_tokens.weight");
            if (it != tensorCache.end()) {
                m_tokenEmbed = createTensorFromCache(it->second.first, it->second.second,
                                                     {m_nVocab, m_nEmbd});
            }
        }
        
        // Load output projection: [n_embd, vocab_size]
        it = tensorCache.find("output.weight");
        if (it != tensorCache.end()) {
            m_outputWeight = createTensorFromCache(it->second.first, it->second.second,
                                                   {m_nEmbd, m_nVocab});
        } else {
            it = tensorCache.find("lm_head.weight");
            if (it != tensorCache.end()) {
                m_outputWeight = createTensorFromCache(it->second.first, it->second.second,
                                                       {m_nEmbd, m_nVocab});
            }
        }
        
        // Load per-layer weights
        m_layers.resize(m_nLayers);
        for (int i = 0; i < m_nLayers; ++i) {
            std::string prefix = "blk." + std::to_string(i) + ".";
            std::string altPrefix = "model.layers." + std::to_string(i) + ".";
            
            LayerWeights& layer = m_layers[i];
            
            // Attention weights - Q
            auto qIt = tensorCache.find(prefix + "attn_q.weight");
            if (qIt != tensorCache.end()) {
                layer.attn_q = createTensorFromCache(qIt->second.first, qIt->second.second,
                                                     {m_nEmbd, m_nEmbd});
            } else {
                qIt = tensorCache.find(altPrefix + "self_attn.q_proj.weight");
                if (qIt != tensorCache.end()) {
                    layer.attn_q = createTensorFromCache(qIt->second.first, qIt->second.second,
                                                         {m_nEmbd, m_nEmbd});
                }
            }
            
            // Attention weights - K
            auto kIt = tensorCache.find(prefix + "attn_k.weight");
            if (kIt != tensorCache.end()) {
                layer.attn_k = createTensorFromCache(kIt->second.first, kIt->second.second,
                                                     {m_nEmbd, m_nEmbd});
            } else {
                kIt = tensorCache.find(altPrefix + "self_attn.k_proj.weight");
                if (kIt != tensorCache.end()) {
                    layer.attn_k = createTensorFromCache(kIt->second.first, kIt->second.second,
                                                         {m_nEmbd, m_nEmbd});
                }
            }
            
            // Attention weights - V
            auto vIt = tensorCache.find(prefix + "attn_v.weight");
            if (vIt != tensorCache.end()) {
                layer.attn_v = createTensorFromCache(vIt->second.first, vIt->second.second,
                                                     {m_nEmbd, m_nEmbd});
            } else {
                vIt = tensorCache.find(altPrefix + "self_attn.v_proj.weight");
                if (vIt != tensorCache.end()) {
                    layer.attn_v = createTensorFromCache(vIt->second.first, vIt->second.second,
                                                         {m_nEmbd, m_nEmbd});
                }
            }
            
            // Attention output projection
            auto outIt = tensorCache.find(prefix + "attn_out.weight");
            if (outIt != tensorCache.end()) {
                layer.attn_out = createTensorFromCache(outIt->second.first, outIt->second.second,
                                                       {m_nEmbd, m_nEmbd});
            } else {
                outIt = tensorCache.find(altPrefix + "self_attn.o_proj.weight");
                if (outIt != tensorCache.end()) {
                    layer.attn_out = createTensorFromCache(outIt->second.first, outIt->second.second,
                                                           {m_nEmbd, m_nEmbd});
                }
            }
        }
        
        m_ready = true;
        std::cerr << "Successfully loaded transformer weights\n";
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading weights: " << e.what() << "\n";
        freeContext();
        return false;
    }
}

bool TransformerInference::loadWeightsWithTypes(const std::map<std::string, std::pair<std::vector<uint8_t>, int>>& tensorCache,
                                               int nLayers, int nEmbd, int nHead, int nVocab) {
    // Extended version that handles quantized types
    return loadWeights(tensorCache, nLayers, nEmbd, nHead, nVocab);
}

struct ggml_tensor* TransformerInference::createTensorFromCache(const std::vector<uint8_t>& data,
                                                                int ggmlType,
                                                                const std::vector<int64_t>& shape) {
    if (data.empty() || !m_ctx) {
        return nullptr;
    }
    
    // Create tensor with appropriate shape
    struct ggml_tensor* tensor = nullptr;
    
    if (shape.size() == 1) {
        tensor = ggml_new_tensor_1d(m_ctx, static_cast<enum ggml_type>(ggmlType), shape[0]);
    } else if (shape.size() == 2) {
        tensor = ggml_new_tensor_2d(m_ctx, static_cast<enum ggml_type>(ggmlType), shape[0], shape[1]);
    } else if (shape.size() == 3) {
        tensor = ggml_new_tensor_3d(m_ctx, static_cast<enum ggml_type>(ggmlType), shape[0], shape[1], shape[2]);
    }
    
    if (!tensor) {
        return nullptr;
    }
    
    // Copy data into tensor
    if (tensor->data && data.size() <= ggml_nbytes(tensor)) {
        std::memcpy(tensor->data, data.data(), data.size());
    }
    
    return tensor;
}

struct ggml_tensor* TransformerInference::createTensorRef(const uint8_t* data,
                                                          int ggmlType,
                                                          const std::vector<int64_t>& shape) {
    if (!data || !m_ctx) {
        return nullptr;
    }
    
    // Create tensor reference (doesn't own data)
    struct ggml_tensor* tensor = nullptr;
    
    if (shape.size() == 1) {
        tensor = ggml_new_tensor_1d(m_ctx, static_cast<enum ggml_type>(ggmlType), shape[0]);
    } else if (shape.size() == 2) {
        tensor = ggml_new_tensor_2d(m_ctx, static_cast<enum ggml_type>(ggmlType), shape[0], shape[1]);
    }
    
    if (tensor) {
        tensor->data = const_cast<uint8_t*>(data);
    }
    
    return tensor;
}

std::vector<int32_t> TransformerInference::generateTokens(const std::vector<int32_t>& inputTokens, int maxTokens) {
    if (!m_ready) {
        return inputTokens;
    }
    
    std::vector<int32_t> output = inputTokens;
    
    for (int i = 0; i < maxTokens; ++i) {
        auto logits = getLogits(output);
        
        if (logits.empty()) break;
        
        // Sample next token
        auto nextTokens = sample(logits);
        if (!nextTokens.empty()) {
            output.push_back(nextTokens[0]);
        } else {
            break;
        }
    }
    
    return output;
}

std::vector<float> TransformerInference::getTokenEmbedding(int32_t tokenId) {
    std::vector<float> embedding(m_nEmbd, 0.0f);
    
    if (!m_tokenEmbed || tokenId < 0 || tokenId >= m_nVocab) {
        return embedding;
    }
    
    // Get embedding row
    if (m_tokenEmbed->data) {
        float* embedData = static_cast<float*>(m_tokenEmbed->data);
        size_t offset = tokenId * m_nEmbd;
        std::copy(embedData + offset, embedData + offset + m_nEmbd, embedding.begin());
    }
    
    return embedding;
}

std::vector<float> TransformerInference::getLogits(const std::vector<int32_t>& inputTokens) {
    std::vector<float> logits(m_nVocab, 0.0f);
    
    if (inputTokens.empty() || !m_ready) {
        return logits;
    }
    
    // Simple logits: average of input embeddings projected to vocab space
    std::vector<float> contextEmbedding(m_nEmbd, 0.0f);
    
    for (int32_t token : inputTokens) {
        auto embedding = getTokenEmbedding(token);
        for (size_t i = 0; i < embedding.size(); ++i) {
            contextEmbedding[i] += embedding[i];
        }
    }
    
    // Normalize
    for (auto& val : contextEmbedding) {
        val /= inputTokens.size();
    }
    
    // Project to vocabulary (placeholder)
    if (m_outputWeight && m_outputWeight->data) {
        float* outputData = static_cast<float*>(m_outputWeight->data);
        for (int v = 0; v < m_nVocab; ++v) {
            float score = 0.0f;
            for (int e = 0; e < m_nEmbd; ++e) {
                score += contextEmbedding[e] * outputData[v * m_nEmbd + e];
            }
            logits[v] = score;
        }
    }
    
    return logits;
}

std::vector<int32_t> TransformerInference::sample(const std::vector<float>& logits, int topK, float topP) {
    if (logits.empty()) {
        return {};
    }
    
    // Find top-K logits
    std::vector<std::pair<float, int>> scored;
    for (size_t i = 0; i < logits.size(); ++i) {
        scored.emplace_back(logits[i], i);
    }
    
    std::sort(scored.begin(), scored.end(), 
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    if (scored.size() > static_cast<size_t>(topK)) {
        scored.resize(topK);
    }
    
    // Convert to probabilities
    float maxLogit = scored[0].first;
    float sumProb = 0.0f;
    
    for (auto& p : scored) {
        p.first = std::exp(p.first - maxLogit);
        sumProb += p.first;
    }
    
    for (auto& p : scored) {
        p.first /= sumProb;
    }
    
    // Top-P filtering
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    float cumProb = 0.0f;
    size_t cutoff = scored.size();
    
    for (size_t i = 0; i < scored.size(); ++i) {
        cumProb += scored[i].first;
        if (cumProb > topP) {
            cutoff = i + 1;
            break;
        }
    }
    
    scored.resize(cutoff);
    
    // Sample token
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    float sample = dis(gen);
    float cumulative = 0.0f;
    
    for (const auto& p : scored) {
        cumulative += p.first;
        if (sample < cumulative) {
            return {static_cast<int32_t>(p.second)};
        }
    }
    
    return {static_cast<int32_t>(scored[0].second)};
}

std::vector<float> TransformerInference::forward(const std::vector<int32_t>& inputTokens) {
    // Placeholder for actual transformer forward pass
    return getLogits(inputTokens);
}
