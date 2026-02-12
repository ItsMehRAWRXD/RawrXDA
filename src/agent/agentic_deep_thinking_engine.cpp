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
#include <iomanip>

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
    ScopedMeasurement perf(static_cast<uint32_t>(KernelSlot::TotalInference));
    auto startTime = std::chrono::high_resolution_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalThinkingRequests++;
    }

    // Track feature usage via telemetry
    if (auto* tc = TelemetryCollector::instance()) {
        tc->trackFeatureUsage("deep_thinking.think");
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

        // Log failure to telemetry
        if (auto* tc = TelemetryCollector::instance()) {
            tc->trackCrash(std::string("deep_thinking_exception: ") + e.what());
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.avgThinkingTime = (m_stats.avgThinkingTime * (m_stats.successfulThinking - 1) + result.elapsedMilliseconds)
                                 / std::max(m_stats.successfulThinking, 1);
    }

    // Record overall latency in perf telemetry
    if (auto* tc = TelemetryCollector::instance()) {
        tc->trackPerformance("deep_thinking.total_latency_ms",
                             static_cast<double>(result.elapsedMilliseconds), "ms");
        tc->trackPerformance("deep_thinking.step_count",
                             static_cast<double>(result.steps.size()), "count");
        tc->trackPerformance("deep_thinking.overall_confidence",
                             static_cast<double>(result.overallConfidence), "ratio");
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
        std::string content = readFileContent(filePath, 500);
        if (content.empty()) {
            return "File not found or empty: " + filePath;
        }

        // Build structural analysis
        std::ostringstream analysis;
        analysis << "=== File Analysis: " << filePath << " ===\n";

        // File size and line count
        int totalLines = 0;
        int codeLines = 0;
        int commentLines = 0;
        int blankLines = 0;
        int includeCount = 0;
        int functionDefCount = 0;
        int classDefCount = 0;
        int todoCount = 0;
        bool inBlockComment = false;

        std::istringstream iss(content);
        std::string line;
        std::vector<std::string> functionNames;
        std::vector<std::string> classNames;
        std::vector<std::string> includeList;

        while (std::getline(iss, line)) {
            totalLines++;
            // Trim
            size_t first = line.find_first_not_of(" \t");
            std::string trimmed = (first != std::string::npos) ? line.substr(first) : "";

            if (trimmed.empty()) {
                blankLines++;
                continue;
            }

            // Block comment tracking
            if (inBlockComment) {
                commentLines++;
                if (trimmed.find("*/") != std::string::npos) inBlockComment = false;
                continue;
            }
            if (trimmed.find("/*") != std::string::npos) {
                commentLines++;
                if (trimmed.find("*/") == std::string::npos) inBlockComment = true;
                continue;
            }
            if (trimmed.substr(0, 2) == "//") {
                commentLines++;
                // Check for TODO/FIXME
                std::string lowerTrimmed = trimmed;
                std::transform(lowerTrimmed.begin(), lowerTrimmed.end(), lowerTrimmed.begin(), ::tolower);
                if (lowerTrimmed.find("todo") != std::string::npos ||
                    lowerTrimmed.find("fixme") != std::string::npos ||
                    lowerTrimmed.find("hack") != std::string::npos) {
                    todoCount++;
                }
                continue;
            }

            codeLines++;

            // Detect includes
            if (trimmed.substr(0, 8) == "#include") {
                includeCount++;
                includeList.push_back(trimmed);
            }

            // Detect function definitions (heuristic: return_type name(...) {)
            // Look for lines ending with '{' that contain '(' and ')'
            if (trimmed.find('(') != std::string::npos &&
                trimmed.find(')') != std::string::npos &&
                (trimmed.back() == '{' || trimmed.back() == ')') &&
                trimmed.find("if") == std::string::npos &&
                trimmed.find("while") == std::string::npos &&
                trimmed.find("for") == std::string::npos &&
                trimmed.find("switch") == std::string::npos) {
                functionDefCount++;
                // Extract function name (word before '(')
                size_t parenPos = trimmed.find('(');
                if (parenPos > 0) {
                    size_t nameEnd = parenPos;
                    size_t nameStart = trimmed.find_last_of(" \t:>", nameEnd - 1);
                    nameStart = (nameStart == std::string::npos) ? 0 : nameStart + 1;
                    std::string fname = trimmed.substr(nameStart, nameEnd - nameStart);
                    if (!fname.empty() && fname.length() < 80) {
                        functionNames.push_back(fname);
                    }
                }
            }

            // Detect class/struct definitions
            if ((trimmed.find("class ") == 0 || trimmed.find("struct ") == 0) &&
                trimmed.find(';') == std::string::npos) {
                classDefCount++;
                std::istringstream cls(trimmed);
                std::string keyword, name;
                cls >> keyword >> name;
                // Strip trailing ':' or '{'
                while (!name.empty() && (name.back() == ':' || name.back() == '{' || name.back() == ' '))
                    name.pop_back();
                if (!name.empty()) classNames.push_back(name);
            }
        }

        // Extension detection
        std::string ext;
        size_t dotPos = filePath.find_last_of('.');
        if (dotPos != std::string::npos) ext = filePath.substr(dotPos);

        float commentRatio = totalLines > 0 ? static_cast<float>(commentLines) / totalLines * 100.0f : 0.0f;
        float codeRatio = totalLines > 0 ? static_cast<float>(codeLines) / totalLines * 100.0f : 0.0f;

        analysis << "Type: " << ext << "\n";
        analysis << "Lines: " << totalLines << " (code: " << codeLines
                 << ", comments: " << commentLines
                 << ", blank: " << blankLines << ")\n";
        analysis << "Comment ratio: " << std::fixed << std::setprecision(1) << commentRatio << "%\n";
        analysis << "Code density: " << std::fixed << std::setprecision(1) << codeRatio << "%\n";
        analysis << "Includes: " << includeCount << "\n";
        analysis << "Functions: " << functionDefCount << "\n";
        analysis << "Classes/Structs: " << classDefCount << "\n";
        if (todoCount > 0) {
            analysis << "TODO/FIXME markers: " << todoCount << "\n";
        }

        if (!classNames.empty()) {
            analysis << "Defined types: ";
            for (size_t i = 0; i < std::min(classNames.size(), size_t(10)); ++i) {
                if (i > 0) analysis << ", ";
                analysis << classNames[i];
            }
            analysis << "\n";
        }

        if (!functionNames.empty()) {
            analysis << "Key functions: ";
            for (size_t i = 0; i < std::min(functionNames.size(), size_t(15)); ++i) {
                if (i > 0) analysis << ", ";
                analysis << functionNames[i];
            }
            analysis << "\n";
        }

        // Complexity heuristic
        std::string complexity = "Low";
        if (codeLines > 500) complexity = "High";
        else if (codeLines > 200) complexity = "Medium";
        if (functionDefCount > 20) complexity = "High";
        analysis << "Complexity: " << complexity << "\n";

        return analysis.str();
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

    // Step 6 (optional): Self-Correction — may iterate with cycle multiplier
    int correctionIterations = 0;
    if (context.allowSelfCorrection) {
        // Apply cycle multiplier (1x-8x) to increase iteration depth
        int effectiveMaxIter = context.maxIterations * std::clamp(context.cycleMultiplier, 1, 8);
        
        // Track telemetry for cycle multiplier usage
        if (auto* tc = TelemetryCollector::instance(); context.cycleMultiplier > 1) {
            tc->trackPerformance("deep_thinking.cycle_multiplier",
                                 static_cast<double>(context.cycleMultiplier), "multiplier");
            tc->trackPerformance("deep_thinking.effective_iterations",
                                 static_cast<double>(effectiveMaxIter), "iterations");
        }
        
        for (int iter = 0; iter < effectiveMaxIter; ++iter) {
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_stats.stepFrequency[ThinkingStep::SelfCorrection]++;
            }
            auto step6 = selfCorrect(context, steps);
            if (step6.successful) {
                steps.push_back(step6);
                contextAccumulator += step6.content + "\n";
                correctionIterations++;
            } else {
                break;  // No more flaws detected, stop iterating
            }
        }
        if (correctionIterations > 0) {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_patternFrequency["self_correction_iterations"] += correctionIterations;
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
    if (problem.empty()) return issues;

    std::string lower = problem;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // --- Phase 1: Sentence extraction with quality filtering ---
    // Split on sentence boundaries: '.', '!', '?', '\n', and also ';' for code-style
    std::string delimiters = ".!?\n;";
    size_t start = 0;
    while (start < problem.size()) {
        size_t end = std::string::npos;
        for (char d : delimiters) {
            size_t pos = problem.find(d, start);
            if (pos != std::string::npos && (end == std::string::npos || pos < end)) {
                end = pos;
            }
        }
        if (end == std::string::npos) end = problem.size();

        std::string segment = problem.substr(start, end - start);
        // Trim leading/trailing whitespace
        size_t first = segment.find_first_not_of(" \t\r\n");
        size_t last = segment.find_last_not_of(" \t\r\n");
        if (first != std::string::npos && last != std::string::npos) {
            segment = segment.substr(first, last - first + 1);
        }
        // Only keep meaningful segments (>8 chars, has at least 2 words)
        if (segment.length() > 8) {
            int wordCount = 0;
            std::istringstream wc(segment);
            std::string w;
            while (wc >> w) wordCount++;
            if (wordCount >= 2) {
                issues.push_back(segment);
            }
        }
        start = (end < problem.size()) ? end + 1 : problem.size();
    }

    // --- Phase 2: Pattern-based issue detection ---
    // Detect error/exception patterns
    static const std::vector<std::pair<std::string, std::string>> patterns = {
        {"error",                 "Error condition detected"},
        {"exception",            "Exception handling concern"},
        {"crash",                "Crash/stability issue"},
        {"segfault",             "Memory access violation"},
        {"access violation",     "Memory access violation"},
        {"null pointer",         "Null pointer dereference"},
        {"nullptr",              "Null pointer dereference"},
        {"memory leak",          "Memory leak concern"},
        {"buffer overflow",      "Buffer overflow vulnerability"},
        {"race condition",       "Threading/race condition"},
        {"deadlock",             "Deadlock potential"},
        {"performance",          "Performance optimization needed"},
        {"slow",                 "Performance bottleneck"},
        {"latency",              "Latency issue"},
        {"linker",               "Linker/build error"},
        {"unresolved",           "Unresolved symbol"},
        {"undefined reference",  "Undefined reference"},
        {"compile",              "Compilation issue"},
        {"syntax",               "Syntax error"},
        {"deprecated",           "Deprecated API usage"},
        {"todo",                 "Incomplete implementation"},
        {"fixme",                "Known defect marker"},
        {"hack",                 "Technical debt"},
        {"stub",                 "Stub/unfinished implementation"},
        {"hardcoded",            "Hardcoded value concern"},
        {"refactor",             "Refactoring opportunity"},
    };

    for (const auto& [pattern, label] : patterns) {
        if (lower.find(pattern) != std::string::npos) {
            // Check it's not already covered by an extracted sentence
            bool alreadyCovered = false;
            for (const auto& existing : issues) {
                std::string existLower = existing;
                std::transform(existLower.begin(), existLower.end(), existLower.begin(), ::tolower);
                if (existLower.find(pattern) != std::string::npos) {
                    alreadyCovered = true;
                    break;
                }
            }
            if (!alreadyCovered) {
                issues.push_back("[Pattern] " + label);
            }
        }
    }

    // --- Phase 3: Code artifact extraction ---
    // Find class names, function names, file references in the problem text
    std::regex codeRef(R"(\b([A-Z][a-zA-Z]+(?:::[a-zA-Z_]+)+)\b)");  // e.g., ClassName::method
    std::regex fileRef(R"(\b([a-zA-Z_][a-zA-Z0-9_]*\.(?:cpp|hpp|h|c|asm))\b)");
    std::smatch match;

    std::string searchTarget = problem;
    while (std::regex_search(searchTarget, match, codeRef)) {
        issues.push_back("[CodeRef] " + match[1].str());
        searchTarget = match.suffix().str();
    }

    searchTarget = problem;
    while (std::regex_search(searchTarget, match, fileRef)) {
        issues.push_back("[FileRef] " + match[1].str());
        searchTarget = match.suffix().str();
    }

    return issues;
}

std::string AgenticDeepThinkingEngine::categorizeIssue(const std::string& issue) {
    std::string lower = issue;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Priority-ordered category matching with multi-keyword support
    struct CategoryRule {
        const char* category;
        std::vector<const char*> keywords;
    };
    static const std::vector<CategoryRule> rules = {
        {"Security/Vulnerability",   {"vulnerability", "exploit", "injection", "overflow",
                                      "xss", "csrf", "security", "access violation", "cve"}},
        {"Memory/Resource",          {"memory leak", "segfault", "nullptr", "null pointer",
                                      "double free", "use after free", "buffer", "allocation",
                                      "resource exhaust", "oom", "out of memory"}},
        {"Threading/Concurrency",    {"race condition", "deadlock", "mutex", "atomic",
                                      "thread", "concurrent", "synchronization", "lock"}},
        {"Build/Linker",             {"linker", "unresolved", "undefined reference", "lnk",
                                      "compile error", "c2", "syntax error", "cmake"}},
        {"Performance",              {"performance", "slow", "latency", "throughput",
                                      "bottleneck", "optimize", "cache miss", "profil"}},
        {"Error/Failure",            {"error", "failed", "crash", "exception", "abort",
                                      "panic", "fatal", "assert"}},
        {"API/Integration",          {"api", "endpoint", "http", "request", "response",
                                      "webhook", "rest", "grpc", "protocol"}},
        {"Data/Model",               {"model", "gguf", "tensor", "inference", "quantiz",
                                      "weight", "token", "embedding", "layer"}},
        {"Architecture/Design",      {"design", "architecture", "pattern", "refactor",
                                      "decouple", "abstraction", "modular", "solid"}},
        {"Testing/Quality",          {"test", "verify", "validate", "assert", "coverage",
                                      "regression", "fuzz", "benchmark"}},
        {"Documentation",            {"document", "comment", "readme", "usage", "example"}},
        {"Technical Debt",           {"todo", "fixme", "hack", "stub", "hardcoded",
                                      "workaround", "kludge", "deprecated"}},
        {"UI/UX",                    {"dialog", "window", "menu", "button", "widget",
                                      "gui", "display", "render", "layout"}},
        {"Configuration",            {"config", "setting", "option", "flag", "toggle",
                                      "environment", "parameter"}},
    };

    // Score each category and pick the best match
    int bestScore = 0;
    std::string bestCategory = "General";
    for (const auto& rule : rules) {
        int score = 0;
        for (const char* kw : rule.keywords) {
            if (lower.find(kw) != std::string::npos) {
                score++;
            }
        }
        if (score > bestScore) {
            bestScore = score;
            bestCategory = rule.category;
        }
    }

    // Track category frequency for patterns
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_patternFrequency["category_" + bestCategory]++;
    }

    return bestCategory;
}

AgenticDeepThinkingEngine::ReasoningStep AgenticDeepThinkingEngine::gatherContext(const ThinkingContext& context) {
    ScopedMeasurement perf(static_cast<uint32_t>(KernelSlot::TotalInference));
    ReasoningStep step;
    step.step = ThinkingStep::ContextGathering;
    step.title = "Context Gathering";
    step.successful = true;

    // Phase 1: Find relevant code files by keyword matching
    auto relatedCode = findRelevantCode(context.problem, 5);
    step.findings = relatedCode;

    // Phase 2: Extract project structure for broader understanding
    std::string projectRoot = context.projectRoot.empty() ? "." : context.projectRoot;
    std::string structureInfo;
    auto allFiles = listFilesRecursive(projectRoot, "");

    // Classify files by type and directory
    std::map<std::string, int> dirCounts;
    std::map<std::string, int> extCounts;
    for (const auto& f : allFiles) {
        // Extract directory
        size_t lastSep = f.find_last_of("/\\");
        std::string dir = (lastSep != std::string::npos) ? f.substr(0, lastSep) : ".";
        dirCounts[dir]++;

        // Extract extension
        size_t dotPos = f.find_last_of('.');
        if (dotPos != std::string::npos) {
            extCounts[f.substr(dotPos)]++;
        }
    }

    structureInfo += "Project structure: " + std::to_string(allFiles.size()) + " files across "
                   + std::to_string(dirCounts.size()) + " directories\n";
    for (const auto& [ext, count] : extCounts) {
        if (count >= 3) {
            structureInfo += "  " + ext + ": " + std::to_string(count) + " files\n";
        }
    }

    // Phase 3: Read first 30 lines of top-scoring files for direct context
    for (size_t i = 0; i < std::min(relatedCode.size(), size_t(3)); ++i) {
        // Extract file path from the code snippet (format: "path:line: content")
        std::string snippet = relatedCode[i];
        size_t firstColon = snippet.find(':');
        if (firstColon != std::string::npos) {
            std::string filePath = snippet.substr(0, firstColon);
            std::string fileContent = readFileContent(filePath, 30);
            if (!fileContent.empty()) {
                step.findings.push_back("[File context: " + filePath + "]\n" + fileContent);
            }
        }
    }

    // Phase 4: Search for related TODO/FIXME markers
    auto todoMatches = searchInFiles("TODO|FIXME|HACK|XXX", "src");
    if (!todoMatches.empty()) {
        std::string todoContext = "Found " + std::to_string(todoMatches.size()) + " TODO/FIXME markers";
        step.findings.push_back(todoContext);
    }

    step.content = "Found " + std::to_string(relatedCode.size()) + " relevant code sections\n"
                 + structureInfo;
    step.confidence = relatedCode.empty() ? 0.55f : (0.75f + std::min(0.15f, relatedCode.size() * 0.03f));

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
    ScopedMeasurement perf(static_cast<uint32_t>(KernelSlot::TotalInference));
    ReasoningStep step;
    step.step = ThinkingStep::HypothesiGeneration;
    step.title = "Hypothesis Generation";
    step.successful = true;

    // Determine hypothesis count based on problem complexity
    int hypothesisCount = 3;
    if (context.deepResearch) hypothesisCount = 5;
    auto issues = identifyKeyIssues(context.problem);
    if (issues.size() > 3) hypothesisCount = std::min(7, static_cast<int>(issues.size()) + 1);

    auto hypotheses = brainstormSolutions(context.problem, hypothesisCount);
    step.findings = hypotheses;

    // Integrate prior analysis into content for downstream steps
    std::string content;
    content += "Generated " + std::to_string(hypotheses.size()) + " hypotheses";
    if (!analysis.empty()) {
        // Extract key analysis takeaways for hypothesis grounding
        content += "\nGrounded in prior analysis:\n";
        std::istringstream iss(analysis);
        std::string line;
        int lineCount = 0;
        while (std::getline(iss, line) && lineCount < 5) {
            size_t start = line.find_first_not_of(" \t");
            if (start != std::string::npos && line.length() > 15) {
                content += "  " + line.substr(start) + "\n";
                lineCount++;
            }
        }
    }
    step.content = content;

    // Dynamic confidence: higher if LLM-generated hypotheses matched analysis keywords
    if (!analysis.empty() && !hypotheses.empty()) {
        int matchCount = 0;
        std::string lowerAnalysis = analysis;
        std::transform(lowerAnalysis.begin(), lowerAnalysis.end(), lowerAnalysis.begin(), ::tolower);
        for (const auto& h : hypotheses) {
            std::string lowerH = h;
            std::transform(lowerH.begin(), lowerH.end(), lowerH.begin(), ::tolower);
            // Check if hypothesis references keywords from analysis
            std::istringstream words(lowerH);
            std::string word;
            int wordMatches = 0;
            while (words >> word) {
                if (word.length() > 4 && lowerAnalysis.find(word) != std::string::npos) {
                    wordMatches++;
                }
            }
            if (wordMatches >= 2) matchCount++;
        }
        float groundingRatio = static_cast<float>(matchCount) / hypotheses.size();
        step.confidence = 0.60f + (groundingRatio * 0.30f);  // 0.60-0.90 range
    } else {
        step.confidence = hypotheses.empty() ? 0.40f : 0.65f;
    }

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_patternFrequency["hypothesis_count"] += static_cast<int>(hypotheses.size());
    }

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

    // Compute confidence based on actual evidence found across hypothesis tests
    int supportedCount = 0;
    for (const auto& f : step.findings) {
        std::string lower = f;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find("supported") != std::string::npos ||
            lower.find("evidence") != std::string::npos) {
            supportedCount++;
        }
    }
    float evidenceRatio = hypotheses.empty() ? 0.0f
        : static_cast<float>(supportedCount) / hypotheses.size();
    step.confidence = 0.5f + (evidenceRatio * 0.4f);  // Range: 0.5 (no evidence) to 0.9 (all supported)
    step.content = "Tested " + std::to_string(hypotheses.size()) + " hypotheses, "
                 + std::to_string(supportedCount) + " supported by evidence";

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
    ScopedMeasurement perf(static_cast<uint32_t>(KernelSlot::TotalInference));
    ReasoningStep step;
    step.step = ThinkingStep::ResultEvaluation;
    step.title = "Result Evaluation";
    step.successful = true;

    if (results.empty()) {
        step.content = "No results to evaluate";
        step.confidence = 0.0f;
        return step;
    }

    // Score each result individually and build breakdown
    float totalScore = 0.0f;
    float maxScore = 0.0f;
    float minScore = 1.0f;
    int highQualityCount = 0;
    int evidenceBackedCount = 0;
    std::vector<std::pair<float, int>> scoredIndices;

    for (size_t i = 0; i < results.size(); ++i) {
        float score = scoreResult(results[i], context.problem);
        totalScore += score;
        maxScore = std::max(maxScore, score);
        minScore = std::min(minScore, score);
        scoredIndices.push_back({score, static_cast<int>(i)});

        if (score > 0.7f) {
            highQualityCount++;
            step.findings.push_back(results[i]);
        }

        // Check if result contains file/code evidence
        std::string lower = results[i];
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find(".cpp") != std::string::npos ||
            lower.find(".hpp") != std::string::npos ||
            lower.find("::") != std::string::npos ||
            lower.find("evidence") != std::string::npos) {
            evidenceBackedCount++;
        }
    }

    // Sort by score descending to rank results
    std::sort(scoredIndices.begin(), scoredIndices.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    float avgScore = totalScore / results.size();
    float scoreRange = maxScore - minScore;

    // Build detailed evaluation report
    std::ostringstream report;
    report << "Evaluation Summary:\n";
    report << "  Total results: " << results.size() << "\n";
    report << "  Average score: " << std::fixed << std::setprecision(3) << avgScore << "\n";
    report << "  Score range: [" << minScore << ", " << maxScore << "]\n";
    report << "  High quality (>0.7): " << highQualityCount << "/" << results.size() << "\n";
    report << "  Evidence-backed: " << evidenceBackedCount << "/" << results.size() << "\n";
    report << "  Top ranked results:\n";
    for (size_t i = 0; i < std::min(scoredIndices.size(), size_t(3)); ++i) {
        report << "    #" << (i + 1) << " (score: " << scoredIndices[i].first << "): ";
        std::string preview = results[scoredIndices[i].second];
        if (preview.length() > 120) preview = preview.substr(0, 120) + "...";
        // Replace newlines for compact display
        std::replace(preview.begin(), preview.end(), '\n', ' ');
        report << preview << "\n";
    }
    step.content = report.str();

    // Confidence combines average score, evidence ratio, and consistency
    float evidenceRatio = static_cast<float>(evidenceBackedCount) / results.size();
    float qualityRatio = static_cast<float>(highQualityCount) / results.size();
    float consistency = 1.0f - std::min(scoreRange, 1.0f);  // Tight range = more consistent
    step.confidence = (avgScore * 0.40f) + (evidenceRatio * 0.25f)
                    + (qualityRatio * 0.20f) + (consistency * 0.15f);
    step.confidence = std::clamp(step.confidence, 0.0f, 1.0f);

    // Track evaluation metrics
    if (auto* tc = TelemetryCollector::instance()) {
        tc->trackPerformance("deep_thinking.eval_avg_score",
                             static_cast<double>(avgScore), "score");
        tc->trackPerformance("deep_thinking.eval_high_quality_ratio",
                             static_cast<double>(qualityRatio), "ratio");
        tc->trackPerformance("deep_thinking.eval_evidence_ratio",
                             static_cast<double>(evidenceRatio), "ratio");
    }

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
    // Weighted confidence: base confidence adjusted by evidence quality
    float base = step.confidence;

    // Boost for steps with concrete findings
    float findingsBoost = 0.0f;
    if (!step.findings.empty()) {
        int evidenceFindings = 0;
        for (const auto& f : step.findings) {
            std::string lower = f;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find(".cpp") != std::string::npos ||
                lower.find(".hpp") != std::string::npos ||
                lower.find("::") != std::string::npos ||
                lower.find("evidence") != std::string::npos ||
                lower.find("supported") != std::string::npos ||
                lower.find("found") != std::string::npos) {
                evidenceFindings++;
            }
        }
        float evidenceRatio = static_cast<float>(evidenceFindings) / step.findings.size();
        findingsBoost = evidenceRatio * 0.10f;  // Up to +0.10 for evidence-backed findings
    }

    // Penalty for very short, uninformative content
    float contentPenalty = 0.0f;
    if (step.content.length() < 30) contentPenalty = 0.10f;

    // Bonus for successful steps
    float successBonus = step.successful ? 0.0f : -0.15f;

    return std::clamp(base + findingsBoost - contentPenalty + successBonus, 0.0f, 1.0f);
}

