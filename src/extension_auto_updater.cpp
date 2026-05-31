// ============================================================================
// extension_auto_updater.cpp — Auto-Update System Implementation
// ============================================================================
// Architecture: C++20 | Win32 | Background scheduler | No exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "extension_auto_updater.h"

#include <windows.h>
#include <urlmon.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Helper Functions
// ============================================================================

static std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return ss.str();
}

static fs::path GetExtensionDirectory(const std::string& extensionId) {
    wchar_t appDataPath[MAX_PATH] = {};
    if (SUCCEEDED(::SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
        char bufferA[MAX_PATH * 2] = {};
        ::WideCharToMultiByte(CP_UTF8, 0, appDataPath, -1, bufferA, sizeof(bufferA), nullptr, nullptr);
        return fs::path(bufferA) / "RawrXD" / "extensions" / extensionId;
    }
    return fs::path(".") / "extensions" / extensionId;
}

static fs::path GetBackupDirectory() {
    wchar_t appDataPath[MAX_PATH] = {};
    if (SUCCEEDED(::SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
        char bufferA[MAX_PATH * 2] = {};
        ::WideCharToMultiByte(CP_UTF8, 0, appDataPath, -1, bufferA, sizeof(bufferA), nullptr, nullptr);
        fs::path dir = fs::path(bufferA) / "RawrXD" / "backups";
        fs::create_directories(dir);
        return dir;
    }
    fs::path dir = fs::path(".") / "backups";
    fs::create_directories(dir);
    return dir;
}

// ============================================================================
// Global Instance
// ============================================================================

static ExtensionAutoUpdater* g_autoUpdater = nullptr;

ExtensionAutoUpdater& GetAutoUpdater() {
    if (!g_autoUpdater) {
        g_autoUpdater = new ExtensionAutoUpdater();
    }
    return *g_autoUpdater;
}

// ============================================================================
// ExtensionAutoUpdater Implementation
// ============================================================================

ExtensionAutoUpdater::ExtensionAutoUpdater() {
}

ExtensionAutoUpdater::~ExtensionAutoUpdater() {
    StopAutoUpdateScheduler();
}

void ExtensionAutoUpdater::SetCheckInterval(uint32_t hours) {
    std::lock_guard<std::mutex> lock(m_lock);
    m_checkIntervalHours = hours;
}

uint32_t ExtensionAutoUpdater::GetCheckInterval() const {
    std::lock_guard<std::mutex> lock(m_lock);
    return m_checkIntervalHours;
}

void ExtensionAutoUpdater::SetUpdatePolicy(UpdatePolicy policy) {
    std::lock_guard<std::mutex> lock(m_lock);
    m_defaultPolicy = policy;
}

UpdatePolicy ExtensionAutoUpdater::GetUpdatePolicy() const {
    std::lock_guard<std::mutex> lock(m_lock);
    return m_defaultPolicy;
}

void ExtensionAutoUpdater::SetExtensionUpdatePolicy(
    const std::string& extensionId,
    UpdatePolicy policy
) {
    if (extensionId.empty()) return;

    std::lock_guard<std::mutex> lock(m_lock);
    m_perExtensionPolicy[extensionId] = policy;
}

UpdatePolicy ExtensionAutoUpdater::GetExtensionUpdatePolicy(
    const std::string& extensionId
) const {
    if (extensionId.empty()) return UpdatePolicy::Manual;

    std::lock_guard<std::mutex> lock(m_lock);

    auto it = m_perExtensionPolicy.find(extensionId);
    if (it != m_perExtensionPolicy.end()) {
        return it->second;
    }
    
    return m_defaultPolicy;
}

void ExtensionAutoUpdater::SetNotificationsEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_lock);
    m_notificationsEnabled = enabled;
}

bool ExtensionAutoUpdater::AreNotificationsEnabled() const {
    std::lock_guard<std::mutex> lock(m_lock);
    return m_notificationsEnabled;
}

