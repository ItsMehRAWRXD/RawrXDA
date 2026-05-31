// streaming_inference_engine.h - Latency-optimized streaming inference
// Implements 15 TPS/latency enhancements:
//   1. Speculative decoding (Q4_K draft + Q6_K verify)
//   2. Prefix KV-cache reuse
//   3. Sliding window context
//   4. Async double-buffered dispatch
//   5. Persistent mapped buffers
//   6. Wave-level reductions (subgroup ops)
//   7. Kernel fusion (matmul + dequant)
//   8. Token batching (micro-batch 2-4)
//   9. Adaptive quant switching
//   10. Early-exit heuristic
//   11. CPU/GPU overlap
//   12. Token streaming before full decode
//   13. Hot shader residency
//   14. Branchless dequant paths
//   15. Memory layout alignment
// Part of the Copilot-like inference pipeline.

#pragma once

#include "kernel_arbiter.h"
#include "vulkan_compute.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace RawrXD {

// Token callback types
using TokenCallback = std::function<void(const std::string& token)>;
using CompleteCallback = std::function<void()>;
using TokenIdCallback = std::function<void(int32_t token_id)>;

// Context window for sliding window optimization
struct ContextWindow {
    std::string file_path;
    int cursor_line;
    int cursor_column;
    std::string prefix;      // Lines before cursor
    std::string suffix;      // Lines after cursor
    std::string full_context; // Combined context
    uint64_t hash;           // Hash for KV cache reuse
};

// KV cache entry for prefix reuse
struct KVCacheEntry {
    uint64_t prefix_hash;
    uint32_t seq_len;
    std::vector<uint32_t> token_ids;
    std::chrono::steady_clock::time_point last_used;
    bool valid;
};

// Streaming statistics for adaptive optimization
struct StreamingStats {
    std::chrono::microseconds first_token_latency;
    std::chrono::microseconds avg_token_latency;
    int tokens_generated;
    int tokens_accepted;
    int tokens_rejected;      // Speculative decode rejections
    float avg_confidence;
    int kernel_switches;      // Adaptive quant switches
};

// Micro-batch for token batching (enhancement #8)
struct TokenBatch {
    static constexpr int BATCH_SIZE = 4;
    std::array<uint32_t, BATCH_SIZE> tokens;
    int count;
    std::chrono::steady_clock::time_point start_time;
};

// Double-buffered dispatch state (enhancement #4)
struct DispatchBuffer {
    enum class State { IDLE, PREPARING, COMPUTING, READY };
    std::atomic<State> state{State::IDLE};
    std::vector<float> input_buffer;
    std::vector<float> output_buffer;
    std::mutex mutex;
    std::condition_variable cv;
};

// Streaming inference engine with all 15 enhancements
class StreamingInferenceEngine {
public:
    StreamingInferenceEngine(VulkanCompute* vulkan);
    ~StreamingInferenceEngine();
    
    // Main streaming generation API
    void GenerateStreaming(
        const ContextWindow& context,
        int max_tokens,
        TokenCallback token_callback,
        CompleteCallback complete_callback = nullptr,
        TokenIdCallback token_id_callback = nullptr
    );
    
    // Stop generation
    void Stop();
    
    // Get statistics
    StreamingStats GetStats() const;
    
    // Prefetch context for lower latency (enhancement #11)
    void PrefetchContext(const ContextWindow& context);
    
    // Clear KV cache
    void ClearKVCaches();
    
    // Set kernel mode directly (for manual control)
    void SetKernelMode(int mode);
    
    // Get current kernel mode
    int GetKernelMode() const { return current_kernel_mode_; }

private:
    // Enhancement implementations
    
    // #1: Speculative decoding
    struct SpeculativeState {
        std::vector<uint32_t> draft_tokens;
        std::vector<float> draft_logits;
        std::vector<uint32_t> verified_tokens;
        int draft_kernel;
        int verify_kernel;
        bool active;
    };
    void RunSpeculativeDecode(
        const std::vector<uint32_t>& prompt_tokens,
        int max_tokens,
        TokenCallback token_callback
    );
    
    // #2: Prefix KV-cache reuse
    bool TryReuseKVCache(uint64_t prefix_hash, int& cache_hit_len);
    void UpdateKVCache(uint64_t prefix_hash, const std::vector<uint32_t>& tokens);
    
    // #3: Sliding window context
    std::string BuildSlidingWindow(const ContextWindow& context, int max_lines = 400);
    
