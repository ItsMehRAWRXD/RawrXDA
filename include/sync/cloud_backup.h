#pragma once
/**
 * @file cloud_backup.h
 * @brief Cloud backup and restore functionality
 * Batch 5 - Item 67: Cloud backup
 */

#include <string>
#include <vector>
#include <functional>
#include <future>
#include <chrono>

namespace RawrXD::Sync {

enum class BackupStatus {
    Idle,
    BackingUp,
    Restoring,
    Completed,
    Error
};

struct BackupItem {
    std::string id;
    std::string name;
    std::chrono::system_clock::time_point timestamp;
    size_t size;
    std::string description;
    bool isAutomatic;
};

struct BackupProgress {
    BackupStatus status;
    std::string currentFile;
    int totalFiles;
    int processedFiles;
    size_t totalBytes;
    size_t processedBytes;
    std::string error;
};

class CloudBackup {
public:
    CloudBackup();
    ~CloudBackup();

    // Initialization
    bool initialize();
    void shutdown();

    // Backup
    bool createBackup(const std::string& name, const std::string& description = "");
    std::future<bool> createBackupAsync(const std::string& name, const std::string& description = "");
    bool createAutomaticBackup();

    // Restore
    bool restoreBackup(const std::string& backupId);
    std::future<bool> restoreBackupAsync(const std::string& backupId);
    bool restoreBackup(const std::string& backupId, const std::string& targetPath);

    // Management
    std::vector<BackupItem> listBackups();
    bool deleteBackup(const std::string& backupId);
    bool deleteAllBackups();
    std::optional<BackupItem> getBackup(const std::string& backupId);

    // Configuration
    void setBackupLocation(const std::string& path);
    std::string getBackupLocation() const;
    void setMaxBackups(int count);
    int getMaxBackups() const;
    void setAutomaticBackup(bool enabled);
    bool isAutomaticBackupEnabled() const;
    void setAutomaticBackupInterval(int hours);
    int getAutomaticBackupInterval() const;

    // Items to backup
    void addBackupItem(const std::string& path);
    void removeBackupItem(const std::string& path);
    std::vector<std::string> getBackupItems() const;
    void setDefaultBackupItems();

    // Progress
    BackupProgress getProgress() const;
    void cancelOperation();
    bool isOperationInProgress() const;

    // Export/Import
    bool exportBackup(const std::string& backupId, const std::string& exportPath);
    bool importBackup(const std::string& importPath);

    // Events
    using ProgressCallback = std::function<void(const BackupProgress&)>;
    using CompleteCallback = std::function<void(bool success, const std::string& message)>;
    void onProgress(ProgressCallback callback);
    void onComplete(CompleteCallback callback);

private:
    std::string m_backupLocation;
    int m_maxBackups{10};
    bool m_automaticBackup{true};
    int m_automaticBackupInterval{24};
    std::vector<std::string> m_backupItems;
    BackupProgress m_progress;
    bool m_operationInProgress{false};
    std::atomic<bool> m_cancelled{false};

    ProgressCallback m_progressCallback;
    CompleteCallback m_completeCallback;

    std::thread m_backupThread;

    void backupLoop();
    bool performBackup(const std::string& name, const std::string& description);
    bool performRestore(const std::string& backupId, const std::string& targetPath);
    void cleanupOldBackups();
    std::string generateBackupId();
    void notifyProgress();
    void notifyComplete(bool success, const std::string& message);
};

// Global instance
CloudBackup& getCloudBackup();

} // namespace RawrXD::Sync
