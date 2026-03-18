#include "rawr_engine.h"
#include <cstring>
#include <algorithm>
#include <vector>
#include <cmath>

bool RawrEngine::load(const char* model_path, const char* tok_path, const char* merge_path) {
    if (!model.load(model_path)) return false;
    if (!tokenizer.load(tok_path, merge_path)) return false;
    
    auto read_meta_int = [this](const char* key, int fallback) -> int {
        auto it = model.metadata.find(key);
        if (it == model.metadata.end() || it->second.empty()) {
            return fallback;
        }
        try {
            int v = std::stoi(it->second);
            return (v > 0) ? v : fallback;
        } catch (...) {
            return fallback;
        }
    };

    // Load hyperparams
    dim = read_meta_int("llama.embedding_length", 4096);
    n_layers = read_meta_int("llama.block_count", 32);
    n_heads = read_meta_int("llama.attention.head_count", 32);
    n_kv_heads = read_meta_int("llama.attention.head_count_kv", n_heads);
    vocab_size = read_meta_int("tokenizer.ggml.tokens", 32000);
    hidden_dim = read_meta_int("llama.feed_forward_length", 11008);
    n_kv_heads = std::max(1, std::min(n_kv_heads, n_heads));
    head_dim = std::max(1, dim / std::max(1, n_heads));
    
    // Load embeddings
    auto* emb = model.getTensor("token_embd.weight");
    if (!emb || !emb->data) {
        return false;
    }
    tok_embeddings = (float*)emb->data;
    
    // Load output norm
    auto* on = model.getTensor("output_norm.weight");
    if (!on || !on->data) {
        return false;
    }
    output_norm = (float*)on->data;
    
    // Load output weight (lm_head)
    auto* ow = model.getTensor("output.weight");
    if (!ow || !ow->data) {
        return false;
    }
    output_weight = ow->data;
    output_weight_type = ow->type;
    
    // Initialize layers
    layers.clear();
    layers.reserve(std::max(0, n_layers));
    for (int i = 0; i < n_layers; i++) {
        auto layer = std::make_unique<TransformerLayer>(dim, n_heads, n_kv_heads, hidden_dim);
        
        std::string pre = "blk." + std::to_string(i) + ".";
        
        auto* an = model.getTensor((pre + "attn_norm.weight").c_str());
        if (!an || !an->data) return false;
        layer->attn_norm = (float*)an->data;
        
        auto* wq = model.getTensor((pre + "attn_q.weight").c_str());
        if (!wq || !wq->data) return false;
        layer->wq = wq->data;
        layer->wq_type = wq->type;
        
        auto* wk = model.getTensor((pre + "attn_k.weight").c_str());
        if (!wk || !wk->data) return false;
        layer->wk = wk->data;
        
        auto* wv = model.getTensor((pre + "attn_v.weight").c_str());
        if (!wv || !wv->data) return false;
        layer->wv = wv->data;
        
        auto* wo = model.getTensor((pre + "attn_output.weight").c_str());
        if (!wo || !wo->data) return false;
        layer->wo = wo->data;
        
        auto* fn = model.getTensor((pre + "ffn_norm.weight").c_str());
        if (!fn || !fn->data) return false;
        layer->ffn_norm = (float*)fn->data;
        
        auto* w1 = model.getTensor((pre + "ffn_gate.weight").c_str());
        if (!w1 || !w1->data) return false;
        layer->w1 = w1->data;
        layer->ffn_type = w1->type;
        
        auto* w2 = model.getTensor((pre + "ffn_down.weight").c_str());
        if (!w2 || !w2->data) return false;
        layer->w2 = w2->data;
        
        auto* w3 = model.getTensor((pre + "ffn_up.weight").c_str());
        if (!w3 || !w3->data) return false;
        layer->w3 = w3->data;
        
        layers.push_back(std::move(layer));
    }
    
    return true;
}

