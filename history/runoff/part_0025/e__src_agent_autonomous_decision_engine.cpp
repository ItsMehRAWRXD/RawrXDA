#include "autonomous_decision_engine.hpp"
#include <QDebug>
#include <QDateTime>
#include <QJsonArray>
#include <algorithm>

AutonomousDecisionEngine::AutonomousDecisionEngine(QObject* parent)
    : QObject(parent)
    , m_idCounter(0)
{
    initializeAutonomyLevels();
    initializeRiskFactors();
    
    // Set default autonomy level to supervised
    setAutonomyLevel("supervised");
}

AutonomousDecisionEngine::~AutonomousDecisionEngine()
{
}

DecisionOutcome AutonomousDecisionEngine::makeDecision(const DecisionContext& context)
{
    DecisionOutcome outcome;
    outcome.decisionId = generateUniqueId();
    outcome.decidedAt = QDateTime::currentDateTime();
    
    // If no actions available, we can't make a decision
    if (context.availableActions.isEmpty()) {
        outcome.chosenAction = "no_action";
        outcome.confidenceLevel = 0.0;
        outcome.expectedOutcome = "No actions available to take";
        outcome.requiresApproval = false;
        emit decisionMade(outcome);
        return outcome;
    }
    
    // Start with the first action as default
    outcome.chosenAction = context.availableActions.first();
    outcome.confidenceLevel = 0.5; // Default confidence
    
    // Assess risks for the chosen action
    outcome.risks = assessRisks(outcome.chosenAction, context);
    
    // Calculate confidence based on risks and context
    outcome.confidenceLevel = calculateConfidence(outcome.chosenAction, context, outcome.risks);
    
    // Determine expected outcome
    if (context.wish.contains("add", Qt::CaseInsensitive) || 
        context.wish.contains("implement", Qt::CaseInsensitive)) {
        outcome.expectedOutcome = "Feature implemented successfully";
    } else if (context.wish.contains("fix", Qt::CaseInsensitive) || 
               context.wish.contains("resolve", Qt::CaseInsensitive)) {
        outcome.expectedOutcome = "Issue resolved successfully";
    } else if (context.wish.contains("optimize", Qt::CaseInsensitive) || 
               context.wish.contains("improve", Qt::CaseInsensitive)) {
        outcome.expectedOutcome = "Performance or quality improved";
    } else {
        outcome.expectedOutcome = "Task completed successfully";
    }
    
    // Determine if human approval is required
    outcome.requiresApproval = requiresHumanApproval(outcome);
    
    // Log the decision
    logDecision(outcome);
    
    // Emit appropriate signal
    if (outcome.requiresApproval) {
        emit approvalRequired(outcome);
    } else {
        emit decisionMade(outcome);
    }
    
    return outcome;
}

