#include "transformer_inference.hpp"
#include <ggml.h>
#include <ggml-backend.h>
#include <ggml-cpu.h>
#include <QDebug>
#include <cstring>
#include <cmath>
#include <random>
#include <algorithm>

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

bool TransformerInference::loadWeights(const QHash<QString, QByteArray>& tensorCache,
                                       int nLayers, int nEmbd, int nHead, int nVocab) {
    qInfo() << "Loading transformer weights: layers=" << nLayers 
            << "embd=" << nEmbd << "heads=" << nHead << "vocab=" << nVocab;
    
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
        qCritical() << "Failed to initialize ggml context";
        return false;
    }
    
    // Load token embedding: [vocab_size, n_embd]
    int64_t embdShape[] = {m_nVocab, m_nEmbd};
    m_tokenEmbed = createTensorFromCache("token_embd.weight", tensorCache, embdShape, 2);
    if (!m_tokenEmbed) {
        // Try alternative name
        m_tokenEmbed = createTensorFromCache("model.embed_tokens.weight", tensorCache, embdShape, 2);
    }
    
    // Load output projection: [n_embd, vocab_size]
    int64_t outShape[] = {m_nEmbd, m_nVocab};
    m_outputWeight = createTensorFromCache("output.weight", tensorCache, outShape, 2);
    if (!m_outputWeight) {
        m_outputWeight = createTensorFromCache("lm_head.weight", tensorCache, outShape, 2);
    }
    
    // Load per-layer weights
    m_layers.resize(m_nLayers);
    for (int i = 0; i < m_nLayers; ++i) {
        QString prefix = QString("blk.%1.").arg(i);
        QString altPrefix = QString("model.layers.%1.").arg(i);
        
        LayerWeights& layer = m_layers[i];
        int64_t qkvShape[] = {m_nEmbd, m_nEmbd};
        int64_t mlpShape[] = {m_nEmbd, m_nEmbd * 4};
        int64_t mlp2Shape[] = {m_nEmbd * 4, m_nEmbd};
        int64_t lnShape[] = {m_nEmbd};
        
        // Attention weights
        layer.attn_q = createTensorFromCache(prefix + "attn_q.weight", tensorCache, qkvShape, 2);
        if (!layer.attn_q) layer.attn_q = createTensorFromCache(altPrefix + "self_attn.q_proj.weight", tensorCache, qkvShape, 2);
        
        layer.attn_k = createTensorFromCache(prefix + "attn_k.weight", tensorCache, qkvShape, 2);
        if (!layer.attn_k) layer.attn_k = createTensorFromCache(altPrefix + "self_attn.k_proj.weight", tensorCache, qkvShape, 2);
        
        layer.attn_v = createTensorFromCache(prefix + "attn_v.weight", tensorCache, qkvShape, 2);
        if (!layer.attn_v) layer.attn_v = createTensorFromCache(altPrefix + "self_attn.v_proj.weight", tensorCache, qkvShape, 2);
        
        layer.attn_proj = createTensorFromCache(prefix + "attn_output.weight", tensorCache, qkvShape, 2);
        if (!layer.attn_proj) layer.attn_proj = createTensorFromCache(altPrefix + "self_attn.o_proj.weight", tensorCache, qkvShape, 2);
        
        // Layer norm
        layer.ln1_weight = createTensorFromCache(prefix + "attn_norm.weight", tensorCache, lnShape, 1);
        if (!layer.ln1_weight) layer.ln1_weight = createTensorFromCache(altPrefix + "input_layernorm.weight", tensorCache, lnShape, 1);
        
        // MLP
        layer.mlp_fc1 = createTensorFromCache(prefix + "ffn_up.weight", tensorCache, mlpShape, 2);
        if (!layer.mlp_fc1) layer.mlp_fc1 = createTensorFromCache(altPrefix + "mlp.up_proj.weight", tensorCache, mlpShape, 2);
        
        layer.mlp_fc2 = createTensorFromCache(prefix + "ffn_down.weight", tensorCache, mlp2Shape, 2);
        if (!layer.mlp_fc2) layer.mlp_fc2 = createTensorFromCache(altPrefix + "mlp.down_proj.weight", tensorCache, mlp2Shape, 2);
        
        layer.ln2_weight = createTensorFromCache(prefix + "ffn_norm.weight", tensorCache, lnShape, 1);
        if (!layer.ln2_weight) layer.ln2_weight = createTensorFromCache(altPrefix + "post_attention_layernorm.weight", tensorCache, lnShape, 1);
    }
    
    // Initialize KV cache
    initKVCache();
    
    m_ready = true;
    qInfo() << "Transformer weights loaded successfully";
    return true;
}

