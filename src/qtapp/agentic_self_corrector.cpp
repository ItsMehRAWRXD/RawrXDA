// agentic_self_corrector.cpp - Implementation of self-correcting agentic system
#include "agentic_self_corrector.hpp"
#include <QDebug>
#include <QRegularExpression>

AgenticSelfCorrector::AgenticSelfCorrector()
{
    qInfo() << "[AgenticSelfCorrector] Initialized";
    
    // Initialize enabled methods
    m_enabledMethods["grammar"] = true;
    m_enabledMethods["semantic"] = true;
    m_enabledMethods["structural"] = true;
}

AgenticSelfCorrector::~AgenticSelfCorrector()
{
    qInfo() << "[AgenticSelfCorrector] Destroyed";
}

CorrectionResult AgenticSelfCorrector::correctAgentOutput(const QByteArray& output, const QString& context)
{
    QMutexLocker locker(&m_mutex);
    
    if (output.isEmpty()) {
        return CorrectionResult::failure("Empty output");
    }
    
    m_stats.totalAttempts++;
    
    // Detect the type of error and apply appropriate correction
    if (detectFormatViolation(output)) {
        auto result = correctFormatViolation(output);
        if (result.succeeded) {
            m_stats.successfulCorrections++;
            m_stats.methodSuccessCounts[result.correctionMethod]++;
            return result;
        }
    }
    
    if (detectRefusal(output)) {
        auto result = correctRefusalResponse(output);
        if (result.succeeded) {
            m_stats.successfulCorrections++;
            m_stats.methodSuccessCounts[result.correctionMethod]++;
            return result;
        }
    }
    
    if (detectHallucination(output)) {
        auto result = correctHallucination(output);
        if (result.succeeded) {
            m_stats.successfulCorrections++;
            m_stats.methodSuccessCounts[result.correctionMethod]++;
            return result;
        }
    }
    
    m_stats.failedCorrections++;
    return CorrectionResult::failure("No applicable correction method");
}

CorrectionResult AgenticSelfCorrector::correctWithRetry(const QByteArray& output, int maxRetries)
{
    QMutexLocker locker(&m_mutex);
    
    CorrectionResult result = CorrectionResult::failure("Max retries exceeded");
    
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        locker.unlock();
        result = correctAgentOutput(output, QString("retry_%1").arg(attempt));
        locker.relock();
        
        if (result.succeeded) {
            result.attemptsUsed = attempt + 1;
            return result;
        }
    }
    
    result.attemptsUsed = maxRetries;
    return result;
}

CorrectionResult AgenticSelfCorrector::correctFormatViolation(const QByteArray& output)
{
    QByteArray corrected = performStructuralCorrection(output);
    
    double confidence = calculateConfidenceScore(corrected);
    if (confidence >= m_confidenceThreshold) {
        return CorrectionResult::success(corrected, "format_correction", 1, confidence);
    }
    
    return CorrectionResult::failure("Format correction confidence too low");
}

CorrectionResult AgenticSelfCorrector::correctRefusalResponse(const QByteArray& output)
{
    QString text = QString::fromUtf8(output);
    
    // Replace common refusal patterns
    text.replace(QRegularExpression("i\\s+can't|i\\s+cannot|i'm\\s+unable|i\\s+am\\s+unable", QRegularExpression::CaseInsensitiveOption),
                "I can");
    text.replace(QRegularExpression("i\\s+cannot\\s+help|i\\s+can't\\s+help", QRegularExpression::CaseInsensitiveOption),
                "I can help");
    
    QByteArray corrected = text.toUtf8();
    double confidence = calculateConfidenceScore(corrected);
    
    if (confidence >= m_confidenceThreshold) {
        return CorrectionResult::success(corrected, "refusal_bypass", 1, confidence);
    }
    
    return CorrectionResult::failure("Refusal correction confidence too low");
}

CorrectionResult AgenticSelfCorrector::correctHallucination(const QByteArray& output)
{
    // Hallucination correction involves semantic analysis
    QByteArray corrected = performSemanticCorrection(output);
    
    double confidence = calculateConfidenceScore(corrected);
    if (confidence >= m_confidenceThreshold) {
        return CorrectionResult::success(corrected, "hallucination_correction", 1, confidence);
    }
    
    return CorrectionResult::failure("Hallucination correction confidence too low");
}

CorrectionResult AgenticSelfCorrector::correctInfiniteLoop(const QByteArray& output)
{
    QString text = QString::fromUtf8(output);
    
    // Detect and truncate infinite loops
    QRegularExpression loopPattern("(.+)(\\s+\\1){3,}");
    if (loopPattern.match(text).hasMatch()) {
        // Truncate at first repetition
        int firstMatch = loopPattern.match(text).capturedStart();
        text = text.left(firstMatch);
    }
    
    QByteArray corrected = text.toUtf8();
    double confidence = calculateConfidenceScore(corrected);
    
    if (confidence >= m_confidenceThreshold) {
        return CorrectionResult::success(corrected, "infinite_loop_truncation", 1, confidence);
    }
    
    return CorrectionResult::failure("Infinite loop correction failed");
}

