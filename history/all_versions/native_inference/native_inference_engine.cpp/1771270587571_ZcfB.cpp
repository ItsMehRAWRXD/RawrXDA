#include "native_inference_engine.h"
#include <algorithm>
#include <random>
#include <iostream>
#include <cstring>

NativeInferenceEngine::NativeInferenceEngine()
    : vocab_size_(0), context_length_(0), embedding_dim_(0),
      num_layers_(0), num_heads_(0), head_dim_(0), current_seq_pos_(0) {
}

NativeInferenceEngine::~NativeInferenceEngine() {
}

bool NativeInferenceEngine::Initialize(const std::string& model_path) {
    loader_ = std::make_unique<NativeGGUFLoader>();
    tokenizer_ = std::make_unique<NativeBPETokenizer>();
    speculative_decoder_ = std::make_unique<NativeSpeculativeDecoder>();
    kv_cache_ = std::make_unique<NativeKVCache>();

    if (!LoadModel(model_path)) {
        return false;
    }

    // Initialize KV cache
    kv_cache_->Initialize(context_length_, head_dim_, num_heads_);

    return true;
}

bool NativeInferenceEngine::LoadModel(const std::string& gguf_path) {
    if (!loader_->Open(gguf_path)) {
        std::cerr << "Failed to open GGUF file: " << gguf_path << std::endl;
        return false;
    }

    // Parse metadata
    auto& metadata_map = loader_->GetMetadata();
    
    // Extract key parameters from metadata
    // auto get_uint64 = [&](const std::string& key) -> uint64_t {
    //     auto it = metadata_map.find(key);
    //     if (it != metadata_map.end() && it->second.type == static_cast<uint32_t>(GGUFUtils::MetadataType::UINT64)) {
    //         return std::get<uint64_t>(it->second.value);
    //     }
    //     return 0;
    // };
    
    vocab_size_ = 32000; // get_uint64("llama.vocab_size");
    context_length_ = 2048; // get_uint64("llama.context_length");
    embedding_dim_ = 4096; // get_uint64("llama.embedding_length");
    num_layers_ = 32; // get_uint64("llama.block_count");

    // Estimate other parameters
    num_heads_ = 32;  // Common default
    head_dim_ = embedding_dim_ / num_heads_;

    // Load all tensors
    auto& tensors = loader_->GetTensors();
    tensor_data_.resize(tensors.size());
    tensor_map_.clear();

    for (size_t i = 0; i < tensors.size(); ++i) {
        const auto& tensor = tensors[i];
        tensor_data_[i].resize(tensor.size_bytes);
        if (!loader_->LoadTensorData(tensor.name, tensor_data_[i].data(), tensor_data_[i].size())) {
            std::cerr << "Failed to load tensor: " << tensor.name << std::endl;
            return false;
        }
        tensor_map_[tensor.name] = i;
    }

    // Initialize tokenizer (simplified)
    std::unordered_map<std::string, uint32_t> vocab;
    std::vector<std::pair<std::string, std::string>> merges;

    // Add byte tokens
    for (uint32_t i = 0; i < 256; ++i) {
        std::string byte_str(1, static_cast<char>(i));
        vocab[byte_str] = i;
    }

    // Add some common merges (simplified)
    merges.emplace_back("a", "b");
    merges.emplace_back("c", "d");

    tokenizer_->Initialize(vocab, merges);

    return true;
}

std::string NativeInferenceEngine::Generate(const std::string& prompt, size_t max_tokens, float temperature) {
    auto tokens = Tokenize(prompt);
    current_context_ = tokens;
    current_seq_pos_ = tokens.size();

    // Generate tokens
    for (size_t i = 0; i < max_tokens; ++i) {
        // Forward pass
        auto logits = ForwardPass(current_context_);

        // Sample next token
        uint32_t next_token = SampleToken(logits, temperature);
        current_context_.push_back(next_token);

        // Check for EOS
        if (next_token == tokenizer_->GetEOSId()) {
            break;
        }
    }

    return Detokenize(current_context_);
}

std::vector<uint32_t> NativeInferenceEngine::Tokenize(const std::string& text) const {
    return tokenizer_->Encode(text);
}

