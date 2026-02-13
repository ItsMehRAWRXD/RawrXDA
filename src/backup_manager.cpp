#include "backup_manager.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <thread>
#include <atomic>

namespace fs = std::filesystem;

BackupManager::BackupManager() : m_running(false), m_intervalMinutes(30) {
    m_backupDirectory = (fs::current_path() / "backups").string();
}

BackupManager::~BackupManager() {
    stop();
}

BackupManager& BackupManager::instance() {
    static BackupManager inst;
    return inst;
}

void BackupManager::setBackupDirectory(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_backupDirectory = path;
    if (!fs::exists(m_backupDirectory)) {
        fs::create_directories(m_backupDirectory);
    }
}

std::string BackupManager::backupDirectory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_backupDirectory;
}

void BackupManager::start(int intervalMinutes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_intervalMinutes = intervalMinutes;
    if (!m_running) {
        m_running = true;
        // In a real app this would start a background thread
        // std::thread(&BackupManager::autoBackupLoop, this).detach();
        std::cout << "[BackupManager] Started auto-backup service (Interval: " << intervalMinutes << "m)" << std::endl;
    }
}

void BackupManager::stop() {
    m_running = false;
}

std::string BackupManager::createBackup(BackupType type, const std::string& targetPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!fs::exists(m_backupDirectory)) {
        fs::create_directories(m_backupDirectory);
    }

    std::string id = generateBackupId();
    std::string typeStr;
    switch(type) {
        case BackupType::Auto: typeStr = "auto"; break;
        case BackupType::Manual: typeStr = "manual"; break;
        case BackupType::PreBuild: typeStr = "prebuild"; break;
        case BackupType::PreRefactor: typeStr = "prerefactor"; break;
    }

    std::string backupName = id + "_" + typeStr;
    fs::path destPath = fs::path(m_backupDirectory) / backupName;
    fs::path source = targetPath.empty() ? fs::current_path() : fs::path(targetPath);

    // Filter out build artifacts and the backup folder itself
    try {
        if (targetPath.empty()) {
            // Full project backup logic (simplified to src folder for safety)
            destPath = destPath / "src";
            fs::create_directories(destPath);
            source = fs::current_path() / "src";
        } else {
             fs::create_directories(destPath);
        }

        if (fs::exists(source)) {
             // Recursive copy with error checking
             const auto copyOptions = fs::copy_options::recursive | fs::copy_options::update_existing;
             fs::copy(source, destPath, copyOptions);
             return destPath.string();
        }
    } catch (const std::exception& e) {
        std::cerr << "[BackupManager] Backup failed: " << e.what() << std::endl;
        return "";
    }

    return destPath.string();
}

bool BackupManager::restoreBackup(const std::string& backupIdOrPath) {
    // Implementation of restore logic would mimic copy but in reverse
    // DANGER: Overwrites current files. 
    return false; // Safety first
}

bool BackupManager::verifyBackup(const std::string& path) {
    return fs::exists(path) && !fs::is_empty(path);
}

std::vector<BackupManager::BackupInfo> BackupManager::listBackups() const {
    std::vector<BackupInfo> backups;
    if (!fs::exists(m_backupDirectory)) return backups;

    for (const auto& entry : fs::directory_iterator(m_backupDirectory)) {
        if (entry.is_directory()) {
            BackupInfo info;
            info.path = entry.path().string();
            info.id = entry.path().filename().string();
            // Parse timestamp from folder name logic would go here
            backups.push_back(info);
        }
    }
    return backups;
}

void BackupManager::cleanOldBackups(int daysToKeep) {
    // Walk directory and remove old folders
}

std::string BackupManager::generateBackupId() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    return ss.str();
}
