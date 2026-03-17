#pragma once

#include <QString>
#include <QStringList>
#include <QObject>
#include <QFileSystemWatcher>
#include <memory>


/**
 * @file file_system_manager.h
 * @brief Centralized file I/O system for RawrXD IDE
 *
 * Provides:
 * - File reading/writing with encoding detection
 * - File watching for external changes
 * - Recent files tracking
 * - File metadata caching
 */

class FileSystemManager : public QObject {
    Q_OBJECT

public:
    explicit FileSystemManager(QObject* parent = nullptr);
    ~FileSystemManager();

    // Singleton instance
    static FileSystemManager& instance();

    // File operations
    QString readFile(const QString& filePath);
    bool writeFile(const QString& filePath, const QString& content);
    bool fileExists(const QString& filePath);
    QStringList getRecentFiles() const;
    void addToRecentFiles(const QString& filePath);

    // File watching
    void watchFile(const QString& filePath);
    void unwatchFile(const QString& filePath);
    bool isWatched(const QString& filePath) const;

    // Encoding detection
    QString detectEncoding(const QString& filePath);
    QStringList getSupportedEncodings() const;

signals:
    void fileChanged(const QString& filePath);
    void fileAdded(const QString& filePath);
    void fileRemoved(const QString& filePath);
    void errorOccurred(const QString& errorMessage);
    void fileChangedExternally(const QString& filePath);
    void fileOperationComplete(const QString& filePath);
    void fileRead(const QString& filePath);

private slots:
    void onFileChanged(const QString& filePath);
    void onDirectoryChanged(const QString& directoryPath);

private:
    void initialize();
    void loadRecentFiles();
    void saveRecentFiles();
    QString getRecentFilesPath() const;

    // Member variables
    std::unique_ptr<QFileSystemWatcher> m_fileWatcher;
    QStringList m_recentFiles;
    QStringList m_watchedFiles;
    QMap<QString, QString> m_fileEncodings;
    QString m_configPath;
    int m_maxRecentFiles = 20;
};

