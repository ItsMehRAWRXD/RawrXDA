#include "inference/token_pipeline.h"
#include <chrono>
#include <cmath>
#include <thread>

namespace RawrXD {
namespace Inference {

// Implementation
class TokenPipeline::Impl {
public:
    // SPSC queue for token submission (GPU -> CPU)
    // Capacity: 8192 tokens (2x max context for 4K models)
    DoubleBufferQueue<uint32_t, 8192> token_queue_;
    DoubleBufferQueue<float, 8192> logit_queue_;
    DoubleBufferQueue<uint64_t, 1024> seq_id_queue_;
    
    std::atomic<size_t> total_submitted_{0};
    std::atomic<size_t> total_retrieved_{0};
    std::atomic<size_t> dropped_tokens_{0};
    
    size_t max_sequence_length_ = 4096;
    size_t batch_size_ = 1;
    bool initialized_ = false;
    
    // Latency tracking
    std::atomic<uint64_t> total_latency_ns_{0};
    std::atomic<uint64_t> latency_samples_{0};
    
    bool Initialize(size_t max_seq_len, size_t batch_size) {
        max_sequence_length_ = max_seq_len;
        batch_size_ = batch_size;
        initialized_ = true;
        return true;
    }
    
    bool SubmitTokens(const uint32_t* tokens, const float* logits,
                      size_t count, uint64_t sequence_id) {
        if (!initialized_ || count == 0) return false;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Try to submit all tokens
        for (size_t i = 0; i < count; i++) {
            if (!token_queue_.Push(tokens[i])) {
                dropped_tokens_ += (count - i);
                return false;
            }
            if (logits && !logit_queue_.Push(logits[i])) {
                // Shouldn't happen if token succeeded, but handle it
                dropped_tokens_++;
                return false;
            }
        }
        
        // Submit sequence ID (once per batch) - best effort
        seq_id_queue_.Push(sequence_id);
        
        total_submitted_ += count;
        
        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        total_latency_ns_ += latency;
        latency_samples_++;
        
        return true;
    }
    
    size_t RetrieveTokens(uint32_t* tokens, float* logits,
                          size_t max_count, uint64_t& sequence_id) {
        if (!initialized_ || max_count == 0) return 0;
        
        // Try to get sequence ID first (non-blocking)
        uint64_t seq_id;
        if (!seq_id_queue_.Pop(seq_id)) {
            // No sequence ID available - check if there are tokens anyway
            // (for backward compatibility)
            seq_id = 0;
        }
        sequence_id = seq_id;
        
        // Retrieve tokens - don't require sequence ID match
        size_t retrieved = 0;
        for (size_t i = 0; i < max_count; i++) {
            if (!token_queue_.Pop(tokens[i])) {
                break;
            }
            if (logits) {
                logit_queue_.Pop(logits[i]);
            }
            retrieved++;
        }
        
        total_retrieved_ += retrieved;
        return retrieved;
    }
    
    void Drain() {
        // Wait for all tokens to be retrieved
        while (token_queue_.Size() > 0) {
            std::this_thread::yield();
        }
    }
    
    void Reset() {
        // Clear all queues
        uint32_t dummy_token;
        float dummy_logit;
        uint64_t dummy_seq;
        
        while (token_queue_.Pop(dummy_token)) {}
        while (logit_queue_.Pop(dummy_logit)) {}
        while (seq_id_queue_.Pop(dummy_seq)) {}
        
        total_submitted_ = 0;
        total_retrieved_ = 0;
        dropped_tokens_ = 0;
        total_latency_ns_ = 0;
        latency_samples_ = 0;
    }
    
    TokenPipeline::Stats GetStats() const {
        Stats stats;
        stats.pending_tokens = token_queue_.Size();
        stats.processed_tokens = total_retrieved_.load();
        stats.dropped_tokens = dropped_tokens_.load();
        
        auto samples = latency_samples_.load();
        if (samples > 0) {
            stats.avg_latency_us = (total_latency_ns_.load() / samples) / 1000.0;
        } else {
            stats.avg_latency_us = 0.0;
        }
        
        return stats;
    }
};

// TokenPipeline public interface
TokenPipeline::TokenPipeline() : pImpl(std::make_unique<Impl>()) {}
TokenPipeline::~TokenPipeline() = default;

bool TokenPipeline::Initialize(size_t maxSequenceLength, size_t batchSize) {
    return pImpl->Initialize(maxSequenceLength, batchSize);
}

bool TokenPipeline::SubmitTokens(const uint32_t* tokens, const float* logits,
                                  size_t count, uint64_t sequence_id) {
    return pImpl->SubmitTokens(tokens, logits, count, sequence_id);
}

size_t TokenPipeline::RetrieveTokens(uint32_t* tokens, float* logits,
                                      size_t max_count, uint64_t& sequence_id) {
    return pImpl->RetrieveTokens(tokens, logits, max_count, sequence_id);
}

void TokenPipeline::EndSequence(uint64_t sequence_id) {
    // Mark end of sequence (could add special token or flag)
    (void)sequence_id;
}

TokenPipeline::Stats TokenPipeline::GetStats() const {
    return pImpl->GetStats();
}

void TokenPipeline::Drain() {
    pImpl->Drain();
}

void TokenPipeline::Reset() {
    pImpl->Reset();
}

// C-API implementation
extern "C" {

struct RawrXD_TokenPipeline {
    TokenPipeline* pipeline;
};

RawrXD_TokenPipeline* RawrXD_token_pipeline_create(size_t max_seq_len) {
    auto* wrapper = new RawrXD_TokenPipeline();
    wrapper->pipeline = new TokenPipeline();
    wrapper->pipeline->Initialize(max_seq_len, 1);
    return wrapper;
}

void RawrXD_token_pipeline_destroy(RawrXD_TokenPipeline* pipeline) {
    if (pipeline) {
        delete pipeline->pipeline;
        delete pipeline;
    }
}

int RawrXD_token_pipeline_submit(RawrXD_TokenPipeline* pipeline,
                                  const uint32_t* tokens,
                                  const float* logits,
                                  size_t count,
                                  uint64_t sequence_id) {
    if (!pipeline || !pipeline->pipeline) return 0;
    return pipeline->pipeline->SubmitTokens(tokens, logits, count, sequence_id) ? 1 : 0;
}

size_t RawrXD_token_pipeline_retrieve(RawrXD_TokenPipeline* pipeline,
                                       uint32_t* tokens,
                                       float* logits,
                                       size_t max_count,
                                       uint64_t* sequence_id) {
    if (!pipeline || !pipeline->pipeline || !sequence_id) return 0;
    uint64_t seq_id;
    size_t result = pipeline->pipeline->RetrieveTokens(tokens, logits, max_count, seq_id);
    *sequence_id = seq_id;
    return result;
}

void RawrXD_token_pipeline_drain(RawrXD_TokenPipeline* pipeline) {
    if (pipeline && pipeline->pipeline) {
        pipeline->pipeline->Drain();
    }
}

void RawrXD_token_pipeline_reset(RawrXD_TokenPipeline* pipeline) {
    if (pipeline && pipeline->pipeline) {
        pipeline->pipeline->Reset();
    }
}

} // extern "C"

} // namespace Inference
} // namespace RawrXD
