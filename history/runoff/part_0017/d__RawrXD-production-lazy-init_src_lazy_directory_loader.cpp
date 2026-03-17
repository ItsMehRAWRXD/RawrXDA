#include "lazy_directory_loader.h"
#include "logging/structured_logger.h"
#include "error_handler.h"
#include <QCoreApplication>
#include <QRegularExpression>

namespace RawrXD {

LazyDirectoryLoader& LazyDirectoryLoader::instance() {
    static LazyDirectoryLoader instance;
    return instance;
}

void LazyDirectoryLoader::initialize(int batchSize, int throttleMs) {
    QMutexLocker lock(&mutex_);
    
    batchSize_ = batchSize;
    throttleMs_ = throttleMs;
    
    // Connect future watcher
    connect(&futureWatcher_, &QFutureWatcher<QVector<DirectoryEntry>>::finished, this, &LazyDirectoryLoader::onLoadingFinished);
    
    LOG_INFO("Lazy directory loader initialized", {
        {"batch_size", batchSize_},
        {"throttle_ms", throttleMs_}
    });
}

void LazyDirectoryLoader::shutdown() {
    QMutexLocker lock(&mutex_);
    
    if (loading_) {
        future_.cancel();
        future_.waitForFinished();
    }
    
    entries_.clear();
    cache_.clear();
    loading_ = false;
    loaded_ = false;
    
    LOG_INFO("Lazy directory loader shut down");
}

bool LazyDirectoryLoader::loadDirectory(const QString& path, bool recursive) {
    QMutexLocker lock(&mutex_);
    
    if (loading_) {
        LOG_WARN("Directory loading already in progress", {{"current_path", currentPath_}});
        return false;
    }
    
    if (!QDir(path).exists()) {
        ERROR_HANDLE("Directory does not exist", ErrorContext()
            .setSeverity(ErrorSeverity::MEDIUM)
            .setCategory(ErrorCategory::FILE_SYSTEM)
            .setOperation("LazyDirectoryLoader loadDirectory")
            .addMetadata("path", path));
        return false;
    }
    
    // Check cache first
    if (cache_.contains(path)) {
        entries_ = cache_[path];
        loaded_ = true;
        emit loadingFinished(path, entries_.size());
        LOG_DEBUG("Directory loaded from cache", {{"path", path}, {"entries", entries_.size()}});
        return true;
    }
    
    currentPath_ = path;
    recursive_ = recursive;
    loading_ = true;
    loaded_ = false;
    currentBatch_ = 0;
    totalEntries_ = 0;
    entries_.clear();
    
    // Process .gitignore if filtering is enabled
    if (gitignoreFiltering_) {
        processGitignore(path);
    }
    
    emit loadingStarted(path);
    
    // Start loading first batch
    future_ = QtConcurrent::run([this, path]() {
        return loadBatch(path, 0, batchSize_, recursive_);
    });
    futureWatcher_.setFuture(future_);
    
    LOG_INFO("Directory loading started", {{"path", path}, {"recursive", recursive}});
    
    return true;
}

void LazyDirectoryLoader::cancelLoading() {
    QMutexLocker lock(&mutex_);
    
    if (loading_) {
        future_.cancel();
        future_.waitForFinished();
        loading_ = false;
        emit loadingCancelled(currentPath_);
        LOG_INFO("Directory loading cancelled", {{"path", currentPath_}});
    }
}

QVector<DirectoryEntry> LazyDirectoryLoader::getEntries(int start, int count) const {
    QMutexLocker lock(&mutex_);
    
    if (!loaded_ && !loading_) {
        return QVector<DirectoryEntry>();
    }
    
    if (start < 0 || start >= entries_.size()) {
        return QVector<DirectoryEntry>();
    }
    
    if (count == -1 || start + count > entries_.size()) {
        count = entries_.size() - start;
    }
    
    return entries_.mid(start, count);
}

int LazyDirectoryLoader::getEntryCount() const {
    QMutexLocker lock(&mutex_);
    return entries_.size();
}

bool LazyDirectoryLoader::isLoaded() const {
    QMutexLocker lock(&mutex_);
    return loaded_;
}

bool LazyDirectoryLoader::isLoading() const {
    QMutexLocker lock(&mutex_);
    return loading_;
}

void LazyDirectoryLoader::setGitignoreFiltering(bool enable) {
    QMutexLocker lock(&mutex_);
    gitignoreFiltering_ = enable;
    LOG_DEBUG("Gitignore filtering", {{"enabled", enable}});
}

void LazyDirectoryLoader::addFilterPattern(const QString& pattern) {
    QMutexLocker lock(&mutex_);
    filterPatterns_.append(pattern);
    LOG_DEBUG("Filter pattern added", {{"pattern", pattern}});
}

void LazyDirectoryLoader::clearFilters() {
    QMutexLocker lock(&mutex_);
    filterPatterns_.clear();
    gitignorePatterns_.clear();
    LOG_DEBUG("Filters cleared");
}

qint64 LazyDirectoryLoader::getTotalSize() const {
    QMutexLocker lock(&mutex_);
    
    qint64 total = 0;
    for (const DirectoryEntry& entry : entries_) {
        if (!entry.isDirectory) {
            total += entry.size;
        }
    }
    
    return total;
}

int LazyDirectoryLoader::getFileCount() const {
    QMutexLocker lock(&mutex_);
    
    int count = 0;
    for (const DirectoryEntry& entry : entries_) {
        if (!entry.isDirectory) {
            count++;
        }
    }
    
    return count;
}

int LazyDirectoryLoader::getDirectoryCount() const {
    QMutexLocker lock(&mutex_);
    
    int count = 0;
    for (const DirectoryEntry& entry : entries_) {
        if (entry.isDirectory) {
            count++;
        }
    }
    
    return count;
}

void LazyDirectoryLoader::clearCache() {
    QMutexLocker lock(&mutex_);
    cache_.clear();
    LOG_DEBUG("Cache cleared");
}

void LazyDirectoryLoader::setCacheSize(int maxEntries) {
    QMutexLocker lock(&mutex_);
    maxCacheEntries_ = maxEntries;
    LOG_DEBUG("Cache size set", {{"max_entries", maxEntries}});
}

void LazyDirectoryLoader::onBatchReady() {
    QMutexLocker lock(&mutex_);
    
    if (!loading_) return;
    
    QVector<DirectoryEntry> batch = future_.result();
    
    if (batch.isEmpty()) {
        // Loading complete
        loading_ = false;
        loaded_ = true;
        
        // Cache the results
        if (cache_.size() >= maxCacheEntries_) {
            // Remove oldest cache entry
            QString oldestKey = cache_.keys().first();
            cache_.remove(oldestKey);
        }
        cache_[currentPath_] = entries_;
        
        emit loadingFinished(currentPath_, entries_.size());
        LOG_INFO("Directory loading finished", {{"path", currentPath_}, {"entries", entries_.size()}});
        return;
    }
    
    // Add batch to entries
    int oldSize = entries_.size();
    entries_.append(batch);
    
    // Sort entries (directories first, then alphabetical)
    std::sort(entries_.begin(), entries_.end());
    
    emit loadingProgress(entries_.size(), totalEntries_);
    
    // Load next batch if there are more entries
    if (batch.size() == batchSize_) {
        currentBatch_++;
        
        // Throttle next batch
        QTimer::singleShot(throttleMs_, this, [this]() {
            QMutexLocker innerLock(&mutex_);
            if (loading_) {
                future_ = QtConcurrent::run([this]() {
                    return loadBatch(currentPath_, currentBatch_ * batchSize_, batchSize_, recursive_);
                });
                futureWatcher_.setFuture(future_);
            }
        });
    } else {
        // Last batch
        loading_ = false;
        loaded_ = true;
        
        // Cache the results
        cache_[currentPath_] = entries_;
        
        emit loadingFinished(currentPath_, entries_.size());
        LOG_INFO("Directory loading finished", {{"path", currentPath_}, {"entries", entries_.size()}});
    }
}

void LazyDirectoryLoader::onLoadingFinished() {
    onBatchReady();
}

QVector<DirectoryEntry> LazyDirectoryLoader::loadBatch(const QString& path, int start, int batchSize, bool recursive) {
    QVector<DirectoryEntry> batch;
    
    try {
        QDir dir(path);
        
        // Get all entries if this is the first batch
        if (start == 0) {
            QFileInfoList allEntries;
            
            if (recursive) {
                allEntries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::DirsFirst);
                
                // Recursively get subdirectory entries
                QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
                for (const QString& subdir : subdirs) {
                    QString subdirPath = dir.filePath(subdir);
                    QFileInfoList subdirEntries = QDir(subdirPath).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::DirsFirst);
                    
                    for (QFileInfo& info : subdirEntries) {
                        info.setFile(subdirPath + "/" + info.fileName());
                    }
                    
                    allEntries.append(subdirEntries);
                }
            } else {
                allEntries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::DirsFirst);
            }
            
