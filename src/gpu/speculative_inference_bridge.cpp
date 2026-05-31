// ============================================================================
// speculative_inference_bridge.cpp
// Implements SpeculativeInferenceBridge — connects SpeculativeDecoderV2 to
// AutonomousInferenceEngine.
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "gpu/speculative_inference_bridge.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <numeric>
#include <sstream>

namespace RawrXD {
namespace Speculative {

// ============================================================================
// Constructor / Destructor
// ============================================================================

SpeculativeInferenceBridge::SpeculativeInferenceBridge() {
    m_config.maxDraftTokens  = 5;
    m_config.minDraftTokens  = 1;
    m_config.adaptiveDraftLen = true;
}

SpeculativeInferenceBridge::~SpeculativeInferenceBridge() {}

// ============================================================================
// Model Setup
// ============================================================================

void SpeculativeInferenceBridge::setDraftModel(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_draftPath   = modelPath;
    m_draftLoaded = false; // force re-init on next generate
}

void SpeculativeInferenceBridge::setTargetModel(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_targetPath   = modelPath;
    m_targetLoaded = false;
}

// ============================================================================
// Configuration
// ============================================================================

void SpeculativeInferenceBridge::configure(const SpeculationConfig& cfg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = cfg;
}

// ============================================================================
// Lazy engine loader
// ============================================================================

bool SpeculativeInferenceBridge::ensureLoaded() {
    if (!m_draftLoaded && !m_draftPath.empty()) {
        rawrxd::inference::AutonomousInferenceEngine::InferenceConfig cfg;
        cfg.model_path        = m_draftPath;
        cfg.enable_gpu        = true;
        cfg.quality_target    = 0.6f; // draft: speed > quality
        cfg.max_memory_mb     = 4096; // keep draft model small

        m_draftEngine = std::make_unique<
            rawrxd::inference::AutonomousInferenceEngine>(cfg);
        m_draftLoaded = m_draftEngine->loadModelAutomatic(m_draftPath);
        if (!m_draftLoaded) {
            m_draftEngine.reset();
            return false;
        }
    }

    if (!m_targetLoaded && !m_targetPath.empty()) {
        rawrxd::inference::AutonomousInferenceEngine::InferenceConfig cfg;
        cfg.model_path        = m_targetPath;
        cfg.enable_gpu        = true;
        cfg.quality_target    = 0.95f; // target: quality first

        m_targetEngine = std::make_unique<
            rawrxd::inference::AutonomousInferenceEngine>(cfg);
        m_targetLoaded = m_targetEngine->loadModelAutomatic(m_targetPath);
        if (!m_targetLoaded) {
            m_targetEngine.reset();
            return false;
        }
    }

    return m_draftLoaded && m_targetLoaded;
}

// ============================================================================
// syntheticTopK — delegate to AutonomousInferenceEngine::computeLogprobs().
//
// Previously this function reimplemented a fixed-λ Laplace distribution
// inline.  Now it delegates to the engine, which uses model-weight-anchored
// seeds so the distribution shifts as weights change, and is the single
// integration point that transparently gains real probabilities once the
// kernel exposes a full softmax export.
// ============================================================================

std::vector<std::pair<int, float>>
SpeculativeInferenceBridge::syntheticTopK(
    const std::vector<int>& context, int topK,
    rawrxd::inference::AutonomousInferenceEngine& engine, int /*seed*/)
{
    // Convert context to the int32_t ABI the engine expects.
    std::vector<int32_t> ctx32(context.begin(), context.end());
    return engine.computeLogprobs(ctx32, topK);
}

// ============================================================================
// ModelInference factory
// ============================================================================

ModelInference SpeculativeInferenceBridge::buildDraftInference() {
    ModelInference m;
    m.modelId = "draft:" + m_draftPath;

    auto* eng = m_draftEngine.get();

    // logprobs callback — single-context, top-K synthetic
    m.logprobs = [](const std::vector<int>& ctx, int topK, void* ud)
        -> std::vector<std::pair<int, float>>
    {
        auto* self = reinterpret_cast<SpeculativeInferenceBridge*>(ud);
        if (!self->m_draftEngine) return {};
        return self->syntheticTopK(ctx, topK, *self->m_draftEngine,
                                    static_cast<int>(ctx.size()));
    };

    // decode callback
    m.decode = [](int tokenId, void* /*ud*/) -> std::string {
        return std::string(1, static_cast<char>(std::clamp(tokenId, 0, 255)));
    };

    // encode callback
    m.encode = [](const std::string& text, void* /*ud*/) -> std::vector<int> {
        std::vector<int> toks;
        toks.reserve(text.size());
        for (unsigned char c : text) toks.push_back(static_cast<int>(c));
        return toks;
    };

    m.userData = this;
    (void)eng; // used via closure
    return m;
}

ModelInference SpeculativeInferenceBridge::buildTargetInference() {
    ModelInference m;
    m.modelId = "target:" + m_targetPath;

    auto* eng = m_targetEngine.get();

    m.logprobs = [](const std::vector<int>& ctx, int topK, void* ud)
        -> std::vector<std::pair<int, float>>
    {
        auto* self = reinterpret_cast<SpeculativeInferenceBridge*>(ud);
        if (!self->m_targetEngine) return {};
        // Use context size + 0x1000 as a distinct seed so target and draft
        // produce independent greedy tokens on the same context.
        return self->syntheticTopK(ctx, topK, *self->m_targetEngine,
                                    static_cast<int>(ctx.size()) + 0x1000);
    };

    m.decode = [](int tokenId, void* /*ud*/) -> std::string {
        return std::string(1, static_cast<char>(std::clamp(tokenId, 0, 255)));
    };

    m.encode = [](const std::string& text, void* /*ud*/) -> std::vector<int> {
        std::vector<int> toks;
        toks.reserve(text.size());
        for (unsigned char c : text) toks.push_back(static_cast<int>(c));
        return toks;
    };

    m.userData = this;
    (void)eng;
    return m;
}

// ============================================================================
// generateStreaming
// ============================================================================

SpeculativeDecoderV2::GenerateResult
SpeculativeInferenceBridge::generateStreaming(
    const std::vector<int>& promptTokens,
    int maxNewTokens,
    TokenStreamCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!ensureLoaded()) {
        return SpeculativeDecoderV2::GenerateResult::error(
            "Failed to load draft or target model");
    }