QList<RiskFactor> AutonomousDecisionEngine::assessRisks(const QString& action, 
                                                       const DecisionContext& context)
{
    QList<RiskFactor> identifiedRisks;
    
    // Check for action-specific risks
    if (action == "FileEdit") {
        // Add file modification risks
        RiskFactor fileRisk;
        fileRisk.riskId = generateUniqueId();
        fileRisk.category = "data_integrity";
        fileRisk.description = "Risk of data loss or corruption during file modification";
        fileRisk.probability = 0.1; // 10% probability
        fileRisk.impact = 0.8; // High impact
        fileRisk.mitigationStrategies << "Create backup before modification" 
                                     << "Verify changes with diff" 
                                     << "Use atomic file operations";
        fileRisk.isMitigated = false;
        identifiedRisks.append(fileRisk);
        
        // Add security risk
        RiskFactor securityRisk;
        securityRisk.riskId = generateUniqueId();
        securityRisk.category = "security";
        securityRisk.description = "Risk of introducing security vulnerabilities";
        securityRisk.probability = 0.05; // 5% probability
        securityRisk.impact = 0.9; // High impact
        securityRisk.mitigationStrategies << "Review code for security issues" 
                                         << "Use secure coding practices" 
                                         << "Run security scans";
        securityRisk.isMitigated = false;
        identifiedRisks.append(securityRisk);
    } else if (action == "RunBuild") {
        // Add build failure risk
        RiskFactor buildRisk;
        buildRisk.riskId = generateUniqueId();
        buildRisk.category = "operational";
        buildRisk.description = "Risk of build failure causing downtime";
        buildRisk.probability = 0.2; // 20% probability
        buildRisk.impact = 0.6; // Medium impact
        buildRisk.mitigationStrategies << "Run build in isolated environment" 
                                      << "Use incremental builds" 
                                      << "Have rollback plan";
        buildRisk.isMitigated = false;
        identifiedRisks.append(buildRisk);
    } else if (action == "CommitGit") {
        // Add version control risk
        RiskFactor gitRisk;
        gitRisk.riskId = generateUniqueId();
        gitRisk.category = "version_control";
        gitRisk.description = "Risk of committing incorrect or incomplete changes";
        gitRisk.probability = 0.15; // 15% probability
        gitRisk.impact = 0.7; // High impact
        gitRisk.mitigationStrategies << "Review changes before commit" 
                                    << "Use pre-commit hooks" 
                                    << "Commit small, focused changes";
        gitRisk.isMitigated = false;
        identifiedRisks.append(gitRisk);
    }
    
    // Check for context-specific risks
    if (context.urgency == "high") {
        RiskFactor urgencyRisk;
        urgencyRisk.riskId = generateUniqueId();
        urgencyRisk.category = "process";
        urgencyRisk.description = "Risk of mistakes due to time pressure";
        urgencyRisk.probability = 0.3; // 30% probability
        urgencyRisk.impact = 0.5; // Medium impact
        urgencyRisk.mitigationStrategies << "Take time to review changes" 
                                        << "Get peer review if possible" 
                                        << "Document rushed decisions";
        urgencyRisk.isMitigated = false;
        identifiedRisks.append(urgencyRisk);
    }
    
    // Check for complexity-related risks
    if (context.complexity == "high") {
        RiskFactor complexityRisk;
        complexityRisk.riskId = generateUniqueId();
        complexityRisk.category = "technical";
        complexityRisk.description = "Risk of issues due to complex changes";
        complexityRisk.probability = 0.4; // 40% probability
        complexityRisk.impact = 0.7; // High impact
        complexityRisk.mitigationStrategies << "Break changes into smaller steps" 
                                           << "Test each step thoroughly" 
                                           << "Have expert review";
        complexityRisk.isMitigated = false;
        identifiedRisks.append(complexityRisk);
    }
    
    // Mitigate risks where possible
    QList<RiskFactor> mitigatedRisks = mitigateRisks(identifiedRisks);
    
    return mitigatedRisks;
}

double AutonomousDecisionEngine::calculateConfidence(const QString& action, 
                                                    const DecisionContext& context,
                                                    const QList<RiskFactor>& risks)
{
    // Start with base confidence
    double confidence = 0.7; // Default 70% confidence
    
    // Adjust based on risk score
    double riskScore = calculateRiskScore(risks);
    confidence -= riskScore * 0.5; // Risk can reduce confidence by up to 50%
    
    // Adjust based on autonomy level
    if (m_currentLevel.levelName == "fully-autonomous") {
        confidence += 0.2; // 20% boost for fully autonomous mode
    } else if (m_currentLevel.levelName == "semi-autonomous") {
        confidence += 0.1; // 10% boost for semi-autonomous mode
    }
    // supervised mode gets no boost
    
    // Adjust based on action type
    if (action == "QueryUser") {
        confidence += 0.3; // High confidence for user queries
    } else if (action == "SearchFiles") {
        confidence += 0.2; // Medium confidence for file searches
    } else if (action == "CommitGit") {
        confidence -= 0.2; // Lower confidence for git operations
    }
    
    // Adjust based on urgency
    if (context.urgency == "high") {
        confidence -= 0.1; // Slightly lower confidence for urgent tasks
    }
    
    // Adjust based on complexity
    if (context.complexity == "high") {
        confidence -= 0.2; // Lower confidence for complex tasks
    } else if (context.complexity == "low") {
        confidence += 0.1; // Higher confidence for simple tasks
    }
    
    // Clamp confidence between 0.0 and 1.0
    confidence = qMax(0.0, qMin(1.0, confidence));
    
    return confidence;
}

bool AutonomousDecisionEngine::requiresHumanApproval(const DecisionOutcome& decisionOutcome)
{
    // Check if any risk exceeds the current autonomy level threshold
    double riskScore = calculateRiskScore(decisionOutcome.risks);
    if (riskScore > m_currentLevel.maxRiskThreshold) {
        // Emit signal for exceeded risk threshold
        for (const RiskFactor& risk : decisionOutcome.risks) {
            double riskValue = risk.probability * risk.impact;
            if (riskValue > m_currentLevel.maxRiskThreshold) {
                emit riskThresholdExceeded(risk);
            }
        }
        return true;
    }
    
    // Check if action requires human override at current level
    if (m_currentLevel.requiresHumanOverride) {
        // Check if this specific action requires override
        // For now, we'll assume certain high-risk actions always need approval
        if (decisionOutcome.chosenAction == "CommitGit" || 
            decisionOutcome.chosenAction == "FileEdit") {
            return true;
        }
    }
    
    // Check if confidence is too low
    if (decisionOutcome.confidenceLevel < 0.3) {
        return true;
    }
    
    // Check if the current autonomy level allows this action
    if (!isActionAllowed(decisionOutcome.chosenAction)) {
        return true;
    }
    
    // No approval required
    return false;
}

