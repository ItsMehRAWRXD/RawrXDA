#ifndef ENTERPRISE_AUTONOMOUS_CODE_REVIEW_HPP
#define ENTERPRISE_AUTONOMOUS_CODE_REVIEW_HPP

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

struct CodeInsight {
    QString type; // syntax, security, performance, maintainability, quantum_security
    QString severity; // critical, high, medium, low, info
    QString description;
    QString suggestion;
    double confidenceScore;
    int lineNumber;
    QJsonObject metadata;
};

class EnterpriseAutonomousCodeReview {
public:
    struct ReviewResult {
        QString reviewId;
        QString filePath;
        QList<CodeInsight> insights;
        QJsonObject metrics;
        QJsonObject suggestions;
        double overallScore;
        QString overallRating;
        QJsonObject aiReasoning;
        bool quantumSafeVerified;
        QDateTime reviewedAt;
    };
    
    static ReviewResult performAutonomousCodeReview(const QString& filePath, const QString& code);
    
private:
    static QList<CodeInsight> performRealMultiLayeredAnalysis(const QString& code);
    static QList<CodeInsight> performRealSyntaxAnalysis(const QString& code);
    static QList<CodeInsight> performRealSecurityAnalysis(const QString& code);
    static QList<CodeInsight> performRealPerformanceAnalysis(const QString& code);
    static QList<CodeInsight> performRealMaintainabilityAnalysis(const QString& code);
    static QList<CodeInsight> performRealQuantumSafetyAnalysis(const QString& code);
    static QList<CodeInsight> performRealAIPatternRecognition(const QString& code);
    
    static QJsonObject calculateRealCodeQualityMetrics(const QString& code);
    static double calculateMaintainabilityIndex(double commentRatio, double cyclomaticComplexity);
    static QJsonObject generateRealImprovementSuggestions(const QString& code, const QList<CodeInsight>& insights);
    static QJsonObject generateRealAIReviewReasoning(const ReviewResult& result);
    static QString generateRealReasoningText(const ReviewResult& result);
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
