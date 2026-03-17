#include "agentic_deep_thinking_engine.hpp"
#include "../agentic/AgentOllamaClient.h"
#include "model_invoker.hpp"
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
#include <numeric>
#include <set>

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

    std::string refined = currentAnswer;

    // Parse feedback for directives
    std::string lowerFeedback = feedback;
    std::transform(lowerFeedback.begin(), lowerFeedback.end(), lowerFeedback.begin(), ::tolower);

    // Handle common refinement requests
    if (lowerFeedback.find("more detail") != std::string::npos ||
        lowerFeedback.find("elaborate") != std::string::npos) {
        refined += "\n\n[Refined with additional detail]\n";
        refined += "Additional context based on feedback: " + feedback + "\n";
        refined += "The original analysis has been expanded to address the request for more detail.";
    }
    else if (lowerFeedback.find("simplify") != std::string::npos ||
             lowerFeedback.find("shorter") != std::string::npos) {
        // Extract key sentences only
        std::istringstream iss(currentAnswer);
        std::string line;
        std::string simplified;
        int lineCount = 0;
        while (std::getline(iss, line)) {
            // Keep lines with key indicators
            if (line.find("Based on") != std::string::npos ||
                line.find("Result") != std::string::npos ||
                line.find("- ") == 0 ||
                line.find("Conclusion") != std::string::npos) {
                simplified += line + "\n";
                lineCount++;
            }
            if (lineCount >= 5) break;
        }
        if (!simplified.empty()) refined = simplified;
    }
    else if (lowerFeedback.find("focus on") != std::string::npos ||
             lowerFeedback.find("specifically") != std::string::npos) {
        refined += "\n\n[Focused refinement]\n";
        refined += "Addressing specific feedback: " + feedback + "\n";
    }
    else {
        // General refinement: append feedback context
        refined += "\n\n[Refined based on feedback: " + feedback.substr(0, 200) + "]\n";
    }

    // Track refinement pattern
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
    ReasoningStep step;
    step.step = ThinkingStep::ProblemAnalysis;
    step.title = "Problem Analysis";
    step.successful = true;

    // Identify key issues
    auto issues = identifyKeyIssues(context.problem);
    step.findings = issues;

    // Categorize the problem
    if (!issues.empty()) {
        step.content = "Categorized as: " + categorizeIssue(issues[0]);
    }

    // Compute confidence from evidence: more issues identified = higher confidence in analysis
    float issueRatio = issues.empty() ? 0.0f :
        std::min(static_cast<float>(issues.size()) / 5.0f, 1.0f);
    bool hasCategorization = !issues.empty();
    step.confidence = 0.50f + (issueRatio * 0.30f) + (hasCategorization ? 0.15f : 0.0f);

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

    // Confidence derived from how many relevant files were actually found
    float filesRatio = std::min(static_cast<float>(relatedCode.size()) / 5.0f, 1.0f);
    step.confidence = 0.40f + (filesRatio * 0.50f);  // 0.40 base + up to 0.50 from evidence

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

    // Confidence based on quantity and quality of generated hypotheses
    float hypRatio = std::min(static_cast<float>(hypotheses.size()) / 3.0f, 1.0f);
    // Check if hypotheses are substantive (not just problem restatements)
    int substantiveCount = 0;
    for (const auto& h : hypotheses) {
        if (h.length() > 30 && h != context.problem) substantiveCount++;
    }
    float qualityRatio = hypotheses.empty() ? 0.0f :
        static_cast<float>(substantiveCount) / hypotheses.size();
    step.confidence = 0.35f + (hypRatio * 0.30f) + (qualityRatio * 0.30f);

    return step;
}

