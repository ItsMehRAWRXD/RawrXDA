// d:/rawrxd/src/ai/fast_spec_inference_bridge.h
//
// FastSpecInferenceBridge — wires FastSpec (the 120M-TPS lock-free draft
// generator) into the real token generation hot path.
//
// Contract:
//   - The caller provides the target model's logits for the current step.
//   - The bridge proposes K draft tokens via FastSpec::SpeculateFast (no
//     allocations, no locks, ~10ns / call) and validates them against the
//     target logits with greedy argmax.
//   - Accepted tokens are enqueued into FastSpec's existing SPSC validation
//     ring; the FastSpec drain thread updates the bigram table off-path.
//     We do not introduce a second ring — that would be duplication and
//     would re-add the kernel transition cost we just removed.
//   - The bridge has no opinion on the target backend; it accepts logits
//     from any GPU verifier (Vulkan / DML / HIP / CUDA / Titan).
//
// Threading: GenerateToken is single-threaded per stream (the inference
// hot loop). FastSpec::EnqueueValidation is SPSC; the bridge is the sole
// producer.

#pragma once

#include "fast_spec.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace RawrXD {

class FastSpecInferenceBridge {
public:
    struct Config {
        uint32_t vocab_size  = 32000;   // must match the target model's vocab
        uint32_t draft_width = 4;       // K candidates per step (≤ FastSpec::Config::top_k)
        uint32_t ring_log2   = 16;      // SPSC ring log2 capacity for FastSpec
    };

    struct Stats {
        uint64_t steps                  = 0;  // GenerateToken invocations
        uint64_t draft_emit             = 0;  // candidates received from FastSpec
        uint64_t draft_accepted         = 0;  // candidates that matched target argmax
        uint64_t draft_zero_emit_steps  = 0;  // steps where FastSpec returned 0
        uint64_t sampled_accepted       = 0;  // probabilistic accepts (non-argmax included)
        uint64_t sampled_residual       = 0;  // steps where all drafts rejected, sampled fresh
        double   acceptance_rate        = 0.0;
        double   effective_speedup      = 1.0;
    };

    // Result of a single generation step.
    struct Step {
        uint32_t accepted_token = FastSpec::kNoToken;
        uint32_t draft_count    = 0;  // candidates evaluated
        uint32_t accepted_count = 0;  // candidates that matched target argmax (0 or 1
                                      // under greedy validation)
    };

    explicit FastSpecInferenceBridge(const Config& cfg = {});
    ~FastSpecInferenceBridge() = default;

    FastSpecInferenceBridge(const FastSpecInferenceBridge&)            = delete;
    FastSpecInferenceBridge& operator=(const FastSpecInferenceBridge&) = delete;

    // Optional warmup: walk a prompt through the bigram trainer so step 1 has
    // useful candidates instead of cold misses. Lock-free; uses the same
    // SPSC validation path the hot loop does.
    void PrefillContext(const std::vector<uint32_t>& prompt_tokens) noexcept;

    // Single inference step.
    //
    //   last_token   — the most recently committed token (anchor for speculation).
    //   target_logits — logits emitted by the target GPU model for the next position.
    //
    // Returns the accepted next token plus diagnostic counts.
    Step GenerateToken(uint32_t                  last_token,
                       const std::vector<float>& target_logits) noexcept;

    // Same as GenerateToken but accepts a precomputed argmax (skips the argmax
    // scan when the GPU verifier already produced it). Use this on the hot
    // path when your sampler returned the greedy token.
    Step GenerateTokenWithArgmax(uint32_t last_token,
                                 uint32_t target_argmax) noexcept;

    // Probabilistic speculative-decoding step (Leviathan et al. 2023).
    //
    // For each draft d_i, accept with probability min(1, q(d_i) / p(d_i))
    // where q is the target softmax and p is the draft distribution. We
    // approximate p as uniform over the K emitted drafts (1/K), which is a
    // conservative but unbiased estimate when FastSpec returns its top-K
    // bigram successors. On rejection we sample from the target softmax via
    // CDF inversion.
    //
    //   rng_state — caller-owned 64-bit xorshift state (non-zero). Mutated.
    //
    // Returns the committed token. Unlike GenerateTokenWithArgmax this can
    // commit non-argmax tokens, which is what real (temperature > 0)
    // sampling requires.
    Step GenerateTokenSampled(uint32_t                  last_token,
                              const std::vector<float>& target_logits,
                              uint64_t*                 rng_state) noexcept;

    Stats GetStats() const noexcept;

    // Underlying speculator (exposed for tests and stats UI).
    FastSpec& Speculator() noexcept { return *m_spec; }

private:
    Config m_cfg;
    std::unique_ptr<FastSpec> m_spec;

    alignas(64) std::atomic<uint64_t> m_steps{0};
    std::atomic<uint64_t> m_draft_emit{0};
    std::atomic<uint64_t> m_draft_accepted{0};
    std::atomic<uint64_t> m_draft_zero_emit{0};
    std::atomic<uint64_t> m_sampled_accepted{0};
    std::atomic<uint64_t> m_sampled_residual{0};
};

} // namespace RawrXD
