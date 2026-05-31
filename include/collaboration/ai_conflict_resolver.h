#pragma once
/**
 * @file ai_conflict_resolver.h
 * @brief AI-powered conflict resolution for collaborative editing
 * Integrates SovereignInferenceClient with LiveShare for neural merge suggestions
 */

#include "collaboration/live_share.h"
#include "agentic/SovereignInferenceClient.h"
#include <memory>
#include <string>
#include <vector>

namespace RawrXD::Collaboration {

// ---------------------------------------------------------------------------
// Merge suggestion result
// ---------------------------------------------------------------------------
struct MergeSuggestion {
    std::string mergedText;
    double confidence = 0.0;
    std::string reasoning;
    std::vector<std::string> resolvedOperationIds;
    bool fromAI = false;
    double latencyMs = 0.0;
};

// ---------------------------------------------------------------------------
// AI Conflict Resolver — bridges LiveShare + SovereignInferenceClient
// ---------------------------------------------------------------------------
class AIConflictResolver {
public:
    AIConflictResolver();
    ~AIConflictResolver();

    // Initialize with model path (loads SovereignInferenceClient)
    bool initialize(const std::string& modelPath = "");
    void shutdown();
    bool isInitialized() const;

    // Main entry: resolve conflicting operations into a merge suggestion
    MergeSuggestion resolveConflicts(
        const std::vector<TextOperation>& conflicts,
        const std::string& documentContext = "");

    // Batch resolve multiple conflict sets
    std::vector<MergeSuggestion> resolveConflictsBatch(
        const std::vector<std::vector<TextOperation>>& conflictSets);

    // Statistics
    struct Stats {
        uint64_t totalResolutions = 0;
        uint64_t aiResolutions = 0;
        uint64_t heuristicResolutions = 0;
        uint64_t failedResolutions = 0;
        double avgLatencyMs = 0.0;
        double avgConfidence = 0.0;
    };
    Stats getStats() const;
    void resetStats();

    // Configuration
    void setTemperature(float temp);
    void setMaxTokens(uint32_t maxTokens);
    void setEnableAI(bool enable);

private:
    std::unique_ptr<Agent::SovereignInferenceClient> m_inferenceClient;
    bool m_initialized = false;
    bool m_enableAI = true;
    float m_temperature = 0.1f;
    uint32_t m_maxTokens = 256;

    mutable std::mutex m_statsMutex;
    Stats m_stats;

    // Internal methods
    std::string buildPrompt(
        const std::vector<TextOperation>& conflicts,
        const std::string& documentContext);

    MergeSuggestion heuristicResolve(
        const std::vector<TextOperation>& conflicts);

    MergeSuggestion aiResolve(
        const std::vector<TextOperation>& conflicts,
        const std::string& documentContext);

    void updateStats(const MergeSuggestion& result, double latencyMs);
};

} // namespace RawrXD::Collaboration