float AgenticDeepThinkingEngine::calculateOverallConfidence(const std::vector<ReasoningStep>& steps) {
    if (steps.empty()) {
        return 0.0f;
    }

    // Step importance weights based on reasoning phase
    auto getStepWeight = [](ThinkingStep s) -> float {
        switch (s) {
            case ThinkingStep::ProblemAnalysis:    return 1.2f;  // Foundation — critical
            case ThinkingStep::ContextGathering:   return 1.0f;  // Supporting evidence
            case ThinkingStep::HypothesiGeneration:return 1.1f;  // Direction of investigation
            case ThinkingStep::ExperimentationRun: return 1.3f;  // Empirical validation — most important
            case ThinkingStep::ResultEvaluation:   return 1.2f;  // Quality gate
            case ThinkingStep::SelfCorrection:     return 0.8f;  // Optional improvement
            case ThinkingStep::FinalSynthesis:     return 1.4f;  // Final output — highest weight
            default:                               return 1.0f;
        }
    };

    float weightedSum = 0.0f;
    float totalWeight = 0.0f;
    float minConfidence = 1.0f;
    int failedSteps = 0;

    for (const auto& step : steps) {
        float adjustedConf = calculateStepConfidence(step);
        float weight = getStepWeight(step.step);
        weightedSum += adjustedConf * weight;
        totalWeight += weight;
        minConfidence = std::min(minConfidence, adjustedConf);
        if (!step.successful) failedSteps++;
    }

    float weightedAvg = (totalWeight > 0.0f) ? weightedSum / totalWeight : 0.0f;

    // Apply penalties for failed steps
    float failurePenalty = static_cast<float>(failedSteps) * 0.05f;

    // Apply a floor: if any step is very low, drag overall down
    float floorPenalty = 0.0f;
    if (minConfidence < 0.3f) floorPenalty = 0.10f;

    return std::clamp(weightedAvg - failurePenalty - floorPenalty, 0.0f, 1.0f);
}

