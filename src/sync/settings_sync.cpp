/**
 * @file settings_sync.cpp
 * @brief Cross-device settings synchronization implementation
 * Batch 5 - Item 66: Settings sync
 */

#include "sync/settings_sync.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace RawrXD::Sync {

SettingsSync::SettingsSync()
    : m_status(SyncStatus::Disabled)
    , m_enabled(false)
    , m_signedIn(false)
    , m_syncInterval(30)
    , m_syncOnStartup(true) {
}

SettingsSync::~SettingsSync() {
    shutdown();
}

bool SettingsSync::initialize(const std::string& deviceId) {
    m_deviceId = deviceId;
    m_deviceName = "RawrXD-" + deviceId.substr(0, 8);
    
    // Load local settings
    loadLocalSettings();
    
    if (m_syncOnStartup && m_signedIn) {
        syncNow();
    }
    
    return true;
}

void SettingsSync::shutdown() {
    if (m_enabled && m_signedIn) {
        syncNow();
    }
}

bool SettingsSync::signIn(const std::string& token) {
    // Validate token (simplified - would validate against server)
    if (token.empty()) {
        return false;
    }
    
    m_authToken = token;
    m_signedIn = true;
    
    // Extract user ID from token (simplified)
    m_userId = "user_" + token.substr(0, 16);
    
    // Register device
    registerDevice();
    
    return true;
}

void SettingsSync::signOut() {
    if (m_signedIn) {
        // Unregister device
        unregisterDevice();
    }
    
    m_signedIn = false;
    m_authToken.clear();
    m_userId.clear();
    m_enabled = false;
    m_status = SyncStatus::Disabled;
}

bool SettingsSync::isSignedIn() const {
    return m_signedIn;
}

std::string SettingsSync::getUserId() const {
    return m_userId;
}

void SettingsSync::enableSync() {
    if (!m_signedIn) {
        return;
    }
    
    m_enabled = true;
    m_status = SyncStatus::Synced;
    
    // Start sync timer (simplified - would use actual timer)
}

void SettingsSync::disableSync() {
    m_enabled = false;
    m_status = SyncStatus::Disabled;
}

bool SettingsSync::isEnabled() const {
    return m_enabled;
}

SyncStatus SettingsSync::getStatus() const {
    return m_status;
}

bool SettingsSync::syncNow() {
    if (!m_enabled || !m_signedIn) {
        return false;
    }
    
    m_status = SyncStatus::Syncing;
    
    // Download remote settings
    auto remoteSettings = downloadSettings();
    
    // Resolve conflicts
    resolveConflicts(remoteSettings);
    
    // Upload local settings
    uploadSettings();
    
    m_status = SyncStatus::Synced;
    m_lastSync = std::chrono::system_clock::now();
    
    if (m_syncCallback) {
        m_syncCallback(true);
    }
    
    return true;
}

std::future<bool> SettingsSync::syncNowAsync() {
    return std::async(std::launch::async, [this]() {
        return syncNow();
    });
}

void SettingsSync::forceUpload() {
    if (m_signedIn) {
        uploadSettings();
    }
}

void SettingsSync::forceDownload() {
    if (m_signedIn) {
        auto remoteSettings = downloadSettings();
        mergeSettings(remoteSettings, true);
    }
}

void SettingsSync::setSetting(const std::string& key, const std::string& value) {
    SyncItem item;
    item.key = key;
    item.value = value;
    item.timestamp = std::chrono::system_clock::now();
    item.deviceId = m_deviceId;
    item.version = getNextVersion();
    
    m_localSettings[key] = item;
    
    // Auto-sync if enabled
    if (m_enabled) {
        uploadSetting(item);
    }
}

std::optional<std::string> SettingsSync::getSetting(const std::string& key) {
    auto it = m_localSettings.find(key);
    if (it != m_localSettings.end()) {
        return it->second.value;
    }
    return std::nullopt;
}

void SettingsSync::deleteSetting(const std::string& key) {
    m_localSettings.erase(key);
    
    if (m_enabled) {
        deleteRemoteSetting(key);
    }
}

std::vector<std::string> SettingsSync::getKeys() {
    std::vector<std::string> keys;
    for (const auto& [key, _] : m_localSettings) {
        keys.push_back(key);
    }
    return keys;
}

std::vector<SyncConflict> SettingsSync::getConflicts() {
    return m_conflicts;
}

void SettingsSync::resolveConflict(const std::string& key, bool useLocal) {
    auto it = std::find_if(m_conflicts.begin(), m_conflicts.end(),
        [&key](const SyncConflict& c) { return c.key == key; });
    
    if (it != m_conflicts.end()) {
        if (useLocal) {
            m_localSettings[key] = it->local;
            uploadSetting(it->local);
        } else {
            m_localSettings[key] = it->remote;
        }
        m_conflicts.erase(it);
    }
}

void SettingsSync::resolveAllConflicts(bool useLocal) {
    for (const auto& conflict : m_conflicts) {
        if (useLocal) {
            m_localSettings[conflict.key] = conflict.local;
            uploadSetting(conflict.local);
        } else {
            m_localSettings[conflict.key] = conflict.remote;
        }
    }
    m_conflicts.clear();
}

std::vector<DeviceInfo> SettingsSync::getDevices() {
    // Would fetch from server
    std::vector<DeviceInfo> devices;
    
    // Add current device
    DeviceInfo current;
    current.id = m_deviceId;
    current.name = m_deviceName;
    current.type = "desktop";
    current.lastSeen = std::chrono::system_clock::now();
    current.isCurrent = true;
    devices.push_back(current);
    
    return devices;
}

