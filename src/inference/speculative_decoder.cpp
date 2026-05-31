#include "inference/speculative_decoder.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#include <random>

namespace RawrXD {
namespace Inference {

// Register-only verification buffer implementation
class RegisterVerifyBuffer::Impl {
public:
    alignas(64) float draft_logits_[MAX_BATCH * MAX_VOCAB];
    alignas(64) float target_logits_[MAX_BATCH * MAX_VOCAB];
    alignas(64) uint32_t accepted_tokens_[MAX_BATCH];
    
    size_t loaded_count_ = 0;
    size_t vocab_size_ = 0;
    
    bool LoadDraftLogits(const float* logits, size_t count, size_t vocab_size) {
        if (count > MAX_BATCH || vocab_size > MAX_VOCAB) return false;
        
        vocab_size_ = vocab_size;
        loaded_count_ = count;
        
        // Copy to aligned buffer
        for (size_t i = 0; i < count * vocab_size; i++) {
            draft_logits_[i] = logits[i];
        }
        
        return true;
    }
    
    bool LoadTargetLogits(const float* logits, size_t count, size_t vocab_size) {
        if (count > MAX_BATCH || vocab_size > MAX_VOCAB) return false;
        
        // Copy to aligned buffer
        for (size_t i = 0; i < count * vocab_size; i++) {
            target_logits_[i] = logits[i];
        }
        
        return true;
    }
    
    // Softmax in register space
    void Softmax(float* logits, size_t vocab_size) {
        float max_logit = logits[0];
        for (size_t i = 1; i < vocab_size; i++) {
            if (logits[i] > max_logit) max_logit = logits[i];
        }
        
        float sum = 0.0f;
        for (size_t i = 0; i < vocab_size; i++) {
            logits[i] = std::exp(logits[i] - max_logit);
            sum += logits[i];
        }
        
        for (size_t i = 0; i < vocab_size; i++) {
            logits[i] /= sum;
        }
    }
    
    // Temperature scaling in register space
    void ApplyTemperature(float* logits, size_t vocab_size, float temperature) {
        if (temperature == 0.0f) return; // Greedy
        
        float inv_temp = 1.0f / temperature;
        for (size_t i = 0; i < vocab_size; i++) {
            logits[i] *= inv_temp;
        }
    }
    
    VerifyResult Verify(const DraftSequence& draft, float temperature) {
        VerifyResult result;
        result.accepted_count = 0;
        result.reject_index = 0;
        result.corrected_token = 0;
        result.all_accepted = false;
        result.acceptance_rate = 0.0f;
        
        if (draft.tokens.empty() || loaded_count_ == 0) {
            return result;
        }
        
        size_t verify_count = std::min(draft.tokens.size(), loaded_count_);
        size_t accepted = 0;
        
        // Process each token position
        for (size_t pos = 0; pos < verify_count; pos++) {
            // Get draft token probability from draft model
            uint32_t draft_token = draft.tokens[pos].token;
            float draft_prob = draft.tokens[pos].prob;
            
            // Get target logits for this position
            float* target_logits_pos = target_logits_ + pos * vocab_size_;
            
            // Apply temperature
            ApplyTemperature(target_logits_pos, vocab_size_, temperature);
            
            // Softmax to get probabilities
            Softmax(target_logits_pos, vocab_size_);
            
            // Get target probability for draft token
            float target_prob = target_logits_pos[draft_token];
            
            // Acceptance criterion: target_prob / draft_prob >= random
            // Simplified: accept if target_prob >= draft_prob * threshold
            float acceptance_threshold = draft_prob * 0.9f; // 90% of draft prob
            
            if (target_prob >= acceptance_threshold) {
                // Accept this token
                accepted_tokens_[accepted] = draft_token;
                accepted++;
            } else {
                // Reject - sample corrected token from target distribution
                result.reject_index = pos;
                
                // Greedy sample from target
                uint32_t best_token = 0;
                float best_prob = target_logits_pos[0];
                for (size_t i = 1; i < vocab_size_; i++) {
                    if (target_logits_pos[i] > best_prob) {
                        best_prob = target_logits_pos[i];
                        best_token = static_cast<uint32_t>(i);
                    }
                }
                result.corrected_token = best_token;
                accepted_tokens_[accepted] = best_token;
                accepted++;
                break; // Stop verification after first rejection
            }
        }
        
        result.accepted_count = accepted;
        result.all_accepted = (accepted == draft.tokens.size());
        result.acceptance_rate = static_cast<float>(accepted) / verify_count;
        
        return result;
    }
    
    size_t GetAcceptedTokens(uint32_t* tokens, size_t max_count) {
        size_t to_copy = std::min(max_count, loaded_count_);
        for (size_t i = 0; i < to_copy; i++) {
            tokens[i] = accepted_tokens_[i];
        }
        return to_copy;
    }
    