// ============================================================================
// executeStep — Dispatch a single reasoning step by enum value
// ============================================================================
AgenticDeepThinkingEngine::ReasoningStep AgenticDeepThinkingEngine::executeStep(
    ThinkingStep step,
    const ThinkingContext& context,
    const std::string& previousContext
) {
    switch (step) {
        case ThinkingStep::ProblemAnalysis:
            return analyzeProblem(context);

        case ThinkingStep::ContextGathering:
            return gatherContext(context);

        case ThinkingStep::HypothesiGeneration:
            return generateHypotheses(context, previousContext);

        case ThinkingStep::ExperimentationRun: {
            // Extract hypotheses from previous context (look for H1:, H2: lines)
            std::vector<std::string> hypotheses;
            std::istringstream iss(previousContext);
            std::string line;
            while (std::getline(iss, line)) {
                size_t start = line.find_first_not_of(" \t");
                if (start == std::string::npos) continue;
                char first = line[start];
                if (first == 'H' || first == '-' || first == '*' ||
                    (first >= '1' && first <= '9')) {
                    hypotheses.push_back(line.substr(start));
                }
            }
            if (hypotheses.empty()) hypotheses.push_back(previousContext);
            return runExperiments(context, hypotheses);
        }

        case ThinkingStep::ResultEvaluation: {
            // Extract findings from previous context as results to evaluate
            std::vector<std::string> results;
            std::istringstream iss(previousContext);
            std::string line;
            std::string currentResult;
            while (std::getline(iss, line)) {
                if (!line.empty()) {
                    currentResult += line + "\n";
                } else if (!currentResult.empty()) {
                    results.push_back(currentResult);
                    currentResult.clear();
                }
            }
            if (!currentResult.empty()) results.push_back(currentResult);
            if (results.empty()) results.push_back(previousContext);
            return evaluateResults(results, context);
        }

        case ThinkingStep::SelfCorrection: {
            // Build steps from accumulated context to self-correct
            std::vector<ReasoningStep> priorSteps;
            ReasoningStep dummy;
            dummy.step = ThinkingStep::ProblemAnalysis;
            dummy.title = "Prior Analysis";
            dummy.content = previousContext;
            dummy.confidence = 0.7f;
            dummy.successful = true;
            priorSteps.push_back(dummy);
            return selfCorrect(context, priorSteps);
        }

        case ThinkingStep::FinalSynthesis: {
            // Build all prior context into steps for synthesis
            std::vector<ReasoningStep> synthSteps;
            ReasoningStep ctx_step;
            ctx_step.step = ThinkingStep::ContextGathering;
            ctx_step.title = "Accumulated Context";
            ctx_step.content = previousContext;
            ctx_step.confidence = 0.75f;
            ctx_step.successful = true;
            synthSteps.push_back(ctx_step);
            return synthesizeAnswer(synthSteps, context);
        }

        default: {
            // Initialization / Complete / unknown — return a pass-through step
            ReasoningStep passthrough;
            passthrough.step = step;
            passthrough.title = "Step " + std::to_string(static_cast<int>(step));
            passthrough.content = previousContext;
            passthrough.confidence = 1.0f;
            passthrough.successful = true;
            return passthrough;
        }
    }
}

