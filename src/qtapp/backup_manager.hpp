#pragma once


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
class BackupManager : public void {

public:
    enum BackupType {
        Full,
        Incremental,
        Differential
    };

    struct BackupInfo {
        std::string id;
        BackupType type;
        std::chrono::system_clock::time_point timestamp;
        std::string path;
        size_t sizeBytes;
        bool verified;
        std::string checksum;
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
    std::string createBackup(BackupType type = Full);

    /**
     * @brief Restore from backup
     * @param backupId Backup ID to restore
     */
    bool restoreBackup(const std::string& backupId);

    /**
     * @brief List available backups
     */
    std::vector<BackupInfo> listBackups() const;

    /**
     * @brief Verify backup integrity
     */
    bool verifyBackup(const std::string& backupId);

    /**
     * @brief Delete old backups (retention policy)
     * @param daysToKeep Keep backups from last N days
     */
    void cleanOldBackups(int daysToKeep = 30);

    /**
     * @brief Set backup directory
     */
    void setBackupDirectory(const std::string& path);

    /**
     * @brief Get backup directory
     */
    std::string backupDirectory() const;

    void backupStarted(const std::string& backupId);
    void backupCompleted(const std::string& backupId, size_t sizeBytes);
    void backupFailed(const std::string& error);
    void restoreStarted(const std::string& backupId);
    void restoreCompleted(const std::string& backupId);
    void restoreFailed(const std::string& error);

private:
    void performAutomaticBackup();

private:
    BackupManager();  // Singleton
    BackupManager(const BackupManager&) = delete;
    BackupManager& operator=(const BackupManager&) = delete;

    std::string calculateChecksum(const std::string& filePath);
    bool compressBackup(const std::string& srcPath, const std::string& dstPath);
    bool decompressBackup(const std::string& srcPath, const std::string& dstPath);

    std::string m_backupDirectory;
    void** m_backupTimer = nullptr;
    std::unordered_map<std::string, BackupInfo> m_backups;
    bool m_running = false;
};

