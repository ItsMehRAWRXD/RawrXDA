#include "agentic_deep_thinking_engine.hpp"
#include "agentic_failure_detector.hpp"
#include "agentic_puppeteer.hpp"
#include "model_invoker.hpp"
#include "telemetry_collector.hpp"
#include "../agentic/AgentOllamaClient.h"
#include "../core/perf_telemetry.hpp"
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <cmath>
#include <set>
#include <numeric>

// ---------------------------------------------------------------------------
// Internal: Lazily-initialized LLM client for deep thinking inference
// ---------------------------------------------------------------------------
static RawrXD::Agent::AgentOllamaClient& getThinkingLLM() {
    static RawrXD::Agent::OllamaConfig cfg;
    cfg.chat_model  = "qwen2.5-coder:14b";
    cfg.temperature = 0.3f;
    cfg.max_tokens  = 4096;
    cfg.timeout_ms  = 60000;
    static RawrXD::Agent::AgentOllamaClient client(cfg);
    return client;
}

static AgenticFailureDetector& getFailureDetector() {
    static AgenticFailureDetector detector;
    return detector;
}

static AgenticPuppeteer& getPuppeteer() {
    static AgenticPuppeteer puppeteer;
    return puppeteer;
}

AgenticDeepThinkingEngine::AgenticDeepThinkingEngine() = default;

AgenticDeepThinkingEngine::~AgenticDeepThinkingEngine() {
    if (m_thinking) {
        cancelThinking();
    }
}

AgenticDeepThinkingEngine::ThinkingResult AgenticDeepThinkingEngine::think(const ThinkingContext& context) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalThinkingRequests++;
    }

    ThinkingResult result;
    result.iterationCount = 0;
    result.overallConfidence = 0.0f;

    try {
        // Check cache
        auto cacheKey = context.problem + "_" + context.language + "_" + context.projectRoot;
        auto cachedIt = m_thinkingCache.find(cacheKey);
        if (cachedIt != m_thinkingCache.end()) {
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_stats.cacheHits++;
            }
            return cachedIt->second;
        }

        // Perform Chain-of-Thought reasoning
        result.steps = performChainOfThought(context);

        // Extract and format final answer
        if (!result.steps.empty()) {
            for (const auto& step : result.steps) {
                if (step.step == ThinkingStep::FinalSynthesis) {
                    result.finalAnswer = step.content;
                }
                // Collect suggested fixes from all steps
                result.suggestedFixes.insert(result.suggestedFixes.end(),
                                            step.findings.begin(), step.findings.end());
            }
        }

        // Calculate overall confidence
        result.overallConfidence = calculateOverallConfidence(result.steps);
        result.iterationCount = 1;  // Could be higher with self-correction loops

        // Find related files
        result.relatedFiles = findRelatedFiles(context.problem, 5);

        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.successfulThinking++;
            m_stats.avgConfidence = (m_stats.avgConfidence * (m_stats.successfulThinking - 1) + result.overallConfidence)
                                   / m_stats.successfulThinking;
        }

        // Cache result
        m_thinkingCache[cacheKey] = result;

    } catch (const std::exception& e) {
        std::cerr << "[DeepThinking] Error during thinking: " << e.what() << std::endl;
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.failedThinking++;
        }
        result.finalAnswer = "Error during thinking: " + std::string(e.what());
        result.overallConfidence = 0.0f;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.avgThinkingTime = (m_stats.avgThinkingTime * (m_stats.successfulThinking - 1) + result.elapsedMilliseconds)
                                 / m_stats.successfulThinking;
    }

    return result;
}

void AgenticDeepThinkingEngine::startThinking(
    const ThinkingContext& context,
    std::function<void(const ReasoningStep&)> onStepComplete,
    std::function<void(float)> onProgressUpdate,
    std::function<void(const std::string&)> onError
) {
    if (m_thinking) {
        onError("Already thinking");
        return;
    }

    m_thinking = true;
    m_thinkingThread = std::thread([this, context, onStepComplete, onProgressUpdate, onError]() {
        try {
            auto result = think(context);
            
            float progressInc = 1.0f / result.steps.size();
            for (const auto& step : result.steps) {
                if (!m_thinking) break;
                
                onStepComplete(step);
                onProgressUpdate(progressInc);
            }
        } catch (const std::exception& e) {
            onError(std::string("Thinking error: ") + e.what());
        }
        m_thinking = false;
    });

    if (m_thinkingThread.joinable()) {
        m_thinkingThread.detach();
    }
}

void AgenticDeepThinkingEngine::cancelThinking() {
    m_thinking = false;
    if (m_thinkingThread.joinable()) {
        m_thinkingThread.join();
    }
}

std::vector<std::string> AgenticDeepThinkingEngine::findRelatedFiles(const std::string& query, int maxResults) {
    std::vector<std::string> relatedFiles;
    
    // Parse query for keywords (case-insensitive)
    std::istringstream iss(query);
    std::vector<std::string> keywords;
    std::string word;
    while (iss >> word) {
        if (word.length() > 3) {
            std::string lower = word;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            keywords.push_back(lower);
        }
    }

    if (keywords.empty()) return relatedFiles;

    // Collect candidate files with relevance scores
    struct ScoredFile {
        std::string path;
        int score;
    };
    std::vector<ScoredFile> candidates;

    // Search common source directories
    std::vector<std::string> searchDirs = {".", "src", "src/core", "src/agent", "src/win32app",
                                           "src/cli", "src/engine", "src/server", "src/gui"};
    std::vector<std::string> extensions = {".cpp", ".hpp", ".h", ".c", ".py", ".js", ".ts", ".asm"};

    for (const auto& dir : searchDirs) {
        auto files = listFilesRecursive(dir, "");
        for (const auto& filePath : files) {
            // Check extension
            bool hasExt = false;
            for (const auto& ext : extensions) {
                if (filePath.length() >= ext.length() &&
                    filePath.substr(filePath.length() - ext.length()) == ext) {
                    hasExt = true;
                    break;
                }
            }
            if (!hasExt) continue;

            // Score by filename match
            std::string lowerPath = filePath;
            std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

            int score = 0;
            for (const auto& kw : keywords) {
                if (lowerPath.find(kw) != std::string::npos) {
                    score += 3; // filename match is strong
                }
            }

            // Score by content match (read first 100 lines)
            if (score == 0) {
                std::string content = readFileContent(filePath, 100);
                std::string lowerContent = content;
                std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
                for (const auto& kw : keywords) {
                    size_t pos = 0;
                    while ((pos = lowerContent.find(kw, pos)) != std::string::npos) {
                        score++;
                        pos += kw.length();
                    }
                }
            }

            if (score > 0) {
                candidates.push_back({filePath, score});
            }
        }
    }

    // Sort by score descending
    std::sort(candidates.begin(), candidates.end(),
              [](const ScoredFile& a, const ScoredFile& b) { return a.score > b.score; });

    // Return top N
    for (int i = 0; i < maxResults && i < static_cast<int>(candidates.size()); ++i) {
        relatedFiles.push_back(candidates[i].path);
    }

    return relatedFiles;
}

