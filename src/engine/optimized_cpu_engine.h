#pragma once
// OptimizedCPUEngine — integrates all RawrXD inference optimisation layers:
//   Q4_0/Q8_0 SIMD kernels, KV ring-buffer, Flash Attention 2, work-stealing
//   pool, prefetch pipeline, optimised sampler, and huge-page weight storage.

#include "../kernels/q4_0_gemm.h"
#include "../kv_cache/ring_buffer.h"
#include "../kernels/flash_attention.h"
#include "../threading/work_stealing_pool.h"
#include "../kernels/prefetch_pipeline.h"
#include "../sampling/optimized_sampler.h"
#include "../memory/huge_page_allocator.h"
#include "../inference/batched_decoder.h"

#include <cassert>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RawrXD {

// ─── Weight layout ───────────────────────────────────────────────────────────

struct LayerWeights {
    // Attention
    std::vector<Kernels::Q4_0Block> wq, wk, wv, wo;
    std::vector<float>              rms_attn;  // RMSNorm scale (fp32)
    // FFN
    std::vector<Kernels::Q4_0Block> w_gate, w_up, w_down;
    std::vector<float>              rms_ffn;
};

struct ModelWeights {
    std::vector<LayerWeights> layers;
    std::vector<float>        token_embed;    // [vocab_size × d_model]
    std::vector<float>        rms_final;      // [d_model]
    std::vector<float>        lm_head;        // [d_model × vocab_size] (fp32)
    size_t                    vocab_size  = 0;
    size_t                    d_model     = 0;
    size_t                    n_heads     = 0;
    size_t                    n_kv_heads  = 0;
    size_t                    head_dim    = 0;
    size_t                    ffn_dim     = 0;
};

// ─── Engine ──────────────────────────────────────────────────────────────────

class OptimizedCPUEngine {
public:
    struct Config {
        size_t n_threads   = 0;    // 0 = hardware_concurrency
        size_t ctx_len     = 4096;
        float  top_p       = 0.9f;
        float  temperature = 0.8f;
        OptimizedSampler::RepetitionPenaltyArgs rep_args{};
    };

    OptimizedCPUEngine(ModelWeights weights, Config cfg = {})
        : m_w(std::move(weights))
        , m_cfg(cfg)
        , m_pool(cfg.n_threads > 0 ? cfg.n_threads : std::thread::hardware_concurrency())
        , m_prefetch(m_w.layers.size())
        , m_sampler(/* seed */ 42)
    {
        // Set up KV cache (one per layer)
        KVCacheRingBuffer::Config kv_cfg;
        kv_cfg.n_layers   = m_w.layers.size();
        kv_cfg.n_kv_heads = m_w.n_kv_heads;
        kv_cfg.head_dim   = m_w.head_dim;
        kv_cfg.max_seq_len = m_cfg.ctx_len;
        m_kv = std::make_unique<KVCacheRingBuffer>(kv_cfg);

        // Register layer pointers with the prefetch pipeline
        for (size_t l = 0; l < m_w.layers.size(); ++l) {
            const auto& lw = m_w.layers[l];
            if (!lw.wq.empty()) {
                m_prefetch.registerLayer(l, lw.wq.data(),
                    lw.wq.size() * sizeof(Kernels::Q4_0Block));
            }
        }
    }

    // Generate `max_new_tokens` tokens from `prompt`.  Calls `on_token` for
    // each produced token so the caller can stream output.
    void generateStreaming(
        const std::vector<int32_t>& prompt,
        size_t max_new_tokens,
        std::function<bool(int32_t)> on_token  // return false to stop
    ) {
        if (prompt.empty()) return;

        std::vector<int32_t> context(prompt);
        m_kv->reset();

        // Prefill the prompt
        std::vector<float> hidden = embed(context);
        for (size_t t = 0; t < context.size(); ++t) {
            hidden = partialForward(hidden, t, t < context.size() - 1);
        }

        // Decode loop
        for (size_t step = 0; step < max_new_tokens; ++step) {
            int32_t token = sampleNextToken(hidden, context);
            context.push_back(token);
            if (!on_token(token)) break;

            // Next token embedding
            hidden = embed({token});
            hidden = partialForward(hidden, context.size() - 1, false);
        }
    }

    // Single-token forward pass.  Returns logits (vocab_size).
    std::vector<float> forwardToken(int32_t token, size_t pos) {
        std::vector<float> h = embed({token});
        h = partialForward(h, pos, false);
        return computeLMHead(h);
    }

private:
    // ── Embedding lookup ────────────────────────────────────────────────────
    std::vector<float> embed(const std::vector<int32_t>& tokens) const {
        const size_t d = m_w.d_model;
        std::vector<float> out(tokens.size() * d);
        for (size_t i = 0; i < tokens.size(); ++i) {
            int32_t tok = tokens[i];
            assert(tok >= 0 && (size_t)tok < m_w.vocab_size);
            std::memcpy(out.data() + i * d,
                        m_w.token_embed.data() + (size_t)tok * d,
                        d * sizeof(float));
        }
        return out;
    }

    // ── Full layer stack (last token only after prefill) ────────────────────
    std::vector<float> partialForward(
        const std::vector<float>& h_in,
        size_t  pos,
        bool    /* is_prefill */
    ) {
        const size_t d     = m_w.d_model;
        const size_t n_lay = m_w.layers.size();

        std::vector<float> h(h_in);  // residual stream

        for (size_t l = 0; l < n_lay; ++l) {
            // Kick off prefetch for the next layer
            if (l + 1 < n_lay) {
                Kernels::LayerComputeGuard guard(m_prefetch, l + 1);
                (void)guard;
            }

            h = computeLayer(h, pos, l);
        }

        // Final RMSNorm
        rmsNorm(h, m_w.rms_final);
        return h;
    }

