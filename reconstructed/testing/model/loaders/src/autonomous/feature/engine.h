// autonomous_feature_engine.h - Real-time Autonomous Code Suggestions
// Converted from Qt to pure C++17
#ifndef AUTONOMOUS_FEATURE_ENGINE_H
#define AUTONOMOUS_FEATURE_ENGINE_H

#include "common/json_types.hpp"
#include "common/callback_system.hpp"
#include "common/time_utils.hpp"
#include "common/string_utils.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

// Forward declarations
class HybridCloudManager;
class IntelligentCodebaseEngine;

// Autonomous suggestion types
struct AutonomousSuggestion {
    std::string suggestionId;
    std::string type;              // test_generation, optimization, refactoring, security_fix, documentation
    std::string filePath;
    int lineNumber = 0;
    std::string originalCode;
    std::string suggestedCode;
    std::string explanation;
    double confidence = 0.0;       // 0-1
    std::vector<std::string> benefits;
    JsonObject metadata;
    TimePoint timestamp;
    bool wasAccepted = false;
};

// Security vulnerability detection
struct SecurityIssue {
    std::string issueId;
    std::string severity;          // critical, high, medium, low
    std::string type;              // sql_injection, xss, buffer_overflow, etc.
    std::string filePath;
    int lineNumber = 0;
    std::string vulnerableCode;
    std::string description;
    std::string suggestedFix;
    std::string cveReference;
    std::vector<std::string> affectedComponents;
    double riskScore = 0.0;        // 0-10
};

// Performance optimization suggestion
struct PerformanceOptimization {
    std::string optimizationId;
    std::string type;              // algorithm, caching, parallelization, memory
    std::string filePath;
    int lineNumber = 0;
    std::string currentImplementation;
    std::string optimizedImplementation;
    std::string reasoning;
    double expectedSpeedup = 0.0;    // Multiplier (e.g., 2.0 = 2x faster)
    int64_t expectedMemorySaving = 0; // Bytes
    double confidence = 0.0;
};

// Documentation gap
struct DocumentationGap {
    std::string gapId;
    std::string filePath;
    int lineNumber = 0;
    std::string symbolName;
    std::string symbolType;        // function, class, variable
    std::string suggestedDocumentation;
    bool isCritical = false;       // Public API?
};

// Test generation request
struct TestGenerationRequest {
    std::string functionName;
    std::string functionCode;
    std::string language;
    std::vector<std::string> inputTypes;
    std::string returnType;
};

// Generated test case
struct GeneratedTest {
    std::string testId;
    std::string testName;
    std::string testCode;
    std::string framework;         // jest, pytest, gtest, etc.
    std::string language;
    std::vector<std::string> testCases;
    double coverage = 0.0;         // Expected coverage
    std::string reasoning;
};

// Code quality metrics
struct CodeQualityMetrics {
    double maintainability = 0.0;    // 0-100
    double reliability = 0.0;        // 0-100
    double security = 0.0;           // 0-100
    double efficiency = 0.0;         // 0-100
    double overallScore = 0.0;       // 0-100
    JsonObject details;
};

// Learning profile for user
struct UserCodingProfile {
    std::string userId;
    std::unordered_map<std::string, int> languagePreferences;
    std::unordered_map<std::string, int> patternUsage;
    std::vector<std::string> commonMistakes;
    double averageAcceptanceRate = 0.0;
    JsonObject preferences;
};

class AutonomousFeatureEngine {
public:
    AutonomousFeatureEngine();
    ~AutonomousFeatureEngine();

    // Initialize dependencies
    void setHybridCloudManager(HybridCloudManager* manager);
    void setCodebaseEngine(IntelligentCodebaseEngine* engine);

    // Real-time code analysis
    void analyzeCode(const std::string& code, const std::string& filePath, const std::string& language);
    void analyzeCodeChange(const std::string& oldCode, const std::string& newCode,
                           const std::string& filePath, const std::string& language);

    // Autonomous suggestions
    std::vector<AutonomousSuggestion> getSuggestionsForCode(const std::string& code,
                                                             const std::string& language);
    std::vector<AutonomousSuggestion> getActiveSuggestions() const;
    void acceptSuggestion(const std::string& suggestionId);
    void rejectSuggestion(const std::string& suggestionId);
    void dismissSuggestion(const std::string& suggestionId);

    // Test generation
    GeneratedTest generateTestsForFunction(const std::string& functionCode,
                                            const std::string& language);
    std::vector<GeneratedTest> generateTestSuite(const std::string& filePath);

    // Security analysis
    std::vector<SecurityIssue> detectSecurityVulnerabilities(const std::string& code,
                                                              const std::string& language);
    SecurityIssue analyzeSecurityIssue(const std::string& code, int lineNumber);
    std::string suggestSecurityFix(const SecurityIssue& issue);

    // Performance optimization
    std::vector<PerformanceOptimization> suggestOptimizations(const std::string& code,
                                                              const std::string& language);
    std::string optimizeCode(const std::string& code, const std::string& optimizationType);
    double estimatePerformanceGain(const std::string& originalCode,
                                    const std::string& optimizedCode);

    // Documentation generation
    std::vector<DocumentationGap> findDocumentationGaps(const std::string& filePath);
    std::string generateDocumentation(const std::string& symbolCode, const std::string& symbolType);
    std::string generateAPIDocumentation(const std::string& classCode);

