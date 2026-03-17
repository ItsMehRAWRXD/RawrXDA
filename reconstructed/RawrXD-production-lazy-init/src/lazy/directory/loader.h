#ifndef LAZY_DIRECTORY_LOADER_H
#define LAZY_DIRECTORY_LOADER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>

namespace RawrXD {

struct DirectoryEntry {
    QString name;
    QString path;
    qint64 size;
    QDateTime modified;
    bool isDirectory;
    bool isHidden;
    
    DirectoryEntry() : size(0), isDirectory(false), isHidden(false) {}
    DirectoryEntry(const QFileInfo& info)
        : name(info.fileName())
        , path(info.absoluteFilePath())
        , size(info.size())
        , modified(info.lastModified())
        , isDirectory(info.isDir())
        , isHidden(info.isHidden()) {}
    
    bool operator<(const DirectoryEntry& other) const {
        if (isDirectory != other.isDirectory) {
            return isDirectory; // Directories first
        }
        return name.toLower() < other.name.toLower();
    }
};

class LazyDirectoryLoader : public QObject {
    Q_OBJECT

public:
    static LazyDirectoryLoader& instance();
    
    void initialize(int batchSize = 100, int throttleMs = 100);
    void shutdown();
    
    // Directory loading
    bool loadDirectory(const QString& path, bool recursive = false);
    void cancelLoading();
    
    // Entry access
    QVector<DirectoryEntry> getEntries(int start = 0, int count = -1) const;
    int getEntryCount() const;
    bool isLoaded() const;
    bool isLoading() const;
    
    // Filtering
    void setGitignoreFiltering(bool enable);
    void addFilterPattern(const QString& pattern);
    void clearFilters();
    
    // Statistics
    qint64 getTotalSize() const;
    int getFileCount() const;
    int getDirectoryCount() const;
    
    // Cache management
    void clearCache();
    void setCacheSize(int maxEntries);
    
signals:
    void loadingStarted(const QString& path);
    void loadingProgress(int loaded, int total);
    void loadingFinished(const QString& path, int entryCount);
    void loadingCancelled(const QString& path);
    void entryAdded(const DirectoryEntry& entry);
    void errorOccurred(const QString& error);

private slots:
    void onBatchReady();
    void onLoadingFinished();

private:
    LazyDirectoryLoader() = default;
    ~LazyDirectoryLoader();
    
    QVector<DirectoryEntry> loadBatch(const QString& path, int start, int batchSize, bool recursive);
    bool shouldFilter(const QString& fileName) const;
    void processGitignore(const QString& path);
    
    mutable QMutex mutex_;
    QVector<DirectoryEntry> entries_;
    QHash<QString, QVector<DirectoryEntry>> cache_;
    
    QFuture<QVector<DirectoryEntry>> future_;
    QFutureWatcher<QVector<DirectoryEntry>> futureWatcher_;
    
    QString currentPath_;
    bool recursive_ = false;
    bool loading_ = false;
    bool loaded_ = false;
    
    int batchSize_;
    int throttleMs_;
    int currentBatch_ = 0;
    int totalEntries_ = 0;
    
    bool gitignoreFiltering_ = false;
    QStringList filterPatterns_;
    QStringList gitignorePatterns_;
    
    int maxCacheEntries_ = 1000;
    
    static const int DEFAULT_BATCH_SIZE = 100;
    static const int DEFAULT_THROTTLE_MS = 100;
};

// Convenience macros
#define LAZY_LOAD_DIR(path) RawrXD::LazyDirectoryLoader::instance().loadDirectory(path)
#define LAZY_GET_ENTRIES(start, count) RawrXD::LazyDirectoryLoader::instance().getEntries(start, count)
#define LAZY_CANCEL() RawrXD::LazyDirectoryLoader::instance().cancelLoading()

} // namespace RawrXD

#endif // LAZY_DIRECTORY_LOADER_H