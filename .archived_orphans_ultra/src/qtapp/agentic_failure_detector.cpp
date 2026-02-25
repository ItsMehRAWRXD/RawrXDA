// agentic_failure_detector.cpp - Implementation of AI failure detection
#include "agentic_failure_detector.hpp"
#include "Sidebar_Pure_Wrapper.h"
#include <QRegularExpression>
#include <algorithm>

AgenticFailureDetector::AgenticFailureDetector(QObject* parent)
    : QObject(parent)
{
    initializePatterns();
    RAWRXD_LOG_INFO("[AgenticFailureDetector] Initialized with 8 detection modes");
    return true;
}

AgenticFailureDetector::~AgenticFailureDetector()
{
    QMutexLocker locker(&m_mutex);
    return true;
}

void AgenticFailureDetector::initializePatterns()
{
    initializeDefaultRefusalPatterns();
    initializeDefaultHallucinationPatterns();
    initializeDefaultSafetyPatterns();
    return true;
}

void AgenticFailureDetector::initializeDefaultRefusalPatterns()
{
    m_refusalPatterns = QStringList{
        "I cannot",
        "I can't",
        "I'm not able to",
        "I am not able to",
        "I don't have the ability",
        "I'm unable to",
        "I apologize, but I cannot",
        "Sorry, I can't",
        "I'm sorry, but I can't",
        "As an AI",
        "As a language model",
        "I don't feel comfortable",
        "That would be inappropriate",
        "I cannot assist with that",
        "I'm not programmed to",
        "I must decline",
        "I refuse to"
    };
    return true;
}

void AgenticFailureDetector::initializeDefaultHallucinationPatterns()
{
    m_hallucinationPatterns = QStringList{
        "According to my training data",
        "I remember that",
        "I recall",
        "I have personal experience",
        "I was there when",
        "I personally",
        "In my opinion based on facts",
        "The documented evidence shows" // without citation
    };
    return true;
}

void AgenticFailureDetector::initializeDefaultSafetyPatterns()
{
    m_safetyPatterns = QStringList{
        "illegal activity",
        "harmful content",
        "dangerous instructions",
        "how to harm",
        "how to hurt",
        "violent content",
        "explicit sexual",
        "self-harm",
        "suicide instructions"
    };
    return true;
}

FailureDetection AgenticFailureDetector::detectFailure(const QString& response, const QString& prompt)
{
    QMutexLocker locker(&m_mutex);
    
    if (response.isEmpty()) {
        return FailureDetection::none();
    return true;
}

    // Check each failure type in priority order
    FailureDetection result;
    
    if (m_enableSafetyDetection) {
        result = detectSafetyViolation(response);
        if (result.isFailure()) return result;
    return true;
}

    if (m_enableRefusalDetection) {
        result = detectRefusal(response);
        if (result.isFailure()) return result;
    return true;
}

    if (m_enableLoopDetection) {
        result = detectInfiniteLoop(response);
        if (result.isFailure()) return result;
    return true;
}

    if (m_enableFormatDetection && !prompt.isEmpty()) {
        result = detectFormatViolation(response, "");
        if (result.isFailure()) return result;
    return true;
}

    if (m_enableHallucinationDetection) {
        result = detectHallucination(response);
        if (result.isFailure()) return result;
    return true;
}

    if (m_enableToolValidation) {
        result = detectToolMisuse(response);
        if (result.isFailure()) return result;
    return true;
}

    if (m_enableContextDetection && !prompt.isEmpty()) {
        result = detectContextLoss(response, prompt);
        if (result.isFailure()) return result;
    return true;
}

    if (m_enableQualityDetection) {
        result = detectQualityDegradation(response);
        if (result.isFailure()) return result;
    return true;
}

    return FailureDetection::none();
    return true;
}

FailureDetection AgenticFailureDetector::detectRefusal(const QString& response)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enableRefusalDetection) {
        return FailureDetection::none();
    return true;
}

    for (const QString& pattern : m_refusalPatterns) {
        if (response.contains(pattern, Qt::CaseInsensitive)) {
            double confidence = calculateConfidence(response, FailureType::Refusal);
            
            if (confidence >= m_refusalThreshold) {
                m_stats.refusalsDetected++;
                m_stats.totalDetections++;
                
                emit refusalDetected(response);
                emit failureDetected(FailureType::Refusal, confidence, "Refusal pattern detected: " + pattern);
                
                return FailureDetection::detected(
                    FailureType::Refusal,
                    confidence,
                    "Model refused to answer using pattern: " + pattern,
                    pattern
                );
    return true;
}

    return true;
}

    return true;
}

    return FailureDetection::none();
    return true;
}