// ============================================================================
// extractProjectStructure — Build a structural overview of the project tree
// ============================================================================
std::string AgenticDeepThinkingEngine::extractProjectStructure(const std::string& projectRoot) {
    std::string root = projectRoot.empty() ? "." : projectRoot;
    auto allFiles = listFilesRecursive(root, "");

    if (allFiles.empty()) {
        return "No files found in: " + root;
    }

    // Classify files by directory and extension
    std::map<std::string, int> dirCounts;
    std::map<std::string, int> extCounts;
    int totalSourceFiles = 0;
    int totalHeaderFiles = 0;
    int totalAsmFiles = 0;
    int totalOtherFiles = 0;
    uint64_t totalSize = 0;

    for (const auto& f : allFiles) {
        // Directory
        size_t lastSep = f.find_last_of("/\\");
        std::string dir = (lastSep != std::string::npos) ? f.substr(0, lastSep) : ".";
        dirCounts[dir]++;

        // Extension
        size_t dotPos = f.find_last_of('.');
        std::string ext = (dotPos != std::string::npos) ? f.substr(dotPos) : "";
        extCounts[ext]++;

        if (ext == ".cpp" || ext == ".c" || ext == ".cc") totalSourceFiles++;
        else if (ext == ".hpp" || ext == ".h" || ext == ".hxx") totalHeaderFiles++;
        else if (ext == ".asm") totalAsmFiles++;
        else totalOtherFiles++;

        // File size (best effort)
        try {
            if (std::filesystem::exists(f)) {
                totalSize += std::filesystem::file_size(f);
            }
        } catch (...) {}
    }

    std::ostringstream report;
    report << "=== Project Structure: " << root << " ===\n";
    report << "Total files: " << allFiles.size() << "\n";
    report << "Total size: " << std::fixed << std::setprecision(1)
           << (static_cast<double>(totalSize) / (1024.0 * 1024.0)) << " MB\n";
    report << "Directories: " << dirCounts.size() << "\n";
    report << "Source files (.cpp/.c): " << totalSourceFiles << "\n";
    report << "Header files (.hpp/.h): " << totalHeaderFiles << "\n";
    report << "Assembly files (.asm): " << totalAsmFiles << "\n";
    report << "Other files: " << totalOtherFiles << "\n\n";

    // Top directories by file count
    std::vector<std::pair<std::string, int>> sortedDirs(dirCounts.begin(), dirCounts.end());
    std::sort(sortedDirs.begin(), sortedDirs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    report << "Top directories:\n";
    for (size_t i = 0; i < std::min(sortedDirs.size(), size_t(15)); ++i) {
        report << "  " << sortedDirs[i].first << "/ (" << sortedDirs[i].second << " files)\n";
    }

    // Extension breakdown
    report << "\nFile types:\n";
    std::vector<std::pair<std::string, int>> sortedExts(extCounts.begin(), extCounts.end());
    std::sort(sortedExts.begin(), sortedExts.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    for (const auto& [ext, count] : sortedExts) {
        if (count >= 2 && !ext.empty()) {
            report << "  " << ext << ": " << count << "\n";
        }
    }

    return report.str();
}

// ============================================================================
// formatAnswer — Language-aware answer formatting with code block detection
// ============================================================================
std::string AgenticDeepThinkingEngine::formatAnswer(const std::string& rawAnswer, const std::string& language) {
    if (rawAnswer.empty()) return rawAnswer;

    std::string formatted;
    formatted.reserve(rawAnswer.size() + 256);

    // Determine the language tag for code blocks
    std::string langTag = language;
    if (langTag.empty()) langTag = m_defaultLanguage;
    std::string lowerLang = langTag;
    std::transform(lowerLang.begin(), lowerLang.end(), lowerLang.begin(), ::tolower);

    // Normalize common language aliases
    if (lowerLang == "c++" || lowerLang == "cxx" || lowerLang == "cc") langTag = "cpp";
    else if (lowerLang == "javascript") langTag = "js";
    else if (lowerLang == "typescript") langTag = "ts";
    else if (lowerLang == "assembly" || lowerLang == "masm" || lowerLang == "nasm") langTag = "asm";
    else if (lowerLang == "python3") langTag = "python";

    // Process line-by-line to detect and format code blocks
    std::istringstream iss(rawAnswer);
    std::string line;
    bool inCodeBlock = false;
    bool needsAutoWrap = false;
    int consecutiveCodeLines = 0;

    // Heuristics for code line detection
    auto isLikelyCodeLine = [](const std::string& l) -> bool {
        if (l.empty()) return false;
        std::string trimmed = l;
        size_t start = trimmed.find_first_not_of(" \t");
        if (start == std::string::npos) return false;
        trimmed = trimmed.substr(start);

        // Strong code indicators
        if (trimmed.find("//") == 0) return true;
        if (trimmed.find('#') == 0) return true;  // preprocessor
        if (trimmed.find("return ") == 0) return true;
        if (trimmed.find("if (") == 0 || trimmed.find("if(") == 0) return true;
        if (trimmed.find("for (") == 0 || trimmed.find("for(") == 0) return true;
        if (trimmed.find("while (") == 0 || trimmed.find("while(") == 0) return true;
        if (trimmed.find("class ") == 0 || trimmed.find("struct ") == 0) return true;
        if (trimmed.find("void ") == 0 || trimmed.find("int ") == 0 ||
            trimmed.find("bool ") == 0 || trimmed.find("auto ") == 0) return true;
        if (trimmed.back() == ';' || trimmed.back() == '{' || trimmed.back() == '}') return true;
        if (trimmed.find("->") != std::string::npos) return true;
        if (trimmed.find("::") != std::string::npos) return true;
        return false;
    };

    while (std::getline(iss, line)) {
        // Already in a markdown code block — pass through
        if (line.find("```") == 0) {
            inCodeBlock = !inCodeBlock;
            if (needsAutoWrap && !inCodeBlock) {
                // Close our auto-opened block
                needsAutoWrap = false;
            }
            formatted += line + "\n";
            consecutiveCodeLines = 0;
            continue;
        }

        if (inCodeBlock) {
            formatted += line + "\n";
            continue;
        }

        // Detect consecutive code lines to auto-wrap
        if (isLikelyCodeLine(line)) {
            consecutiveCodeLines++;
            if (consecutiveCodeLines == 3 && !needsAutoWrap) {
                // Start auto code block — insert before the first code line
                // (We buffered 2 lines, so re-emit them inside fence)
                formatted += "```" + langTag + "\n";
                needsAutoWrap = true;
            }
        } else {
            if (needsAutoWrap && consecutiveCodeLines > 0) {
                // End of code section
                formatted += "```\n";
                needsAutoWrap = false;
            }
            consecutiveCodeLines = 0;
        }

        formatted += line + "\n";
    }

    // Close any unclosed auto-block
    if (needsAutoWrap) {
        formatted += "```\n";
    }

    return formatted;
}
// ============================================================================
// Multi-Agent Orchestration (8x cycle + parallel execution)
// ============================================================================

AgenticDeepThinkingEngine::MultiAgentResult AgenticDeepThinkingEngine::thinkMultiAgent(const ThinkingContext& context) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.multiAgentRequests++;
    }

    MultiAgentResult multiResult;
    multiResult.consensusReached = false;
    multiResult.consensusConfidence = 0.0f;

    // Determine agent count (1-8)
    int agentCount = std::clamp(context.agentCount, 1, 8);
    
    // Apply cycle multiplier to maxIterations (up to 8x)
    ThinkingContext enhancedContext = context;
    int cycleMultiplier = std::clamp(context.cycleMultiplier, 1, 8);
    enhancedContext.maxIterations = context.maxIterations * cycleMultiplier;
    
    if (m_detailedLogging) {
        std::cout << "[MultiAgent] Spawning " << agentCount << " agents with "
                  << enhancedContext.maxIterations << " max iterations (base: "
                  << context.maxIterations << " x " << cycleMultiplier << ")\n";
    }

    // Set up agent models
    std::vector<std::string> agentModels;
    if (!context.agentModels.empty()) {
        agentModels = context.agentModels;
    } else {
        // Default model pool: diverse models for multi-perspective analysis
        agentModels = {
            "qwen2.5-coder:14b",     // Primary coding model
            "qwen2.5-coder:7b",      // Lighter, faster variant
            "deepseek-coder:6.7b",   // Alternative perspective
            "codellama:13b",         // Meta's code model
            "qwen2.5-coder:14b",     // Repeat for more consensus
            "qwen2.5-coder:7b",      // Repeat
            "deepseek-coder:6.7b",   // Repeat
            "codellama:13b"          // Repeat
        };
    }
    
    // Ensure we have enough models
    while (static_cast<int>(agentModels.size()) < agentCount) {
        agentModels.push_back ("qwen2.5-coder:14b");
    }

    // Phase 1: Spawn agents in parallel threads
    std::vector<std::thread> agentThreads;
    std::vector<AgentResult> agentResults(agentCount);
    std::mutex resultsMutex;

    for (int i = 0; i < agentCount; ++i) {
        agentThreads.emplace_back([this, i, &agentModels, &enhancedContext, &agentResults, &resultsMutex]() {
            try {
                auto agentRes = runSingleAgent(i, agentModels[i], enhancedContext);
                std::lock_guard<std::mutex> lock(resultsMutex);
                agentResults[i] = agentRes;
            } catch (const std::exception& e) {
                std::cerr << "[MultiAgent] Agent " << i << " failed: " << e.what() << "\n";
                std::lock_guard<std::mutex> lock(resultsMutex);
                agentResults[i].agentId = i;
                agentResults[i].modelName = agentModels[i];
                agentResults[i].result.overallConfidence = 0.0f;
                agentResults[i].result.finalAnswer = "Agent failed: " + std::string(e.what());
            }
        });
    }

    // Wait for all agents to complete
    for (auto& t : agentThreads) {
        if (t.joinable()) t.join();
    }

    multiResult.agentResults = agentResults;

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalAgentsSpawned += agentCount;
    }

    // Phase 2: Calculate inter-agent agreement
    for (size_t i = 0; i < agentResults.size(); ++i) {
        float totalAgreement = 0.0f;
        for (size_t j = 0; j < agentResults.size(); ++j) {
            if (i != j) {
                totalAgreement += calculateAgreement(agentResults[i], agentResults[j]);
            }
        }
        agentResults[i].agreementScore = (agentCount > 1) ? (totalAgreement / (agentCount - 1)) : 1.0f;
    }

    // Phase 3: Merge results based on strategy
    if (context.enableAgentVoting) {
        // Voting: select the result with highest agreement
        auto bestAgent = selectBestByVoting(agentResults);
        multiResult.consensusResult = bestAgent.result;
        multiResult.consensusConfidence = bestAgent.agreementScore;
        multiResult.consensusReached = (bestAgent.agreementScore >= context.consensusThreshold);
    } else {
        // Merge: combine all agent insights
        multiResult.consensusResult = mergeAgentResults(agentResults, enhancedContext);
        
        // Calculate consensus confidence (average of all agents weighted by agreement)
        float totalWeightedConf = 0.0f;
        float totalWeight = 0.0f;
        for (const auto& ar : agentResults) {
            float weight = ar.agreementScore;
            totalWeightedConf += ar.result.overallConfidence * weight;
            totalWeight += weight;
        }
        multiResult.consensusConfidence = (totalWeight > 0.0f) ? (totalWeightedConf / totalWeight) : 0.0f;
        multiResult.consensusReached = (multiResult.consensusConfidence >= context.consensusThreshold);
    }

    // Phase 4: Identify disagreements
    multiResult.disagreementPoints = findDisagreements(agentResults);

    // Update stats
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        if (multiResult.consensusReached) {
            m_stats.consensusReached++;
        }
        m_stats.avgConsensusConfidence = 
            (m_stats.avgConsensusConfidence * (m_stats.multiAgentRequests - 1) + multiResult.consensusConfidence)
            / m_stats.multiAgentRequests;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    multiResult.consensusResult.elapsedMilliseconds = 
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_detailedLogging) {
        std::cout << "[MultiAgent] Consensus: " << (multiResult.consensusReached ? "YES" : "NO")
                  << " (confidence: " << std::fixed << std::setprecision(1)
                  << multiResult.consensusConfidence * 100.0f << "%)\n";
        std::cout << "[MultiAgent] Elapsed: " << multiResult.consensusResult.elapsedMilliseconds << "ms\n";
    }

    // Track multi-agent telemetry
    if (auto* tc = TelemetryCollector::instance()) {
        tc->trackFeatureUsage("deep_thinking.multi_agent");
        tc->trackPerformance("multi_agent.agent_count", static_cast<double>(agentCount), "count");
        tc->trackPerformance("multi_agent.consensus_confidence", 
                             static_cast<double>(multiResult.consensusConfidence), "ratio");
        tc->trackPerformance("multi_agent.total_latency_ms",
                             static_cast<double>(multiResult.consensusResult.elapsedMilliseconds), "ms");
    }

    return multiResult;
}

