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
        const QByteArray& embdData = tensorCache.value("token_embd.weight");
        m_tokenEmbed = createTensorFromCache(embdData, GGML_TYPE_F32, {m_nVocab, m_nEmbd});
        if (!m_tokenEmbed) {
            // Try alternative name
            const QByteArray& altData = tensorCache.value("model.embed_tokens.weight");
            m_tokenEmbed = createTensorFromCache(altData, GGML_TYPE_F32, {m_nVocab, m_nEmbd});
        }
        
        // Load output projection: [n_embd, vocab_size]
        const QByteArray& outData = tensorCache.value("output.weight");
        m_outputWeight = createTensorFromCache(outData, GGML_TYPE_F32, {m_nEmbd, m_nVocab});
        if (!m_outputWeight) {
            const QByteArray& altData = tensorCache.value("lm_head.weight");
            m_outputWeight = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd, m_nVocab});
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
        const QByteArray& qData = tensorCache.value(prefix + "attn_q.weight");
        layer.attn_q = createTensorFromCache(qData, GGML_TYPE_F32, {m_nEmbd, m_nEmbd});
        if (!layer.attn_q) {
            const QByteArray& altData = tensorCache.value(altPrefix + "self_attn.q_proj.weight");
            layer.attn_q = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd, m_nEmbd});
        }
        
        const QByteArray& kData = tensorCache.value(prefix + "attn_k.weight");
        layer.attn_k = createTensorFromCache(kData, GGML_TYPE_F32, {m_nEmbd, m_nEmbd});
        if (!layer.attn_k) {
            const QByteArray& altData = tensorCache.value(altPrefix + "self_attn.k_proj.weight");
            layer.attn_k = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd, m_nEmbd});
        }
        
        const QByteArray& vData = tensorCache.value(prefix + "attn_v.weight");
        layer.attn_v = createTensorFromCache(vData, GGML_TYPE_F32, {m_nEmbd, m_nEmbd});
        if (!layer.attn_v) {
            const QByteArray& altData = tensorCache.value(altPrefix + "self_attn.v_proj.weight");
            layer.attn_v = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd, m_nEmbd});
        }
        
        const QByteArray& projData = tensorCache.value(prefix + "attn_output.weight");
        layer.attn_proj = createTensorFromCache(projData, GGML_TYPE_F32, {m_nEmbd, m_nEmbd});
        if (!layer.attn_proj) {
            const QByteArray& altData = tensorCache.value(altPrefix + "self_attn.o_proj.weight");
            layer.attn_proj = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd, m_nEmbd});
        }
        
        // FIX 4: Attention Biases
        const QByteArray& qBiasData = tensorCache.value(prefix + "attn_q.bias");
        layer.attn_q_bias = createTensorFromCache(qBiasData, GGML_TYPE_F32, {m_nEmbd});
        if (!layer.attn_q_bias) {
            const QByteArray& altData = tensorCache.value(altPrefix + "self_attn.q_proj.bias");
            layer.attn_q_bias = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd});
        }
        
        const QByteArray& kBiasData = tensorCache.value(prefix + "attn_k.bias");
        layer.attn_k_bias = createTensorFromCache(kBiasData, GGML_TYPE_F32, {m_nEmbd});
        if (!layer.attn_k_bias) {
            const QByteArray& altData = tensorCache.value(altPrefix + "self_attn.k_proj.bias");
            layer.attn_k_bias = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd});
        }
        
        const QByteArray& vBiasData = tensorCache.value(prefix + "attn_v.bias");
        layer.attn_v_bias = createTensorFromCache(vBiasData, GGML_TYPE_F32, {m_nEmbd});
        if (!layer.attn_v_bias) {
            const QByteArray& altData = tensorCache.value(altPrefix + "self_attn.v_proj.bias");
            layer.attn_v_bias = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd});
        }
        
        const QByteArray& projBiasData = tensorCache.value(prefix + "attn_output.bias");
        layer.attn_output_bias = createTensorFromCache(projBiasData, GGML_TYPE_F32, {m_nEmbd});
        if (!layer.attn_output_bias) {
            const QByteArray& altData = tensorCache.value(altPrefix + "self_attn.o_proj.bias");
            layer.attn_output_bias = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd});
        }
        
        // Layer norm
        const CachedTensorData& ln1Data = tensorCache.value(prefix + "attn_norm.weight");
        layer.ln1_weight = createTensorFromCache(ln1Data.data, ln1Data.ggml_type_id, {m_nEmbd});
        if (!layer.ln1_weight) {
            const CachedTensorData& altData = tensorCache.value(altPrefix + "input_layernorm.weight");
            layer.ln1_weight = createTensorFromCache(altData.data, altData.ggml_type_id, {m_nEmbd});
        }
        
        // MLP
        const QByteArray& mlp1Data = tensorCache.value(prefix + "ffn_up.weight");
        layer.mlp_fc1 = createTensorFromCache(mlp1Data, GGML_TYPE_F32, {m_nEmbd, m_nEmbd * 4});
        if (!layer.mlp_fc1) {
            const QByteArray& altData = tensorCache.value(altPrefix + "mlp.up_proj.weight");
            layer.mlp_fc1 = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd, m_nEmbd * 4});
        }
        
        const QByteArray& mlp2Data = tensorCache.value(prefix + "ffn_down.weight");
        layer.mlp_fc2 = createTensorFromCache(mlp2Data, GGML_TYPE_F32, {m_nEmbd * 4, m_nEmbd});
        if (!layer.mlp_fc2) {
            const QByteArray& altData = tensorCache.value(altPrefix + "mlp.down_proj.weight");
            layer.mlp_fc2 = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd * 4, m_nEmbd});
        }
        
        // FIX 4: MLP Biases
        const QByteArray& mlp1BiasData = tensorCache.value(prefix + "ffn_up.bias");
        layer.mlp_fc1_bias = createTensorFromCache(mlp1BiasData, GGML_TYPE_F32, {m_nEmbd * 4});
        if (!layer.mlp_fc1_bias) {
            const QByteArray& altData = tensorCache.value(altPrefix + "mlp.up_proj.bias");
            layer.mlp_fc1_bias = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd * 4});
        }
        
        const QByteArray& mlp2BiasData = tensorCache.value(prefix + "ffn_down.bias");
        layer.mlp_fc2_bias = createTensorFromCache(mlp2BiasData, GGML_TYPE_F32, {m_nEmbd});
        if (!layer.mlp_fc2_bias) {
            const QByteArray& altData = tensorCache.value(altPrefix + "mlp.down_proj.bias");
            layer.mlp_fc2_bias = createTensorFromCache(altData, GGML_TYPE_F32, {m_nEmbd});
        }
        
        const CachedTensorData& ln2Data = tensorCache.value(prefix + "ffn_norm.weight");
        layer.ln2_weight = createTensorFromCache(ln2Data.data, ln2Data.ggml_type_id, {m_nEmbd});
        if (!layer.ln2_weight) {
            const CachedTensorData& altData = tensorCache.value(altPrefix + "post_attention_layernorm.weight");
            layer.ln2_weight = createTensorFromCache(altData.data, altData.ggml_type_id, {m_nEmbd});
        }
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
    
    // Copy quantized data - now with proper type handling
    size_t expectedSize = ggml_nbytes(tensor);
    if (expectedSize != (size_t)data.size()) {
        qCritical() << QString("Size mismatch for tensor %1: Expected %2, Got %3")
                        .arg(name).arg(expectedSize).arg(data.size());
        return nullptr;
    }
    
    std::memcpy(tensor->data, data.constData(), expectedSize);
    
    return tensor;
}

