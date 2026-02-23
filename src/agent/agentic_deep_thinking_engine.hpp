#pragma once
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
#include "quantum_dynamic_time_manager.hpp"
#include "quantum_autonomous_todo_system.hpp"

// Forward declarations for quantum systems
namespace RawrXD::Agent {
    class QuantumAutonomousTodoSystem;
    struct ExecutionResult;
}

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
        
        // Enhanced multi-agent features
        int cycleMultiplier = 1;           // 1x-8x multiplier for iteration depth (5x8=40, 10x8=80)
        bool enableMultiAgent = false;     // Enable parallel multi-agent execution
        int agentCount = 1;                // Number of parallel agents (1-8)
        std::vector<std::string> agentModels;  // Specific models for multi-agent (empty = use default)
        bool enableAgentDebate = false;    // Enable agents to critique each other's work
        bool enableAgentVoting = false;    // Use voting to select best answer from multi-agent results
        float consensusThreshold = 0.7f;   // Required agreement ratio for consensus (0.5-1.0)
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
        
        // Enhanced quantum features
        float productionReadinessScore = 0.0f;
        std::vector<std::string> auditFindings;
        std::vector<RawrXD::Agent::QuantumAutonomousTodoSystem::TaskDefinition> generatedTodos;
        float quantumOptimizationBonus = 0.0f;
        bool usedMasmAcceleration = false;
        
        // Multi-agent consensus data
        std::vector<float> agentConfidences;
        float consensusScore = 0.0f;
        bool consensusReached = false;
        
        ThinkingResult() : overallConfidence(0.0f), iterationCount(0), elapsedMilliseconds(0), 
                          requiresUserInput(false) {}
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
    
    // Quantum enhancements
    void enableQuantumMode(bool enable = true) { m_quantum_enabled = enable; }
    void enableProductionAudit(bool enable = true) { m_production_audit_enabled = enable; }
    void enableMasmAcceleration(bool enable = true) { m_masm_acceleration_enabled = enable; }
    void setAutonomousMode(bool enable = true) { m_autonomous_mode = enable; }
    void setQualityThresholds(float quality, float performance, float safety) {
        m_quality_threshold = quality;
        m_performance_threshold = performance; 
        m_safety_threshold = safety;
    }

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

    // Multi-agent execution
    struct AgentResult {
        int agentId;
        std::string modelName;
        ThinkingResult result;
        float agreementScore;  // How much this agent agrees with others
    };
    
    struct MultiAgentResult {
        std::vector<AgentResult> agentResults;
        ThinkingResult consensusResult;  // Merged result from all agents
        std::vector<std::string> disagreementPoints;  // Where agents disagreed
        float consensusConfidence;  // Overall consensus confidence
        bool consensusReached;
    };
    
    MultiAgentResult thinkMultiAgent(const ThinkingContext& context);
    
    // Statistics
    struct ThinkingStats {
        int totalThinkingRequests = 0;
        int successfulThinking = 0;
        int failedThinking = 0;
        float avgThinkingTime = 0.0f;
        float avgConfidence = 0.0f;
        int cacheHits = 0;
        std::map<ThinkingStep, int> stepFrequency;
        
        // Multi-agent stats
        int multiAgentRequests = 0;
        int consensusReached = 0;
        float avgConsensusConfidence = 0.0f;
        int totalAgentsSpawned = 0;
        
        // Quantum enhancement stats
        int quantumThinkingRequests = 0;
        int masmAcceleratedRequests = 0;
        int productionAuditsPerformed = 0;
        float avgProductionReadinessScore = 0.0f;
        int todoGenerationEvents = 0;
        int autonomousExecutions = 0;
    };
    ThinkingStats getStats() const;
    void resetStats();

private:
    // Core reasoning engine
    std::vector<ReasoningStep> performChainOfThought(const ThinkingContext& context);
    ReasoningStep executeStep(ThinkingStep step, const ThinkingContext& context, const std::string& previousContext);
    
    // Enhanced quantum thinking methods
    ThinkingResult performQuantumThinking(const ThinkingContext& context);
    ThinkingResult performTraditionalThinking(const ThinkingContext& context);
    std::vector<ReasoningStep> performQuantumChainOfThought(const ThinkingContext& context, 
                                                           const RawrXD::Agent::QuantumDynamicTimeManager::TimeAllocation& allocation);
    std::vector<ReasoningStep> performMultiModelSelfCorrection(const std::vector<ReasoningStep>& steps,
                                                              const ThinkingContext& context);
    
    // Production audit system
    struct ProductionAuditResult {
        float overall_score = 0.0f;
        float code_quality_score = 0.0f;
        float performance_score = 0.0f;
        float security_score = 0.0f;
        float maintainability_score = 0.0f;
        std::vector<std::string> findings;
    };
    
    ProductionAuditResult performProductionAudit(const ThinkingResult& result, const ThinkingContext& context);
    float auditCodeQuality(const std::string& code);
    float auditPerformance(const std::string& code);
    float auditSecurity(const std::string& code);
    float auditMaintainability(const std::string& code);
    
    // Multi-agent enhancement
    ThinkingResult enhanceWithMultiAgentConsensus(const ThinkingResult& result, const ThinkingContext& context);
    float calculateEnhancedConfidence(const ThinkingResult& result);

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

    // Multi-agent coordination
    AgentResult runSingleAgent(int agentId, const std::string& model, const ThinkingContext& context);
    ThinkingResult mergeAgentResults(const std::vector<AgentResult>& results, const ThinkingContext& context);
    std::vector<std::string> findDisagreements(const std::vector<AgentResult>& results);
    float calculateAgreement(const AgentResult& a1, const AgentResult& a2);
    AgentResult selectBestByVoting(const std::vector<AgentResult>& results);
    
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
    
    // Quantum enhancement flags
    bool m_quantum_enabled = true;
    bool m_production_audit_enabled = true;
    bool m_masm_acceleration_enabled = true;
    bool m_autonomous_mode = false;
    
    // Quality thresholds
    float m_quality_threshold = 0.85f;
    float m_performance_threshold = 0.80f;
    float m_safety_threshold = 0.95f;

    // Caching and memory
    std::map<std::string, ThinkingResult> m_thinkingCache;
    std::map<std::string, int> m_patternFrequency;

    // Statistics
    mutable std::mutex m_statsMutex;
    ThinkingStats m_stats;
};
