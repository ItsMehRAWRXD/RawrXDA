#pragma once
#include <QObject>
#include <QDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QFuture>
#include <QMutex>
#include <QReadWriteLock>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QSemaphore>
#include <QAtomicInt>
#include <QAtomicPointer>
#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE
class QProgressBar;
class QTextEdit;
class QTreeWidget;
QT_END_NAMESPACE

struct LanguageProfile {
    QString name;
    QStringList extensions;
    QVector<QRegularExpression> stubPatterns;
    QString singleLineComment;
    QString multiLineCommentStart;
    QString multiLineCommentEnd;
    QHash<QString, QString> fixTemplates;
    int complexityWeight = 1;
};

struct CodeMetrics {
    int linesOfCode = 0;
    int commentLines = 0;
    int blankLines = 0;
    int cyclomaticComplexity = 0;
    int maxNestingDepth = 0;
    double maintainabilityIndex = 0.0;
};

struct AgenticTask {
    QString filePath;
    int lineNumber;
    int column;
    QString stubType;
    QString severity; // "critical", "warning", "info"
    QString contextBefore;
    QString contextAfter;
    QString fullFunctionContext;
    QString suggestedFix;
    QString originalCode;
    bool applied = false;
    qint64 processingTimeMs = 0;
    QVector<QString> dependencies;
};

struct DigestionStats {
    QAtomicInt totalFiles{0};
    QAtomicInt scannedFiles{0};
    QAtomicInt stubsFound{0};
    QAtomicInt extensionsApplied{0};
    QAtomicInt errors{0};
    QAtomicInt warnings{0};
    QAtomicInt criticals{0};
    QHash<QString, int> stubsByLanguage;
    QHash<QString, int> stubsByType;
    QElapsedTimer timer;
    qint64 peakMemoryUsage = 0;
    int parallelWorkers = 0;
};

struct DigestionCheckpoint {
    QString rootDir;
    int lastProcessedIndex;
    QDateTime timestamp;
    QJsonArray pendingResults;
    DigestionStats stats;
};

class DigestionReverseEngineeringSystem : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(DigestionStats stats READ stats NOTIFY statsChanged)

public:
    explicit DigestionReverseEngineeringSystem(QObject *parent = nullptr);
    ~DigestionReverseEngineeringSystem();

    // Main pipeline
    void runFullDigestionPipeline(const QString &rootDir, 
                                  int maxFiles = 0,
                                  int chunkSize = 50, 
                                  int maxTasksPerFile = 0,
                                  bool applyExtensions = true,
                                  const QStringList &excludePatterns = QStringList());
    
    // Checkpoint/resume for 1500+ file codebases
    void saveCheckpoint(const QString &path);
    bool loadCheckpoint(const QString &path);
    void resumeFromCheckpoint(const QString &checkpointPath, bool applyExtensions = true);
    
    // Incremental scanning (git diff aware)
    void runIncrementalDigestion(const QString &rootDir, 
                                 const QStringList &modifiedFiles,
                                 bool applyExtensions = true);
    
    // Advanced analysis
    QHash<QString, CodeMetrics> analyzeCodeMetrics(const QStringList &files);
    QVector<QPair<QString, QString>> findDuplicateCode(int minLines = 5);
    QHash<QString, QVector<QString>> buildDependencyGraph();
    
    // Control
    void stop();
    void pause();
    void resume();
    bool isRunning() const;
    bool isPaused() const;
    
    // Results
    DigestionStats stats() const;
    QJsonObject lastReport() const;
    QVector<AgenticTask> pendingTasks() const;
    QString generatePatchFile(const QString &originalDir, const QString &modifiedDir);
    
    // GUI Integration helpers
    void bindToProgressBar(QProgressBar *bar);
    void bindToLogView(QTextEdit *log);
    void bindToTreeView(QTreeWidget *tree);

signals:
    void progressUpdate(int filesProcessed, int totalFiles, int stubsFound, int percentComplete);
    void fileScanned(const QString &path, int stubsInFile, const QString &language);
    void agenticTaskGenerated(const AgenticTask &task);
    void extensionApplied(const QString &file, int line, const QString &fixType);
    void extensionFailed(const QString &file, int line, const QString &reason);
    void errorOccurred(const QString &file, const QString &error, bool critical);
    void warningIssued(const QString &file, const QString &warning);
    void pipelineFinished(const QJsonObject &report, qint64 elapsedMs);
    void chunkCompleted(int chunkIndex, int chunksTotal, int stubsInChunk);
    void metricsCalculated(const QString &file, const CodeMetrics &metrics);
    void duplicateFound(const QString &file1, const QString &file2, int line1, int line2, int similarity);
    void runningChanged(bool running);
    void statsChanged(const DigestionStats &stats);
    void checkpointSaved(const QString &path);

private slots:
    void onFileProcessed(const QString &path, const QJsonObject &result);
    void onTaskApplied(const QString &file, int line, bool success);

private:
    void initializeLanguageProfiles();
    void initializeFixTemplates();
    QStringList collectFiles(const QString &rootDir, int maxFiles, const QStringList &excludes);
    void processChunk(const QStringList &files, int chunkId, int totalChunks, 
                      int maxTasksPerFile, bool applyExtensions);
    void scanFile(const QString &filePath, int maxTasksPerFile, bool applyExtensions);
    QList<AgenticTask> findStubs(const QString &content, const LanguageProfile &lang, 
                                  const QString &filePath, int maxTasks);
    bool applyAgenticExtension(const QString &filePath, const AgenticTask &task);
    QString generateFix(const AgenticTask &task, const LanguageProfile &lang);
    QString detectLanguage(const QString &filePath);
    CodeMetrics calculateMetrics(const QString &content, const LanguageProfile &lang);
    QString extractFunctionContext(const QString &content, int lineNumber);
    QString generateUnifiedDiff(const QString &original, const QString &modified, 
                                const QString &filename);
    void updateStats();
    void emitProgress();
    void generateReport();
    
    // Threading control
    mutable QMutex m_mutex;
    mutable QReadWriteLock m_statsLock;
    QSemaphore m_pauseSemaphore{0};
    QAtomicInt m_running{0};
    QAtomicInt m_paused{0};
    QAtomicInt m_stopRequested{0};
    QAtomicInt m_currentFileIndex{0};
    
    // Data
    DigestionStats m_stats;
    QJsonArray m_results;
    QVector<AgenticTask> m_pendingTasks;
    QList<LanguageProfile> m_profiles;
    QString m_rootDir;
    QJsonObject m_lastReport;
    QElapsedTimer m_timer;
    QHash<QString, LanguageProfile> m_profileMap;
    
    // GUI bindings (weak refs)
    QProgressBar *m_boundProgress = nullptr;
    QTextEdit *m_boundLog = nullptr;
    QTreeWidget *m_boundTree = nullptr;
    
    // Checkpointing
    DigestionCheckpoint m_checkpoint;
    bool m_hasCheckpoint = false;
};