// New implementation that uses the correct GGML type
struct ggml_tensor* TransformerInference::createTensorFromCache(
    const QByteArray& data,
    int typeId,
    const std::vector<qint64>& dimensions)
{
    // Use the retrieved typeId to cast to the correct ggml_type enum
    enum ggml_type type = (enum ggml_type)typeId;

    struct ggml_tensor* tensor = nullptr;
    
    if (dimensions.size() == 1) {
        tensor = ggml_new_tensor_1d(m_ctx, type, dimensions[0]);
    } else if (dimensions.size() == 2) {
        // Dimensions in ggml are stored in reverse order
        tensor = ggml_new_tensor_2d(m_ctx, type, dimensions[1], dimensions[0]);
    } else if (dimensions.size() == 3) {
        tensor = ggml_new_tensor_3d(m_ctx, type, dimensions[2], dimensions[1], dimensions[0]);
    } else {
        qWarning() << "Unsupported tensor dimension count:" << dimensions.size();
        return nullptr;
    }

    // Critical safety check - ensure data size matches tensor size
    size_t expected_size = ggml_nbytes(tensor);
    if (expected_size != (size_t)data.size()) {
        qCritical() << QString("Size mismatch for tensor: Expected %1 (Type %2), Got %3")
                        .arg(expected_size).arg(typeId).arg(data.size());
        return nullptr;
    }

    // The memcpy is now safe because the tensor type matches the data format
    std::memcpy(tensor->data, data.constData(), expected_size);
    
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
