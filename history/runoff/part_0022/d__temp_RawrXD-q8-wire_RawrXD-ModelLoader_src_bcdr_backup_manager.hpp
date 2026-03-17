#pragma once
#include <QString>
#include <QDateTime>
#include <vector>
#include <cstdint>

/**
 * @class BackupManager
 * @brief Business Continuity and Disaster Recovery (BCDR)
 * 
 * Features:
 * - Automated backup scheduling
 * - Incremental and full backups
 * - Backup verification
 * - Point-in-time recovery
 * - Replication to secondary storage
 */
class BackupManager {
    
public:
    enum BackupType { Full, Incremental, Differential };
    enum BackupStatus { Pending, InProgress, Completed, Failed, Verified };
    
    struct BackupMetadata {
        QString backupId;
        BackupType type;
        QDateTime timestamp;
        QString sourceDirectory;
        QString targetLocation;
        uint64_t sizeBytes = 0;
        int fileCount = 0;
        QString checksumSHA256;
        BackupStatus status = Pending;
        QString errorMessage;
        QDateTime verifiedAt;
        bool verified = false;
    };
    
    struct RecoveryPoint {
        QString backupId;
        QDateTime timestamp;
        QString description;
        uint64_t sizeBytes;
        bool verified;
    };
    
    BackupManager();
    ~BackupManager();
    
    // Backup operations
    /**
     * @brief Start full backup
     * @param sourceDir Directory to backup
     * @param targetLocation Backup destination path
     * @return backupId for tracking
     */
    QString startFullBackup(const QString& sourceDir, const QString& targetLocation);
    
    /**
     * @brief Start incremental backup (since last full backup)
     */
    QString startIncrementalBackup(const QString& sourceDir, const QString& targetLocation);
    
    /**
     * @brief Verify backup integrity
     * @return true if backup is valid
     */
    bool verifyBackup(const QString& backupId);
    
    /**
     * @brief Get all recovery points
     */
    std::vector<RecoveryPoint> getRecoveryPoints() const;
    
    // Recovery operations
    /**
     * @brief Recover from specific backup
     * @param backupId Backup to recover from
     * @param targetDir Directory to restore to
     * @return true if recovery successful
     */
    bool recoverFromBackup(const QString& backupId, const QString& targetDir);
    
    /**
     * @brief Perform point-in-time recovery
     * @param pointInTime Timestamp to recover to
     */
    bool recoverToPointInTime(const QDateTime& pointInTime, const QString& targetDir);
    
    // Replication
    /**
     * @brief Setup backup replication to secondary storage
     */
    void setupReplication(const QString& secondaryStoragePath, int replicationDelayMs = 5000);
    
    /**
     * @brief Check replication status
     */
    bool isReplicationHealthy() const;
    
    // Configuration
    /**
     * @brief Enable automatic backup scheduling
     */
    void enableAutoBackup(int intervalMinutes, const QString& backupDir);
    
    /**
     * @brief Disable automatic backup
     */
    void disableAutoBackup();
    
    /**
     * @brief Set retention policy (days to keep backups)
     */
    void setRetentionPolicy(int daysToKeep);
    
    // Status
    /**
     * @brief Get backup status
     */
    BackupMetadata getBackupStatus(const QString& backupId) const;
    
    /**
     * @brief Get latest backup
     */
    QString getLatestBackupId() const;
    
    /**
     * @brief Estimate recovery time objective (RTO)
     * @return milliseconds
     */
    qint64 estimateRTO() const;
    
    /**
     * @brief Estimate recovery point objective (RPO)
     * @return milliseconds
     */
    qint64 estimateRPO() const;
    
private:
    struct BackupJob {
        BackupMetadata metadata;
        std::vector<QString> files;
        qint64 startTime = 0;
        qint64 endTime = 0;
    };
    
    std::vector<BackupJob> m_backups;
    QString m_latestBackupId;
    bool m_autoBackupEnabled = false;
    int m_retentionDays = 30;
    QString m_replicationPath;
    bool m_replicationHealthy = true;
    
    QString generateBackupId();
    bool performFullBackup(const BackupJob& job);
    bool performIncrementalBackup(const BackupJob& job);
    bool verifyChecksums(const BackupJob& job);
    void cleanupOldBackups();
};