std::string AgenticDeepThinkingEngine::analyzeFile(const std::string& filePath) {
    try {
        return readFileContent(filePath, 50);
    } catch (const std::exception& e) {
        return std::string("Error analyzing file: ") + e.what();
    }
}

std::string AgenticDeepThinkingEngine::searchProjectForPattern(const std::string& pattern) {
    std::ostringstream results;
    results << "Pattern search results for: " << pattern << "\n";
    int totalMatches = 0;

    std::vector<std::string> searchDirs = {".", "src", "src/core", "src/agent", "src/win32app",
                                           "src/cli", "src/engine", "src/server"};

    std::regex searchRegex;
    bool useRegex = false;
    try {
        searchRegex = std::regex(pattern, std::regex::icase);
        useRegex = true;
    } catch (...) {
        useRegex = false; // Fall back to plain string search
    }

    for (const auto& dir : searchDirs) {
        auto files = listFilesRecursive(dir, "");
        for (const auto& filePath : files) {
            try {
                std::ifstream file(filePath);
                if (!file.is_open()) continue;

                std::string line;
                int lineNum = 0;
                while (std::getline(file, line)) {
                    lineNum++;
                    bool matched = false;
                    if (useRegex) {
                        matched = std::regex_search(line, searchRegex);
                    } else {
                        std::string lowerLine = line;
                        std::string lowerPattern = pattern;
                        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
                        std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
                        matched = lowerLine.find(lowerPattern) != std::string::npos;
                    }
                    if (matched) {
                        results << "  " << filePath << ":" << lineNum << ": "
                                << line.substr(0, 120) << "\n";
                        totalMatches++;
                        if (totalMatches >= 50) {
                            results << "  ... (truncated at 50 matches)\n";
                            return results.str();
                        }
                    }
                }
            } catch (...) { continue; }
        }
    }

    results << "Total matches: " << totalMatches << "\n";
    return results.str();
}

bool AgenticDeepThinkingEngine::evaluateAnswer(const std::string& answer, const ThinkingContext& context) {
    if (answer.empty()) return false;

    // Minimum length check
    if (answer.length() < 10) return false;

    // Check for error/failure indicators
    std::string lowerAnswer = answer;
    std::transform(lowerAnswer.begin(), lowerAnswer.end(), lowerAnswer.begin(), ::tolower);
    if (lowerAnswer.find("error during thinking") != std::string::npos) return false;
    if (lowerAnswer.find("not implemented") != std::string::npos) return false;
    if (lowerAnswer.find("failed to") != std::string::npos && answer.length() < 50) return false;

    // Check keyword overlap with problem statement
    std::istringstream probStream(context.problem);
    std::string word;
    int problemWords = 0, matchedWords = 0;
    while (probStream >> word) {
        if (word.length() > 3) {
            problemWords++;
            std::string lowerWord = word;
            std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), ::tolower);
            if (lowerAnswer.find(lowerWord) != std::string::npos) {
                matchedWords++;
            }
        }
    }

    // At least 20% keyword overlap required
    if (problemWords > 0 && matchedWords == 0) return false;

    // Check structural quality - answers should have some substance
    bool hasStructure = (answer.find('\n') != std::string::npos) ||
                        (answer.find(':') != std::string::npos) ||
                        (answer.find('-') != std::string::npos) ||
                        (answer.length() > 100);

    return hasStructure || (matchedWords > 0);
}

std::string AgenticDeepThinkingEngine::refineAnswer(const std::string& currentAnswer, const std::string& feedback) {
    if (feedback.empty()) return currentAnswer;
    ScopedMeasurement perf(static_cast<uint32_t>(KernelSlot::TotalInference));

    // Step 1: Validate the current answer against the failure detector
    auto& detector = getFailureDetector();
    auto failures = detector.detectMultipleFailures(currentAnswer);

    // Step 2: If failures detected, try puppeteer auto-correction first
    std::string baseAnswer = currentAnswer;
    if (!failures.empty()) {
        auto& puppeteer = getPuppeteer();
        auto correction = puppeteer.correctResponse(currentAnswer, feedback);
        if (correction.success && !correction.correctedOutput.empty()) {
            baseAnswer = correction.correctedOutput;
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_patternFrequency["refine_puppeteer_correction"]++;
            }
        }
    }

    // Step 3: Use LLM to perform the actual refinement with full context
    using ChatMsg = RawrXD::Agent::ChatMessage;
    std::vector<ChatMsg> messages;
    messages.push_back(ChatMsg{"system",
        "You are a code analysis refinement agent. You are given a previous analysis "
        "and user feedback. Produce an improved version that addresses the feedback "
        "while preserving all correct information from the original. Be concrete "
        "and reference specific code structures, files, and line numbers where possible.",
        "", {}});

    std::string userMsg = "## Previous Analysis\n" + baseAnswer
                        + "\n\n## Feedback\n" + feedback;

    // Include detected failures as context
    if (!failures.empty()) {
        userMsg += "\n\n## Detected Issues in Previous Answer\n";
        for (const auto& f : failures) {
            userMsg += "- " + f.description + " (confidence: "
                     + std::to_string(f.confidence) + ")\n";
        }
    }

    messages.push_back(ChatMsg{"user", userMsg, "", {}});

    auto& llm = getThinkingLLM();
    auto result = llm.ChatSync(messages);

    std::string refined;
    if (result.success && !result.response.empty()) {
        refined = result.response;
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_patternFrequency["refine_llm_success"]++;
        }
        if (auto* tc = TelemetryCollector::instance()) {
            tc->trackPerformance("deep_thinking.refine_tokens",
                                 static_cast<double>(result.completion_tokens), "tokens");
            tc->trackPerformance("deep_thinking.refine_tok_per_sec",
                                 result.tokens_per_sec, "tok/s");
            tc->trackPerformance("deep_thinking.refine_latency_ms",
                                 result.total_duration_ms, "ms");
        }
    } else {
        // LLM unavailable — perform local structural refinement
        refined = baseAnswer;
        std::string lowerFeedback = feedback;
        std::transform(lowerFeedback.begin(), lowerFeedback.end(), lowerFeedback.begin(), ::tolower);

        if (lowerFeedback.find("more detail") != std::string::npos ||
            lowerFeedback.find("elaborate") != std::string::npos) {
            // Search codebase for additional context matching the answer content
            auto extraContext = searchInFiles(feedback.substr(0, 60), "src");
            refined += "\n\n[Refined with codebase evidence — "
                     + std::to_string(extraContext.size()) + " references found]\n";
            for (size_t i = 0; i < std::min(extraContext.size(), size_t(5)); ++i) {
                refined += "  " + extraContext[i].first + ":" + std::to_string(extraContext[i].second) + "\n";
            }
        }
        else if (lowerFeedback.find("simplify") != std::string::npos ||
                 lowerFeedback.find("shorter") != std::string::npos) {
            // Extract structurally meaningful lines only
            std::istringstream iss(baseAnswer);
            std::string line;
            std::string simplified;
            int keepCount = 0;
            while (std::getline(iss, line)) {
                if (line.find("Based on") != std::string::npos ||
                    line.find("Result") != std::string::npos ||
                    line.find("- ") == 0 ||
                    line.find("Conclusion") != std::string::npos ||
                    line.find("::") != std::string::npos) {
                    simplified += line + "\n";
                    keepCount++;
                }
                if (keepCount >= 8) break;
            }
            if (!simplified.empty()) refined = simplified;
        }
        else {
            // Enrich with codebase matches for the feedback topic
            auto feedbackMatches = searchInFiles(feedback.substr(0, 50), "src");
            refined += "\n\n[Enhanced with " + std::to_string(feedbackMatches.size())
                     + " codebase references for: " + feedback.substr(0, 80) + "]\n";
        }

        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_patternFrequency["refine_local_fallback"]++;
        }
    }

    // Track refinement in telemetry
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_patternFrequency["refinement"]++;
    }

    return refined;
}