    // ── Single transformer layer ─────────────────────────────────────────────
    std::vector<float> computeLayer(
        const std::vector<float>& h_in,
        size_t pos, size_t layer_idx
    ) {
        const auto& lw = m_w.layers[layer_idx];
        const size_t d = m_w.d_model;

        std::vector<float> h(h_in);

        // 1. RMSNorm (attention)
        rmsNorm(h, lw.rms_attn);

        // 2. QKV projections (Q4_0 GEMV)
        std::vector<float> q = gemvQ4(h, lw.wq, m_w.n_heads    * m_w.head_dim);
        std::vector<float> k = gemvQ4(h, lw.wk, m_w.n_kv_heads * m_w.head_dim);
        std::vector<float> v = gemvQ4(h, lw.wv, m_w.n_kv_heads * m_w.head_dim);

        // 3. Append to KV cache
        m_kv->append(layer_idx, pos, k.data(), v.data());

        // 4. Flash Attention 2 dispatch
        std::vector<float> attn_out(m_w.n_heads * m_w.head_dim, 0.0f);
        {
            auto [kh, vh] = m_kv->getKVRange(layer_idx, 0, m_kv->seqLen());

            if (m_w.head_dim == 64) {
                Kernels::flash_attention_2<64>(
                    q.data(), kh, vh,
                    attn_out.data(),
                    m_w.n_heads, m_w.n_kv_heads,
                    (size_t)m_kv->seqLen()
                );
            } else {
                Kernels::flash_attention_2<128>(
                    q.data(), kh, vh,
                    attn_out.data(),
                    m_w.n_heads, m_w.n_kv_heads,
                    (size_t)m_kv->seqLen()
                );
            }
        }

        // 5. Output projection + residual
        std::vector<float> attn_proj = gemvQ4(attn_out, lw.wo, d);
        for (size_t i = 0; i < d; ++i) h[i] = h_in[i] + attn_proj[i];

        // 6. RMSNorm (FFN)
        std::vector<float> h2(h);
        rmsNorm(h2, lw.rms_ffn);

        // 7. SwiGLU FFN
        std::vector<float> gate = gemvQ4(h2, lw.w_gate, m_w.ffn_dim);
        std::vector<float> up   = gemvQ4(h2, lw.w_up,   m_w.ffn_dim);

        // SiLU(gate) * up
        for (size_t i = 0; i < m_w.ffn_dim; ++i) {
            gate[i] = gate[i] / (1.0f + std::exp(-gate[i]));  // SiLU
            gate[i] *= up[i];
        }

        std::vector<float> ffn_out = gemvQ4(gate, lw.w_down, d);
        for (size_t i = 0; i < d; ++i) h[i] += ffn_out[i];

        return h;
    }

    // ── LM head ─────────────────────────────────────────────────────────────
    std::vector<float> computeLMHead(const std::vector<float>& h) const {
        const size_t d = m_w.d_model;
        const size_t v = m_w.vocab_size;
        std::vector<float> logits(v, 0.0f);

        m_pool.parallelFor(0, v, [&](size_t i) {
            float acc = 0.0f;
            const float* row = m_w.lm_head.data() + i * d;
            for (size_t j = 0; j < d; ++j) acc += h[j] * row[j];
            logits[i] = acc;
        });

        return logits;
    }

    // ── Sampling ────────────────────────────────────────────────────────────
    int32_t sampleNextToken(
        const std::vector<float>& h,
        const std::vector<int32_t>& context
    ) {
        auto logits = computeLMHead(h);
        m_sampler.applyRepetitionPenalty(logits, context, m_cfg.rep_args);
        return m_sampler.sampleTopP(logits, m_cfg.top_p, m_cfg.temperature);
    }

    // ── Helpers ─────────────────────────────────────────────────────────────
    static void rmsNorm(std::vector<float>& x, const std::vector<float>& scale) {
        const size_t n = x.size();
        assert(scale.size() == n);
        float sum_sq = 0.0f;
        for (float v : x) sum_sq += v * v;
        float inv_rms = 1.0f / std::sqrt(sum_sq / (float)n + 1e-5f);
        for (size_t i = 0; i < n; ++i) x[i] *= inv_rms * scale[i];
    }

    // Thin wrapper: Q4_0 GEMV using the kernel from q4_0_gemm.h
    static std::vector<float> gemvQ4(
        const std::vector<float>&           x,
        const std::vector<Kernels::Q4_0Block>& w,
        size_t                              out_dim
    ) {
        const size_t in_dim = x.size();
        std::vector<float> out(out_dim, 0.0f);
        Kernels::gemv_q4_0_fp32_avx2(
            out.data(), x.data(), w.data(),
            (int)out_dim, (int)in_dim
        );
        return out;
    }

    // ── State ────────────────────────────────────────────────────────────────
    ModelWeights                       m_w;
    Config                             m_cfg;
    mutable WorkStealingPool           m_pool;
    Kernels::PrefetchPipeline          m_prefetch;
    OptimizedSampler                   m_sampler;
    std::unique_ptr<KVCacheRingBuffer> m_kv;
};

} // namespace RawrXD
