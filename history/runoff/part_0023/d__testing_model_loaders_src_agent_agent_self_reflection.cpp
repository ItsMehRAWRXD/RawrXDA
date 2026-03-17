// ============================================================================
// File: src/agent/agent_self_reflection.cpp
// Purpose: Agent Self-Reflection System implementation
// Converted from Qt to pure C++17
// ============================================================================
#include "agent_self_reflection.hpp"
#include "../common/logger.hpp"
#include <algorithm>
#include <cmath>

AgentSelfReflection::AgentSelfReflection()
    : m_idCounter(0)
{
    initializeConfidenceLevels();
}

AgentSelfReflection::~AgentSelfReflection() {}

ErrorAnalysis AgentSelfReflection::analyzeError(const std::string& actionType,
                                               const std::string& errorMessage,
                                               const std::string& context)
{
    (void)context;
    ErrorAnalysis analysis;
    analysis.errorId = generateUniqueId();
    analysis.actionType = actionType;
    analysis.errorMessage = errorMessage;
    analysis.rootCause = identifyRootCause(errorMessage);
    analysis.impact = calculateImpact(errorMessage);
    analysis.recommendations = generateRecommendations(analysis.rootCause);
    analysis.analyzedAt = TimeUtils::now();
    analysis.requiresHumanInput = requiresHumanInput(analysis.rootCause, analysis.impact);
    onErrorAnalyzed.emit(analysis);
    return analysis;
}

std::vector<AlternativeApproach> AgentSelfReflection::generateAlternatives(const std::string& actionType,
                                                                          const ErrorAnalysis& errorAnalysis,
                                                                          const std::string& context)
{
    (void)context;
    std::vector<AlternativeApproach> alternatives;

    if (actionType == "FileEdit") {
        if (StringUtils::containsCI(errorAnalysis.rootCause, "permission")) {
            AlternativeApproach a1;
            a1.approachId = generateUniqueId();
            a1.description = "Use elevated privileges to modify file";
            a1.confidenceLevel = 0.8;
            a1.expectedSuccess = 0.9;
            a1.requiredSteps = {"Request administrator privileges", "Retry file operation"};
            a1.estimatedTime = "5 minutes";
            alternatives.push_back(a1);

            AlternativeApproach a2;
            a2.approachId = generateUniqueId();
            a2.description = "Create a copy and replace original manually";
            a2.confidenceLevel = 0.7;
            a2.expectedSuccess = 0.8;
            a2.requiredSteps = {"Create backup", "Modify copy", "Replace original"};
            a2.estimatedTime = "10 minutes";
            alternatives.push_back(a2);
        } else if (StringUtils::containsCI(errorAnalysis.rootCause, "not found")) {
            AlternativeApproach a1;
            a1.approachId = generateUniqueId();
            a1.description = "Verify file path and create directory structure if needed";
            a1.confidenceLevel = 0.9;
            a1.expectedSuccess = 0.95;
            a1.requiredSteps = {"Check file path", "Create directories", "Create file"};
            a1.estimatedTime = "3 minutes";
            alternatives.push_back(a1);
        }
    } else if (actionType == "RunBuild") {
        if (StringUtils::containsCI(errorAnalysis.rootCause, "dependency")) {
            AlternativeApproach a1;
            a1.approachId = generateUniqueId();
            a1.description = "Install missing dependencies";
            a1.confidenceLevel = 0.85;
            a1.expectedSuccess = 0.9;
            a1.requiredSteps = {"Identify missing dependencies", "Install dependencies", "Retry build"};
            a1.estimatedTime = "15 minutes";
            alternatives.push_back(a1);

            AlternativeApproach a2;
            a2.approachId = generateUniqueId();
            a2.description = "Use alternative build configuration";
            a2.confidenceLevel = 0.7;
            a2.expectedSuccess = 0.75;
            a2.requiredSteps = {"Check available configurations", "Select alternative", "Run build"};
            a2.estimatedTime = "10 minutes";
            alternatives.push_back(a2);
        }
    }

    AlternativeApproach fallback;
    fallback.approachId = generateUniqueId();
    fallback.description = "Break task into smaller steps and execute incrementally";
    fallback.confidenceLevel = 0.6;
    fallback.expectedSuccess = 0.7;
    fallback.requiredSteps = {"Decompose task", "Execute steps one by one", "Validate each step"};
    fallback.estimatedTime = "Varies based on task complexity";
    alternatives.push_back(fallback);

    onAlternativesGenerated.emit(alternatives);
    return alternatives;
}