void AutonomousDecisionEngine::setAutonomyLevel(const QString& level)
{
    for (const AutonomyLevel& autonomyLevel : m_autonomyLevels) {
        if (autonomyLevel.levelName == level) {
            m_currentLevel = autonomyLevel;
            emit autonomyLevelChanged(m_currentLevel);
            return;
        }
    }
    
    // If level not found, default to supervised
    qWarning() << "Autonomy level" << level << "not found, defaulting to supervised";
    for (const AutonomyLevel& autonomyLevel : m_autonomyLevels) {
        if (autonomyLevel.levelName == "supervised") {
            m_currentLevel = autonomyLevel;
            emit autonomyLevelChanged(m_currentLevel);
            return;
        }
    }
}

AutonomyLevel AutonomousDecisionEngine::getAutonomyLevel() const
{
    return m_currentLevel;
}

QList<AutonomyLevel> AutonomousDecisionEngine::getAutonomyLevels() const
{
    return m_autonomyLevels;
}

QJsonObject AutonomousDecisionEngine::evaluateCostBenefit(const QString& action, 
                                                         const DecisionContext& context)
{
    QJsonObject costBenefit;
    
    // Estimate costs
    QJsonObject costs;
    if (action == "FileEdit") {
        costs["time"] = 5; // 5 minutes
        costs["complexity"] = 3; // Scale of 1-10
        costs["risk"] = 7; // Scale of 1-10
    } else if (action == "RunBuild") {
        costs["time"] = 10; // 10 minutes
        costs["complexity"] = 2; // Scale of 1-10
        costs["risk"] = 5; // Scale of 1-10
    } else if (action == "CommitGit") {
        costs["time"] = 2; // 2 minutes
        costs["complexity"] = 1; // Scale of 1-10
        costs["risk"] = 8; // Scale of 1-10
    } else {
        costs["time"] = 3; // Default 3 minutes
        costs["complexity"] = 2; // Scale of 1-10
        costs["risk"] = 3; // Scale of 1-10
    }
    costBenefit["costs"] = costs;
    
    // Estimate benefits
    QJsonObject benefits;
    if (context.wish.contains("add", Qt::CaseInsensitive) || 
        context.wish.contains("implement", Qt::CaseInsensitive)) {
        benefits["value"] = 8; // Scale of 1-10
        benefits["impact"] = 7; // Scale of 1-10
        benefits["urgency"] = context.urgency == "high" ? 9 : 
                             context.urgency == "medium" ? 6 : 3;
    } else if (context.wish.contains("fix", Qt::CaseInsensitive) || 
               context.wish.contains("resolve", Qt::CaseInsensitive)) {
        benefits["value"] = 9; // Scale of 1-10
        benefits["impact"] = 8; // Scale of 1-10
        benefits["urgency"] = context.urgency == "high" ? 10 : 
                             context.urgency == "medium" ? 7 : 4;
    } else if (context.wish.contains("optimize", Qt::CaseInsensitive) || 
               context.wish.contains("improve", Qt::CaseInsensitive)) {
        benefits["value"] = 7; // Scale of 1-10
        benefits["impact"] = 6; // Scale of 1-10
        benefits["urgency"] = context.urgency == "high" ? 6 : 
                             context.urgency == "medium" ? 4 : 2;
    } else {
        benefits["value"] = 5; // Scale of 1-10
        benefits["impact"] = 4; // Scale of 1-10
        benefits["urgency"] = 3; // Scale of 1-10
    }
    costBenefit["benefits"] = benefits;
    
    // Calculate net benefit
    double totalCosts = costs["time"].toDouble() + costs["complexity"].toDouble() + costs["risk"].toDouble();
    double totalBenefits = benefits["value"].toDouble() + benefits["impact"].toDouble() + benefits["urgency"].toDouble();
    double netBenefit = totalBenefits - totalCosts;
    costBenefit["netBenefit"] = netBenefit;
    
    // Determine if action is recommended
    costBenefit["recommended"] = netBenefit > 0;
    
    return costBenefit;
}

