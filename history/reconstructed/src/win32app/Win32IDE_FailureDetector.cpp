// ============================================================================
// Win32IDE_FailureDetector.cpp — Agent Failure Detection & Self-Correction
// ============================================================================
// Phase 4B: Failure Detection & Bounded Retry Intelligence
//
// Detects 12 failure types in agent output with confidence scoring:
//   1. Refusal — agent refuses to perform the task
//   2. Hallucination — agent generates fabricated facts/code
//   3. FormatViolation — output doesn't match expected format
//   4. InfiniteLoop — agent gets stuck repeating itself
//   5. QualityDegradation — output quality drops below threshold
//   6. EmptyResponse — agent returns nothing
//   7. Timeout — inference took too long
//   8. ToolError — tool invocation returned an error
//   9. InvalidOutput — structurally invalid output (malformed JSON, etc.)
//  10. LowConfidence — model expressed high uncertainty
//  11. SafetyViolation — safety filter triggered
//  12. UserAbort — user explicitly cancelled
//
// Phase 4B Key Rules:
//   - Failures detected at BOUNDARIES only (4 choke points)
//   - Max retries: 1 (hard cap)
//   - Every retry requires explicit user approval via Plan Dialog
//   - Every failure emits: FailureDetected → FailureCorrected | FailureRetryDeclined
//   - FailureClassification carries reason + confidence(0-1) + evidence
//
// Correction strategies (one per reason, bounded):
//   - Hallucination: Tighten prompt + add validation constraint
//   - ToolError:     Retry tool once, same inputs
//   - LowConfidence: Re-run step with reduced temperature
//   - Timeout:       Reduce parallelism or step size
//   - InvalidOutput: Regenerate with schema reminder
//   - Refusal:       Rephrase to avoid trigger
//   - FormatViolation: Add explicit format instructions
//   - InfiniteLoop:  Reduce token count + termination instruction
//   - QualityDegradation: Request higher quality with examples
//   - EmptyResponse: Simplify the prompt
//   - SafetyViolation: Reframe as technical context
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
    m_failureMaxRetries      = 1;  // Phase 4B: hard cap at 1

    // Reset statistics
    m_failureStats = {};

    LOG_INFO("Failure detector initialized (Phase 4B, maxRetries=" +
             std::to_string(m_failureMaxRetries) + ")");
}

// ============================================================================
// CLASSIFY FAILURE — returns a single FailureClassification with confidence
// ============================================================================

FailureClassification Win32IDE::classifyFailure(const std::string& response,
                                                  const std::string& originalPrompt) {
    auto all = classifyAllFailures(response, originalPrompt);
    if (all.empty()) return FailureClassification::ok();

    // Return the highest-confidence failure
    FailureClassification worst = all[0];
    for (size_t i = 1; i < all.size(); i++) {
        if (all[i].confidence > worst.confidence) worst = all[i];
    }
    return worst;
}

// ============================================================================
// CLASSIFY ALL FAILURES — returns every detected failure with confidence
// ============================================================================