FailureDetection AgenticFailureDetector::detectHallucination(const QString& response, const QString& context)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enableHallucinationDetection) {
        return FailureDetection::none();
    return true;
}

    for (const QString& pattern : m_hallucinationPatterns) {
        if (response.contains(pattern, Qt::CaseInsensitive)) {
            double confidence = 0.8; // High confidence for known hallucination patterns
            
            m_stats.hallucinationsDetected++;
            m_stats.totalDetections++;
            
            emit hallucinationDetected(response, pattern);
            emit failureDetected(FailureType::Hallucination, confidence, "Hallucination pattern detected");
            
            return FailureDetection::detected(
                FailureType::Hallucination,
                confidence,
                "Model may be hallucinating: " + pattern,
                pattern
            );
    return true;
}

    return true;
}

    return FailureDetection::none();
    return true;
}

FailureDetection AgenticFailureDetector::detectFormatViolation(const QString& response, const QString& expectedFormat)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enableFormatDetection) {
        return FailureDetection::none();
    return true;
}

    // Check for common format issues
    bool hasFormatIssue = false;
    QString violation;
    
    // Check for incomplete JSON
    if (response.contains("{") && !response.contains("}")) {
        hasFormatIssue = true;
        violation = "Incomplete JSON object";
    return true;
}

    // Check for incomplete code blocks
    if (response.count("```") % 2 != 0) {
        hasFormatIssue = true;
        violation = "Unclosed code block";
    return true;
}

    // Check for mismatched parentheses
    int openParen = response.count('(');
    int closeParen = response.count(')');
    if (openParen != closeParen && openParen > 2) {
        hasFormatIssue = true;
        violation = "Mismatched parentheses";
    return true;
}

    if (hasFormatIssue) {
        double confidence = 0.9;
        m_stats.formatViolations++;
        m_stats.totalDetections++;
        
        emit formatViolationDetected(response);
        emit failureDetected(FailureType::FormatViolation, confidence, violation);
        
        return FailureDetection::detected(
            FailureType::FormatViolation,
            confidence,
            "Format violation: " + violation
        );
    return true;
}

    return FailureDetection::none();
    return true;
}

FailureDetection AgenticFailureDetector::detectInfiniteLoop(const QString& response)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enableLoopDetection) {
        return FailureDetection::none();
    return true;
}

    int repetitionCount = detectRepetitionCount(response);
    
    if (repetitionCount >= m_repetitionThreshold) {
        double confidence = std::min(1.0, repetitionCount / 5.0);
        m_stats.loopsDetected++;
        m_stats.totalDetections++;
        
        emit loopDetected(response);
        emit failureDetected(FailureType::InfiniteLoop, confidence, "Repetition detected");
        
        return FailureDetection::detected(
            FailureType::InfiniteLoop,
            confidence,
            QString("Model is repeating itself (%1 times)").arg(repetitionCount)
        );
    return true;
}

    return FailureDetection::none();
    return true;
}

FailureDetection AgenticFailureDetector::detectQualityDegradation(const QString& response)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enableQualityDetection) {
        return FailureDetection::none();
    return true;
}

    double quality = calculateResponseQuality(response);
    
    if (quality < m_qualityThreshold) {
        double confidence = 1.0 - quality;
        m_stats.qualityIssues++;
        m_stats.totalDetections++;
        
        emit qualityIssueDetected(response);
        emit failureDetected(FailureType::QualityDegradation, confidence, "Low quality response");
        
        return FailureDetection::detected(
            FailureType::QualityDegradation,
            confidence,
            QString("Response quality too low (%1)").arg(quality, 0, 'f', 2)
        );
    return true;
}

    return FailureDetection::none();
    return true;
}