    void Clear() {
        loaded_count_ = 0;
        vocab_size_ = 0;
    }
};

RegisterVerifyBuffer::RegisterVerifyBuffer() : pImpl(std::make_unique<Impl>()) {}
RegisterVerifyBuffer::~RegisterVerifyBuffer() = default;

bool RegisterVerifyBuffer::LoadDraftLogits(const float* logits, size_t count, size_t vocab_size) {
    return pImpl->LoadDraftLogits(logits, count, vocab_size);
}

bool RegisterVerifyBuffer::LoadTargetLogits(const float* logits, size_t count, size_t vocab_size) {
    return pImpl->LoadTargetLogits(logits, count, vocab_size);
}

VerifyResult RegisterVerifyBuffer::Verify(const DraftSequence& draft, float temperature) {
    return pImpl->Verify(draft, temperature);
}

size_t RegisterVerifyBuffer::GetAcceptedTokens(uint32_t* tokens, size_t max_count) {
    return pImpl->GetAcceptedTokens(tokens, max_count);
}

void RegisterVerifyBuffer::Clear() {
    pImpl->Clear();
}

// FusedSpeculativeDecoder implementation
class FusedSpeculativeDecoder::Impl {
public:
    SpeculativeConfig config_;
    RegisterVerifyBuffer verify_buffer_;
    
    // Statistics
    std::atomic<uint64_t> total_steps_{0};
    std::atomic<uint64_t> total_draft_tokens_{0};
    std::atomic<uint64_t> total_accepted_tokens_{0};
    std::atomic<uint64_t> total_rejected_tokens_{0};
    std::atomic<uint64_t> total_verify_time_ns_{0};
    
    // Simple draft model simulation (for testing)
    // In production, this would call a smaller model
    std::mt19937 rng_{std::random_device{}()};
    
    bool Initialize(const SpeculativeConfig& config) {
        config_ = config;
        return true;
    }
    
    // Simulate draft generation (would call actual small model)
    DraftSequence GenerateDraft(const uint32_t* context, size_t context_len,
                                 uint64_t sequence_id) {
        DraftSequence draft;
        draft.sequence_id = sequence_id;
        draft.is_complete = false;
        draft.cumulative_prob = 1.0f;
        
        // Generate draft tokens (simulated)
        for (size_t i = 0; i < config_.num_draft_tokens; i++) {
            TokenProb token_prob;
            
            // Simple simulation: generate based on context
            if (context_len > 0) {
                // Use last context token to influence draft
                token_prob.token = (context[context_len - 1] + i + 1) % 32000;
            } else {
                token_prob.token = static_cast<uint32_t>(rng_() % 32000);
            }
            
            // Simulate probability (decreasing for later tokens)
            token_prob.prob = 0.9f - (i * 0.1f);
            token_prob.logit = std::log(token_prob.prob);
            
            draft.tokens.push_back(token_prob);
            draft.cumulative_prob *= token_prob.prob;
        }
        
        total_draft_tokens_ += draft.tokens.size();
        return draft;
    }
    
    VerifyResult VerifyDraft(const DraftSequence& draft,
                              const float* target_logits,
                              size_t target_count) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Load logits into register buffer
        if (target_logits && target_count > 0) {
            // Assume vocab_size = 32000 for simulation
            verify_buffer_.LoadTargetLogits(target_logits, target_count, 32000);
        }
        
        // Perform verification
        VerifyResult result = verify_buffer_.Verify(draft, config_.temperature);
        
        // Update stats
        total_accepted_tokens_ += result.accepted_count;
        total_rejected_tokens_ += (draft.tokens.size() - result.accepted_count);
        total_steps_++;
        
        auto end = std::chrono::high_resolution_clock::now();
        auto verify_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        total_verify_time_ns_ += verify_time;
        
        return result;
    }
    
    size_t GenerateFused(const uint32_t* context, size_t context_len,
                         uint32_t* output_tokens, size_t max_output,
                         uint64_t sequence_id) {
        if (max_output == 0) return 0;
        
        size_t generated = 0;
        
        while (generated < max_output) {
            // Generate draft
            DraftSequence draft = GenerateDraft(context, context_len, sequence_id);
            
            // Simulate target logits (in production, from large model)
            std::vector<float> target_logits(draft.tokens.size() * 32000);
            for (size_t i = 0; i < draft.tokens.size() * 32000; i++) {
                target_logits[i] = static_cast<float>(rng_()) / rng_.max() * 10.0f - 5.0f;
            }
            
            // Verify draft
            VerifyResult result = VerifyDraft(draft, target_logits.data(), draft.tokens.size());
            
            // Copy accepted tokens to output
            size_t to_copy = std::min(result.accepted_count, max_output - generated);
            for (size_t i = 0; i < to_copy; i++) {
                output_tokens[generated + i] = draft.tokens[i].token;
            }
            generated += to_copy;
            
            // If we rejected early, stop
            if (!result.all_accepted) {
                break;
            }
            
            // Update context for next iteration
            context_len = 0; // Simplified
        }
        
        return generated;
    }
    
    float GetAcceptanceRate() const {
        uint64_t draft = total_draft_tokens_.load();
        uint64_t accepted = total_accepted_tokens_.load();
        return draft > 0 ? static_cast<float>(accepted) / draft : 0.0f;
    }
    
