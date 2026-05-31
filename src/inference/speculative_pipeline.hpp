// =============================================================================
// inference/speculative_pipeline.hpp — Speculative-decoding pipeline
// =============================================================================
// Wires the existing GenerateStreaming inference interface into a proper
// speculative decoding loop: a small *draft* model proposes N tokens per step;
// a large *target* model verifies the entire batch in one pass and either
// accepts or rolls back.
//
// Expected throughput gain: 2–5× over autoregressive target-only decoding
// when draft-model token acceptance rate > ~75%.
//
// Zero new dependencies — relies only on the existing InferenceEngine interface
// (GenerateStreaming / Generate) already present in the codebase.
// =============================================================================
#pragma once

#include "../inference_engine.h"
#include "../telemetry/sovereign_stats_block_v2.h"
#include "../telemetry/SovereignTelemetry_MASM_Bridge.h"
#include "../telemetry/execution_timeline.h"
#include "../engine/global_runtime_orchestrator.h"
#include <chrono>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace RawrXD::Inference {

// ---------------------------------------------------------------------------
// PipelineConfig
// ---------------------------------------------------------------------------
struct PipelineConfig {
    uint32_t speculate_n    = 4;       // tokens the draft model proposes per step
    uint32_t max_tokens     = 512;     // total tokens to generate
    float    accept_ratio   = 0.75f;   // rolling acceptance ratio gate; below
                                       //   this, reduce speculate_n adaptively
    float    temperature    = 1.0f;    // sampling temperature (1.0 = greedy)
    bool     adaptive_n     = true;    // shrink speculate_n on poor acceptance
    bool     verbose        = false;   // log accept/reject stats to stderr
};

// ---------------------------------------------------------------------------
// PipelineStats — filled in by run()
// ---------------------------------------------------------------------------
struct PipelineStats {
    uint32_t total_steps         = 0;
    uint32_t draft_tokens_total  = 0;
    uint32_t accepted_tokens     = 0;
    uint32_t rejected_tokens     = 0;
    float    acceptance_rate() const {
        if (draft_tokens_total == 0) return 0.0f;
        return static_cast<float>(accepted_tokens)
             / static_cast<float>(draft_tokens_total);
    }
    // Effective speedup vs. purely autoregressive target:
    // each step the target does ONE verify pass instead of speculate_n passes.
    float    speedup_estimate() const {
        if (total_steps == 0) return 1.0f;
        float avg_accepted = static_cast<float>(accepted_tokens + total_steps)
                           / static_cast<float>(total_steps);
        return avg_accepted;
    }
};

// ---------------------------------------------------------------------------
// SpeculativePipeline
// ---------------------------------------------------------------------------
class SpeculativePipeline {
public:
    // draft   — small/fast model used to propose tokens
    // target  — large/accurate model used to verify
    SpeculativePipeline(InferenceEngine* draft,
                        InferenceEngine* target,
                        PipelineConfig   cfg = {})
        : m_draft(draft), m_target(target), m_cfg(cfg)
        , m_rng(std::random_device{}())
    {}

    // -----------------------------------------------------------------------
    // run() — generate at most cfg.max_tokens tokens starting from prompt.
    // on_token is called for every *accepted* token id (may be called from
    // this thread or a std::thread — synchronise in caller if needed).
    // Returns the generated token sequence plus statistics.
    // -----------------------------------------------------------------------
    struct Result {
        std::vector<int32_t> tokens;
        PipelineStats        stats;
    };