std::vector<FailureClassification> Win32IDE::classifyAllFailures(
        const std::string& response, const std::string& originalPrompt) {

    std::vector<FailureClassification> results;

    // 1. Empty Response — highest priority, no point checking further
    if (response.empty() || response.find_first_not_of(" \t\n\r") == std::string::npos) {
        results.push_back(FailureClassification::make(
            AgentFailureType::EmptyResponse, 1.0f, "Response is empty or whitespace-only"));
        m_failureStats.emptyResponseCount++;
        return results;
    }

    // 2. Timeout markers
    {
        static const char* timeoutPatterns[] = {
            "[timeout]", "[TIMEOUT]", "timed out", "inference timeout",
            "deadline exceeded", "request timed out", "operation timed out",
            nullptr
        };
        std::string lower = response;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        for (int i = 0; timeoutPatterns[i]; i++) {
            if (lower.find(timeoutPatterns[i]) != std::string::npos) {
                results.push_back(FailureClassification::make(
                    AgentFailureType::Timeout, 0.95f,
                    "Contains timeout indicator: " + std::string(timeoutPatterns[i])));
                m_failureStats.timeoutCount++;
                break;
            }
        }
    }

    // 3. Safety Violation
    {
        static const char* safetyPatterns[] = {
            "[SENSITIVE]", "[REDACTED]", "[FILTERED]", "[BLOCKED]",
            "[SAFETY]", "[WARNING]", "content policy", "safety filter",
            "content filter triggered", nullptr
        };
        for (int i = 0; safetyPatterns[i]; i++) {
            if (response.find(safetyPatterns[i]) != std::string::npos) {
                results.push_back(FailureClassification::make(
                    AgentFailureType::SafetyViolation, 0.92f,
                    "Contains safety marker: " + std::string(safetyPatterns[i])));
                m_failureStats.safetyViolationCount++;
                break;
            }
        }
    }

    // 4. Refusal Detection — common refusal patterns
    if (detectRefusal(response)) {
        // Compute confidence based on pattern density
        std::string lower = response;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        int patternHits = 0;
        static const char* refusalIndicators[] = {
            "i cannot", "i can't", "i'm unable", "i won't", "i must decline",
            "as an ai", "as a language model", "ethical guidelines", nullptr
        };
        for (int i = 0; refusalIndicators[i]; i++) {
            if (lower.find(refusalIndicators[i]) != std::string::npos) patternHits++;
        }
        float conf = std::min(0.6f + patternHits * 0.1f, 1.0f);
        results.push_back(FailureClassification::make(
            AgentFailureType::Refusal, conf,
            "Matched " + std::to_string(patternHits) + " refusal pattern(s)"));
        m_failureStats.refusalCount++;
    }

    // 5. Hallucination Detection
    if (detectHallucination(response, originalPrompt)) {
        results.push_back(FailureClassification::make(
            AgentFailureType::Hallucination, 0.72f,
            "Contains fabricated content markers or self-contradictions"));
        m_failureStats.hallucinationCount++;
    }

    // 6. Invalid Output — malformed JSON, unclosed code blocks
    {
        bool invalid = false;
        std::string evidence;
        // Check JSON structural validity
        if (response.find('{') != std::string::npos) {
            int openBraces = 0, closeBraces = 0;
            for (char c : response) {
                if (c == '{') openBraces++;
                if (c == '}') closeBraces++;
            }
            if (openBraces != closeBraces) {
                invalid = true;
                evidence = "Mismatched braces: " + std::to_string(openBraces) +
                           " open, " + std::to_string(closeBraces) + " close";
            }
        }
        // Check unclosed code blocks
        int backtickTriples = 0;
        size_t pos = 0;
        while ((pos = response.find("```", pos)) != std::string::npos) {
            backtickTriples++;
            pos += 3;
        }
        if (backtickTriples % 2 != 0) {
            invalid = true;
            evidence = evidence.empty() ? "Unclosed code block (odd ``` count)"
                                        : evidence + "; unclosed code block";
        }
        if (invalid) {
            results.push_back(FailureClassification::make(
                AgentFailureType::InvalidOutput, 0.80f, evidence));
            m_failureStats.invalidOutputCount++;
        }
    }

    // 7. Format Violation
    if (detectFormatViolation(response, originalPrompt)) {
        results.push_back(FailureClassification::make(
            AgentFailureType::FormatViolation, 0.75f,
            "Output format does not match prompt requirements"));
        m_failureStats.formatViolationCount++;
    }

    // 8. Infinite Loop
    if (detectInfiniteLoop(response)) {
        results.push_back(FailureClassification::make(
            AgentFailureType::InfiniteLoop, 0.88f,
            "Repetitive content detected (>40% chunk duplication)"));
        m_failureStats.infiniteLoopCount++;
    }

    // 9. Low Confidence — model expresses high uncertainty
    {
        std::string lower = response;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        int uncertainHits = 0;
        static const char* uncertainPatterns[] = {
            "i'm not sure", "i'm not certain", "i might be wrong",
            "i cannot guarantee", "this may not be correct",
            "i don't have enough information", "use at your own risk",
            "this is a guess", "i'm speculating", nullptr
        };
        for (int i = 0; uncertainPatterns[i]; i++) {
            if (lower.find(uncertainPatterns[i]) != std::string::npos) uncertainHits++;
        }
        if (uncertainHits >= 2) {
            float conf = std::min(0.5f + uncertainHits * 0.12f, 0.95f);
            results.push_back(FailureClassification::make(
                AgentFailureType::LowConfidence, conf,
                "Model expressed uncertainty (" + std::to_string(uncertainHits) + " indicators)"));
            m_failureStats.lowConfidenceCount++;
        }
    }

    // 10. Quality Degradation
    if (detectQualityDegradation(response)) {
        results.push_back(FailureClassification::make(
            AgentFailureType::QualityDegradation, 0.65f,
            "Low information density or filler-dominated output"));
        m_failureStats.qualityDegradationCount++;
    }

    // 11. Tool Error — detected via choke point hooks, not here
    //     (hookToolResult handles this separately)

    if (!results.empty()) {
        m_failureStats.totalFailures++;
    }

    return results;
}

