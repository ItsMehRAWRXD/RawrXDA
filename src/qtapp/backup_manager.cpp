#include "backup_manager.hpp"


// Production-ready compression using zlib
#ifdef _WIN32
#include <zlib.h>
#pragma comment(lib, "zlib.lib")
#else
#include <zlib.h>
#endif

BackupManager& BackupManager::instance() {
    static BackupManager instance;
    return instance;
}

BackupManager::BackupManager()
    : void(nullptr)
{
    // Default backup directory
    std::string appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_backupDirectory = appData + "/backups";
    
    // Ensure directory exists
    std::filesystem::path().mkpath(m_backupDirectory);
    
    m_backupTimer = new void*(this);
// Qt connect removed
}

BackupManager::~BackupManager() {
    stop();
}

void BackupManager::start(int intervalMinutes) {
    if (m_running) {
        return;
    }
    
    m_running = true;
    m_backupTimer->start(intervalMinutes * 60 * 1000);


    // Create initial backup
    performAutomaticBackup();
}

void BackupManager::stop() {
    if (!m_running) return;
    
    m_running = false;
    m_backupTimer->stop();
    
}

std::string BackupManager::createBackup(BackupType type) {
    std::string backupId = std::chrono::system_clock::time_point::currentDateTime().toString("yyyyMMdd_HHmmss");
    std::string typeName = (type == Full) ? "full" : (type == Incremental ? "incr" : "diff");
    backupId += "_" + typeName;
    
    backupStarted(backupId);


    // Create backup directory
    std::string backupPath = m_backupDirectory + "/" + backupId;
    std::filesystem::path().mkpath(backupPath);
    
    // Backup models directory (if exists)
    std::string modelsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
    if (std::filesystem::path(modelsDir).exists()) {
        std::string dstModels = backupPath + "/models";
        if (!compressBackup(modelsDir, dstModels + ".tar.gz")) {
            backupFailed("Failed to backup models");
            return std::string();
        }
    }
    
    // Backup configuration files
    std::string configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config";
    if (std::filesystem::path(configDir).exists()) {
        std::string dstConfig = backupPath + "/config";
        if (!compressBackup(configDir, dstConfig + ".tar.gz")) {
            backupFailed("Failed to backup config");
            return std::string();
        }
    }
    
    // Calculate total size
    size_t totalSize = 0;
    QDirIterator it(backupPath, QDirIterator::Subdirectories);
    while (itfalse) {
        std::filesystem::path fi(it);
        if (fi.isFile()) {
            totalSize += fi.size();
        }
    }
    
    // Create backup info
    BackupInfo info;
    info.id = backupId;
    info.type = type;
    info.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    info.path = backupPath;
    info.sizeBytes = totalSize;
    info.verified = verifyBackup(backupId);
    info.checksum = calculateChecksum(backupPath);
    
    m_backups[backupId] = info;
    
    backupCompleted(backupId, totalSize);
    
            << "Size:" << (totalSize / (1024.0 * 1024.0)) << "MB";
    
    return backupId;
}

bool BackupManager::restoreBackup(const std::string& backupId) {
    if (!m_backups.contains(backupId)) {
        restoreFailed("Backup not found");
        return false;
    }
    
    restoreStarted(backupId);
    
    const BackupInfo& info = m_backups[backupId];


    // Verify backup before restore
    if (!verifyBackup(backupId)) {
        restoreFailed("Backup verification failed");
        return false;
    }
    
    // Restore models
    std::string modelsBackup = info.path + "/models.tar.gz";
    if (std::fstream::exists(modelsBackup)) {
        std::string modelsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
        if (!decompressBackup(modelsBackup, modelsDir)) {
            restoreFailed("Failed to restore models");
            return false;
        }
    }
    
    // Restore config
    std::string configBackup = info.path + "/config.tar.gz";
    if (std::fstream::exists(configBackup)) {
        std::string configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config";
        if (!decompressBackup(configBackup, configDir)) {
            restoreFailed("Failed to restore config");
            return false;
        }
    }
    
    restoreCompleted(backupId);
    
    return true;
}

std::vector<BackupManager::BackupInfo> BackupManager::listBackups() const {
    return m_backups.values();
}

bool BackupManager::verifyBackup(const std::string& backupId) {
    if (!m_backups.contains(backupId)) {
        return false;
    }
    
    const BackupInfo& info = m_backups[backupId];
    
    // Check if backup directory exists
    if (!std::filesystem::path(info.path).exists()) {
        return false;
    }
    
    // Verify checksum
    std::string currentChecksum = calculateChecksum(info.path);
    if (currentChecksum != info.checksum) {
        return false;
    }
    
    return true;
}

