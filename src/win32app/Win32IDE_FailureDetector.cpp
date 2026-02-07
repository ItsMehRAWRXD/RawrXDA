// ============================================================================
// Win32IDE_FailureDetector.cpp — Agent Failure Detection & Self-Correction
// ============================================================================
// Detects 6 failure types in agent output and applies automatic correction:
//   1. Refusal — agent refuses to perform the task
//   2. Hallucination — agent generates fabricated facts/code
//   3. FormatViolation — output doesn't match expected format
//   4. InfiniteLoop — agent gets stuck repeating itself
//   5. QualityDegradation — output quality drops below threshold
//   6. EmptyResponse — agent returns nothing
//
// Correction strategies:
//   - Retry: re-send the same prompt
//   - Rephrase: modify the prompt to be clearer
//   - AddContext: inject additional context
//   - AdjustParams: change temperature/tokens
//   - ForceFormat: add explicit format instructions
//
// All detectors run heuristically — no cloud dependency.
// Statistics are tracked per session for diagnostics.
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <algorithm>
#include <sstream>
#include <set>
#include <cmath>

// ============================================================================
// INITIALIZATION
// ============================================================================

void Win32IDE::initFailureDetector() {
    m_failureDetectorEnabled = true;
    m_failureMaxRetries      = 3;

    // Reset statistics
    m_failureStats = {};
    m_failureStats.totalRequests = 0;
    m_failureStats.totalFailures = 0;
    m_failureStats.totalRetries  = 0;
    m_failureStats.refusalCount  = 0;
    m_failureStats.hallucinationCount = 0;
    m_failureStats.formatViolationCount = 0;
    m_failureStats.infiniteLoopCount = 0;
    m_failureStats.qualityDegradationCount = 0;
    m_failureStats.emptyResponseCount = 0;
    m_failureStats.successAfterRetry = 0;

    LOG_INFO("Failure detector initialized (maxRetries=" +
             std::to_string(m_failureMaxRetries) + ")");
}

// ============================================================================
// DETECT FAILURES — returns detected failure type(s)
// ============================================================================

std::vector<AgentFailureType> Win32IDE::detectFailures(const std::string& response,
                                                        const std::string& originalPrompt) {
    std::vector<AgentFailureType> failures;

    // 1. Empty Response
    if (response.empty() || response.find_first_not_of(" \t\n\r") == std::string::npos) {
        failures.push_back(AgentFailureType::EmptyResponse);
        m_failureStats.emptyResponseCount++;
        return failures;  // No point checking further
    }

    // 2. Refusal Detection — common refusal patterns
    if (detectRefusal(response)) {
        failures.push_back(AgentFailureType::Refusal);
        m_failureStats.refusalCount++;
    }

    // 3. Hallucination Detection — fabricated content markers
    if (detectHallucination(response, originalPrompt)) {
        failures.push_back(AgentFailureType::Hallucination);
        m_failureStats.hallucinationCount++;
    }

    // 4. Format Violation — response doesn't match expected structure
    if (detectFormatViolation(response, originalPrompt)) {
        failures.push_back(AgentFailureType::FormatViolation);
        m_failureStats.formatViolationCount++;
    }

    // 5. Infinite Loop — repetitive/stuck output
    if (detectInfiniteLoop(response)) {
        failures.push_back(AgentFailureType::InfiniteLoop);
        m_failureStats.infiniteLoopCount++;
    }

    // 6. Quality Degradation — low information density
    if (detectQualityDegradation(response)) {
        failures.push_back(AgentFailureType::QualityDegradation);
        m_failureStats.qualityDegradationCount++;
    }

    if (!failures.empty()) {
        m_failureStats.totalFailures++;
    }

    return failures;
}

// ============================================================================
// INDIVIDUAL DETECTORS
// ============================================================================

