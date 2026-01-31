#pragma once
#include <memory>

/**
 * @class IntelligentErrorAnalysis
 * @brief Provides AI-powered error diagnosis and fix generation
 * 
 * Features:
 * - Real-time error detection and classification
 * - Root cause analysis with confidence scoring
 * - Intelligent fix suggestion with multiple options
 * - Error pattern learning and adaptation
 * - Integration with build systems and runtime diagnostics
 * - Automatic error recovery suggestions
 */
class IntelligentErrorAnalysis  {

public:
    explicit IntelligentErrorAnalysis( = nullptr);
    virtual ~IntelligentErrorAnalysis();

    // Core error analysis
    void* analyzeError(const std::string& errorText, const std::string& context = std::string());
    void* diagnoseCompilationError(const std::string& compilerOutput);
    void* diagnoseRuntimeError(const std::string& runtimeError, const std::string& stackTrace);
    void* diagnoseLogicError(const std::string& code, const std::string& testFailure);
    
    // Fix generation
    void* generateFixOptions(const void*& errorAnalysis);
    void* generateDetailedFix(const void*& errorAnalysis, int fixIndex = 0);
    std::string generateFixExplanation(const void*& fix);
    void* validateFix(const void*& fix, const std::string& code);
    
    // Pattern learning
    void learnFromFix(const std::string& errorType, const std::string& appliedFix, bool success);
    void* getErrorPatterns() const;
    void updateErrorPatterns(const void*& patterns);
    
    // Error classification
    std::string classifyError(const std::string& errorText);
    double calculateErrorConfidence(const void*& errorAnalysis);
    void* analyzeErrorSeverity(const void*& errorAnalysis);
\npublic:\n    void processBuildError(const std::string& compiler, const std::string& output);
    void processRuntimeError(const std::string& errorType, const std::string& message, const std::string& stackTrace);
    void processTestFailure(const std::string& testName, const std::string& failureMessage);
    void processCodeAnalysisWarning(const std::string& file, const std::string& line, const std::string& warning);
    void onAutoRefresh();
\npublic:\n    void errorAnalyzed(const void*& analysis);
    void fixGenerated(const void*& fixOptions);
    void fixApplied(const std::string& errorId, const void*& fix, bool success);
    void errorPatternLearned(const std::string& errorType, const std::string& fixPattern);
    void analysisComplete(const std::string& requestId, const void*& results);

private:
    // Error analysis helpers
    void* parseErrorMessage(const std::string& errorText);
    void* identifyRootCause(const void*& parsedError);
    void* suggestFixes(const void*& rootCause);
    std::string generateFixCode(const void*& fixSuggestion);
    
    // Error classification
    struct ErrorCategory {
        std::string name;
        std::string pattern;
        std::string severity;
        std::string commonCauses;
        std::string typicalFixes;
    };
    
    std::map<std::string, ErrorCategory> m_errorCategories;
    std::map<std::string, void*> m_errorPatterns;
    std::map<std::string, int> m_fixSuccessRates;
    
    // Analysis state
    std::string m_currentProjectPath;
    void* m_projectContext;
    // Timer m_analysisTimer;
    
    // Learning and adaptation
    struct FixAttempt {
        std::string errorId;
        std::string errorType;
        std::string appliedFix;
        bool success;
        int64_t timestamp;
        std::string feedback;
    };
    
    std::vector<FixAttempt> m_fixHistory;
    void* m_adaptiveRules;
    
    // Configuration
    bool m_enableLearning = true;
    bool m_autoSuggestFixes = true;
    int m_maxFixOptions = 5;
    double m_minConfidenceThreshold = 0.7;
};