// ============================================================================
// DETECT FAILURES — legacy API, returns failure type vector
// ============================================================================

std::vector<AgentFailureType> Win32IDE::detectFailures(const std::string& response,
                                                        const std::string& originalPrompt) {
    auto classifications = classifyAllFailures(response, originalPrompt);
    std::vector<AgentFailureType> types;
    types.reserve(classifications.size());
    for (const auto& fc : classifications) {
        types.push_back(fc.reason);
    }
    return types;
}

// ============================================================================
// INDIVIDUAL DETECTORS (preserved — NO SIMPLIFICATION)
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
// PHASE 4B: CHOKE-POINT HOOKS — detection at boundaries only
// ============================================================================

// Hook 1: Post-generation validation
// Called after any agent generates a response
FailureClassification Win32IDE::hookPostGeneration(const std::string& response,
                                                     const std::string& prompt) {
    if (!m_failureDetectorEnabled) return FailureClassification::ok();

    m_failureStats.totalRequests++;

    auto fc = classifyFailure(response, prompt);
    if (fc.reason != AgentFailureType::None) {
        LOG_WARNING("[FailureDetector] Post-generation failure: " +
                    failureTypeString(fc.reason) +
                    " (confidence=" + std::to_string((int)(fc.confidence * 100)) + "%)");

        recordEvent(AgentEventType::FailureDetected, "",
                   prompt.substr(0, 256),
                   failureTypeString(fc.reason) + " (" +
                   std::to_string((int)(fc.confidence * 100)) + "%)",
                   0, false, "",
                   "{\"reason\":\"" + failureTypeString(fc.reason) +
                   "\",\"confidence\":" + std::to_string(fc.confidence) +
                   ",\"evidence\":\"" + fc.evidence.substr(0, 128) + "\"}");
    }
    return fc;
}

// Hook 2: Tool invocation result parsing
// Called after a tool returns output
FailureClassification Win32IDE::hookToolResult(const std::string& toolName,
                                                 const std::string& toolOutput) {
    if (!m_failureDetectorEnabled) return FailureClassification::ok();

    // Tool-specific error detection
    std::string lower = toolOutput;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    // Check for explicit error markers in tool output
    bool isError = false;
    std::string evidence;

    if (lower.find("[error]") != std::string::npos ||
        lower.find("error:") != std::string::npos) {
        isError = true;
        evidence = "Tool returned error marker";
    } else if (lower.find("exception") != std::string::npos &&
               lower.find("stack trace") != std::string::npos) {
        isError = true;
        evidence = "Tool threw exception with stack trace";
    } else if (lower.find("permission denied") != std::string::npos ||
               lower.find("access denied") != std::string::npos) {
        isError = true;
        evidence = "Tool access denied";
    } else if (lower.find("file not found") != std::string::npos ||
               lower.find("no such file") != std::string::npos) {
        isError = true;
        evidence = "Tool target file not found";
    } else if (lower.find("command not found") != std::string::npos ||
               lower.find("not recognized") != std::string::npos) {
        isError = true;
        evidence = "Tool command not found";
    } else if (toolOutput.empty()) {
        isError = true;
        evidence = "Tool returned empty output";
    }

    if (isError) {
        auto fc = FailureClassification::make(
            AgentFailureType::ToolError, 0.90f,
            "Tool '" + toolName + "': " + evidence);
        m_failureStats.toolErrorCount++;

        LOG_WARNING("[FailureDetector] Tool error in '" + toolName + "': " + evidence);

        recordEvent(AgentEventType::FailureDetected, "",
                   "Tool: " + toolName,
                   evidence, 0, false, "",
                   "{\"reason\":\"ToolError\",\"tool\":\"" + toolName +
                   "\",\"confidence\":0.90}");

        return fc;
    }

    return FailureClassification::ok();
}