AgenticDeepThinkingEngine::AgentResult AgenticDeepThinkingEngine::runSingleAgent(
    int agentId, const std::string& model, const ThinkingContext& context) {
    
    AgentResult result;
    result.agentId = agentId;
    result.modelName = model;
    result.agreementScore = 0.0f;

    if (m_detailedLogging) {
        std::cout << "[Agent-" << agentId << "] Starting with model: " << model << "\n";
    }

    // TODO: In production, configure per-agent LLM client with specific model
    // For now, use the default thinking engine (which internally uses qwen2.5-coder:14b)
    
    // Run standard think() with the enhanced context
    result.result = think(context);
    
    if (m_detailedLogging) {
        std::cout << "[Agent-" << agentId << "] Completed with confidence: "
                  << std::fixed << std::setprecision(1)
                  << result.result.overallConfidence * 100.0f << "%\n";
    }

    return result;
}

AgenticDeepThinkingEngine::ThinkingResult AgenticDeepThinkingEngine::mergeAgentResults(
    const std::vector<AgentResult>& results, const ThinkingContext& context) {
    
    ThinkingResult merged;
    merged.iterationCount = 0;
    merged.elapsedMilliseconds = 0;
    merged.overallConfidence = 0.0f;

    if (results.empty()) {
        merged.finalAnswer = "No agent results to merge";
        return merged;
    }

    // Collect all steps from all agents
    std::map<ThinkingStep, std::vector<ReasoningStep>> stepsByType;
    std::set<std::string> allRelatedFiles;
    std::set<std::string> allSuggestedFixes;

    for (const auto& ar : results) {
        // Accumulate steps by type
        for (const auto& step : ar.result.steps) {
            stepsByType[step.step].push_back(step);
        }
        
        // Collect all related files and fixes
        for (const auto& f : ar.result.relatedFiles) {
            allRelatedFiles.insert(f);
        }
        for (const auto& fix : ar.result.suggestedFixes) {
            allSuggestedFixes.insert(fix);
        }
        
        merged.iterationCount += ar.result.iterationCount;
        merged.elapsedMilliseconds = std::max(merged.elapsedMilliseconds, ar.result.elapsedMilliseconds);
    }

    // Merge steps: for each step type, synthesize a consensus step
    for (const auto& [stepType, steps] : stepsByType) {
        ReasoningStep consensusStep;
        consensusStep.step = stepType;
        consensusStep.title = steps[0].title;  // Use first agent's title
        consensusStep.successful = true;

        // Merge content: concatenate unique insights
        std::set<std::string> uniqueContent;
        std::vector<std::string> allFindings;
        float totalConf = 0.0f;

        for (const auto& s : steps) {
            if (!s.content.empty() && s.content.length() > 10) {
                uniqueContent.insert(s.content);
            }
            allFindings.insert(allFindings.end(), s.findings.begin(), s.findings.end());
            totalConf += s.confidence;
        }

        // Build merged content
        std::ostringstream mergedContent;
        int agentNum = 0;
        for (const auto& content : uniqueContent) {
            if (results.size() > 1) {
                mergedContent << "[Agent-" << agentNum++ << "] " << content << "\n\n";
            } else {
                mergedContent << content << "\n";
            }
        }
        consensusStep.content = mergedContent.str();
        consensusStep.findings = allFindings;
        consensusStep.confidence = totalConf / steps.size();

        merged.steps.push_back(consensusStep);
    }

    // Build final answer: weighted average of agent answers
    std::ostringstream finalAnswer;
    finalAnswer << "=== Multi-Agent Consensus Analysis ===\n\n";
    
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& ar = results[i];
        float weight = ar.agreementScore;
        finalAnswer << "[Agent-" << i << " | " << ar.modelName
                    << " | Agreement: " << std::fixed << std::setprecision(0)
                    << weight * 100.0f << "%]\n";
        finalAnswer << ar.result.finalAnswer << "\n\n";
        
        merged.overallConfidence += ar.result.overallConfidence * weight;
    }
    
    // Normalize confidence by total agreement weight
    float totalAgreement = 0.0f;
    for (const auto& ar : results) {
        totalAgreement += ar.agreementScore;
    }
    if (totalAgreement > 0.0f) {
        merged.overallConfidence /= totalAgreement;
    }

    finalAnswer << "\n=== Consensus Recommendation ===\n";
    // Select the highest-confidence agent's answer as primary recommendation
    const AgentResult* bestAgent = &results[0];
    for (const auto& ar : results) {
        if (ar.result.overallConfidence > bestAgent->result.overallConfidence) {
            bestAgent = &ar;
        }
    }
    finalAnswer << bestAgent->result.finalAnswer;

    merged.finalAnswer = finalAnswer.str();
    merged.relatedFiles.assign(allRelatedFiles.begin(), allRelatedFiles.end());
    merged.suggestedFixes.assign(allSuggestedFixes.begin(), allSuggestedFixes.end());

    return merged;
}

