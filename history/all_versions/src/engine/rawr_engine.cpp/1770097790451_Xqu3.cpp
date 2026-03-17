#include "rawr_engine.h"
#include <cstring>

bool RawrEngine::load(const char* model_path, const char* tok_path, const char* merge_path) {
    if (!model.load(model_path)) return false;
    if (!tokenizer.load(tok_path, merge_path)) return false;
    
    // Load hyperparams
    dim = std::stoi(model.metadata["llama.embedding_length"]);
    n_layers = std::stoi(model.metadata["llama.block_count"]);
    n_heads = std::stoi(model.metadata["llama.attention.head_count"]);
    n_kv_heads = std::stoi(model.metadata["llama.attention.head_count_kv"]);
    vocab_size = std::stoi(model.metadata["tokenizer.ggml.tokens"]);
    hidden_dim = std::stoi(model.metadata["llama.feed_forward_length"]);
    head_dim = dim / n_heads;
    
    // Load embeddings
    auto* emb = model.getTensor("token_embd.weight");
    tok_embeddings = (float*)emb->data;
    
    // Load output norm
    auto* on = model.getTensor("output_norm.weight");
    output_norm = (float*)on->data;
    
    // Load output weight (lm_head)
    output_weight = model.getTensor("output.weight")->data;
    
    // Initialize layers
    for (int i = 0; i < n_layers; i++) {
        auto layer = std::make_unique<TransformerLayer>(dim, n_heads, n_kv_heads, hidden_dim);
        
        std::string pre = "blk." + std::to_string(i) + ".";
        
        auto* an = model.getTensor((pre + "attn_norm.weight").c_str());
        layer->attn_norm = (float*)an->data;
        
        auto* wq = model.getTensor((pre + "attn_q.weight").c_str());
        layer->wq = wq->data;
        layer->wq_type = wq->type;
        
        auto* wk = model.getTensor((pre + "attn_k.weight").c_str());
        layer->wk = wk->data;
        
        auto* wv = model.getTensor((pre + "attn_v.weight").c_str());
        layer->wv = wv->data;
        
        auto* wo = model.getTensor((pre + "attn_output.weight").c_str());
        layer->wo = wo->data;
        
        auto* fn = model.getTensor((pre + "ffn_norm.weight").c_str());
        layer->ffn_norm = (float*)fn->data;
        
        auto* w1 = model.getTensor((pre + "ffn_gate.weight").c_str());
        layer->w1 = w1->data;
        layer->ffn_type = w1->type;
        
        auto* w2 = model.getTensor((pre + "ffn_down.weight").c_str());
        layer->w2 = w2->data;
        
        auto* w3 = model.getTensor((pre + "ffn_up.weight").c_str());
        layer->w3 = w3->data;
        
        layers.push_back(std::move(layer));
    }
    
    return true;
}

std::string RawrEngine::generate(const std::string& prompt, int max_tokens, 
                        std::function<void(const char*)> callback) {
    auto tokens = tokenizer.encode(prompt);
    int pos = tokens.size();
    
    std::string output;
    std::vector<float> x(dim);
    
    for (int i = 0; i < max_tokens; i++) {
        // Get token embedding
        int tok = tokens.back();
        // Simple F32 embedding lookup
        memcpy(x.data(), tok_embeddings + tok * dim, dim * sizeof(float));
        
        // Forward through layers
        for (auto& layer : layers) {
            layer->forward(x.data(), pos, pos + 1);
        }
        
        // Final norm
        InferenceKernels::rmsnorm_avx512(x.data(), x.data(), output_norm, dim);
        
        // Output projection (logits)
        std::vector<float> logits(vocab_size);
        // Assuming output_weight is Q4_0 for this example flow, though variable type is best
        InferenceKernels::matmul_q4_0_fused(x.data(), (block_q4_0*)output_weight, 
                            logits.data(), 1, vocab_size, dim);
        
        // Sample
        int next_tok = sampler.sample(logits.data(), vocab_size);
        tokens.push_back(next_tok);
        pos++;
        
        // Decode and output
        auto piece = tokenizer.decode({next_tok});
        output += piece;
        if (callback) callback(piece.c_str());
        
        // Stop on EOS
        if (next_tok == tokenizer.encoder["<|endoftext|>"]) break;
    }
    
    return output;
}
