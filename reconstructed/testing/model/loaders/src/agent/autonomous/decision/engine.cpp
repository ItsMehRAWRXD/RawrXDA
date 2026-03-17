// ============================================================================
// File: src/agent/autonomous_decision_engine.cpp
// Purpose: Autonomous Decision Engine implementation
// Converted from Qt to pure C++17
// ============================================================================
#include "autonomous_decision_engine.hpp"
#include "../common/logger.hpp"
#include <algorithm>
#include <cmath>

AutonomousDecisionEngine::AutonomousDecisionEngine() : m_idCounter(0)
{
    initializeAutonomyLevels();
    initializeRiskFactors();
    setAutonomyLevel("supervised");
}

AutonomousDecisionEngine::~AutonomousDecisionEngine() {}

DecisionOutcome AutonomousDecisionEngine::makeDecision(const DecisionContext& context)
{
    DecisionOutcome outcome;
    outcome.decisionId = generateUniqueId();
    outcome.decidedAt = TimeUtils::now();

    if (context.availableActions.empty()) {
        outcome.chosenAction = "no_action";
        outcome.confidenceLevel = 0.0;
        outcome.expectedOutcome = "No actions available to take";
        outcome.requiresApproval = false;
        onDecisionMade.emit(outcome);
        return outcome;
    }

    outcome.chosenAction = context.availableActions.front();
    outcome.confidenceLevel = 0.5;
    outcome.risks = assessRisks(outcome.chosenAction, context);
    outcome.confidenceLevel = calculateConfidence(outcome.chosenAction, context, outcome.risks);

    if (StringUtils::containsCI(context.wish, "add") || StringUtils::containsCI(context.wish, "implement")) {
        outcome.expectedOutcome = "Feature implemented successfully";
    } else if (StringUtils::containsCI(context.wish, "fix") || StringUtils::containsCI(context.wish, "resolve")) {
        outcome.expectedOutcome = "Issue resolved successfully";
    } else if (StringUtils::containsCI(context.wish, "optimize") || StringUtils::containsCI(context.wish, "improve")) {
        outcome.expectedOutcome = "Performance or quality improved";
    } else {
        outcome.expectedOutcome = "Task completed successfully";
    }

    outcome.requiresApproval = requiresHumanApproval(outcome);
    logDecision(outcome);

    if (outcome.requiresApproval) onApprovalRequired.emit(outcome);
    else onDecisionMade.emit(outcome);

    return outcome;
}

std::vector<RiskFactor> AutonomousDecisionEngine::assessRisks(const std::string& action, const DecisionContext& context)
{
    std::vector<RiskFactor> identifiedRisks;

    if (action == "FileEdit") {
        RiskFactor fileRisk;
        fileRisk.riskId = generateUniqueId();
        fileRisk.category = "data_integrity";
        fileRisk.description = "Risk of data loss or corruption during file modification";
        fileRisk.probability = 0.1; fileRisk.impact = 0.8;
        fileRisk.mitigationStrategies = {"Create backup before modification", "Verify changes with diff", "Use atomic file operations"};
        fileRisk.isMitigated = false;
        identifiedRisks.push_back(fileRisk);

        RiskFactor secRisk;
        secRisk.riskId = generateUniqueId(); secRisk.category = "security";
        secRisk.description = "Risk of introducing security vulnerabilities";
        secRisk.probability = 0.05; secRisk.impact = 0.9;
        secRisk.mitigationStrategies = {"Review code for security issues", "Use secure coding practices", "Run security scans"};
        secRisk.isMitigated = false;
        identifiedRisks.push_back(secRisk);
    } else if (action == "RunBuild") {
        RiskFactor buildRisk;
        buildRisk.riskId = generateUniqueId(); buildRisk.category = "operational";
        buildRisk.description = "Risk of build failure causing downtime";
        buildRisk.probability = 0.2; buildRisk.impact = 0.6;
        buildRisk.mitigationStrategies = {"Run build in isolated environment", "Use incremental builds", "Have rollback plan"};
        buildRisk.isMitigated = false;
        identifiedRisks.push_back(buildRisk);
    } else if (action == "CommitGit") {
        RiskFactor gitRisk;
        gitRisk.riskId = generateUniqueId(); gitRisk.category = "version_control";
        gitRisk.description = "Risk of committing incorrect or incomplete changes";
        gitRisk.probability = 0.15; gitRisk.impact = 0.7;
        gitRisk.mitigationStrategies = {"Review changes before commit", "Use pre-commit hooks", "Commit small, focused changes"};
        gitRisk.isMitigated = false;
        identifiedRisks.push_back(gitRisk);
    }

    if (context.urgency == "high") {
        RiskFactor urgencyRisk;
        urgencyRisk.riskId = generateUniqueId(); urgencyRisk.category = "process";
        urgencyRisk.description = "Risk of mistakes due to time pressure";
        urgencyRisk.probability = 0.3; urgencyRisk.impact = 0.5;
        urgencyRisk.mitigationStrategies = {"Take time to review changes", "Get peer review if possible", "Document rushed decisions"};
        urgencyRisk.isMitigated = false;
        identifiedRisks.push_back(urgencyRisk);
    }

    if (context.complexity == "high") {
        RiskFactor complexityRisk;
        complexityRisk.riskId = generateUniqueId(); complexityRisk.category = "technical";
        complexityRisk.description = "Risk of issues due to complex changes";
        complexityRisk.probability = 0.4; complexityRisk.impact = 0.7;
        complexityRisk.mitigationStrategies = {"Break changes into smaller steps", "Test each step thoroughly", "Have expert review"};
        complexityRisk.isMitigated = false;
        identifiedRisks.push_back(complexityRisk);
    }

    return mitigateRisks(identifiedRisks);
}

