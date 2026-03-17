#ifndef CODE_INTELLIGENCE_H
#define CODE_INTELLIGENCE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QSet>
#include <QJsonObject>
#include <QMutex>

/**
 * @brief Advanced code intelligence engine
 * Features: Semantic search, call graphs, dependency analysis, dead code detection
 */
class CodeIntelligence : public QObject
{
    Q_OBJECT

public:
    enum class AnalysisType { SemanticSearch, CallGraph, Dependencies, DeadCode, Complexity };
    
    struct CallGraphNode {
        QString functionName, filePath;
        int line;
        QStringList callers, callees;
        int depth;
        QJsonObject toJson() const;
    };
    
    struct DependencyInfo {
        QString sourceFile, targetFile;
        QString dependencyType;  // include, symbol, etc.
        bool isCircular;
        QJsonObject toJson() const;
    };
    
    struct ComplexityMetrics {
        QString functionName, filePath;
        int cyclomaticComplexity, cognitiveComplexity;
        int lineCount, parameterCount;
        double maintainabilityIndex;
        QJsonObject toJson() const;
    };

    explicit CodeIntelligence(QObject* parent = nullptr);
    ~CodeIntelligence();

    // Semantic search
    QStringList semanticSearch(const QString& query, const QString& scope = "");
    QList<QPair<QString, int>> findSimilarCode(const QString& code, double threshold = 0.8);
    QStringList findByPattern(const QString& pattern, bool useRegex = false);

    // Call graph analysis
    CallGraphNode buildCallGraph(const QString& functionName, const QString& filePath);
    QList<CallGraphNode> getCallers(const QString& functionName);
    QList<CallGraphNode> getCallees(const QString& functionName);
    QList<QString> findUnusedFunctions(const QString& filePath);

    // Dependency analysis
    QList<DependencyInfo> analyzeDependencies(const QString& filePath);
    QList<QStringList> findCircularDependencies(const QString& projectRoot);
    QMap<QString, QStringList> buildDependencyMap(const QString& projectRoot);

    // Code metrics
    ComplexityMetrics analyzeComplexity(const QString& functionName, const QString& filePath);
    QList<ComplexityMetrics> getHighComplexityFunctions(int threshold = 10);
    double calculateMaintainabilityIndex(const QString& filePath);

    // Dead code detection
    QStringList findDeadCode(const QString& filePath);
    QStringList findUnreachableCode(const QString& filePath);
    QStringList findUnusedVariables(const QString& filePath);

    // Code quality
    QJsonObject analyzeCodeQuality(const QString& filePath);
    QStringList detectCodeSmells(const QString& filePath);
    int calculateTechnicalDebt(const QString& filePath);

signals:
    void analysisCompleted(AnalysisType type, const QJsonObject& results);
    void progressUpdated(int percentage, const QString& status);
    void codeSmellDetected(const QString& filePath, const QString& smell);

private:
    QString readFile(const QString& filePath);
    QStringList parseSymbols(const QString& code);
    int calculateCyclomaticComplexity(const QString& code);
    bool isCodeReachable(const QString& code, int line);
    
    mutable QMutex m_mutex;
    QMap<QString, QList<CallGraphNode>> m_callGraphCache;
    QMap<QString, ComplexityMetrics> m_metricsCache;
};

#endif // CODE_INTELLIGENCE_H