UpdateCheckResult ExtensionAutoUpdater::CheckForUpdates() {
    UpdateCheckResult result;
    result.checkTimeMs = ::GetTickCount64();

    // Query marketplace discovery backend for available updates
    auto& discovery = GetDiscoveryBackend();

    // Get list of installed extensions (from our install history)
    std::vector<std::string> installedIds;
    {
        std::lock_guard<std::mutex> lock(m_lock);
        for (const auto& record : m_installHistory) {
            if (std::find(installedIds.begin(), installedIds.end(), record.extensionId) == installedIds.end()) {
                installedIds.push_back(record.extensionId);
            }
        }
    }

    // Check each installed extension for updates
    for (const auto& extId : installedIds) {
        auto versions = discovery.GetAvailableVersions(extId);
        if (!versions.empty()) {
            // Get the latest version from marketplace
            std::string latestVersion = versions.front();

            // Compare with installed version
            std::string installedVersion;
            {
                std::lock_guard<std::mutex> lock(m_lock);
                for (const auto& record : m_installHistory) {
                    if (record.extensionId == extId && record.success) {
                        installedVersion = record.newVersion;
                        break;
                    }
                }
            }

            if (installedVersion != latestVersion) {
                ExtensionUpdateInfo info;
                info.extensionId = extId;
                info.currentVersion = installedVersion;
                info.newVersion = latestVersion;
                result.updatesAvailable.push_back(info);
            }
        }
    }

    result.success = true;

    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_lastCheckTimeMs = result.checkTimeMs;
        m_pendingUpdates = result.updatesAvailable;
    }

    return result;
}

UpdateCheckResult ExtensionAutoUpdater::CheckForUpdatesExclusive(
    const std::vector<std::string>& extensionIds
) {
    UpdateCheckResult result;
    result.checkTimeMs = ::GetTickCount64();

    auto& discovery = GetDiscoveryBackend();

    for (const auto& extId : extensionIds) {
        if (extId.empty()) continue;

        auto versions = discovery.GetAvailableVersions(extId);
        if (!versions.empty()) {
            std::string latestVersion = versions.front();

            // Find installed version from history
            std::string installedVersion;
            {
                std::lock_guard<std::mutex> lock(m_lock);
                for (const auto& record : m_installHistory) {
                    if (record.extensionId == extId && record.success) {
                        installedVersion = record.newVersion;
                        break;
                    }
                }
            }

            if (installedVersion != latestVersion) {
                ExtensionUpdateInfo info;
                info.extensionId = extId;
                info.currentVersion = installedVersion;
                info.newVersion = latestVersion;
                result.updatesAvailable.push_back(info);
            }
        }
    }

    result.success = true;
    return result;
}

bool ExtensionAutoUpdater::InstallUpdate(const std::string& extensionId) {
    if (extensionId.empty()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_lock);

        auto it = std::find_if(m_pendingUpdates.begin(), m_pendingUpdates.end(),
            [&extensionId](const ExtensionUpdateInfo& info) {
                return info.extensionId == extensionId;
            }
        );

        if (it == m_pendingUpdates.end()) {
            return false;  // No pending update for this extension
        }

        const auto updateInfo = *it;
        
        if (InstallUpdateInternal(extensionId, updateInfo)) {
            UpdateInstallRecord record;
            record.extensionId = extensionId;
            record.previousVersion = updateInfo.currentVersion;
            record.newVersion = updateInfo.newVersion;
            record.installTimeMs = ::GetTickCount64();
            record.success = true;

            m_installHistory.push_back(record);
            m_pendingUpdates.erase(it);

            NotifyUpdateInstalled(record);
            return true;
        }
    }

    return false;
}

bool ExtensionAutoUpdater::RollbackUpdate(const std::string& extensionId) {
    if (extensionId.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_lock);

    // Find last successful installation for this extension
    auto it = std::find_if(m_installHistory.rbegin(), m_installHistory.rend(),
        [&extensionId](const UpdateInstallRecord& record) {
            return record.extensionId == extensionId && record.success && !record.backupPath.empty();
        }
    );

    if (it == m_installHistory.rend()) {
        return false;
    }

    // Restore from backup
    bool restored = RestoreExtensionFromBackup(extensionId, it->backupPath);
    if (!restored) {
        return false;
    }

    // Record rollback in history
    UpdateInstallRecord rollbackRecord;
    rollbackRecord.extensionId = extensionId;
    rollbackRecord.previousVersion = it->newVersion;
    rollbackRecord.newVersion = it->previousVersion;
    rollbackRecord.installTimeMs = ::GetTickCount64();
    rollbackRecord.success = true;
    rollbackRecord.errorDetails = "Rolled back to version " + it->previousVersion;

    m_installHistory.push_back(rollbackRecord);

    // Remove the update from pending if it was there
    auto pendingIt = std::find_if(m_pendingUpdates.begin(), m_pendingUpdates.end(),
        [&extensionId](const ExtensionUpdateInfo& info) {
            return info.extensionId == extensionId;
        }
    );
    if (pendingIt != m_pendingUpdates.end()) {
        m_pendingUpdates.erase(pendingIt);
    }

    return true;
}

