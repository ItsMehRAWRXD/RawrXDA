// ═══════════════════════════════════════════════════════════════════════════════
// BIGDADDYG AUDIT ENGINE - COMPREHENSIVE IDE ANALYSIS
// Uses 40GB model to audit entire IDE from -0-800b
// ═══════════════════════════════════════════════════════════════════════════════

#ifndef BIGDADDYG_AUDIT_ENGINE_H
#define BIGDADDYG_AUDIT_ENGINE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QThreadPool>
#include <math>
#include <algorithm>
#include <functional>

// Audit ranges
enum class AuditRange {
    RANGE_NEGATIVE_0_TO_800B = 0,    // -0-800b comprehensive audit
    RANGE_FULL_IDE = 1,               // Entire IDE analysis
    RANGE_CRITICAL_ONLY = 2,         // Critical components only
    RANGE_PERFORMANCE = 3,            // Performance-critical paths
    RANGE_SECURITY = 4                // Security-sensitive areas
};

// Audit severity levels
enum class AuditSeverity {
    CRITICAL = 0,     // Must fix immediately
    HIGH = 1,         // High priority
    MEDIUM = 2,       // Medium priority
    LOW = 3,          // Low priority
    INFO = 4          // Informational only
};

// Audit finding structure
struct AuditFinding {
    QString id;
    AuditSeverity severity;
    QString component;
    QString filePath;
    int lineNumber;
    QString description;
    QString recommendation;
    double confidence;           // 0.0-1.0 confidence level
    QString evidence;           // Supporting evidence
    QJsonObject metrics;       // Performance/security metrics
    QDateTime timestamp;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["severity"] = static_cast<int>(severity);
        obj["component"] = component;
        obj["filePath"] = filePath;
        obj["lineNumber"] = lineNumber;
        obj["description"] = description;
        obj["recommendation"] = recommendation;
        obj["confidence"] = confidence;
        obj["evidence"] = evidence;
        obj["metrics"] = metrics;
        obj["timestamp"] = timestamp.toString(Qt::ISODate);
        return obj;
    }
};

// Component analysis result
struct ComponentAnalysis {
    QString name;
    QString path;
    int fileCount;
    int lineCount;
    double complexityScore;     // 0.0-1.0
    double securityScore;       // 0.0-1.0
    double performanceScore;     // 0.0-1.0
    QVector<AuditFinding> findings;
    QJsonObject metrics;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["name"] = name;
        obj["path"] = path;
        obj["fileCount"] = fileCount;
        obj["lineCount"] = lineCount;
        obj["complexityScore"] = complexityScore;
        obj["securityScore"] = securityScore;
        obj["performanceScore"] = performanceScore;
        
        QJsonArray findingsArray;
        for (const auto& finding : findings) {
            findingsArray.append(finding.toJson());
        }
        obj["findings"] = findingsArray;
        obj["metrics"] = metrics;
        return obj;
    }
};

// Audit statistics
struct AuditStats {
    int totalComponents;
    int totalFiles;
    int totalLines;
    int criticalFindings;
    int highFindings;
    int mediumFindings;
    int lowFindings;
    int infoFindings;
    double overallScore;        // 0.0-1.0
    double securityScore;
    double performanceScore;
    double maintainabilityScore;
    QDateTime startTime;
    QDateTime endTime;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["totalComponents"] = totalComponents;
        obj["totalFiles"] = totalFiles;
        obj["totalLines"] = totalLines;
        obj["criticalFindings"] = criticalFindings;
        obj["highFindings"] = highFindings;
        obj["mediumFindings"] = mediumFindings;
        obj["lowFindings"] = lowFindings;
        obj["infoFindings"] = infoFindings;
        obj["overallScore"] = overallScore;
        obj["securityScore"] = securityScore;
        obj["performanceScore"] = performanceScore;
        obj["maintainabilityScore"] = maintainabilityScore;
        obj["startTime"] = startTime.toString(Qt::ISODate);
        obj["endTime"] = endTime.toString(Qt::ISODate);
        return obj;
    }
};

// 40GB Model Integration
class BigDaddyg40GBModel {
public:
    struct ModelResponse {
        QString analysis;
        double confidence;
        QJsonObject metrics;
        QVector<QString> recommendations;
    };
    
    static ModelResponse analyzeCode(const QString& code, const QString& context);
    static ModelResponse analyzeArchitecture(const QString& componentName, const QJsonObject& structure);
    static ModelResponse assessSecurity(const QString& code, const QString& componentType);
    static ModelResponse assessPerformance(const QString& code, const QString& componentType);
};

class BigDaddygAuditEngine : public QObject {
    Q_OBJECT

public:
    explicit BigDaddygAuditEngine(QObject* parent = nullptr);
    ~BigDaddygAuditEngine();
    
    // Configuration
    void setProjectRoot(const QString& rootPath);
    void setAuditRange(AuditRange range);
    void setUse40GBModel(bool use);
    void setParallelAnalysis(bool enable);
    void setMaxThreads(int threads);
    
    // Audit execution
    bool startAudit();
    void stopAudit();
    void pauseAudit();
    void resumeAudit();
    
    // Results
    AuditStats getStats() const;
    QVector<ComponentAnalysis> getComponentAnalyses() const;
    QVector<AuditFinding> getAllFindings() const;
    QVector<AuditFinding> getFindingsBySeverity(AuditSeverity severity) const;
    
    // Export
    QString generateAuditReport() const;
    bool exportAuditReport(const QString& filePath) const;
    QJsonObject exportToJson() const;

signals:
    void auditStarted();
    void auditProgress(double percent, const QString& currentComponent);
    void auditCompleted();
    void auditPaused();
    void auditResumed();
    void auditStopped();
    void componentAnalyzed(const QString& componentName, int findings);
    void findingDiscovered(const AuditFinding& finding);

private slots:
    void onComponentAnalysisComplete(const QString& component, const ComponentAnalysis& analysis);

private:
    // Audit execution
    void executeAudit();
    void analyzeComponent(const QString& componentPath, const QString& componentName);
    void analyzeFile(const QString& filePath, ComponentAnalysis& componentAnalysis);
    
    // Analysis methods
    AuditFinding analyzeCodeComplexity(const QString& filePath, const QString& code);
    AuditFinding analyzeSecurityVulnerability(const QString& filePath, const QString& code);
    AuditFinding analyzePerformanceIssue(const QString& filePath, const QString& code);
    AuditFinding analyzeMaintainability(const QString& filePath, const QString& code);
    AuditFinding analyzeDependencies(const QString& filePath, const QString& code);
    
    // Helper methods
    QString readFile(const QString& filePath);
    QVector<QString> getAllSourceFiles() const;
    QVector<QString> getComponents() const;
    double calculateComplexityScore(const QString& code);
    double calculateSecurityScore(const QString& code);
    double calculatePerformanceScore(const QString& code);
    void updateStats(const ComponentAnalysis& analysis);
    
    // Data members
    QString m_projectRoot;
    AuditRange m_auditRange;
    bool m_use40GBModel;
    bool m_parallelAnalysis;
    int m_maxThreads;
    
    QHash<QString, ComponentAnalysis> m_componentAnalyses;
    QVector<AuditFinding> m_allFindings;
    AuditStats m_stats;
    
    QMutex m_mutex;
    QThreadPool m_threadPool;
    std::atomic<bool> m_isRunning;
    std::atomic<bool> m_isPaused;
    std::atomic<bool> m_stopRequested;
};

#endif // BIGDADDYG_AUDIT_ENGINE_H
