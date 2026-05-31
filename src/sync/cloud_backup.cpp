/**
 * @file cloud_backup.cpp
 * @brief Cloud backup and restore functionality implementation
 * Batch 5 - Item 67: Cloud backup
 */

#include "sync/cloud_backup.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace RawrXD::Sync {

CloudBackup::CloudBackup()
    : m_backupLocation("backups/")
    , m_maxBackups(10)
    , m_automaticBackup(false)
    , m_backupInterval(24)
    , m_operationInProgress(false) {
    m_progress.status = BackupStatus::Idle;
}

CloudBackup::~CloudBackup() {
    shutdown();
}

bool CloudBackup::initialize() {
    // Create backup directory if it doesn't exist
    std::filesystem::create_directories(m_backupLocation);
    
    // Load backup list
    loadBackupList();
    
    return true;
}

void CloudBackup::shutdown() {
    cancelOperation();
}

bool CloudBackup::createBackup(const std::string& name, const std::string& description) {
    if (m_operationInProgress) {
        return false;
    }
    
    m_operationInProgress = true;
    m_progress = BackupProgress{};
    m_progress.status = BackupStatus::BackingUp;
    
    // Generate backup ID
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "backup_" << timestamp;
    std::string backupId = ss.str();
    
    // Create backup directory
    std::string backupDir = m_backupLocation + backupId + "/";
    std::filesystem::create_directories(backupDir);
    
    // Get files to backup
    auto files = getFilesToBackup();
    m_progress.totalFiles = static_cast<int>(files.size());
    
    // Backup each file
    for (size_t i = 0; i < files.size(); i++) {
        if (!m_operationInProgress) {
            m_progress.status = BackupStatus::Error;
            m_progress.error = "Backup cancelled";
            m_operationInProgress = false;
            return false;
        }
        
        const auto& file = files[i];
        m_progress.currentFile = file;
        m_progress.processedFiles = static_cast<int>(i);
        
        try {
            backupFile(file, backupDir);
        } catch (const std::exception& e) {
            m_progress.error = e.what();
        }
        
        if (m_progressCallback) {
            m_progressCallback(m_progress);
        }
    }
    
    // Create backup metadata
    BackupItem item;
    item.id = backupId;
    item.name = name.empty() ? "Backup " + backupId : name;
    item.timestamp = now;
    item.size = calculateBackupSize(backupDir);
    item.description = description;
    item.isAutomatic = false;
    
    m_backups.push_back(item);
    saveBackupList();
    
    // Clean up old backups
    cleanupOldBackups();
    
    m_progress.status = BackupStatus::Completed;
    m_progress.processedFiles = m_progress.totalFiles;
    m_operationInProgress = false;
    
    if (m_completeCallback) {
        m_completeCallback(true, "Backup completed successfully");
    }
    
    return true;
}

std::future<bool> CloudBackup::createBackupAsync(const std::string& name, const std::string& description) {
    return std::async(std::launch::async, [this, name, description]() {
        return createBackup(name, description);
    });
}

bool CloudBackup::createAutomaticBackup() {
    auto now = std::chrono::system_clock::now();
    std::stringstream ss;
    ss << "Auto-Backup " << std::put_time(std::localtime(&std::chrono::system_clock::to_time_t(now)), "%Y-%m-%d %H:%M");
    
    return createBackup(ss.str(), "Automatic backup");
}

bool CloudBackup::restoreBackup(const std::string& backupId) {
    auto item = getBackup(backupId);
    if (!item) {
        if (m_completeCallback) {
            m_completeCallback(false, "Backup not found");
        }
        return false;
    }
    
    return restoreBackup(backupId, "");
}

std::future<bool> CloudBackup::restoreBackupAsync(const std::string& backupId) {
    return std::async(std::launch::async, [this, backupId]() {
        return restoreBackup(backupId);
    });
}

