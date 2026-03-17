#pragma once
/*  AgenticOrchestrator.hpp - AI Agent Orchestration System
    Provides comprehensive agentic AI capabilities throughout the IDE
    including code analysis, refactoring, file operations, and autonomous editing.
    
    \file AgenticOrchestrator.hpp
    \brief Central AI orchestration system for RawrXD IDE
    \author RawrXD Team
    \date December 3, 2025
*/

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QFuture>
#include <QProcess>
#include <memory>

// Forward declarations for ASM file operations
extern "C" {
    int AgenticReadFile(const char* filepath, void** out_buffer, quint64* out_size);
    int AgenticWriteFile(const char* filepath, const void* buffer, quint64 size);
    int AgenticDeleteFile(const char* filepath);
    int AgenticCreateFile(const char* filepath);
    quint64 AgenticSearchFiles(const char* directory, const char* pattern, void (*callback)(const char*));
    int AgenticGetLastError();
}

/**
 * \class AgenticCapability
 * \brief Enumeration of AI agent capabilities
 */
enum class AgenticCapability {
    FileRead,
    FileWrite,
    FileCreate,
    FileDelete,
    FileSearch,
    CodeAnalysis,
    CodeRefactor,
    CodeGeneration,
    TestGeneration,
    DocumentationGeneration,
    BugDetection,
    PerformanceAnalysis,
    SecurityAudit,
    DependencyManagement,
    GitOperations,
    BuildAutomation,
    DeploymentAutomation,
    MultiFileEdit,
    SemanticSearch,
    ContextAwareness,
    LongTermMemory,
    TaskPlanning,
    SelfCorrection,
    ExplainCode,
    SuggestFixes,
    AutoFormat,
    ImportOptimization
};

/**
 * \class AgenticTask
 * \brief Represents an asynchronous AI task
 */
struct AgenticTask {
    QString id;
    QString type;
    QString description;
    QJsonObject parameters;
    QString status; // "pending", "running", "completed", "failed"
    QJsonObject result;
    QString errorMessage;
    qint64 startTime;
    qint64 endTime;
    
    AgenticTask() : startTime(0), endTime(0) {}
};

/**
 * \class AgenticOrchestrator
 * \brief Central orchestration system for AI agents
 * 
 * Manages all agentic AI capabilities across the IDE:
 * - File system operations via ASM backend
 * - Code analysis and generation
 * - Multi-file refactoring
 * - Autonomous task execution
 * - Long-term context memory
 * - Self-correction and learning
 */
class AgenticOrchestrator : public QObject
{
    Q_OBJECT

public:
    explicit AgenticOrchestrator(QObject* parent = nullptr);
    ~AgenticOrchestrator();

    // Capability queries
    bool hasCapability(AgenticCapability cap) const;
    QStringList availableCapabilities() const;

    // File operations (delegates to ASM backend)
    QString readFile(const QString& filepath);
    bool writeFile(const QString& filepath, const QString& content);
    bool createFile(const QString& filepath);
    bool deleteFile(const QString& filepath);
    QStringList searchFiles(const QString& directory, const QString& pattern);

    // High-level agentic operations
    QString analyzeCode(const QString& code, const QString& language);
    QString suggestRefactoring(const QString& code, const QString& context);
    QString generateTests(const QString& code, const QString& framework);
    QString generateDocumentation(const QString& code, const QString& style);
    QStringList detectBugs(const QString& code, const QString& language);
    QString explainCode(const QString& code, int lineStart, int lineEnd);
    QStringList suggestFixes(const QString& code, const QString& errorMessage);
    QString autoFormat(const QString& code, const QString& language);

    // Multi-file operations
    bool refactorAcrossFiles(const QStringList& files, const QString& refactorType, const QJsonObject& params);
    QStringList findReferences(const QString& symbol, const QStringList& searchPaths);
    bool renameSymbol(const QString& oldName, const QString& newName, const QStringList& affectedFiles);

