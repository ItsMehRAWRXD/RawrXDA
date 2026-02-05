#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QTimer>
#include <QHash>

/**
 * @brief Backup and disaster recovery manager
 * 
 * Features:
 * - Automated model/config backups
 * - Point-in-time recovery
 * - Incremental backups
 * - Backup verification
 * - RTO (Recovery Time Objective): < 5 minutes
 * - RPO (Recovery Point Objective): < 15 minutes
 */
class BackupManager : public QObject {
    Q_OBJECT

public:
    enum BackupType {
        Full,
        Incremental,
        Differential
    };

    struct BackupInfo {
        QString id;
        BackupType type;
        QDateTime timestamp;
        QString path;
        size_t sizeBytes;
        bool verified;
        QString checksum;
    };

    static BackupManager& instance();
    ~BackupManager();

    /**
     * @brief Start automatic backup service
     * @param intervalMinutes Backup interval (default: 15 minutes for RPO)
     */
    void start(int intervalMinutes = 15);

    /**
     * @brief Stop backup service
     */
    void stop();

    /**
     * @brief Create manual backup
     */
    QString createBackup(BackupType type = Full);

    /**
     * @brief Restore from backup
     * @param backupId Backup ID to restore
     */
    bool restoreBackup(const QString& backupId);

    /**
     * @brief List available backups
     */
    QList<BackupInfo> listBackups() const;

    /**
     * @brief Verify backup integrity
     */
    bool verifyBackup(const QString& backupId);

    /**
     * @brief Delete old backups (retention policy)
     * @param daysToKeep Keep backups from last N days
     */
    void cleanOldBackups(int daysToKeep = 30);

    /**
     * @brief Set backup directory
     */
    void setBackupDirectory(const QString& path);

    /**
     * @brief Get backup directory
     */
    QString backupDirectory() const;

signals:
    void backupStarted(const QString& backupId);
    void backupCompleted(const QString& backupId, size_t sizeBytes);
    void backupFailed(const QString& error);
    void restoreStarted(const QString& backupId);
    void restoreCompleted(const QString& backupId);
    void restoreFailed(const QString& error);

private slots:
    void performAutomaticBackup();

private:
    BackupManager();  // Singleton
    BackupManager(const BackupManager&) = delete;
    BackupManager& operator=(const BackupManager&) = delete;

    QString calculateChecksum(const QString& filePath);
    bool compressBackup(const QString& srcPath, const QString& dstPath);
    bool decompressBackup(const QString& srcPath, const QString& dstPath);

    QString m_backupDirectory;
    QTimer* m_backupTimer = nullptr;
    QHash<QString, BackupInfo> m_backups;
    bool m_running = false;
};
