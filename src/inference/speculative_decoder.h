#pragma once
#include <functional>
#include <random>
#include <vector>

namespace RawrXD {

// Speculative decoding: a compact draft model proposes N tokens; the target
// model verifies them all in one batched forward pass, then either accepts or
// rolls back.
//
// Template parameters
//   DraftModel   – must expose:  std::vector<float> forward(token_id)
//   TargetModel  – must expose:  std::vector<std::vector<float>> forwardBatch(tokens)
//                                and:  std::vector<float> forward(token_id)

template<typename DraftModel, typename TargetModel>
class SpeculativeDecoder {
public:
    struct Config {
        size_t speculate_n    = 4;      // draft tokens to propose each step
        float  accept_thresh  = 0.85f;  // minimum acceptance ratio before giving up
        size_t vocab_size     = 32000;
    };

    SpeculativeDecoder(DraftModel& draft, TargetModel& target, Config cfg = {})
        : m_draft(draft), m_target(target), m_cfg(cfg)
        , m_rng(std::random_device{}())
    {}

    // Generate one verified token.  Returns the accepted token id.
    int32_t step(int32_t last_token) {
        // 1. Draft model proposes speculate_n candidate tokens
        std::vector<int32_t>              draft_tokens;
        std::vector<std::vector<float>>   draft_probs;

        int32_t in_tok = last_token;
        draft_tokens.reserve(m_cfg.speculate_n);
        draft_probs.reserve(m_cfg.speculate_n);

        for (size_t i = 0; i < m_cfg.speculate_n; ++i) {
            std::vector<float> logits = m_draft.forward(in_tok);
            softmax(logits);
            int32_t tok = sampleGreedy(logits);
            draft_tokens.push_back(tok);
            draft_probs.push_back(std::move(logits));
            in_tok = tok;
        }

        // 2. Target model verifies the full draft sequence in one batched call
        auto target_logits_batch = m_target.forwardBatch(draft_tokens);

        // 3. Acceptance sampling (speculative decoding acceptance criterion)
        size_t accept_count = 0;
        for (size_t i = 0; i < m_cfg.speculate_n; ++i) {
            softmax(target_logits_batch[i]);

            float p_draft  = draft_probs[i][draft_tokens[i]];
            float p_target = target_logits_batch[i][draft_tokens[i]];

            float accept_prob = std::min(1.0f, p_target / (p_draft + 1e-9f));

            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(m_rng) <= accept_prob) {
                ++accept_count;
            } else {
                break;  // roll back from here
            }
        }

        if (accept_count == m_cfg.speculate_n) {
            // All accepted: target generates one bonus token
            std::vector<float> bonus = m_target.forward(draft_tokens.back());
            softmax(bonus);
            return sampleGreedy(bonus);
        }

        // Partial acceptance: return the first accepted draft token, or fall
        // back to a target-sampled token at the rejection point.
        if (accept_count > 0) {
            return draft_tokens[accept_count - 1];
        }

        // Rejection at position 0: resample from adjusted distribution
        //   p_adjusted = normalise( max(0, p_target - p_draft) )
        std::vector<float> adjusted = target_logits_batch[0];
        for (size_t v = 0; v < adjusted.size(); ++v) {
            adjusted[v] = std::max(0.0f, adjusted[v] - draft_probs[0][v]);
        }
        float sum = 0.0f;
        for (float v : adjusted) sum += v;
        if (sum < 1e-9f) return sampleGreedy(target_logits_batch[0]);
        for (float& v : adjusted) v /= sum;
        return sampleFromDist(adjusted);
    }

private:
    static void softmax(std::vector<float>& v) {
        float mx = *std::max_element(v.begin(), v.end());
        float s  = 0.0f;
        for (float& x : v) { x = std::exp(x - mx); s += x; }
        for (float& x : v) x /= s;
    }

    static int32_t sampleGreedy(const std::vector<float>& probs) {
        return (int32_t)std::distance(probs.begin(),
            std::max_element(probs.begin(), probs.end()));
    }

    int32_t sampleFromDist(const std::vector<float>& probs) {
        std::discrete_distribution<int32_t> dist(probs.begin(), probs.end());
        return dist(m_rng);
    }

    DraftModel&   m_draft;
    TargetModel&  m_target;
    Config        m_cfg;
    std::mt19937  m_rng;
};

} // namespace RawrXD
