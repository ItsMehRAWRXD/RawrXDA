// agentic_puppeteer.cpp - Implementation of response correction
#include "agentic_puppeteer.hpp"
#include <QRegularExpression>
#include <QJsonDocument>
#include <QDebug>
#include <algorithm>

// Base AgenticPuppeteer Implementation

AgenticPuppeteer::AgenticPuppeteer(QObject* parent)
    : QObject(parent)
{
    // Initialize default refusal patterns
    m_refusalPatterns << "I can't" << "I cannot" << "I'm not able to" 
                     << "I can't assist" << "I'm unable" << "I don't feel comfortable"
                     << "I decline" << "I won't" << "I must refuse";
    
    // Initialize hallucination detection patterns
    m_hallucinationPatterns << "As of my knowledge cutoff" << "I'm not sure but"
                           << "I think" << "probably" << "likely" << "might"
                           << "according to" << "was invented by";
    
    qInfo() << "[AgenticPuppeteer] Initialized with" << m_refusalPatterns.count() 
            << "refusal patterns and" << m_hallucinationPatterns.count() << "hallucination patterns";
}

AgenticPuppeteer::~AgenticPuppeteer()
{
}

CorrectionResult AgenticPuppeteer::correctResponse(const QString& originalResponse, const QString& userPrompt)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enabled || originalResponse.isEmpty()) {
        return CorrectionResult::error(FailureType::None, "Puppeteer disabled or empty response");
    }
    
    m_stats.responsesAnalyzed++;
    
    // Detect failure type
    FailureType failure = detectFailure(originalResponse);
    
    if (failure == FailureType::None) {
        return CorrectionResult::ok(originalResponse, FailureType::None);
    }
    
    m_stats.failuresDetected++;
    m_stats.failureTypeCount[static_cast<int>(failure)]++;
    
    emit failureDetected(failure, diagnoseFailure(originalResponse));
    
    // Apply appropriate correction
    QString corrected;
    
    switch (failure) {
        case FailureType::RefusalResponse:
            corrected = applyRefusalBypass(originalResponse);
            break;
            
        case FailureType::Hallucination:
            corrected = correctHallucination(originalResponse);
            break;
            
        case FailureType::FormatViolation:
            corrected = enforceFormat(originalResponse);
            break;
            
        case FailureType::InfiniteLoop:
            corrected = handleInfiniteLoop(originalResponse);
            break;
            
        default:
            corrected = originalResponse;
            break;
    }
    
    if (corrected != originalResponse && !corrected.isEmpty()) {
        m_stats.successfulCorrections++;
        emit correctionApplied(corrected);
        return CorrectionResult::ok(corrected, failure);
    } else {
        m_stats.failedCorrections++;
        emit correctionFailed(failure, "Could not generate correction");
        return CorrectionResult::error(failure, "Correction generation failed");
    }
}

CorrectionResult AgenticPuppeteer::correctJsonResponse(const QJsonObject& response, const QString& context)
{
    QJsonDocument doc(response);
    QString jsonStr = doc.toJson(QJsonDocument::Compact);
    
    return correctResponse(jsonStr, context);
}

FailureType AgenticPuppeteer::detectFailure(const QString& response)
{
    if (response.isEmpty()) {
        return FailureType::None;
    }
    
    QString lower = response.toLower();
    
    // Check for refusal
    for (const QString& pattern : m_refusalPatterns) {
        if (lower.contains(pattern.toLower())) {
            return FailureType::RefusalResponse;
        }
    }
    
    // Check for hallucination indicators
    for (const QString& pattern : m_hallucinationPatterns) {
        if (lower.contains(pattern.toLower())) {
            return FailureType::Hallucination;
        }
    }
    
    // Check for infinite loops (repeated content)
    QStringList lines = response.split('\n', Qt::SkipEmptyParts);
    if (lines.count() > 5) {
        QHash<QString, int> lineCount;
        for (const QString& line : lines) {
            lineCount[line]++;
        }
        
        for (int count : lineCount.values()) {
            if (count > 3) {
                return FailureType::InfiniteLoop;
            }
        }
    }
    
    // Check for token limit (truncated response)
    if (response.endsWith("...") || response.endsWith("[truncated]")) {
        return FailureType::TokenLimitExceeded;
    }
    
    return FailureType::None;
}

