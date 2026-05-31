// ============================================================================
// extension_auto_updater.h — Extension Auto-Update Maintenance System
// ============================================================================
// PURPOSE:
//   Automated update checking, downloading, and installation for extensions.
//   Features:
//   - Background update checks on schedule
//   - Update notifications
//   - Automatic or manual installation
//   - Rollback capability
//   - Update history and logs
//   - Selective per-extension update policies
//
// Architecture: C++20 | Win32 | Background threads | No exceptions | Qt-free
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <cstdint>

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Update Policy Types
// ============================================================================

enum class UpdatePolicy {
    Manual,                 // Manual check and install only
    CheckOnly,              // Check for updates, notify user
    AutoInstall,            // Auto-install non-breaking updates
    AutoInstallAll,         // Auto-install all updates including breaking
};

// ============================================================================
// Update Information
// ============================================================================

struct ExtensionUpdateInfo {
    std::string extensionId;
    std::string currentVersion;
    std::string newVersion;
    std::string releaseNotes;
    std::string downloadUrl;
    size_t downloadSizeBytes = 0;
    bool isBreakingChange = false;
    bool isPrerelease = false;
    uint64_t releaseDateMs = 0;
};

struct UpdateCheckResult {
    bool success = false;
    std::string errorMessage;
    std::vector<ExtensionUpdateInfo> updatesAvailable;
    uint64_t checkTimeMs = 0;
};

// ============================================================================
// Update Installation Record
// ============================================================================

struct UpdateInstallRecord {
    std::string extensionId;
    std::string previousVersion;
    std::string newVersion;
    uint64_t installTimeMs = 0;
    bool success = false;
    std::string errorDetails;
    
    // For rollback
    std::string backupPath;              // Path to backed up previous version
};

// ============================================================================
// Extension Auto-Updater
// ============================================================================

class ExtensionAutoUpdater {
public:
    explicit ExtensionAutoUpdater();
    ~ExtensionAutoUpdater();

    // ── Configuration ──────────────────────────────────────────────

    // Set update check interval (in hours)
    void SetCheckInterval(uint32_t hours);
    uint32_t GetCheckInterval() const;

    // Set update policy
    void SetUpdatePolicy(UpdatePolicy policy);
    UpdatePolicy GetUpdatePolicy() const;

    // Set per-extension policy override
    void SetExtensionUpdatePolicy(const std::string& extensionId, UpdatePolicy policy);
    UpdatePolicy GetExtensionUpdatePolicy(const std::string& extensionId) const;

    // Enable/disable update notifications
    void SetNotificationsEnabled(bool enabled);
    bool AreNotificationsEnabled() const;

    // ── Manual Operations ──────────────────────────────────────────

    // Check for updates immediately
    UpdateCheckResult CheckForUpdates();

    // Check for updates for specific extensions
    UpdateCheckResult CheckForUpdatesExclusive(const std::vector<std::string>& extensionIds);

    // Install available update for extension
    bool InstallUpdate(const std::string& extensionId);

    // Rollback extension to previous version
    bool RollbackUpdate(const std::string& extensionId);

    // ── Automatic Operations ───────────────────────────────────────

    // Start background update scheduler
    bool StartAutoUpdateScheduler();

    // Stop background scheduler
    void StopAutoUpdateScheduler();

    // ── Status & History ──────────────────────────────────────────

    // Get pending updates
    std::vector<ExtensionUpdateInfo> GetPendingUpdates() const;

    // Get update installation history
    std::vector<UpdateInstallRecord> GetUpdateHistory(
        const std::string& extensionId = "",
        int maxRecords = 100
    ) const;

    // Get last check time
    uint64_t GetLastCheckTimeMs() const;

    // ── Callbacks ──────────────────────────────────────────────────

    // Register callback for update available
    void OnUpdateAvailable(
        std::function<void(const ExtensionUpdateInfo&)> callback
    );

    // Register callback for update installation
    void OnUpdateInstalled(
        std::function<void(const UpdateInstallRecord&)> callback
    );

    // ── Persistence ────────────────────────────────────────────────

    // Load update history from storage
    bool LoadUpdateHistory(const std::string& storagePath);

    // Save update history to storage
    bool SaveUpdateHistory(const std::string& storagePath);

    // Create backup of extension (for rollback)
    bool BackupExtension(const std::string& extensionId);

private:
    mutable std::mutex m_lock;

    UpdatePolicy m_defaultPolicy = UpdatePolicy::CheckOnly;
    std::unordered_map<std::string, UpdatePolicy> m_perExtensionPolicy;

    uint32_t m_checkIntervalHours = 24;  // Default: daily
    bool m_notificationsEnabled = true;

    // Scheduler state
    std::atomic<bool> m_schedulerRunning{false};
    std::thread m_schedulerThread;
    uint64_t m_lastCheckTimeMs = 0;

    // Tracking
    std::vector<ExtensionUpdateInfo> m_pendingUpdates;
    std::vector<UpdateInstallRecord> m_installHistory;
    std::unordered_map<std::string, std::string> m_backupPaths;  // extId -> backup dir path

    // Callbacks
    std::function<void(const ExtensionUpdateInfo&)> m_onUpdateAvailable;
    std::function<void(const UpdateInstallRecord&)> m_onUpdateInstalled;

    // Internal methods
    void RunSchedulerThread();
    bool PerformUpdateCheck();
    bool PerformAutoInstall();
    bool InstallUpdateInternal(const std::string& extensionId,
                               const ExtensionUpdateInfo& updateInfo);
    
    void NotifyUpdateAvailable(const ExtensionUpdateInfo& updateInfo);
    void NotifyUpdateInstalled(const UpdateInstallRecord& record);

    bool BackupExtensionInternal(const std::string& extensionId);
    bool RestoreExtensionFromBackup(const std::string& extensionId,
                                    const std::string& backupPath);
};

// ============================================================================
// Global Helper
// ============================================================================

// Get singleton updater instance
ExtensionAutoUpdater& GetAutoUpdater();

// Convenience functions
bool CheckExtensionUpdates(std::vector<ExtensionUpdateInfo>& outUpdates);
bool InstallExtensionUpdate(const std::string& extensionId);

}  // namespace Extensions
}  // namespace RawrXD

#endif  // EXTENSION_AUTO_UPDATER_H