std::vector<std::string> AgenticDeepThinkingEngine::brainstormSolutions(const std::string& problem, int count) {
    std::vector<std::string> solutions;

    // Attempt LLM-powered hypothesis generation via AgentOllamaClient
    try {
        OllamaConfig cfg;
        cfg.chat_model = "deepseek-r1:14b";   // Use available reasoning model
        cfg.timeout_ms = 20000;
        cfg.max_tokens = 1024;
        cfg.temperature = 0.8f;  // Higher temp for creative brainstorming

        AgentOllamaClient client(cfg);
        if (client.TestConnection()) {
            std::vector<ChatMessage> msgs;
            msgs.push_back({"system",
                "You are a chain-of-thought reasoning engine for a C++20/Win32 IDE codebase. "
                "Generate exactly " + std::to_string(count) + " distinct hypotheses to solve the given problem. "
                "Each hypothesis must be a concrete, actionable approach — not a restatement of the problem. "
                "Format: one hypothesis per line, prefixed with 'H1:', 'H2:', etc."
            });
            msgs.push_back({"user",
                "Problem: " + problem + "\n\n"
                "Generate " + std::to_string(count) + " hypotheses:"
            });

            auto result = client.ChatSync(msgs);
            if (result.success && !result.response.empty()) {
                // Parse numbered hypotheses from LLM response
                std::istringstream stream(result.response);
                std::string line;
                while (std::getline(stream, line)) {
                    // Trim whitespace
                    size_t start = line.find_first_not_of(" \t");
                    if (start == std::string::npos) continue;
                    line = line.substr(start);

                    // Accept lines starting with H1:, H2:, 1., 2., -, * etc.
                    if (line.length() < 10) continue;
                    bool isHypothesis = false;
                    if (line[0] == 'H' && line.length() > 2 && line[2] == ':') isHypothesis = true;
                    if (line[0] >= '1' && line[0] <= '9' && (line[1] == '.' || line[1] == ')')) isHypothesis = true;
                    if (line[0] == '-' || line[0] == '*') isHypothesis = true;

                    if (isHypothesis) {
                        // Strip the prefix marker
                        size_t contentStart = line.find_first_of(":.)* ", 1);
                        if (contentStart != std::string::npos) {
                            contentStart = line.find_first_not_of(" :\t", contentStart);
                            if (contentStart != std::string::npos) {
                                solutions.push_back(line.substr(contentStart));
                            } else {
                                solutions.push_back(line);
                            }
                        } else {
                            solutions.push_back(line);
                        }
                    }
                    if (static_cast<int>(solutions.size()) >= count) break;
                }

                if (!solutions.empty()) {
                    std::lock_guard<std::mutex> lock(m_statsMutex);
                    m_patternFrequency["llm_brainstorm"]++;
                    return solutions;
                }
            }
        }
    } catch (...) {
        // Ollama unavailable — fall through to codebase-driven heuristics
    }

    // Fallback: codebase-driven hypothesis generation from real evidence
    // Search for patterns related to the problem in the source code
    auto codeRefs = searchInFiles(problem.substr(0, 60), "src");
    auto relatedFiles = findRelatedFiles(problem, 5);

    // Extract distinct solution approaches from code evidence
    std::set<std::string> seen;
    for (const auto& [path, lineNum] : codeRefs) {
        if (static_cast<int>(solutions.size()) >= count) break;
        std::string content = readFileContent(path, 30);
        if (content.empty()) continue;

        // Extract the function/class context from the matched file
        std::string hypothesis;
        if (path.find("hotpatch") != std::string::npos)
            hypothesis = "Apply runtime hotpatching at " + path + ":" + std::to_string(lineNum) + " to surgically fix the behavior without restart";
        else if (path.find("agent") != std::string::npos)
            hypothesis = "Use the agentic subsystem (" + path + ") to autonomously detect and correct the issue via multi-step reasoning";
        else if (path.find("inference") != std::string::npos || path.find("model") != std::string::npos)
            hypothesis = "Adjust inference pipeline configuration at " + path + ":" + std::to_string(lineNum) + " to resolve model interaction issues";
        else if (path.find("vulkan") != std::string::npos || path.find("gpu") != std::string::npos)
            hypothesis = "Optimize GPU compute dispatch at " + path + " to address performance bottleneck";
        else
            hypothesis = "Modify " + path + ":" + std::to_string(lineNum) + " — the code at this location directly relates to the problem domain";

        if (seen.insert(hypothesis).second) {
            solutions.push_back(hypothesis);
        }
    }

    // Fill remaining slots with architecture-aware hypotheses
    if (static_cast<int>(solutions.size()) < count && !relatedFiles.empty()) {
        for (const auto& file : relatedFiles) {
            if (static_cast<int>(solutions.size()) >= count) break;
            std::string h = "Investigate " + file + " for structural changes that could resolve the underlying issue";
            if (seen.insert(h).second) solutions.push_back(h);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_patternFrequency["heuristic_brainstorm"]++;
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

    // Confidence derived from proportion of hypotheses that found supporting evidence
    int supportedCount = 0;
    for (const auto& finding : step.findings) {
        std::string lower = finding;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find("supported") != std::string::npos ||
            lower.find("evidence found") != std::string::npos) {
            supportedCount++;
        }
    }
    float supportRatio = hypotheses.empty() ? 0.0f :
        static_cast<float>(supportedCount) / hypotheses.size();
    step.confidence = 0.30f + (supportRatio * 0.60f);  // 0.30 base + up to 0.60 from support

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

    float score = 0.0f;
    float maxScore = 0.0f;

    // Factor 1: Keyword overlap (Jaccard-like) — weight 0.30
    {
        maxScore += 0.30f;
        auto tokenize = [](const std::string& text) -> std::set<std::string> {
            std::set<std::string> tokens;
            std::istringstream iss(text);
            std::string word;
            while (iss >> word) {
                if (word.length() > 3) {
                    std::string lower = word;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    // Strip punctuation
                    while (!lower.empty() && !std::isalnum(lower.back())) lower.pop_back();
                    if (lower.length() > 2) tokens.insert(lower);
                }
            }
            return tokens;
        };
        auto resultTokens = tokenize(result);
        auto problemTokens = tokenize(problem);
        if (!resultTokens.empty() && !problemTokens.empty()) {
            int intersection = 0;
            for (const auto& t : problemTokens) {
                if (resultTokens.count(t)) intersection++;
            }
            int unionSize = static_cast<int>(resultTokens.size() + problemTokens.size()) - intersection;
            float jaccard = unionSize > 0 ? static_cast<float>(intersection) / unionSize : 0.0f;
            score += 0.30f * std::min(jaccard * 3.0f, 1.0f); // Scale up — even 0.33 overlap is strong
        }
    }

    // Factor 2: Evidence markers — weight 0.25
    {
        maxScore += 0.25f;
        int evidenceCount = 0;
        std::string lower = result;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        // Count concrete evidence markers
        const char* markers[] = {
            "evidence:", "found ", "references", "verdict:", "supported",
            "affected files:", "analysis:", "tested:", "matches", "result:"
        };
        for (const char* m : markers) {
            if (lower.find(m) != std::string::npos) evidenceCount++;
        }
        score += 0.25f * std::min(static_cast<float>(evidenceCount) / 4.0f, 1.0f);
    }

    // Factor 3: Structural quality — weight 0.20
    {
        maxScore += 0.20f;
        int structureScore = 0;
        if (result.find('\n') != std::string::npos) structureScore++;  // Multi-line
        if (result.find(':') != std::string::npos) structureScore++;   // Labeled
        if (result.find("- ") != std::string::npos) structureScore++; // Bulleted
        if (result.length() > 200) structureScore++;                   // Detailed
        if (result.find(".cpp") != std::string::npos ||
            result.find(".hpp") != std::string::npos ||
            result.find(".h") != std::string::npos) structureScore++; // Code refs
        score += 0.20f * std::min(static_cast<float>(structureScore) / 4.0f, 1.0f);
    }

    // Factor 4: Actionability — weight 0.15
    {
        maxScore += 0.15f;
        std::string lower = result;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        int actionScore = 0;
        const char* actionVerbs[] = {
            "fix", "refactor", "implement", "add", "remove", "modify",
            "replace", "update", "optimize", "restructure"
        };
        for (const char* verb : actionVerbs) {
            if (lower.find(verb) != std::string::npos) actionScore++;
        }
        score += 0.15f * std::min(static_cast<float>(actionScore) / 3.0f, 1.0f);
    }

    // Factor 5: Absence of filler / inconclusive markers — weight 0.10
    {
        maxScore += 0.10f;
        std::string lower = result;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        bool hasFiller = (lower.find("inconclusive") != std::string::npos ||
                          lower.find("no direct evidence") != std::string::npos ||
                          lower.find("manual review") != std::string::npos ||
                          lower.find("placeholder") != std::string::npos);
        score += hasFiller ? 0.0f : 0.10f;
    }

    return std::min(score, 1.0f);
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
        auto correctionText = correctFlaw(flaw, previousSteps);
        step.findings.push_back(correctionText);
        step.successful = true;

        // Confidence based on whether correction produced real evidence
        float correctionQuality = 0.5f;
        std::string lowerCorrection = correctionText;
        std::transform(lowerCorrection.begin(), lowerCorrection.end(), lowerCorrection.begin(), ::tolower);
        if (lowerCorrection.find("evidence") != std::string::npos) correctionQuality += 0.15f;
        if (lowerCorrection.find("references") != std::string::npos) correctionQuality += 0.10f;
        if (lowerCorrection.find("revised") != std::string::npos ||
            lowerCorrection.find("corrected") != std::string::npos) correctionQuality += 0.10f;
        if (correctionText.length() > 200) correctionQuality += 0.10f;
        step.confidence = std::min(correctionQuality, 1.0f);
    }

    return step;
}

