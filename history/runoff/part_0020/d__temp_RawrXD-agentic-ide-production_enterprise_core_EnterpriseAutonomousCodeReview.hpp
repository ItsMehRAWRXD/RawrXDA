#ifndef ENTERPRISE_AUTONOMOUS_CODE_REVIEW_HPP
#define ENTERPRISE_AUTONOMOUS_CODE_REVIEW_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <chrono>

struct CodeInsight {
    std::string type; // syntax, security, performance, maintainability, quantum_security
    std::string severity; // critical, high, medium, low, info
    std::string description;
    std::string suggestion;
    double confidenceScore;
    int lineNumber;
    std::unordered_map<std::string, std::string> metadata;
};

class EnterpriseAutonomousCodeReview {
public:
    struct ReviewResult {
        std::string reviewId;
        std::string filePath;
        std::vector<CodeInsight> insights;
        std::unordered_map<std::string, std::string> metrics;
        std::unordered_map<std::string, std::string> suggestions;
        double overallScore;
        std::string overallRating;
        std::unordered_map<std::string, std::string> aiReasoning;
        bool quantumSafeVerified;
        std::chrono::system_clock::time_point reviewedAt;
    };
    
    static ReviewResult performAutonomousCodeReview(const std::string& filePath, const std::string& code);
    
private:
    static std::vector<CodeInsight> performRealMultiLayeredAnalysis(const std::string& code);
    static std::vector<CodeInsight> performRealSyntaxAnalysis(const std::string& code);
    static std::vector<CodeInsight> performRealSecurityAnalysis(const std::string& code);
    static std::vector<CodeInsight> performRealPerformanceAnalysis(const std::string& code);
    static std::vector<CodeInsight> performRealMaintainabilityAnalysis(const std::string& code);
    static std::vector<CodeInsight> performRealQuantumSafetyAnalysis(const std::string& code);
    static std::vector<CodeInsight> performRealAIPatternRecognition(const std::string& code);
    
    static std::unordered_map<std::string, std::string> calculateRealCodeQualityMetrics(const std::string& code);
    static double calculateMaintainabilityIndex(double commentRatio, double cyclomaticComplexity);
    static std::unordered_map<std::string, std::string> generateRealImprovementSuggestions(const std::string& code, const std::vector<CodeInsight>& insights);
    static std::unordered_map<std::string, std::string> generateRealAIReviewReasoning(const ReviewResult& result);
    static std::string generateRealReasoningText(const ReviewResult& result);
    static double calculateRealReviewConfidence(const ReviewResult& result);
    static bool verifyRealQuantumSafety(const QString& code);
    static void learnFromReviewPatterns(const QString& code, const QList<CodeInsight>& insights);
    static QString extractReviewPattern(const QString& code, const QList<CodeInsight>& insights);
    static double calculateAverageSeverity(const QList<CodeInsight>& insights);
    static double calculateOverallScore(const QList<CodeInsight>& insights, const QJsonObject& metrics);
    static QString ratingFromScore(double score);
    static QJsonArray insightsToJsonArray(const QList<CodeInsight>& insights);
    static QJsonObject insightToJson(const CodeInsight& insight);
};

#endif // ENTERPRISE_AUTONOMOUS_CODE_REVIEW_HPP
