#include "agentic_deep_thinking_engine.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>

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
    
    // Parse query for keywords
    std::istringstream iss(query);
    std::vector<std::string> keywords;
    std::string word;
    while (iss >> word) {
        if (word.length() > 3) {
            keywords.push_back(word);
        }
    }

    // Search for files containing these keywords
    // TODO: Implement file search with scoring
    
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
    // TODO: Implement pattern search
    return "Pattern search not yet implemented";
}

bool AgenticDeepThinkingEngine::evaluateAnswer(const std::string& answer, const ThinkingContext& context) {
    // TODO: Implement answer evaluation
    return !answer.empty();
}

std::string AgenticDeepThinkingEngine::refineAnswer(const std::string& currentAnswer, const std::string& feedback) {
    // TODO: Implement answer refinement
    return currentAnswer;
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

    step.confidence = 0.85f;
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
    // TODO: Implement code search
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
    std::vector<std::string> solutions;
    
    // Generate simple placeholder solutions
    for (int i = 0; i < count; ++i) {
        solutions.push_back("Hypothesis " + std::to_string(i + 1) + ": " + problem);
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
    // TODO: Implement hypothesis testing
    return "Test result for: " + hypothesis;
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
    // Simple scoring - could be enhanced with ML
    if (result.length() > 0) {
        return 0.7f;
    }
    return 0.0f;
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
    return "Corrected: " + flaw;
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
    // TODO: Implement recursive file listing
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
    // TODO: Implement file search with line numbers
    return results;
}

std::vector<std::string> AgenticDeepThinkingEngine::findCommonPatterns(const std::vector<std::string>& codeSnippets) {
    std::vector<std::string> patterns;
    // TODO: Implement pattern extraction
    return patterns;
}

std::string AgenticDeepThinkingEngine::identifyBestMatch(const std::string& query, const std::vector<std::string>& candidates) {
    if (candidates.empty()) {
        return "";
    }
    // TODO: Implement similarity scoring
    return candidates[0];
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