std::string NativeInferenceEngine::Detokenize(const std::vector<uint32_t>& tokens) const {
    return tokenizer_->Decode(tokens);
}

size_t NativeInferenceEngine::GetVocabSize() const {
    return vocab_size_;
}

size_t NativeInferenceEngine::GetContextLength() const {
    return context_length_;
}

size_t NativeInferenceEngine::GetEmbeddingDim() const {
    return embedding_dim_;
}

std::vector<float> NativeInferenceEngine::ForwardPass(const std::vector<uint32_t>& tokens) {
    size_t seq_len = tokens.size();

    // Get embeddings (simplified)
    std::vector<float> embeddings(seq_len * embedding_dim_);

    // For each token, look up embedding
    for (size_t i = 0; i < seq_len; ++i) {
        uint32_t token = tokens[i];
        // Simplified: use token ID as embedding index
        for (size_t j = 0; j < embedding_dim_; ++j) {
            embeddings[i * embedding_dim_ + j] = static_cast<float>(token % 256) / 255.0f;
        }
    }

    // Apply transformer layers
    std::vector<float> hidden = embeddings;
    for (size_t layer = 0; layer < num_layers_; ++layer) {
        std::vector<float> layer_output(seq_len * embedding_dim_);
        TransformerBlock(hidden, layer_output, layer);
        hidden = layer_output;
    }

    // Final linear layer to vocab
    std::vector<float> logits(vocab_size_, 0.0f);

    // Simplified: average pooling to single vector, then linear
    std::vector<float> pooled(embedding_dim_, 0.0f);
    for (size_t i = 0; i < seq_len; ++i) {
        for (size_t j = 0; j < embedding_dim_; ++j) {
            pooled[j] += hidden[i * embedding_dim_ + j];
        }
    }
    for (float& val : pooled) {
        val /= static_cast<float>(seq_len);
    }

    // Linear projection to vocab (simplified)
    for (size_t i = 0; i < vocab_size_; ++i) {
        logits[i] = pooled[i % embedding_dim_] * 0.1f;
    }

    return logits;
}

void NativeInferenceEngine::MatMul(const std::vector<float>& a, const std::vector<float>& b,
                                   std::vector<float>& c, size_t m, size_t k, size_t n) {
    // Use AVX-optimized matrix multiplication
    NativeMatMulAVX(a.data(), b.data(), c.data(), m, k, n);
}

void NativeInferenceEngine::DequantizeQ4_0(const std::vector<uint8_t>& quantized,
                                          std::vector<float>& output, size_t elements) {
    // Calculate number of blocks
    size_t blocks = (elements + 31) / 32;
    std::vector<float> scales(blocks);

    // For now, assume scales are stored separately or computed
    for (size_t i = 0; i < blocks; ++i) {
        scales[i] = 1.0f;  // Placeholder
    }

    NativeDequantizeQ4_0(quantized.data(), output.data(), elements, scales.data());
}

void NativeInferenceEngine::DequantizeQ8_0(const std::vector<uint8_t>& quantized,
                                          std::vector<float>& output, size_t elements) {
    // Similar to Q4_0
    size_t blocks = (elements + 31) / 32;
    std::vector<float> scales(blocks, 1.0f);

    NativeDequantizeQ8_0(reinterpret_cast<const int8_t*>(quantized.data()),
                        output.data(), elements, scales.data());
}