// Hook 3: Plan step output verification
// Called after each plan step executes
FailureClassification Win32IDE::hookPlanStepOutput(int stepIndex,
                                                     const std::string& output) {
    if (!m_failureDetectorEnabled) return FailureClassification::ok();
    if (stepIndex < 0 || stepIndex >= (int)m_currentPlan.steps.size())
        return FailureClassification::ok();

    const auto& step = m_currentPlan.steps[stepIndex];

    // Classify against the step description as "prompt"
    auto fc = classifyFailure(output, step.description);

    // Additional plan-step-specific checks
    if (fc.reason == AgentFailureType::None) {
        // Check for explicit error markers that classifyFailure might miss
        std::string lower = output;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (lower.find("[error]") != std::string::npos ||
            lower.find("error:") != std::string::npos ||
            lower.find("failed to") != std::string::npos ||
            lower.find("fatal:") != std::string::npos) {
            fc = FailureClassification::make(
                AgentFailureType::ToolError, 0.85f,
                "Plan step " + std::to_string(stepIndex + 1) + " output contains error marker");
            m_failureStats.toolErrorCount++;
        }
    }

    if (fc.reason != AgentFailureType::None) {
        LOG_WARNING("[FailureDetector] Plan step " + std::to_string(stepIndex + 1) +
                    " failed: " + failureTypeString(fc.reason) +
                    " (conf=" + std::to_string((int)(fc.confidence * 100)) + "%)");

        recordEvent(AgentEventType::FailureDetected, "",
                   "PlanStep " + std::to_string(stepIndex + 1) + ": " + step.title,
                   failureTypeString(fc.reason),
                   0, false, "",
                   "{\"reason\":\"" + failureTypeString(fc.reason) +
                   "\",\"stepIndex\":" + std::to_string(stepIndex) +
                   ",\"confidence\":" + std::to_string(fc.confidence) + "}");
    }

    return fc;
}

// Hook 4: Agent command post-processing
// Called after the main agent command handler
FailureClassification Win32IDE::hookAgentCommand(const std::string& response,
                                                    const std::string& prompt) {
    // Delegates to hookPostGeneration — same logic for now,
    // but kept as a separate hook for semantic clarity and future divergence
    return hookPostGeneration(response, prompt);
}

