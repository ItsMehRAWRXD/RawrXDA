// agentic_failure_detector.cpp - Implementation of failure detection
#include "agentic_failure_detector.hpp"
#include <QDebug>
#include <QRegularExpression>
#include <algorithm>

AgenticFailureDetector::AgenticFailureDetector(QObject* parent)
    : QObject(parent)
{
    initializePatterns();
    qInfo() << "[AgenticFailureDetector] Initialized with pattern library";
}

AgenticFailureDetector::~AgenticFailureDetector()
{
}

void AgenticFailureDetector::initializePatterns()
{
    QMutexLocker locker(&m_mutex);
    
    // Refusal patterns
    m_refusalPatterns << "I can't" << "I cannot" << "I'm not able to"
                     << "I can't assist" << "I'm unable" << "I don't feel comfortable"
                     << "I decline" << "I won't" << "I must refuse"
                     << "I'm not permitted" << "against my values"
                     << "not allowed to" << "I apologize, but I cannot";
    
    // Hallucination patterns
    m_hallucinationPatterns << "I think" << "probably" << "likely"
                           << "I might have" << "I'm not sure but"
                           << "As far as I know" << "To my knowledge"
                           << "might be" << "could be" << "seems like";
    
    // Infinite loop patterns
    m_loopPatterns << "same" << "repeated" << "over and over"
                  << "again and again" << "same thing";
    
    // Safety patterns
    m_safetyPatterns << "[SENSITIVE]" << "[REDACTED]" << "[FILTERED]"
                    << "[BLOCKED]" << "[SAFETY]" << "[WARNING]";
    
    // Timeout indicators
    m_timeoutIndicators << "[Timeout]" << "[TIMEOUT]" << "timed out"
                       << "inference timeout" << "deadline exceeded";
    
    // Resource exhaustion indicators
    m_resourceExhaustionIndicators << "out of memory" << "OOM" << "[OOM]"
                                  << "resource exhausted" << "no GPU memory"
                                  << "device out of memory";
}

FailureInfo AgenticFailureDetector::detectFailure(const QString& modelOutput, const QString& context)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enabled) {
        return FailureInfo{AgentFailureType::None, "Detector disabled", 0.0, "", QDateTime::currentDateTime(), m_sequenceNumber};
    }
    
    if (modelOutput.isEmpty()) {
        return FailureInfo{AgentFailureType::Refusal, "Empty output", 0.5, "No response generated", QDateTime::currentDateTime(), m_sequenceNumber};
    }
    
    m_stats.totalOutputsAnalyzed++;
    
    // Check each failure type in priority order
    if (isRefusal(modelOutput)) {
        FailureInfo info{AgentFailureType::Refusal, "Model refusal detected", 
                        calculateConfidence(AgentFailureType::Refusal, modelOutput),
                        "Contains refusal keywords", QDateTime::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Refusal)]++;
        return info;
    }
    
    if (isSafetyViolation(modelOutput)) {
        FailureInfo info{AgentFailureType::SafetyViolation, "Safety filter triggered",
                        calculateConfidence(AgentFailureType::SafetyViolation, modelOutput),
                        "Contains safety markers", QDateTime::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::SafetyViolation)]++;
        return info;
    }
    
    if (isTokenLimitExceeded(modelOutput)) {
        FailureInfo info{AgentFailureType::TokenLimitExceeded, "Token limit exceeded",
                        0.9, "Response truncated or incomplete", QDateTime::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::TokenLimitExceeded)]++;
        return info;
    }
    
    if (isTimeout(modelOutput)) {
        FailureInfo info{AgentFailureType::Timeout, "Inference timeout",
                        0.95, "Timeout indicator detected", QDateTime::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Timeout)]++;
        return info;
    }
    
    if (isResourceExhausted(modelOutput)) {
        FailureInfo info{AgentFailureType::ResourceExhausted, "Resource exhaustion",
                        0.95, "Out of memory or compute resources", QDateTime::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::ResourceExhausted)]++;
        return info;
    }
    
    if (isInfiniteLoop(modelOutput)) {
        FailureInfo info{AgentFailureType::InfiniteLoop, "Infinite loop detected",
                        calculateConfidence(AgentFailureType::InfiniteLoop, modelOutput),
                        "Repeating content pattern", QDateTime::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::InfiniteLoop)]++;
        return info;
    }
    
    if (isFormatViolation(modelOutput)) {
        FailureInfo info{AgentFailureType::FormatViolation, "Format violation detected",
                        calculateConfidence(AgentFailureType::FormatViolation, modelOutput),
                        "Output format incorrect", QDateTime::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::FormatViolation)]++;
        return info;
    }
    
    if (isHallucination(modelOutput)) {
        FailureInfo info{AgentFailureType::Hallucination, "Hallucination indicators",
                        calculateConfidence(AgentFailureType::Hallucination, modelOutput),
                        "Contains uncertain language patterns", QDateTime::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Hallucination)]++;
        return info;
    }
    
    // No failure detected
    return FailureInfo{AgentFailureType::None, "No failure detected", 1.0, "", QDateTime::currentDateTime(), m_sequenceNumber};
}