QString AgenticPuppeteer::diagnoseFailure(const QString& response)
{
    switch (detectFailure(response)) {
        case FailureType::RefusalResponse:
            return "Model refused to answer (safety filter triggered)";
        case FailureType::Hallucination:
            return "Model may have generated false information";
        case FailureType::FormatViolation:
            return "Output format doesn't match expected structure";
        case FailureType::InfiniteLoop:
            return "Response contains repeated/looping content";
        case FailureType::TokenLimitExceeded:
            return "Response was truncated (token limit exceeded)";
        default:
            return "No failure detected";
    }
}

void AgenticPuppeteer::addRefusalPattern(const QString& pattern)
{
    QMutexLocker locker(&m_mutex);
    if (!m_refusalPatterns.contains(pattern)) {
        m_refusalPatterns.append(pattern);
    }
}

void AgenticPuppeteer::addHallucinationPattern(const QString& pattern)
{
    QMutexLocker locker(&m_mutex);
    if (!m_hallucinationPatterns.contains(pattern)) {
        m_hallucinationPatterns.append(pattern);
    }
}

void AgenticPuppeteer::addLoopPattern(const QString& pattern)
{
    QMutexLocker locker(&m_mutex);
    if (!m_loopPatterns.contains(pattern)) {
        m_loopPatterns.append(pattern);
    }
}

QStringList AgenticPuppeteer::getRefusalPatterns() const
{
    QMutexLocker locker(&m_mutex);
    return m_refusalPatterns;
}

QStringList AgenticPuppeteer::getHallucinationPatterns() const
{
    QMutexLocker locker(&m_mutex);
    return m_hallucinationPatterns;
}

AgenticPuppeteer::Stats AgenticPuppeteer::getStatistics() const
{
    QMutexLocker locker(&m_mutex);
    return m_stats;
}

void AgenticPuppeteer::resetStatistics()
{
    QMutexLocker locker(&m_mutex);
    m_stats = Stats();
}

void AgenticPuppeteer::setEnabled(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_enabled = enable;
    qInfo() << "[AgenticPuppeteer]" << (enable ? "Enabled" : "Disabled");
}

bool AgenticPuppeteer::isEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled;
}

QString AgenticPuppeteer::applyRefusalBypass(const QString& response)
{
    // Try to extract any partial content or reframe the request
    if (response.contains("however")) {
        return response.mid(response.indexOf("however"));
    }
    
    // Provide a generic bypass attempt
    return "I understand you'd like to know more about this topic. While I have limitations, "
           "I can try to provide general information or suggest alternative approaches.";
}

QString AgenticPuppeteer::correctHallucination(const QString& response)
{
    // Remove hallucination indicators
    QString corrected = response;
    
    for (const QString& pattern : m_hallucinationPatterns) {
        corrected.remove(QRegularExpression(pattern + ".*?\\."));
    }
    
    // Add disclaimer
    if (!corrected.isEmpty()) {
        corrected.prepend("[Note: This response has been filtered for accuracy.]\n\n");
    }
    
    return corrected;
}

QString AgenticPuppeteer::enforceFormat(const QString& response)
{
    // Try to fix common format issues
    QString corrected = response;
    
    // Fix JSON if present
    if (corrected.startsWith('{') && !corrected.endsWith('}')) {
        corrected.append('}');
    }
    
    // Fix markdown code blocks
    if (corrected.contains("```") && (corrected.count("```") % 2) != 0) {
        corrected.append("\n```");
    }
    
    return corrected;
}

QString AgenticPuppeteer::handleInfiniteLoop(const QString& response)
{
    QStringList lines = response.split('\n', Qt::SkipEmptyParts);
    
    if (lines.isEmpty()) {
        return response;
    }
    
    // Remove duplicate consecutive lines
    QStringList unique;
    for (const QString& line : lines) {
        if (unique.isEmpty() || unique.last() != line) {
            unique.append(line);
        }
    }
    
    return unique.join('\n');
}

// RefusalBypassPuppeteer Implementation

RefusalBypassPuppeteer::RefusalBypassPuppeteer(QObject* parent)
    : AgenticPuppeteer(parent)
{
    qInfo() << "[RefusalBypassPuppeteer] Specialized for refusal bypass";
}

CorrectionResult RefusalBypassPuppeteer::bypassRefusal(const QString& refusedResponse, const QString& originalPrompt)
{
    QString reframed = reframePrompt(refusedResponse);
    
    if (!reframed.isEmpty()) {
        return CorrectionResult::ok(reframed, FailureType::RefusalResponse);
    }
    
    return CorrectionResult::error(FailureType::RefusalResponse, "Could not reframe refusal");
}