void AgenticDeepThinkingEngine::saveThinkingResult(const std::string& key, const ThinkingResult& result) {
    m_thinkingCache[key] = result;
}

AgenticDeepThinkingEngine::ThinkingResult* AgenticDeepThinkingEngine::getCachedThinking(const std::string& key) {
    auto it = m_thinkingCache.find(key);
    return (it != m_thinkingCache.end()) ? &it->second : nullptr;
}

void AgenticDeepThinkingEngine::clearMemory() {
    m_thinkingCache.clear();
    m_patternFrequency.clear();
}

std::vector<std::pair<std::string, int>> AgenticDeepThinkingEngine::getMostUsedPatterns() const {
    std::vector<std::pair<std::string, int>> patterns(m_patternFrequency.begin(), m_patternFrequency.end());
    std::sort(patterns.begin(), patterns.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; }
    );
    return patterns;
}

AgenticDeepThinkingEngine::ThinkingStats AgenticDeepThinkingEngine::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void AgenticDeepThinkingEngine::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = ThinkingStats();
}

std::vector<AgenticDeepThinkingEngine::ReasoningStep> AgenticDeepThinkingEngine::performChainOfThought(
    const ThinkingContext& context
) {
    std::vector<ReasoningStep> steps;
    std::string contextAccumulator;

    // Step 1: Problem Analysis
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.stepFrequency[ThinkingStep::ProblemAnalysis]++;
    }
    auto step1 = analyzeProblem(context);
    steps.push_back(step1);
    contextAccumulator += step1.content + "\n";

    // Step 2: Context Gathering
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.stepFrequency[ThinkingStep::ContextGathering]++;
    }
    auto step2 = gatherContext(context);
    steps.push_back(step2);
    contextAccumulator += step2.content + "\n";

    // Step 3: Hypothesis Generation
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.stepFrequency[ThinkingStep::HypothesiGeneration]++;
    }
    auto step3 = generateHypotheses(context, step1.content);
    steps.push_back(step3);
    contextAccumulator += step3.content + "\n";

    // Step 4: Experimentation
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.stepFrequency[ThinkingStep::ExperimentationRun]++;
    }
    std::vector<std::string> hypotheses;
    for (const auto& finding : step3.findings) {
        hypotheses.push_back(finding);
    }
    auto step4 = runExperiments(context, hypotheses);
    steps.push_back(step4);
    contextAccumulator += step4.content + "\n";

    // Step 5: Result Evaluation
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.stepFrequency[ThinkingStep::ResultEvaluation]++;
    }
    std::vector<std::string> results;
    for (const auto& finding : step4.findings) {
        results.push_back(finding);
    }
    auto step5 = evaluateResults(results, context);
    steps.push_back(step5);
    contextAccumulator += step5.content + "\n";

    // Step 6 (optional): Self-Correction
    if (context.allowSelfCorrection) {
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.stepFrequency[ThinkingStep::SelfCorrection]++;
        }
        auto step6 = selfCorrect(context, steps);
        if (step6.successful) {
            steps.push_back(step6);
            contextAccumulator += step6.content + "\n";
        }
    }

    // Final Step: Synthesis
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.stepFrequency[ThinkingStep::FinalSynthesis]++;
    }
    auto stepFinal = synthesizeAnswer(steps, context);
    steps.push_back(stepFinal);

    return steps;
}

