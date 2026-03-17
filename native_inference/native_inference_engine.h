#pragma once
#include "native_gguf_loader.h"
#include "native_tokenizer.hpp"
#include <memory>
#include <vector>
#include <string>

// Native Inference Engine - Complete replacement for ggml
class NativeInferenceEngine {
public:
    NativeInferenceEngine();
    ~NativeInferenceEngine();

    // Initialize the engine
    bool Initialize(const std::string& model_path);

    // Load model from GGUF
    bool LoadModel(const std::string& gguf_path);

    // Generate text
    std::string Generate(const std::string& prompt, size_t max_tokens = 100, float temperature = 1.0f);

    // Tokenize text
    std::vector<uint32_t> Tokenize(const std::string& text) const;

    // Detokenize
    std::string Detokenize(const std::vector<uint32_t>& tokens) const;

    // Get model info
    size_t GetVocabSize() const;
    size_t GetContextLength() const;
    size_t GetEmbeddingDim() const;

private:
    // Core components
    std::unique_ptr<NativeGGUFLoader> loader_;
    std::unique_ptr<NativeBPETokenizer> tokenizer_;
    std::unique_ptr<NativeSpeculativeDecoder> speculative_decoder_;
    std::unique_ptr<NativeKVCache> kv_cache_;

    // Model data
    std::vector<std::vector<uint8_t>> tensor_data_;
    std::unordered_map<std::string, size_t> tensor_map_;

    // Model parameters
    size_t vocab_size_;
    size_t context_length_;
    size_t embedding_dim_;
    size_t num_layers_;
    size_t num_heads_;
    size_t head_dim_;

    // Inference state
    std::vector<uint32_t> current_context_;
    size_t current_seq_pos_;

    // Forward pass implementation
    std::vector<float> ForwardPass(const std::vector<uint32_t>& tokens);

    // Matrix operations
    void MatMul(const std::vector<float>& a, const std::vector<float>& b,
                std::vector<float>& c, size_t m, size_t k, size_t n);

    // Quantization operations
    void DequantizeQ4_0(const std::vector<uint8_t>& quantized,
                       std::vector<float>& output, size_t elements);

    void DequantizeQ8_0(const std::vector<uint8_t>& quantized,
                       std::vector<float>& output, size_t elements);

    // Attention mechanism
    void MultiHeadAttention(const std::vector<float>& query,
                           const std::vector<float>& key,
                           const std::vector<float>& value,
                           std::vector<float>& output,
                           size_t seq_len, size_t head_dim);

    // Transformer block
    void TransformerBlock(const std::vector<float>& input,
                         std::vector<float>& output,
                         size_t layer_idx);

    // Load tensor by name
    bool LoadTensor(const std::string& name, std::vector<uint8_t>& data);
    bool LoadTensorAsFloat(const std::string& name, std::vector<float>& data);
};

// Helper function for sampling tokens
uint32_t SampleToken(const std::vector<float>& logits, float temperature);
extern "C" {
    // Quantization kernels
    void NativeQuantizeQ4_0(const float* input, uint8_t* output, size_t elements, float* scales);
    void NativeDequantizeQ4_0(const uint8_t* input, float* output, size_t elements, const float* scales);
    void NativeQuantizeQ8_0(const float* input, int8_t* output, size_t elements, float* scales);
    void NativeDequantizeQ8_0(const int8_t* input, float* output, size_t elements, const float* scales);

    // Matrix multiplication
    void NativeMatMulCPU(const float* a, const float* b, float* c, size_t m, size_t k, size_t n);
    void NativeMatMulAVX(const float* a, const float* b, float* c, size_t m, size_t k, size_t n);

    // Tokenizer
    bool NativeTokenizerInit(const char* vocab_data, size_t vocab_size,
                            const char* merge_rules, size_t merge_count);
    size_t NativeEncodeBPE(const char* text, size_t text_len, uint32_t* output, size_t max_tokens);
    size_t NativeDecodeBPE(const uint32_t* tokens, size_t token_count, char* output, size_t buffer_size);
}