// ============================================================================
// speculative_inference_bridge.h
// ============================================================================
// Bridges SpeculativeDecoderV2 to the RawrXD inference stack.
//
// Usage:
//   SpeculativeInferenceBridge bridge;
//   bridge.setDraftModel("path/to/small_draft.gguf");
//   bridge.setTargetModel("path/to/full_target.gguf");
//   bridge.configure({ .maxDraftTokens = 5, .adaptiveDraftLen = true });
//
//   bridge.generateStreaming(
//       promptTokens, maxNewTokens,
//       [](const Token& tok, bool) { outputToken(tok); });
//
// Design notes:
//   - The bridge owns two AutonomousInferenceEngine instances (draft + target).
//   - ModelInference callbacks use a generate-and-compare approximation for
//     logprobs: the top token receives logprob 0.0 (prob=1.0), the remaining
//     (topK-1) synthetic candidates receive log(1/(topK * rank)) to form a
//     synthetic decreasing distribution.  This is replaced by real logprobs
//     when the underlying engine exposes a computeLogprobs() API.
//   - The bridge is thread-safe: generateStreaming() holds a per-instance lock.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <cmath>
#include <functional>

#include "speculative_decoder_v2.h"
#include "../inference/ultra_fast_inference.h"

namespace RawrXD {
namespace Speculative {

// ============================================================================
// SpeculativeInferenceBridge
// ============================================================================

class SpeculativeInferenceBridge {
public:
    SpeculativeInferenceBridge();
    ~SpeculativeInferenceBridge();

    // ---- Model Setup ----

    // Set draft model path (small/fast model).  Must be called before generate.
    void setDraftModel(const std::string& modelPath);

    // Set target model path (large/accurate model).  Must be called before generate.
    void setTargetModel(const std::string& modelPath);

    // ---- Configuration ----

    void configure(const SpeculationConfig& cfg);
    const SpeculationConfig& getConfig() const { return m_config; }

    // ---- Generation ----

    using TokenStreamCallback = std::function<void(const Token& tok)>;

    // Generate maxNewTokens tokens from the given prompt token IDs.
    // Fires callback for each accepted/corrected token in order.
    SpeculativeDecoderV2::GenerateResult generateStreaming(
        const std::vector<int>& promptTokens,
        int maxNewTokens,
        TokenStreamCallback callback = nullptr);

    // Convenience overload: generate from text using target model's tokenizer.
    SpeculativeDecoderV2::GenerateResult generateFromText(
        const std::string& prompt,
        int maxNewTokens,
        TokenStreamCallback callback = nullptr);

    // ---- Statistics ----

    AcceptanceStats getStats() const;
    void resetStats();

    // ---- Abort ----

    void abort();

private:
    std::string m_draftPath;
    std::string m_targetPath;
    SpeculationConfig m_config;
    std::mutex m_mutex;

    // Lazily-initialised inference engines (one per model).
    std::unique_ptr<rawrxd::inference::AutonomousInferenceEngine> m_draftEngine;
    std::unique_ptr<rawrxd::inference::AutonomousInferenceEngine> m_targetEngine;
    bool m_draftLoaded{false};
    bool m_targetLoaded{false};

    // Initialise engines on first call.
    bool ensureLoaded();

    // Build ModelInference descriptors backed by each engine.
    ModelInference buildDraftInference();
    ModelInference buildTargetInference();

    // ---- Synthetic logprob helpers ----
    //
    // Produce a top-K logprob vector from a single greedy-generated token for
    // an engine.  The greedy token receives logprob 0.0 (probability 1.0);
    // synthetic runners-up receive decreasing synthetic logprobs to form a
    // plausible distribution for the rejection-sampling acceptance test.
    //
    // This is intentionally non-static so it can hold per-engine lock state.
    std::vector<std::pair<int, float>> syntheticTopK(
        const std::vector<int>& context, int topK,
        rawrxd::inference::AutonomousInferenceEngine& engine, int seed);
};

} // namespace Speculative
} // namespace RawrXD
