#ifndef ENTERPRISE_ADVANCED_CODE_REVIEW_HPP
#define ENTERPRISE_ADVANCED_CODE_REVIEW_HPP

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QMap>
#include <QSet>

struct AdvancedCodeInsight {
    QString type;
    QString severity;
    QString description;
    QString suggestion;
    double confidenceScore;
    int lineNumber;
    QJsonObject metadata;
    QString codeContext;
    QList<int> affectedLines;
    QString remediationPath;
    QJsonArray relatedInsights;
};

struct DataFlowNode {
    QString name;
    QString type;
    int lineNumber;
    QList<QString> sources;
    QList<QString> sinks;
    bool isTainted;
};

struct ControlFlowNode {
    int lineNumber;
    QString nodeType;
    QList<int> successors;
    QList<int> predecessors;
    double complexity;
};

class EnterpriseAdvancedCodeReview {
public:
    struct AdvancedReviewResult {
        QString reviewId;
        QString filePath;
        QList<AdvancedCodeInsight> insights;
        QJsonObject metrics;
        QJsonObject suggestions;
        double overallScore;
        QString overallRating;
        QJsonObject aiReasoning;
        bool quantumSafeVerified;
        QDateTime reviewedAt;
        
        // Advanced analysis
        QJsonObject advancedAnalysis;
        QJsonArray dependencyGraph;
        QJsonArray dataFlowAnalysis;
        QJsonArray controlFlowAnalysis;
        QJsonObject architecturePatterns;
        QJsonObject taintAnalysis;
        QJsonObject typeFlowAnalysis;
        QList<QPair<int, int>> codeClones;
        double cyclomaticComplexity;
        QMap<QString, int> symbolTable;
    };
    
    static AdvancedReviewResult performAdvancedCodeReview(const QString& filePath, const QString& code);
    
private:
    static QList<AdvancedCodeInsight> performDataFlowAnalysis(const QString& code);
    static QList<AdvancedCodeInsight> performControlFlowAnalysis(const QString& code);
    static QList<AdvancedCodeInsight> performDependencyAnalysis(const QString& code);
    static QList<AdvancedCodeInsight> performArchitectureAnalysis(const QString& code);
    static QList<AdvancedCodeInsight> performComplexityAnalysis(const QString& code);
    static QList<AdvancedCodeInsight> performTaintAnalysis(const QString& code);
    static QList<AdvancedCodeInsight> performTypeFlowAnalysis(const QString& code);
    static QList<AdvancedCodeInsight> performCodeCloneDetection(const QString& code);
    static QList<AdvancedCodeInsight> performCallGraphAnalysis(const QString& code);
    static QList<AdvancedCodeInsight> performMemoryAnalysis(const QString& code);
    
    static QJsonArray buildDependencyGraph(const QString& code);
    static QJsonArray buildDataFlowGraph(const QString& code);
    static QJsonArray buildControlFlowGraph(const QString& code);
    static QJsonObject analyzeArchitecturePatterns(const QString& code);
    static double calculateCyclomaticComplexity(const QString& code);
    static QMap<QString, int> extractSymbolTable(const QString& code);
    static QList<QPair<int, int>> findCodeClones(const QString& code);
    static QJsonObject performTaintAnalysisImpl(const QString& code);
    static QJsonObject performTypeFlowAnalysisImpl(const QString& code);
    static QJsonObject buildCallGraph(const QString& code);
    static QJsonObject analyzeMemoryPatterns(const QString& code);
    static QList<DataFlowNode> extractDataFlowNodes(const QString& code);
    static QList<ControlFlowNode> extractControlFlowNodes(const QString& code);
    static QJsonObject generateAdvancedMetrics(const QString& code);
    static QString generateAdvancedReasoningText(const AdvancedReviewResult& result);
};

#endif // ENTERPRISE_ADVANCED_CODE_REVIEW_HPP