ConfidenceAdjustment AgentSelfReflection::adjustConfidence(bool success,
                                                          const std::string& actionType,
                                                          int executionTime)
{
    ConfidenceAdjustment adjustment;
    adjustment.component = actionType;
    adjustment.adjustedAt = TimeUtils::now();

    if (success) {
        adjustment.adjustment = 0.05;
        adjustment.reason = "Successful execution of " + actionType;
        if (executionTime < 30000) {
            adjustment.adjustment += 0.02;
            adjustment.reason += " with fast execution time";
        }
    } else {
        adjustment.adjustment = -0.1;
        adjustment.reason = "Failed execution of " + actionType;
        if (executionTime > 120000) {
            adjustment.adjustment -= 0.05;
            adjustment.reason += " with slow execution time";
        }
    }

    updateConfidence(actionType, adjustment.adjustment);
    onConfidenceAdjusted.emit(adjustment);
    return adjustment;
}

bool AgentSelfReflection::shouldEscalateToHuman(const ErrorAnalysis& errorAnalysis,
                                               const std::vector<AlternativeApproach>& alternatives)
{
    if (errorAnalysis.requiresHumanInput) {
        onEscalationRecommended.emit(errorAnalysis);
        return true;
    }

    bool hasHighConfidence = false;
    for (const auto& approach : alternatives) {
        if (approach.confidenceLevel > 0.8) {
            hasHighConfidence = true;
            break;
        }
    }

    if (!hasHighConfidence) {
        if (getOverallConfidence() < 0.3) {
            ErrorAnalysis escalation = errorAnalysis;
            escalation.errorMessage = "Low overall confidence and no viable alternatives";
            onEscalationRecommended.emit(escalation);
            return true;
        }
    }

    if (errorAnalysis.impact == "high" && getActionConfidence(errorAnalysis.actionType) < 0.4) {
        onEscalationRecommended.emit(errorAnalysis);
        return true;
    }

    return false;
}

void AgentSelfReflection::learnFromExecution(const std::string& actionType,
                                            bool success, int executionTime,
                                            const std::string& errorMessage)
{
    m_executionCounts[actionType]++;
    if (success) m_successCounts[actionType]++;
    adjustConfidence(success, actionType, executionTime);

    if (!success && !errorMessage.empty()) {
        ErrorAnalysis analysis = analyzeError(actionType, errorMessage, "");
        auto alternatives = generateAlternatives(actionType, analysis, "");
        shouldEscalateToHuman(analysis, alternatives);
    }
}

double AgentSelfReflection::getActionConfidence(const std::string& actionType) const
{
    auto it = m_confidenceLevels.find(actionType);
    if (it != m_confidenceLevels.end()) return it->second;
    return 0.5;
}

double AgentSelfReflection::getOverallConfidence() const
{
    if (m_confidenceLevels.empty()) return 0.5;
    double total = 0.0;
    for (const auto& [k, v] : m_confidenceLevels) total += v;
    return total / m_confidenceLevels.size();
}

void AgentSelfReflection::resetConfidence() { initializeConfidenceLevels(); }