            totalEntries_ = allEntries.size();
            
            // Apply filters
            for (int i = allEntries.size() - 1; i >= 0; --i) {
                if (shouldFilter(allEntries[i].fileName())) {
                    allEntries.remove(i);
                    totalEntries_--;
                }
            }
            
            // Store all entries for batch processing
            for (int i = start; i < std::min(start + batchSize, allEntries.size()); ++i) {
                batch.append(DirectoryEntry(allEntries[i]));
            }
        } else {
            // For subsequent batches, we need to re-read the directory
            // This is inefficient but necessary for true lazy loading
            QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::DirsFirst);
            
            // Apply filters
            for (int i = entries.size() - 1; i >= 0; --i) {
                if (shouldFilter(entries[i].fileName())) {
                    entries.remove(i);
                }
            }
            
            for (int i = start; i < std::min(start + batchSize, entries.size()); ++i) {
                batch.append(DirectoryEntry(entries[i]));
            }
        }
        
    } catch (const std::exception& e) {
        ERROR_EXCEPTION(e, ErrorContext()
            .setSeverity(ErrorSeverity::MEDIUM)
            .setCategory(ErrorCategory::FILE_SYSTEM)
            .setOperation("LazyDirectoryLoader loadBatch")
            .addMetadata("path", path));
    }
    
    return batch;
}