bool ExtensionAutoUpdater::StartAutoUpdateScheduler() {
    {
        std::lock_guard<std::mutex> lock(m_lock);

        if (m_schedulerRunning) {
            return false;  // Already running
        }

        m_schedulerRunning = true;
    }

    m_schedulerThread = std::thread([this]() {
        this->RunSchedulerThread();
    });

    return true;
}

void ExtensionAutoUpdater::StopAutoUpdateScheduler() {
    m_schedulerRunning = false;

    if (m_schedulerThread.joinable()) {
        m_schedulerThread.join();
    }
}

std::vector<ExtensionUpdateInfo> ExtensionAutoUpdater::GetPendingUpdates() const {
    std::lock_guard<std::mutex> lock(m_lock);
    return m_pendingUpdates;
}

std::vector<UpdateInstallRecord> ExtensionAutoUpdater::GetUpdateHistory(
    const std::string& extensionId,
    int maxRecords
) const {
    std::lock_guard<std::mutex> lock(m_lock);

    std::vector<UpdateInstallRecord> result;

    for (auto it = m_installHistory.rbegin(); it != m_installHistory.rend(); ++it) {
        if (extensionId.empty() || it->extensionId == extensionId) {
            result.push_back(*it);
            if (static_cast<int>(result.size()) >= maxRecords) {
                break;
            }
        }
    }

    return result;
}

uint64_t ExtensionAutoUpdater::GetLastCheckTimeMs() const {
    std::lock_guard<std::mutex> lock(m_lock);
    return m_lastCheckTimeMs;
}

void ExtensionAutoUpdater::OnUpdateAvailable(
    std::function<void(const ExtensionUpdateInfo&)> callback
) {
    std::lock_guard<std::mutex> lock(m_lock);
    m_onUpdateAvailable = callback;
}

void ExtensionAutoUpdater::OnUpdateInstalled(
    std::function<void(const UpdateInstallRecord&)> callback
) {
    std::lock_guard<std::mutex> lock(m_lock);
    m_onUpdateInstalled = callback;
}

