#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QTimer>
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
class IntelligentErrorAnalysis : public QObject {
    Q_OBJECT
public:
    explicit IntelligentErrorAnalysis(QObject* parent = nullptr);
    virtual ~IntelligentErrorAnalysis();

    // Core error analysis
    QJsonObject analyzeError(const QString& errorText, const QString& context = QString());
    QJsonObject diagnoseCompilationError(const QString& compilerOutput);
    QJsonObject diagnoseRuntimeError(const QString& runtimeError, const QString& stackTrace);
    QJsonObject diagnoseLogicError(const QString& code, const QString& testFailure);
    
    // Fix generation
    QJsonArray generateFixOptions(const QJsonObject& errorAnalysis);
    QJsonObject generateDetailedFix(const QJsonObject& errorAnalysis, int fixIndex = 0);
    QString generateFixExplanation(const QJsonObject& fix);
    QJsonObject validateFix(const QJsonObject& fix, const QString& code);
    
    // Pattern learning
    void learnFromFix(const QString& errorType, const QString& appliedFix, bool success);
    QJsonObject getErrorPatterns() const;
    void updateErrorPatterns(const QJsonObject& patterns);
    
    // Error classification
    QString classifyError(const QString& errorText);
    double calculateErrorConfidence(const QJsonObject& errorAnalysis);
    QJsonObject analyzeErrorSeverity(const QJsonObject& errorAnalysis);

public slots:
    void processBuildError(const QString& compiler, const QString& output);
    void processRuntimeError(const QString& errorType, const QString& message, const QString& stackTrace);
    void processTestFailure(const QString& testName, const QString& failureMessage);
    void processCodeAnalysisWarning(const QString& file, const QString& line, const QString& warning);
    void onAutoRefresh();

signals:
    void errorAnalyzed(const QJsonObject& analysis);
    void fixGenerated(const QJsonObject& fixOptions);
    void fixApplied(const QString& errorId, const QJsonObject& fix, bool success);
    void errorPatternLearned(const QString& errorType, const QString& fixPattern);
    void analysisComplete(const QString& requestId, const QJsonObject& results);

private:
    // Error analysis helpers
    QJsonObject parseErrorMessage(const QString& errorText);
    QJsonObject identifyRootCause(const QJsonObject& parsedError);
    QJsonObject suggestFixes(const QJsonObject& rootCause);
    QString generateFixCode(const QJsonObject& fixSuggestion);
    
    // Error classification
    struct ErrorCategory {
        QString name;
        QString pattern;
        QString severity;
        QString commonCauses;
        QString typicalFixes;
    };
    
    QMap<QString, ErrorCategory> m_errorCategories;
    QMap<QString, QJsonObject> m_errorPatterns;
    QMap<QString, int> m_fixSuccessRates;
    
    // Analysis state
    QString m_currentProjectPath;
    QJsonObject m_projectContext;
    QTimer* m_analysisTimer;
    
    // Learning and adaptation
    struct FixAttempt {
        QString errorId;
        QString errorType;
        QString appliedFix;
        bool success;
        qint64 timestamp;
        QString feedback;
    };
    
    QList<FixAttempt> m_fixHistory;
    QJsonObject m_adaptiveRules;
    
    // Configuration
    bool m_enableLearning = true;
    bool m_autoSuggestFixes = true;
    int m_maxFixOptions = 5;
    double m_minConfidenceThreshold = 0.7;
};
