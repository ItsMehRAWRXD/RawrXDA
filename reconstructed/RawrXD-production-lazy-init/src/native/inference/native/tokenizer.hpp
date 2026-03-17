#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

// Native BPE tokenizer implementation
class NativeBPETokenizer {
public:
    NativeBPETokenizer();
    ~NativeBPETokenizer();

    // Initialize with vocab and merges
    bool Initialize(const std::unordered_map<std::string, uint32_t>& vocab,
                   const std::vector<std::pair<std::string, std::string>>& merges);

    // Encode text to token IDs
    std::vector<uint32_t> Encode(const std::string& text) const;

    // Decode token IDs to text
    std::string Decode(const std::vector<uint32_t>& tokens) const;

    // Get vocabulary size
    size_t GetVocabSize() const;

    // Get special token IDs
    uint32_t GetBOSId() const { return bos_id_; }
    uint32_t GetEOSId() const { return eos_id_; }
    uint32_t GetUNKId() const { return unk_id_; }

private:
    // BPE merge table: pair -> new token
    std::unordered_map<uint64_t, uint32_t> merge_table_;

    // Reverse vocab: token id -> string
    std::unordered_map<uint32_t, std::string> reverse_vocab_;

    // Byte-to-token mapping for initial tokenization
    std::vector<uint32_t> byte_tokens_;

    // Special tokens
    uint32_t bos_id_;
    uint32_t eos_id_;
    uint32_t unk_id_;

    // Helper functions
    std::vector<std::string> BytePairEncode(const std::string& text) const;
    uint64_t MakePairKey(uint32_t a, uint32_t b) const;
    std::pair<uint32_t, uint32_t> SplitPairKey(uint64_t key) const;
    std::vector<uint32_t> ApplyMerges(const std::vector<uint32_t>& tokens) const;
};

// Native speculative decoder
class NativeSpeculativeDecoder {
public:
    NativeSpeculativeDecoder();
    ~NativeSpeculativeDecoder();

    // Initialize with draft and target models
    bool Initialize(void* draft_model, void* target_model);

    // Generate tokens with speculation
    std::vector<uint32_t> Generate(const std::vector<uint32_t>& prompt,
                                  size_t max_tokens,
                                  float temperature = 1.0f);

    // Set speculation parameters
    void SetSpeculationParams(size_t max_draft_tokens = 4,
                             float acceptance_threshold = 0.8f);

private:
    void* draft_model_;
    void* target_model_;
    size_t max_draft_tokens_;
    float acceptance_threshold_;

    // Parallel verification using std::thread
    std::vector<uint32_t> VerifyDraftTokens(const std::vector<uint32_t>& draft_tokens,
                                           const std::vector<uint32_t>& target_tokens);
};

// Native KV cache with sliding window and SVD compression
class NativeKVCache {
public:
    NativeKVCache();
    ~NativeKVCache();

    // Initialize cache
    bool Initialize(size_t max_seq_len, size_t head_dim, size_t num_heads, size_t window_size = 512);

    // Add KV pair to cache
    void AddKV(size_t layer, size_t head, const std::vector<float>& k, const std::vector<float>& v);

    // Get KV from cache with sliding window
    bool GetKV(size_t layer, size_t head, size_t seq_pos,
               std::vector<float>& k_out, std::vector<float>& v_out) const;

    // Compress cache using SVD
    void Compress();

    // Clear cache
    void Clear();

    // Get memory usage
    size_t GetMemoryUsage() const;

private:
    struct KVEntry {
        std::vector<float> k;
        std::vector<float> v;
        size_t seq_pos;
    };

    struct CacheLayer {
        std::vector<std::vector<KVEntry>> heads;  // [head][entries]
    };

    std::vector<CacheLayer> layers_;
    size_t max_seq_len_;
    size_t head_dim_;
    size_t num_heads_;
    size_t window_size_;
    size_t current_seq_pos_;

    // SVD compression helpers
    void ApplySVDCompression(std::vector<float>& data, size_t rank = 32);
    std::vector<float> SVDDecompress(const std::vector<float>& compressed_data) const;
};