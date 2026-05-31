// ============================================================================
// speculative_execution_engine.h — Dual-Model Speculative Decoding
// ============================================================================
// Purpose: Produce multiple tokens in parallel via draft model, verify with
// main model, accept/reject with minimal KV recomputation.
// 
// Architecture:
//   Draft Model (fast, small)  →  Generate K tokens speculatively
//                ↓
//   Main Model (accurate)      →  Verify each token (accept/reject)
//                ↓
//   KV Rollback               →  Discard rejected tokens, keep accepted
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <atomic>

namespace RawrXD {
namespace Speculative {
class SpeculativeInferenceBridge;
}
}  // namespace RawrXD

namespace rawrxd {

// ============================================================================
// Speculative Token Bundle
// ============================================================================

struct SpeculativeToken {
    uint32_t tokenId;
    float draftLogProb;      // Log probability from draft model
    float mainLogProb;       // Log probability from main model
    bool accepted;           // Accepted by verifier
    uint32_t draftStep;      // Which step in speculative sequence
};

// ============================================================================
// Draft Model Selector
// ============================================================================

class DraftModelSelector {
public:
    enum class DraftSize {
        TINY,        // 0.3–0.5B (3-5× speedup)
        SMALL,       // 1–3B (2–3× speedup)
        MEDIUM       // 7B (1.5–2× speedup)
    };
    
    DraftModelSelector();
    
    /// Select appropriate draft model based on main model size + latency budget
    DraftSize SelectDraft(
        uint32_t mainModelParamCount,
        uint32_t latencyBudgetMs
    ) const;
    
    /// Get draft model path for given size
    std::string GetDraftModelPath(DraftSize size) const;
    
    /// Estimate speedup multiplier
    float EstimateSpeedup(DraftSize draft, uint32_t mainParams) const;
};

// ============================================================================
// Speculative Token Generator
// ============================================================================

class SpeculativeTokenGenerator {
public:
    SpeculativeTokenGenerator();
    
    /// Generate K speculative tokens from draft model
    /// Returns: vector of draft token IDs + log probs
    std::vector<SpeculativeToken> GenerateSpeculativeTokens(
        const void* draftModel,              // Draft inference engine
        const std::vector<uint32_t>& context,  // Context window
        uint32_t countToGenerate,            // How many tokens to speculate
        float temperature = 0.7f
    );
};

// ============================================================================
// Acceptance/Rejection Verifier
// ============================================================================

class SpeculativeVerifier {
public:
    SpeculativeVerifier();
    
    /// Verify speculative tokens with main model
    /// Returns: count of tokens accepted before first rejection
    uint32_t VerifyAndAccept(
        const void* mainModel,                      // Main inference engine
        const std::vector<SpeculativeToken>& draft, // Draft tokens to verify
        const std::vector<uint32_t>& initialContext,
        std::vector<uint32_t>& outAcceptedTokens    // Output: accepted token IDs
    );
    
    /// Compute acceptance threshold (controls quality vs speed tradeoff)
    float ComputeAcceptanceThreshold(
        float draftLogProb,
        float mainLogProb
    ) const;
};

// ============================================================================
// KV Rollback State
// ============================================================================

struct KVRollbackPoint {
    uint32_t sequenceLength;              // Sequence length at checkpoint
    uint32_t layerCount;
    std::vector<std::vector<uint8_t>> kvSnapshots;  // Per-layer KV backup
};

// ============================================================================
// KV Rollback Manager
// ============================================================================

class KVRollbackManager {
public:
    KVRollbackManager();
    
    /// Checkpoint KV cache before speculative generation
    KVRollbackPoint CheckpointKV(
        const void* kvCache,
        uint32_t layerCount,
        uint32_t seqLen
    );
    
    /// Rollback KV cache to checkpoint (discard rejected tokens)
    void RollbackToCheckpoint(
        void* kvCache,
        const KVRollbackPoint& checkpoint
    );
    
    /// Advance KV cache by N tokens (accept path)
    void AdvanceKVByTokens(
        void* kvCache,
        uint32_t acceptedTokenCount
    );
};

// ============================================================================
// Speculative Execution Engine (Main Orchestrator)
// ============================================================================

class SpeculativeExecutionEngine {
public:
    struct DiagnosticFrame {
        float acceptance_rate = 0.0f;
        int tokens_produced = 0;
        double draft_latency_ms = 0.0;
        double verify_latency_ms = 0.0;
        double total_ms = 0.0;
        int expert_id = -1;
    };

    struct Config {
        uint32_t maxSpeculativeTokens = 4;  // K in the paper
        float acceptanceThreshold = 0.5f;
        bool enableKVReuse = true;
        bool enableRollback = true;
    };
    
    explicit SpeculativeExecutionEngine(const Config& cfg = Config());
    ~SpeculativeExecutionEngine();
    
    /// Initialize with draft + main models (placeholder / simulation path).
    bool Initialize(
        void* mainModel,
        void* draftModel,
        uint32_t mainModelParams,
        uint32_t draftModelParams
    );

    /// Production path: load draft + target GGUF via SpeculativeInferenceBridge
    /// (AutonomousInferenceEngine + SpeculativeDecoderV2). Mutually exclusive
    /// with Initialize(void*, void*, ...); calling either resets the other mode.
    bool ConfigureGgufModels(
        const std::string& draftGgufPath,
        const std::string& targetGgufPath
    );
    
    /// Generate N tokens using speculative execution
    /// Returns: (acceptedTokens, totalTime_ms, speedupFactor)
    struct ExecutionResult {
        bool success{true};
        const char* detail{nullptr};
        std::vector<uint32_t> tokens;
        uint32_t timeMs;
        float speedupFactor;
        uint32_t totalSpeculated;
        uint32_t totalAccepted;
        float acceptanceRate;  // accepted / speculated
    };
    
    ExecutionResult GenerateWithSpeculation(
        const std::vector<uint32_t>& context,
        uint32_t countToGenerate,
        float temperature = 0.7f
    );
    
    /// Get current statistics
    struct Stats {
        uint64_t totalSpeculated;
        uint64_t totalAccepted;
        uint64_t totalRejected;
        float avgAcceptanceRate;
        float avgSpeedupMultiplier;
        uint64_t kvRollbackCount;  // Number of rollbacks performed
    };
    
    Stats GetStats() const;
    DiagnosticFrame GetLastDiagnosticFrame() const;
    void ResetStats();
    
private:
    Config m_config;
    std::unique_ptr<DraftModelSelector> m_draftSelector;
    std::unique_ptr<SpeculativeTokenGenerator> m_generator;
    std::unique_ptr<SpeculativeVerifier> m_verifier;
    std::unique_ptr<KVRollbackManager> m_kvManager;
    
    void* m_mainModel;
    void* m_draftModel;
    uint32_t m_mainParams;
    uint32_t m_draftParams;
    
    std::atomic<uint64_t> m_totalSpeculated;
    std::atomic<uint64_t> m_totalAccepted;
    std::atomic<uint64_t> m_kvRollbacks;

    std::atomic<float> m_lastAcceptanceRate;
    std::atomic<int> m_lastTokensProduced;
    std::atomic<double> m_lastDraftLatencyMs;
    std::atomic<double> m_lastVerifyLatencyMs;
    std::atomic<double> m_lastTotalMs;
    std::atomic<int> m_lastExpertId;

    std::unique_ptr<RawrXD::Speculative::SpeculativeInferenceBridge> m_inferenceBridge;
};

}  // namespace rawrxd