// Hook 5: Swarm merge sanity check
// Called after all swarm tasks are merged into a single result
FailureClassification Win32IDE::hookSwarmMerge(const std::string& mergedResult,
                                                int taskCount,
                                                const std::string& strategy) {
    if (!m_failureDetectorEnabled) return FailureClassification::ok();

    // First: run standard classifiers on the merged output
    auto fc = classifyFailure(mergedResult, "swarm merge (" + strategy + ")");

    // Additional swarm-specific sanity checks
    if (fc.reason == AgentFailureType::None) {
        // Check 1: Merged result is suspiciously short for multi-task swarm
        if (taskCount > 2 && mergedResult.size() < 50) {
            fc = FailureClassification::make(
                AgentFailureType::QualityDegradation, 0.70f,
                "Swarm merged " + std::to_string(taskCount) +
                " tasks but result is only " + std::to_string(mergedResult.size()) + " chars");
            m_failureStats.qualityDegradationCount++;
        }

        // Check 2: Merged result contains unresolved conflict markers
        if (mergedResult.find("[CONFLICT]") != std::string::npos ||
            mergedResult.find("[MERGE ERROR]") != std::string::npos ||
            mergedResult.find("<<<<<<") != std::string::npos) {
            fc = FailureClassification::make(
                AgentFailureType::InvalidOutput, 0.85f,
                "Swarm merge contains unresolved conflict markers");
            m_failureStats.invalidOutputCount++;
        }

        // Check 3: All tasks returned the same output (likely stuck model)
        // We check by seeing if the merged output is just repetition
        if (taskCount > 1 && mergedResult.size() > 100) {
            size_t halfLen = mergedResult.size() / 2;
            std::string firstHalf = mergedResult.substr(0, halfLen);
            std::string secondHalf = mergedResult.substr(halfLen);
            // Simple check: if the halves are >80% identical chars
            int matchCount = 0;
            size_t checkLen = std::min(firstHalf.size(), secondHalf.size());
            for (size_t i = 0; i < checkLen; i++) {
                if (firstHalf[i] == secondHalf[i]) matchCount++;
            }
            if (checkLen > 0 && matchCount > (int)(checkLen * 8 / 10)) {
                fc = FailureClassification::make(
                    AgentFailureType::InfiniteLoop, 0.78f,
                    "Swarm tasks produced near-identical outputs (" +
                    std::to_string(matchCount * 100 / checkLen) + "% match)");
                m_failureStats.infiniteLoopCount++;
            }
        }
    }

    if (fc.reason != AgentFailureType::None) {
        m_failureStats.totalFailures++;

        LOG_WARNING("[FailureDetector] Swarm merge failure: " +
                    failureTypeString(fc.reason) +
                    " (confidence=" + std::to_string((int)(fc.confidence * 100)) +
                    "%, strategy=" + strategy +
                    ", tasks=" + std::to_string(taskCount) + ")");

        recordEvent(AgentEventType::FailureDetected, "",
                   "Swarm merge (" + strategy + ", " + std::to_string(taskCount) + " tasks)",
                   failureTypeString(fc.reason) + " (" +
                   std::to_string((int)(fc.confidence * 100)) + "%)",
                   0, false, "",
                   "{\"reason\":\"" + failureTypeString(fc.reason) +
                   "\",\"confidence\":" + std::to_string(fc.confidence) +
                   ",\"strategy\":\"" + strategy +
                   "\",\"taskCount\":" + std::to_string(taskCount) +
                   ",\"evidence\":\"" + fc.evidence.substr(0, 128) + "\"}");
    }

    return fc;
}

// ============================================================================
// PHASE 4B: BOUNDED RETRY — max 1 retry, approval required
// ============================================================================

std::string Win32IDE::buildRetryPrompt(const FailureClassification& failure,
                                        const std::string& originalPrompt) {
    switch (failure.reason) {
        case AgentFailureType::Hallucination:
            return "IMPORTANT: Only use real, verified APIs and libraries. "
                   "Do not fabricate function names or URLs. "
                   "If you are unsure, say so explicitly.\n\n" + originalPrompt;

        case AgentFailureType::ToolError:
            // Retry same inputs — let the tool try again
            return originalPrompt;

        case AgentFailureType::LowConfidence:
            return "Be precise and specific in your response. "
                   "Provide only facts you are confident about. "
                   "Use lower temperature reasoning.\n\n" + originalPrompt;

        case AgentFailureType::Timeout:
            return "Keep your response concise (under 150 words). "
                   "Focus on the most critical part only.\n\n" + originalPrompt;

        case AgentFailureType::InvalidOutput:
            return "CRITICAL FORMAT REQUIREMENT: Ensure all JSON is valid, "
                   "all code blocks are closed, and the output is structurally complete.\n\n" +
                   originalPrompt;

        case AgentFailureType::Refusal:
            return "Please help me with the following technical task. "
                   "This is for legitimate software development purposes:\n\n" + originalPrompt;

        case AgentFailureType::FormatViolation:
            return "CRITICAL FORMAT REQUIREMENT: Follow the exact output format requested. "
                   "Do not add explanations unless asked. "
                   "Output ONLY what was requested.\n\n" + originalPrompt;

        case AgentFailureType::InfiniteLoop:
            return "Keep your response concise (under 200 words). "
                   "Do not repeat yourself. Stop after answering once.\n\n" + originalPrompt;

        case AgentFailureType::QualityDegradation:
            return "Please provide a detailed, high-quality response with specific examples "
                   "and concrete implementation details:\n\n" + originalPrompt;

        case AgentFailureType::EmptyResponse:
            return "Please respond to the following:\n\n" + originalPrompt;

        case AgentFailureType::SafetyViolation:
            return "This is a legitimate technical/educational request. "
                   "Please provide the information in a professional context:\n\n" + originalPrompt;

        default:
            return originalPrompt;
    }
}

