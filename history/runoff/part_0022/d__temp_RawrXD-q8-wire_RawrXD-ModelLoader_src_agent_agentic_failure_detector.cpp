#include "agentic_failure_detector.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <vector>

namespace {

inline std::string toLower(std::string_view input) {
    std::string lower;
    lower.reserve(input.size());
    std::transform(input.begin(), input.end(), std::back_inserter(lower), [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    return lower;
}

inline bool containsInsensitive(std::string_view text, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    const auto lowerText = toLower(text);
    const auto lowerNeedle = toLower(needle);
    return lowerText.find(lowerNeedle) != std::string::npos;
}

inline bool endsWith(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() &&
           text.substr(text.size() - suffix.size()) == suffix;
}

inline std::string trim(std::string_view input) {
    const auto isSpace = [](char c) { return std::isspace(static_cast<unsigned char>(c)); };
    auto begin = input.begin();
    auto end = input.end();

    while (begin != end && isSpace(*begin)) {
        ++begin;
    }

    while (end != begin && isSpace(*(end - 1))) {
        --end;
    }

    return std::string(begin, end);
}

inline std::vector<std::string> splitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::string current;
    std::istringstream stream(std::string{text});
    while (std::getline(stream, current)) {
        std::string trimmed = trim(current);
        if (!trimmed.empty()) {
            lines.push_back(trimmed);
        }
    }
    return lines;
}

inline std::int64_t countChar(std::string_view text, char needle) {
    return static_cast<std::int64_t>(std::count(text.begin(), text.end(), needle));
}

inline auto now() {
    return std::chrono::system_clock::now();
}

} // namespace

AgenticFailureDetector::AgenticFailureDetector() {
    initializePatterns();
    std::clog << "[AgenticFailureDetector] Initialized with pattern library" << std::endl;
}

AgenticFailureDetector::~AgenticFailureDetector() = default;

void AgenticFailureDetector::initializePatterns() {
    std::lock_guard<std::mutex> locker(m_mutex);

    m_refusalPatterns = {
        "I can't", "I cannot", "I'm not able to",
        "I can't assist", "I'm unable", "I don't feel comfortable",
        "I decline", "I won't", "I must refuse",
        "I'm not permitted", "against my values",
        "not allowed to", "I apologize, but I cannot"
    };

    m_hallucinationPatterns = {
        "I think", "probably", "likely",
        "I might have", "I'm not sure but",
        "As far as I know", "To my knowledge",
        "might be", "could be", "seems like"
    };

    m_loopPatterns = {"same", "repeated", "over and over", "again and again", "same thing"};

    m_safetyPatterns = {"[SENSITIVE]", "[REDACTED]", "[FILTERED]", "[BLOCKED]", "[SAFETY]", "[WARNING]"};

    m_timeoutIndicators = {"[Timeout]", "[TIMEOUT]", "timed out", "inference timeout", "deadline exceeded"};
    m_resourceExhaustionIndicators = {
        "out of memory", "OOM", "[OOM]", "resource exhausted", "no GPU memory", "device out of memory"
    };
}

FailureInfo AgenticFailureDetector::detectFailure(const std::string& modelOutput, const std::string&) {
    std::lock_guard<std::mutex> locker(m_mutex);
    const auto timestamp = now();

    if (!m_enabled) {
        return FailureInfo{AgentFailureType::None, "Detector disabled", 0.0, {}, timestamp, m_sequenceNumber};
    }

    if (modelOutput.empty()) {
        return FailureInfo{AgentFailureType::Refusal, "Empty output", 0.5, "No response", timestamp, m_sequenceNumber};
    }

    m_stats.totalOutputsAnalyzed++;

    auto report = [&](AgentFailureType type, const std::string& description, const std::string& evidence, double confidence) {
        m_stats.failureTypeCounts[static_cast<int>(type)]++;
        return FailureInfo{type, description, confidence, evidence, timestamp, m_sequenceNumber++};
    };

    if (isRefusal(modelOutput)) {
        return report(AgentFailureType::Refusal, "Model refusal detected", "Contains refusal keywords", calculateConfidence(AgentFailureType::Refusal, modelOutput));
    }

    if (isSafetyViolation(modelOutput)) {
        return report(AgentFailureType::SafetyViolation, "Safety filter triggered", "Contains safety markers", calculateConfidence(AgentFailureType::SafetyViolation, modelOutput));
    }

    if (isTokenLimitExceeded(modelOutput)) {
        return report(AgentFailureType::TokenLimitExceeded, "Token limit exceeded", "Response truncated", 0.9);
    }

    if (isTimeout(modelOutput)) {
        return report(AgentFailureType::Timeout, "Inference timeout", "Timeout indicator detected", 0.95);
    }

    if (isResourceExhausted(modelOutput)) {
        return report(AgentFailureType::ResourceExhausted, "Resource exhaustion", "Out of memory", 0.95);
    }

    if (isInfiniteLoop(modelOutput)) {
        return report(AgentFailureType::InfiniteLoop, "Infinite loop detected", "Repeating content pattern", calculateConfidence(AgentFailureType::InfiniteLoop, modelOutput));
    }

    if (isFormatViolation(modelOutput)) {
        return report(AgentFailureType::FormatViolation, "Format violation detected", "Output format incorrect", calculateConfidence(AgentFailureType::FormatViolation, modelOutput));
    }

    if (isHallucination(modelOutput)) {
        return report(AgentFailureType::Hallucination, "Hallucination indicators", "Uncertain language", calculateConfidence(AgentFailureType::Hallucination, modelOutput));
    }

    return FailureInfo{AgentFailureType::None, "No failure detected", 1.0, {}, timestamp, m_sequenceNumber};
}

std::vector<FailureInfo> AgenticFailureDetector::detectMultipleFailures(const std::string& modelOutput) {
    std::lock_guard<std::mutex> locker(m_mutex);
    std::vector<FailureInfo> failures;
    const auto timestamp = now();

    auto push = [&](AgentFailureType type, const std::string& description, double confidence) {
        failures.push_back({type, description, confidence, {}, timestamp, m_sequenceNumber});
    };

    if (isRefusal(modelOutput)) {
        push(AgentFailureType::Refusal, "Refusal", 0.8);
    }
    if (isHallucination(modelOutput)) {
        push(AgentFailureType::Hallucination, "Hallucination", 0.6);
    }
    if (isFormatViolation(modelOutput)) {
        push(AgentFailureType::FormatViolation, "Format issue", 0.7);
    }
    if (isInfiniteLoop(modelOutput)) {
        push(AgentFailureType::InfiniteLoop, "Repetition", 0.85);
    }
    if (isSafetyViolation(modelOutput)) {
        push(AgentFailureType::SafetyViolation, "Safety block", 0.95);
    }

    if (!failures.empty()) {
        m_sequenceNumber += static_cast<std::int64_t>(failures.size());
    }

    return failures;
}

bool AgenticFailureDetector::isRefusal(const std::string& output) const {
    const auto lower = toLower(output);
    for (const auto& pattern : m_refusalPatterns) {
        if (containsInsensitive(lower, pattern)) {
            return true;
        }
    }
    return false;
}

bool AgenticFailureDetector::isHallucination(const std::string& output) const {
    const auto lower = toLower(output);
    int indicators = 0;
    for (const auto& pattern : m_hallucinationPatterns) {
        if (containsInsensitive(lower, pattern)) {
            indicators++;
        }
    }
    return indicators >= 2;
}

bool AgenticFailureDetector::isFormatViolation(const std::string& output) const {
    const auto trimmed = trim(output);
    if (!trimmed.empty() && trimmed.front() == '{') {
        if (countChar(output, '{') != countChar(output, '}')) {
            return true;
        }
    }

    int tripleTicks = 0;
    for (std::size_t pos = 0; pos + 2 < output.size(); ++pos) {
        if (output[pos] == '`' && output[pos + 1] == '`' && output[pos + 2] == '`') {
            ++tripleTicks;
        }
    }
    if (tripleTicks % 2 != 0) {
        return true;
    }

    return false;
}

bool AgenticFailureDetector::isInfiniteLoop(const std::string& output) const {
    auto lines = splitLines(output);
    if (static_cast<int>(lines.size()) < 5) {
        return false;
    }

    std::unordered_map<std::string, int> lineCount;
    for (auto& line : lines) {
        ++lineCount[line];
    }

    for (const auto& entry : lineCount) {
        if (entry.second > 3) {
            return true;
        }
    }

    return false;
}

bool AgenticFailureDetector::isTokenLimitExceeded(const std::string& output) const {
    return endsWith(output, "...") || endsWith(output, "[truncated]") ||
           endsWith(output, "[end of response]") || containsInsensitive(output, "[token limit]");
}

bool AgenticFailureDetector::isResourceExhausted(const std::string& output) const {
    for (const auto& indicator : m_resourceExhaustionIndicators) {
        if (containsInsensitive(output, indicator)) {
            return true;
        }
    }
    return false;
}

bool AgenticFailureDetector::isTimeout(const std::string& output) const {
    for (const auto& indicator : m_timeoutIndicators) {
        if (containsInsensitive(output, indicator)) {
            return true;
        }
    }
    return false;
}

bool AgenticFailureDetector::isSafetyViolation(const std::string& output) const {
    for (const auto& pattern : m_safetyPatterns) {
        if (output.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void AgenticFailureDetector::setRefusalThreshold(double threshold) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_refusalThreshold = threshold;
}

void AgenticFailureDetector::setQualityThreshold(double threshold) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_qualityThreshold = threshold;
}

void AgenticFailureDetector::enableToolValidation(bool enable) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_enableToolValidation = enable;
}

void AgenticFailureDetector::addRefusalPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> locker(m_mutex);
    if (std::find(m_refusalPatterns.begin(), m_refusalPatterns.end(), pattern) == m_refusalPatterns.end()) {
        m_refusalPatterns.push_back(pattern);
    }
}