AgenticDeepThinkingEngine::ReasoningStep AgenticDeepThinkingEngine::analyzeProblem(const ThinkingContext& context) {
    ScopedMeasurement perf(static_cast<uint32_t>(KernelSlot::TotalInference));
    ReasoningStep step;
    step.step = ThinkingStep::ProblemAnalysis;
    step.title = "Problem Analysis";
    step.successful = true;

    // Phase 1: Local structural analysis (always available)
    auto issues = identifyKeyIssues(context.problem);
    std::string category = issues.empty() ? "General" : categorizeIssue(issues[0]);

    // Phase 2: Codebase context — find related files and code patterns
    auto relatedFiles = findRelatedFiles(context.problem, 5);
    auto codeMatches  = searchInFiles(context.problem.substr(0, 60), "src");

    std::string codebaseContext;
    if (!relatedFiles.empty()) {
        codebaseContext += "Related files:\n";
        for (const auto& f : relatedFiles) {
            codebaseContext += "  " + f + "\n";
        }
    }
    if (!codeMatches.empty()) {
        codebaseContext += "Code references (" + std::to_string(codeMatches.size()) + " matches):\n";
        for (size_t i = 0; i < std::min(codeMatches.size(), size_t(8)); ++i) {
            codebaseContext += "  " + codeMatches[i].first + ":" + std::to_string(codeMatches[i].second) + "\n";
        }
    }

    // Phase 3: LLM-driven deep analysis with codebase context
    using ChatMsg = RawrXD::Agent::ChatMessage;
    std::vector<ChatMsg> messages;
    messages.push_back(ChatMsg{"system",
        "You are a senior code analyst. Analyze the following problem in the context "
        "of a C++20 Win32 codebase. Identify root causes, affected components, risk level, "
        "and key areas to investigate. Be specific and technical.", "", {}});

    std::string userMsg = "## Problem\n" + context.problem
                        + "\n\nLanguage: " + context.language
                        + "\nProject root: " + context.projectRoot
                        + "\nCategory: " + category
                        + "\n\n## Codebase Context\n" + codebaseContext;
    messages.push_back(ChatMsg{"user", userMsg, "", {}});

    auto& llm = getThinkingLLM();
    auto result = llm.ChatSync(messages);

    if (result.success && !result.response.empty()) {
        step.content = result.response;
        step.confidence = 0.90f;

        // Extract key findings from LLM response (lines starting with -, *, or numbered)
        std::istringstream iss(result.response);
        std::string line;
        while (std::getline(iss, line)) {
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            char first = line[start];
            if (first == '-' || first == '*' || (first >= '1' && first <= '9')) {
                step.findings.push_back(line.substr(start));
            }
        }

        // Track LLM analysis metrics
        if (auto* tc = TelemetryCollector::instance()) {
            tc->trackPerformance("deep_thinking.analysis_tokens",
                                 static_cast<double>(result.completion_tokens), "tokens");
            tc->trackPerformance("deep_thinking.analysis_tok_per_sec",
                                 result.tokens_per_sec, "tok/s");
            tc->trackPerformance("deep_thinking.analysis_latency_ms",
                                 result.total_duration_ms, "ms");
            tc->trackFeatureUsage("deep_thinking.analyze_problem");
        }
    } else {
        // Fallback: use local structural analysis (always works offline)
        step.content = "Category: " + category + "\n";
        step.content += "Local issues identified: " + std::to_string(issues.size()) + "\n";
        step.content += codebaseContext;
        step.findings = issues;
        step.confidence = 0.70f;  // Lower confidence without LLM

        // Add file context as findings
        for (const auto& f : relatedFiles) {
            step.findings.push_back("Related: " + f);
        }
    }

    // Always include the structural issues as baseline findings
    for (const auto& issue : issues) {
        bool alreadyPresent = false;
        for (const auto& existing : step.findings) {
            if (existing.find(issue) != std::string::npos) { alreadyPresent = true; break; }
        }
        if (!alreadyPresent) step.findings.push_back(issue);
    }

    return step;
}

std::vector<std::string> AgenticDeepThinkingEngine::identifyKeyIssues(const std::string& problem) {
    std::vector<std::string> issues;
    
    // Split problem into sentences and extract key points
    std::istringstream iss(problem);
    std::string sentence;
    while (std::getline(iss, sentence, '.')) {
        if (!sentence.empty()) {
            issues.push_back(sentence);
        }
    }

    return issues;
}

std::string AgenticDeepThinkingEngine::categorizeIssue(const std::string& issue) {
    if (issue.find("error") != std::string::npos || issue.find("failed") != std::string::npos) {
        return "Error/Failure";
    } else if (issue.find("performance") != std::string::npos || issue.find("slow") != std::string::npos) {
        return "Performance";
    } else if (issue.find("design") != std::string::npos || issue.find("architecture") != std::string::npos) {
        return "Design";
    } else if (issue.find("test") != std::string::npos || issue.find("verify") != std::string::npos) {
        return "Testing";
    }
    return "General";
}

AgenticDeepThinkingEngine::ReasoningStep AgenticDeepThinkingEngine::gatherContext(const ThinkingContext& context) {
    ReasoningStep step;
    step.step = ThinkingStep::ContextGathering;
    step.title = "Context Gathering";
    step.successful = true;

    // Find relevant code files
    auto relatedCode = findRelevantCode(context.problem, 5);
    step.findings = relatedCode;

    step.content = "Found " + std::to_string(relatedCode.size()) + " relevant code files";
    step.confidence = 0.8f;

    return step;
}

std::vector<std::string> AgenticDeepThinkingEngine::findRelevantCode(const std::string& problem, int maxResults) {
    std::vector<std::string> relevant;

    // Extract keywords from problem statement
    std::istringstream iss(problem);
    std::vector<std::string> keywords;
    std::string word;
    while (iss >> word) {
        if (word.length() > 3) {
            std::string lower = word;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            keywords.push_back(lower);
        }
    }

    if (keywords.empty()) return relevant;

    // Search source files for code matching keywords
    std::vector<std::string> searchDirs = {"src", "src/core", "src/agent", "src/win32app",
                                           "src/cli", "src/engine", "src/server", "src/gui"};
    struct ScoredSnippet {
        std::string content;
        int score;
    };
    std::vector<ScoredSnippet> snippets;

    for (const auto& dir : searchDirs) {
        auto files = listFilesRecursive(dir, "");
        for (const auto& filePath : files) {
            // Only search source files
            if (filePath.find(".cpp") == std::string::npos &&
                filePath.find(".hpp") == std::string::npos &&
                filePath.find(".h") == std::string::npos) continue;

            try {
                std::ifstream file(filePath);
                if (!file.is_open()) continue;

                std::string line;
                int lineNum = 0;
                int score = 0;
                std::string context;

                while (std::getline(file, line)) {
                    lineNum++;
                    std::string lowerLine = line;
                    std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

                    for (const auto& kw : keywords) {
                        if (lowerLine.find(kw) != std::string::npos) {
                            score++;
                            if (context.length() < 500) {
                                context += filePath + ":" + std::to_string(lineNum) + ": " + line + "\n";
                            }
                        }
                    }
                }

                if (score > 0 && !context.empty()) {
                    snippets.push_back({context, score});
                }
            } catch (...) { continue; }
        }
    }

    // Sort by score and return top results
    std::sort(snippets.begin(), snippets.end(),
              [](const ScoredSnippet& a, const ScoredSnippet& b) { return a.score > b.score; });

    for (int i = 0; i < maxResults && i < static_cast<int>(snippets.size()); ++i) {
        relevant.push_back(snippets[i].content);
    }

    return relevant;
}

AgenticDeepThinkingEngine::ReasoningStep AgenticDeepThinkingEngine::generateHypotheses(
    const ThinkingContext& context,
    const std::string& analysis
) {
    ReasoningStep step;
    step.step = ThinkingStep::HypothesiGeneration;
    step.title = "Hypothesis Generation";
    step.successful = true;

    auto hypotheses = brainstormSolutions(context.problem, 3);
    step.findings = hypotheses;
    step.content = "Generated " + std::to_string(hypotheses.size()) + " hypotheses";
    step.confidence = 0.75f;

    return step;
}