double AutonomousDecisionEngine::calculateConfidence(const std::string& action,
                                                    const DecisionContext& context,
                                                    const std::vector<RiskFactor>& risks)
{
    double confidence = 0.7;
    confidence -= calculateRiskScore(risks) * 0.5;
    if (m_currentLevel.levelName == "fully-autonomous") confidence += 0.2;
    else if (m_currentLevel.levelName == "semi-autonomous") confidence += 0.1;
    if (action == "QueryUser") confidence += 0.3;
    else if (action == "SearchFiles") confidence += 0.2;
    else if (action == "CommitGit") confidence -= 0.2;
    if (context.urgency == "high") confidence -= 0.1;
    if (context.complexity == "high") confidence -= 0.2;
    else if (context.complexity == "low") confidence += 0.1;
    return std::max(0.0, std::min(1.0, confidence));
}

bool AutonomousDecisionEngine::requiresHumanApproval(const DecisionOutcome& decisionOutcome)
{
    double riskScore = calculateRiskScore(decisionOutcome.risks);
    if (riskScore > m_currentLevel.maxRiskThreshold) {
        for (const auto& risk : decisionOutcome.risks) {
            if (risk.probability * risk.impact > m_currentLevel.maxRiskThreshold)
                onRiskThresholdExceeded.emit(risk);
        }
        return true;
    }
    if (m_currentLevel.requiresHumanOverride) {
        if (decisionOutcome.chosenAction == "CommitGit" || decisionOutcome.chosenAction == "FileEdit")
            return true;
    }
    if (decisionOutcome.confidenceLevel < 0.3) return true;
    if (!isActionAllowed(decisionOutcome.chosenAction)) return true;
    return false;
}

void AutonomousDecisionEngine::setAutonomyLevel(const std::string& level)
{
    for (const auto& al : m_autonomyLevels) {
        if (al.levelName == level) { m_currentLevel = al; onAutonomyLevelChanged.emit(m_currentLevel); return; }
    }
    logWarning() << "Autonomy level" << level << "not found, defaulting to supervised";
    for (const auto& al : m_autonomyLevels) {
        if (al.levelName == "supervised") { m_currentLevel = al; onAutonomyLevelChanged.emit(m_currentLevel); return; }
    }
}

AutonomyLevel AutonomousDecisionEngine::getAutonomyLevel() const { return m_currentLevel; }
std::vector<AutonomyLevel> AutonomousDecisionEngine::getAutonomyLevels() const { return m_autonomyLevels; }