std::string RawrEngine::generate(const std::string& prompt, int max_tokens, 
                        std::function<void(const char*)> callback) {
    if (max_tokens <= 0 || layers.empty() || !tok_embeddings || !output_norm || !output_weight) {
        return {};
    }
    max_tokens = std::min(max_tokens, 8192);

    auto tokens = tokenizer.encode(prompt);
    if (tokens.empty()) {
        tokens.push_back(0);
    }
    const int ctx_limit = std::max(1, layers.front()->max_seq_len);
    if (static_cast<int>(tokens.size()) > ctx_limit) {
        tokens.erase(tokens.begin(), tokens.end() - ctx_limit);
    }
    
    std::string output;
    output.reserve(static_cast<size_t>(max_tokens) * 4);
    std::vector<float> x(dim);
    std::vector<float> logits(vocab_size);
    int repeat_count = 0;
    int prev_token = -1;

    // Reset layer-local KV state for a fresh generation run.
    for (auto& layer : layers) {
        layer->cache_pos = 0;
        layer->total_tokens_seen = 0;
    }

    // Full prompt prefill: run each prompt token through the transformer to seed KV cache.
    for (size_t t = 0; t < tokens.size(); ++t) {
        int tok = tokens[t];
        tok = std::max(0, std::min(tok, vocab_size - 1));
        memcpy(x.data(), tok_embeddings + static_cast<size_t>(tok) * dim, static_cast<size_t>(dim) * sizeof(float));
        for (auto& layer : layers) {
            layer->forward(x.data(), static_cast<int>(t), static_cast<int>(t) + 1);
        }
    }

    for (int i = 0; i < max_tokens; i++) {
        // Final norm
        InferenceKernels::rmsnorm_avx512(x.data(), x.data(), output_norm, dim);
        
        // Output projection (logits) with quantized-or-f32 dispatch.
        if (output_weight_type == GGML_TYPE_Q4_0) {
            InferenceKernels::matmul_q4_0_fused(
                x.data(), (block_q4_0*)output_weight, logits.data(), 1, vocab_size, dim);
        } else {
            const float* w = static_cast<const float*>(output_weight);
            for (int v = 0; v < vocab_size; ++v) {
                float acc = 0.0f;
                const float* row = w + static_cast<size_t>(v) * dim;
                for (int d = 0; d < dim; ++d) {
                    acc += row[d] * x[d];
                }
                logits[v] = acc;
            }
        }

        bool has_finite_logit = false;
        for (float v : logits) {
            if (std::isfinite(v)) {
                has_finite_logit = true;
                break;
            }
        }
        if (!has_finite_logit) {
            break;
        }
        
        // Sample
        int next_tok = sampler.sample(logits.data(), vocab_size);
        next_tok = std::max(0, std::min(next_tok, vocab_size - 1));
        tokens.push_back(next_tok);
        
        // Decode and output
        auto piece = tokenizer.decode({next_tok});
        output += piece;
        if (callback) {
            try {
                callback(piece.c_str());
            } catch (...) {
                // Keep generation alive even if UI callback throws.
            }
        }
        
        // Stop on EOS, if present in tokenizer.
        auto eos_it = tokenizer.encoder.find("<|endoftext|>");
        if (eos_it != tokenizer.encoder.end() && next_tok == eos_it->second) {
            break;
        }

        if (next_tok == prev_token) {
            repeat_count++;
        } else {
            repeat_count = 0;
        }
        prev_token = next_tok;
        if (repeat_count >= 256) {
            break;
        }

        // Advance one-step decode: embed sampled token and run one transformer step.
        const int pos = static_cast<int>(tokens.size()) - 1;
        memcpy(x.data(), tok_embeddings + static_cast<size_t>(next_tok) * dim, static_cast<size_t>(dim) * sizeof(float));
        for (auto& layer : layers) {
            layer->forward(x.data(), pos, pos + 1);
        }
    }
    
    return output;
}

// Registration function for linker
void register_rawr_inference() {
    // Engine registration - wired into runtime
}