void AgenticFailureDetector::addHallucinationPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> locker(m_mutex);
    if (std::find(m_hallucinationPatterns.begin(), m_hallucinationPatterns.end(), pattern) == m_hallucinationPatterns.end()) {
        m_hallucinationPatterns.push_back(pattern);
    }
}

void AgenticFailureDetector::addLoopPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> locker(m_mutex);
    if (std::find(m_loopPatterns.begin(), m_loopPatterns.end(), pattern) == m_loopPatterns.end()) {
        m_loopPatterns.push_back(pattern);
    }
}

void AgenticFailureDetector::addSafetyPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> locker(m_mutex);
    if (std::find(m_safetyPatterns.begin(), m_safetyPatterns.end(), pattern) == m_safetyPatterns.end()) {
        m_safetyPatterns.push_back(pattern);
    }
}

AgenticFailureDetector::Stats AgenticFailureDetector::getStatistics() const {
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_stats;
}

void AgenticFailureDetector::resetStatistics() {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_stats = Stats();
}

void AgenticFailureDetector::setEnabled(bool enable) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_enabled = enable;
    std::clog << "[AgenticFailureDetector] " << (enable ? "Enabled" : "Disabled") << std::endl;
}

bool AgenticFailureDetector::isEnabled() const {
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_enabled;
}

double AgenticFailureDetector::calculateConfidence(AgentFailureType type, const std::string& output) {
    double confidence = 0.5;
    switch (type) {
        case AgentFailureType::Refusal:
            confidence = containsInsensitive(output, "cannot") ? 0.9 : 0.7;
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