JsonObject AutonomousDecisionEngine::evaluateCostBenefit(const std::string& action, const DecisionContext& context)
{
    JsonObject costBenefit;
    JsonObject costs;
    if (action == "FileEdit") { costs["time"] = JsonValue(5); costs["complexity"] = JsonValue(3); costs["risk"] = JsonValue(7); }
    else if (action == "RunBuild") { costs["time"] = JsonValue(10); costs["complexity"] = JsonValue(2); costs["risk"] = JsonValue(5); }
    else if (action == "CommitGit") { costs["time"] = JsonValue(2); costs["complexity"] = JsonValue(1); costs["risk"] = JsonValue(8); }
    else { costs["time"] = JsonValue(3); costs["complexity"] = JsonValue(2); costs["risk"] = JsonValue(3); }
    costBenefit["costs"] = JsonValue(costs);

    JsonObject benefits;
    if (StringUtils::containsCI(context.wish, "add") || StringUtils::containsCI(context.wish, "implement")) {
        benefits["value"] = JsonValue(8); benefits["impact"] = JsonValue(7);
        benefits["urgency"] = JsonValue(context.urgency == "high" ? 9 : (context.urgency == "medium" ? 6 : 3));
    } else if (StringUtils::containsCI(context.wish, "fix") || StringUtils::containsCI(context.wish, "resolve")) {
        benefits["value"] = JsonValue(9); benefits["impact"] = JsonValue(8);
        benefits["urgency"] = JsonValue(context.urgency == "high" ? 10 : (context.urgency == "medium" ? 7 : 4));
    } else if (StringUtils::containsCI(context.wish, "optimize") || StringUtils::containsCI(context.wish, "improve")) {
        benefits["value"] = JsonValue(7); benefits["impact"] = JsonValue(6);
        benefits["urgency"] = JsonValue(context.urgency == "high" ? 6 : (context.urgency == "medium" ? 4 : 2));
    } else {
        benefits["value"] = JsonValue(5); benefits["impact"] = JsonValue(4); benefits["urgency"] = JsonValue(3);
    }
    costBenefit["benefits"] = JsonValue(benefits);

    double totalCosts = costs["time"].toDouble() + costs["complexity"].toDouble() + costs["risk"].toDouble();
    double totalBenefits = benefits["value"].toDouble() + benefits["impact"].toDouble() + benefits["urgency"].toDouble();
    costBenefit["netBenefit"] = JsonValue(totalBenefits - totalCosts);
    costBenefit["recommended"] = JsonValue(totalBenefits - totalCosts > 0);
    return costBenefit;
}

void AutonomousDecisionEngine::logDecision(const DecisionOutcome& decision)
{
    m_decisionHistory.push_back(decision);
    if (m_decisionHistory.size() > 100) m_decisionHistory.erase(m_decisionHistory.begin());
}

std::vector<DecisionOutcome> AutonomousDecisionEngine::getDecisionHistory(int limit) const
{
    int start = std::max(0, static_cast<int>(m_decisionHistory.size()) - limit);
    return std::vector<DecisionOutcome>(m_decisionHistory.begin() + start, m_decisionHistory.end());
}

void AutonomousDecisionEngine::initializeAutonomyLevels()
{
    AutonomyLevel supervised;
    supervised.levelName = "supervised";
    supervised.description = "Agent requires human approval for all significant actions";
    supervised.allowedActions = {"SearchFiles", "QueryUser"};
    supervised.maxRiskThreshold = 0.3;
    supervised.requiresHumanOverride = true;
    m_autonomyLevels.push_back(supervised);

    AutonomyLevel semi;
    semi.levelName = "semi-autonomous";
    semi.description = "Agent can execute low-risk actions independently";
    semi.allowedActions = {"SearchFiles", "QueryUser", "RunBuild", "ExecuteTests"};
    semi.maxRiskThreshold = 0.5;
    semi.requiresHumanOverride = true;
    m_autonomyLevels.push_back(semi);

    AutonomyLevel full;
    full.levelName = "fully-autonomous";
    full.description = "Agent can execute most actions independently with risk monitoring";
    full.allowedActions = {"SearchFiles", "QueryUser", "RunBuild", "ExecuteTests", "FileEdit", "InvokeCommand"};
    full.maxRiskThreshold = 0.7;
    full.requiresHumanOverride = false;
    m_autonomyLevels.push_back(full);
}

void AutonomousDecisionEngine::initializeRiskFactors()
{
    RiskFactor sec; sec.riskId = generateUniqueId(); sec.category = "security";
    sec.description = "Risk of introducing security vulnerabilities";
    sec.probability = 0.1; sec.impact = 0.9;
    sec.mitigationStrategies = {"Code review", "Security scanning", "Follow secure coding practices"};
    sec.isMitigated = false;
    m_riskFactors.push_back(sec);

    RiskFactor data; data.riskId = generateUniqueId(); data.category = "data_integrity";
    data.description = "Risk of data loss or corruption";
    data.probability = 0.05; data.impact = 0.95;
    data.mitigationStrategies = {"Backup data", "Use transactions", "Validate changes"};
    data.isMitigated = false;
    m_riskFactors.push_back(data);

    RiskFactor ops; ops.riskId = generateUniqueId(); ops.category = "operational";
    ops.description = "Risk of system downtime or performance degradation";
    ops.probability = 0.15; ops.impact = 0.7;
    ops.mitigationStrategies = {"Test in staging", "Monitor performance", "Have rollback plan"};
    ops.isMitigated = false;
    m_riskFactors.push_back(ops);
}