void AutonomousDecisionEngine::logDecision(const DecisionOutcome& decision)
{
    // Add to decision history
    m_decisionHistory.append(decision);
    
    // Limit history to last 100 decisions
    if (m_decisionHistory.size() > 100) {
        m_decisionHistory.removeFirst();
    }
}

QList<DecisionOutcome> AutonomousDecisionEngine::getDecisionHistory(int limit) const
{
    // Return last 'limit' decisions
    int start = qMax(0, m_decisionHistory.size() - limit);
    return m_decisionHistory.mid(start);
}

void AutonomousDecisionEngine::initializeAutonomyLevels()
{
    // Supervised level - requires human approval for most actions
    AutonomyLevel supervised;
    supervised.levelName = "supervised";
    supervised.description = "Agent requires human approval for all significant actions";
    supervised.allowedActions << "SearchFiles" << "QueryUser";
    supervised.maxRiskThreshold = 0.3; // Low risk threshold
    supervised.requiresHumanOverride = true;
    m_autonomyLevels.append(supervised);
    
    // Semi-autonomous level - can execute low-risk actions independently
    AutonomyLevel semiAutonomous;
    semiAutonomous.levelName = "semi-autonomous";
    semiAutonomous.description = "Agent can execute low-risk actions independently";
    semiAutonomous.allowedActions << "SearchFiles" << "QueryUser" << "RunBuild" << "ExecuteTests";
    semiAutonomous.maxRiskThreshold = 0.5; // Medium risk threshold
    semiAutonomous.requiresHumanOverride = true;
    m_autonomyLevels.append(semiAutonomous);
    
    // Fully autonomous level - can execute most actions independently
    AutonomyLevel fullyAutonomous;
    fullyAutonomous.levelName = "fully-autonomous";
    fullyAutonomous.description = "Agent can execute most actions independently with risk monitoring";
    fullyAutonomous.allowedActions << "SearchFiles" << "QueryUser" << "RunBuild" << "ExecuteTests" 
                                  << "FileEdit" << "InvokeCommand";
    fullyAutonomous.maxRiskThreshold = 0.7; // High risk threshold
    fullyAutonomous.requiresHumanOverride = false;
    m_autonomyLevels.append(fullyAutonomous);
}

void AutonomousDecisionEngine::initializeRiskFactors()
{
    // Security risks
    RiskFactor securityRisk;
    securityRisk.riskId = generateUniqueId();
    securityRisk.category = "security";
    securityRisk.description = "Risk of introducing security vulnerabilities";
    securityRisk.probability = 0.1;
    securityRisk.impact = 0.9;
    securityRisk.mitigationStrategies << "Code review" << "Security scanning" << "Follow secure coding practices";
    securityRisk.isMitigated = false;
    m_riskFactors.append(securityRisk);
    
    // Data integrity risks
    RiskFactor dataRisk;
    dataRisk.riskId = generateUniqueId();
    dataRisk.category = "data_integrity";
    dataRisk.description = "Risk of data loss or corruption";
    dataRisk.probability = 0.05;
    dataRisk.impact = 0.95;
    dataRisk.mitigationStrategies << "Backup data" << "Use transactions" << "Validate changes";
    dataRisk.isMitigated = false;
    m_riskFactors.append(dataRisk);
    
    // Operational risks
    RiskFactor operationalRisk;
    operationalRisk.riskId = generateUniqueId();
    operationalRisk.category = "operational";
    operationalRisk.description = "Risk of system downtime or performance degradation";
    operationalRisk.probability = 0.15;
    operationalRisk.impact = 0.7;
    operationalRisk.mitigationStrategies << "Test in staging" << "Monitor performance" << "Have rollback plan";
    operationalRisk.isMitigated = false;
    m_riskFactors.append(operationalRisk);
}

QList<RiskFactor> AutonomousDecisionEngine::getRiskFactorsByCategory(const QString& category) const
{
    QList<RiskFactor> categoryRisks;
    for (const RiskFactor& risk : m_riskFactors) {
        if (risk.category == category) {
            categoryRisks.append(risk);
        }
    }
    return categoryRisks;
}

double AutonomousDecisionEngine::calculateRiskScore(const QList<RiskFactor>& risks) const
{
    if (risks.isEmpty()) {
        return 0.0;
    }
    
    double totalRisk = 0.0;
    for (const RiskFactor& risk : risks) {
        totalRisk += risk.probability * risk.impact;
    }
    
    // Return average risk score
    return totalRisk / risks.size();
}

QString AutonomousDecisionEngine::generateUniqueId()
{
    return QString::number(m_idCounter++);
}

bool AutonomousDecisionEngine::isActionAllowed(const QString& action) const
{
    return m_currentLevel.allowedActions.contains(action);
}