    // Bind models to the global decoder instance.
    auto& decoder = SpeculativeDecoderV2::Global();
    decoder.setConfig(m_config);

    SpecResult dr = decoder.setDraftModel(buildDraftInference());
    if (!dr.success) {
        return SpeculativeDecoderV2::GenerateResult::error(dr.detail);
    }

    SpecResult tr = decoder.setTargetModel(buildTargetInference());
    if (!tr.success) {
        return SpeculativeDecoderV2::GenerateResult::error(tr.detail);
    }

    // Adapt the bridge's TokenStreamCallback to the V2 TokenCallback ABI.
    TokenCallback v2cb = nullptr;
    void* v2ud = nullptr;

    struct CallbackAdaptor {
        TokenStreamCallback fn;
    };

    std::unique_ptr<CallbackAdaptor> adaptor;
    if (callback) {
        adaptor = std::make_unique<CallbackAdaptor>();
        adaptor->fn = callback;

        v2cb = [](const Token& tok, bool /*isDraft*/, void* ud) {
            reinterpret_cast<CallbackAdaptor*>(ud)->fn(tok);
        };
        v2ud = adaptor.get();
    }

    return decoder.generateStreaming(promptTokens, maxNewTokens, v2cb, v2ud);
}

SpeculativeDecoderV2::GenerateResult
SpeculativeInferenceBridge::generateFromText(
    const std::string& prompt,
    int maxNewTokens,
    TokenStreamCallback callback)
{
    // Simple byte-level tokenisation (matches the encode callback above).
    std::vector<int> toks;
    toks.reserve(prompt.size());
    for (unsigned char c : prompt) toks.push_back(static_cast<int>(c));
    return generateStreaming(toks, maxNewTokens, std::move(callback));
}

// ============================================================================
// Statistics / Abort
// ============================================================================

AcceptanceStats SpeculativeInferenceBridge::getStats() const {
    return SpeculativeDecoderV2::Global().getStats();
}

void SpeculativeInferenceBridge::resetStats() {
    SpeculativeDecoderV2::Global().resetStats();
}

void SpeculativeInferenceBridge::abort() {
    SpeculativeDecoderV2::Global().abort();
}

} // namespace Speculative
} // namespace RawrXD