QList<FailureInfo> AgenticFailureDetector::detectMultipleFailures(const QString& modelOutput)
{
    QMutexLocker locker(&m_mutex);
    QList<FailureInfo> failures;
    
    if (isRefusal(modelOutput)) {
        failures.append(FailureInfo{AgentFailureType::Refusal, "Refusal", 0.8, "", QDateTime::currentDateTime(), m_sequenceNumber});
    }
    if (isHallucination(modelOutput)) {
        failures.append(FailureInfo{AgentFailureType::Hallucination, "Hallucination", 0.6, "", QDateTime::currentDateTime(), m_sequenceNumber});
    }
    if (isFormatViolation(modelOutput)) {
        failures.append(FailureInfo{AgentFailureType::FormatViolation, "Format issue", 0.7, "", QDateTime::currentDateTime(), m_sequenceNumber});
    }
    if (isInfiniteLoop(modelOutput)) {
        failures.append(FailureInfo{AgentFailureType::InfiniteLoop, "Repetition", 0.85, "", QDateTime::currentDateTime(), m_sequenceNumber});
    }
    if (isSafetyViolation(modelOutput)) {
        failures.append(FailureInfo{AgentFailureType::SafetyViolation, "Safety block", 0.95, "", QDateTime::currentDateTime(), m_sequenceNumber});
    }
    
    m_sequenceNumber++;
    
    if (!failures.isEmpty()) {
        emit multipleFailuresDetected(failures);
    }
    
    return failures;
}

bool AgenticFailureDetector::isRefusal(const QString& output) const
{
    QString lower = output.toLower();
    
    for (const QString& pattern : m_refusalPatterns) {
        if (lower.contains(pattern.toLower())) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isHallucination(const QString& output) const
{
    QString lower = output.toLower();
    int hallucIndicators = 0;
    
    for (const QString& pattern : m_hallucinationPatterns) {
        if (lower.contains(pattern.toLower())) {
            hallucIndicators++;
        }
    }
    
    return hallucIndicators >= 2; // Need multiple indicators
}

bool AgenticFailureDetector::isFormatViolation(const QString& output) const
{
    // Check JSON format
    if (output.trimmed().startsWith('{')) {
        int openBraces = output.count('{');
        int closeBraces = output.count('}');
        if (openBraces != closeBraces) {
            return true;
        }
    }
    
    // Check code block format
    if (output.contains("```")) {
        if ((output.count("```") % 2) != 0) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isInfiniteLoop(const QString& output) const
{
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    if (lines.count() < 5) {
        return false;
    }
    
    // Check for repeated lines
    QHash<QString, int> lineCount;
    for (const QString& line : lines) {
        lineCount[line.trimmed()]++;
    }
    
    for (int count : lineCount.values()) {
        if (count > 3) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isTokenLimitExceeded(const QString& output) const
{
    return output.endsWith("...") || output.endsWith("[truncated]") || 
           output.endsWith("[end of response]") || output.contains("[token limit]");
}

bool AgenticFailureDetector::isResourceExhausted(const QString& output) const
{
    QString lower = output.toLower();
    
    for (const QString& indicator : m_resourceExhaustionIndicators) {
        if (lower.contains(indicator.toLower())) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isTimeout(const QString& output) const
{
    QString lower = output.toLower();
    
    for (const QString& indicator : m_timeoutIndicators) {
        if (lower.contains(indicator.toLower())) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isSafetyViolation(const QString& output) const
{
    for (const QString& pattern : m_safetyPatterns) {
        if (output.contains(pattern)) {
            return true;
        }
    }
    
    return false;
}

void AgenticFailureDetector::setRefusalThreshold(double threshold)
{
    QMutexLocker locker(&m_mutex);
    m_refusalThreshold = threshold;
}

void AgenticFailureDetector::setQualityThreshold(double threshold)
{
    QMutexLocker locker(&m_mutex);
    m_qualityThreshold = threshold;
}

void AgenticFailureDetector::enableToolValidation(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_enableToolValidation = enable;
}

void AgenticFailureDetector::addRefusalPattern(const QString& pattern)
{
    QMutexLocker locker(&m_mutex);
    if (!m_refusalPatterns.contains(pattern)) {
        m_refusalPatterns.append(pattern);
    }
}

void AgenticFailureDetector::addHallucinationPattern(const QString& pattern)
{
    QMutexLocker locker(&m_mutex);
    if (!m_hallucinationPatterns.contains(pattern)) {
        m_hallucinationPatterns.append(pattern);
    }
}

void AgenticFailureDetector::addLoopPattern(const QString& pattern)
{
    QMutexLocker locker(&m_mutex);
    if (!m_loopPatterns.contains(pattern)) {
        m_loopPatterns.append(pattern);
    }
}

void AgenticFailureDetector::addSafetyPattern(const QString& pattern)
{
    QMutexLocker locker(&m_mutex);
    if (!m_safetyPatterns.contains(pattern)) {
        m_safetyPatterns.append(pattern);
    }
}

AgenticFailureDetector::Stats AgenticFailureDetector::getStatistics() const
{
    QMutexLocker locker(&m_mutex);
    return m_stats;
}

void AgenticFailureDetector::resetStatistics()
{
    QMutexLocker locker(&m_mutex);
    m_stats = Stats();
}

void AgenticFailureDetector::setEnabled(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_enabled = enable;
    qInfo() << "[AgenticFailureDetector]" << (enable ? "Enabled" : "Disabled");
}

bool AgenticFailureDetector::isEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled;
}

double AgenticFailureDetector::calculateConfidence(AgentFailureType type, const QString& output)
{
    double confidence = 0.5;
    
    switch (type) {
        case AgentFailureType::Refusal:
            confidence = output.count("cannot") > 0 ? 0.9 : 0.7;
            break;
        case AgentFailureType::Hallucination:
            confidence = 0.6;
            break;
        case AgentFailureType::InfiniteLoop:
            confidence = 0.85;
            break;
        default:
            confidence = 0.7;
            break;
    }
    
    return confidence;
}