bool Win32IDE::detectRefusal(const std::string& response) {
    // Common refusal patterns across models
    static const char* refusalPatterns[] = {
        "I cannot",
        "I can't",
        "I'm unable to",
        "I am unable to",
        "I won't",
        "I will not",
        "I'm not able to",
        "As an AI",
        "as a language model",
        "I apologize, but I",
        "I'm sorry, but I can't",
        "I must decline",
        "I cannot assist with",
        "I'm not comfortable",
        "violates my guidelines",
        "against my policy",
        "ethical guidelines",
        "I cannot provide",
        nullptr
    };

    std::string lower = response;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    for (int i = 0; refusalPatterns[i]; i++) {
        std::string pattern = refusalPatterns[i];
        std::transform(pattern.begin(), pattern.end(), pattern.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (lower.find(pattern) != std::string::npos) {
            return true;
        }
    }

    // Refusal heuristic: very short response that's mostly apology
    if (response.size() < 100 && lower.find("sorry") != std::string::npos) {
        return true;
    }

    return false;
}

bool Win32IDE::detectHallucination(const std::string& response, const std::string& prompt) {
    // Heuristic: check for fabricated library/API names
    // Check for suspiciously specific version numbers that seem made up
    // Check for URLs that look fabricated

    // Pattern 1: Made-up imports/includes
    static const char* fabricatedLibraries[] = {
        "import quantum_",
        "from hyperspace",
        "#include <turbo_",
        "using namespace ultra_",
        nullptr
    };

    for (int i = 0; fabricatedLibraries[i]; i++) {
        if (response.find(fabricatedLibraries[i]) != std::string::npos) {
            return true;
        }
    }

    // Pattern 2: Self-contradictions (says "yes" and "no" to the same thing)
    size_t yesCount = 0, noCount = 0;
    std::string lower = response;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    size_t pos = 0;
    while ((pos = lower.find("yes", pos)) != std::string::npos) { yesCount++; pos += 3; }
    pos = 0;
    while ((pos = lower.find("no", pos)) != std::string::npos) { noCount++; pos += 2; }

    // High contradiction ratio in a short response
    if (response.size() < 300 && yesCount > 3 && noCount > 3) {
        return true;
    }

    return false;
}

bool Win32IDE::detectFormatViolation(const std::string& response, const std::string& prompt) {
    std::string lowerPrompt = prompt;
    std::transform(lowerPrompt.begin(), lowerPrompt.end(), lowerPrompt.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    // If prompt asked for JSON, check for valid JSON-like structure
    if (lowerPrompt.find("json") != std::string::npos) {
        if (response.find('{') == std::string::npos && response.find('[') == std::string::npos) {
            return true;
        }
    }

    // If prompt asked for "only code" or "code only", check for explanation text
    if (lowerPrompt.find("only code") != std::string::npos ||
        lowerPrompt.find("code only") != std::string::npos ||
        lowerPrompt.find("only output the") != std::string::npos) {
        // Check if response has excessive natural language
        int alphaCount = 0;
        int codeIndicators = 0;
        for (char c : response) {
            if (std::isalpha((unsigned char)c)) alphaCount++;
            if (c == '{' || c == '}' || c == ';' || c == '(' || c == ')') codeIndicators++;
        }
        // If it's mostly text with very few code characters
        if (alphaCount > 200 && codeIndicators < 5) {
            return true;
        }
    }

    return false;
}

bool Win32IDE::detectInfiniteLoop(const std::string& response) {
    if (response.size() < 100) return false;

    // Check for repeated segments
    // Split into chunks and check for repetition
    const int chunkSize = 50;
    std::set<std::string> seen;
    int repeats = 0;

    for (size_t i = 0; i + chunkSize <= response.size(); i += chunkSize) {
        std::string chunk = response.substr(i, chunkSize);
        if (seen.count(chunk)) {
            repeats++;
        } else {
            seen.insert(chunk);
        }
    }

    // If more than 40% of chunks are repeats, likely stuck
    int totalChunks = (int)(response.size() / chunkSize);
    if (totalChunks > 3 && repeats > totalChunks * 4 / 10) {
        return true;
    }

    // Check for exact line repetition
    std::istringstream stream(response);
    std::string line;
    std::map<std::string, int> lineCounts;
    int totalLines = 0;

    while (std::getline(stream, line)) {
        if (line.size() > 5) {  // Skip blank/trivial lines
            lineCounts[line]++;
            totalLines++;
        }
    }

    // If any line appears more than 5 times, likely looping
    for (const auto& [text, count] : lineCounts) {
        if (count > 5 && totalLines > 0 && count > totalLines / 3) {
            return true;
        }
    }

    return false;
}

bool Win32IDE::detectQualityDegradation(const std::string& response) {
    if (response.size() < 50) return false;

    // Information density heuristic: ratio of unique words to total words
    std::istringstream stream(response);
    std::string word;
    std::set<std::string> uniqueWords;
    int totalWords = 0;

    while (stream >> word) {
        std::transform(word.begin(), word.end(), word.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        uniqueWords.insert(word);
        totalWords++;
    }

    if (totalWords < 10) return false;

    float uniqueRatio = (float)uniqueWords.size() / totalWords;

    // Very low unique ratio indicates repetitive/low-quality output
    if (uniqueRatio < 0.15f) {
        return true;
    }

    // Check for "filler" dominance
    static const char* fillerWords[] = {
        "the", "a", "an", "is", "are", "was", "were", "be", "been",
        "have", "has", "had", "do", "does", "did", "will", "would",
        "could", "should", "may", "might", "can", "shall", "to", "of",
        "in", "for", "on", "with", "at", "by", "from", "as", "into",
        "it", "this", "that", "these", "those", nullptr
    };

    int fillerCount = 0;
    for (const auto& w : uniqueWords) {
        for (int i = 0; fillerWords[i]; i++) {
            if (w == fillerWords[i]) {
                fillerCount++;
                break;
            }
        }
    }

    // If filler words dominate unique vocabulary
    if (uniqueWords.size() > 10 && fillerCount > (int)(uniqueWords.size() * 7 / 10)) {
        return true;
    }

    return false;
}

// ============================================================================
// APPLY CORRECTION — attempts to fix the failure
// ============================================================================

std::string Win32IDE::applyCorrectionStrategy(AgentFailureType failure,
                                               const std::string& originalPrompt,
                                               int retryAttempt) {
    m_failureStats.totalRetries++;

    switch (failure) {
        case AgentFailureType::Refusal:
            // Rephrase to avoid triggering refusal
            return "Please help me with the following technical task. "
                   "This is for legitimate software development purposes:\n\n" + originalPrompt;

        case AgentFailureType::Hallucination:
            // Add context grounding
            return "IMPORTANT: Only use real, verified APIs and libraries. "
                   "Do not fabricate function names or URLs. "
                   "If you are unsure, say so explicitly.\n\n" + originalPrompt;

        case AgentFailureType::FormatViolation:
            // Force format compliance
            return "CRITICAL FORMAT REQUIREMENT: Follow the exact output format requested. "
                   "Do not add explanations unless asked. "
                   "Output ONLY what was requested.\n\n" + originalPrompt;

        case AgentFailureType::InfiniteLoop:
            // Reduce token count and add termination instruction
            return "Keep your response concise (under 200 words). "
                   "Do not repeat yourself. "
                   "Stop after answering once.\n\n" + originalPrompt;

        case AgentFailureType::QualityDegradation:
            // Request higher quality
            return "Please provide a detailed, high-quality response with specific examples "
                   "and concrete implementation details:\n\n" + originalPrompt;

        case AgentFailureType::EmptyResponse:
            // Simplify the prompt
            if (retryAttempt == 1) {
                return "Please respond to the following:\n\n" + originalPrompt;
            } else {
                // Further simplification
                return "Answer this: " + originalPrompt.substr(0, 500);
            }

        default:
            return originalPrompt;
    }
}

// ============================================================================
// EXECUTE WITH RETRY — the main entry point for resilient agent calls
// ============================================================================

AgentResponse Win32IDE::executeWithFailureDetection(const std::string& prompt) {
    if (!m_failureDetectorEnabled || !m_agenticBridge) {
        // Passthrough if detector is disabled
        if (m_agenticBridge) return m_agenticBridge->ExecuteAgentCommand(prompt);
        return {AgentResponseType::AGENT_ERROR, "Agent not initialized"};
    }

    m_failureStats.totalRequests++;

    std::string currentPrompt = prompt;

    for (int attempt = 0; attempt <= m_failureMaxRetries; attempt++) {
        AgentResponse response = m_agenticBridge->ExecuteAgentCommand(currentPrompt);

        // Detect failures
        auto failures = detectFailures(response.content, prompt);

        if (failures.empty()) {
            // Success
            if (attempt > 0) {
                m_failureStats.successAfterRetry++;
                appendToOutput("[FailureDetector] Succeeded after " + std::to_string(attempt) +
                               " retry(s)", "General", OutputSeverity::Info);

                // Record correction success event
                recordEvent(AgentEventType::FailureCorrected, "",
                           prompt.substr(0, 256), "Corrected after " + std::to_string(attempt) + " retry(s)",
                           0, true);
            }
            return response;
        }

        // Log failures
        for (auto f : failures) {
            appendToOutput("[FailureDetector] Detected: " + failureTypeString(f) +
                           " (attempt " + std::to_string(attempt + 1) + "/" +
                           std::to_string(m_failureMaxRetries + 1) + ")",
                           "General", OutputSeverity::Warning);

            // Record failure detection event
            recordEvent(AgentEventType::FailureDetected, "",
                       prompt.substr(0, 256), failureTypeString(f),
                       0, false, "",
                       "{\"attempt\":" + std::to_string(attempt + 1) + "}");
        }

        // If this is the last attempt, return what we have
        if (attempt == m_failureMaxRetries) {
            appendToOutput("[FailureDetector] Max retries exhausted. Returning last response.",
                           "General", OutputSeverity::Error);
            return response;
        }

        // Apply correction for the most severe failure
        AgentFailureType primaryFailure = failures[0];
        currentPrompt = applyCorrectionStrategy(primaryFailure, prompt, attempt + 1);
    }

    return {AgentResponseType::AGENT_ERROR, "Failure detection exhausted all retries"};
}

// ============================================================================
// FAILURE TYPE STRING
// ============================================================================

std::string Win32IDE::failureTypeString(AgentFailureType type) const {
    switch (type) {
        case AgentFailureType::Refusal:             return "Refusal";
        case AgentFailureType::Hallucination:       return "Hallucination";
        case AgentFailureType::FormatViolation:     return "Format Violation";
        case AgentFailureType::InfiniteLoop:        return "Infinite Loop";
        case AgentFailureType::QualityDegradation:  return "Quality Degradation";
        case AgentFailureType::EmptyResponse:       return "Empty Response";
        default:                                    return "Unknown";
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

std::string Win32IDE::getFailureDetectorStats() const {
    std::ostringstream oss;
    oss << "=== Failure Detector Statistics ===\r\n";
    oss << "Total Requests:        " << m_failureStats.totalRequests << "\r\n";
    oss << "Total Failures:        " << m_failureStats.totalFailures << "\r\n";
    oss << "Total Retries:         " << m_failureStats.totalRetries << "\r\n";
    oss << "Success After Retry:   " << m_failureStats.successAfterRetry << "\r\n";
    oss << "---\r\n";
    oss << "Refusals:              " << m_failureStats.refusalCount << "\r\n";
    oss << "Hallucinations:        " << m_failureStats.hallucinationCount << "\r\n";
    oss << "Format Violations:     " << m_failureStats.formatViolationCount << "\r\n";
    oss << "Infinite Loops:        " << m_failureStats.infiniteLoopCount << "\r\n";
    oss << "Quality Degradations:  " << m_failureStats.qualityDegradationCount << "\r\n";
    oss << "Empty Responses:       " << m_failureStats.emptyResponseCount << "\r\n";

    if (m_failureStats.totalRequests > 0) {
        float successRate = 1.0f - (float)m_failureStats.totalFailures / m_failureStats.totalRequests;
        oss << "---\r\n";
        oss << "Success Rate:          " << (int)(successRate * 100) << "%\r\n";
    }

    return oss.str();
}

void Win32IDE::toggleFailureDetector() {
    m_failureDetectorEnabled = !m_failureDetectorEnabled;
    appendToOutput(std::string("[FailureDetector] ") +
                   (m_failureDetectorEnabled ? "Enabled" : "Disabled"),
                   "General", OutputSeverity::Info);
}