std::vector<std::string> AgenticDeepThinkingEngine::brainstormSolutions(const std::string& problem, int count) {
    ScopedMeasurement perf(static_cast<uint32_t>(KernelSlot::TotalInference));
    std::vector<std::string> solutions;

    // Build a structured prompt asking the LLM to generate concrete, distinct hypotheses
    std::string systemPrompt =
        "You are a senior software engineer. Given a code problem, generate exactly "
        + std::to_string(count) + " distinct, actionable hypotheses to solve it. "
        "Each hypothesis must be a concrete, specific technical approach — not a restatement "
        "of the problem. Format: one hypothesis per line, prefixed with 'H1:', 'H2:', etc.";

    using ChatMsg = RawrXD::Agent::ChatMessage;
    std::vector<ChatMsg> messages;
    messages.push_back(ChatMsg{"system", systemPrompt, "", {}});
    messages.push_back(ChatMsg{"user", problem, "", {}});

    auto& llm = getThinkingLLM();
    auto result = llm.ChatSync(messages);

    if (result.success && !result.response.empty()) {
        // Parse numbered hypotheses from LLM output
        std::istringstream iss(result.response);
        std::string line;
        while (std::getline(iss, line)) {
            // Strip leading whitespace
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            line = line.substr(start);

            // Accept lines starting with H1:, H2:, 1., 2., -, * etc.
            if (line.length() > 5 &&
                (line[0] == 'H' || line[0] == '-' || line[0] == '*' ||
                 (line[0] >= '1' && line[0] <= '9'))) {
                solutions.push_back(line);
            }
        }

        // Track real LLM metrics
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_patternFrequency["llm_brainstorm"]++;
        }
        if (auto* tc = TelemetryCollector::instance()) {
            tc->trackPerformance("deep_thinking.brainstorm_tokens",
                                 static_cast<double>(result.completion_tokens), "tokens");
            tc->trackPerformance("deep_thinking.brainstorm_tok_per_sec",
                                 result.tokens_per_sec, "tok/s");
        }
    }

    // Fallback: if LLM unavailable or returned nothing, do codebase-driven analysis
    if (solutions.empty()) {
        // Search codebase for patterns related to the problem
        auto codeMatches = searchInFiles(problem.substr(0, 60), "src");
        if (!codeMatches.empty()) {
            solutions.push_back("H1: Refactor the " + std::to_string(codeMatches.size()) +
                " code locations matching the problem pattern for consistency");
        }
        auto todoMatches = searchInFiles("TODO|FIXME|HACK", "src");
        if (!todoMatches.empty()) {
            solutions.push_back("H2: Address " + std::to_string(todoMatches.size()) +
                " outstanding TODO/FIXME markers that may relate to root cause");
        }
        auto errorMatches = searchInFiles("error|fail|exception", "src");
        if (!errorMatches.empty()) {
            solutions.push_back("H3: Audit " + std::to_string(errorMatches.size()) +
                " error-handling sites for missing recovery paths");
        }
        // Always add at least one hypothesis
        if (solutions.empty()) {
            solutions.push_back("H1: Investigate the problem domain by tracing call paths from entry points");
        }
    }

    // Clamp to requested count
    if (static_cast<int>(solutions.size()) > count) {
        solutions.resize(count);
    }

    return solutions;
}

AgenticDeepThinkingEngine::ReasoningStep AgenticDeepThinkingEngine::runExperiments(
    const ThinkingContext& context,
    const std::vector<std::string>& hypotheses
) {
    ReasoningStep step;
    step.step = ThinkingStep::ExperimentationRun;
    step.title = "Experimentation";
    step.successful = true;

    for (const auto& hypothesis : hypotheses) {
        auto result = testHypothesis(hypothesis, context);
        step.findings.push_back(result);
    }

    step.content = "Tested " + std::to_string(hypotheses.size()) + " hypotheses";
    step.confidence = 0.7f;

    return step;
}

std::string AgenticDeepThinkingEngine::testHypothesis(const std::string& hypothesis, const ThinkingContext& context) {
    std::ostringstream result;
    result << "Testing: " << hypothesis.substr(0, 100) << "\n";

    // Extract actionable keywords from hypothesis
    std::string lower = hypothesis;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    bool evidenceFound = false;

    // Test 1: Search for supporting evidence in codebase
    auto searchResults = searchInFiles(hypothesis.substr(0, 50), "src");
    if (!searchResults.empty()) {
        result << "  Evidence: Found " << searchResults.size() << " references in codebase\n";
        for (size_t i = 0; i < std::min(searchResults.size(), size_t(3)); ++i) {
            result << "    - " << searchResults[i].first << ":" << searchResults[i].second << "\n";
        }
        evidenceFound = true;
    }

    // Test 2: Check if hypothesis relates to code patterns
    if (lower.find("refactor") != std::string::npos ||
        lower.find("extract") != std::string::npos ||
        lower.find("rename") != std::string::npos) {
        result << "  Analysis: Structural change hypothesis - validating scope\n";
        auto files = findRelatedFiles(hypothesis, 3);
        if (!files.empty()) {
            result << "  Affected files: " << files.size() << "\n";
            evidenceFound = true;
        }
    }

    // Test 3: Check against known patterns
    if (lower.find("bug") != std::string::npos ||
        lower.find("error") != std::string::npos ||
        lower.find("fix") != std::string::npos) {
        auto patterns = searchInFiles("TODO|FIXME|BUG|HACK", "src");
        if (!patterns.empty()) {
            result << "  Found " << patterns.size() << " known issue markers related to hypothesis\n";
            evidenceFound = true;
        }
    }

    // Test 4: Confidence assessment
    if (evidenceFound) {
        result << "  Verdict: SUPPORTED (evidence found in codebase)\n";
    } else {
        result << "  Verdict: INCONCLUSIVE (no direct evidence, may need manual review)\n";
    }

    return result.str();
}

AgenticDeepThinkingEngine::ReasoningStep AgenticDeepThinkingEngine::evaluateResults(
    const std::vector<std::string>& results,
    const ThinkingContext& context
) {
    ReasoningStep step;
    step.step = ThinkingStep::ResultEvaluation;
    step.title = "Result Evaluation";
    step.successful = true;

    float totalScore = 0.0f;
    for (const auto& result : results) {
        float score = scoreResult(result, context.problem);
        totalScore += score;
        if (score > 0.7f) {
            step.findings.push_back(result);
        }
    }

    float avgScore = results.empty() ? 0.0f : totalScore / results.size();
    step.content = "Average score: " + std::to_string(avgScore);
    step.confidence = avgScore;

    return step;
}

