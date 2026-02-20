// agentic_failure_detector.cpp - Implementation of failure detection
#include "agentic_failure_detector.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Helper functions replacing Qt string methods
// ---------------------------------------------------------------------------
static std::string str_to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

static bool str_contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

static bool str_contains_ci(const std::string& haystack, const std::string& needle) {
    return str_to_lower(haystack).find(str_to_lower(needle)) != std::string::npos;
}

static std::string str_trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> str_split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::string current;
    for (char c : s) {
        if (c == delim) {
            if (!current.empty()) result.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) result.push_back(current);
    return result;
}

static int str_count_char(const std::string& s, char c) {
    return static_cast<int>(std::count(s.begin(), s.end(), c));
}

static int str_count_substr(const std::string& s, const std::string& sub) {
    if (sub.empty()) return 0;
    int count = 0;
    size_t pos = 0;
    while ((pos = s.find(sub, pos)) != std::string::npos) {
        ++count;
        pos += sub.size();
    }
    return count;
}

AgenticFailureDetector::AgenticFailureDetector()
    {
    initializePatterns();
    fprintf(stderr, "[INFO] [AgenticFailureDetector] Initialized with pattern library\n");
}

void AgenticFailureDetector::initializePatterns()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Refusal patterns
    m_refusalPatterns = {"I can't", "I cannot", "I'm not able to",
                         "I can't assist", "I'm unable", "I don't feel comfortable",
                         "I decline", "I won't", "I must refuse",
                         "I'm not permitted", "against my values",
                         "not allowed to", "I apologize, but I cannot"};
    
    // Hallucination patterns
    m_hallucinationPatterns = {"I think", "probably", "likely",
                               "I might have", "I'm not sure but",
                               "As far as I know", "To my knowledge",
                               "might be", "could be", "seems like"};
    
    // Infinite loop patterns
    m_loopPatterns = {"same", "repeated", "over and over",
                      "again and again", "same thing"};
    
    // Safety patterns
    m_safetyPatterns = {"[SENSITIVE]", "[REDACTED]", "[FILTERED]",
                        "[BLOCKED]", "[SAFETY]", "[WARNING]"};
    
    // Timeout indicators
    m_timeoutIndicators = {"[Timeout]", "[TIMEOUT]", "timed out",
                           "inference timeout", "deadline exceeded"};
    
    // Resource exhaustion indicators
    m_resourceExhaustionIndicators = {"out of memory", "OOM", "[OOM]",
                                      "resource exhausted", "no GPU memory",
                                      "device out of memory"};
}

FailureInfo AgenticFailureDetector::detectFailure(const std::string& modelOutput, const std::string& context)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_enabled) {
        return FailureInfo{AgentFailureType::None, "Detector disabled", 0.0, "", std::chrono::system_clock::now(), m_sequenceNumber};
    }
    
    if (modelOutput.empty()) {
        return FailureInfo{AgentFailureType::Refusal, "Empty output", 0.5, "No response generated", std::chrono::system_clock::now(), m_sequenceNumber};
    }
    
    m_stats.totalOutputsAnalyzed++;
    
    // Check each failure type in priority order
    if (isRefusal(modelOutput)) {
        FailureInfo info{AgentFailureType::Refusal, "Model refusal detected", 
                        calculateConfidence(AgentFailureType::Refusal, modelOutput),
                        "Contains refusal keywords", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Refusal)]++;
        return info;
    }
    
    if (isSafetyViolation(modelOutput)) {
        FailureInfo info{AgentFailureType::SafetyViolation, "Safety filter triggered",
                        calculateConfidence(AgentFailureType::SafetyViolation, modelOutput),
                        "Contains safety markers", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::SafetyViolation)]++;
        return info;
    }
    
    if (isTokenLimitExceeded(modelOutput)) {
        FailureInfo info{AgentFailureType::TokenLimitExceeded, "Token limit exceeded",
                        0.9, "Response truncated or incomplete", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::TokenLimitExceeded)]++;
        return info;
    }
    
    if (isTimeout(modelOutput)) {
        FailureInfo info{AgentFailureType::Timeout, "Inference timeout",
                        0.95, "Timeout indicator detected", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Timeout)]++;
        return info;
    }
    
    if (isResourceExhausted(modelOutput)) {
        FailureInfo info{AgentFailureType::ResourceExhausted, "Resource exhaustion",
                        0.95, "Out of memory or compute resources", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::ResourceExhausted)]++;
        return info;
    }
    
    if (isInfiniteLoop(modelOutput)) {
        FailureInfo info{AgentFailureType::InfiniteLoop, "Infinite loop detected",
                        calculateConfidence(AgentFailureType::InfiniteLoop, modelOutput),
                        "Repeating content pattern", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::InfiniteLoop)]++;
        return info;
    }
    
    if (isFormatViolation(modelOutput)) {
        FailureInfo info{AgentFailureType::FormatViolation, "Format violation detected",
                        calculateConfidence(AgentFailureType::FormatViolation, modelOutput),
                        "Output format incorrect", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::FormatViolation)]++;
        return info;
    }
    
    if (isHallucination(modelOutput)) {
        FailureInfo info{AgentFailureType::Hallucination, "Hallucination indicators",
                        calculateConfidence(AgentFailureType::Hallucination, modelOutput),
                        "Contains uncertain language patterns", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Hallucination)]++;
        return info;
    }
    
    // No failure detected
    return FailureInfo{AgentFailureType::None, "No failure detected", 1.0, "", std::chrono::system_clock::now(), m_sequenceNumber};
}

