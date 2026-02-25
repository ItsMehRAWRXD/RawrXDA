#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <filesystem>

class BackupManager {
public:
    enum class BackupType {
        Auto,
        Manual,
        PreBuild,
        PreRefactor
    };

    struct BackupInfo {
        std::string id;
        std::string timestamp;
        BackupType type;
        std::string path;
        size_t sizeBytes;
    };

    static BackupManager& instance();
    ~BackupManager();

    // Configuration
    void setBackupDirectory(const std::string& path);
    std::string backupDirectory() const;
    void start(int intervalMinutes = 30);
    void stop();

    // Operations
    std::string createBackup(BackupType type = BackupType::Manual, const std::string& targetPath = "");
    bool restoreBackup(const std::string& backupIdOrPath);
    bool verifyBackup(const std::string& backupIdOrPath);
    
    // Management
    std::vector<BackupInfo> listBackups() const;
    void cleanOldBackups(int daysToKeep);

private:
    BackupManager();
    BackupManager(const BackupManager&) = delete;
    BackupManager& operator=(const BackupManager&) = delete;

    std::string generateBackupId() const;
    std::string getTimestampString() const;
    void autoBackupLoop();

    std::string m_backupDirectory;
    bool m_running;
    int m_intervalMinutes;
    mutable std::mutex m_mutex;
    // std::thread m_thread; // Logic implemented but thread management handled by caller or simple loop
};