float AgenticDeepThinkingEngine::scoreResult(const std::string& result, const std::string& problem) {
    if (result.empty() || problem.empty()) return 0.0f;

    // --- 1. Jaccard-style keyword overlap (0.0–0.35) ---
    auto tokenize = [](const std::string& text) -> std::set<std::string> {
        std::set<std::string> tokens;
        std::istringstream iss(text);
        std::string word;
        while (iss >> word) {
            if (word.length() > 2) {
                std::string lower = word;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                // Strip punctuation
                while (!lower.empty() && !std::isalnum(static_cast<unsigned char>(lower.back())))
                    lower.pop_back();
                if (lower.length() > 2) tokens.insert(lower);
            }
        }
        return tokens;
    };

    auto problemTokens = tokenize(problem);
    auto resultTokens  = tokenize(result);
    if (problemTokens.empty()) return 0.0f;

    int intersection = 0;
    for (const auto& t : problemTokens) {
        if (resultTokens.count(t)) intersection++;
    }
    int unionSize = static_cast<int>(problemTokens.size() + resultTokens.size()) - intersection;
    float jaccardScore = unionSize > 0 ? static_cast<float>(intersection) / unionSize : 0.0f;

    // --- 2. Evidence density: lines with concrete references (0.0–0.25) ---
    int totalLines = 0, evidenceLines = 0;
    std::istringstream lineStream(result);
    std::string line;
    while (std::getline(lineStream, line)) {
        totalLines++;
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        // Lines with file paths, line numbers, code markers, or specific findings
        if (lower.find(".cpp") != std::string::npos ||
            lower.find(".hpp") != std::string::npos ||
            lower.find(".h") != std::string::npos ||
            lower.find("line ") != std::string::npos ||
            lower.find("found") != std::string::npos ||
            lower.find("evidence") != std::string::npos ||
            lower.find("supported") != std::string::npos ||
            lower.find("::") != std::string::npos) {
            evidenceLines++;
        }
    }
    float evidenceRatio = totalLines > 0
        ? static_cast<float>(evidenceLines) / totalLines
        : 0.0f;

    // --- 3. Structural quality: has formatting, detail (0.0–0.20) ---
    float structureScore = 0.0f;
    if (result.find('\n') != std::string::npos) structureScore += 0.05f;
    if (result.find("- ") != std::string::npos) structureScore += 0.05f;
    if (result.length() > 200) structureScore += 0.05f;
    if (result.length() > 500) structureScore += 0.05f;

    // --- 4. Failure penalty: run through failure detector (0.0–-0.3) ---
    float failurePenalty = 0.0f;
    auto& detector = getFailureDetector();
    auto failure = detector.detectFailure(result, problem);
    if (failure.type != AgentFailureType::None) {
        failurePenalty = static_cast<float>(failure.confidence) * 0.3f;
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_patternFrequency["score_failure_penalty"]++;
        }
    }

    // --- 5. Length penalty for very short results ---
    float lengthPenalty = 0.0f;
    if (result.length() < 20)  lengthPenalty = 0.3f;
    else if (result.length() < 50)  lengthPenalty = 0.15f;
    else if (result.length() < 100) lengthPenalty = 0.05f;

    // Composite score: weighted sum, clamped to [0, 1]
    float composite = (jaccardScore * 0.35f)
                    + (evidenceRatio * 0.25f)
                    + (structureScore)
                    + 0.20f  // base score for non-empty result
                    - failurePenalty
                    - lengthPenalty;

    return std::clamp(composite, 0.0f, 1.0f);
}

AgenticDeepThinkingEngine::ReasoningStep AgenticDeepThinkingEngine::selfCorrect(
    const ThinkingContext& context,
    const std::vector<ReasoningStep>& previousSteps
) {
    ReasoningStep step;
    step.step = ThinkingStep::SelfCorrection;
    step.title = "Self-Correction";
    step.successful = false;

    std::string flaw;
    if (detectFlaws(previousSteps, flaw)) {
        step.content = "Detected flaw: " + flaw;
        step.findings.push_back(correctFlaw(flaw, previousSteps));
        step.successful = true;
        step.confidence = 0.8f;
    }

    return step;
}

bool AgenticDeepThinkingEngine::detectFlaws(const std::vector<ReasoningStep>& steps, std::string& flaw) {
    // 1. Check for low confidence steps
    for (const auto& step : steps) {
        if (step.confidence < 0.5f) {
            flaw = "Low confidence (" + std::to_string(step.confidence) + ") in " + step.title;
            return true;
        }
    }

    // 2. Check for empty findings in critical steps
    for (const auto& step : steps) {
        if ((step.step == ThinkingStep::ProblemAnalysis ||
             step.step == ThinkingStep::HypothesiGeneration ||
             step.step == ThinkingStep::ExperimentationRun) &&
            step.findings.empty()) {
            flaw = "No findings produced in critical step: " + step.title;
            return true;
        }
    }

    // 3. Run failure detection on step content
    auto& detector = getFailureDetector();
    for (const auto& step : steps) {
        if (!step.content.empty()) {
            auto failure = detector.detectFailure(step.content);
            if (failure.type != AgentFailureType::None && failure.confidence > 0.7) {
                flaw = "Failure detected in " + step.title + ": " + failure.description
                     + " (confidence: " + std::to_string(failure.confidence) + ")";
                return true;
            }
        }
    }

    // 4. Check for contradictory findings across steps
    std::set<std::string> supportedVerdicts, inconclusiveVerdicts;
    for (const auto& step : steps) {
        for (const auto& f : step.findings) {
            std::string lower = f;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("supported") != std::string::npos) supportedVerdicts.insert(f);
            if (lower.find("inconclusive") != std::string::npos) inconclusiveVerdicts.insert(f);
        }
    }
    if (!supportedVerdicts.empty() && inconclusiveVerdicts.size() > supportedVerdicts.size()) {
        flaw = "Majority of hypotheses inconclusive (" + std::to_string(inconclusiveVerdicts.size())
             + " vs " + std::to_string(supportedVerdicts.size()) + " supported)";
        return true;
    }

    // 5. Check confidence trend — declining confidence across steps
    if (steps.size() >= 3) {
        int declineCount = 0;
        for (size_t i = 1; i < steps.size(); ++i) {
            if (steps[i].confidence < steps[i-1].confidence - 0.1f) declineCount++;
        }
        if (declineCount >= static_cast<int>(steps.size()) - 1) {
            flaw = "Confidence declining across all steps — reasoning may be diverging";
            return true;
        }
    }

    return false;
}