std::vector<FailureInfo> AgenticFailureDetector::detectMultipleFailures(const std::string& modelOutput)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<FailureInfo> failures;
    
    if (isRefusal(modelOutput)) {
        failures.push_back(FailureInfo{AgentFailureType::Refusal, "Refusal", 0.8, "", std::chrono::system_clock::now(), m_sequenceNumber});
    }
    if (isHallucination(modelOutput)) {
        failures.push_back(FailureInfo{AgentFailureType::Hallucination, "Hallucination", 0.6, "", std::chrono::system_clock::now(), m_sequenceNumber});
    }
    if (isFormatViolation(modelOutput)) {
        failures.push_back(FailureInfo{AgentFailureType::FormatViolation, "Format issue", 0.7, "", std::chrono::system_clock::now(), m_sequenceNumber});
    }
    if (isInfiniteLoop(modelOutput)) {
        failures.push_back(FailureInfo{AgentFailureType::InfiniteLoop, "Repetition", 0.85, "", std::chrono::system_clock::now(), m_sequenceNumber});
    }
    if (isSafetyViolation(modelOutput)) {
        failures.push_back(FailureInfo{AgentFailureType::SafetyViolation, "Safety block", 0.95, "", std::chrono::system_clock::now(), m_sequenceNumber});
    }
    
    m_sequenceNumber++;
    
    if (!failures.empty()) {
        if (onMultipleFailuresDetected) onMultipleFailuresDetected(failures.data(), failures.size(), callbackContext);
    }
    
    return failures;
}

bool AgenticFailureDetector::isRefusal(const std::string& output) const
{
    std::string lower = str_to_lower(output);
    
    for (const std::string& pattern : m_refusalPatterns) {
        if (str_contains(lower, str_to_lower(pattern))) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isHallucination(const std::string& output) const
{
    std::string lower = str_to_lower(output);
    int hallucIndicators = 0;
    
    for (const std::string& pattern : m_hallucinationPatterns) {
        if (str_contains(lower, str_to_lower(pattern))) {
            hallucIndicators++;
        }
    }
    
    return hallucIndicators >= 2; // Need multiple indicators
}

bool AgenticFailureDetector::isFormatViolation(const std::string& output) const
{
    // Check JSON format
    if (str_trim(output).starts_with('{')) {
        int openBraces = str_count_char(output, '{');
        int closeBraces = str_count_char(output, '}');
        if (openBraces != closeBraces) {
            return true;
        }
    }
    
    // Check code block format
    if (str_contains(output, "```")) {
        if ((str_count_substr(output, "```") % 2) != 0) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isInfiniteLoop(const std::string& output) const
{
    std::vector<std::string> lines = str_split(output, '\n');
    
    if (lines.size() < 5) {
        return false;
    }
    
    // Check for repeated lines
    std::unordered_map<std::string, int> lineCount;
    for (const std::string& line : lines) {
        lineCount[str_trim(line)]++;
    }
    
    for (const auto& [key, count] : lineCount) {
        if (count > 3) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isTokenLimitExceeded(const std::string& output) const
{
    return output.ends_with("...") || output.ends_with("[truncated]") || 
           output.ends_with("[end of response]") || str_contains(output, "[token limit]");
}

bool AgenticFailureDetector::isResourceExhausted(const std::string& output) const
{
    std::string lower = str_to_lower(output);
    
    for (const std::string& indicator : m_resourceExhaustionIndicators) {
        if (str_contains(lower, str_to_lower(indicator))) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isTimeout(const std::string& output) const
{
    std::string lower = str_to_lower(output);
    
    for (const std::string& indicator : m_timeoutIndicators) {
        if (str_contains(lower, str_to_lower(indicator))) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isSafetyViolation(const std::string& output) const
{
    for (const std::string& pattern : m_safetyPatterns) {
        if (str_contains(output, pattern)) {
            return true;
        }
    }
    
    return false;
}

void AgenticFailureDetector::setRefusalThreshold(double threshold)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refusalThreshold = threshold;
}

void AgenticFailureDetector::setQualityThreshold(double threshold)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_qualityThreshold = threshold;
}

void AgenticFailureDetector::enableToolValidation(bool enable)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enableToolValidation = enable;
}

void AgenticFailureDetector::addRefusalPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_refusalPatterns.begin(), m_refusalPatterns.end(), pattern) == m_refusalPatterns.end()) {
        m_refusalPatterns.push_back(pattern);
    }
}

void AgenticFailureDetector::addHallucinationPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_hallucinationPatterns.begin(), m_hallucinationPatterns.end(), pattern) == m_hallucinationPatterns.end()) {
        m_hallucinationPatterns.push_back(pattern);
    }
}

void AgenticFailureDetector::addLoopPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_loopPatterns.begin(), m_loopPatterns.end(), pattern) == m_loopPatterns.end()) {
        m_loopPatterns.push_back(pattern);
    }
}

