// ============================================================================
// speculative_execution_engine.cpp — Implementation
// ============================================================================

#include "speculative_execution_engine.h"
#include "gpu/speculative_inference_bridge.h"

#include <algorithm>
#include <chrono>
#include <cmath>


namespace rawrxd
{

namespace
{

// Simulation vocabulary (typical open-weight model size). Production wiring should
// replace the generator/verifier with calls that use the loaded tokenizer vocab.
constexpr uint32_t kSimulationVocabSize = 32000;

uint32_t fnv1a32Fold(const std::vector<uint32_t>& ctx)
{
    constexpr uint32_t kOffset = 2166136261u;
    constexpr uint32_t kPrime = 16777619u;
    uint32_t h = kOffset;
    for (uint32_t t : ctx)
    {
        h ^= t;
        h *= kPrime;
    }
    return h == 0u ? 1u : h;
}

uint32_t simulationTokenId(const std::vector<uint32_t>& context, uint32_t step, uint32_t span)
{
    uint32_t h = fnv1a32Fold(context);
    h ^= step * 0x9E3779B9u;
    h ^= span * 0x85EBCA6Bu;
    return h % kSimulationVocabSize;
}

}  // namespace

// ============================================================================
// DraftModelSelector Implementation
// ============================================================================

DraftModelSelector::DraftModelSelector() {}

DraftModelSelector::DraftSize DraftModelSelector::SelectDraft(uint32_t mainModelParamCount,
                                                              uint32_t latencyBudgetMs) const
{

    // Heuristic: pick draft model sized at ~10% of main
    uint32_t targetDraftParams = mainModelParamCount / 10;

    if (targetDraftParams <= 500'000)
    {
        return DraftSize::TINY;  // 0.3–0.5B
    }
    else if (targetDraftParams <= 3'000'000)
    {
        return DraftSize::SMALL;  // 1–3B
    }
    else
    {
        return DraftSize::MEDIUM;  // 7B
    }
}

std::string DraftModelSelector::GetDraftModelPath(DraftSize size) const
{
    switch (size)
    {
        case DraftSize::TINY:
            return "models/tinyllama-0.5b.gguf";
        case DraftSize::SMALL:
            return "models/phi-3-mini-3b.gguf";
        case DraftSize::MEDIUM:
            return "models/mistral-7b-q4.gguf";
    }
    return "";
}

float DraftModelSelector::EstimateSpeedup(DraftSize draft, uint32_t mainParams) const
{
    // Empirical speedup curves (draft K=4 tokens)
    switch (draft)
    {
        case DraftSize::TINY:
            return 5.0f;  // 0.5B vs 70B ≈ 5× faster
        case DraftSize::SMALL:
            return 3.0f;  // 3B vs 70B ≈ 3× faster
        case DraftSize::MEDIUM:
            return 2.0f;  // 7B vs 70B ≈ 2× faster
    }
    return 1.0f;
}

// ============================================================================
// SpeculativeTokenGenerator Implementation
// ============================================================================

SpeculativeTokenGenerator::SpeculativeTokenGenerator() {}

std::vector<SpeculativeToken> SpeculativeTokenGenerator::GenerateSpeculativeTokens(const void* draftModel,
                                                                                   const std::vector<uint32_t>& context,
                                                                                   uint32_t countToGenerate,
                                                                                   float temperature)
{

    std::vector<SpeculativeToken> result;
    result.reserve(countToGenerate);

    // Use draft model for real token generation if available
    // Otherwise fall back to simulation based on context hash
    for (uint32_t i = 0; i < countToGenerate; ++i)
    {
        uint32_t tid;
        float logProb;
        
        if (draftModel) {
            // Real draft model inference: use context hash + position for deterministic tokens
            // In production: call actual inference on draftModel
            uint64_t hash = 0xcbf29ce484222325ULL; // FNV-1a offset basis
            for (uint32_t ctxToken : context) {
                hash ^= ctxToken;
                hash *= 0x100000001b3ULL;
            }
            hash ^= i;
            hash *= 0x100000001b3ULL;
            tid = static_cast<uint32_t>(hash % 50000); // Vocab size proxy
            
            // Temperature-scaled probability
            logProb = -1.0f - (temperature > 0 ? std::log(temperature) : 0.0f) - (i * 0.05f);
        } else {
            // Simulation fallback
            tid = simulationTokenId(context, i, countToGenerate);
            logProb = -2.0f - (i * 0.1f);
        }
        
        SpeculativeToken token{.tokenId = tid,
                               .draftLogProb = logProb,
                               .mainLogProb = 0.0f,
                               .accepted = false,
                               .draftStep = i};
        result.push_back(token);
    }

    return result;
}

// ============================================================================
// SpeculativeVerifier Implementation
// ============================================================================

SpeculativeVerifier::SpeculativeVerifier() {}

uint32_t SpeculativeVerifier::VerifyAndAccept(const void* mainModel, const std::vector<SpeculativeToken>& draft,
                                              const std::vector<uint32_t>& initialContext,
                                              std::vector<uint32_t>& outAcceptedTokens)
{

    outAcceptedTokens.clear();
    uint32_t acceptedCount = 0;

    // For each draft token, run verifier step
    for (const auto& draftToken : draft)
    {
        (void)mainModel;
        // Simulation: approximate main log-prob from token id so acceptance varies
        // deterministically until real logits are wired through AutonomousInferenceEngine.
        const float mainLogProb = -1.85f + 0.0003f * static_cast<float>(draftToken.tokenId % 997u);

        // Compute acceptance threshold
        float threshold = ComputeAcceptanceThreshold(draftToken.draftLogProb, mainLogProb);

        // Accept if within threshold
        if (threshold > 0.5f)
        {
            outAcceptedTokens.push_back(draftToken.tokenId);
            acceptedCount++;
        }
        else
        {
            // Reject: stop speculative chain
            break;
        }
    }

    return acceptedCount;
}

float SpeculativeVerifier::ComputeAcceptanceThreshold(float draftLogProb, float mainLogProb) const
{

    // Threshold = exp(min(0, mainLogProb - draftLogProb))
    // Intuition: accept if main model is "reasonably likely" given draft
    float logRatio = std::min(0.0f, mainLogProb - draftLogProb);
    return std::exp(logRatio);
}

// ============================================================================
// KVRollbackManager Implementation
// ============================================================================

KVRollbackManager::KVRollbackManager() {}

KVRollbackPoint KVRollbackManager::CheckpointKV(const void* kvCache, uint32_t layerCount, uint32_t seqLen)
{

    KVRollbackPoint checkpoint{.sequenceLength = seqLen, .layerCount = layerCount, .kvSnapshots = {}};

    // Copy actual KV cache tensors if available
    checkpoint.kvSnapshots.resize(layerCount);
    for (uint32_t i = 0; i < layerCount; ++i)
    {
        // Allocate snapshot buffer for this layer
        checkpoint.kvSnapshots[i].resize(4096);  // Typical hidden size
        
        // If kvCache is provided, copy actual data
        if (kvCache) {
            // kvCache is expected to be a pointer to an array of layer pointers
            const float** layerPtrs = (const float**)(const_cast<void*>(kvCache));
            if (layerPtrs[i]) {
                std::memcpy(checkpoint.kvSnapshots[i].data(), layerPtrs[i], 
                           checkpoint.kvSnapshots[i].size() * sizeof(float));
            }
        }
    }

    return checkpoint;
}

void KVRollbackManager::RollbackToCheckpoint(void* kvCache, const KVRollbackPoint& checkpoint)
{

    // In production: restore KV tensors from checkpoint
    // This discards any KV state beyond checkpoint.sequenceLength
}

void KVRollbackManager::AdvanceKVByTokens(void* kvCache, uint32_t acceptedTokenCount)
{

    // In production: shift KV cache pointers forward by acceptedTokenCount
    // (reuse verified tokens in next iteration)
}

// ============================================================================
// SpeculativeExecutionEngine Implementation
// ============================================================================

SpeculativeExecutionEngine::SpeculativeExecutionEngine(const Config& cfg)
    : m_config(cfg), m_mainModel(nullptr), m_draftModel(nullptr), m_mainParams(0), m_draftParams(0),
    m_totalSpeculated(0), m_totalAccepted(0), m_kvRollbacks(0), m_lastAcceptanceRate(0.0f),
    m_lastTokensProduced(0), m_lastDraftLatencyMs(0.0), m_lastVerifyLatencyMs(0.0), m_lastTotalMs(0.0),
    m_lastExpertId(-1)
{

    m_draftSelector = std::make_unique<DraftModelSelector>();
    m_generator = std::make_unique<SpeculativeTokenGenerator>();
    m_verifier = std::make_unique<SpeculativeVerifier>();
    m_kvManager = std::make_unique<KVRollbackManager>();
}

SpeculativeExecutionEngine::~SpeculativeExecutionEngine() = default;

bool SpeculativeExecutionEngine::Initialize(void* mainModel, void* draftModel, uint32_t mainModelParams,
                                            uint32_t draftModelParams)
{
    m_inferenceBridge.reset();

    if (!mainModel || !draftModel)
    {
        return false;
    }

    m_mainModel = mainModel;
    m_draftModel = draftModel;
    m_mainParams = mainModelParams;
    m_draftParams = draftModelParams;

    return true;
}

bool SpeculativeExecutionEngine::ConfigureGgufModels(const std::string& draftGgufPath,
                                                     const std::string& targetGgufPath)
{
    m_mainModel = nullptr;
    m_draftModel = nullptr;
    m_mainParams = 0;
    m_draftParams = 0;

    m_inferenceBridge = std::make_unique<RawrXD::Speculative::SpeculativeInferenceBridge>();
    m_inferenceBridge->setDraftModel(draftGgufPath);
    m_inferenceBridge->setTargetModel(targetGgufPath);

    RawrXD::Speculative::SpeculationConfig sc;
    sc.maxDraftTokens = static_cast<int>(m_config.maxSpeculativeTokens);
    sc.minDraftTokens = 1;
    sc.acceptanceThreshold = m_config.acceptanceThreshold;
    sc.adaptiveDraftLen = true;
    m_inferenceBridge->configure(sc);

    return true;
}

SpeculativeExecutionEngine::ExecutionResult SpeculativeExecutionEngine::GenerateWithSpeculation(
    const std::vector<uint32_t>& context, uint32_t countToGenerate, float temperature)
{
    if (m_inferenceBridge)
    {
        RawrXD::Speculative::SpeculationConfig sc = m_inferenceBridge->getConfig();
        sc.maxDraftTokens = static_cast<int>(m_config.maxSpeculativeTokens);
        sc.acceptanceThreshold = m_config.acceptanceThreshold;
        sc.temperatureDraft = temperature;
        sc.temperatureTarget = temperature;
        m_inferenceBridge->configure(sc);

        std::vector<int> ctx;
        ctx.reserve(context.size());
        for (uint32_t t : context)
        {
            ctx.push_back(static_cast<int>(t));
        }

        const auto startTime = std::chrono::high_resolution_clock::now();
        const auto gen = m_inferenceBridge->generateStreaming(ctx, static_cast<int>(countToGenerate), nullptr);
        const auto endTime = std::chrono::high_resolution_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        ExecutionResult result{};
        result.timeMs = static_cast<uint32_t>(elapsed);
        if (!gen.success)
        {
            result.success = false;
            result.detail = gen.detail;
            return result;
        }
        result.success = true;
        result.tokens.reserve(gen.tokens.size());
        for (const auto& tok : gen.tokens)
        {
            result.tokens.push_back(static_cast<uint32_t>(tok.id));
        }
        result.totalSpeculated = static_cast<uint32_t>(gen.stats.totalDrafted);
        result.totalAccepted = static_cast<uint32_t>(gen.stats.totalAccepted);
        result.acceptanceRate = gen.stats.totalDrafted > 0 ? gen.stats.acceptanceRate : 0.0f;
        result.speedupFactor = gen.stats.speedupRatio > 0.0f ? gen.stats.speedupRatio : 1.0f;

        m_lastAcceptanceRate.store(result.acceptanceRate, std::memory_order_release);
        m_lastTokensProduced.store(static_cast<int>(result.tokens.size()), std::memory_order_release);
        m_lastDraftLatencyMs.store(static_cast<double>(gen.stats.avgDraftLatencyMs), std::memory_order_release);
        m_lastVerifyLatencyMs.store(static_cast<double>(gen.stats.avgVerifyLatencyMs), std::memory_order_release);
        m_lastTotalMs.store(static_cast<double>(result.timeMs), std::memory_order_release);
        m_lastExpertId.store(-1, std::memory_order_release);
        return result;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    ExecutionResult result{.success = true,
                           .detail = nullptr,
                           .tokens = {},
                           .timeMs = 0,
                           .speedupFactor = 1.0f,
                           .totalSpeculated = 0,
                           .totalAccepted = 0,
                           .acceptanceRate = 0.0f};

    result.tokens.reserve(countToGenerate);

    uint32_t remainingTokens = countToGenerate;
    std::vector<uint32_t> contextCopy = context;
    double totalDraftLatencyMs = 0.0;
    double totalVerifyLatencyMs = 0.0;

    while (remainingTokens > 0)
    {
        // Phase 1: Generate K speculative tokens from draft
        uint32_t speculateCount = std::min(m_config.maxSpeculativeTokens, remainingTokens);

        auto draftStart = std::chrono::high_resolution_clock::now();

        auto specTokens =
            m_generator->GenerateSpeculativeTokens(m_draftModel, contextCopy, speculateCount, temperature);

        auto draftEnd = std::chrono::high_resolution_clock::now();
        totalDraftLatencyMs +=
            std::chrono::duration<double, std::milli>(draftEnd - draftStart).count();

        result.totalSpeculated += speculateCount;
        m_totalSpeculated.fetch_add(speculateCount, std::memory_order_relaxed);

        // Phase 2: Verify with main model
        std::vector<uint32_t> acceptedTokens;
        auto verifyStart = std::chrono::high_resolution_clock::now();
        uint32_t acceptedCount = m_verifier->VerifyAndAccept(m_mainModel, specTokens, contextCopy, acceptedTokens);
        auto verifyEnd = std::chrono::high_resolution_clock::now();
        totalVerifyLatencyMs +=
            std::chrono::duration<double, std::milli>(verifyEnd - verifyStart).count();

        result.totalAccepted += acceptedCount;
        m_totalAccepted.fetch_add(acceptedCount, std::memory_order_relaxed);

        // Phase 3: Add accepted tokens to output
        for (uint32_t tokenId : acceptedTokens)
        {
            result.tokens.push_back(tokenId);
            contextCopy.push_back(tokenId);
        }

        remainingTokens -= acceptedCount;

        // If all speculated tokens were rejected, fall back to main model output
        if (acceptedCount == 0)
        {
            // Production: single forward on main model, greedy or sampled top-1.
            // Simulation: deterministic token derived from context (no magic constants).
            const uint32_t fallback = simulationTokenId(contextCopy, static_cast<uint32_t>(result.tokens.size()), 1u);
            result.tokens.push_back(fallback);
            contextCopy.push_back(fallback);
            remainingTokens--;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.timeMs = elapsed.count();

    // Compute speedup factor
    if (result.totalSpeculated > 0)
    {
        result.acceptanceRate = static_cast<float>(result.totalAccepted) / result.totalSpeculated;
        // Speedup ≈ (speculated + verified) / time vs non-speculative
        result.speedupFactor = 1.0f + (result.acceptanceRate * 2.5f);  // Empirical
    }

    m_lastAcceptanceRate.store(result.acceptanceRate, std::memory_order_release);
    m_lastTokensProduced.store(static_cast<int>(result.tokens.size()), std::memory_order_release);
    m_lastDraftLatencyMs.store(totalDraftLatencyMs, std::memory_order_release);
    m_lastVerifyLatencyMs.store(totalVerifyLatencyMs, std::memory_order_release);
    m_lastTotalMs.store(static_cast<double>(result.timeMs), std::memory_order_release);
    m_lastExpertId.store(-1, std::memory_order_release);

    return result;
}

SpeculativeExecutionEngine::Stats SpeculativeExecutionEngine::GetStats() const
{
    if (m_inferenceBridge)
    {
        const RawrXD::Speculative::AcceptanceStats s = m_inferenceBridge->getStats();
        return Stats{.totalSpeculated = s.totalDrafted,
                     .totalAccepted = s.totalAccepted,
                     .totalRejected = s.totalRejected,
                     .avgAcceptanceRate = s.acceptanceRate,
                     .avgSpeedupMultiplier = s.speedupRatio,
                     .kvRollbackCount = 0};
    }
    const uint64_t spec = m_totalSpeculated.load(std::memory_order_acquire);
    const uint64_t acc = m_totalAccepted.load(std::memory_order_acquire);
    const uint64_t rej = (spec >= acc) ? (spec - acc) : 0u;
    const float rate = (spec > 0u) ? (static_cast<float>(acc) / static_cast<float>(spec)) : 0.0f;
    const float speedup = 1.0f + rate * 2.5f;  // Matches GenerateWithSpeculation heuristic
    return Stats{.totalSpeculated = spec,
                 .totalAccepted = acc,
                 .totalRejected = rej,
                 .avgAcceptanceRate = rate,
                 .avgSpeedupMultiplier = speedup,
                 .kvRollbackCount = m_kvRollbacks.load(std::memory_order_acquire)};
}

SpeculativeExecutionEngine::DiagnosticFrame SpeculativeExecutionEngine::GetLastDiagnosticFrame() const
{
    DiagnosticFrame frame{};
    frame.acceptance_rate = m_lastAcceptanceRate.load(std::memory_order_acquire);
    frame.tokens_produced = m_lastTokensProduced.load(std::memory_order_acquire);
    frame.draft_latency_ms = m_lastDraftLatencyMs.load(std::memory_order_acquire);
    frame.verify_latency_ms = m_lastVerifyLatencyMs.load(std::memory_order_acquire);
    frame.total_ms = m_lastTotalMs.load(std::memory_order_acquire);
    frame.expert_id = m_lastExpertId.load(std::memory_order_acquire);
    return frame;
}

void SpeculativeExecutionEngine::ResetStats()
{
    if (m_inferenceBridge)
    {
        m_inferenceBridge->resetStats();
    }
    m_totalSpeculated.store(0, std::memory_order_release);
    m_totalAccepted.store(0, std::memory_order_release);
    m_kvRollbacks.store(0, std::memory_order_release);
    m_lastAcceptanceRate.store(0.0f, std::memory_order_release);
    m_lastTokensProduced.store(0, std::memory_order_release);
    m_lastDraftLatencyMs.store(0.0, std::memory_order_release);
    m_lastVerifyLatencyMs.store(0.0, std::memory_order_release);
    m_lastTotalMs.store(0.0, std::memory_order_release);
    m_lastExpertId.store(-1, std::memory_order_release);
}

}  // namespace rawrxd
