dddddddddddddddddddd#include "native_tokenizer.hpp"
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <cmath>
#include <iostream>

// Native BPE Tokenizer Implementation

NativeBPETokenizer::NativeBPETokenizer()
    : bos_id_(0), eos_id_(1), unk_id_(2)
{
    // Initialize byte tokens (0-255)
    byte_tokens_.resize(256);
    for (uint32_t i = 0; i < 256; ++i) {
        byte_tokens_[i] = i;
    }
}

NativeBPETokenizer::~NativeBPETokenizer() {
}

bool NativeBPETokenizer::Initialize(const std::unordered_map<std::string, uint32_t>& vocab,
                                   const std::vector<std::pair<std::string, std::string>>& merges) {
    // Build reverse vocab
    reverse_vocab_.clear();
    for (const auto& pair : vocab) {
        reverse_vocab_[pair.second] = pair.first;
    }

    // Build merge table
    merge_table_.clear();
    for (size_t i = 0; i < merges.size(); ++i) {
        const auto& merge = merges[i];
        // Find token IDs for the pair
        auto it1 = vocab.find(merge.first);
        auto it2 = vocab.find(merge.second);
        if (it1 != vocab.end() && it2 != vocab.end()) {
            uint64_t key = MakePairKey(it1->second, it2->second);
            uint32_t new_token = static_cast<uint32_t>(256 + i);  // Start after byte tokens
            merge_table_[key] = new_token;
        }
    }

    return true;
}

std::vector<uint32_t> NativeBPETokenizer::Encode(const std::string& text) const {
    // Start with byte-level tokenization
    std::vector<uint32_t> tokens;
    tokens.reserve(text.size());

    for (char c : text) {
        uint8_t byte = static_cast<uint8_t>(c);
        tokens.push_back(byte_tokens_[byte]);
    }

    // Apply BPE merges
    return ApplyMerges(tokens);
}

std::string NativeBPETokenizer::Decode(const std::vector<uint32_t>& tokens) const {
    std::string result;
    result.reserve(tokens.size());  // Rough estimate

    for (uint32_t token : tokens) {
        if (token < 256) {
            // Byte token
            result.push_back(static_cast<char>(token));
        } else {
            // Look up in reverse vocab
            auto it = reverse_vocab_.find(token);
            if (it != reverse_vocab_.end()) {
                result += it->second;
            } else {
                result.push_back('?');  // Unknown token
            }
        }
    }

    return result;
}

size_t NativeBPETokenizer::GetVocabSize() const {
    return reverse_vocab_.size();
}

std::vector<uint32_t> NativeBPETokenizer::ApplyMerges(const std::vector<uint32_t>& tokens) const {
    std::vector<uint32_t> result = tokens;

    // Simple BPE application - repeatedly merge most frequent pairs
    bool changed = true;
    while (changed) {
        changed = false;

        for (size_t i = 0; i + 1 < result.size(); ++i) {
            uint64_t pair_key = MakePairKey(result[i], result[i + 1]);
            auto it = merge_table_.find(pair_key);
            if (it != merge_table_.end()) {
                // Merge the pair
                result[i] = it->second;
                result.erase(result.begin() + i + 1);
                changed = true;
                break;  // Restart from beginning
            }
        }
    }

    return result;
}

uint64_t NativeBPETokenizer::MakePairKey(uint32_t a, uint32_t b) const {
    return (static_cast<uint64_t>(a) << 32) | b;
}

std::pair<uint32_t, uint32_t> NativeBPETokenizer::SplitPairKey(uint64_t key) const {
    return {static_cast<uint32_t>(key >> 32), static_cast<uint32_t>(key & 0xFFFFFFFF)};
}

// Native Speculative Decoder Implementation

NativeSpeculativeDecoder::NativeSpeculativeDecoder()
    : draft_model_(nullptr), target_model_(nullptr),
      max_draft_tokens_(4), acceptance_threshold_(0.8f) {
}

NativeSpeculativeDecoder::~NativeSpeculativeDecoder() {
}

bool NativeSpeculativeDecoder::Initialize(void* draft_model, void* target_model) {
    draft_model_ = draft_model;
    target_model_ = target_model;
    return draft_model_ && target_model_;
}

std::vector<uint32_t> NativeSpeculativeDecoder::Generate(const std::vector<uint32_t>& prompt,
                                                        size_t max_tokens,
                                                        float temperature) {
    std::vector<uint32_t> result = prompt;
    result.reserve(prompt.size() + max_tokens);

    for (size_t i = 0; i < max_tokens; ++i) {
        // Generate draft tokens with draft model
        std::vector<uint32_t> draft_tokens;
        draft_tokens.reserve(max_draft_tokens_);

        // Simplified: generate a few tokens
        for (size_t j = 0; j < max_draft_tokens_; ++j) {
            // In real implementation, call draft model
            draft_tokens.push_back(100 + (rand() % 100));  // Placeholder
        }

        // Verify with target model using parallel threads
        auto verified_tokens = VerifyDraftTokens(draft_tokens, result);

        // Add accepted tokens
        for (uint32_t token : verified_tokens) {
            result.push_back(token);
        }

        // If no tokens accepted, generate one with target model
        if (verified_tokens.empty()) {
            // In real implementation, call target model for one token
            result.push_back(200 + (rand() % 100));  // Placeholder
        }
    }

    return result;
}

