// ============================================================================
// File: src/agent/agent_self_reflection.hpp
// Purpose: Agent Self-Reflection System for Meta-Cognition and Error Recovery
// Converted from Qt to pure C++17
// ============================================================================
#pragma once

#include "../common/json_types.hpp"
#include "../common/callback_system.hpp"
#include "../common/time_utils.hpp"
#include "../common/string_utils.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

struct ErrorAnalysis {
    std::string errorId;
    std::string actionType;
    std::string errorMessage;
    std::string rootCause;
    std::string impact;
    std::vector<std::string> recommendations;
    TimePoint analyzedAt;
    bool requiresHumanInput;
};

struct AlternativeApproach {
    std::string approachId;
    std::string description;
    double confidenceLevel;
    double expectedSuccess;
    std::vector<std::string> requiredSteps;
    std::string estimatedTime;
};

struct ConfidenceAdjustment {
    std::string component;
    double adjustment;
    std::string reason;
    TimePoint adjustedAt;
};

class AgentSelfReflection {
public:
    AgentSelfReflection();
    ~AgentSelfReflection();

    ErrorAnalysis analyzeError(const std::string& actionType,
                              const std::string& errorMessage,
                              const std::string& context);
    std::vector<AlternativeApproach> generateAlternatives(const std::string& actionType,
                                                         const ErrorAnalysis& errorAnalysis,
                                                         const std::string& context);
    ConfidenceAdjustment adjustConfidence(bool success, const std::string& actionType, int executionTime);
    bool shouldEscalateToHuman(const ErrorAnalysis& errorAnalysis,
                              const std::vector<AlternativeApproach>& alternatives);
    void learnFromExecution(const std::string& actionType, bool success,
                           int executionTime, const std::string& errorMessage = "");
    double getActionConfidence(const std::string& actionType) const;
    double getOverallConfidence() const;
    void resetConfidence();

    // Callbacks (replacing Qt signals)
    CallbackList<const ErrorAnalysis&> onErrorAnalyzed;
    CallbackList<const std::vector<AlternativeApproach>&> onAlternativesGenerated;
    CallbackList<const ConfidenceAdjustment&> onConfidenceAdjusted;
    CallbackList<const ErrorAnalysis&> onEscalationRecommended;

private:
    void initializeConfidenceLevels();
    std::string identifyRootCause(const std::string& errorMessage) const;
    std::vector<std::string> generateRecommendations(const std::string& rootCause) const;
    std::string calculateImpact(const std::string& errorMessage) const;
    bool requiresHumanInput(const std::string& rootCause, const std::string& impact) const;
    std::string generateUniqueId();
    void updateConfidence(const std::string& component, double adjustment);

    std::map<std::string, double> m_confidenceLevels;
    std::map<std::string, int> m_executionCounts;
    std::map<std::string, int> m_successCounts;
    int m_idCounter;
};
