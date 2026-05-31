// ============================================================================
// ExtensionAutoUpdateManager.hpp — Auto-Update Mechanism for Extensions
// ============================================================================
//
// Phase 29D: Extension Auto-Update System
//
// Purpose:
//   Provides comprehensive auto-update functionality for installed extensions.
//   Handles version checking, staged rollouts, and update management.
//   Ensures safe and reliable extension updates with rollback capabilities.
//
// Features:
//   - Automatic Version Checking & Discovery
//   - Staged Rollout Support (canary, beta, stable)
//   - Background Update Downloads
//   - Transactional Updates with Rollback
//   - Update Scheduling & Rate Limiting
//   - User Preference Management
//   - Dependency-Aware Updates
//   - Security Update Prioritization
//
// Design:
//   - Non-blocking background operations
//   - Configurable update policies
//   - Crash-safe update handling
//   - Integration with marketplace backend
//   - Comprehensive update logging
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include "ExtensionPermissions.hpp"
#include "MarketplaceBackend.hpp"
#include "ExtensionDependencyResolver.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <chrono>
#include <filesystem>

// ============================================================================
// Update Policy & Configuration
// ============================================================================

enum class UpdateChannel {
    Stable,         // Only stable releases
    Beta,           // Beta and stable releases
    Canary,         // All releases including canary
    Manual,         // No automatic updates
    Security        // Only security updates
};

enum class UpdateTrigger {
    Startup,        // Check on IDE startup
    Scheduled,      // Scheduled background checks
    Manual,         // User-initiated
    Force          // Force update regardless of policy
};

enum class UpdatePriority {
    Low,           // Non-critical updates
    Normal,        // Regular feature updates
    High,          // Important bug fixes
    Security,      // Security patches
    Critical       // Critical fixes (immediate)
};

struct UpdatePolicy {
    UpdateChannel channel;                           // Update channel preference
    bool autoUpdateEnabled;                          // Enable automatic updates
    bool autoInstallSecurity;                       // Auto-install security updates
    bool requireUserConfirmation;                   // Require user approval
    std::chrono::hours checkInterval;               // How often to check for updates
    std::chrono::hours installDelay;                // Delay before auto-install
    uint32_t maxConcurrentUpdates;                  // Limit concurrent updates
    bool updateDuringActiveUse;                     // Allow updates while IDE is active
    bool rollbackOnFailure;                         // Auto-rollback failed updates
    std::vector<std::string> excludedExtensions;    // Extensions to never auto-update
    std::vector<std::string> pinnedVersions;        // Extensions pinned to specific versions
    
    static UpdatePolicy getDefault() {
        UpdatePolicy policy;
        policy.channel = UpdateChannel::Stable;
        policy.autoUpdateEnabled = true;
        policy.autoInstallSecurity = true;
        policy.requireUserConfirmation = false;
        policy.checkInterval = std::chrono::hours(24);
        policy.installDelay = std::chrono::hours(1);
        policy.maxConcurrentUpdates = 3;
        policy.updateDuringActiveUse = false;
        policy.rollbackOnFailure = true;
        return policy;
    }
    
    static UpdatePolicy getConservative() {
        UpdatePolicy policy = getDefault();
        policy.channel = UpdateChannel::Stable;
        policy.requireUserConfirmation = true;
        policy.checkInterval = std::chrono::hours(72);
        policy.installDelay = std::chrono::hours(24);
        policy.updateDuringActiveUse = false;
        return policy;
    }
};

// ============================================================================
// Update Information Structures
// ============================================================================

enum class UpdateStatus {
    NoUpdate,           // No update available
    Available,          // Update available but not downloaded
    Downloaded,         // Update downloaded, ready to install
    Installing,         // Currently installing
    Completed,          // Update completed successfully
    Failed,             // Update failed
    RolledBack,         // Update was rolled back
    Cancelled,          // Update was cancelled by user
    Pending            // Waiting for user confirmation
};

struct UpdateInfo {
    std::string extensionId;
    SemanticVersion currentVersion;
    SemanticVersion availableVersion;
    UpdateChannel channel;
    UpdatePriority priority;
    UpdateStatus status;
    std::string releaseNotes;
    std::string changelog;
    std::chrono::system_clock::time_point releaseDate;
    std::chrono::system_clock::time_point discoveredAt;
    std::chrono::system_clock::time_point scheduledAt;
    uint64_t downloadSize;
    std::string downloadUrl;
    std::string downloadPath;
    std::vector<std::string> dependencies;
    std::vector<std::string> breakingChanges;
    bool isSecurityUpdate;
    bool requiresRestart;
    std::string errorMessage;
    uint32_t retryCount;
    
    UpdateInfo() : priority(UpdatePriority::Normal), status(UpdateStatus::NoUpdate), 
                  downloadSize(0), isSecurityUpdate(false), requiresRestart(false), retryCount(0) {}
};

