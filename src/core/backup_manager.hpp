#pragma once
// ============================================================================
// Backup Manager — Automated Backup/Restore System (Phase 33 Quick-Win Port)
// Pure Win32 — replaces Qt backup_manager with CopyFile/CreateFile APIs
// Features:
//   - Timed auto-backup (RPO < 15 min)
//   - Full and incremental backup modes
//   - Checksum verification (CRC32)
//   - Retention policy (max backups, max age)
//   - Restore from backup
//   - Backup manifest (JSON index)
//   - GDPR-aware: purge old backups automatically
// ============================================================================

#ifndef RAWRXD_BACKUP_MANAGER_HPP
#define RAWRXD_BACKUP_MANAGER_HPP

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>
#include <functional>

// ============================================================================
// Backup Type
// ============================================================================
enum class BackupType : int {
    Full         = 0,  // Complete copy of all tracked files
    Incremental  = 1,  // Only files changed since last backup
    Differential = 2   // Files changed since last full backup
};

// ============================================================================
// Backup Entry (one backup snapshot)
// ============================================================================
struct BackupEntry {
    std::string id;            // UUID-style ID
    std::string timestamp;     // ISO 8601
    BackupType type;
    std::string backupDir;     // Directory containing this backup
    size_t fileCount;
    size_t totalBytes;
    uint32_t checksum;         // CRC32 of manifest
    bool verified;             // Post-backup integrity check passed
    std::string description;
};

// ============================================================================
// Backup Configuration
// ============================================================================
struct BackupConfig {
    std::string backupRoot = "./backups";     // Root directory for all backups
    std::string projectRoot = ".";            // Project root to back up
    int autoBackupIntervalMs = 900000;        // 15 minutes
    bool enableAutoBackup = true;
    int maxBackups = 50;                       // Retention: max backups to keep
    int maxAgeDays = 30;                       // Retention: max age in days
    bool enableChecksums = true;
    std::vector<std::string> includePatterns;  // e.g. {"*.cpp", "*.h", "*.hpp", "*.asm"}
    std::vector<std::string> excludePatterns;  // e.g. {"build/*", ".git/*", "*.obj"}
};

// ============================================================================
// Backup Result
// ============================================================================
struct BackupResult {
    bool success;
    const char* detail;
    BackupEntry entry;

    static BackupResult ok(const char* msg) {
        return { true, msg, {} };
    }
    static BackupResult error(const char* msg) {
        return { false, msg, {} };
    }
};

// ============================================================================
// Backup event callback
// ============================================================================
using BackupEventCallback = void(*)(const char* event, const BackupEntry* entry, void* userData);

// ============================================================================
// BackupManager class
// ============================================================================
class BackupManager {
public:
    BackupManager();
    ~BackupManager();

    // ---- Configuration ----
    void configure(const BackupConfig& config);
    BackupConfig getConfig() const;

    // ---- Manual Operations ----
    BackupResult createBackup(BackupType type = BackupType::Incremental,
                               const std::string& description = "");
    BackupResult restoreBackup(const std::string& backupId);
    BackupResult verifyBackup(const std::string& backupId);
    BackupResult deleteBackup(const std::string& backupId);

    // ---- Auto-Backup ----
    BackupResult startAutoBackup();
    BackupResult stopAutoBackup();
    bool isAutoBackupRunning() const;

    // ---- Query ----
    std::vector<BackupEntry> listBackups() const;
    BackupEntry getLatestBackup() const;
    size_t getBackupCount() const;
    size_t getTotalBackupSize() const;

    // ---- Retention ----
    int pruneOldBackups();

    // ---- Callbacks ----
    void setEventCallback(BackupEventCallback cb, void* userData);

private:
    BackupConfig m_config;
    mutable std::mutex m_configMutex;

    std::vector<BackupEntry> m_backups;
    mutable std::mutex m_backupsMutex;

    // Auto-backup thread
    std::atomic<bool> m_autoBackupRunning{false};
    std::atomic<bool> m_stopRequested{false};
    std::thread m_autoBackupThread;

    // Callbacks
    BackupEventCallback m_eventCb = nullptr;
    void* m_eventCbUserData = nullptr;

    // Helpers
    std::string generateBackupId() const;
    std::string getTimestamp() const;
    uint32_t computeCRC32(const std::string& filepath) const;
    bool ensureDirectory(const std::string& path) const;
    bool matchesPattern(const std::string& filename,
                         const std::vector<std::string>& patterns) const;
    std::vector<std::string> collectFiles() const;
    void autoBackupLoop();
    void loadManifest();
    void saveManifest() const;
    void emitEvent(const char* event, const BackupEntry* entry = nullptr);
};

#endif // RAWRXD_BACKUP_MANAGER_HPP
