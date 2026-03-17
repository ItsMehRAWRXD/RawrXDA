#include "agent_self_reflection.hpp"
#include <QDebug>
#include <QDateTime>
#include <QUuid>

AgentSelfReflection::AgentSelfReflection(QObject* parent)
    : QObject(parent)
    , m_idCounter(0)
{
    initializeConfidenceLevels();
}

AgentSelfReflection::~AgentSelfReflection()
{
}

ErrorAnalysis AgentSelfReflection::analyzeError(const QString& actionType, 
                                               const QString& errorMessage,
                                               const QString& context)
{
    ErrorAnalysis analysis;
    analysis.errorId = generateUniqueId();
    analysis.actionType = actionType;
    analysis.errorMessage = errorMessage;
    analysis.rootCause = identifyRootCause(errorMessage);
    analysis.impact = calculateImpact(errorMessage);
    analysis.recommendations = generateRecommendations(analysis.rootCause);
    analysis.analyzedAt = QDateTime::currentDateTime();
    analysis.requiresHumanInput = requiresHumanInput(analysis.rootCause, analysis.impact);
    
    emit errorAnalyzed(analysis);
    
    return analysis;
}

QList<AlternativeApproach> AgentSelfReflection::generateAlternatives(const QString& actionType,
                                                                    const ErrorAnalysis& errorAnalysis,
                                                                    const QString& context)
{
    QList<AlternativeApproach> alternatives;
    
    // Generate alternative approaches based on the error analysis
    if (actionType == "FileEdit") {
        if (errorAnalysis.rootCause.contains("permission", Qt::CaseInsensitive)) {
            AlternativeApproach approach1;
            approach1.approachId = generateUniqueId();
            approach1.description = "Use elevated privileges to modify file";
            approach1.confidenceLevel = 0.8;
            approach1.expectedSuccess = 0.9;
            approach1.requiredSteps << "Request administrator privileges" << "Retry file operation";
            approach1.estimatedTime = "5 minutes";
            alternatives.append(approach1);
            
            AlternativeApproach approach2;
            approach2.approachId = generateUniqueId();
            approach2.description = "Create a copy and replace original manually";
            approach2.confidenceLevel = 0.7;
            approach2.expectedSuccess = 0.8;
            approach2.requiredSteps << "Create backup" << "Modify copy" << "Replace original";
            approach2.estimatedTime = "10 minutes";
            alternatives.append(approach2);
        } else if (errorAnalysis.rootCause.contains("not found", Qt::CaseInsensitive)) {
            AlternativeApproach approach1;
            approach1.approachId = generateUniqueId();
            approach1.description = "Verify file path and create directory structure if needed";
            approach1.confidenceLevel = 0.9;
            approach1.expectedSuccess = 0.95;
            approach1.requiredSteps << "Check file path" << "Create directories" << "Create file";
            approach1.estimatedTime = "3 minutes";
            alternatives.append(approach1);
        }
    } else if (actionType == "RunBuild") {
        if (errorAnalysis.rootCause.contains("dependency", Qt::CaseInsensitive)) {
            AlternativeApproach approach1;
            approach1.approachId = generateUniqueId();
            approach1.description = "Install missing dependencies";
            approach1.confidenceLevel = 0.85;
            approach1.expectedSuccess = 0.9;
            approach1.requiredSteps << "Identify missing dependencies" << "Install dependencies" << "Retry build";
            approach1.estimatedTime = "15 minutes";
            alternatives.append(approach1);
            
            AlternativeApproach approach2;
            approach2.approachId = generateUniqueId();
            approach2.description = "Use alternative build configuration";
            approach2.confidenceLevel = 0.7;
            approach2.expectedSuccess = 0.75;
            approach2.requiredSteps << "Check available configurations" << "Select alternative" << "Run build";
            approach2.estimatedTime = "10 minutes";
            alternatives.append(approach2);
        }
    }
    
    // Add a generic fallback approach
    AlternativeApproach fallback;
    fallback.approachId = generateUniqueId();
    fallback.description = "Break task into smaller steps and execute incrementally";
    fallback.confidenceLevel = 0.6;
    fallback.expectedSuccess = 0.7;
    fallback.requiredSteps << "Decompose task" << "Execute steps one by one" << "Validate each step";
    fallback.estimatedTime = "Varies based on task complexity";
    alternatives.append(fallback);
    
    emit alternativesGenerated(alternatives);
    
    return alternatives;
}