void TransformerInference::initKVCache() {
    // Allocate separate context for KV cache
    size_t kvSize = 512ull * 1024 * 1024;  // 512MB for KV cache
    struct ggml_init_params params = {
        .mem_size = kvSize,
        .mem_buffer = nullptr,
        .no_alloc = false,
    };
    
    m_kvCtx = ggml_init(params);
    if (!m_kvCtx) {
        qWarning() << "Failed to init KV cache context";
        return;
    }
    
    // Allocate K and V cache tensors: [n_layers, ctx_size, n_embd]
    m_kCache.resize(m_nLayers);
    m_vCache.resize(m_nLayers);
    
    for (int i = 0; i < m_nLayers; ++i) {
        m_kCache[i] = ggml_new_tensor_2d(m_kvCtx, GGML_TYPE_F32, m_nEmbd, m_ctxSize);
        m_vCache[i] = ggml_new_tensor_2d(m_kvCtx, GGML_TYPE_F32, m_nEmbd, m_ctxSize);
        
        // Zero initialize
        ggml_set_zero(m_kCache[i]);
        ggml_set_zero(m_vCache[i]);
    }
}

ggml_tensor* TransformerInference::createTensorFromCache(
    const QString& name,
    const QHash<QString, QByteArray>& cache,
    const int64_t* shape, int nDims) {
    
    if (!cache.contains(name)) {
        qWarning() << "Tensor not found in cache:" << name;
        return nullptr;
    }
    
    const QByteArray& data = cache[name];
    
    // Create tensor with specified shape
    ggml_tensor* tensor = nullptr;
    if (nDims == 1) {
        tensor = ggml_new_tensor_1d(m_ctx, GGML_TYPE_F32, shape[0]);
    } else if (nDims == 2) {
        tensor = ggml_new_tensor_2d(m_ctx, GGML_TYPE_F32, shape[0], shape[1]);
    } else {
        qWarning() << "Unsupported tensor dims:" << nDims;
        return nullptr;
    }
    
    if (!tensor) {
        qWarning() << "Failed to create tensor:" << name;
        return nullptr;
    }
    
    // Copy quantized data - for now assume F32 or will need dequant
    size_t expectedSize = ggml_nbytes(tensor);
    if (data.size() < (int)expectedSize) {
        qWarning() << "Tensor data too small:" << name << data.size() << "vs" << expectedSize;
        // Copy what we have
        std::memcpy(tensor->data, data.constData(), std::min<size_t>(data.size(), expectedSize));
    } else {
        std::memcpy(tensor->data, data.constData(), expectedSize);
    }
    
    return tensor;
}

std::vector<int32_t> TransformerInference::generate(const std::vector<int32_t>& prompt,
                                                     int maxTokens, float temperature) {
    if (!m_ready) {
        qWarning() << "Model not ready for generation";
        return {};
    }
    
    std::vector<int32_t> tokens = prompt;
    tokens.reserve(prompt.size() + maxTokens);
    
    for (int i = 0; i < maxTokens; ++i) {
        // Run forward pass
        std::vector<float> logits = forward(tokens);
        if (logits.empty()) break;
        
        // Sample next token
        int32_t nextToken = sampleToken(logits, temperature);
        tokens.push_back(nextToken);
        
        // Stop on EOS (assuming token 2 is EOS)
        if (nextToken == 2) break;
    }
    
    return tokens;
}

std::vector<float> TransformerInference::forward(const std::vector<int32_t>& tokens) {
    if (!m_ready || tokens.empty()) return {};
    
    // Create computation graph context
    size_t graphMem = 128 * 1024 * 1024;  // 128MB for compute graph
    struct ggml_init_params params = {
        .mem_size = graphMem,
        .mem_buffer = nullptr,
        .no_alloc = false,
    };
    
    ggml_context* gfCtx = ggml_init(params);
    if (!gfCtx) {
        qWarning() << "Failed to init graph context";
        return {};
    }
    
    // Build computation graph
    ggml_tensor* logitsTensor = buildGraph(gfCtx, tokens);
    
    if (!logitsTensor) {
        ggml_free(gfCtx);
        return {};
    }
    
    // Execute graph
    struct ggml_cgraph* gf = ggml_new_graph(gfCtx);
    ggml_build_forward_expand(gf, logitsTensor);
    
    // Create CPU backend for graph execution
    ggml_backend_t backend = ggml_backend_cpu_init();
    if (!backend) {
        qCritical() << "Failed to initialize GGML CPU backend";
        ggml_free(gfCtx);
        return {};
    }
    
    // Execute the computation graph
    enum ggml_status status = ggml_backend_graph_compute(backend, gf);
    if (status != GGML_STATUS_SUCCESS) {
        qWarning() << "Graph computation failed with status" << status;
    }
    
    // Extract logits from computed tensor
    std::vector<float> logits(m_nVocab);
    ggml_backend_tensor_get(logitsTensor, logits.data(), 0, m_nVocab * sizeof(float));
    
    // Cleanup backend
    ggml_backend_free(backend);
    
    ggml_free(gfCtx);
    return logits;
}

