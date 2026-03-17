#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonObject>
#include <QFuture>
#include <QMutex>
#include <QElapsedTimer>
#include <QQueue>
#include <QThreadPool>
#include <functional>
#include <memory>
#include <QDateTime>
#include <QMap>
#include <QRegularExpression>

// Forward declarations for AI system integration
class CompletionEngine;
class CodebaseContextAnalyzer;
class SmartRewriteEngine;
class MultiModalModelRouter;
class AdvancedCodingAgent;

struct DigestionTask {
    int id;
    QString filePath;
    QString language;
    int priority;
    QDateTime created;
    QDateTime started;
    QDateTime completed;
    int stubsFound;
    int stubsFixed;
    QString status; // pending|running|completed|error
    QString errorMsg;
};

struct StubInstance {
    int id;
    int taskId;
    QString filePath;
    int lineNumber;
    QString stubType;
    QString originalCode;
    QString context; // Added for context-aware fixes
    QString suggestedFix;
    QString appliedFix;
    bool applied;
    float confidenceScore;
    QDateTime detected;
    QDateTime fixed;
};

class RawrXDDigestionEngine : public QObject {
    Q_OBJECT
public:
    explicit RawrXDDigestionEngine(QObject *parent = nullptr);
    ~RawrXDDigestionEngine();
    
    // Core pipeline
    void initializeDatabase(const QString &dbPath = "digestion.db");
    void runFullDigestionPipeline(const QString &rootDir, 
                                  int maxFiles = 0,
                                  int chunkSize = 50,
                                  int maxTasksPerFile = 0,
                                  bool applyExtensions = true,
                                  bool useAVX512 = true);
    
    // AI Integration hooks
    void setCompletionEngine(CompletionEngine *engine);
    void setContextAnalyzer(CodebaseContextAnalyzer *analyzer);
    void setRewriteEngine(SmartRewriteEngine *engine);
    void setModelRouter(MultiModalModelRouter *router);
    void setCodingAgent(AdvancedCodingAgent *agent);
    
    // Control
    void pause();
    void resume();
    void stop();
    bool isRunning() const;
    bool isPaused() const;
    
    // Queries
    QList<DigestionTask> getTaskHistory(int limit = 100);
    QList<StubInstance> getPendingStubs();
    QJsonObject generateReport(const QDateTime &from = QDateTime(), 
                               const QDateTime &to = QDateTime());
    bool exportToJson(const QString &path);
    bool importFromJson(const QString &path);
    
    // Statistics
    struct Stats {
        int totalFilesScanned;
        int totalStubsFound;
        int totalStubsFixed;
        int totalErrors;
        qint64 totalTimeMs;
        float avgTimePerFileMs;
        QMap<QString, int> stubsByLanguage;
    };
    Stats getStatistics() const;

signals:
    void pipelineStarted(int totalFiles);
    void fileScanStarted(const QString &path);
    void fileScanCompleted(const QString &path, int stubsFound, int stubsFixed);
    void stubDetected(const StubInstance &stub);
    void stubFixed(const StubInstance &stub);
    void progressUpdate(int filesProcessed, int totalFiles, int stubsFound, int stubsFixed);
    void errorOccurred(const QString &file, const QString &error);
    void pipelineCompleted(const Stats &stats);
    void checkpointSaved(int filesProcessed);
    void aiFixRequested(const QString &context, QString *outFix);
    void aiFixGenerated(const QString &fix, float confidence);

private slots:
    void processNextChunk();
    void onFileScanned(const QString &path, const QList<StubInstance> &stubs);
    void onStubFixApplied(int stubId, bool success);

private:
    void setupTables();
    void scanFileInternal(const QString &filePath, int maxTasksPerFile, bool applyExtensions, bool useAVX512);
    QList<StubInstance> detectStubsAVX512(const QString &content, const QString &language, 
                                          const QString &filePath);
    QList<StubInstance> detectStubsFallback(const QString &content, const QString &language, 
                                            const QString &filePath);
    QString generateAIFix(const StubInstance &stub);
    bool applyFix(const StubInstance &stub, const QString &fix);
    void saveCheckpoint();
    void loadCheckpoint();
    void updateTaskStatus(int taskId, const QString &status);
    
    // Database
    QSqlDatabase m_db;
    mutable QMutex m_dbMutex;
    
    // State
    struct SystemState {
        QAtomicInt running{0};
        QAtomicInt paused{0};
        QAtomicInt stopRequested{0};
        QAtomicInt filesProcessed{0};
        QAtomicInt totalFiles{0};
        QAtomicInt stubsFound{0};
        QAtomicInt stubsFixed{0};
        QString currentRootDir;
        QElapsedTimer timer;
    } m_state;
    
    // AI System pointers
    CompletionEngine *m_completionEngine = nullptr;
    CodebaseContextAnalyzer *m_contextAnalyzer = nullptr;
    SmartRewriteEngine *m_rewriteEngine = nullptr;
    MultiModalModelRouter *m_modelRouter = nullptr;
    AdvancedCodingAgent *m_codingAgent = nullptr;
    
    // Threading
    QThreadPool m_threadPool;
    QQueue<QString> m_pendingFiles;
    QMutex m_queueMutex;
    int m_chunkSize = 50;
    int m_checkpointInterval = 100; // Save every 100 files
    
    // Language profiles
    struct LangProfile {
        QString name;
        QStringList extensions;
        QVector<QByteArray> stubSignatures; // For AVX-512
        QRegularExpression stubRegex;
    };
    QVector<LangProfile> m_profiles;
    void initProfiles();
};