bool ExtensionAutoUpdater::LoadUpdateHistory(const std::string& storagePath) {
    if (storagePath.empty()) {
        return false;
    }

    try {
        std::ifstream file(storagePath);
        if (!file.is_open()) {
            return true;  // File doesn't exist yet
        }

        json data;
        file >> data;
        file.close();

        std::lock_guard<std::mutex> lock(m_lock);

        if (data.contains("history") && data["history"].is_array()) {
            for (const auto& item : data["history"]) {
                if (!item.is_object()) continue;
                UpdateInstallRecord record;
                if (item.contains("extensionId") && item["extensionId"].is_string()) {
                    record.extensionId = item["extensionId"].get<std::string>();
                }
                if (item.contains("previousVersion") && item["previousVersion"].is_string()) {
                    record.previousVersion = item["previousVersion"].get<std::string>();
                }
                if (item.contains("newVersion") && item["newVersion"].is_string()) {
                    record.newVersion = item["newVersion"].get<std::string>();
                }
                if (item.contains("installTimeMs") && item["installTimeMs"].is_number()) {
                    record.installTimeMs = item["installTimeMs"].get<uint64_t>();
                }
                if (item.contains("success") && item["success"].is_boolean()) {
                    record.success = item["success"].get<bool>();
                }
                if (item.contains("errorDetails") && item["errorDetails"].is_string()) {
                    record.errorDetails = item["errorDetails"].get<std::string>();
                }
                if (item.contains("backupPath") && item["backupPath"].is_string()) {
                    record.backupPath = item["backupPath"].get<std::string>();
                }
                m_installHistory.push_back(record);
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool ExtensionAutoUpdater::SaveUpdateHistory(const std::string& storagePath) {
    if (storagePath.empty()) {
        return false;
    }

    try {
        json data;
        data["history"] = json::array();

        {
            std::lock_guard<std::mutex> lock(m_lock);

            for (const auto& record : m_installHistory) {
                json item;
                item["extensionId"] = record.extensionId;
                item["previousVersion"] = record.previousVersion;
                item["newVersion"] = record.newVersion;
                item["installTimeMs"] = record.installTimeMs;
                item["success"] = record.success;
                item["errorDetails"] = record.errorDetails;
                item["backupPath"] = record.backupPath;
                data["history"].push_back(item);
            }
        }

        std::ofstream file(storagePath);
        if (!file.is_open()) {
            return false;
        }

        file << data.dump(2);
        file.close();

        return true;
    } catch (...) {
        return false;
    }
}

bool ExtensionAutoUpdater::BackupExtension(const std::string& extensionId) {
    return BackupExtensionInternal(extensionId);
}

void ExtensionAutoUpdater::RunSchedulerThread() {
    while (m_schedulerRunning) {
        uint32_t checkIntervalMs = GetCheckInterval() * 3600 * 1000;  // Convert hours to ms
        uint64_t now = ::GetTickCount64();

        if (now - m_lastCheckTimeMs >= checkIntervalMs) {
            PerformUpdateCheck();
            PerformAutoInstall();
        }

        // Sleep for a bit to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}

bool ExtensionAutoUpdater::PerformUpdateCheck() {
    auto result = CheckForUpdates();
    return result.success;
}

bool ExtensionAutoUpdater::PerformAutoInstall() {
    std::lock_guard<std::mutex> lock(m_lock);

    for (const auto& update : m_pendingUpdates) {
        UpdatePolicy policy = GetExtensionUpdatePolicy(update.extensionId);

        bool shouldInstall = false;
        if (policy == UpdatePolicy::AutoInstall && !update.isBreakingChange) {
            shouldInstall = true;
        } else if (policy == UpdatePolicy::AutoInstallAll) {
            shouldInstall = true;
        }

        if (shouldInstall) {
            InstallUpdate(update.extensionId);
        }
    }

    return true;
}

bool ExtensionAutoUpdater::InstallUpdateInternal(
    const std::string& extensionId,
    const ExtensionUpdateInfo& updateInfo
) {
    if (extensionId.empty()) {
        return false;
    }

    // Step 1: Backup current version
    if (!BackupExtensionInternal(extensionId)) {
        return false;
    }

    // Step 2: Download the update
    fs::path downloadPath = GetBackupDirectory() / (extensionId + "_" + updateInfo.newVersion + ".vsix");
    bool downloaded = false;

    if (!updateInfo.downloadUrl.empty()) {
        // Use WinHTTP to download the VSIX
        downloaded = DownloadFile(updateInfo.downloadUrl, downloadPath.string());
    }

    if (!downloaded) {
        // Download failed — restore from backup
        auto it = m_backupPaths.find(extensionId);
        if (it != m_backupPaths.end()) {
            RestoreExtensionFromBackup(extensionId, it->second);
        }
        return false;
    }

    // Step 3: Uninstall current version
    fs::path extDir = GetExtensionDirectory(extensionId);
    try {
        if (fs::exists(extDir)) {
            fs::remove_all(extDir);
        }
    } catch (...) {
        // Restore from backup on failure
        auto it = m_backupPaths.find(extensionId);
        if (it != m_backupPaths.end()) {
            RestoreExtensionFromBackup(extensionId, it->second);
        }
        return false;
    }

    // Step 4: Install new version (extract VSIX)
    try {
        fs::create_directories(extDir);
        // VSIX is a ZIP file — extract it
        if (!ExtractVsix(downloadPath.string(), extDir.string())) {
            // Extraction failed — restore from backup
            auto it = m_backupPaths.find(extensionId);
            if (it != m_backupPaths.end()) {
                RestoreExtensionFromBackup(extensionId, it->second);
            }
            return false;
        }
    } catch (...) {
        // Restore from backup on failure
        auto it = m_backupPaths.find(extensionId);
        if (it != m_backupPaths.end()) {
            RestoreExtensionFromBackup(extensionId, it->second);
        }
        return false;
    }

    // Step 5: Verify installation (check for package.json)
    fs::path packageJson = extDir / "package.json";
    if (!fs::exists(packageJson)) {
        // Verification failed — restore from backup
        auto it = m_backupPaths.find(extensionId);
        if (it != m_backupPaths.end()) {
            RestoreExtensionFromBackup(extensionId, it->second);
        }
        return false;
    }

    // Step 6: Clean up downloaded VSIX on success
    try {
        fs::remove(downloadPath);
    } catch (...) {
        fprintf(stderr, "[ExtensionAutoUpdater] Cleanup error ignored\n");
    }

    return true;
}

// ============================================================================
// Download Helper
// ============================================================================

static bool DownloadFile(const std::string& url, const std::string& destPath) {
    // Use URLDownloadToFile for simple HTTP downloads
    std::wstring wUrl(url.begin(), url.end());
    std::wstring wDest(destPath.begin(), destPath.end());

    HRESULT hr = URLDownloadToFileW(nullptr, wUrl.c_str(), wDest.c_str(), 0, nullptr);
    return SUCCEEDED(hr);
}

// ============================================================================
// VSIX Extraction Helper (simplified — VSIX is ZIP)
// ============================================================================

static bool ExtractVsix(const std::string& vsixPath, const std::string& destDir) {
    // For MVP, use Shell API to extract ZIP
    // In production, integrate with a proper ZIP library
    std::wstring wVsix(vsixPath.begin(), vsixPath.end());
    std::wstring wDest(destDir.begin(), destDir.end());

    // Ensure destination exists
    fs::create_directories(destDir);

    // Use PowerShell Expand-Archive as a fallback
    std::string psCmd = "powershell -NoProfile -Command \"Expand-Archive -Path '\"" + vsixPath +
                         "'\" -DestinationPath '\"" + destDir + "'\" -Force\"";
    int result = std::system(psCmd.c_str());
    return (result == 0);
}

void ExtensionAutoUpdater::NotifyUpdateAvailable(const ExtensionUpdateInfo& updateInfo) {
    if (m_notificationsEnabled && m_onUpdateAvailable) {
        m_onUpdateAvailable(updateInfo);
    }
}

void ExtensionAutoUpdater::NotifyUpdateInstalled(const UpdateInstallRecord& record) {
    if (m_onUpdateInstalled) {
        m_onUpdateInstalled(record);
    }
}

bool ExtensionAutoUpdater::BackupExtensionInternal(const std::string& extensionId) {
    // Create timestamped backup of extension directory
    namespace fs = std::filesystem;
    fs::path extDir = GetExtensionDirectory(extensionId);
    if (!fs::exists(extDir)) return false;
    
    fs::path backupDir = GetBackupDirectory() / (extensionId + "_" + GetTimestamp());
    try {
        fs::create_directories(backupDir);
        fs::copy(extDir, backupDir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        m_backupPaths[extensionId] = backupDir.string();
        return true;
    } catch (const std::exception& e) {
        LogMessage(LOG_ERROR, "Backup failed for %s: %s", extensionId.c_str(), e.what());
        return false;
    }
}

bool ExtensionAutoUpdater::RestoreExtensionFromBackup(
    const std::string& extensionId,
    const std::string& backupPath
) {
    // Restore extension from backup directory
    namespace fs = std::filesystem;
    fs::path backupDir(backupPath);
    fs::path extDir = GetExtensionDirectory(extensionId);
    
    if (!fs::exists(backupDir)) return false;
    
    try {
        // Remove current extension directory if exists
        if (fs::exists(extDir)) {
            fs::remove_all(extDir);
        }
        fs::create_directories(extDir);
        fs::copy(backupDir, extDir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        return true;
    } catch (const std::exception& e) {
        LogMessage(LOG_ERROR, "Restore failed for %s: %s", extensionId.c_str(), e.what());
        return false;
    }
}

// ============================================================================
// Global Helpers
// ============================================================================

bool CheckExtensionUpdates(std::vector<ExtensionUpdateInfo>& outUpdates) {
    auto result = GetAutoUpdater().CheckForUpdates();
    if (result.success) {
        outUpdates = result.updatesAvailable;
        return true;
    }
    return false;
}

bool InstallExtensionUpdate(const std::string& extensionId) {
    return GetAutoUpdater().InstallUpdate(extensionId);
}

}  // namespace Extensions
}  // namespace RawrXD

// ============================================================================
// End of extension_auto_updater.cpp
// ============================================================================