    // Task management
    QString submitTask(const QString& taskType, const QJsonObject& parameters);
    AgenticTask getTaskStatus(const QString& taskId);
    void cancelTask(const QString& taskId);
    QList<AgenticTask> activeTasks() const;

    // Context and memory
    void addContext(const QString& key, const QVariant& value);
    QVariant getContext(const QString& key) const;
    void clearContext();
    void storeMemory(const QString& category, const QJsonObject& data);
    QJsonObject retrieveMemory(const QString& category) const;

    // Configuration
    void setModel(const QString& modelPath);
    void setMaxTokens(int tokens);
    void setTemperature(double temp);
    void enableCapability(AgenticCapability cap, bool enabled);

signals:
    void taskStarted(const QString& taskId);
    void taskProgress(const QString& taskId, int progress, const QString& message);
    void taskCompleted(const QString& taskId, const QJsonObject& result);
    void taskFailed(const QString& taskId, const QString& error);
    void fileChanged(const QString& filepath);
    void codeAnalysisReady(const QString& analysis);
    void suggestionsAvailable(const QStringList& suggestions);

private slots:
    void onTaskFinished(const QString& taskId);
    void onStreamingChunk(const QString& taskId, const QString& chunk);

private:
    struct Impl;
    std::unique_ptr<Impl> d;

    void executeTask(AgenticTask& task);
    QString callLLM(const QString& prompt, const QJsonObject& params);
    void updateTaskStatus(const QString& taskId, const QString& status);
    
    // ASM backend helpers
    QByteArray readFileRaw(const QString& filepath);
    bool writeFileRaw(const QString& filepath, const QByteArray& data);
};

/**
 * \class AgenticFileWatcher
 * \brief Monitors file system for AI-driven operations
 */
class AgenticFileWatcher : public QObject
{
    Q_OBJECT

public:
    explicit AgenticFileWatcher(AgenticOrchestrator* orchestrator, QObject* parent = nullptr);
    
    void watchDirectory(const QString& path, bool recursive = true);
    void unwatchDirectory(const QString& path);

signals:
    void fileModified(const QString& filepath);
    void fileCreated(const QString& filepath);
    void fileDeleted(const QString& filepath);

private:
    AgenticOrchestrator* m_orchestrator;
    QMap<QString, void*> m_watchers;
};

/**
 * \class AgenticCodeAnalyzer
 * \brief Specialized AI-powered code analysis
 */
class AgenticCodeAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit AgenticCodeAnalyzer(AgenticOrchestrator* orchestrator, QObject* parent = nullptr);
    
    struct AnalysisResult {
        QString summary;
        QStringList issues;
        QStringList suggestions;
        QJsonObject metrics;
        int complexity;
        int maintainabilityIndex;
    };

    QFuture<AnalysisResult> analyzeAsync(const QString& code, const QString& language);
    AnalysisResult analyzeSync(const QString& code, const QString& language);

signals:
    void analysisComplete(const AnalysisResult& result);

private:
    AgenticOrchestrator* m_orchestrator;
};

/**
 * \class AgenticRefactoringEngine
 * \brief AI-powered refactoring engine
 */
class AgenticRefactoringEngine : public QObject
{
    Q_OBJECT

public:
    explicit AgenticRefactoringEngine(AgenticOrchestrator* orchestrator, QObject* parent = nullptr);
    
    enum RefactorType {
        ExtractMethod,
        ExtractVariable,
        InlineMethod,
        RenameSymbol,
        MoveMethod,
        ChangeSignature,
        ConvertToLambda,
        SimplifyExpression
    };

    struct RefactoringPlan {
        RefactorType type;
        QStringList affectedFiles;
        QJsonObject changes;
        QString preview;
    };

    QFuture<RefactoringPlan> planRefactoring(RefactorType type, const QJsonObject& params);
    bool applyRefactoring(const RefactoringPlan& plan);

signals:
    void refactoringPlanned(const RefactoringPlan& plan);
    void refactoringApplied(bool success);

private:
    AgenticOrchestrator* m_orchestrator;
};