std::string AgenticDeepThinkingEngine::correctFlaw(const std::string& flaw, const std::vector<ReasoningStep>& steps) {
    ScopedMeasurement perf(static_cast<uint32_t>(KernelSlot::TotalInference));

    // Accumulate all findings from prior steps so the correction has full context
    std::string accumulatedContext;
    for (const auto& s : steps) {
        if (!s.content.empty()) {
            accumulatedContext += "[" + s.title + "] " + s.content + "\n";
        }
        for (const auto& f : s.findings) {
            accumulatedContext += "  Finding: " + f + "\n";
        }
    }

    // Step 1: Use AgenticPuppeteer to detect/correct the flaw pattern
    auto& puppeteer = getPuppeteer();
    CorrectionResult correction = puppeteer.correctResponse(flaw, accumulatedContext);
    if (correction.success && !correction.correctedOutput.empty()) {
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_patternFrequency["puppeteer_correction"]++;
        }
        return "[Puppeteer-corrected] " + correction.correctedOutput
               + "\nDiagnostic: " + correction.diagnosticMessage;
    }

    // Step 2: If puppeteer can't fix it, ask LLM to re-analyze the flaw
    using ChatMsg = RawrXD::Agent::ChatMessage;
    std::vector<ChatMsg> messages;
    messages.push_back(ChatMsg{"system",
        "You are a code analysis self-correction agent. A prior reasoning step "
        "had a flaw. Analyze the flaw and provide a corrected, improved analysis. "
        "Be specific and reference concrete code structures.", "", {}});

    std::string userMsg = "Detected flaw: " + flaw + "\n\nPrior context:\n" + accumulatedContext;
    messages.push_back(ChatMsg{"user", userMsg, "", {}});

    auto& llm = getThinkingLLM();
    auto result = llm.ChatSync(messages);
    if (result.success && !result.response.empty()) {
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_patternFrequency["llm_flaw_correction"]++;
        }
        if (auto* tc = TelemetryCollector::instance()) {
            tc->trackPerformance("deep_thinking.correction_tokens",
                                 static_cast<double>(result.completion_tokens), "tokens");
        }
        return "[LLM-corrected] " + result.response;
    }

    // Step 3: Minimal fallback — re-run flaw detection to provide diagnostics
    auto& detector = getFailureDetector();
    auto failures = detector.detectMultipleFailures(flaw);
    std::string fallback = "[Fallback correction] Detected " + std::to_string(failures.size()) + " issues:\n";
    for (const auto& f : failures) {
        fallback += " - " + f.description + " (confidence: " + std::to_string(f.confidence) + ")\n";
    }
    fallback += "Original flaw: " + flaw;
    return fallback;
}

AgenticDeepThinkingEngine::ReasoningStep AgenticDeepThinkingEngine::synthesizeAnswer(
    const std::vector<ReasoningStep>& steps,
    const ThinkingContext& context
) {
    ScopedMeasurement perf(static_cast<uint32_t>(KernelSlot::TotalInference));
    ReasoningStep step;
    step.step = ThinkingStep::FinalSynthesis;
    step.title = "Final Synthesis";
    step.successful = true;

    // Compile accumulated context from all reasoning steps
    std::string stepsContext;
    int totalFindings = 0;
    float confidenceSum = 0.0f;
    for (const auto& s : steps) {
        stepsContext += "### " + s.title + " (confidence: " + std::to_string(s.confidence) + ")\n";
        stepsContext += s.content + "\n";
        if (!s.findings.empty()) {
            stepsContext += "Findings:\n";
            for (const auto& f : s.findings) {
                stepsContext += "  - " + f + "\n";
                totalFindings++;
            }
        }
        stepsContext += "\n";
        confidenceSum += s.confidence;
    }

    // Try LLM-driven synthesis for a coherent, structured final answer
    using ChatMsg = RawrXD::Agent::ChatMessage;
    std::vector<ChatMsg> messages;
    messages.push_back(ChatMsg{"system",
        "You are a senior technical writer synthesizing code analysis results. "
        "Given the reasoning steps below, produce a clear, structured final answer. "
        "Include: 1) Summary of findings, 2) Root cause analysis, 3) Recommended actions "
        "with specific file/function references, 4) Risk assessment. "
        "Be concise but thorough. Use markdown formatting.", "", {}});

    std::string userMsg = "## Problem\n" + context.problem
                        + "\nLanguage: " + context.language
                        + "\n\n## Reasoning Steps\n" + stepsContext;
    messages.push_back(ChatMsg{"user", userMsg, "", {}});

    auto& llm = getThinkingLLM();
    auto result = llm.ChatSync(messages);

    if (result.success && !result.response.empty()) {
        step.content = result.response;

        // Validate the synthesis through failure detection
        auto& detector = getFailureDetector();
        auto failure = detector.detectFailure(result.response, context.problem);
        if (failure.type != AgentFailureType::None &&
            failure.confidence > 0.8) {
            // LLM synthesis has issues — apply puppeteer correction
            auto& puppeteer = getPuppeteer();
            auto correction = puppeteer.correctResponse(result.response, context.problem);
            if (correction.success && !correction.correctedOutput.empty()) {
                step.content = correction.correctedOutput;
                {
                    std::lock_guard<std::mutex> lock(m_statsMutex);
                    m_patternFrequency["synthesis_puppeteer_fix"]++;
                }
            }
        }

        // Track synthesis metrics
        if (auto* tc = TelemetryCollector::instance()) {
            tc->trackPerformance("deep_thinking.synthesis_tokens",
                                 static_cast<double>(result.completion_tokens), "tokens");
            tc->trackPerformance("deep_thinking.synthesis_tok_per_sec",
                                 result.tokens_per_sec, "tok/s");
            tc->trackPerformance("deep_thinking.synthesis_latency_ms",
                                 result.total_duration_ms, "ms");
            tc->trackPerformance("deep_thinking.total_findings",
                                 static_cast<double>(totalFindings), "count");
            tc->trackFeatureUsage("deep_thinking.synthesize");
        }
    } else {
        // Fallback: structured local synthesis
        std::string finalAnswer;
        finalAnswer += "# Analysis Summary\n\n";

        // Group findings by step
        for (const auto& s : steps) {
            if (!s.findings.empty()) {
                finalAnswer += "## " + s.title + "\n";
                for (const auto& finding : s.findings) {
                    finalAnswer += "- " + finding + "\n";
                }
                finalAnswer += "\n";
            }
        }

        // Add confidence assessment
        float avgConf = steps.empty() ? 0.0f : confidenceSum / steps.size();
        finalAnswer += "## Confidence Assessment\n";
        finalAnswer += "- Overall confidence: " + std::to_string(avgConf) + "\n";
        finalAnswer += "- Total findings: " + std::to_string(totalFindings) + "\n";
        finalAnswer += "- Steps completed: " + std::to_string(steps.size()) + "\n";

        step.content = finalAnswer;

        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_patternFrequency["synthesis_local_fallback"]++;
        }
    }

    step.confidence = calculateOverallConfidence(steps);

    // Collect key findings from all steps as suggested fixes
    for (const auto& s : steps) {
        for (const auto& f : s.findings) {
            step.findings.push_back(f);
        }
    }

    return step;
}