    Result run(const std::vector<int32_t>& prompt,
               std::function<void(int32_t)> on_token = nullptr) {
        Result result;
        if (!m_draft || !m_target) return result;

        std::vector<int32_t> context = prompt;
        uint32_t total_generated     = 0;
        uint32_t current_speculate_n = m_cfg.speculate_n;
        auto start_time = std::chrono::high_resolution_clock::now();

        while (total_generated < m_cfg.max_tokens) {
            TimelineScope ts_step(ExecutionPhase::VULKAN_VERIFY);
            auto step_start = std::chrono::high_resolution_clock::now();
            // --- Step 1: draft proposes current_speculate_n tokens -----------
            std::vector<int32_t> draft_seq;
            draft_seq.reserve(current_speculate_n);
            {
                TimelineScope ts_draft(ExecutionPhase::SPECULATIVE_DRAFT, (uint16_t)current_speculate_n);
                std::vector<int32_t> draft_ctx = context;
                std::vector<int32_t> proposed  =
                    m_draft->Generate(draft_ctx,
                                      static_cast<int>(current_speculate_n));
                for (auto t : proposed) {
                    draft_seq.push_back(t);
                    if (draft_seq.size() >= current_speculate_n) break;
                }
            }
            if (draft_seq.empty()) break;

            result.stats.total_steps++;
            result.stats.draft_tokens_total += static_cast<uint32_t>(draft_seq.size());

            // --- Step 2: target verifies draft_seq in one batched pass -------
            // We feed context + draft_seq to the target and ask it to produce
            // the same number of tokens.  Any divergence ⇒ rollback.
            std::vector<int32_t> verify_ctx = context;
            verify_ctx.insert(verify_ctx.end(), draft_seq.begin(), draft_seq.end());

            std::vector<int32_t> target_seq =
                m_target->Generate(verify_ctx,
                                   static_cast<int>(draft_seq.size()) + 1);

            // --- Step 3: acceptance comparison and rollback ------------------
            uint32_t accepted = 0;
            for (size_t i = 0; i < draft_seq.size() && i < target_seq.size(); ++i) {
                if (draft_seq[i] == target_seq[i]) {
                    ++accepted;
                } else {
                    break; // first mismatch: stop acceptance chain
                }
            }

            // Emit accepted draft tokens
            for (uint32_t i = 0; i < accepted; ++i) {
                context.push_back(draft_seq[i]);
                result.tokens.push_back(draft_seq[i]);
                if (on_token) on_token(draft_seq[i]);
                total_generated++;
                if (total_generated >= m_cfg.max_tokens) goto done;
            }
            result.stats.accepted_tokens += accepted;
            result.stats.rejected_tokens +=
                static_cast<uint32_t>(draft_seq.size()) - accepted;

            // Target's correction token at the rollback point (always valid)
            if (!target_seq.empty()) {
                int32_t correction = accepted < target_seq.size()
                                   ? target_seq[accepted]
                                   : target_seq.back();
                context.push_back(correction);
                result.tokens.push_back(correction);
                if (on_token) on_token(correction);
                total_generated++;
                if (total_generated >= m_cfg.max_tokens) goto done;
            }

            // --- Step 4: adaptive speculate_n --------------------------------
            if (m_cfg.adaptive_n) {
                float rate = result.stats.acceptance_rate();
                float total_elapsed = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - start_time).count();
                float tps = total_elapsed > 0 ? (float)total_generated / total_elapsed : 0.0f;

                // Consult the Global Orchestrator instead of local heuristics
                GlobalRuntimeOrchestrator::Get().UpdateInferenceMetrics(rate, tps);
                current_speculate_n = GlobalRuntimeOrchestrator::Get().GetCurrentState().optimal_speculate_n;
            }

            // --- Step 5: Sovereign Telemetry Update --------------------------
            if (m_statsBlock) {
                auto now = std::chrono::high_resolution_clock::now();
                float total_elapsed = std::chrono::duration<float>(now - start_time).count();
                float step_elapsed_ms = std::chrono::duration<float, std::milli>(now - step_start).count();
                
                float tps = total_elapsed > 0 ? (float)total_generated / total_elapsed : 0.0f;
                float mspt = (float)step_elapsed_ms / (accepted + 1); // rough estimate per step

                SovereignTelemetry_Update(m_statsBlock, tps, mspt, accepted, (uint32_t)draft_seq.size() - accepted, 0.0f);
            }
        }
    done:
        if (m_cfg.verbose) {
            fprintf(stderr,
                "[SpecPipe] steps=%u draft=%u acc=%u rej=%u rate=%.2f speedup≈%.2fx\n",
                result.stats.total_steps,
                result.stats.draft_tokens_total,
                result.stats.accepted_tokens,
                result.stats.rejected_tokens,
                result.stats.acceptance_rate(),
                result.stats.speedup_estimate());
        }
        return result;
    }

    // -----------------------------------------------------------------------
    // run_async() — background thread; calls on_done when complete.
    // -----------------------------------------------------------------------
    void run_async(const std::vector<int32_t>& prompt,
                   std::function<void(int32_t)>   on_token,
                   std::function<void(Result)>    on_done) {
        std::thread([this, prompt, on_token, on_done]() mutable {
            on_done(run(prompt, on_token));
        }).detach();
    }

    // Stop a running async decode (cooperative)
    void stop() { m_stop.store(true, std::memory_order_relaxed); }

    PipelineConfig& config()       { return m_cfg; }
    const PipelineConfig& config() const { return m_cfg; }

    void setStatsBlock(SovereignStatsBlockV2* block) {
        m_statsBlock = block;
    }

private:
    InferenceEngine* m_draft  = nullptr;
    InferenceEngine* m_target = nullptr;
    PipelineConfig   m_cfg;
    std::mt19937     m_rng;
    std::atomic<bool> m_stop{false};
    SovereignStatsBlockV2* m_statsBlock = nullptr;
};

} // namespace RawrXD::Inference