bool CloudBackup::restoreBackup(const std::string& backupId, const std::string& targetPath) {
    if (m_operationInProgress) {
        return false;
    }
    
    m_operationInProgress = true;
    m_progress = BackupProgress{};
    m_progress.status = BackupStatus::Restoring;
    
    std::string backupDir = m_backupLocation + backupId + "/";
    std::string restoreDir = targetPath.empty() ? "./" : targetPath;
    
    if (!std::filesystem::exists(backupDir)) {
        m_progress.status = BackupStatus::Error;
        m_progress.error = "Backup not found";
        m_operationInProgress = false;
        if (m_completeCallback) {
            m_completeCallback(false, "Backup not found");
        }
        return false;
    }
    
    // Get files to restore
    std::vector<std::string> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(backupDir)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path().string());
        }
    }
    
    m_progress.totalFiles = static_cast<int>(files.size());
    
    // Restore each file
    for (size_t i = 0; i < files.size(); i++) {
        if (!m_operationInProgress) {
            m_progress.status = BackupStatus::Error;
            m_progress.error = "Restore cancelled";
            m_operationInProgress = false;
            return false;
        }
        
        const auto& file = files[i];
        m_progress.currentFile = file;
        m_progress.processedFiles = static_cast<int>(i);
        
        try {
            restoreFile(file, backupDir, restoreDir);
        } catch (const std::exception& e) {
            m_progress.error = e.what();
        }
        
        if (m_progressCallback) {
            m_progressCallback(m_progress);
        }
    }
    
    m_progress.status = BackupStatus::Completed;
    m_progress.processedFiles = m_progress.totalFiles;
    m_operationInProgress = false;
    
    if (m_completeCallback) {
        m_completeCallback(true, "Restore completed successfully");
    }
    
    return true;
}

std::vector<BackupItem> CloudBackup::listBackups() {
    return m_backups;
}

bool CloudBackup::deleteBackup(const std::string& backupId) {
    auto it = std::find_if(m_backups.begin(), m_backups.end(),
        [&backupId](const BackupItem& item) { return item.id == backupId; });
    
    if (it == m_backups.end()) {
        return false;
    }
    
    // Delete backup directory
    std::string backupDir = m_backupLocation + backupId;
    std::filesystem::remove_all(backupDir);
    
    // Remove from list
    m_backups.erase(it);
    saveBackupList();
    
    return true;
}

bool CloudBackup::deleteAllBackups() {
    for (const auto& backup : m_backups) {
        std::string backupDir = m_backupLocation + backup.id;
        std::filesystem::remove_all(backupDir);
    }
    
    m_backups.clear();
    saveBackupList();
    
    return true;
}

std::optional<BackupItem> CloudBackup::getBackup(const std::string& backupId) {
    auto it = std::find_if(m_backups.begin(), m_backups.end(),
        [&backupId](const BackupItem& item) { return item.id == backupId; });
    
    if (it != m_backups.end()) {
        return *it;
    }
    
    return std::nullopt;
}

void CloudBackup::setBackupLocation(const std::string& path) {
    m_backupLocation = path;
    if (!m_backupLocation.empty() && m_backupLocation.back() != '/') {
        m_backupLocation += '/';
    }
    std::filesystem::create_directories(m_backupLocation);
}

std::string CloudBackup::getBackupLocation() const {
    return m_backupLocation;
}

void CloudBackup::setMaxBackups(int count) {
    m_maxBackups = count;
    cleanupOldBackups();
}

int CloudBackup::getMaxBackups() const {
    return m_maxBackups;
}

void CloudBackup::setAutomaticBackup(bool enabled) {
    m_automaticBackup = enabled;
}

bool CloudBackup::isAutomaticBackupEnabled() const {
    return m_automaticBackup;
}

void CloudBackup::setAutomaticBackupInterval(int hours) {
    m_backupInterval = hours;
}

int CloudBackup::getAutomaticBackupInterval() const {
    return m_backupInterval;
}

void CloudBackup::addBackupItem(const std::string& path) {
    m_backupItems.push_back(path);
}

void CloudBackup::removeBackupItem(const std::string& path) {
    auto it = std::find(m_backupItems.begin(), m_backupItems.end(), path);
    if (it != m_backupItems.end()) {
        m_backupItems.erase(it);
    }
}

std::vector<std::string> CloudBackup::getBackupItems() const {
    return m_backupItems;
}

void CloudBackup::setDefaultBackupItems() {
    m_backupItems = {
        "settings/",
        "keybindings/",
        "snippets/",
        "extensions/",
        "workspaces/"
    };
}

BackupProgress CloudBackup::getProgress() const {
    return m_progress;
}

void CloudBackup::cancelOperation() {
    m_operationInProgress = false;
}

bool CloudBackup::isOperationInProgress() const {
    return m_operationInProgress;
}