QList<RiskFactor> AutonomousDecisionEngine::mitigateRisks(const QList<RiskFactor>& risks)
{
    QList<RiskFactor> mitigatedRisks = risks;
    
    // Apply mitigation strategies more granularly with category-aware adjustments
    for (int i = 0; i < mitigatedRisks.size(); ++i) {
        RiskFactor& risk = mitigatedRisks[i];

        // Clamp incoming values
        risk.probability = qMax(0.0, qMin(1.0, risk.probability));
        risk.impact = qMax(0.0, qMin(1.0, risk.impact));

        const double originalProbability = risk.probability;
        const double originalImpact = risk.impact;

        // If risk has low probability and low impact, consider it mitigated early
        if (risk.probability < 0.1 && risk.impact < 0.3) {
            risk.isMitigated = true;
        }

        // Apply per-strategy effects with diminishing returns
        if (!risk.mitigationStrategies.isEmpty()) {
            int applied = 0;
            for (const QString& strat : risk.mitigationStrategies) {
                const QString s = strat.toLower();
                const double preP = risk.probability;
                const double preI = risk.impact;

                if (s.contains("backup")) {
                    // Backups mainly reduce impact of data loss
                    risk.impact *= 0.6;
                } else if (s.contains("review")) {
                    // Code review reduces likelihood of issues
                    risk.probability *= 0.8;
                } else if (s.contains("scan")) {
                    // Security scans reduce likelihood of vulnerabilities
                    risk.probability *= 0.85;
                } else if (s.contains("test") || s.contains("staging")) {
                    // Testing reduces likelihood and slightly reduces impact
                    risk.probability *= 0.85;
                    risk.impact *= 0.9;
                } else if (s.contains("rollback")) {
                    // Rollback plans reduce potential impact
                    risk.impact *= 0.85;
                } else if (s.contains("transaction") || s.contains("atomic")) {
                    // Atomic/transactional ops reduce both probability and impact
                    risk.probability *= 0.85;
                    risk.impact *= 0.8;
                } else if (s.contains("hooks")) {
                    // Pre-commit hooks reduce likelihood
                    risk.probability *= 0.9;
                } else if (s.contains("monitor")) {
                    // Monitoring reduces impact via faster detection
                    risk.impact *= 0.9;
                } else if (s.contains("incremental") || s.contains("small")) {
                    // Smaller, incremental changes reduce likelihood
                    risk.probability *= 0.9;
                } else if (s.contains("document")) {
                    // Documentation has a small positive effect on likelihood
                    risk.probability *= 0.97;
                } else {
                    // Generic small reduction for unspecified strategies
                    risk.probability *= 0.95;
                    risk.impact *= 0.97;
                }

                ++applied;
                // Diminishing returns after many strategies
                if (applied > 5) {
                    risk.probability = preP * 0.99 + (risk.probability - preP) * 0.5;
                    risk.impact = preI * 0.99 + (risk.impact - preI) * 0.5;
                }
            }

            // Category-specific floors and rules
            const QString cat = risk.category.toLower();
            if (cat == "security") {
                bool hasReview = false, hasScan = false;
                for (const QString& strat : risk.mitigationStrategies) {
                    const QString s = strat.toLower();
                    if (s.contains("review")) hasReview = true;
                    if (s.contains("scan")) hasScan = true;
                }
                // Without both review and scan, keep a conservative floor
                const double floorP = (hasReview && hasScan) ? 0.02 : 0.05;
                risk.probability = qMax(floorP, risk.probability);
            } else if (cat == "data_integrity") {
                bool hasBackup = false, hasTxn = false;
                for (const QString& strat : risk.mitigationStrategies) {
                    const QString s = strat.toLower();
                    if (s.contains("backup")) hasBackup = true;
                    if (s.contains("transaction") || s.contains("atomic")) hasTxn = true;
                }
                // Impact for data integrity shouldn't fall too low unless strong controls exist
                const double floorI = (hasBackup || hasTxn) ? 0.1 : 0.2;
                risk.impact = qMax(floorI, risk.impact);
            }

            // Final clamping
            risk.probability = qMax(0.0, qMin(1.0, risk.probability));
            risk.impact = qMax(0.0, qMin(1.0, risk.impact));
        }

        // Determine mitigation state based on thresholds and improvement
        const double originalScore = originalProbability * originalImpact;
        const double newScore = risk.probability * risk.impact;
        if (newScore < 0.15 || (originalScore > 0.0 && (originalScore - newScore) / originalScore >= 0.4)) {
            risk.isMitigated = true;
        }
    }
    
    return mitigatedRisks;
}