std::vector<RiskFactor> AutonomousDecisionEngine::getRiskFactorsByCategory(const std::string& category) const
{
    std::vector<RiskFactor> result;
    for (const auto& r : m_riskFactors)
        if (r.category == category) result.push_back(r);
    return result;
}

double AutonomousDecisionEngine::calculateRiskScore(const std::vector<RiskFactor>& risks) const
{
    if (risks.empty()) return 0.0;
    double total = 0.0;
    for (const auto& r : risks) total += r.probability * r.impact;
    return total / risks.size();
}

std::string AutonomousDecisionEngine::generateUniqueId() { return std::to_string(m_idCounter++); }

bool AutonomousDecisionEngine::isActionAllowed(const std::string& action) const
{
    return std::find(m_currentLevel.allowedActions.begin(), m_currentLevel.allowedActions.end(), action) != m_currentLevel.allowedActions.end();
}

std::vector<RiskFactor> AutonomousDecisionEngine::mitigateRisks(const std::vector<RiskFactor>& risks)
{
    std::vector<RiskFactor> mitigated = risks;
    for (auto& risk : mitigated) {
        risk.probability = std::max(0.0, std::min(1.0, risk.probability));
        risk.impact = std::max(0.0, std::min(1.0, risk.impact));
        double origP = risk.probability, origI = risk.impact;

        if (risk.probability < 0.1 && risk.impact < 0.3) { risk.isMitigated = true; }

        if (!risk.mitigationStrategies.empty()) {
            int applied = 0;
            for (const auto& strat : risk.mitigationStrategies) {
                std::string s = StringUtils::toLower(strat);
                if (StringUtils::contains(s, "backup")) risk.impact *= 0.6;
                else if (StringUtils::contains(s, "review")) risk.probability *= 0.8;
                else if (StringUtils::contains(s, "scan")) risk.probability *= 0.85;
                else if (StringUtils::contains(s, "test") || StringUtils::contains(s, "staging")) { risk.probability *= 0.85; risk.impact *= 0.9; }
                else if (StringUtils::contains(s, "rollback")) risk.impact *= 0.85;
                else if (StringUtils::contains(s, "transaction") || StringUtils::contains(s, "atomic")) { risk.probability *= 0.85; risk.impact *= 0.8; }
                else if (StringUtils::contains(s, "hooks")) risk.probability *= 0.9;
                else if (StringUtils::contains(s, "monitor")) risk.impact *= 0.9;
                else if (StringUtils::contains(s, "incremental") || StringUtils::contains(s, "small")) risk.probability *= 0.9;
                else if (StringUtils::contains(s, "document")) risk.probability *= 0.97;
                else { risk.probability *= 0.95; risk.impact *= 0.97; }
                ++applied;
            }

            std::string cat = StringUtils::toLower(risk.category);
            if (cat == "security") {
                bool hasReview = false, hasScan = false;
                for (const auto& st : risk.mitigationStrategies) {
                    std::string sl = StringUtils::toLower(st);
                    if (StringUtils::contains(sl, "review")) hasReview = true;
                    if (StringUtils::contains(sl, "scan")) hasScan = true;
                }
                double floorP = (hasReview && hasScan) ? 0.02 : 0.05;
                risk.probability = std::max(floorP, risk.probability);
            } else if (cat == "data_integrity") {
                bool hasBackup = false, hasTxn = false;
                for (const auto& st : risk.mitigationStrategies) {
                    std::string sl = StringUtils::toLower(st);
                    if (StringUtils::contains(sl, "backup")) hasBackup = true;
                    if (StringUtils::contains(sl, "transaction") || StringUtils::contains(sl, "atomic")) hasTxn = true;
                }
                double floorI = (hasBackup || hasTxn) ? 0.1 : 0.2;
                risk.impact = std::max(floorI, risk.impact);
            }

            risk.probability = std::max(0.0, std::min(1.0, risk.probability));
            risk.impact = std::max(0.0, std::min(1.0, risk.impact));
        }

        double origScore = origP * origI;
        double newScore = risk.probability * risk.impact;
        if (newScore < 0.15 || (origScore > 0.0 && (origScore - newScore) / origScore >= 0.4))
            risk.isMitigated = true;
    }
    return mitigated;
}
