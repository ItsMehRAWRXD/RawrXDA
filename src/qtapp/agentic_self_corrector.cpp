// agentic_self_corrector.cpp - Implementation of self-correcting agentic system
#include "agentic_self_corrector.hpp"


AgenticSelfCorrector::AgenticSelfCorrector()
{
    
    // Initialize enabled methods
    m_enabledMethods["grammar"] = true;
    m_enabledMethods["semantic"] = true;
    m_enabledMethods["structural"] = true;
}

AgenticSelfCorrector::~AgenticSelfCorrector()
{
}

CorrectionResult AgenticSelfCorrector::correctAgentOutput(const std::vector<uint8_t>& output, const std::string& context)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (output.empty()) {
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

CorrectionResult AgenticSelfCorrector::correctWithRetry(const std::vector<uint8_t>& output, int maxRetries)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    CorrectionResult result = CorrectionResult::failure("Max retries exceeded");
    
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        locker.unlock();
        result = correctAgentOutput(output, std::string("retry_%1"));
        locker.relock();
        
        if (result.succeeded) {
            result.attemptsUsed = attempt + 1;
            return result;
        }
    }
    
    result.attemptsUsed = maxRetries;
    return result;
}

CorrectionResult AgenticSelfCorrector::correctFormatViolation(const std::vector<uint8_t>& output)
{
    std::vector<uint8_t> corrected = performStructuralCorrection(output);
    
    double confidence = calculateConfidenceScore(corrected);
    if (confidence >= m_confidenceThreshold) {
        return CorrectionResult::success(corrected, "format_correction", 1, confidence);
    }
    
    return CorrectionResult::failure("Format correction confidence too low");
}

CorrectionResult AgenticSelfCorrector::correctRefusalResponse(const std::vector<uint8_t>& output)
{
    std::string text = std::string::fromUtf8(output);
    
    // Replace common refusal patterns
    text.replace(std::regex("i\\s+can't|i\\s+cannot|i'm\\s+unable|i\\s+am\\s+unable", std::regex::CaseInsensitiveOption),
                "I can");
    text.replace(std::regex("i\\s+cannot\\s+help|i\\s+can't\\s+help", std::regex::CaseInsensitiveOption),
                "I can help");
    
    std::vector<uint8_t> corrected = text.toUtf8();
    double confidence = calculateConfidenceScore(corrected);
    
    if (confidence >= m_confidenceThreshold) {
        return CorrectionResult::success(corrected, "refusal_bypass", 1, confidence);
    }
    
    return CorrectionResult::failure("Refusal correction confidence too low");
}

CorrectionResult AgenticSelfCorrector::correctHallucination(const std::vector<uint8_t>& output)
{
    // Hallucination correction involves semantic analysis
    std::vector<uint8_t> corrected = performSemanticCorrection(output);
    
    double confidence = calculateConfidenceScore(corrected);
    if (confidence >= m_confidenceThreshold) {
        return CorrectionResult::success(corrected, "hallucination_correction", 1, confidence);
    }
    
    return CorrectionResult::failure("Hallucination correction confidence too low");
}

CorrectionResult AgenticSelfCorrector::correctInfiniteLoop(const std::vector<uint8_t>& output)
{
    std::string text = std::string::fromUtf8(output);
    
    // Detect and truncate infinite loops
    std::regex loopPattern("(.+)(\\s+\\1){3,}");
    if (loopPattern.match(text).hasMatch()) {
        // Truncate at first repetition
        int firstMatch = loopPattern.match(text).capturedStart();
        text = text.left(firstMatch);
    }
    
    std::vector<uint8_t> corrected = text.toUtf8();
    double confidence = calculateConfidenceScore(corrected);
    
    if (confidence >= m_confidenceThreshold) {
        return CorrectionResult::success(corrected, "infinite_loop_truncation", 1, confidence);
    }
    
    return CorrectionResult::failure("Infinite loop correction failed");
}

CorrectionResult AgenticSelfCorrector::correctTokenLimit(const std::vector<uint8_t>& output)
{
    std::string text = std::string::fromUtf8(output);
    
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
    
    std::vector<uint8_t> corrected = text.toUtf8();
    double confidence = calculateConfidenceScore(corrected);
    
    if (confidence >= m_confidenceThreshold) {
        return CorrectionResult::success(corrected, "token_limit_truncation", 1, confidence);
    }
    
    return CorrectionResult::failure("Token limit correction failed");
}