struct UpdateBatch {
    std::string batchId;
    std::vector<UpdateInfo> updates;
    UpdateTrigger trigger;
    std::chrono::system_clock::time_point scheduledTime;
    bool requiresUserApproval;
    bool containsSecurityUpdates;
    bool requiresRestart;
    ResolutionPlan dependencyPlan;
    
    UpdateBatch() : trigger(UpdateTrigger::Manual), requiresUserApproval(false), 
                   containsSecurityUpdates(false), requiresRestart(false) {}
};

// ============================================================================
// Update Transaction & Rollback
// ============================================================================

struct UpdateTransaction {
    std::string transactionId;
    std::vector<UpdateInfo> updates;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    bool isCommitted;
    bool canRollback;
    std::vector<std::filesystem::path> backupPaths;
    std::vector<std::string> completedUpdates;
    std::string failureReason;
    
    UpdateTransaction() : isCommitted(false), canRollback(true) {}
};

// ============================================================================
// Update Statistics & Telemetry
// ============================================================================

struct UpdateStatistics {
    uint32_t totalUpdatesChecked;
    uint32_t updatesAvailable;
    uint32_t updatesInstalled;
    uint32_t updatesFailed;
    uint32_t updatesRolledBack;
    uint32_t securityUpdatesInstalled;
    std::chrono::system_clock::time_point lastCheckTime;
    std::chrono::system_clock::time_point lastUpdateTime;
    std::chrono::milliseconds averageCheckTime;
    std::chrono::milliseconds averageInstallTime;
    std::unordered_map<std::string, uint32_t> failuresByExtension;
    std::unordered_map<UpdateChannel, uint32_t> updatesByChannel;
    
    UpdateStatistics() : totalUpdatesChecked(0), updatesAvailable(0), updatesInstalled(0),
                        updatesFailed(0), updatesRolledBack(0), securityUpdatesInstalled(0) {}
};

// ============================================================================
// Main Auto-Update Manager
// ============================================================================

class ExtensionAutoUpdateManager {
public:
    ExtensionAutoUpdateManager(const UpdatePolicy& policy = UpdatePolicy::getDefault());
    ~ExtensionAutoUpdateManager();
    
    // Lifecycle management
    bool initialize();
    void shutdown();
    bool isRunning() const { return m_running.load(); }
    
    // Update discovery & checking
    std::vector<UpdateInfo> checkForUpdates(UpdateTrigger trigger = UpdateTrigger::Manual);
    std::future<std::vector<UpdateInfo>> checkForUpdatesAsync(UpdateTrigger trigger = UpdateTrigger::Manual);
    UpdateInfo checkExtensionUpdate(const std::string& extensionId);
    bool hasUpdatesAvailable() const;
    
    // Update management
    bool downloadUpdate(const std::string& extensionId);
    bool installUpdate(const std::string& extensionId, bool force = false);
    bool installUpdates(const std::vector<std::string>& extensionIds, bool force = false);
    bool installAllUpdates(bool force = false);
    
    // Batch operations
    UpdateBatch createUpdateBatch(const std::vector<std::string>& extensionIds, UpdateTrigger trigger);
    bool executeUpdateBatch(const UpdateBatch& batch);
    std::vector<UpdateBatch> getPendingBatches() const;
    bool cancelUpdateBatch(const std::string& batchId);
    
    // Transaction management
    UpdateTransaction beginTransaction(const std::vector<UpdateInfo>& updates);
    bool commitTransaction(const std::string& transactionId);
    bool rollbackTransaction(const std::string& transactionId);
    std::vector<UpdateTransaction> getActiveTransactions() const;
    
    // Rollback & recovery
    bool rollbackExtension(const std::string& extensionId);
    bool rollbackToVersion(const std::string& extensionId, const SemanticVersion& version);
    std::vector<std::string> getBackupVersions(const std::string& extensionId) const;
    bool cleanupBackups(uint32_t keepCount = 3);
    
    // Policy & configuration
    void setUpdatePolicy(const UpdatePolicy& policy);
    UpdatePolicy getUpdatePolicy() const;
    void setExtensionUpdateEnabled(const std::string& extensionId, bool enabled);
    void pinExtensionVersion(const std::string& extensionId, const SemanticVersion& version);
    void unpinExtensionVersion(const std::string& extensionId);
    
    // Scheduling & automation
    void enableAutoUpdates(bool enabled);
    void scheduleUpdateCheck(std::chrono::system_clock::time_point when);
    void scheduleUpdate(const std::string& extensionId, std::chrono::system_clock::time_point when);
    std::chrono::system_clock::time_point getNextScheduledCheck() const;
    
    // Status & monitoring
    std::vector<UpdateInfo> getAvailableUpdates() const;
    std::vector<UpdateInfo> getPendingUpdates() const;
    UpdateInfo getUpdateStatus(const std::string& extensionId) const;
    UpdateStatistics getStatistics() const;
    
    // User interaction callbacks
    void setUpdateAvailableCallback(std::function<void(const UpdateInfo&)> callback);
    void setUserConfirmationCallback(std::function<bool(const UpdateBatch&)> callback);
    void setProgressCallback(std::function<void(const std::string&, int, int)> callback);
    void setUpdateCompletedCallback(std::function<void(const UpdateInfo&, bool)> callback);
    void setErrorCallback(std::function<void(const std::string&, const std::string&)> callback);
    