void SettingsSync::renameDevice(const std::string& deviceId, const std::string& name) {
    if (deviceId == m_deviceId) {
        m_deviceName = name;
    }
    // Would update on server
}

void SettingsSync::removeDevice(const std::string& deviceId) {
    // Would remove from server
}

void SettingsSync::setSyncInterval(int minutes) {
    m_syncInterval = minutes;
}

int SettingsSync::getSyncInterval() const {
    return m_syncInterval;
}

void SettingsSync::setSyncOnStartup(bool sync) {
    m_syncOnStartup = sync;
}

bool SettingsSync::getSyncOnStartup() const {
    return m_syncOnStartup;
}

void SettingsSync::includeKey(const std::string& key) {
    m_includeKeys.insert(key);
    m_excludeKeys.erase(key);
}

void SettingsSync::excludeKey(const std::string& key) {
    m_excludeKeys.insert(key);
    m_includeKeys.erase(key);
}

void SettingsSync::includePattern(const std::string& pattern) {
    m_includePatterns.push_back(pattern);
}

void SettingsSync::excludePattern(const std::string& pattern) {
    m_excludePatterns.push_back(pattern);
}

void SettingsSync::onSyncComplete(SyncCallback callback) {
    m_syncCallback = callback;
}

void SettingsSync::onConflict(ConflictCallback callback) {
    m_conflictCallback = callback;
}

void SettingsSync::onError(ErrorCallback callback) {
    m_errorCallback = callback;
}

void SettingsSync::loadLocalSettings() {
    // Load from local file
    std::ifstream file("settings_sync.json");
    if (!file.is_open()) {
        return;
    }
    
    // Simplified JSON parsing
    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim whitespace and quotes
            key = trim(key);
            value = trim(value);
            
            SyncItem item;
            item.key = key;
            item.value = value;
            item.timestamp = std::chrono::system_clock::now();
            item.deviceId = m_deviceId;
            item.version = 1;
            
            m_localSettings[key] = item;
        }
    }
}

void SettingsSync::saveLocalSettings() {
    std::ofstream file("settings_sync.json");
    if (!file.is_open()) {
        return;
    }
    
    for (const auto& [key, item] : m_localSettings) {
        file << key << ": " << item.value << "\n";
    }
}

std::map<std::string, SyncItem> SettingsSync::downloadSettings() {
    // Would download from server
    return {};
}

void SettingsSync::uploadSettings() {
    for (const auto& [key, item] : m_localSettings) {
        if (shouldSync(key)) {
            uploadSetting(item);
        }
    }
}

void SettingsSync::uploadSetting(const SyncItem& item) {
    // Would upload to server
}

void SettingsSync::deleteRemoteSetting(const std::string& key) {
    // Would delete from server
}

void SettingsSync::resolveConflicts(const std::map<std::string, SyncItem>& remoteSettings) {
    m_conflicts.clear();
    
    for (const auto& [key, remoteItem] : remoteSettings) {
        auto localIt = m_localSettings.find(key);
        if (localIt != m_localSettings.end()) {
            if (localIt->second.timestamp < remoteItem.timestamp) {
                // Remote is newer
                m_localSettings[key] = remoteItem;
            } else if (localIt->second.timestamp > remoteItem.timestamp) {
                // Local is newer, upload
                uploadSetting(localIt->second);
            } else if (localIt->second.value != remoteItem.value) {
                // Conflict - same timestamp, different values
                SyncConflict conflict;
                conflict.key = key;
                conflict.local = localIt->second;
                conflict.remote = remoteItem;
                m_conflicts.push_back(conflict);
            }
        } else {
            // Key only exists remotely
            m_localSettings[key] = remoteItem;
        }
    }
    
    if (!m_conflicts.empty() && m_conflictCallback) {
        m_conflictCallback(m_conflicts);
    }
}

void SettingsSync::mergeSettings(const std::map<std::string, SyncItem>& remoteSettings, bool preferRemote) {
    for (const auto& [key, remoteItem] : remoteSettings) {
        auto localIt = m_localSettings.find(key);
        if (localIt == m_localSettings.end() || preferRemote) {
            m_localSettings[key] = remoteItem;
        }
    }
}

bool SettingsSync::shouldSync(const std::string& key) const {
    // Check include patterns
    if (!m_includeKeys.empty() || !m_includePatterns.empty()) {
        bool included = m_includeKeys.count(key) > 0;
        for (const auto& pattern : m_includePatterns) {
            if (std::regex_match(key, std::regex(pattern))) {
                included = true;
                break;
            }
        }
        if (!included) return false;
    }
    
    // Check exclude patterns
    if (m_excludeKeys.count(key) > 0) return false;
    for (const auto& pattern : m_excludePatterns) {
        if (std::regex_match(key, std::regex(pattern))) {
            return false;
        }
    }
    
    return true;
}

uint64_t SettingsSync::getNextVersion() {
    static uint64_t version = 1;
    return version++;
}

void SettingsSync::registerDevice() {
    // Would register with server
}

void SettingsSync::unregisterDevice() {
    // Would unregister from server
}

std::string SettingsSync::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \"\t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \"\t\r\n");
    return str.substr(start, end - start + 1);
}

} // namespace RawrXD::Sync