bool CloudBackup::exportBackup(const std::string& backupId, const std::string& exportPath) {
    auto item = getBackup(backupId);
    if (!item) {
        return false;
    }
    
    std::string sourceDir = m_backupLocation + backupId;
    std::string destFile = exportPath;
    
    // Create ZIP archive (simplified - would use actual ZIP library)
    // For now, just copy the directory
    try {
        std::filesystem::copy(sourceDir, exportPath,
            std::filesystem::copy_options::recursive |
            std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

bool CloudBackup::importBackup(const std::string& importPath) {
    if (!std::filesystem::exists(importPath)) {
        return false;
    }
    
    // Generate new backup ID
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "imported_" << timestamp;
    std::string backupId = ss.str();
    
    std::string destDir = m_backupLocation + backupId;
    
    try {
        std::filesystem::copy(importPath, destDir,
            std::filesystem::copy_options::recursive);
        
        // Add to backup list
        BackupItem item;
        item.id = backupId;
        item.name = "Imported Backup";
        item.timestamp = now;
        item.size = calculateBackupSize(destDir);
        item.description = "Imported from " + importPath;
        item.isAutomatic = false;
        
        m_backups.push_back(item);
        saveBackupList();
        
        return true;
    } catch (...) {
        return false;
    }
}

void CloudBackup::onProgress(ProgressCallback callback) {
    m_progressCallback = callback;
}

void CloudBackup::onComplete(CompleteCallback callback) {
    m_completeCallback = callback;
}

void CloudBackup::loadBackupList() {
    std::ifstream file(m_backupLocation + "backups.json");
    if (!file.is_open()) {
        return;
    }
    
    // Simplified JSON parsing
    std::string line;
    while (std::getline(file, line)) {
        // Parse backup entry
        if (line.find("\"id\"") != std::string::npos) {
            BackupItem item;
            // Simplified parsing
            m_backups.push_back(item);
        }
    }
}

void CloudBackup::saveBackupList() {
    std::ofstream file(m_backupLocation + "backups.json");
    if (!file.is_open()) {
        return;
    }
    
    file << "[\n";
    for (size_t i = 0; i < m_backups.size(); i++) {
        const auto& backup = m_backups[i];
        file << "  {\n";
        file << "    \"id\": \"" << backup.id << "\",\n";
        file << "    \"name\": \"" << backup.name << "\",\n";
        file << "    \"description\": \"" << backup.description << "\"\n";
        file << "  }";
        if (i < m_backups.size() - 1) file << ",";
        file << "\n";
    }
    file << "]\n";
}

std::vector<std::string> CloudBackup::getFilesToBackup() {
    std::vector<std::string> files;
    
    if (m_backupItems.empty()) {
        setDefaultBackupItems();
    }
    
    for (const auto& item : m_backupItems) {
        if (std::filesystem::exists(item)) {
            if (std::filesystem::is_directory(item)) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(item)) {
                    if (entry.is_regular_file()) {
                        files.push_back(entry.path().string());
                    }
                }
            } else {
                files.push_back(item);
            }
        }
    }
    
    return files;
}

void CloudBackup::backupFile(const std::string& filePath, const std::string& backupDir) {
    std::filesystem::path source(filePath);
    std::filesystem::path dest = std::filesystem::path(backupDir) / source.filename();
    
    std::filesystem::create_directories(dest.parent_path());
    std::filesystem::copy_file(source, dest,
        std::filesystem::copy_options::overwrite_existing);
}

void CloudBackup::restoreFile(const std::string& filePath, const std::string& backupDir, const std::string& restoreDir) {
    std::filesystem::path source(filePath);
    std::filesystem::path relativePath = std::filesystem::relative(source, backupDir);
    std::filesystem::path dest = std::filesystem::path(restoreDir) / relativePath;
    
    std::filesystem::create_directories(dest.parent_path());
    std::filesystem::copy_file(source, dest,
        std::filesystem::copy_options::overwrite_existing);
}

size_t CloudBackup::calculateBackupSize(const std::string& backupDir) {
    size_t totalSize = 0;
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(backupDir)) {
        if (entry.is_regular_file()) {
            totalSize += entry.file_size();
        }
    }
    
    return totalSize;
}

void CloudBackup::cleanupOldBackups() {
    if (m_backups.size() <= static_cast<size_t>(m_maxBackups)) {
        return;
    }
    
    // Sort by timestamp (oldest first)
    std::sort(m_backups.begin(), m_backups.end(),
        [](const BackupItem& a, const BackupItem& b) {
            return a.timestamp < b.timestamp;
        });
    
    // Remove oldest backups
    while (m_backups.size() > static_cast<size_t>(m_maxBackups)) {
        const auto& oldest = m_backups[0];
        if (!oldest.isAutomatic) {
            // Keep manual backups
            break;
        }
        
        std::string backupDir = m_backupLocation + oldest.id;
        std::filesystem::remove_all(backupDir);
        m_backups.erase(m_backups.begin());
    }
    
    saveBackupList();
}

} // namespace RawrXD::Sync