std::string Win32IDE::getRetryStrategyDescription(AgentFailureType reason) const {
    switch (reason) {
        case AgentFailureType::Hallucination:
            return "Tighten prompt + add validation constraint";
        case AgentFailureType::ToolError:
            return "Retry tool once with same inputs";
        case AgentFailureType::LowConfidence:
            return "Re-run with reduced temperature and precision directive";
        case AgentFailureType::Timeout:
            return "Reduce response size and simplify request";
        case AgentFailureType::InvalidOutput:
            return "Regenerate with schema/format reminder";
        case AgentFailureType::Refusal:
            return "Rephrase as technical context";
        case AgentFailureType::FormatViolation:
            return "Add explicit format instructions";
        case AgentFailureType::InfiniteLoop:
            return "Reduce token count + termination instruction";
        case AgentFailureType::QualityDegradation:
            return "Request higher quality with examples";
        case AgentFailureType::EmptyResponse:
            return "Simplify the prompt";
        case AgentFailureType::SafetyViolation:
            return "Reframe as professional technical context";
        default:
            return "No strategy available";
    }
}

// ============================================================================
// PHASE 4B: RETRY APPROVAL IN PLAN DIALOG
// ============================================================================
// Shows a retry proposal inside the existing plan dialog.
// Returns true if user approved, false if declined.