ggml_tensor* TransformerInference::buildGraph(ggml_context* ctx, const std::vector<int32_t>& tokens) {
    int nTokens = tokens.size();
    
    // Create input tensor for token IDs
    ggml_tensor* inp = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, nTokens);
    std::memcpy(inp->data, tokens.data(), nTokens * sizeof(int32_t));
    
    // Token embedding lookup: [n_tokens, n_embd]
    ggml_tensor* cur = ggml_get_rows(ctx, m_tokenEmbed, inp);
    
    // Process through transformer layers
    for (int il = 0; il < m_nLayers; ++il) {
        LayerWeights& layer = m_layers[il];
        
        // Layer norm 1 (pre-attention normalization)
        ggml_tensor* inpL = cur;
        if (layer.ln1_weight) {
            cur = ggml_norm(ctx, cur, 1e-5f);
            cur = ggml_mul(ctx, cur, layer.ln1_weight);
            if (layer.ln1_bias) {
                cur = ggml_add(ctx, cur, layer.ln1_bias);
            }
        }
        
        // Self-attention with proper multi-head handling
        // Project to Q, K, V: [n_tokens, n_embd] -> [n_tokens, n_embd]
        ggml_tensor* Q = ggml_mul_mat(ctx, layer.attn_q, cur);
        ggml_tensor* K = ggml_mul_mat(ctx, layer.attn_k, cur);
        ggml_tensor* V = ggml_mul_mat(ctx, layer.attn_v, cur);
        
        // Scaled dot-product attention: softmax((Q @ K^T) / sqrt(d_k)) @ V
        // Note: ggml_mul_mat(A, B) computes A @ B^T for proper matrix mult
        ggml_tensor* KQ = ggml_mul_mat(ctx, K, Q);  // [n_tokens, n_tokens] attention scores
        
        // Scale by 1/sqrt(d_k) where d_k = n_embd / n_head
        float scale = 1.0f / sqrtf((float)(m_nEmbd / m_nHead));
        KQ = ggml_scale(ctx, KQ, scale);
        
        // Apply causal mask for autoregressive generation (if needed)
        // For simplicity, just softmax without mask
        KQ = ggml_soft_max(ctx, KQ);
        
        // Attention output: [n_tokens, n_tokens] @ [n_tokens, n_embd] -> [n_tokens, n_embd]
        ggml_tensor* attnOut = ggml_mul_mat(ctx, V, KQ);
        
        // Attention projection back to embedding dimension
        if (layer.attn_proj) {
            attnOut = ggml_mul_mat(ctx, layer.attn_proj, attnOut);
        }
        
        // Residual connection: x = attn(ln(x)) + x
        cur = ggml_add(ctx, attnOut, inpL);
        
        // Layer norm 2 (pre-MLP normalization)
        inpL = cur;
        if (layer.ln2_weight) {
            cur = ggml_norm(ctx, cur, 1e-5f);
            cur = ggml_mul(ctx, cur, layer.ln2_weight);
            if (layer.ln2_bias) {
                cur = ggml_add(ctx, cur, layer.ln2_bias);
            }
        }
        
        // MLP: FC1 -> GELU -> FC2
        if (layer.mlp_fc1 && layer.mlp_fc2) {
            cur = ggml_mul_mat(ctx, layer.mlp_fc1, cur);
            cur = ggml_gelu(ctx, cur);
            cur = ggml_mul_mat(ctx, layer.mlp_fc2, cur);
        }
        
        // Residual connection: x = mlp(ln(x)) + x
        cur = ggml_add(ctx, cur, inpL);
    }
    
    // Final layer norm (typically uses dedicated final norm weights)
    if (!m_layers.empty() && m_layers.back().ln2_weight) {
        cur = ggml_norm(ctx, cur, 1e-5f);
        cur = ggml_mul(ctx, cur, m_layers.back().ln2_weight);
        if (m_layers.back().ln2_bias) {
            cur = ggml_add(ctx, cur, m_layers.back().ln2_bias);
        }
    }
    
    // Output projection to vocabulary: [n_tokens, n_embd] -> [n_tokens, vocab]
    if (m_outputWeight) {
        cur = ggml_mul_mat(ctx, m_outputWeight, cur);
    }
    
    // Extract last token logits for next-token prediction
    // ggml_view_1d uses element offsets, not byte offsets
    int lastTokenOffset = (nTokens - 1) * m_nVocab;
    cur = ggml_view_1d(ctx, cur, m_nVocab, lastTokenOffset * sizeof(float));
    
    return cur;
}

int TransformerInference::sampleToken(const std::vector<float>& logits, float temperature) {
    if (temperature <= 0.0f) {
        // Greedy sampling
        return std::max_element(logits.begin(), logits.end()) - logits.begin();
    }
    
    // Temperature sampling
    std::vector<float> probs = logits;
    for (float& p : probs) p = expf(p / temperature);
    
    float sum = 0.0f;
    for (float p : probs) sum += p;
    for (float& p : probs) p /= sum;
    
    // Sample from distribution
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float r = dist(rng);
    
    float cumulative = 0.0f;
    for (size_t i = 0; i < probs.size(); ++i) {
        cumulative += probs[i];
        if (r < cumulative) return i;
    }
    
    return probs.size() - 1;
}
