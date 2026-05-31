// dual_stream_speculative.h - True dual-stream speculative decoding
// Implements parallel Q4_K + Q6_K pipeline for instant perceived response
//
// Strategy:
//   - Q4_K streams immediately (user sees results instantly)
//   - Q6_K runs behind (higher quality verification)
//   - If Q6_K token != Q4_K token: replace inline
//   - This is where it starts feeling like magic
//
// Key insight:
//   - Not sequential: Q4_K → THEN Q6_K
//   - But parallel: Q4_K (streaming) + Q6_K (running behind)
//   - User sees Q4_K immediately, Q6_K refines in place
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "kernel_arbiter.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace RawrXD {

// Token pair for dual-stream
struct DualToken {
    uint32_t draft_token;      // Q4_K token
    uint32_t verify_token;     // Q6_K token
    std::string draft_text;    // Q4_K decoded text
    std::string verify_text;   // Q6_K decoded text
    bool draft_ready;          // Q4_K token is ready
    bool verify_ready;         // Q6_K token is ready
    bool accepted;             // Whether draft was accepted
    bool replaced;             // Whether we replaced draft with verify
    std::chrono::microseconds draft_latency;
    std::chrono::microseconds verify_latency;
};

// Dual-stream statistics
struct DualStreamStats {
    int total_tokens;
    int draft_tokens_accepted;
    int draft_tokens_rejected;
    float acceptance_rate;
    std::chrono::microseconds avg_draft_latency;
    std::chrono::microseconds avg_verify_latency;
    std::chrono::microseconds time_saved;
    int inline_replacements;
};

// Dual-stream speculative decoder
class DualStreamSpeculative {
public:
    DualStreamSpeculative();
    ~DualStreamSpeculative();
    
    // Configure dual-stream
    struct Config {
        int draft_kernel = 1;           // Q4_K
        int verify_kernel = 4;          // Q6_K
        int max_draft_tokens = 5;       // Max tokens to draft before verify
        int max_verify_tokens = 10;     // Max tokens to verify in parallel
        bool enable_inline_replace = true;  // Replace tokens inline
        bool enable_streaming = true;   // Stream draft tokens immediately
        std::chrono::milliseconds verify_timeout{100};  // Timeout for verification
    };
    void SetConfig(const Config& config);
    
    // Start dual-stream generation
    void StartGeneration(
        const std::vector<uint32_t>& prompt_tokens,
        int max_tokens,
        std::function<void(const std::string& token, bool is_draft)> token_callback,
        std::function<void()> complete_callback
    );
    
    // Stop generation
    void Stop();
    
    // Check if generating
    bool IsGenerating() const { return generating_.load(); }
    
    // Get statistics
    DualStreamStats GetStats() const;
    
    // Reset statistics
    void ResetStats();
    
private:
    // Draft thread (Q4_K)
    void DraftThread(
        std::vector<uint32_t> prompt_tokens,
        int max_tokens,
        std::function<void(const std::string& token, bool is_draft)> token_callback
    );
    
    // Verify thread (Q6_K)
    void VerifyThread(
        std::vector<uint32_t> prompt_tokens,
        int max_tokens
    );
    
    // Process token queue
    void ProcessQueue(
        std::function<void(const std::string& token, bool is_draft)> token_callback,
        std::function<void()> complete_callback
    );
    
    // Check if draft token matches verify token
    bool TokensMatch(uint32_t draft, uint32_t verify) const;
    
    // Replace draft token with verify token
    void ReplaceToken(
        int token_index,
        const std::string& new_text,
        std::function<void(const std::string& token, bool is_draft)> token_callback
    );
    
    // Members
    Config config_;
    KernelArbiter arbiter_;
    
    // Thread management
    std::thread draft_thread_;
    std::thread verify_thread_;
    std::thread process_thread_;
    std::atomic<bool> generating_{false};
    std::atomic<bool> stop_flag_{false};
    
    // Token queue
    std::queue<DualToken> token_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Draft tokens (for verification)
    std::vector<uint32_t> draft_tokens_;
    std::vector<uint32_t> verify_tokens_;
    std::mutex tokens_mutex_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    DualStreamStats stats_;
};

// Inline implementations

inline bool DualStreamSpeculative::IsGenerating() const {
    return generating_.load();
}

inline DualStreamStats DualStreamSpeculative::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

inline void DualStreamSpeculative::ResetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = {};
}

inline bool DualStreamSpeculative::TokensMatch(uint32_t draft, uint32_t verify) const {
    // Simple equality check
    // In practice, might want fuzzy matching for similar tokens
    return draft == verify;
}

} // namespace RawrXD