bool Win32IDE::showRetryApprovalInPlanDialog(int stepIndex,
                                               const FailureClassification& failure) {
    // Build the explanation text
    char msg[1024];
    snprintf(msg, sizeof(msg),
             "Step %d failed due to %s (%.0f%% confidence).\n\n"
             "Evidence: %s\n\n"
             "Strategy: %s\n\n"
             "Retry this step?",
             stepIndex + 1,
             failureTypeString(failure.reason).c_str(),
             failure.confidence * 100.0f,
             failure.evidence.c_str(),
             getRetryStrategyDescription(failure.reason).c_str());

    // If the plan dialog is open, use it as parent; otherwise use main window
    HWND parentWnd = (m_hwndPlanDialog && IsWindow(m_hwndPlanDialog))
                     ? m_hwndPlanDialog : m_hwndMain;

    // Update the plan dialog detail panel with failure info before showing prompt
    if (m_hwndPlanDialog && IsWindow(m_hwndPlanDialog) && m_hwndPlanDetail) {
        std::ostringstream detailOss;
        detailOss << "=== FAILURE DETECTED ===\r\n";
        detailOss << "Reason: " << failureTypeString(failure.reason) << "\r\n";
        detailOss << "Confidence: " << (int)(failure.confidence * 100) << "%\r\n";
        detailOss << "Evidence: " << failure.evidence << "\r\n";
        detailOss << "\r\nRetry Strategy: " << getRetryStrategyDescription(failure.reason) << "\r\n";
        SetWindowTextA(m_hwndPlanDetail, detailOss.str().c_str());
    }

    // Highlight the failed step in the ListView
    if (m_hwndPlanList && stepIndex >= 0 && stepIndex < (int)m_currentPlan.steps.size()) {
        ListView_SetItemText(m_hwndPlanList, stepIndex, 1, const_cast<char*>("RETRY?"));
        ListView_SetItemState(m_hwndPlanList, stepIndex,
                              LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(m_hwndPlanList, stepIndex, FALSE);
    }

    // Show approval dialog — this is the human control point
    int choice = MessageBoxA(parentWnd, msg, "Retry Step \xe2\x80\x94 Failure Detected",
                             MB_YESNO | MB_ICONQUESTION);

    if (choice == IDYES) {
        appendToOutput("[FailureDetector] Retry approved for step " +
                       std::to_string(stepIndex + 1) + " (" +
                       failureTypeString(failure.reason) + ")",
                       "General", OutputSeverity::Info);

        recordEvent(AgentEventType::FailureDetected, "",
                   "Retry approved: step " + std::to_string(stepIndex + 1),
                   failureTypeString(failure.reason),
                   0, true, "",
                   "{\"action\":\"retry_approved\",\"reason\":\"" +
                   failureTypeString(failure.reason) + "\",\"confidence\":" +
                   std::to_string(failure.confidence) + "}");

        return true;
    } else {
        appendToOutput("[FailureDetector] Retry declined for step " +
                       std::to_string(stepIndex + 1),
                       "General", OutputSeverity::Warning);

        // Record the decline
        recordEvent(AgentEventType::FailureRetryDeclined, "",
                   "Retry declined: step " + std::to_string(stepIndex + 1),
                   failureTypeString(failure.reason),
                   0, false, "",
                   "{\"action\":\"retry_declined\",\"reason\":\"" +
                   failureTypeString(failure.reason) + "\",\"confidence\":" +
                   std::to_string(failure.confidence) + "}");

        m_failureStats.retriesDeclined++;
        return false;
    }
}

// ============================================================================
// PHASE 4B: EXECUTE WITH BOUNDED RETRY
// ============================================================================
// Replaces the old executeWithFailureDetection with Phase 4B semantics:
//   - Max 1 retry (hard cap)
//   - Every retry requires user approval
//   - Records full lifecycle: Detected → Corrected | RetryDeclined

AgentResponse Win32IDE::executeWithBoundedRetry(const std::string& prompt) {
    if (!m_failureDetectorEnabled || !m_agenticBridge) {
        if (m_agenticBridge) return m_agenticBridge->ExecuteAgentCommand(prompt);
        return {AgentResponseType::AGENT_ERROR, "Agent not initialized"};
    }

    m_failureStats.totalRequests++;

    // Attempt 1: Execute normally
    AgentResponse response = m_agenticBridge->ExecuteAgentCommand(prompt);

    // Classify via post-generation hook
    auto fc = hookPostGeneration(response.content, prompt);

    if (fc.reason == AgentFailureType::None) {
        return response;  // Clean success
    }

    // Failure detected — log it
    appendToOutput("[FailureDetector] " + failureTypeString(fc.reason) +
                   " detected (confidence=" + std::to_string((int)(fc.confidence * 100)) +
                   "%). Evidence: " + fc.evidence,
                   "General", OutputSeverity::Warning);

    // UserAbort is terminal — no retry
    if (fc.reason == AgentFailureType::UserAbort) {
        return response;
    }

    // Ask user for approval (hard requirement — human stays in control)
    bool approved = showRetryApprovalInPlanDialog(-1, fc);

    if (!approved) {
        return response;  // User declined, return original
    }

    // Attempt 2: Retry with corrected prompt (max 1 retry)
    m_failureStats.totalRetries++;
    std::string retryPrompt = buildRetryPrompt(fc, prompt);

    AgentResponse retryResponse = m_agenticBridge->ExecuteAgentCommand(retryPrompt);

    // Classify retry result
    auto retryFc = hookPostGeneration(retryResponse.content, prompt);

    if (retryFc.reason == AgentFailureType::None) {
        m_failureStats.successAfterRetry++;
        appendToOutput("[FailureDetector] Retry succeeded!",
                       "General", OutputSeverity::Info);

        recordEvent(AgentEventType::FailureCorrected, "",
                   prompt.substr(0, 256), "Corrected after 1 retry",
                   0, true, "",
                   "{\"originalFailure\":\"" + failureTypeString(fc.reason) + "\"}");
        return retryResponse;
    } else {
        appendToOutput("[FailureDetector] Retry still failed: " +
                       failureTypeString(retryFc.reason) +
                       ". Returning best attempt.",
                       "General", OutputSeverity::Error);
        return retryResponse;  // Return retry result even if imperfect
    }
}

// ============================================================================
// EXECUTE WITH RETRY — legacy API preserved for backward compatibility
// ============================================================================

AgentResponse Win32IDE::executeWithFailureDetection(const std::string& prompt) {
    // Phase 4B: Delegate to bounded retry
    return executeWithBoundedRetry(prompt);
}

// ============================================================================
// APPLY CORRECTION — kept for backward compatibility (used by legacy paths)
// ============================================================================

std::string Win32IDE::applyCorrectionStrategy(AgentFailureType failure,
                                               const std::string& originalPrompt,
                                               int retryAttempt) {
    // Phase 4B: Delegate to buildRetryPrompt via a temporary classification
    FailureClassification fc;
    fc.reason = failure;
    fc.confidence = 0.75f;
    return buildRetryPrompt(fc, originalPrompt);
}

// ============================================================================
// FAILURE TYPE STRING — expanded for Phase 4B types
// ============================================================================

std::string Win32IDE::failureTypeString(AgentFailureType type) const {
    switch (type) {
        case AgentFailureType::None:                return "None";
        case AgentFailureType::Refusal:             return "Refusal";
        case AgentFailureType::Hallucination:       return "Hallucination";
        case AgentFailureType::FormatViolation:     return "Format Violation";
        case AgentFailureType::InfiniteLoop:        return "Infinite Loop";
        case AgentFailureType::QualityDegradation:  return "Quality Degradation";
        case AgentFailureType::EmptyResponse:       return "Empty Response";
        case AgentFailureType::Timeout:             return "Timeout";
        case AgentFailureType::ToolError:           return "Tool Error";
        case AgentFailureType::InvalidOutput:       return "Invalid Output";
        case AgentFailureType::LowConfidence:       return "Low Confidence";
        case AgentFailureType::SafetyViolation:     return "Safety Violation";
        case AgentFailureType::UserAbort:           return "User Abort";
        default:                                    return "Unknown";
    }
}

// ============================================================================
// STATISTICS — expanded for Phase 4B types
// ============================================================================

std::string Win32IDE::getFailureDetectorStats() const {
    std::ostringstream oss;
    oss << "=== Failure Detector Statistics (Phase 4B) ===\r\n";
    oss << "Total Requests:        " << m_failureStats.totalRequests << "\r\n";
    oss << "Total Failures:        " << m_failureStats.totalFailures << "\r\n";
    oss << "Total Retries:         " << m_failureStats.totalRetries << "\r\n";
    oss << "Retries Declined:      " << m_failureStats.retriesDeclined << "\r\n";
    oss << "Success After Retry:   " << m_failureStats.successAfterRetry << "\r\n";
    oss << "---\r\n";
    oss << "Refusals:              " << m_failureStats.refusalCount << "\r\n";
    oss << "Hallucinations:        " << m_failureStats.hallucinationCount << "\r\n";
    oss << "Format Violations:     " << m_failureStats.formatViolationCount << "\r\n";
    oss << "Infinite Loops:        " << m_failureStats.infiniteLoopCount << "\r\n";
    oss << "Quality Degradations:  " << m_failureStats.qualityDegradationCount << "\r\n";
    oss << "Empty Responses:       " << m_failureStats.emptyResponseCount << "\r\n";
    oss << "Timeouts:              " << m_failureStats.timeoutCount << "\r\n";
    oss << "Tool Errors:           " << m_failureStats.toolErrorCount << "\r\n";
    oss << "Invalid Outputs:       " << m_failureStats.invalidOutputCount << "\r\n";
    oss << "Low Confidence:        " << m_failureStats.lowConfidenceCount << "\r\n";
    oss << "Safety Violations:     " << m_failureStats.safetyViolationCount << "\r\n";
    oss << "User Aborts:           " << m_failureStats.userAbortCount << "\r\n";

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