bool LazyDirectoryLoader::shouldFilter(const QString& fileName) const {
    // Check gitignore patterns
    for (const QString& pattern : gitignorePatterns_) {
        QRegularExpression regex(pattern, QRegularExpression::CaseInsensitiveOption);
        if (regex.match(fileName).hasMatch()) {
            return true;
        }
    }
    
    // Check custom filter patterns
    for (const QString& pattern : filterPatterns_) {
        QRegularExpression regex(pattern, QRegularExpression::CaseInsensitiveOption);
        if (regex.match(fileName).hasMatch()) {
            return true;
        }
    }
    
    return false;
}

void LazyDirectoryLoader::processGitignore(const QString& path) {
    QString gitignorePath = path + "/.gitignore";
    
    if (!QFile::exists(gitignorePath)) {
        return;
    }
    
    QFile file(gitignorePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    gitignorePatterns_.clear();
    
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        
        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        
        // Convert gitignore pattern to regex
        QString pattern = line;
        
        // Escape special regex characters
        pattern = QRegularExpression::escape(pattern);
        
        // Convert gitignore wildcards to regex
        pattern.replace("*", ".*");
        pattern.replace("?", ".");
        
        // Pattern matches from start of string
        if (!pattern.startsWith('/')) {
            pattern = "^" + pattern;
        }
        
        gitignorePatterns_.append(pattern);
    }
    
    file.close();
    
    LOG_DEBUG("Gitignore patterns loaded", {{"path", gitignorePath}, {"patterns", gitignorePatterns_.size()}});
}

LazyDirectoryLoader::~LazyDirectoryLoader() {
    shutdown();
}

} // namespace RawrXD