ConfidenceAdjustment AgentSelfReflection::adjustConfidence(bool success, 
                                                          const QString& actionType,
                                                          int executionTime)
{
    ConfidenceAdjustment adjustment;
    adjustment.component = actionType;
    adjustment.adjustedAt = QDateTime::currentDateTime();
    
    if (success) {
        // Increase confidence for successful execution
        adjustment.adjustment = 0.05; // 5% increase
        adjustment.reason = "Successful execution of " + actionType;
        
        // Additional confidence boost for fast execution
        if (executionTime < 30000) { // Less than 30 seconds
            adjustment.adjustment += 0.02; // Additional 2% boost
            adjustment.reason += " with fast execution time";
        }
    } else {
        // Decrease confidence for failed execution
        adjustment.adjustment = -0.1; // 10% decrease
        adjustment.reason = "Failed execution of " + actionType;
        
        // Larger decrease for slow failures
        if (executionTime > 120000) { // More than 2 minutes
            adjustment.adjustment -= 0.05; // Additional 5% decrease
            adjustment.reason += " with slow execution time";
        }
    }
    
    updateConfidence(actionType, adjustment.adjustment);
    
    emit confidenceAdjusted(adjustment);
    
    return adjustment;
}

bool AgentSelfReflection::shouldEscalateToHuman(const ErrorAnalysis& errorAnalysis,
                                               const QList<AlternativeApproach>& alternatives)
{
    // Escalate if:
    // 1. Error analysis indicates human input is required
    // 2. No high-confidence alternatives are available
    // 3. Impact is high and confidence is low
    
    if (errorAnalysis.requiresHumanInput) {
        emit escalationRecommended(errorAnalysis);
        return true;
    }
    
    // Check if any alternative has high confidence (> 0.8)
    bool hasHighConfidenceAlternative = false;
    for (const AlternativeApproach& approach : alternatives) {
        if (approach.confidenceLevel > 0.8) {
            hasHighConfidenceAlternative = true;
            break;
        }
    }
    
    if (!hasHighConfidenceAlternative) {
        // If no high-confidence alternatives, check overall confidence
        if (getOverallConfidence() < 0.3) {
            ErrorAnalysis escalationAnalysis = errorAnalysis;
            escalationAnalysis.errorMessage = "Low overall confidence and no viable alternatives";
            emit escalationRecommended(escalationAnalysis);
            return true;
        }
    }
    
    // Check if impact is high and confidence is low
    if (errorAnalysis.impact == "high" && getActionConfidence(errorAnalysis.actionType) < 0.4) {
        emit escalationRecommended(errorAnalysis);
        return true;
    }
    
    return false;
}

void AgentSelfReflection::learnFromExecution(const QString& actionType, 
                                            bool success, 
                                            int executionTime,
                                            const QString& errorMessage)
{
    // Update execution counts
    m_executionCounts[actionType]++;
    
    if (success) {
        m_successCounts[actionType]++;
    }
    
    // Adjust confidence based on this execution
    adjustConfidence(success, actionType, executionTime);
    
    // If this was a failure, analyze the error
    if (!success && !errorMessage.isEmpty()) {
        ErrorAnalysis analysis = analyzeError(actionType, errorMessage, "");
        
        // If escalation is recommended, emit the signal
        QList<AlternativeApproach> alternatives = generateAlternatives(actionType, analysis, "");
        if (shouldEscalateToHuman(analysis, alternatives)) {
            // Escalation signal already emitted in shouldEscalateToHuman
        }
    }
}

double AgentSelfReflection::getActionConfidence(const QString& actionType) const
{
    auto it = m_confidenceLevels.find(actionType);
    if (it != m_confidenceLevels.end()) {
        return it.value();
    }
    return 0.5; // Default confidence
}

double AgentSelfReflection::getOverallConfidence() const
{
    if (m_confidenceLevels.isEmpty()) {
        return 0.5; // Default confidence
    }
    
    double totalConfidence = 0.0;
    for (auto it = m_confidenceLevels.begin(); it != m_confidenceLevels.end(); ++it) {
        totalConfidence += it.value();
    }
    
    return totalConfidence / m_confidenceLevels.size();
}

void AgentSelfReflection::resetConfidence()
{
    initializeConfidenceLevels();
}

void AgentSelfReflection::initializeConfidenceLevels()
{
    // Initialize default confidence levels for different action types
    m_confidenceLevels["FileEdit"] = 0.7;
    m_confidenceLevels["SearchFiles"] = 0.8;
    m_confidenceLevels["RunBuild"] = 0.6;
    m_confidenceLevels["ExecuteTests"] = 0.7;
    m_confidenceLevels["CommitGit"] = 0.6;
    m_confidenceLevels["InvokeCommand"] = 0.5;
    m_confidenceLevels["RecursiveAgent"] = 0.4;
    m_confidenceLevels["QueryUser"] = 0.9;
    
    // Initialize execution counts
    for (auto it = m_confidenceLevels.begin(); it != m_confidenceLevels.end(); ++it) {
        m_executionCounts[it.key()] = 0;
        m_successCounts[it.key()] = 0;
    }
}