void AgentSelfReflection::initializeConfidenceLevels()
{
    m_confidenceLevels["FileEdit"] = 0.7;
    m_confidenceLevels["SearchFiles"] = 0.8;
    m_confidenceLevels["RunBuild"] = 0.6;
    m_confidenceLevels["ExecuteTests"] = 0.7;
    m_confidenceLevels["CommitGit"] = 0.6;
    m_confidenceLevels["InvokeCommand"] = 0.5;
    m_confidenceLevels["RecursiveAgent"] = 0.4;
    m_confidenceLevels["QueryUser"] = 0.9;
    for (const auto& [k, v] : m_confidenceLevels) {
        m_executionCounts[k] = 0;
        m_successCounts[k] = 0;
    }
}

std::string AgentSelfReflection::identifyRootCause(const std::string& errorMessage) const
{
    if (StringUtils::containsCI(errorMessage, "permission")) return "Insufficient permissions to perform operation";
    if (StringUtils::containsCI(errorMessage, "not found") || StringUtils::containsCI(errorMessage, "no such file")) return "File or directory not found";
    if (StringUtils::containsCI(errorMessage, "syntax")) return "Syntax error in code or configuration";
    if (StringUtils::containsCI(errorMessage, "dependency") || StringUtils::containsCI(errorMessage, "missing")) return "Missing dependency or resource";
    if (StringUtils::containsCI(errorMessage, "timeout")) return "Operation timed out";
    if (StringUtils::containsCI(errorMessage, "memory")) return "Insufficient memory available";
    if (StringUtils::containsCI(errorMessage, "network")) return "Network connectivity issue";
    return "Unknown or unspecified error";
}

std::vector<std::string> AgentSelfReflection::generateRecommendations(const std::string& rootCause) const
{
    if (StringUtils::containsCI(rootCause, "permission"))
        return {"Check user permissions for the affected resource", "Run the agent with elevated privileges if necessary", "Verify file/directory ownership"};
    if (StringUtils::containsCI(rootCause, "not found"))
        return {"Verify the file path is correct", "Check if the file/directory exists", "Create missing directories if needed"};
    if (StringUtils::containsCI(rootCause, "syntax"))
        return {"Review the code or configuration for syntax errors", "Use a linter to identify issues", "Check documentation for correct syntax"};
    if (StringUtils::containsCI(rootCause, "dependency"))
        return {"Install missing dependencies", "Check project documentation for requirements", "Verify dependency versions are compatible"};
    if (StringUtils::containsCI(rootCause, "timeout"))
        return {"Increase timeout values for the operation", "Check system resources", "Optimize the operation to reduce execution time"};
    if (StringUtils::containsCI(rootCause, "memory"))
        return {"Close unnecessary applications to free memory", "Increase system memory if possible", "Optimize memory usage in the operation"};
    if (StringUtils::containsCI(rootCause, "network"))
        return {"Check network connectivity", "Verify firewall settings", "Retry the operation when network is stable"};
    return {"Review the error message for specific details", "Check system logs for additional information", "Consult documentation or seek expert assistance"};
}

std::string AgentSelfReflection::calculateImpact(const std::string& errorMessage) const
{
    if (StringUtils::containsCI(errorMessage, "critical") || StringUtils::containsCI(errorMessage, "fatal") || StringUtils::containsCI(errorMessage, "corrupt"))
        return "high";
    if (StringUtils::containsCI(errorMessage, "warning") || StringUtils::containsCI(errorMessage, "deprecated"))
        return "low";
    return "medium";
}

bool AgentSelfReflection::requiresHumanInput(const std::string& rootCause, const std::string& impact) const
{
    if (impact == "high") return true;
    if (StringUtils::containsCI(rootCause, "unknown") || StringUtils::containsCI(rootCause, "unspecified")) return true;
    return false;
}

std::string AgentSelfReflection::generateUniqueId() { return std::to_string(m_idCounter++); }

void AgentSelfReflection::updateConfidence(const std::string& component, double adjustment)
{
    double current = getActionConfidence(component);
    double newConf = std::max(0.0, std::min(1.0, current + adjustment));
    m_confidenceLevels[component] = newConf;
}