    // #4: Async double-buffered dispatch
    void DispatchAsync(DispatchBuffer& buf, std::function<void()> compute_fn);
    void WaitForCompletion(DispatchBuffer& buf);
    
    // #5: Persistent mapped buffers (handled in VulkanCompute)
    // Already implemented via persistent buffer mapping
    
    // #6: Wave-level reductions (handled in shaders)
    // Already implemented via subgroup ops
    
    // #7: Kernel fusion (handled in shaders)
    // Already implemented via fused matmul + dequant
    
    // #8: Token batching
    void ProcessBatch(const TokenBatch& batch);
    bool ShouldBatch() const;
    
    // #9: Adaptive quant switching
    void AdaptKernel(float confidence, int tokens_generated);
    
    // #10: Early-exit heuristic
    bool ShouldEarlyExit(float confidence, int tokens_generated) const;
    
    // #11: CPU/GPU overlap
    void PrepareNextToken();
    void OverlapComputeAndPrepare();
    
    // #12: Token streaming before full decode
    void StreamTokenImmediately(const std::string& token);
    
    // #13: Hot shader residency (handled in VulkanCompute)
    // Pipeline caching already implemented
    
    // #14: Branchless dequant paths (handled in shaders)
    // Already implemented via bit manipulation
    
    // #15: Memory layout alignment
    void AlignBuffers();
    
    // Core generation loop
    void GenerateLoop(
        const std::vector<uint32_t>& prompt_tokens,
        int max_tokens,
        TokenCallback token_callback,
        TokenIdCallback token_id_callback
    );
    
    // Token sampling with confidence
    struct SampleResult {
        uint32_t token;
        float confidence;
        std::vector<float> logits;
    };
    SampleResult SampleToken(const float* logits, size_t vocab_size);
    
    // Members
    VulkanCompute* vulkan_;
    KernelArbiter arbiter_;
    
    // KV cache (enhancement #2)
    std::unordered_map<uint64_t, KVCacheEntry> kv_cache_;
    std::mutex kv_cache_mutex_;
    static constexpr int MAX_KV_CACHE_ENTRIES = 100;
    
    // Double buffers (enhancement #4)
    DispatchBuffer buffers_[2];
    int current_buffer_{0};
    
    // Streaming state
    std::atomic<bool> stop_flag_{false};
    std::atomic<bool> generating_{false};
    std::thread generation_thread_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    StreamingStats stats_;
    
    // Current kernel mode
    std::atomic<int> current_kernel_mode_{1}; // Default: Q4_K
    
    // Speculative state
    SpeculativeState spec_state_;
    
    // Token batch (enhancement #8)
    TokenBatch current_batch_;
    
    // Context hash for KV reuse
    uint64_t last_context_hash_{0};
    
    // Performance tuning constants
    static constexpr int MIN_TOKENS_FOR_SPEC = 10;
    static constexpr int MAX_SPEC_DRAFT_TOKENS = 4;
    static constexpr float EARLY_EXIT_CONFIDENCE = 0.95f;
    static constexpr int EARLY_EXIT_MIN_TOKENS = 5;
};

// Inline implementations for hot paths

inline bool StreamingInferenceEngine::ShouldEarlyExit(float confidence, int tokens_generated) const {
    // Enhancement #10: Early-exit when confidence is high
    return confidence >= EARLY_EXIT_CONFIDENCE && tokens_generated >= EARLY_EXIT_MIN_TOKENS;
}

inline bool StreamingInferenceEngine::ShouldBatch() const {
    // Enhancement #8: Batch when we have pending tokens
    return current_batch_.count > 0 && current_batch_.count < TokenBatch::BATCH_SIZE;
}

inline void StreamingInferenceEngine::StreamTokenImmediately(const std::string& token) {
    // Enhancement #12: Stream before full decode
    // This is called immediately after token is sampled
    // The actual callback is invoked in GenerateStreaming
}

inline void StreamingInferenceEngine::AdaptKernel(float confidence, int tokens_generated) {
    // Enhancement #9: Adaptive quant switching
    auto selection = arbiter_.SelectAdaptive(
        TaskType::AUTOCOMPLETE,
        std::chrono::microseconds(0), // Will be computed from stats
        tokens_generated,
        confidence
    );
    
    if (selection.kernel_mode != current_kernel_mode_.load()) {
        current_kernel_mode_.store(selection.kernel_mode);
        stats_.kernel_switches++;
    }
}

// Factory function
std::unique_ptr<StreamingInferenceEngine> CreateStreamingEngine(VulkanCompute* vulkan);

} // namespace RawrXD