#pragma once
#include <QObject>
#include <QDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QFuture>
#include <QMutex>
#include <QElapsedTimer>
#include <QHash>
#include <QCache>
#include <QThreadPool>
#include <QMap>
#include <QRegularExpression>
#include <functional>
#include <memory>

struct LanguageProfile {
    QString name;
    QStringList extensions;
    QVector<QRegularExpression> stubPatterns;
    QString singleLineComment;
    QString multiLineCommentStart;
    QString multiLineCommentEnd;
    bool supportsInlineAsm = false;
};

struct FileDigest {
    QString path;
    QString language;
    QByteArray hash;
    qint64 lastModified;
    int lineCount = 0;
    bool hasStubs = false;
};

struct AgenticTask {
    QString filePath;
    int lineNumber;
    QString stubType;
    QString contextBefore;
    QString contextAfter;
    QString fullContext;
    QString suggestedFix;
    QString confidence; // "high", "medium", "low"
    bool applied = false;
    qint64 timestamp;
    QString backupId;
};

struct DigestionConfig {
    int maxFiles = 0;           // 0 = unlimited
    int chunkSize = 50;         // Files per parallel chunk
    int maxTasksPerFile = 0;    // 0 = unlimited stubs per file
    bool applyExtensions = false;
    bool createBackups = true;
    bool useGitMode = false;    // Only scan git-modified files
    bool incremental = true;    // Use hash cache
    int threadCount = 0;        // 0 = auto (QThreadPool default)
    int maxFileSizeMB = 10;     // Skip files larger than this
    QString backupDir = ".digest_backups";
    QStringList excludePatterns; // Regex patterns to exclude
};

struct DigestionStats {
    QAtomicInt totalFiles{0};
    QAtomicInt scannedFiles{0};
    QAtomicInt stubsFound{0};
    QAtomicInt extensionsApplied{0};
    QAtomicInt errors{0};
    QAtomicInt skippedLargeFiles{0};
    QAtomicInt cacheHits{0};
    qint64 elapsedMs = 0;
    qint64 bytesProcessed = 0;
};

class DigestionReverseEngineeringSystem : public QObject {
    Q_OBJECT
public:
    explicit DigestionReverseEngineeringSystem(QObject *parent = nullptr);
    ~DigestionReverseEngineeringSystem();

    void runFullDigestionPipeline(const QString &rootDir, const DigestionConfig &config = DigestionConfig());
    void stop();
    bool isRunning() const;
    DigestionStats stats() const;
    QJsonObject lastReport() const;
    QJsonObject generateIncrementalReport(const QStringList &changedFiles);
    
    // Advanced: Pre-load hash cache for incremental mode
    void loadHashCache(const QString &cacheFile);
    void saveHashCache(const QString &cacheFile);

signals:
    void pipelineStarted(const QString &rootDir, int totalFiles);
    void progressUpdate(int filesProcessed, int totalFiles, int stubsFound, int percent);
    void fileScanned(const QString &path, const QString &language, int stubsInFile);
    void agenticTaskDiscovered(const AgenticTask &task);
    void extensionApplied(const QString &file, int line, const QString &fixType);
    void extensionFailed(const QString &file, int line, const QString &reason);
    void errorOccurred(const QString &file, const QString &error);
    void chunkCompleted(int chunkIndex, int totalChunks);
    void pipelineFinished(const QJsonObject &report, qint64 elapsedMs);
    void backupCreated(const QString &original, const QString &backupPath);
    void rollbackAvailable(const QString &backupId);

public slots:
    bool rollbackFile(const QString &backupId);
    bool rollbackAll(const QDateTime &timestamp);
    void clearCache();

private:
    void initializeLanguageProfiles();
    void scanDirectory(const QString &rootDir);
    void processChunk(const QVector<FileDigest> &files, int chunkId, const DigestionConfig &config);
    void scanSingleFile(const FileDigest &fileDigest, const DigestionConfig &config);
    QList<AgenticTask> findStubs(const QString &content, const LanguageProfile &lang, const FileDigest &file, int maxTasks);
    bool applyAgenticFix(const QString &filePath, const AgenticTask &task, const DigestionConfig &config);
    QString generateIntelligentFix(const AgenticTask &task, const LanguageProfile &lang);
    QByteArray computeFileHash(const QString &filePath);
    bool shouldProcessFile(const QString &filePath, const DigestionConfig &config);
    void createBackup(const QString &filePath, const QString &backupId);
    void generateFinalReport();
    void updateProgress();
    
    // ASM-optimized internal
    QByteArray fastHash(const QByteArray &data);
    bool asmOptimizedScan(const QByteArray &data, const char *pattern);

    mutable QMutex m_mutex;
    mutable QMutex m_backupMutex;
    QAtomicInt m_running{0};
    QAtomicInt m_stopRequested{0};
    DigestionStats m_stats;
    QJsonArray m_results;
    QVector<LanguageProfile> m_profiles;
    QString m_rootDir;
    QJsonObject m_lastReport;
    QElapsedTimer m_timer;
    
    // Caches
    QHash<QString, QByteArray> m_hashCache; // path -> hash
    QHash<QString, QString> m_backupRegistry; // backupId -> original path
    QHash<QString, qint64> m_backupTimes; // backupId -> timestamp
    QCache<QString, LanguageProfile> m_profileCache;
    QString m_backupDir;
    
    // Threading
    std::unique_ptr<QThreadPool> m_threadPool;
    
    // Git integration
    QStringList getGitModifiedFiles(const QString &rootDir);
    QStringList getGitIgnoredPatterns(const QString &rootDir);
};