QString AgentSelfReflection::identifyRootCause(const QString& errorMessage) const
{
    // Simple rule-based root cause identification
    // In a real implementation, this would be more sophisticated
    
    if (errorMessage.contains("permission", Qt::CaseInsensitive)) {
        return "Insufficient permissions to perform operation";
    } else if (errorMessage.contains("not found", Qt::CaseInsensitive) || 
               errorMessage.contains("no such file", Qt::CaseInsensitive)) {
        return "File or directory not found";
    } else if (errorMessage.contains("syntax", Qt::CaseInsensitive)) {
        return "Syntax error in code or configuration";
    } else if (errorMessage.contains("dependency", Qt::CaseInsensitive) ||
               errorMessage.contains("missing", Qt::CaseInsensitive)) {
        return "Missing dependency or resource";
    } else if (errorMessage.contains("timeout", Qt::CaseInsensitive)) {
        return "Operation timed out";
    } else if (errorMessage.contains("memory", Qt::CaseInsensitive)) {
        return "Insufficient memory available";
    } else if (errorMessage.contains("network", Qt::CaseInsensitive)) {
        return "Network connectivity issue";
    } else {
        return "Unknown or unspecified error";
    }
}

QStringList AgentSelfReflection::generateRecommendations(const QString& rootCause) const
{
    QStringList recommendations;
    
    if (rootCause.contains("permission", Qt::CaseInsensitive)) {
        recommendations << "Check user permissions for the affected resource"
                       << "Run the agent with elevated privileges if necessary"
                       << "Verify file/directory ownership";
    } else if (rootCause.contains("not found", Qt::CaseInsensitive)) {
        recommendations << "Verify the file path is correct"
                       << "Check if the file/directory exists"
                       << "Create missing directories if needed";
    } else if (rootCause.contains("syntax", Qt::CaseInsensitive)) {
        recommendations << "Review the code or configuration for syntax errors"
                       << "Use a linter to identify issues"
                       << "Check documentation for correct syntax";
    } else if (rootCause.contains("dependency", Qt::CaseInsensitive)) {
        recommendations << "Install missing dependencies"
                       << "Check project documentation for requirements"
                       << "Verify dependency versions are compatible";
    } else if (rootCause.contains("timeout", Qt::CaseInsensitive)) {
        recommendations << "Increase timeout values for the operation"
                       << "Check system resources"
                       << "Optimize the operation to reduce execution time";
    } else if (rootCause.contains("memory", Qt::CaseInsensitive)) {
        recommendations << "Close unnecessary applications to free memory"
                       << "Increase system memory if possible"
                       << "Optimize memory usage in the operation";
    } else if (rootCause.contains("network", Qt::CaseInsensitive)) {
        recommendations << "Check network connectivity"
                       << "Verify firewall settings"
                       << "Retry the operation when network is stable";
    } else {
        recommendations << "Review the error message for specific details"
                       << "Check system logs for additional information"
                       << "Consult documentation or seek expert assistance";
    }
    
    return recommendations;
}

QString AgentSelfReflection::calculateImpact(const QString& errorMessage) const
{
    // Simple impact calculation based on keywords
    if (errorMessage.contains("critical", Qt::CaseInsensitive) ||
        errorMessage.contains("fatal", Qt::CaseInsensitive) ||
        errorMessage.contains("corrupt", Qt::CaseInsensitive)) {
        return "high";
    } else if (errorMessage.contains("warning", Qt::CaseInsensitive) ||
               errorMessage.contains("deprecated", Qt::CaseInsensitive)) {
        return "low";
    } else {
        return "medium";
    }
}

bool AgentSelfReflection::requiresHumanInput(const QString& rootCause, const QString& impact) const
{
    // Require human input for high-impact issues or specific root causes
    if (impact == "high") {
        return true;
    }
    
    if (rootCause.contains("unknown", Qt::CaseInsensitive) ||
        rootCause.contains("unspecified", Qt::CaseInsensitive)) {
        return true;
    }
    
    return false;
}

QString AgentSelfReflection::generateUniqueId()
{
    return QString::number(m_idCounter++);
}

void AgentSelfReflection::updateConfidence(const QString& component, double adjustment)
{
    double currentConfidence = getActionConfidence(component);
    double newConfidence = currentConfidence + adjustment;
    
    // Clamp confidence between 0.0 and 1.0
    newConfidence = qMax(0.0, qMin(1.0, newConfidence));
    
    m_confidenceLevels[component] = newConfidence;
}