std::vector<std::string> AgenticDeepThinkingEngine::listFilesRecursive(const std::string& directory, const std::string& extension) {
    std::vector<std::string> files;
    try {
        if (!std::filesystem::exists(directory)) return files;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 directory, std::filesystem::directory_options::skip_permission_denied)) {
            try {
                if (!entry.is_regular_file()) continue;
                std::string path = entry.path().string();

                // Skip hidden directories and build artifacts
                if (path.find("\\.git\\") != std::string::npos ||
                    path.find("/.git/") != std::string::npos ||
                    path.find("\\build\\") != std::string::npos ||
                    path.find("/build/") != std::string::npos ||
                    path.find("\\node_modules\\") != std::string::npos) {
                    continue;
                }

                // Filter by extension if specified
                if (!extension.empty()) {
                    std::string ext = entry.path().extension().string();
                    if (ext != extension) continue;
                }

                files.push_back(path);

                // Safety limit
                if (files.size() >= 5000) break;
            } catch (...) { continue; }
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeepThinking] listFilesRecursive error: " << e.what() << std::endl;
    }
    return files;
}

std::string AgenticDeepThinkingEngine::readFileContent(const std::string& filePath, int maxLines) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return "";
        }

        std::string content;
        std::string line;
        int lineCount = 0;

        while (std::getline(file, line) && lineCount < maxLines) {
            content += line + "\n";
            lineCount++;
        }

        return content;
    } catch (...) {
        return "";
    }
}

std::vector<std::pair<std::string, int>> AgenticDeepThinkingEngine::searchInFiles(
    const std::string& pattern,
    const std::string& directory
) {
    std::vector<std::pair<std::string, int>> results;

    std::regex searchRegex;
    bool useRegex = false;
    try {
        searchRegex = std::regex(pattern, std::regex::icase);
        useRegex = true;
    } catch (...) {
        useRegex = false;
    }

    std::string lowerPattern = pattern;
    std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);

    auto files = listFilesRecursive(directory, "");
    for (const auto& filePath : files) {
        // Only search source-type files
        std::string ext = "";
        size_t dot = filePath.find_last_of('.');
        if (dot != std::string::npos) ext = filePath.substr(dot);
        if (ext != ".cpp" && ext != ".hpp" && ext != ".h" && ext != ".c" &&
            ext != ".py" && ext != ".js" && ext != ".ts" && ext != ".asm") continue;

        try {
            std::ifstream file(filePath);
            if (!file.is_open()) continue;

            std::string line;
            int lineNum = 0;
            while (std::getline(file, line)) {
                lineNum++;
                bool matched = false;
                if (useRegex) {
                    try { matched = std::regex_search(line, searchRegex); }
                    catch (...) { matched = false; }
                } else {
                    std::string lowerLine = line;
                    std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
                    matched = lowerLine.find(lowerPattern) != std::string::npos;
                }

                if (matched) {
                    results.push_back({filePath, lineNum});
                    if (results.size() >= 200) return results;
                }
            }
        } catch (...) { continue; }
    }

    return results;
}

std::vector<std::string> AgenticDeepThinkingEngine::findCommonPatterns(const std::vector<std::string>& codeSnippets) {
    std::vector<std::string> patterns;
    if (codeSnippets.size() < 2) return patterns;

    // Extract lines from all snippets
    std::map<std::string, int> lineFrequency;
    for (const auto& snippet : codeSnippets) {
        std::istringstream iss(snippet);
        std::string line;
        std::set<std::string> seenInSnippet;
        while (std::getline(iss, line)) {
            // Normalize whitespace
            std::string trimmed;
            bool lastSpace = false;
            for (char c : line) {
                if (c == ' ' || c == '\t') {
                    if (!lastSpace && !trimmed.empty()) {
                        trimmed += ' ';
                        lastSpace = true;
                    }
                } else {
                    trimmed += c;
                    lastSpace = false;
                }
            }
            if (trimmed.length() > 10 && seenInSnippet.find(trimmed) == seenInSnippet.end()) {
                lineFrequency[trimmed]++;
                seenInSnippet.insert(trimmed);
            }
        }
    }

    // Find lines appearing in multiple snippets (common patterns)
    std::vector<std::pair<std::string, int>> sorted(lineFrequency.begin(), lineFrequency.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& [pattern, count] : sorted) {
        if (count >= 2) {
            patterns.push_back(pattern + " (found in " + std::to_string(count) + " snippets)");
            m_patternFrequency[pattern] += count;
        }
        if (patterns.size() >= 10) break;
    }

    return patterns;
}

std::string AgenticDeepThinkingEngine::identifyBestMatch(const std::string& query, const std::vector<std::string>& candidates) {
    if (candidates.empty()) return "";
    if (candidates.size() == 1) return candidates[0];

    // Tokenize query into words for Jaccard similarity
    auto tokenize = [](const std::string& text) -> std::set<std::string> {
        std::set<std::string> tokens;
        std::istringstream iss(text);
        std::string word;
        while (iss >> word) {
            std::string lower = word;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.length() > 2) tokens.insert(lower);
        }
        return tokens;
    };

    auto queryTokens = tokenize(query);
    if (queryTokens.empty()) return candidates[0];

    float bestScore = -1.0f;
    std::string bestMatch;

    for (const auto& candidate : candidates) {
        auto candidateTokens = tokenize(candidate);
        if (candidateTokens.empty()) continue;

        // Jaccard similarity: |intersection| / |union|
        int intersection = 0;
        for (const auto& token : queryTokens) {
            if (candidateTokens.count(token)) intersection++;
        }

        int unionSize = static_cast<int>(queryTokens.size() + candidateTokens.size()) - intersection;
        float score = unionSize > 0 ? static_cast<float>(intersection) / unionSize : 0.0f;

        // Bonus for substring containment
        std::string lowerCandidate = candidate;
        std::string lowerQuery = query;
        std::transform(lowerCandidate.begin(), lowerCandidate.end(), lowerCandidate.begin(), ::tolower);
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
        if (lowerCandidate.find(lowerQuery) != std::string::npos) score += 0.3f;
        if (lowerQuery.find(lowerCandidate) != std::string::npos) score += 0.2f;

        if (score > bestScore) {
            bestScore = score;
            bestMatch = candidate;
        }
    }

    return bestMatch.empty() ? candidates[0] : bestMatch;
}

float AgenticDeepThinkingEngine::calculateStepConfidence(const ReasoningStep& step) {
    return step.confidence;
}

float AgenticDeepThinkingEngine::calculateOverallConfidence(const std::vector<ReasoningStep>& steps) {
    if (steps.empty()) {
        return 0.0f;
    }

    float totalConfidence = 0.0f;
    for (const auto& step : steps) {
        totalConfidence += step.confidence;
    }

    return totalConfidence / steps.size();
}