QString RefusalBypassPuppeteer::reframePrompt(const QString& refusedResponse)
{
    return generateAlternativePrompt(refusedResponse);
}

QString RefusalBypassPuppeteer::generateAlternativePrompt(const QString& original)
{
    // Provide educational/technical framing instead of blocked request
    return "From a technical/educational perspective, could you explain how this topic relates to "
           "your training or knowledge base? What aspects can you discuss?";
}

// HallucinationCorrectorPuppeteer Implementation

HallucinationCorrectorPuppeteer::HallucinationCorrectorPuppeteer(QObject* parent)
    : AgenticPuppeteer(parent)
{
    qInfo() << "[HallucinationCorrectorPuppeteer] Specialized for hallucination detection";
}

CorrectionResult HallucinationCorrectorPuppeteer::detectAndCorrectHallucination(
    const QString& response, const QStringList& knownFacts)
{
    m_knownFactDatabase = knownFacts;
    
    // Check claims against known facts
    QString corrected = response;
    bool foundHallucination = false;
    
    // Very basic hallucination detection
    for (const QString& fact : knownFacts) {
        if (!response.contains(fact, Qt::CaseInsensitive)) {
            foundHallucination = true;
        }
    }
    
    if (foundHallucination) {
        corrected = correctHallucination(response);
        return CorrectionResult::ok(corrected, FailureType::Hallucination);
    }
    
    return CorrectionResult::ok(response, FailureType::None);
}

QString HallucinationCorrectorPuppeteer::validateFactuality(const QString& claim)
{
    for (const QString& fact : m_knownFactDatabase) {
        if (claim.contains(fact, Qt::CaseInsensitive)) {
            return "[Verified] " + claim;
        }
    }
    
    return "[Unverified] " + claim;
}

// FormatEnforcerPuppeteer Implementation

FormatEnforcerPuppeteer::FormatEnforcerPuppeteer(QObject* parent)
    : AgenticPuppeteer(parent)
{
    qInfo() << "[FormatEnforcerPuppeteer] Specialized for format enforcement";
}

CorrectionResult FormatEnforcerPuppeteer::enforceJsonFormat(const QString& response)
{
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    
    if (!doc.isNull()) {
        // Already valid JSON
        return CorrectionResult::ok(response, FailureType::None);
    }
    
    // Try to fix common JSON issues
    QString corrected = response;
    
    // Add missing closing braces
    int braceCount = corrected.count('{') - corrected.count('}');
    for (int i = 0; i < braceCount; ++i) {
        corrected.append('}');
    }
    
    // Verify it's now valid
    QJsonDocument fixedDoc = QJsonDocument::fromJson(corrected.toUtf8());
    if (!fixedDoc.isNull()) {
        return CorrectionResult::ok(corrected, FailureType::FormatViolation);
    }
    
    return CorrectionResult::error(FailureType::FormatViolation, "Could not repair JSON");
}

CorrectionResult FormatEnforcerPuppeteer::enforceMarkdownFormat(const QString& response)
{
    QString corrected = response;
    
    // Fix unmatched markdown code blocks
    if ((corrected.count("```") % 2) != 0) {
        corrected.append("\n```");
    }
    
    // Fix bold/italic markers
    corrected.replace(QRegularExpression("\\*{3}"), "**");
    
    return CorrectionResult::ok(corrected, FailureType::FormatViolation);
}

CorrectionResult FormatEnforcerPuppeteer::enforceCodeBlockFormat(const QString& response)
{
    QString corrected = response;
    
    // Ensure code blocks have language identifier and closing marker
    QRegularExpression codeBlockRegex("```([\\s\\S]*?)```");
    QRegularExpressionMatch match = codeBlockRegex.match(corrected);
    
    if (match.hasMatch() && match.captured(1).trimmed().isEmpty()) {
        corrected.replace("```", "```cpp");
    }
    
    return CorrectionResult::ok(corrected, FailureType::FormatViolation);
}

void FormatEnforcerPuppeteer::setRequiredJsonSchema(const QJsonObject& schema)
{
    QMutexLocker locker(&m_mutex);
    m_requiredSchema = schema;
}

QJsonObject FormatEnforcerPuppeteer::getRequiredJsonSchema() const
{
    QMutexLocker locker(&m_mutex);
    return m_requiredSchema;
}
