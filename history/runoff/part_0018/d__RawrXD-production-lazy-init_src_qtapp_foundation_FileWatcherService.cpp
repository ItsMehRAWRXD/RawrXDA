#include "FileWatcherService.h"

#include <QFileInfo>

FileWatcherService::FileWatcherService(QObject* parent) : QObject(parent) {
    m_timer.setInterval(75);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &FileWatcherService::flush);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &FileWatcherService::onFileChanged);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &FileWatcherService::onDirChanged);
}

FileWatcherService::~FileWatcherService() {}

bool FileWatcherService::watchFile(const QString& path) {
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) return false;
    if (!m_watcher.files().contains(path)) m_watcher.addPath(path);
    return true;
}

bool FileWatcherService::watchDirectory(const QString& path) {
    QFileInfo info(path);
    if (!info.exists() || !info.isDir()) return false;
    if (!m_watcher.directories().contains(path)) m_watcher.addPath(path);
    return true;
}

bool FileWatcherService::unwatch(const QString& path) {
    bool removed = false;
    if (m_watcher.files().contains(path)) {
        m_watcher.removePath(path);
        removed = true;
    }
    if (m_watcher.directories().contains(path)) {
        m_watcher.removePath(path);
        removed = true;
    }
    return removed;
}

QStringList FileWatcherService::watchedFiles() const {
    return m_watcher.files();
}

QStringList FileWatcherService::watchedDirectories() const {
    return m_watcher.directories();
}

void FileWatcherService::onFileChanged(const QString& path) {
    QMutexLocker locker(&m_mutex);
    m_pending.insert(path);
    m_timer.start();
}

void FileWatcherService::onDirChanged(const QString& path) {
    QMutexLocker locker(&m_mutex);
    m_pending.insert(path);
    m_timer.start();
}

void FileWatcherService::flush() {
    QStringList changed;
    {
        QMutexLocker locker(&m_mutex);
        changed = m_pending.values();
        m_pending.clear();
    }
    if (!changed.isEmpty()) emit filesChanged(changed);
}
