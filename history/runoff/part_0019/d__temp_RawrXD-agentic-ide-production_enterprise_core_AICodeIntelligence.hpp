
#ifndef AI_CODE_INTELLIGENCE_HPP
#define AI_CODE_INTELLIGENCE_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

struct CodeInsight {
    std::string type; // "security", "performance", "maintainability", "best_practice"
    std::string severity; // "info", "warning", "error", "critical"
    std::string description;
    std::string suggestion;
    std::string filePath;
    int lineNumber = 0;
    int columnNumber = 0;
    double confidenceScore = 0.0;
    std::map<std::string, std::string> metadata;
};

struct CodePattern {
    std::string pattern;
    std::string language;
    std::string category;
    std::map<std::string, std::string> metadata;
    double detectionAccuracy = 0.0;
};

class AICodeIntelligence {
public:
    AICodeIntelligence();
    ~AICodeIntelligence();

    // Advanced code analysis
    std::vector<CodeInsight> analyzeCode(const std::string& filePath, const std::string& code);
    std::vector<CodeInsight> analyzeProject(const std::string& projectPath);

    // AI-powered pattern detection
    std::vector<CodePattern> detectPatterns(const std::string& code, const std::string& language);
    bool isSecurityVulnerability(const std::string& code, const std::string& language);
    bool isPerformanceIssue(const std::string& code, const std::string& language);

    // Machine learning insights
    std::map<std::string, std::string> predictCodeQuality(const std::string& filePath);
    std::map<std::string, std::string> suggestOptimizations(const std::string& filePath);
    std::map<std::string, std::string> generateDocumentation(const std::string& filePath);

    // Enterprise knowledge base
    void trainOnEnterpriseCodebase(const std::string& codebasePath);
    std::map<std::string, std::string> getEnterpriseBestPractices(const std::string& language);

    // Code metrics and statistics
    std::map<std::string, std::string> calculateCodeMetrics(const std::string& filePath);
    std::map<std::string, std::string> compareCodeVersions(const std::string& filePath1, const std::string& filePath2);
    std::map<std::string, std::string> generateCodeHealthReport(const std::string& projectPath);

    // Security analysis
    std::vector<CodeInsight> analyzeSecurity(const std::string& filePath);
    std::map<std::string, std::string> generateSecurityReport(const std::string& projectPath);
    bool hasKnownVulnerabilities(const std::string& code, const std::string& language);

    // Performance analysis
    std::vector<CodeInsight> analyzePerformance(const std::string& filePath);
    std::map<std::string, std::string> generatePerformanceReport(const std::string& projectPath);
    std::map<std::string, std::string> suggestPerformanceOptimizations(const std::string& filePath);

    // Maintainability analysis
    std::vector<CodeInsight> analyzeMaintainability(const std::string& filePath);
    std::map<std::string, std::string> calculateMaintainabilityIndex(const std::string& filePath);
    std::map<std::string, std::string> generateMaintainabilityReport(const std::string& projectPath);

    // Code transformation suggestions
    std::map<std::string, std::string> suggestRefactoring(const std::string& filePath);
    std::map<std::string, std::string> suggestCodeModernization(const std::string& filePath);
    std::map<std::string, std::string> suggestArchitectureImprovements(const std::string& projectPath);

    // Learning and adaptation
    void learnFromUserFeedback(const std::string& filePath, const std::map<std::string, std::string>& feedback);
    void updatePatternDatabase(const std::map<std::string, std::string>& newPatterns);
    void optimizeDetectionAlgorithms();

private:
    struct Private;
    std::unique_ptr<Private> d_ptr;
};

#endif // AI_CODE_INTELLIGENCE_HPP