void AgenticSelfCorrector::setMaxCorrectionAttempts(int max)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_maxAttempts = max;
}

void AgenticSelfCorrector::setConfidenceThreshold(double threshold)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_confidenceThreshold = threshold;
}

void AgenticSelfCorrector::enableCorrectionMethod(const std::string& method, bool enable)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enabledMethods[method] = enable;
}

AgenticSelfCorrector::Stats AgenticSelfCorrector::getStatistics() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_stats;
}

void AgenticSelfCorrector::resetStatistics()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_stats = Stats();
}

std::vector<uint8_t> AgenticSelfCorrector::performGrammarCorrection(const std::vector<uint8_t>& output)
{
    std::string text = std::string::fromUtf8(output);
    
    // Basic grammar fixes
    text.replace(std::regex("\\s+", std::regex::UseUnicodePropertiesOption), " ");
    text.replace(std::regex("([.!?])([A-Za-z])", std::regex::UseUnicodePropertiesOption), "\\1 \\2");
    
    return text.toUtf8();
}

std::vector<uint8_t> AgenticSelfCorrector::performSemanticCorrection(const std::vector<uint8_t>& output)
{
    std::string text = std::string::fromUtf8(output);
    
    // Remove obviously contradictory statements
    std::vector<std::string> lines = text.split("\n", //SkipEmptyParts);
    std::vector<std::string> correctedLines;
    
    for (const std::string& line : lines) {
        if (!line.contains("contradicts", //CaseInsensitive) &&
            !line.contains("both true and false", //CaseInsensitive)) {
            correctedLines.append(line);
        }
    }
    
    return correctedLines.join("\n").toUtf8();
}

std::vector<uint8_t> AgenticSelfCorrector::performStructuralCorrection(const std::vector<uint8_t>& output)
{
    std::string text = std::string::fromUtf8(output);
    
    // Ensure proper structure with line breaks and formatting
    if (!text.endsWith(".") && !text.endsWith("?") && !text.endsWith("!")) {
        text.append(".");
    }
    
    return text.toUtf8();
}

bool AgenticSelfCorrector::detectFormatViolation(const std::vector<uint8_t>& output) const
{
    std::string text = std::string::fromUtf8(output);
    
    // Check for missing structure
    if (text.empty() || (text.length() < 5)) {
        return true;
    }
    
    // Check for incomplete sentences
    if (!text.contains(std::regex("[.!?]$"))) {
        return true;
    }
    
    return false;
}

bool AgenticSelfCorrector::detectRefusal(const std::vector<uint8_t>& output) const
{
    std::string text = std::string::fromUtf8(output).toLower();
    
    std::vector<std::string> refusalPatterns = {
        "can't", "cannot", "unable to", "i'm sorry", "i apologize",
        "not able", "refuse", "refusal", "against my guidelines"
    };
    
    for (const std::string& pattern : refusalPatterns) {
        if (text.contains(pattern)) {
            return true;
        }
    }
    
    return false;
}

bool AgenticSelfCorrector::detectHallucination(const std::vector<uint8_t>& output) const
{
    std::string text = std::string::fromUtf8(output);
    
    // Detect unrealistic claims or contradictions
    if (text.contains(std::regex("\\d{20,}", std::regex::UseUnicodePropertiesOption))) {
        return true;  // Unrealistic number
    }
    
    if (text.contains("both true and false", //CaseInsensitive)) {
        return true;  // Contradiction
    }
    
    return false;
}

double AgenticSelfCorrector::calculateConfidenceScore(const std::vector<uint8_t>& output) const
{
    if (output.empty()) return 0.0;
    
    double score = 0.5;  // Base score
    
    std::string text = std::string::fromUtf8(output);
    
    // Increase confidence for proper sentence structure
    if (text.endsWith(".") || text.endsWith("?") || text.endsWith("!")) {
        score += 0.2;
    }
    
    // Increase confidence for length
    if (text.length() > 20) {
        score += 0.15;
    }
    
    // Decrease confidence if contains obvious errors
    if (text.contains(std::regex("\\s{2,}", std::regex::UseUnicodePropertiesOption))) {
        score -= 0.1;
    }
    
    return qBound(0.0, score, 1.0);
}