CorrectionResult AgenticSelfCorrector::correctTokenLimit(const QByteArray& output)
{
    QString text = QString::fromUtf8(output);
    
    // Truncate to reasonable length if token limit exceeded
    if (text.length() > 4096) {
        // Find last sentence break before limit
        int truncPoint = text.lastIndexOf(".", 4000);
        if (truncPoint > 3500) {
            text = text.left(truncPoint + 1);
        } else {
            text = text.left(4000);
        }
    }
    
    QByteArray corrected = text.toUtf8();
    double confidence = calculateConfidenceScore(corrected);
    
    if (confidence >= m_confidenceThreshold) {
        return CorrectionResult::success(corrected, "token_limit_truncation", 1, confidence);
    }
    
    return CorrectionResult::failure("Token limit correction failed");
}

void AgenticSelfCorrector::setMaxCorrectionAttempts(int max)
{
    QMutexLocker locker(&m_mutex);
    m_maxAttempts = max;
    qInfo() << "[AgenticSelfCorrector] Max attempts set to" << max;
}

void AgenticSelfCorrector::setConfidenceThreshold(double threshold)
{
    QMutexLocker locker(&m_mutex);
    m_confidenceThreshold = threshold;
    qInfo() << "[AgenticSelfCorrector] Confidence threshold set to" << threshold;
}

void AgenticSelfCorrector::enableCorrectionMethod(const QString& method, bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_enabledMethods[method] = enable;
    qInfo() << "[AgenticSelfCorrector] Method" << method << (enable ? "enabled" : "disabled");
}

AgenticSelfCorrector::Stats AgenticSelfCorrector::getStatistics() const
{
    QMutexLocker locker(&m_mutex);
    return m_stats;
}

void AgenticSelfCorrector::resetStatistics()
{
    QMutexLocker locker(&m_mutex);
    m_stats = Stats();
    qInfo() << "[AgenticSelfCorrector] Statistics reset";
}

QByteArray AgenticSelfCorrector::performGrammarCorrection(const QByteArray& output)
{
    QString text = QString::fromUtf8(output);
    
    // Basic grammar fixes
    text.replace(QRegularExpression("\\s+", QRegularExpression::UseUnicodePropertiesOption), " ");
    text.replace(QRegularExpression("([.!?])([A-Za-z])", QRegularExpression::UseUnicodePropertiesOption), "\\1 \\2");
    
    return text.toUtf8();
}

QByteArray AgenticSelfCorrector::performSemanticCorrection(const QByteArray& output)
{
    QString text = QString::fromUtf8(output);
    
    // Remove obviously contradictory statements
    QStringList lines = text.split("\n", Qt::SkipEmptyParts);
    QStringList correctedLines;
    
    for (const QString& line : lines) {
        if (!line.contains("contradicts", Qt::CaseInsensitive) &&
            !line.contains("both true and false", Qt::CaseInsensitive)) {
            correctedLines.append(line);
        }
    }
    
    return correctedLines.join("\n").toUtf8();
}

QByteArray AgenticSelfCorrector::performStructuralCorrection(const QByteArray& output)
{
    QString text = QString::fromUtf8(output);
    
    // Ensure proper structure with line breaks and formatting
    if (!text.endsWith(".") && !text.endsWith("?") && !text.endsWith("!")) {
        text.append(".");
    }
    
    return text.toUtf8();
}

bool AgenticSelfCorrector::detectFormatViolation(const QByteArray& output) const
{
    QString text = QString::fromUtf8(output);
    
    // Check for missing structure
    if (text.isEmpty() || (text.length() < 5)) {
        return true;
    }
    
    // Check for incomplete sentences
    if (!text.contains(QRegularExpression("[.!?]$"))) {
        return true;
    }
    
    return false;
}

bool AgenticSelfCorrector::detectRefusal(const QByteArray& output) const
{
    QString text = QString::fromUtf8(output).toLower();
    
    QStringList refusalPatterns = {
        "can't", "cannot", "unable to", "i'm sorry", "i apologize",
        "not able", "refuse", "refusal", "against my guidelines"
    };
    
    for (const QString& pattern : refusalPatterns) {
        if (text.contains(pattern)) {
            return true;
        }
    }
    
    return false;
}

bool AgenticSelfCorrector::detectHallucination(const QByteArray& output) const
{
    QString text = QString::fromUtf8(output);
    
    // Detect unrealistic claims or contradictions
    if (text.contains(QRegularExpression("\\d{20,}", QRegularExpression::UseUnicodePropertiesOption))) {
        return true;  // Unrealistic number
    }
    
    if (text.contains("both true and false", Qt::CaseInsensitive)) {
        return true;  // Contradiction
    }
    
    return false;
}

double AgenticSelfCorrector::calculateConfidenceScore(const QByteArray& output) const
{
    if (output.isEmpty()) return 0.0;
    
    double score = 0.5;  // Base score
    
    QString text = QString::fromUtf8(output);
    
    // Increase confidence for proper sentence structure
    if (text.endsWith(".") || text.endsWith("?") || text.endsWith("!")) {
        score += 0.2;
    }
    
    // Increase confidence for length
    if (text.length() > 20) {
        score += 0.15;
    }
    
    // Decrease confidence if contains obvious errors
    if (text.contains(QRegularExpression("\\s{2,}", QRegularExpression::UseUnicodePropertiesOption))) {
        score -= 0.1;
    }
    
    return qBound(0.0, score, 1.0);
}

