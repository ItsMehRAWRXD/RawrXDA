// d:/rawrxd/src/ai/fast_spec_inference_bridge.cpp

#include "fast_spec_inference_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace RawrXD {

namespace {

constexpr uint32_t kMaxDraftWidth = 16;

uint32_t Argmax(const std::vector<float>& logits) noexcept {
    if (logits.empty()) return FastSpec::kNoToken;
    uint32_t best_idx = 0;
    float    best_val = logits[0];
    const size_t n = logits.size();
    for (size_t i = 1; i < n; ++i) {
        const float v = logits[i];
        if (v > best_val) {
            best_val = v;
            best_idx = static_cast<uint32_t>(i);
        }
    }
    return best_idx;
}

// xorshift64* — high-quality, branch-free, ~1ns per call. Caller owns state.
inline uint64_t Xorshift64(uint64_t* s) noexcept {
    uint64_t x = *s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

inline float UniformF32(uint64_t* s) noexcept {
    // 24-bit mantissa → [0, 1) with uniform spacing of 2^-24.
    return static_cast<float>(Xorshift64(s) >> 40) * (1.0f / 16777216.0f);
}

} // namespace

FastSpecInferenceBridge::FastSpecInferenceBridge(const Config& cfg)
    : m_cfg(cfg)
{
    if (m_cfg.draft_width == 0)            m_cfg.draft_width = 1;
    if (m_cfg.draft_width > kMaxDraftWidth) m_cfg.draft_width = kMaxDraftWidth;

    FastSpec::Config sc;
    sc.vocab_size  = m_cfg.vocab_size;
    sc.top_k       = m_cfg.draft_width;
    sc.ring_log2   = m_cfg.ring_log2;
    sc.start_drain = true;
    m_spec.reset(new FastSpec(sc));
}

void FastSpecInferenceBridge::PrefillContext(
    const std::vector<uint32_t>& prompt_tokens) noexcept
{
    if (!m_spec || prompt_tokens.size() < 2) return;
    // Walk the prompt as (anchor → actual) pairs; the predicted slot is
    // unused for prefill so we pass the actual token (counts as a "correct"
    // observation, which strengthens confident bigrams from the prompt).
    for (size_t i = 1; i < prompt_tokens.size(); ++i) {
        const uint32_t anchor = prompt_tokens[i - 1];
        const uint32_t actual = prompt_tokens[i];
        m_spec->EnqueueValidation(anchor, actual, actual);
    }
    // Force an immediate drain so step 1 sees a populated table.
    m_spec->DrainNow();
}

FastSpecInferenceBridge::Step
FastSpecInferenceBridge::GenerateTokenWithArgmax(uint32_t last_token,
                                                 uint32_t target_argmax) noexcept
{
    Step step;
    if (!m_spec || target_argmax == FastSpec::kNoToken) {
        return step;
    }

    // Hot path: ask FastSpec for K candidates. ~10ns, no allocs, no locks.
    // The buffer is sparse: SpeculateFast writes each candidate to its slot
    // index and pads empty slots with kNoToken, so we must scan the full
    // draft_width window — emitted is just the non-empty count.
    uint32_t drafts[kMaxDraftWidth];
    const size_t emitted = m_spec->SpeculateFast(last_token, drafts, m_cfg.draft_width);
    step.draft_count = static_cast<uint32_t>(emitted);

    // Greedy validation: accept any draft that matches target argmax. Under
    // greedy sampling argmax is unique, so accepted_count is 0 or 1.
    uint32_t accepted_count = 0;
    uint32_t first_valid_draft = FastSpec::kNoToken;
    for (uint32_t i = 0; i < m_cfg.draft_width; ++i) {
        const uint32_t d = drafts[i];
        if (d == FastSpec::kNoToken) continue;
        if (first_valid_draft == FastSpec::kNoToken) first_valid_draft = d;
        if (d == target_argmax) {
            accepted_count = 1;
            break;
        }
    }

    step.accepted_count = accepted_count;
    step.accepted_token = target_argmax;

    // Off-path training: enqueue (anchor=last_token, predicted=draft0, actual=target).
    // FastSpec's drain thread updates the bigram table without touching us.
    m_spec->EnqueueValidation(last_token, first_valid_draft, target_argmax);

    // Stats (relaxed; readers tolerate eventual consistency).
    m_steps.fetch_add(1, std::memory_order_relaxed);
    m_draft_emit.fetch_add(emitted, std::memory_order_relaxed);
    m_draft_accepted.fetch_add(accepted_count, std::memory_order_relaxed);
    if (emitted == 0) {
        m_draft_zero_emit.fetch_add(1, std::memory_order_relaxed);
    }

    return step;
}

FastSpecInferenceBridge::Step
FastSpecInferenceBridge::GenerateToken(uint32_t                  last_token,
                                       const std::vector<float>& target_logits) noexcept
{
    return GenerateTokenWithArgmax(last_token, Argmax(target_logits));
}

FastSpecInferenceBridge::Step
FastSpecInferenceBridge::GenerateTokenSampled(uint32_t                  last_token,
                                              const std::vector<float>& target_logits,
                                              uint64_t*                 rng_state) noexcept
{
    Step step;
    if (!m_spec || target_logits.empty() || rng_state == nullptr || *rng_state == 0) {
        return step;
    }
    const size_t V = target_logits.size();

    // ---- Get K drafts from FastSpec (sparse buffer; pad slots are kNoToken).
    uint32_t drafts[kMaxDraftWidth];
    const size_t emitted = m_spec->SpeculateFast(last_token, drafts, m_cfg.draft_width);
    step.draft_count = static_cast<uint32_t>(emitted);

    // ---- Compute target softmax over the full vocab. We need probabilities
    //      for the rejection ratio AND for residual sampling on rejection.
    //      Numerically-stable two-pass softmax (max-subtract then exp/sum).
    float max_logit = target_logits[0];
    for (size_t i = 1; i < V; ++i) {
        if (target_logits[i] > max_logit) max_logit = target_logits[i];
    }
    double sum_exp = 0.0;
    for (size_t i = 0; i < V; ++i) {
        sum_exp += std::exp(static_cast<double>(target_logits[i] - max_logit));
    }
    if (!(sum_exp > 0.0)) {
        // Degenerate logits — fall back to argmax behaviour.
        return GenerateTokenWithArgmax(last_token, Argmax(target_logits));
    }
    const double inv_sum = 1.0 / sum_exp;

    auto target_prob = [&](uint32_t tok) -> double {
        if (tok >= V) return 0.0;
        return std::exp(static_cast<double>(target_logits[tok] - max_logit)) * inv_sum;
    };

    // ---- Probabilistic acceptance loop.
    //   p(d) ≈ 1/K_eff (uniform over emitted drafts; conservative).
    //   q(d)  = target_prob(d).
    //   Accept iff u01 < min(1, q(d) / p(d)) = min(1, q(d) * K_eff).
    const uint32_t K_eff = (emitted > 0) ? static_cast<uint32_t>(emitted) : 1u;
    uint32_t first_valid_draft = FastSpec::kNoToken;
    uint32_t accepted_count    = 0;
    uint32_t accepted_token    = FastSpec::kNoToken;

    for (uint32_t i = 0; i < m_cfg.draft_width; ++i) {
        const uint32_t d = drafts[i];
        if (d == FastSpec::kNoToken) continue;
        if (first_valid_draft == FastSpec::kNoToken) first_valid_draft = d;

        const double q = target_prob(d);
        const double accept_p = std::min(1.0, q * static_cast<double>(K_eff));
        const float  u = UniformF32(rng_state);
        if (static_cast<double>(u) < accept_p) {
            accepted_token  = d;
            accepted_count  = 1;
            break;
        }
    }

    bool used_residual = false;
    if (accepted_token == FastSpec::kNoToken) {
        // ---- All drafts rejected (or none emitted): sample fresh from
        //      target softmax via inverse-CDF lookup. This is what a normal
        //      verifier step would do, so we never lose a token.
        const float u = UniformF32(rng_state);
        double cdf = 0.0;
        const double target = static_cast<double>(u);
        uint32_t pick = static_cast<uint32_t>(V - 1);
        for (size_t i = 0; i < V; ++i) {
            cdf += std::exp(static_cast<double>(target_logits[i] - max_logit)) * inv_sum;
            if (cdf >= target) { pick = static_cast<uint32_t>(i); break; }
        }
        accepted_token = pick;
        used_residual  = true;
    }

    step.accepted_token = accepted_token;
    step.accepted_count = accepted_count;

    // ---- Off-path training. Treat the actual committed token as truth.
    m_spec->EnqueueValidation(last_token, first_valid_draft, accepted_token);

    // ---- Stats.
    m_steps.fetch_add(1, std::memory_order_relaxed);
    m_draft_emit.fetch_add(emitted, std::memory_order_relaxed);
    if (emitted == 0) {
        m_draft_zero_emit.fetch_add(1, std::memory_order_relaxed);
    }
    if (accepted_count == 1) {
        m_sampled_accepted.fetch_add(1, std::memory_order_relaxed);
    }
    if (used_residual) {
        m_sampled_residual.fetch_add(1, std::memory_order_relaxed);
    }
    return step;
}

FastSpecInferenceBridge::Stats FastSpecInferenceBridge::GetStats() const noexcept {
    Stats s;
    s.steps                 = m_steps.load(std::memory_order_relaxed);
    s.draft_emit            = m_draft_emit.load(std::memory_order_relaxed);
    s.draft_accepted        = m_draft_accepted.load(std::memory_order_relaxed);
    s.draft_zero_emit_steps = m_draft_zero_emit.load(std::memory_order_relaxed);
    s.sampled_accepted      = m_sampled_accepted.load(std::memory_order_relaxed);
    s.sampled_residual      = m_sampled_residual.load(std::memory_order_relaxed);

    if (s.draft_emit > 0) {
        s.acceptance_rate = static_cast<double>(s.draft_accepted + s.sampled_accepted) /
                            static_cast<double>(s.draft_emit);
    }
    if (s.steps > 0) {
        const double committed = static_cast<double>(s.steps) +
                                 static_cast<double>(s.draft_accepted + s.sampled_accepted);
        s.effective_speedup = committed / static_cast<double>(s.steps);
    }
    return s;
}

} // namespace RawrXD