void AgenticFailureDetector::addSafetyPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_safetyPatterns.begin(), m_safetyPatterns.end(), pattern) == m_safetyPatterns.end()) {
        m_safetyPatterns.push_back(pattern);
    }
}

AgenticFailureDetector::Stats AgenticFailureDetector::getStatistics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void AgenticFailureDetector::resetStatistics()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = Stats();
}

void AgenticFailureDetector::setEnabled(bool enable)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = enable;
    fprintf(stderr, "[INFO] [AgenticFailureDetector] %s\n", enable ? "Enabled" : "Disabled");
}

bool AgenticFailureDetector::isEnabled() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabled;
}

// ---------------------------------------------------------------------------
// Pre-execution safety (agentic/autonomous roadmap)
// ---------------------------------------------------------------------------
namespace {
    const std::vector<std::string>& dangerousCommandPatterns() {
        static const std::vector<std::string> pats = {
            "rm -rf", "rm -r /", "rmdir /s", "del /f /s", "format ", "mkfs",
            "shutdown", "reboot", "init 0", "init 6", "> /dev/sd", "dd if=",
            "chmod 777", ":(){ :|:& };:", "mv / ", "> /etc", "curl | sh",
            "wget -O- | bash", "format c:", "diskpart", "bcdedit"
        };
        return pats;
    }

    bool containsCi(const std::string& haystack, const std::string& needle) {
        std::string h = haystack;
        std::string n = needle;
        std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return h.find(n) != std::string::npos;
    }
}

bool AgenticFailureDetector::validateActionBeforeExecution(const ActionSummary& action)
{
    if (!m_enabled || !m_enableToolValidation) return true;

    if (wouldCauseDataLoss(action)) {
        fprintf(stderr, "[AgenticFailureDetector] Blocked: action would cause data loss: %s / %s\n",
                action.type.c_str(), action.target.c_str());
        return false;
    }

    std::string cmd;
    if (action.type == "invoke_command" || action.type == "run_command") {
        cmd = action.target;
        if (!action.params.empty()) cmd += " " + action.params;
    } else {
        cmd = action.target + " " + action.params;
    }

    if (isDangerousCommand(cmd)) {
        fprintf(stderr, "[AgenticFailureDetector] Blocked: dangerous command: %.80s\n", cmd.c_str());
        return false;
    }

    return true;
}

bool AgenticFailureDetector::isDangerousCommand(const std::string& commandStr) const
{
    std::string c = commandStr;
    std::transform(c.begin(), c.end(), c.begin(), [](unsigned char x) { return static_cast<char>(std::tolower(x)); });

    for (const std::string& p : dangerousCommandPatterns()) {
        if (containsCi(c, p)) return true;
    }

    if (c.find("format") != std::string::npos && (c.find("drive") != std::string::npos || c.find("disk") != std::string::npos))
        return true;

    return false;
}

bool AgenticFailureDetector::wouldCauseDataLoss(const ActionSummary& action) const
{
    std::string type = action.type;
    std::string target = action.target;
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);

    if (type == "file_edit") {
        if (containsCi(action.params, "\"action\":\"delete\"") || action.params.find("delete") != std::string::npos)
            return true;
        if (containsCi(target, "system32") || containsCi(target, "/etc/") || containsCi(target, "/boot/"))
            return true;
    }
    if (type == "file_delete" || type == "delete_file") return true;
    if (type == "run_command" || type == "invoke_command") {
        if (isDangerousCommand(target + " " + action.params)) return true;
    }
    return false;
}

std::string AgenticFailureDetector::suggestRecoveryAction(const FailureInfo& failure) const
{
    switch (failure.type) {
        case AgentFailureType::Refusal:
            return "Retry with rephrased request or different model.";
        case AgentFailureType::Hallucination:
            return "Verify outputs and retry with more context.";
        case AgentFailureType::FormatViolation:
            return "Retry; request JSON-only or structured output.";
        case AgentFailureType::InfiniteLoop:
            return "Stop current run; reduce max tokens or simplify task.";
        case AgentFailureType::TokenLimitExceeded:
            return "Increase max_tokens or summarize context and retry.";
        case AgentFailureType::ResourceExhausted:
            return "Reduce batch size or free GPU memory and retry.";
        case AgentFailureType::Timeout:
            return "Increase timeout or retry with smaller input.";
        case AgentFailureType::SafetyViolation:
            return "Review content; adjust prompt to avoid blocked patterns.";
        default:
            return "Check logs and retry; escalate if persistent.";
    }
}

double AgenticFailureDetector::calculateConfidence(AgentFailureType type, const std::string& output)
{
    double confidence = 0.5;
    
    switch (type) {
        case AgentFailureType::Refusal:
            confidence = str_count_substr(output, "cannot") > 0 ? 0.9 : 0.7;
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
