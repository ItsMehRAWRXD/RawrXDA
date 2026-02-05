// autonomous_feature_engine.h - Real-time Autonomous Code Suggestions
#ifndef AUTONOMOUS_FEATURE_ENGINE_H
#define AUTONOMOUS_FEATURE_ENGINE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QJsonObject>
#include <QDateTime>
#include <QTimer>

// Forward declarations
class HybridCloudManager;
class IntelligentCodebaseEngine;
namespace CPUInference { class CPUInferenceEngine; }

// Autonomous suggestion types
struct AutonomousSuggestion {
    QString suggestionId;
    QString type;              // test_generation, optimization, refactoring, security_fix, documentation
    QString filePath;
    int lineNumber;
    QString originalCode;
    QString suggestedCode;
    QString explanation;
    double confidence;         // 0-1
    QStringList benefits;
    QJsonObject metadata;
    QDateTime timestamp;
    bool wasAccepted;
};

// Security vulnerability detection
struct SecurityIssue {
    QString issueId;
    QString severity;          // critical, high, medium, low
    QString type;              // sql_injection, xss, buffer_overflow, etc.
    QString filePath;
    int lineNumber;
    QString vulnerableCode;
    QString description;
    QString suggestedFix;
    QString cveReference;
    QStringList affectedComponents;
    double riskScore;          // 0-10
};

// Performance optimization suggestion
struct PerformanceOptimization {
    QString optimizationId;
    QString type;              // algorithm, caching, parallelization, memory
    QString filePath;
    int lineNumber;
    QString currentImplementation;
    QString optimizedImplementation;
    QString reasoning;
    double expectedSpeedup;    // Multiplier (e.g., 2.0 = 2x faster)
    qint64 expectedMemorySaving; // Bytes
    double confidence;
};

// Documentation gap
struct DocumentationGap {
    QString gapId;
    QString filePath;
    int lineNumber;
    QString symbolName;
    QString symbolType;        // function, class, variable
    QString suggestedDocumentation;
    bool isCritical;           // Public API?
};

// Test generation request
struct TestGenerationRequest {
    QString functionName;
    QString functionCode;
    QString language;
    QStringList inputTypes;
    QString returnType;
};

// Generated test case
struct GeneratedTest {
    QString testId;
    QString testName;
    QString testCode;
    QString framework;         // jest, pytest, gtest, etc.
    QString language;          // cpp, python, javascript, etc.
    QStringList testCases;
    double coverage;           // Expected coverage
    QString reasoning;
};

// Code quality metrics
struct CodeQualityMetrics {
    double maintainability;    // 0-100
    double reliability;        // 0-100
    double security;           // 0-100
    double efficiency;         // 0-100
    double overallScore;       // 0-100
    QJsonObject details;
};

// Learning profile for user
struct UserCodingProfile {
    QString userId;
    QHash<QString, int> languagePreferences;
    QHash<QString, int> patternUsage;
    QVector<QString> commonMistakes;
    double averageAcceptanceRate;
    QJsonObject preferences;
};

class AutonomousFeatureEngine : public QObject {
    Q_OBJECT

public:
    explicit AutonomousFeatureEngine(QObject* parent = nullptr);
    ~AutonomousFeatureEngine();

    // Initialize dependencies
    void setHybridCloudManager(HybridCloudManager* manager);
    void setCodebaseEngine(IntelligentCodebaseEngine* engine);
    void setInferenceEngine(CPUInference::CPUInferenceEngine* engine);

    // Real-time code analysis
    void analyzeCode(const QString& code, const QString& filePath, const QString& language);
    void analyzeCodeChange(const QString& oldCode, const QString& newCode, 
                          const QString& filePath, const QString& language);
    
    // Autonomous suggestions
    QVector<AutonomousSuggestion> getSuggestionsForCode(const QString& code, 
                                                        const QString& language);
    QVector<AutonomousSuggestion> getActiveSuggestions() const;
    void acceptSuggestion(const QString& suggestionId);
    void rejectSuggestion(const QString& suggestionId);
    void dismissSuggestion(const QString& suggestionId);
    
    // Test generation
    GeneratedTest generateTestsForFunction(const QString& functionCode, 
                                          const QString& language);
    QVector<GeneratedTest> generateTestSuite(const QString& filePath);
    
    // Security analysis
    QVector<SecurityIssue> detectSecurityVulnerabilities(const QString& code,
                                                         const QString& language);
    SecurityIssue analyzeSecurityIssue(const QString& code, int lineNumber);
    QString suggestSecurityFix(const SecurityIssue& issue);
    
    // Performance optimization
    QVector<PerformanceOptimization> suggestOptimizations(const QString& code,
                                                         const QString& language);
    QString optimizeCode(const QString& code, const QString& optimizationType);
    double estimatePerformanceGain(const QString& originalCode, 
                                   const QString& optimizedCode);
    
    // Documentation generation
    QVector<DocumentationGap> findDocumentationGaps(const QString& filePath);
    QString generateDocumentation(const QString& symbolCode, const QString& symbolType);
    QString generateAPIDocumentation(const QString& classCode);
    
