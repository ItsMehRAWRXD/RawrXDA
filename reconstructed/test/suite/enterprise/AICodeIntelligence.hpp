#ifndef AI_CODE_INTELLIGENCE_HPP
#define AI_CODE_INTELLIGENCE_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QMap>

struct CodeInsight {
    QString type; // "security", "performance", "maintainability", "best_practice"
    QString severity; // "info", "warning", "error", "critical"
    QString description;
    QString suggestion;
    QString filePath;
    int lineNumber;
    int columnNumber;
    double confidenceScore;
    QJsonObject metadata;
};

struct CodePattern {
    QString pattern;
    QString language;
    QString category;
    QJsonObject metadata;
    double detectionAccuracy;
};

class AICodeIntelligence : public QObject {
    Q_OBJECT
    
public:
    explicit AICodeIntelligence(QObject *parent = nullptr);
    ~AICodeIntelligence();
    
    // Advanced code analysis
    QList<CodeInsight> analyzeCode(const QString& filePath, const QString& code);
    QList<CodeInsight> analyzeProject(const QString& projectPath);
    
    // AI-powered pattern detection
    QList<CodePattern> detectPatterns(const QString& code, const QString& language);
    bool isSecurityVulnerability(const QString& code, const QString& language);
    bool isPerformanceIssue(const QString& code, const QString& language);
    
    // Machine learning insights
    QJsonObject predictCodeQuality(const QString& filePath);
    QJsonObject suggestOptimizations(const QString& filePath);
    QJsonObject generateDocumentation(const QString& filePath);
    
    // Enterprise knowledge base
    void trainOnEnterpriseCodebase(const QString& codebasePath);
    QJsonObject getEnterpriseBestPractices(const QString& language);
    
    // Code metrics and statistics
    QJsonObject calculateCodeMetrics(const QString& filePath);
    QJsonObject compareCodeVersions(const QString& filePath1, const QString& filePath2);
    QJsonObject generateCodeHealthReport(const QString& projectPath);
    
    // Security analysis
    QList<CodeInsight> analyzeSecurity(const QString& filePath);
    QJsonObject generateSecurityReport(const QString& projectPath);
    bool hasKnownVulnerabilities(const QString& code, const QString& language);
    
    // Performance analysis
    QList<CodeInsight> analyzePerformance(const QString& filePath);
    QJsonObject generatePerformanceReport(const QString& projectPath);
    QJsonObject suggestPerformanceOptimizations(const QString& filePath);
    
    // Maintainability analysis
    QList<CodeInsight> analyzeMaintainability(const QString& filePath);
    QJsonObject calculateMaintainabilityIndex(const QString& filePath);
    QJsonObject generateMaintainabilityReport(const QString& projectPath);
    
    // Code transformation suggestions
    QJsonObject suggestRefactoring(const QString& filePath);
    QJsonObject suggestCodeModernization(const QString& filePath);
    QJsonObject suggestArchitectureImprovements(const QString& projectPath);
    
    // Learning and adaptation
    void learnFromUserFeedback(const QString& filePath, const QJsonObject& feedback);
    void updatePatternDatabase(const QJsonObject& newPatterns);
    void optimizeDetectionAlgorithms();
    
signals:
    void analysisComplete(const QString& filePath, const QList<CodeInsight>& insights);
    void patternDetected(const QString& pattern, double confidence);
    void securityVulnerabilityFound(const CodeInsight& insight);
    void optimizationSuggested(const QString& filePath, const QJsonObject& suggestion);
    void codeQualityReportGenerated(const QString& projectPath, const QJsonObject& report);
    void learningCompleted(const QString& dataset, int samples);
    
private:
    class Private;
    QScopedPointer<Private> d_ptr;
};

#endif // AI_CODE_INTELLIGENCE_HPP