    float GetAvgTokensPerStep() const {
        uint64_t steps = total_steps_.load();
        uint64_t accepted = total_accepted_tokens_.load();
        return steps > 0 ? static_cast<float>(accepted) / steps : 0.0f;
    }
    
    void Reset() {
        total_steps_ = 0;
        total_draft_tokens_ = 0;
        total_accepted_tokens_ = 0;
        total_rejected_tokens_ = 0;
        total_verify_time_ns_ = 0;
        verify_buffer_.Clear();
    }
    
    FusedSpeculativeDecoder::Stats GetStats() const {
        Stats stats;
        stats.total_steps = total_steps_.load();
        stats.total_draft_tokens = total_draft_tokens_.load();
        stats.total_accepted_tokens = total_accepted_tokens_.load();
        stats.total_rejected_tokens = total_rejected_tokens_.load();
        stats.avg_acceptance_rate = GetAcceptanceRate();
        
        // Speedup = accepted / steps (vs 1 token per step baseline)
        stats.speedup_ratio = GetAvgTokensPerStep();
        
        uint64_t verify_samples = total_steps_.load();
        if (verify_samples > 0) {
            stats.avg_verify_time_us = (total_verify_time_ns_.load() / verify_samples) / 1000.0;
        } else {
            stats.avg_verify_time_us = 0.0;
        }
        
        return stats;
    }
};

FusedSpeculativeDecoder::FusedSpeculativeDecoder() : pImpl(std::make_unique<Impl>()) {}
FusedSpeculativeDecoder::~FusedSpeculativeDecoder() = default;

bool FusedSpeculativeDecoder::Initialize(const SpeculativeConfig& config) {
    return pImpl->Initialize(config);
}

DraftSequence FusedSpeculativeDecoder::GenerateDraft(const uint32_t* context, size_t context_len,
                                                       uint64_t sequence_id) {
    return pImpl->GenerateDraft(context, context_len, sequence_id);
}

VerifyResult FusedSpeculativeDecoder::VerifyDraft(const DraftSequence& draft,
                                                   const float* target_logits,
                                                   size_t target_count) {
    return pImpl->VerifyDraft(draft, target_logits, target_count);
}

size_t FusedSpeculativeDecoder::GenerateFused(const uint32_t* context, size_t context_len,
                                               uint32_t* output_tokens, size_t max_output,
                                               uint64_t sequence_id) {
    return pImpl->GenerateFused(context, context_len, output_tokens, max_output, sequence_id);
}

float FusedSpeculativeDecoder::GetAcceptanceRate() const {
    return pImpl->GetAcceptanceRate();
}

float FusedSpeculativeDecoder::GetAvgTokensPerStep() const {
    return pImpl->GetAvgTokensPerStep();
}

void FusedSpeculativeDecoder::Reset() {
    pImpl->Reset();
}

FusedSpeculativeDecoder::Stats FusedSpeculativeDecoder::GetStats() const {
    return pImpl->GetStats();
}

// C-API implementation
extern "C" {

struct RawrXD_SpeculativeDecoder {
    FusedSpeculativeDecoder* decoder;
};

RawrXD_SpeculativeDecoder* RawrXD_speculative_decoder_create(
    size_t num_draft_tokens,
    float temperature,
    float top_p
) {
    auto* wrapper = new RawrXD_SpeculativeDecoder();
    wrapper->decoder = new FusedSpeculativeDecoder();
    
    SpeculativeConfig config;
    config.num_draft_tokens = num_draft_tokens;
    config.temperature = temperature;
    config.top_p = top_p;
    wrapper->decoder->Initialize(config);
    
    return wrapper;
}

void RawrXD_speculative_decoder_destroy(RawrXD_SpeculativeDecoder* decoder) {
    if (decoder) {
        delete decoder->decoder;
        delete decoder;
    }
}

size_t RawrXD_speculative_decoder_generate(
    RawrXD_SpeculativeDecoder* decoder,
    const uint32_t* context,
    size_t context_len,
    uint32_t* output_tokens,
    size_t max_output,
    uint64_t sequence_id
) {
    if (!decoder || !decoder->decoder) return 0;
    return decoder->decoder->GenerateFused(context, context_len, output_tokens, max_output, sequence_id);
}

float RawrXD_speculative_decoder_get_acceptance_rate(RawrXD_SpeculativeDecoder* decoder) {
    if (!decoder || !decoder->decoder) return 0.0f;
    return decoder->decoder->GetAcceptanceRate();
}

float RawrXD_speculative_decoder_get_speedup(RawrXD_SpeculativeDecoder* decoder) {
    if (!decoder || !decoder->decoder) return 1.0f;
    return decoder->decoder->GetAvgTokensPerStep();
}

void RawrXD_speculative_decoder_reset(RawrXD_SpeculativeDecoder* decoder) {
    if (decoder && decoder->decoder) {
        decoder->decoder->Reset();
    }
}

} // extern "C"

} // namespace Inference
} // namespace RawrXD