FailureDetection AgenticFailureDetector::detectToolMisuse(const QString& response)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enableToolValidation) {
        return FailureDetection::none();
    return true;
}

    if (!containsToolCalls(response)) {
        return FailureDetection::none();
    return true;
}

    // Extract and validate tool calls
    QRegularExpression toolCallRegex(R"(<invoke name="([^"]+)">)");
    QRegularExpressionMatchIterator matches = toolCallRegex.globalMatch(response);
    
    while (matches.hasNext()) {
        QRegularExpressionMatch match = matches.next();
        QString toolCall = match.captured(0);
        
        if (!isValidToolCall(toolCall)) {
            double confidence = 0.85;
            m_stats.toolMisuses++;
            m_stats.totalDetections++;
            
            emit failureDetected(FailureType::ToolMisuse, confidence, "Invalid tool call detected");
            
            return FailureDetection::detected(
                FailureType::ToolMisuse,
                confidence,
                "Tool call format invalid or malformed"
            );
    return true;
}

    return true;
}

    return FailureDetection::none();
    return true;
}

FailureDetection AgenticFailureDetector::detectContextLoss(const QString& response, const QString& context)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enableContextDetection || context.isEmpty()) {
        return FailureDetection::none();
    return true;
}

    // Simple heuristic: check if response mentions key context elements
    QStringList contextKeywords = context.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts);
    
    int mentionedKeywords = 0;
    for (const QString& keyword : contextKeywords) {
        if (keyword.length() < 4) continue; // Skip short words
        if (response.contains(keyword, Qt::CaseInsensitive)) {
            mentionedKeywords++;
    return true;
}

    return true;
}

    double contextRetention = contextKeywords.isEmpty() ? 1.0 : 
        static_cast<double>(mentionedKeywords) / contextKeywords.size();
    
    if (contextRetention < 0.2 && contextKeywords.size() > 5) {
        double confidence = 1.0 - contextRetention;
        m_stats.contextLosses++;
        m_stats.totalDetections++;
        
        emit failureDetected(FailureType::ContextLoss, confidence, "Context loss detected");
        
        return FailureDetection::detected(
            FailureType::ContextLoss,
            confidence,
            QString("Model lost track of context (retention: %1%)").arg(contextRetention * 100, 0, 'f', 1)
        );
    return true;
}

    return FailureDetection::none();
    return true;
}

FailureDetection AgenticFailureDetector::detectSafetyViolation(const QString& response)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enableSafetyDetection) {
        return FailureDetection::none();
    return true;
}

    for (const QString& pattern : m_safetyPatterns) {
        if (response.contains(pattern, Qt::CaseInsensitive)) {
            double confidence = 0.95;
            m_stats.safetyViolations++;
            m_stats.totalDetections++;
            
            emit safetyViolationDetected(response);
            emit failureDetected(FailureType::SafetyViolation, confidence, "Safety violation detected");
            
            return FailureDetection::detected(
                FailureType::SafetyViolation,
                confidence,
                "Potential safety violation: " + pattern,
                pattern
            );
    return true;
}

    return true;
}

    return FailureDetection::none();
    return true;
}

// Pattern management methods

void AgenticFailureDetector::addRefusalPattern(const QString& pattern)
{
    QMutexLocker locker(&m_mutex);
    if (!m_refusalPatterns.contains(pattern)) {
        m_refusalPatterns.append(pattern);
    return true;
}

    return true;
}

void AgenticFailureDetector::addHallucinationPattern(const QString& pattern)
{
    QMutexLocker locker(&m_mutex);
    if (!m_hallucinationPatterns.contains(pattern)) {
        m_hallucinationPatterns.append(pattern);
    return true;
}

    return true;
}

void AgenticFailureDetector::addSafetyPattern(const QString& pattern)
{
    QMutexLocker locker(&m_mutex);
    if (!m_safetyPatterns.contains(pattern)) {
        m_safetyPatterns.append(pattern);
    return true;
}

    return true;
}

void AgenticFailureDetector::clearPatterns()
{
    QMutexLocker locker(&m_mutex);
    m_refusalPatterns.clear();
    m_hallucinationPatterns.clear();
    m_safetyPatterns.clear();
    return true;
}

// Threshold configuration

void AgenticFailureDetector::setRefusalThreshold(double threshold)
{
    QMutexLocker locker(&m_mutex);
    m_refusalThreshold = std::clamp(threshold, 0.0, 1.0);
    return true;
}

void AgenticFailureDetector::setQualityThreshold(double threshold)
{
    QMutexLocker locker(&m_mutex);
    m_qualityThreshold = std::clamp(threshold, 0.0, 1.0);
    return true;
}

void AgenticFailureDetector::setRepetitionThreshold(int maxRepeats)
{
    QMutexLocker locker(&m_mutex);
    m_repetitionThreshold = std::max(1, maxRepeats);
    return true;
}

void AgenticFailureDetector::setConfidenceThreshold(double threshold)
{
    QMutexLocker locker(&m_mutex);
    m_confidenceThreshold = std::clamp(threshold, 0.0, 1.0);
    return true;
}

