#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>

/**
 * @class AgenticDeepThinkingEngine
 * @brief Autonomous reasoning system with Chain-of-Thought for complex code problems
 * 
 * Features:
 * - Multi-step reasoning with intermediate justification
 * - File-based research and context gathering
 * - Self-correction and refinement loops
 * - Memory of previous analyses
 * - Multi-modal problem solving
 */
class AgenticDeepThinkingEngine {
public:
    enum class ThinkingStep {
        Initialization = 0,
        ProblemAnalysis = 1,
        ContextGathering = 2,
        HypothesiGeneration = 3,
        ExperimentationRun = 4,
        ResultEvaluation = 5,
        SelfCorrection = 6,
        FinalSynthesis = 7,
        Complete = 8
    };

    struct ThinkingContext {
        std::string problem;
        std::string language;
        std::string projectRoot;
        int maxTokens = 2048;
        bool deepResearch = false;
        bool allowSelfCorrection = true;
        int maxIterations = 5;
    };

    struct ReasoningStep {
        ThinkingStep step;
        std::string title;
        std::string content;
        std::vector<std::string> findings;
        float confidence;
        bool successful;
    };

    struct ThinkingResult {
        std::string finalAnswer;
        std::vector<ReasoningStep> steps;
        std::vector<std::string> suggestedFixes;
        std::vector<std::string> relatedFiles;
        float overallConfidence;
        int iterationCount;
        long long elapsedMilliseconds;
        bool requiresUserInput;
        std::string userInputRequest;
    };

    explicit AgenticDeepThinkingEngine();
    ~AgenticDeepThinkingEngine();

    // Core API
    ThinkingResult think(const ThinkingContext& context);
    
    // Streaming API
    void startThinking(
        const ThinkingContext& context,
        std::function<void(const ReasoningStep&)> onStepComplete,
        std::function<void(float)> onProgressUpdate,
        std::function<void(const std::string&)> onError
    );

    void cancelThinking();
    bool isThinking() const { return m_thinking.load(); }

    // Configuration
    void setMaxThinkingTime(int milliseconds) { m_maxThinkingTime = milliseconds; }
    void setDefaultLanguage(const std::string& language) { m_defaultLanguage = language; }
    void enableDetailedLogging() { m_detailedLogging = true; }
    void disableDetailedLogging() { m_detailedLogging = false; }

    // File research
    std::vector<std::string> findRelatedFiles(const std::string& query, int maxResults = 5);
    std::string analyzeFile(const std::string& filePath);
    std::string searchProjectForPattern(const std::string& pattern);

    // Self-correction
    bool evaluateAnswer(const std::string& answer, const ThinkingContext& context);
    std::string refineAnswer(const std::string& currentAnswer, const std::string& feedback);

    // Memory and learning
    void saveThinkingResult(const std::string& key, const ThinkingResult& result);
    ThinkingResult* getCachedThinking(const std::string& key);
    void clearMemory();
    std::vector<std::pair<std::string, int>> getMostUsedPatterns() const;

    // Statistics
    struct ThinkingStats {
        int totalThinkingRequests = 0;
        int successfulThinking = 0;
        int failedThinking = 0;
        float avgThinkingTime = 0.0f;
        float avgConfidence = 0.0f;
        int cacheHits = 0;
        std::map<ThinkingStep, int> stepFrequency;
    };
    ThinkingStats getStats() const;
    void resetStats();

private:
    // Core reasoning engine
    std::vector<ReasoningStep> performChainOfThought(const ThinkingContext& context);
    ReasoningStep executeStep(ThinkingStep step, const ThinkingContext& context, const std::string& previousContext);

    // Problem analysis
    ReasoningStep analyzeProblem(const ThinkingContext& context);
    std::vector<std::string> identifyKeyIssues(const std::string& problem);
    std::string categorizeIssue(const std::string& issue);

    // Context gathering
    ReasoningStep gatherContext(const ThinkingContext& context);
    std::vector<std::string> findRelevantCode(const std::string& problem, int maxResults = 10);
    std::string extractProjectStructure(const std::string& projectRoot);

    // Hypothesis generation
    ReasoningStep generateHypotheses(const ThinkingContext& context, const std::string& analysis);
    std::vector<std::string> brainstormSolutions(const std::string& problem, int count = 5);

    // Experimentation
    ReasoningStep runExperiments(const ThinkingContext& context, const std::vector<std::string>& hypotheses);
    std::string testHypothesis(const std::string& hypothesis, const ThinkingContext& context);

    // Evaluation
    ReasoningStep evaluateResults(const std::vector<std::string>& results, const ThinkingContext& context);
    float scoreResult(const std::string& result, const std::string& problem);

    // Self-correction
    ReasoningStep selfCorrect(const ThinkingContext& context, const std::vector<ReasoningStep>& previousSteps);
    bool detectFlaws(const std::vector<ReasoningStep>& steps, std::string& flaw);
    std::string correctFlaw(const std::string& flaw, const std::vector<ReasoningStep>& context);

    // Synthesis
    ReasoningStep synthesizeAnswer(const std::vector<ReasoningStep>& steps, const ThinkingContext& context);
    std::string formatAnswer(const std::string& rawAnswer, const std::string& language);

    // File operations
    std::vector<std::string> listFilesRecursive(const std::string& directory, const std::string& extension = "");
    std::string readFileContent(const std::string& filePath, int maxLines = 100);
    std::vector<std::pair<std::string, int>> searchInFiles(const std::string& pattern, const std::string& directory);

    // Pattern matching
    std::vector<std::string> findCommonPatterns(const std::vector<std::string>& codeSnippets);
    std::string identifyBestMatch(const std::string& query, const std::vector<std::string>& candidates);

    // Confidence calculation
    float calculateStepConfidence(const ReasoningStep& step);
    float calculateOverallConfidence(const std::vector<ReasoningStep>& steps);

    // State management
    std::atomic<bool> m_thinking{false};
    std::thread m_thinkingThread;
    std::mutex m_thinkingMutex;

    // Configuration
    int m_maxThinkingTime = 30000;  // 30 seconds
    std::string m_defaultLanguage = "cpp";
    bool m_detailedLogging = false;

    // Caching and memory
    std::map<std::string, ThinkingResult> m_thinkingCache;
    std::map<std::string, int> m_patternFrequency;

    // Statistics
    mutable std::mutex m_statsMutex;
    ThinkingStats m_stats;
};