void NativeSpeculativeDecoder::SetSpeculationParams(size_t max_draft_tokens, float acceptance_threshold) {
    max_draft_tokens_ = max_draft_tokens;
    acceptance_threshold_ = acceptance_threshold;
}

std::vector<uint32_t> NativeSpeculativeDecoder::VerifyDraftTokens(const std::vector<uint32_t>& draft_tokens,
                                                                 const std::vector<uint32_t>& context) {
    std::vector<uint32_t> accepted;
    std::mutex result_mutex;

    // Use parallel verification with std::thread
    std::vector<std::thread> threads;
    std::atomic<size_t> next_token{0};

    auto verify_worker = [&]() {
        while (true) {
            size_t token_idx = next_token.fetch_add(1);
            if (token_idx >= draft_tokens.size()) break;

            // In real implementation, compute probability with target model
            float prob = 0.9f - (token_idx * 0.1f);  // Placeholder decreasing probability

            if (prob >= acceptance_threshold_) {
                std::lock_guard<std::mutex> lock(result_mutex);
                accepted.push_back(draft_tokens[token_idx]);
            } else {
                // Stop at first rejection
                break;
            }
        }
    };

    // Launch worker threads
    size_t num_threads = std::min(draft_tokens.size(), static_cast<size_t>(4));
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(verify_worker);
    }

    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }

    return accepted;
}

// Native KV Cache Implementation

NativeKVCache::NativeKVCache()
    : max_seq_len_(0), head_dim_(0), num_heads_(0), window_size_(512), current_seq_pos_(0) {
}

NativeKVCache::~NativeKVCache() {
}

bool NativeKVCache::Initialize(size_t max_seq_len, size_t head_dim, size_t num_heads, size_t window_size) {
    max_seq_len_ = max_seq_len;
    head_dim_ = head_dim;
    num_heads_ = num_heads;
    window_size_ = window_size;
    current_seq_pos_ = 0;

    layers_.resize(1);  // Assume single layer for simplicity
    layers_[0].heads.resize(num_heads_);

    return true;
}

void NativeKVCache::AddKV(size_t layer, size_t head, const std::vector<float>& k, const std::vector<float>& v) {
    if (layer >= layers_.size() || head >= layers_[layer].heads.size()) return;

    KVEntry entry;
    entry.k = k;
    entry.v = v;
    entry.seq_pos = current_seq_pos_++;

    layers_[layer].heads[head].push_back(entry);

    // Apply sliding window: keep only recent entries
    if (layers_[layer].heads[head].size() > window_size_) {
        layers_[layer].heads[head].erase(
            layers_[layer].heads[head].begin(),
            layers_[layer].heads[head].end() - window_size_
        );
    }
}

bool NativeKVCache::GetKV(size_t layer, size_t head, size_t seq_pos,
                         std::vector<float>& k_out, std::vector<float>& v_out) const {
    if (layer >= layers_.size() || head >= layers_[layer].heads.size()) return false;

    const auto& entries = layers_[layer].heads[head];

    // Find entry for this sequence position
    for (const auto& entry : entries) {
        if (entry.seq_pos == seq_pos) {
            k_out = entry.k;
            v_out = entry.v;
            return true;
        }
    }

    return false;
}

void NativeKVCache::Compress() {
    // Apply SVD compression to each head's KV cache
    for (auto& layer : layers_) {
        for (auto& head_cache : layer.heads) {
            if (head_cache.size() > window_size_ / 2) {
                // Compress older entries
                for (size_t i = 0; i < head_cache.size() / 2; ++i) {
                    ApplySVDCompression(head_cache[i].k);
                    ApplySVDCompression(head_cache[i].v);
                }
            }
        }
    }
}

void NativeKVCache::Clear() {
    for (auto& layer : layers_) {
        for (auto& head_cache : layer.heads) {
            head_cache.clear();
        }
    }
    current_seq_pos_ = 0;
}

size_t NativeKVCache::GetMemoryUsage() const {
    size_t total = 0;
    for (const auto& layer : layers_) {
        for (const auto& head_cache : layer.heads) {
            for (const auto& entry : head_cache) {
                total += entry.k.size() * sizeof(float);
                total += entry.v.size() * sizeof(float);
            }
        }
    }
    return total;
}

void NativeKVCache::ApplySVDCompression(std::vector<float>& data, size_t rank) {
    // Simplified SVD compression
    // In real implementation, would perform actual SVD decomposition
    // For now, just truncate to rank elements as placeholder
    if (data.size() > rank) {
        data.resize(rank);
    }
}

std::vector<float> NativeKVCache::SVDDecompress(const std::vector<float>& compressed_data) const {
    // Placeholder decompression
    return compressed_data;
}