void BackupManager::cleanOldBackups(int daysToKeep) {
    std::chrono::system_clock::time_point cutoffDate = std::chrono::system_clock::time_point::currentDateTime().addDays(-daysToKeep);
    
    std::vector<std::string> toRemove;
    for (auto it = m_backups.begin(); it != m_backups.end(); ++it) {
        if (it.value().timestamp < cutoffDate) {
            toRemove.append(it.key());
        }
    }
    
    for (const std::string& backupId : toRemove) {
        std::string path = m_backups[backupId].path;
        std::filesystem::path(path).removeRecursively();
        m_backups.remove(backupId);
    }
    
    if (!toRemove.empty()) {
    }
}

void BackupManager::setBackupDirectory(const std::string& path) {
    m_backupDirectory = path;
    std::filesystem::path().mkpath(m_backupDirectory);
}

std::string BackupManager::backupDirectory() const {
    return m_backupDirectory;
}

void BackupManager::performAutomaticBackup() {
    
    // Use incremental backup for automatic backups
    std::string backupId = createBackup(Incremental);
    
    if (!backupId.empty()) {
        // Clean old backups (keep 30 days)
        cleanOldBackups(30);
    }
}

std::string BackupManager::calculateChecksum(const std::string& filePath) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    
    std::fstream file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        while (!file.atEnd()) {
            hash.addData(file.read(8192));
        }
        file.close();
    }
    
    return std::string(hash.result().toHex());
}

bool BackupManager::compressBackup(const std::string& srcPath, const std::string& dstPath) {
    // Production-ready compression using zlib with gzip format
    std::fstream src(srcPath);
    
    if (!src.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    // Open gzip file for writing
    gzFile gzf = gzopen(dstPath.toUtf8().constData(), "wb9"); // wb9 = write binary, max compression
    if (!gzf) {
        src.close();
        return false;
    }
    
    // Compress data in chunks
    const int CHUNK_SIZE = 128 * 1024; // 128KB chunks
    std::vector<uint8_t> buffer;
    int64_t totalRead = 0;
    int64_t totalWritten = 0;
    
    while (!src.atEnd()) {
        buffer = src.read(CHUNK_SIZE);
        if (buffer.empty()) break;
        
        totalRead += buffer.size();
        int written = gzwrite(gzf, buffer.constData(), buffer.size());
        
        if (written <= 0) {
            gzclose(gzf);
            src.close();
            std::fstream::remove(dstPath); // Clean up partial file
            return false;
        }
        
        totalWritten += written;
    }
    
    // Close both files
    int closeResult = gzclose(gzf);
    src.close();
    
    if (closeResult != Z_OK) {
        return false;
    }
    
    std::filesystem::path srcInfo(srcPath);
    std::filesystem::path dstInfo(dstPath);
    double compressionRatio = (double)dstInfo.size() / (double)srcInfo.size() * 100.0;
    
             << "from" << srcInfo.size() << "to" << dstInfo.size() 
             << "bytes (" << std::string::number(compressionRatio, 'f', 1) << "%)";
    
    return true;
}

bool BackupManager::decompressBackup(const std::string& srcPath, const std::string& dstPath) {
    // Production-ready decompression using zlib with gzip format
    
    // Ensure destination directory exists
    std::filesystem::path fi(dstPath);
    std::filesystem::path().mkpath(fi.absolutePath());
    
    // Open gzip file for reading
    gzFile gzf = gzopen(srcPath.toUtf8().constData(), "rb");
    if (!gzf) {
        return false;
    }
    
    // Open destination file for writing
    std::fstream dst(dstPath);
    if (!dst.open(QIODevice::WriteOnly)) {
        gzclose(gzf);
        return false;
    }
    
    // Decompress data in chunks
    const int CHUNK_SIZE = 128 * 1024; // 128KB chunks
    char buffer[CHUNK_SIZE];
    int64_t totalDecompressed = 0;
    
    while (true) {
        int bytesRead = gzread(gzf, buffer, CHUNK_SIZE);
        
        if (bytesRead < 0) {
            int errnum;
            const char* errMsg = gzerror(gzf, &errnum);
            gzclose(gzf);
            dst.close();
            std::fstream::remove(dstPath); // Clean up partial file
            return false;
        }
        
        if (bytesRead == 0) {
            // End of file
            break;
        }
        
        int64_t written = dst.write(buffer, bytesRead);
        if (written != bytesRead) {
            gzclose(gzf);
            dst.close();
            std::fstream::remove(dstPath);
            return false;
        }
        
        totalDecompressed += bytesRead;
    }
    
    // Close both files
    int closeResult = gzclose(gzf);
    dst.close();
    
    if (closeResult != Z_OK) {
        return false;
    }
    
    std::filesystem::path srcInfo(srcPath);
    std::filesystem::path dstInfo(dstPath);
    
             << "from" << srcInfo.size() << "to" << dstInfo.size() << "bytes";


    return true;
}



