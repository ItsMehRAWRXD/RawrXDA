#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace RawrXD {

// Continuous-batching decoder.
// Supports both prefill (prompt ingestion) and per-step token generation
// for a dynamic batch of sequences.
class BatchedDecoder {
public:
    struct SequenceState {
        int32_t              seq_id     = -1;
        std::vector<int32_t> tokens;          // full token history
        size_t               kv_len    = 0;   // how many tokens are in the KV cache
        bool                 done      = false;
    };

    using ForwardFn = std::function<
        std::vector<std::vector<float>>(
            const std::vector<SequenceState*>& seqs,
            const std::vector<int32_t>&        input_tokens)
    >;

    explicit BatchedDecoder(ForwardFn fwd, size_t max_batch = 8)
        : m_fwd(std::move(fwd)), m_maxBatch(max_batch)
    {}

    // Add a new sequence for the next batch step.
    void addSequence(SequenceState* seq) {
        m_active.push_back(seq);
    }

    // Prefill prompt tokens for all queued sequences.
    // Returns per-sequence logits for the last token only.
    std::vector<std::vector<float>> prefillBatch() {
        std::vector<int32_t> flat_tokens;
        for (auto* s : m_active) {
            for (int32_t t : s->tokens) flat_tokens.push_back(t);
        }
        if (flat_tokens.empty()) return {};

        auto results = m_fwd(m_active, flat_tokens);

        // Update kv_len for each sequence
        size_t off = 0;
        for (auto* s : m_active) {
            s->kv_len = s->tokens.size();
            off += s->tokens.size();
        }

        return results;
    }

    // Decode one token per active sequence (continuous batching step).
    // `sample_fn` receives per-sequence logits and returns chosen token.
    using SampleFn = std::function<int32_t(int32_t seq_id, std::vector<float>&)>;

    std::vector<int32_t> decodeBatch(SampleFn sample_fn) {
        // Gather the last token per active sequence
        std::vector<int32_t> input_tokens;
        input_tokens.reserve(m_active.size());
        for (auto* s : m_active) {
            input_tokens.push_back(s->tokens.empty() ? 0 : s->tokens.back());
        }

        auto logits_batch = m_fwd(m_active, input_tokens);

        std::vector<int32_t> output;
        output.reserve(m_active.size());

        for (size_t i = 0; i < m_active.size(); ++i) {
            int32_t tok = sample_fn(m_active[i]->seq_id, logits_batch[i]);
            m_active[i]->tokens.push_back(tok);
            m_active[i]->kv_len++;
            output.push_back(tok);
        }

        return output;
    }

    // Attention helper: compute cross-attention scores between a query batch
    // and flattened keys/values (simple scaled dot-product, no softmax chunking).
    //
    // Q: [n_heads, head_dim]   K,V: [seq_len, n_heads, head_dim]
    static std::vector<float> batchAttention(
        const float* Q, const float* K, const float* V,
        size_t n_heads, size_t head_dim, size_t seq_len)
    {
        std::vector<float> out(n_heads * head_dim, 0.0f);
        const float scale = 1.0f / std::sqrt((float)head_dim);

        for (size_t h = 0; h < n_heads; ++h) {
            const float* q = Q + h * head_dim;

            // Compute attention scores
            std::vector<float> scores(seq_len);
            for (size_t t = 0; t < seq_len; ++t) {
                const float* k = K + t * n_heads * head_dim + h * head_dim;
                float dot = 0.0f;
                for (size_t d = 0; d < head_dim; ++d) dot += q[d] * k[d];
                scores[t] = dot * scale;
            }

            // Softmax
            float mx = *std::max_element(scores.begin(), scores.end());
            float s  = 0.0f;
            for (float& sc : scores) { sc = std::exp(sc - mx); s += sc; }
            for (float& sc : scores) sc /= s;

            // Weighted sum of values
            float* o = out.data() + h * head_dim;
            for (size_t t = 0; t < seq_len; ++t) {
                const float* v = V + t * n_heads * head_dim + h * head_dim;
                for (size_t d = 0; d < head_dim; ++d) o[d] += scores[t] * v[d];
            }
        }
        return out;
    }

    size_t activeCount() const { return m_active.size(); }

    void removeCompleted() {
        m_active.erase(
            std::remove_if(m_active.begin(), m_active.end(),
                [](const SequenceState* s) { return s->done; }),
            m_active.end()
        );
    }

private:
    ForwardFn                       m_fwd;
    size_t                          m_maxBatch;
    std::vector<SequenceState*>     m_active;
};

} // namespace RawrXD
