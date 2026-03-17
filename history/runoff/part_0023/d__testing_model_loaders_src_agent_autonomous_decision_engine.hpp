// ============================================================================
// File: src/agent/autonomous_decision_engine.hpp
// Purpose: Autonomous Decision Engine for Graduated Autonomy
// Converted from Qt to pure C++17
// ============================================================================
#pragma once

#include "../common/json_types.hpp"
#include "../common/callback_system.hpp"
#include "../common/time_utils.hpp"
#include "../common/string_utils.hpp"
#include <string>
#include <vector>
#include <memory>

struct RiskFactor {
    std::string riskId;
    std::string category;
    std::string description;
    double probability;
    double impact;
    std::vector<std::string> mitigationStrategies;
    bool isMitigated;
};

struct DecisionContext {
    std::string wish;
    std::vector<std::string> availableActions;
    std::vector<std::string> constraints;
    std::string projectContext;
    std::string urgency;
    std::string complexity;
};

struct DecisionOutcome {
    std::string decisionId;
    std::string chosenAction;
    double confidenceLevel;
    std::string expectedOutcome;
    std::vector<RiskFactor> risks;
    TimePoint decidedAt;
    bool requiresApproval;
};

struct AutonomyLevel {
    std::string levelName;
    std::string description;
    std::vector<std::string> allowedActions;
    double maxRiskThreshold;
    bool requiresHumanOverride;
};

class AutonomousDecisionEngine {
public:
    AutonomousDecisionEngine();
    ~AutonomousDecisionEngine();

    DecisionOutcome makeDecision(const DecisionContext& context);
    std::vector<RiskFactor> assessRisks(const std::string& action, const DecisionContext& context);
    double calculateConfidence(const std::string& action, const DecisionContext& context, const std::vector<RiskFactor>& risks);
    bool requiresHumanApproval(const DecisionOutcome& decisionOutcome);
    void setAutonomyLevel(const std::string& level);
    AutonomyLevel getAutonomyLevel() const;
    std::vector<AutonomyLevel> getAutonomyLevels() const;
    JsonObject evaluateCostBenefit(const std::string& action, const DecisionContext& context);
    void logDecision(const DecisionOutcome& decision);
    std::vector<DecisionOutcome> getDecisionHistory(int limit = 50) const;

    CallbackList<const DecisionOutcome&> onDecisionMade;
    CallbackList<const DecisionOutcome&> onApprovalRequired;
    CallbackList<const RiskFactor&> onRiskThresholdExceeded;
    CallbackList<const AutonomyLevel&> onAutonomyLevelChanged;

private:
    void initializeAutonomyLevels();
    void initializeRiskFactors();
    std::vector<RiskFactor> getRiskFactorsByCategory(const std::string& category) const;
    double calculateRiskScore(const std::vector<RiskFactor>& risks) const;
    std::string generateUniqueId();
    bool isActionAllowed(const std::string& action) const;
    std::vector<RiskFactor> mitigateRisks(const std::vector<RiskFactor>& risks);

    std::vector<AutonomyLevel> m_autonomyLevels;
    AutonomyLevel m_currentLevel;
    std::vector<RiskFactor> m_riskFactors;
    std::vector<DecisionOutcome> m_decisionHistory;
    int m_idCounter;
};