    // Integration points
    void setMarketplaceBackend(std::shared_ptr<MarketplaceBackend> backend);
    void setDependencyResolver(std::shared_ptr<ExtensionDependencyResolver> resolver);
    void setPermissionManager(std::shared_ptr<ExtensionPermissionManager> permissions);
    
    // Utilities & debugging
    void pauseUpdates();
    void resumeUpdates();
    bool isPaused() const { return m_paused.load(); }
    std::vector<std::string> getUpdateLog(const std::string& extensionId = "") const;
    void exportUpdateReport(const std::string& filePath) const;
    
private:
    UpdatePolicy m_policy;
    std::shared_ptr<MarketplaceBackend> m_marketplace;
    std::shared_ptr<ExtensionDependencyResolver> m_dependencyResolver;
    std::shared_ptr<ExtensionPermissionManager> m_permissions;
    
    // State management
    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
    mutable std::mutex m_stateMutex;
    
    // Update tracking
    std::unordered_map<std::string, UpdateInfo> m_availableUpdates;
    std::unordered_map<std::string, UpdateInfo> m_pendingUpdates;
    std::vector<UpdateBatch> m_pendingBatches;
    std::unordered_map<std::string, UpdateTransaction> m_activeTransactions;
    mutable std::mutex m_updatesMutex;
    
    // Background workers
    std::thread m_checkWorker;
    std::thread m_downloadWorker;
    std::thread m_installWorker;
    std::queue<std::string> m_downloadQueue;
    std::queue<std::string> m_installQueue;
    std::condition_variable m_downloadCondition;
    std::condition_variable m_installCondition;
    std::mutex m_queueMutex;
    
    // Scheduling
    std::chrono::system_clock::time_point m_nextCheckTime;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> m_scheduledUpdates;
    mutable std::mutex m_scheduleMutex;
    
    // Statistics & logging
    UpdateStatistics m_statistics;
    std::vector<std::string> m_updateLog;
    mutable std::mutex m_logMutex;
    
    // Callbacks
    std::function<void(const UpdateInfo&)> m_updateAvailableCallback;
    std::function<bool(const UpdateBatch&)> m_userConfirmationCallback;
    std::function<void(const std::string&, int, int)> m_progressCallback;
    std::function<void(const UpdateInfo&, bool)> m_updateCompletedCallback;
    std::function<void(const std::string&, const std::string&)> m_errorCallback;
    
    // Configuration paths
    std::filesystem::path m_configPath;
    std::filesystem::path m_backupPath;
    std::filesystem::path m_logPath;
    
    // Worker methods
    void checkWorkerMain();
    void downloadWorkerMain();
    void installWorkerMain();
    
    // Core update operations
    std::vector<UpdateInfo> performUpdateCheck(UpdateTrigger trigger);
    UpdateInfo fetchExtensionUpdate(const std::string& extensionId);
    bool downloadExtensionUpdate(UpdateInfo& update);
    bool installExtensionUpdate(UpdateInfo& update);
    
    // Transaction operations
    std::string generateTransactionId() const;
    bool createBackup(const std::string& extensionId, const std::filesystem::path& backupPath);
    bool restoreBackup(const std::string& extensionId, const std::filesystem::path& backupPath);
    
    // Policy enforcement
    bool shouldCheckForUpdates() const;
    bool shouldAutoInstall(const UpdateInfo& update) const;
    bool isExtensionExcluded(const std::string& extensionId) const;
    bool isVersionPinned(const std::string& extensionId, SemanticVersion& pinnedVersion) const;
    
    // Utilities
    void recordStatistics(const UpdateInfo& update, bool success);
    void logUpdateEvent(const std::string& event, const std::string& details);
    void notifyUpdateAvailable(const UpdateInfo& update);
    void notifyUpdateCompleted(const UpdateInfo& update, bool success);
    void notifyError(const std::string& operation, const std::string& error);
    
    // Persistence
    void loadConfiguration();
    void saveConfiguration() const;
    void loadStatistics();
    void saveStatistics() const;
};

// ============================================================================
// Global Auto-Update Manager
// ============================================================================

// Singleton instance for global access
ExtensionAutoUpdateManager& GetAutoUpdateManager();

// Initialize auto-update manager with policy
bool InitializeAutoUpdateManager(const UpdatePolicy& policy = UpdatePolicy::getDefault());

// Shutdown auto-update manager
void ShutdownAutoUpdateManager();

// Utility functions
std::string updateChannelToString(UpdateChannel channel);
UpdateChannel stringToUpdateChannel(const std::string& str);
std::string updatePriorityToString(UpdatePriority priority);
UpdatePriority stringToUpdatePriority(const std::string& str);
std::string updateStatusToString(UpdateStatus status);
UpdateStatus stringToUpdateStatus(const std::string& str);
