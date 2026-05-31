#pragma once
/**
 * @file settings_sync.h
 * @brief Cross-device settings synchronization
 * Batch 5 - Item 66: Settings sync
 */

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <future>

namespace RawrXD::Sync {

enum class SyncStatus {
    Disabled,
    Syncing,
    Synced,
    Error,
    Conflict
};

struct SyncItem {
    std::string key;
    std::string value;
    std::chrono::system_clock::time_point timestamp;
    std::string deviceId;
    uint64_t version;
};

struct SyncConflict {
    std::string key;
    SyncItem local;
    SyncItem remote;
};

struct DeviceInfo {
    std::string id;
    std::string name;
    std::string type;
    std::chrono::system_clock::time_point lastSeen;
    bool isCurrent;
};

class SettingsSync {
public:
    SettingsSync();
    ~SettingsSync();

    // Initialization
    bool initialize(const std::string& deviceId);
    void shutdown();

    // Authentication
    bool signIn(const std::string& token);
    void signOut();
    bool isSignedIn() const;
    std::string getUserId() const;

    // Sync control
    void enableSync();
    void disableSync();
    bool isEnabled() const;
    SyncStatus getStatus() const;

    // Manual sync
    bool syncNow();
    std::future<bool> syncNowAsync();
    void forceUpload();
    void forceDownload();

    // Settings
    void setSetting(const std::string& key, const std::string& value);
    std::optional<std::string> getSetting(const std::string& key);
    void deleteSetting(const std::string& key);
    std::vector<std::string> getKeys();

    // Conflicts
    std::vector<SyncConflict> getConflicts();
    void resolveConflict(const std::string& key, bool useLocal);
    void resolveAllConflicts(bool useLocal);

    // Devices
    std::vector<DeviceInfo> getDevices();
    void renameDevice(const std::string& deviceId, const std::string& name);
    void removeDevice(const std::string& deviceId);

    // Sync configuration
    void setSyncInterval(int minutes);
    int getSyncInterval() const;
    void setSyncOnStartup(bool sync);
    bool getSyncOnStartup() const;

    // Selective sync
    void includeKey(const std::string& key);
    void excludeKey(const std::string& key);
    void includePattern(const std::string& pattern);
    void excludePattern(const std::string& pattern);
    bool shouldSync(const std::string& key) const;

    // Events
    using SyncCallback = std::function<void(SyncStatus status)>;
    using ConflictCallback = std::function<void(const SyncConflict&)>;
    using DeviceCallback = std::function<void(const DeviceInfo&)>;
    void onSyncStatusChanged(SyncCallback callback);
    void onConflictDetected(ConflictCallback callback);
    void onDeviceChanged(DeviceCallback callback);

private:
    std::string m_deviceId;
    std::string m_userId;
    bool m_signedIn{false};
    bool m_enabled{false};
    SyncStatus m_status{SyncStatus::Disabled};
    int m_syncInterval{15};
    bool m_syncOnStartup{true};
    std::map<std::string, SyncItem> m_settings;
    std::vector<SyncConflict> m_conflicts;
    std::vector<DeviceInfo> m_devices;
    std::vector<std::string> m_includePatterns;
    std::vector<std::string> m_excludePatterns;

    SyncCallback m_syncCallback;
    ConflictCallback m_conflictCallback;
    DeviceCallback m_deviceCallback;

    std::thread m_syncThread;
    std::atomic<bool> m_running{false};

    void syncLoop();
    bool performSync();
    void loadLocalSettings();
    void saveLocalSettings();
    void notifyStatusChanged();
};

// Global instance
SettingsSync& getSettingsSync();

} // namespace RawrXD::Sync