    // Code quality assessment
    CodeQualityMetrics assessCodeQuality(const std::string& code, const std::string& language);
    double calculateMaintainability(const std::string& code);
    double calculateReliability(const std::string& code);
    double calculateSecurity(const std::string& code);
    double calculateEfficiency(const std::string& code);

    // Learning and adaptation
    void recordUserInteraction(const std::string& suggestionId, bool accepted);
    UserCodingProfile getUserProfile() const;
    void updateLearningModel(const std::string& code, const std::string& feedback);
    std::vector<std::string> predictNextAction(const std::string& context);

    // Pattern learning
    void analyzeCodePattern(const std::string& code, const std::string& language);
    std::vector<std::string> suggestPatternImprovements(const std::string& code);
    bool detectAntiPattern(const std::string& code, std::string& antiPatternName);

    // Real-time monitoring
    void enableRealTimeAnalysis(bool enable);
    void setAnalysisInterval(int milliseconds);
    void startBackgroundAnalysis(const std::string& projectPath);
    void stopBackgroundAnalysis();

    // Configuration
    void setConfidenceThreshold(double threshold);
    void enableAutomaticSuggestions(bool enable);
    void setMaxConcurrentAnalyses(int max);
    double getConfidenceThreshold() const;

    // Callbacks (replacing Qt signals)
    CallbackList<const AutonomousSuggestion&> onSuggestionGenerated;
    CallbackList<const SecurityIssue&> onSecurityIssueDetected;
    CallbackList<const PerformanceOptimization&> onOptimizationFound;
    CallbackList<const DocumentationGap&> onDocumentationGapFound;
    CallbackList<const GeneratedTest&> onTestGenerated;
    CallbackList<const CodeQualityMetrics&> onCodeQualityAssessed;
    CallbackList<const std::string&> onAnalysisComplete;
    CallbackList<const std::string&> onErrorOccurred;

private:
    // Analysis functions
    AutonomousSuggestion generateTestSuggestion(const std::string& functionCode,
                                                 const std::string& language);
    AutonomousSuggestion generateRefactoringSuggestion(const std::string& code,
                                                       const std::string& filePath);
    AutonomousSuggestion generateOptimizationSuggestion(const std::string& code);
    AutonomousSuggestion generateSecurityFixSuggestion(const SecurityIssue& issue);

    // Security detection algorithms
    bool detectSQLInjection(const std::string& code);
    bool detectXSS(const std::string& code);
    bool detectBufferOverflow(const std::string& code);
    bool detectCommandInjection(const std::string& code);
    bool detectPathTraversal(const std::string& code);
    bool detectInsecureCrypto(const std::string& code);

    // Performance analysis
    bool canParallelize(const std::string& code);
    bool canCache(const std::string& code);
    bool hasInefficientAlgorithm(const std::string& code, std::string& algorithmName);
    bool hasMemoryWaste(const std::string& code);

    // Test generation helpers
    std::vector<std::string> extractFunctionParameters(const std::string& functionCode);
    std::string inferReturnType(const std::string& functionCode);
    std::vector<std::string> generateTestCases(const TestGenerationRequest& request);
    std::string generateAssertions(const std::string& functionName,
                                    const std::vector<std::string>& inputs,
                                    const std::string& expectedOutput);

    // Documentation helpers
    std::string extractFunctionPurpose(const std::string& functionCode);
    std::vector<std::string> extractParameters(const std::string& functionCode);
    std::string extractReturnValue(const std::string& functionCode);
    std::vector<std::string> extractExceptions(const std::string& functionCode);

    // Code quality calculations
    int calculateComplexity(const std::string& code);
    int calculateDuplication(const std::string& code);
    int countCodeSmells(const std::string& code);
    double calculateTestability(const std::string& code);

    // Machine learning functions
    double calculateSuggestionConfidence(const AutonomousSuggestion& suggestion);
    void updateAcceptanceRates();
    std::vector<double> extractCodeFeatures(const std::string& code);
    double predictAcceptanceProbability(const AutonomousSuggestion& suggestion);

    // Pattern matching
    bool matchesPattern(const std::string& code, const std::string& patternName);
    std::vector<std::string> detectPatterns(const std::string& code);

    // Data members
    HybridCloudManager* hybridCloudManager;
    IntelligentCodebaseEngine* codebaseEngine;

    std::vector<AutonomousSuggestion> activeSuggestions;
    std::vector<SecurityIssue> detectedSecurityIssues;
    std::vector<PerformanceOptimization> optimizationSuggestions;
    std::vector<DocumentationGap> documentationGaps;

    UserCodingProfile userProfile;
    std::unordered_map<std::string, int> acceptedSuggestionsByType;
    std::unordered_map<std::string, int> rejectedSuggestionsByType;

    std::unique_ptr<std::thread> analysisThread;
    std::atomic<bool> analysisRunning{false};
    std::string currentProjectPath;

    bool realTimeAnalysisEnabled = false;
    int analysisIntervalMs = DEFAULT_ANALYSIS_INTERVAL_MS;
    double confidenceThreshold = DEFAULT_CONFIDENCE_THRESHOLD;
    bool automaticSuggestionsEnabled = true;
    int maxConcurrentAnalyses = 4;

    // Configuration
    static constexpr double DEFAULT_CONFIDENCE_THRESHOLD = 0.70;
    static constexpr int DEFAULT_ANALYSIS_INTERVAL_MS = 30000; // 30 seconds
    static constexpr int MAX_ACTIVE_SUGGESTIONS = 50;
};

#endif // AUTONOMOUS_FEATURE_ENGINE_H