void NativeInferenceEngine::MultiHeadAttention(const std::vector<float>& query,
                                               const std::vector<float>& key,
                                               const std::vector<float>& value,
                                               std::vector<float>& output,
                                               size_t seq_len, size_t head_dim) {
    // Simplified attention
    size_t total_dim = seq_len * head_dim;

    // QK^T
    std::vector<float> qk_scores(seq_len * seq_len, 0.0f);
    MatMul(query, key, qk_scores, seq_len, head_dim, seq_len);

    // Scale by sqrt(head_dim)
    float scale = 1.0f / std::sqrt(static_cast<float>(head_dim));
    for (float& score : qk_scores) {
        score *= scale;
    }

    // Softmax (simplified)
    for (size_t i = 0; i < seq_len; ++i) {
        float max_val = *std::max_element(qk_scores.begin() + i * seq_len,
                                         qk_scores.begin() + (i + 1) * seq_len);
        float sum = 0.0f;
        for (size_t j = 0; j < seq_len; ++j) {
            qk_scores[i * seq_len + j] = std::exp(qk_scores[i * seq_len + j] - max_val);
            sum += qk_scores[i * seq_len + j];
        }
        for (size_t j = 0; j < seq_len; ++j) {
            qk_scores[i * seq_len + j] /= sum;
        }
    }

    // Attention * V
    MatMul(qk_scores, value, output, seq_len, seq_len, head_dim);
}

void NativeInferenceEngine::TransformerBlock(const std::vector<float>& input,
                                            std::vector<float>& output,
                                            size_t layer_idx) {
    size_t seq_len = input.size() / embedding_dim_;

    // Self-attention
    std::vector<float> attn_output(seq_len * embedding_dim_);
    MultiHeadAttention(input, input, input, attn_output, seq_len, head_dim_);

    // Residual + Layer norm (simplified)
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] + attn_output[i];
        // Simple normalization
        output[i] /= std::sqrt(output[i] * output[i] + 1.0f);
    }

    // Feed-forward (simplified)
    std::vector<float> ff_input = output;
    std::vector<float> ff_hidden(seq_len * embedding_dim_ * 4);  // 4x expansion

    // First linear layer
    std::vector<float> weights1(embedding_dim_ * embedding_dim_ * 4, 0.1f);
    MatMul(ff_input, weights1, ff_hidden, seq_len, embedding_dim_, embedding_dim_ * 4);

    // SiLU activation (simplified)
    for (float& val : ff_hidden) {
        val = val / (1.0f + std::exp(-val));
    }

    // Second linear layer
    std::vector<float> weights2(embedding_dim_ * 4 * embedding_dim_, 0.1f);
    MatMul(ff_hidden, weights2, output, seq_len, embedding_dim_ * 4, embedding_dim_);

    // Residual
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] += ff_input[i];
    }
}

bool NativeInferenceEngine::LoadTensor(const std::string& name, std::vector<uint8_t>& data) {
    auto it = tensor_map_.find(name);
    if (it == tensor_map_.end()) return false;

    data = tensor_data_[it->second];
    return true;
}

bool NativeInferenceEngine::LoadTensorAsFloat(const std::string& name, std::vector<float>& data) {
    std::vector<uint8_t> raw_data;
    if (!LoadTensor(name, raw_data)) return false;

    // Get tensor info to determine quantization type
    auto& tensor_info = loader_->GetTensors();
    auto it = tensor_map_.find(name);
    if (it == tensor_map_.end()) return false;

    const auto& tensor = tensor_info[it->second];
    size_t elements = 1;
    for (uint64_t dim : tensor.dims) {
        elements *= dim;
    }

    data.resize(elements);

    // Dequantize based on type
    switch (tensor.type) {
        case QuantType::F32:
            std::memcpy(data.data(), raw_data.data(), raw_data.size());
            break;
        case QuantType::Q4_0:
            DequantizeQ4_0(raw_data, data, elements);
            break;
        case QuantType::Q8_0:
            DequantizeQ8_0(raw_data, data, elements);
            break;
        default:
            return false;
    }

    return true;
}

// Helper function to sample token from logits
uint32_t SampleToken(const std::vector<float>& logits, float temperature) {
    std::vector<float> probs = logits;

    // Apply temperature
    if (temperature != 1.0f) {
        for (float& prob : probs) {
            prob /= temperature;
        }
    }

    // Softmax
    float max_logit = *std::max_element(probs.begin(), probs.end());
    float sum = 0.0f;
    for (float& prob : probs) {
        prob = std::exp(prob - max_logit);
        sum += prob;
    }
    for (float& prob : probs) {
        prob /= sum;
    }

    // Sample
    std::random_device rd;
    std::mt19937 gen(rd());
    std::discrete_distribution<> dist(probs.begin(), probs.end());

    return dist(gen);
}