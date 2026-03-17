#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QMutex>
#include <QSet>

// FileWatcherService wraps QFileSystemWatcher with debounce and batch notifications.
class FileWatcherService : public QObject {
    Q_OBJECT
public:
    explicit FileWatcherService(QObject* parent = nullptr);
    ~FileWatcherService();

    bool watchFile(const QString& path);
    bool watchDirectory(const QString& path);
    bool unwatch(const QString& path);
    QStringList watchedFiles() const;
    QStringList watchedDirectories() const;

signals:
    void filesChanged(const QStringList& paths);

private slots:
    void onFileChanged(const QString& path);
    void onDirChanged(const QString& path);
    void flush();

private:
    QFileSystemWatcher m_watcher;
    QTimer m_timer;
    QSet<QString> m_pending;
    mutable QMutex m_mutex;
};