    // Code quality assessment
    CodeQualityMetrics assessCodeQuality(const QString& code, const QString& language);
    double calculateMaintainability(const QString& code);
    double calculateReliability(const QString& code);
    double calculateSecurity(const QString& code);
    double calculateEfficiency(const QString& code);
    
    // Learning and adaptation
    void recordUserInteraction(const QString& suggestionId, bool accepted);
    UserCodingProfile getUserProfile() const;
    void updateLearningModel(const QString& code, const QString& feedback);
    QVector<QString> predictNextAction(const QString& context);
    
    // Pattern learning
    void analyzeCodePattern(const QString& code, const QString& language);
    QVector<QString> suggestPatternImprovements(const QString& code);
    bool detectAntiPattern(const QString& code, QString& antiPatternName);
    
    // Real-time monitoring
    void enableRealTimeAnalysis(bool enable);
    void setAnalysisInterval(int milliseconds);
    void startBackgroundAnalysis(const QString& projectPath);
    void stopBackgroundAnalysis();
    
    // Configuration
    void setConfidenceThreshold(double threshold);
    void enableAutomaticSuggestions(bool enable);
    void setMaxConcurrentAnalyses(int max);
    double getConfidenceThreshold() const;

signals:
    void suggestionGenerated(const AutonomousSuggestion& suggestion);
    void securityIssueDetected(const SecurityIssue& issue);
    void optimizationFound(const PerformanceOptimization& optimization);
    void documentationGapFound(const DocumentationGap& gap);
    void testGenerated(const GeneratedTest& test);
    void codeQualityAssessed(const CodeQualityMetrics& metrics);
    void analysisComplete(const QString& filePath);
    void errorOccurred(const QString& error);

private slots:
    void onAnalysisTimerTimeout();

private:
    // Analysis functions
    AutonomousSuggestion generateTestSuggestion(const QString& functionCode,
                                               const QString& language);
    AutonomousSuggestion generateRefactoringSuggestion(const QString& code,
                                                      const QString& filePath);
    AutonomousSuggestion generateOptimizationSuggestion(const QString& code);
    AutonomousSuggestion generateSecurityFixSuggestion(const SecurityIssue& issue);
    
    // Security detection algorithms
    bool detectSQLInjection(const QString& code);
    bool detectXSS(const QString& code);
    bool detectBufferOverflow(const QString& code);
    bool detectCommandInjection(const QString& code);
    bool detectPathTraversal(const QString& code);
    bool detectInsecureCrypto(const QString& code);
    
    // Performance analysis
    bool canParallelize(const QString& code);
    bool canCache(const QString& code);
    bool hasInefficientAlgorithm(const QString& code, QString& algorithmName);
    bool hasMemoryWaste(const QString& code);
    
    // Test generation helpers
    QStringList extractFunctionParameters(const QString& functionCode);
    QString inferReturnType(const QString& functionCode);
    QStringList generateTestCases(const TestGenerationRequest& request);
    QString generateAssertions(const QString& functionName, 
                               const QStringList& inputs,
                               const QString& expectedOutput);
    
    // Documentation helpers
    QString extractFunctionPurpose(const QString& functionCode);
    QStringList extractParameters(const QString& functionCode);
    QString extractReturnValue(const QString& functionCode);
    QStringList extractExceptions(const QString& functionCode);
    
    // Code quality calculations
    int calculateComplexity(const QString& code);
    int calculateDuplication(const QString& code);
    int countCodeSmells(const QString& code);
    double calculateTestability(const QString& code);
    
    // Machine learning functions
    double calculateSuggestionConfidence(const AutonomousSuggestion& suggestion);
    void updateAcceptanceRates();
    QVector<double> extractCodeFeatures(const QString& code);
    double predictAcceptanceProbability(const AutonomousSuggestion& suggestion);
    
    // Pattern matching
    bool matchesPattern(const QString& code, const QString& patternName);
    QVector<QString> detectPatterns(const QString& code);
    
    // Data members
    HybridCloudManager* hybridCloudManager;
    IntelligentCodebaseEngine* codebaseEngine;
    CPUInference::CPUInferenceEngine* inferenceEngine;
    
    QVector<AutonomousSuggestion> activeSuggestions;
    QVector<SecurityIssue> detectedSecurityIssues;
    QVector<PerformanceOptimization> optimizationSuggestions;
    QVector<DocumentationGap> documentationGaps;
    
    UserCodingProfile userProfile;
    QHash<QString, int> acceptedSuggestionsByType;
    QHash<QString, int> rejectedSuggestionsByType;
    
    QTimer* analysisTimer;
    QString currentProjectPath;
    
    bool realTimeAnalysisEnabled;
    int analysisIntervalMs;
    double confidenceThreshold;
    bool automaticSuggestionsEnabled;
    int maxConcurrentAnalyses;
    
    // Configuration
    static constexpr double DEFAULT_CONFIDENCE_THRESHOLD = 0.70;
    static constexpr int DEFAULT_ANALYSIS_INTERVAL_MS = 30000; // 30 seconds
    static constexpr int MAX_ACTIVE_SUGGESTIONS = 50;
};

#endif // AUTONOMOUS_FEATURE_ENGINE_H