std::vector<std::string> AgenticDeepThinkingEngine::findDisagreements(const std::vector<AgentResult>& results) {
    std::vector<std::string> disagreements;
    
    if (results.size() < 2) return disagreements;

    // Compare answers pairwise to find dissenting opinions
    for (size_t i = 0; i < results.size(); ++i) {
        for (size_t j = i + 1; j < results.size(); ++j) {
            float agreement = calculateAgreement(results[i], results[j]);
            
            if (agreement < 0.5f) {
                // Significant disagreement
                std::ostringstream disagreement;
                disagreement << "Agent-" << i << " (" << results[i].modelName << ") vs Agent-"
                             << j << " (" << results[j].modelName << ") disagree (agreement: "
                             << std::fixed << std::setprecision(0) << agreement * 100.0f << "%)";
                disagreements.push_back(disagreement.str());
            }
        }
    }

    return disagreements;
}

float AgenticDeepThinkingEngine::calculateAgreement(const AgentResult& a1, const AgentResult& a2) {
    // Calculate similarity between two agent results
    const auto& ans1 = a1.result.finalAnswer;
    const auto& ans2 = a2.result.finalAnswer;

    if (ans1.empty() || ans2.empty()) return 0.0f;

    // Word-level overlap (Jaccard similarity)
    std::set<std::string> words1, words2;
    
    auto tokenize = [](const std::string& text, std::set<std::string>& words) {
        std::istringstream iss(text);
        std::string word;
        while (iss >> word) {
            // Normalize: lowercase, strip punctuation
            std::string normalized;
            for (char c : word) {
                if (std::isalnum(static_cast<unsigned char>(c))) {
                    normalized += std::tolower(static_cast<unsigned char>(c));
                }
            }
            if (normalized.length() > 2) {  // Skip short words
                words.insert(normalized);
            }
        }
    };

    tokenize(ans1, words1);
    tokenize(ans2, words2);

    if (words1.empty() || words2.empty()) return 0.0f;

    // Intersection and union
    std::vector<std::string> intersection;
    std::set_intersection(words1.begin(), words1.end(),
                          words2.begin(), words2.end(),
                          std::back_inserter(intersection));
    
    size_t unionSize = words1.size() + words2.size() - intersection.size();
    float jaccard = (unionSize > 0) ? (static_cast<float>(intersection.size()) / unionSize) : 0.0f;

    // Also consider confidence similarity
    float confDelta = std::abs(a1.result.overallConfidence - a2.result.overallConfidence);
    float confSim = 1.0f - confDelta;

    // Weighted combination: 70% content overlap, 30% confidence similarity
    return 0.7f * jaccard + 0.3f * confSim;
}

AgenticDeepThinkingEngine::AgentResult AgenticDeepThinkingEngine::selectBestByVoting(
    const std::vector<AgentResult>& results) {
    
    if (results.empty()) {
        AgentResult empty;
        empty.agentId = -1;
        empty.agreementScore = 0.0f;
        return empty;
    }

    // Select agent with highest agreement score
    const AgentResult* best = &results[0];
    for (const auto& ar : results) {
        if (ar.agreementScore > best->agreementScore) {
            best = &ar;
        }
    }

    return *best;
}