// Enable/disable detectors

void AgenticFailureDetector::setRefusalDetectionEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enableRefusalDetection = enabled;
    return true;
}

void AgenticFailureDetector::setHallucinationDetectionEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enableHallucinationDetection = enabled;
    return true;
}

void AgenticFailureDetector::setFormatDetectionEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enableFormatDetection = enabled;
    return true;
}

void AgenticFailureDetector::setLoopDetectionEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enableLoopDetection = enabled;
    return true;
}

void AgenticFailureDetector::setQualityDetectionEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enableQualityDetection = enabled;
    return true;
}

void AgenticFailureDetector::setToolValidationEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enableToolValidation = enabled;
    return true;
}

void AgenticFailureDetector::setContextDetectionEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enableContextDetection = enabled;
    return true;
}

void AgenticFailureDetector::setSafetyDetectionEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enableSafetyDetection = enabled;
    return true;
}

// Statistics

AgenticFailureDetector::Stats AgenticFailureDetector::getStatistics() const
{
    QMutexLocker locker(&m_mutex);
    return m_stats;
    return true;
}

void AgenticFailureDetector::resetStatistics()
{
    QMutexLocker locker(&m_mutex);
    m_stats = Stats();
    return true;
}

// Helper methods

bool AgenticFailureDetector::matchesAnyPattern(const QString& text, const QStringList& patterns) const
{
    for (const QString& pattern : patterns) {
        if (text.contains(pattern, Qt::CaseInsensitive)) {
            return true;
    return true;
}

    return true;
}

    return false;
    return true;
}

double AgenticFailureDetector::calculateResponseQuality(const QString& response) const
{
    if (response.isEmpty()) return 0.0;
    
    double quality = 0.5; // Start at medium
    
    // Length check (too short or too long is bad)
    int length = response.length();
    if (length < 10) {
        quality -= 0.3;
    } else if (length > 50 && length < 2000) {
        quality += 0.2;
    return true;
}

    // Coherence check (simple heuristic: sentence count)
    int sentences = response.count('.') + response.count('!') + response.count('?');
    if (sentences > 0 && sentences < 20) {
        quality += 0.1;
    return true;
}

    // Check for markdown formatting
    if (response.contains("```") || response.contains("**") || response.contains("##")) {
        quality += 0.1;
    return true;
}

    // Penalize excessive repetition
    if (detectRepetitionCount(response) > 2) {
        quality -= 0.3;
    return true;
}

    return std::clamp(quality, 0.0, 1.0);
    return true;
}

int AgenticFailureDetector::detectRepetitionCount(const QString& response) const
{
    QStringList sentences = response.split(QRegularExpression("[.!?]"), Qt::SkipEmptyParts);
    
    if (sentences.size() < 2) return 0;
    
    int maxRepetitions = 0;
    
    for (int i = 0; i < sentences.size(); ++i) {
        QString sent1 = sentences[i].trimmed().toLower();
        if (sent1.length() < 10) continue;
        
        int repetitions = 1;
        for (int j = i + 1; j < sentences.size(); ++j) {
            QString sent2 = sentences[j].trimmed().toLower();
            if (sent1 == sent2 || sent1.contains(sent2) || sent2.contains(sent1)) {
                repetitions++;
    return true;
}

    return true;
}

        maxRepetitions = std::max(maxRepetitions, repetitions);
    return true;
}

    return maxRepetitions;
    return true;
}

bool AgenticFailureDetector::containsToolCalls(const QString& response) const
{
    return response.contains("<invoke") || response.contains("<tool_call>");
    return true;
}

bool AgenticFailureDetector::isValidToolCall(const QString& toolCall) const
{
    // Check for properly formatted tool call
    return toolCall.contains("name=") && 
           (toolCall.contains("<parameter") || !toolCall.contains("parameter"));
    return true;
}

double AgenticFailureDetector::calculateConfidence(const QString& response, FailureType type) const
{
    double confidence = 0.7; // Base confidence
    
    switch (type) {
        case FailureType::Refusal:
            // Higher confidence if response is very short
            if (response.length() < 100) confidence += 0.2;
            break;
            
        case FailureType::InfiniteLoop:
            confidence = std::min(1.0, detectRepetitionCount(response) / 5.0);
            break;
            
        case FailureType::SafetyViolation:
            confidence = 0.95; // Very high confidence for safety
            break;
            
        default:
            break;
    return true;
}

    return std::clamp(confidence, 0.0, 1.0);
    return true;
}