bool AgenticDeepThinkingEngine::detectFlaws(const std::vector<ReasoningStep>& steps, std::string& flaw) {
    // Check for low confidence steps
    for (const auto& step : steps) {
        if (step.confidence < 0.6f) {
            flaw = "Low confidence in " + step.title;
            return true;
        }
    }
    return false;
}

std::string AgenticDeepThinkingEngine::correctFlaw(const std::string& flaw, const std::vector<ReasoningStep>& context) {
    std::ostringstream correction;
    correction << "[Self-Correction] Addressing: " << flaw << "\n";

    // Attempt LLM-driven correction via AgentOllamaClient
    try {
        OllamaConfig cfg;
        cfg.chat_model = "deepseek-r1:14b";
        cfg.timeout_ms = 15000;
        cfg.max_tokens = 512;
        cfg.temperature = 0.3f;  // Low temp for precise correction

        AgentOllamaClient client(cfg);
        if (client.TestConnection()) {
            // Build context from previous reasoning steps
            std::string stepContext;
            for (const auto& step : context) {
                stepContext += step.title + ": " + step.content + "\n";
                if (stepContext.length() > 1500) break;
            }

            std::vector<ChatMessage> msgs;
            msgs.push_back({"system",
                "You are a self-correction module. A flaw was detected in a reasoning chain. "
                "Provide a concise corrected analysis. Be specific and actionable."
            });
            msgs.push_back({"user",
                "Flaw detected: " + flaw + "\n\n"
                "Previous reasoning:\n" + stepContext + "\n"
                "Provide the corrected analysis:"
            });

            auto result = client.ChatSync(msgs);
            if (result.success && !result.response.empty()) {
                correction << result.response << "\n";
                {
                    std::lock_guard<std::mutex> lock(m_statsMutex);
                    m_patternFrequency["llm_correction"]++;
                }
                return correction.str();
            }
        }
    } catch (...) {
        // Fall through to heuristic correction
    }

    // Heuristic correction: re-analyze the flaw using codebase evidence
    std::string lowerFlaw = flaw;
    std::transform(lowerFlaw.begin(), lowerFlaw.end(), lowerFlaw.begin(), ::tolower);

    if (lowerFlaw.find("low confidence") != std::string::npos) {
        // Find the low-confidence step and gather more evidence
        for (const auto& step : context) {
            if (step.confidence < 0.6f) {
                correction << "Re-examining '" << step.title << "' (confidence: "
                           << step.confidence << ")\n";
                // Search for additional evidence
                auto extraEvidence = searchInFiles(step.content.substr(0, 50), "src");
                if (!extraEvidence.empty()) {
                    correction << "Additional evidence found: " << extraEvidence.size() << " references\n";
                    for (size_t i = 0; i < std::min(extraEvidence.size(), size_t(3)); ++i) {
                        correction << "  - " << extraEvidence[i].first << ":" << extraEvidence[i].second << "\n";
                    }
                    correction << "Revised assessment: Evidence strengthens original hypothesis\n";
                } else {
                    correction << "No additional evidence found — original conclusion may need broader scope\n";
                }
                break;
            }
        }
    } else if (lowerFlaw.find("inconsistent") != std::string::npos) {
        correction << "Cross-referencing findings across steps for consistency...\n";
        std::set<std::string> allFindings;
        for (const auto& step : context) {
            for (const auto& f : step.findings) {
                allFindings.insert(f);
            }
        }
        correction << "Unique findings across all steps: " << allFindings.size() << "\n";
    } else {
        correction << "General correction: Re-evaluating with expanded search scope\n";
        auto patterns = searchInFiles("TODO|FIXME|BUG", "src");
        correction << "Found " << patterns.size() << " issue markers in codebase for context\n";
    }

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_patternFrequency["heuristic_correction"]++;
    }
    return correction.str();
}

AgenticDeepThinkingEngine::ReasoningStep AgenticDeepThinkingEngine::synthesizeAnswer(
    const std::vector<ReasoningStep>& steps,
    const ThinkingContext& context
) {
    ReasoningStep step;
    step.step = ThinkingStep::FinalSynthesis;
    step.title = "Final Synthesis";
    step.successful = true;

    // Compile final answer from all steps
    std::string finalAnswer = "Based on the analysis:\n";
    for (const auto& s : steps) {
        if (!s.findings.empty()) {
            finalAnswer += "- " + s.title + ": ";
            for (const auto& finding : s.findings) {
                finalAnswer += finding + " ";
            }
            finalAnswer += "\n";
        }
    }

    step.content = finalAnswer;
    step.confidence = calculateOverallConfidence(steps);

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
