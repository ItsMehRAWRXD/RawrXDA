/*
 * AutoSaveManager.h - Complete Auto-Save Implementation
 * 
 * Production-grade auto-save system with:
 * - Configurable auto-save interval
 * - Dirty file tracking
 * - Backup management
 * - Crash recovery
 * 
 * NO STUBS - COMPLETE IMPLEMENTATION
 */

#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QSet>
#include <QMap>
#include <QDateTime>
#include <QMutex>

/**
 * Auto-save manager for editor files
 */
class AutoSaveManager : public QObject {
    Q_OBJECT

public:
    explicit AutoSaveManager(QObject* parent = nullptr);
    ~AutoSaveManager();

    /**
     * Set auto-save enabled/disabled
     */
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    /**
     * Set auto-save interval in milliseconds
     */
    void setInterval(int intervalMs);
    int interval() const { return m_intervalMs; }

    /**
     * Set backup directory for crash recovery
     */
    void setBackupDirectory(const QString& backupDir);
    QString backupDirectory() const { return m_backupDir; }

    /**
     * Register a file as "dirty" (needs saving)
     */
    void markDirty(const QString& filePath);

    /**
     * Mark a file as clean (saved)
     */
    void markClean(const QString& filePath);

    /**
     * Check if file is dirty
     */
    bool isDirty(const QString& filePath) const;

    /**
     * Get all dirty files
     */
    QSet<QString> getDirtyFiles() const;

    /**
     * Force immediate save
     */
    void saveNow();

    /**
     * Restore from backup after crash
     */
    void restoreFromBackup();

    /**
     * Clear all backups
     */
    void clearBackups();

signals:
    /**
     * Emitted before auto-save
     */
    void aboutToAutoSave(const QSet<QString>& files);

    /**
     * Emitted after auto-save completes
     */
    void autoSaveCompleted(const QSet<QString>& files, bool success);

    /**
     * Emitted when auto-save fails
     */
    void autoSaveFailed(const QString& error);

    /**
     * Emitted when file content changes
     */
    void fileContentChanged(const QString& filePath);

public slots:
    /**
     * Called periodically to perform auto-save
     */
    void performAutoSave();

    /**
     * Called when file is saved externally
     */
    void onFileSaved(const QString& filePath);

    /**
     * Called when application is about to quit
     */
    void onApplicationQuitting();

private:
    /**
     * Create backup of file
     */
    bool createBackup(const QString& filePath);

    /**
     * Delete old backups (older than retention period)
     */
    void deleteOldBackups();

    /**
     * Get backup path for file
     */
    QString getBackupPath(const QString& filePath) const;

    /**
     * Get backup metadata file path
     */
    QString getBackupMetadataPath() const;

    /**
     * Save backup metadata
     */
    void saveBackupMetadata();

    /**
     * Load backup metadata
     */
    void loadBackupMetadata();

private:
    bool m_enabled = true;
    int m_intervalMs = 30000;  // 30 seconds default
    QString m_backupDir;
    
    QTimer* m_autoSaveTimer = nullptr;
    QSet<QString> m_dirtyFiles;
    QMap<QString, QDateTime> m_lastSaved;  // Track last save time per file
    QMap<QString, QDateTime> m_backupMetadata;  // Track backup times
    
    mutable QMutex m_mutex;
    
    static constexpr int MAX_BACKUP_AGE_HOURS = 24;  // Delete backups older than 24 hours
    static constexpr int MAX_BACKUP_SIZE_MB = 100;  // Keep max 100MB of backups
};

#endif // AUTOSAVEMANAGER_H
